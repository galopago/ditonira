#include "rx/DecodeEngine.h"

#include "rx/CtcDecoder.h"
#include "rx/DeepCwConstants.h"
#include "rx/SpectrogramBuilder.h"
#include "rx/Stft.h"

#include <QMutexLocker>
#include <chrono>
#include <cmath>
#include <optional>
#include <thread>
#include <vector>

namespace deepcw {
namespace {

constexpr int kDisplayStartBin = 8;
constexpr int kDisplayEndBin = 121;
constexpr int kDisplayBins = kDisplayEndBin - kDisplayStartBin;

std::int64_t steadyNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace

DecodeEngine::DecodeEngine(AudioRingBuffer *ring, QObject *parent)
    : QThread(parent)
    , m_ring(ring)
{
    qRegisterMetaType<QVector<float>>("QVector<float>");
    qRegisterMetaType<PileupTrackText>("deepcw::PileupTrackText");
    qRegisterMetaType<QVector<PileupTrackText>>("QVector<deepcw::PileupTrackText>");
}

DecodeEngine::~DecodeEngine()
{
    requestStop();
    wait(10000);
}

bool DecodeEngine::loadModels(const QString &wideModelPath, const QString &narrowModelPath,
                              QString *errorOut)
{
    std::string err;
    if (!m_wideModel.load(wideModelPath.toStdString(), &err)) {
        if (errorOut) {
            *errorOut = QString::fromStdString(err);
        }
        return false;
    }
    if (!m_narrowModel.load(narrowModelPath.toStdString(), &err)) {
        if (errorOut) {
            *errorOut = QString::fromStdString(err);
        }
        return false;
    }
    return true;
}

void DecodeEngine::resetStreaming()
{
    m_streaming.reset();
    m_streamCursor = m_ring ? m_ring->writeCount() : 0;
    m_streamScratch.clear();
}

void DecodeEngine::clearStreamingPending()
{
    m_streaming.clearPendingAudio();
    m_streamCursor = m_ring ? m_ring->writeCount() : 0;
}

void DecodeEngine::resetPileup()
{
    m_pileupTracker.reset();
    m_pileupText.clear();
    m_pileupLastNonEmptyMs.clear();
    m_pileupLockedHz.clear();
}

void DecodeEngine::setMode(Mode mode)
{
    QMutexLocker lock(&m_paramMutex);
    m_mode = mode;
    m_wake.store(true);
}

void DecodeEngine::setWindowSeconds(int seconds)
{
    QMutexLocker lock(&m_paramMutex);
    m_windowSeconds = seconds;
}

void DecodeEngine::setCenterHz(int hz)
{
    QMutexLocker lock(&m_paramMutex);
    m_centerHz = hz;
}

void DecodeEngine::setBandwidthHz(int hz)
{
    QMutexLocker lock(&m_paramMutex);
    m_bandwidthHz = hz;
}

void DecodeEngine::setSpectrumPaused(bool paused)
{
    m_spectrumPaused.store(paused);
}

void DecodeEngine::requestDecodeWake()
{
    m_wake.store(true);
}

void DecodeEngine::requestStop()
{
    m_stop.store(true);
    m_wake.store(true);
}

QVector<float> DecodeEngine::computeDisplaySpectrum(const std::vector<float> &audio) const
{
    QVector<float> out(kDisplayBins, 0.f);
    if (static_cast<int>(audio.size()) < kFftLength) {
        return out;
    }

    const float *tail = audio.data() + (static_cast<int>(audio.size()) - kFftLength);
    Stft stft(kFftLength, kHopLength);
    stft.forEachSpectrum(tail, kFftLength, [&](const float *complexFrame, int) {
        for (int bin = kDisplayStartBin; bin < kDisplayEndBin; ++bin) {
            const float re = complexFrame[bin * 2];
            const float im = complexFrame[bin * 2 + 1];
            out[bin - kDisplayStartBin] = std::sqrt(re * re + im * im);
        }
    });
    return out;
}

void DecodeEngine::runNormalTick(float filterFreq, float filterWidth)
{
    if (!m_wideModel.isLoaded() || !m_ring) {
        return;
    }
    // Pull new samples on the decode thread (§10.1) — never from the PA callback.
    m_ring->consumeNew(&m_streamCursor, &m_streamScratch);
    if (!m_streamScratch.empty()) {
        m_streaming.appendAudio(m_streamScratch.data(),
                                static_cast<int>(m_streamScratch.size()));
    }

    StreamingTranscript::Update update;
    try {
        update = m_streaming.process(m_wideModel, filterFreq, filterWidth);
    } catch (const std::exception &ex) {
        emit decodeFailed(QString::fromUtf8(ex.what()));
        emit rxStatusChanged(tr("Listening…"));
        return;
    } catch (...) {
        emit decodeFailed(tr("Decode failed"));
        emit rxStatusChanged(tr("Listening…"));
        return;
    }
    if (!update.analyzed) {
        return;
    }
    emit transcriptUpdated(QString::fromStdString(update.fullCommitted),
                           QString::fromStdString(update.pendingText));
    if (!update.newlyCommitted.empty() || !update.pendingText.empty()) {
        emit rxStatusChanged(tr("Decoding…"));
    } else {
        emit rxStatusChanged(tr("Listening…"));
    }
}

void DecodeEngine::runPileupTick(const std::vector<float> &audio)
{
    if (!m_narrowModel.isLoaded() || audio.empty()) {
        return;
    }

    std::optional<CwBinSnrEstimate> snr;
    try {
        snr = estimateCwBinSnr(audio.data(), static_cast<int>(audio.size()),
                               static_cast<float>(kPileupMinFreqHz),
                               static_cast<float>(kPileupMaxFreqHz));
    } catch (...) {
        emit decodeFailed(tr("Pileup detection failed"));
        return;
    }
    if (!snr) {
        return;
    }

    auto raw = detectCwBinCandidates(snr->frequenciesHz, snr->snrDb, m_pileupLockedHz);
    const auto nowMs = steadyNowMs();
    auto tracks = m_pileupTracker.update(raw, nowMs);

    m_pileupLockedHz.clear();
    for (const auto &t : tracks) {
        m_pileupLockedHz.push_back(static_cast<float>(t.frequencyHz));
    }

    std::unordered_map<int, bool> active;
    QVector<PileupTrackText> views;

    if (tracks.empty()) {
        m_pileupText.clear();
        m_pileupLastNonEmptyMs.clear();
        emit pileupUpdated(views);
        emit rxStatusChanged(tr("Listening…"));
        return;
    }

    emit rxStatusChanged(tr("Decoding…"));

    for (const auto &track : tracks) {
        active[track.id] = true;
        QString text;
        try {
            auto spectrogram = audioToNarrowShiftedSpectrogramTensor(
                audio.data(), static_cast<int>(audio.size()),
                static_cast<float>(track.frequencyHz));
            if (spectrogram) {
                InferenceResult infer;
                std::string err;
                if (m_narrowModel.run(*spectrogram, &infer, &err)) {
                    const auto decoded = decodeEnLogProbs(infer.logProbs.data(),
                                                          infer.timeSteps, infer.numClasses);
                    text = QString::fromStdString(decoded.plainText).trimmed();
                } else if (!err.empty()) {
                    emit decodeFailed(QString::fromStdString(err));
                }
            }
        } catch (const std::exception &ex) {
            emit decodeFailed(QString::fromUtf8(ex.what()));
            continue;
        } catch (...) {
            emit decodeFailed(tr("Pileup decode failed"));
            continue;
        }

        const bool hasNew = !text.isEmpty();
        QString shown;
        if (hasNew) {
            shown = text;
            m_pileupText[track.id] = text;
            m_pileupLastNonEmptyMs[track.id] = nowMs;
        } else {
            const auto it = m_pileupText.find(track.id);
            const auto tIt = m_pileupLastNonEmptyMs.find(track.id);
            if (it != m_pileupText.end() && tIt != m_pileupLastNonEmptyMs.end()
                && (nowMs - tIt->second) < kPileupTextHoldMs) {
                shown = it->second;
            } else {
                m_pileupText.erase(track.id);
                m_pileupLastNonEmptyMs.erase(track.id);
            }
        }

        PileupTrackText row;
        row.id = track.id;
        row.frequencyHz = track.frequencyHz;
        row.text = shown;
        views.push_back(row);
    }

    for (auto it = m_pileupText.begin(); it != m_pileupText.end();) {
        if (!active.count(it->first)) {
            m_pileupLastNonEmptyMs.erase(it->first);
            it = m_pileupText.erase(it);
        } else {
            ++it;
        }
    }

    emit pileupUpdated(views);
    emit rxStatusChanged(tr("Listening…"));
}

void DecodeEngine::run()
{
    m_stop.store(false);
    m_spectrumPaused.store(false);
    emit rxStatusChanged(tr("Listening…"));

    auto lastSpectrum = std::chrono::steady_clock::now();
    auto lastDecodeTick = std::chrono::steady_clock::now() - std::chrono::seconds(1);

    while (!m_stop.load()) {
        const auto now = std::chrono::steady_clock::now();
        const bool paused = m_spectrumPaused.load();
        const bool wantSpectrum =
            !paused && (now - lastSpectrum) >= std::chrono::milliseconds(50);
        const bool wantDecode =
            !paused
            && (m_wake.exchange(false)
                || (now - lastDecodeTick) >= std::chrono::milliseconds(200));

        if (!wantSpectrum && !wantDecode) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (m_stop.load()) {
            break;
        }

        Mode mode = Mode::Normal;
        int centerHz = 800;
        int bandwidthHz = 800;
        {
            QMutexLocker lock(&m_paramMutex);
            mode = m_mode;
            centerHz = m_centerHz;
            bandwidthHz = m_bandwidthHz;
        }

        std::vector<float> audio;
        if (m_ring && (wantSpectrum || mode == Mode::Pileup)) {
            m_ring->snapshot(&audio);
        }

        if (wantSpectrum && !audio.empty()
            && static_cast<int>(audio.size()) > kCenterPad + 1) {
            emit spectrumUpdated(computeDisplaySpectrum(audio));
            lastSpectrum = now;
        }

        if (!wantDecode) {
            continue;
        }
        lastDecodeTick = now;

        float filterFreq = -1.f;
        float filterWidth = 0.f;
        if (bandwidthHz > 0 && bandwidthHz < kDefaultBandwidthHz) {
            filterFreq = static_cast<float>(centerHz);
            filterWidth = static_cast<float>(bandwidthHz);
        }

        if (mode == Mode::Normal) {
            runNormalTick(filterFreq, filterWidth);
        } else {
            // Prefer newest pileup window samples from the ring snapshot.
            if (audio.empty() && m_ring) {
                m_ring->snapshot(&audio);
            }
            const int need = kPileupWindowS * kSampleRate;
            if (static_cast<int>(audio.size()) > need) {
                audio.erase(audio.begin(),
                            audio.begin() + (static_cast<int>(audio.size()) - need));
            }
            // Drop leading zeros if ring not full yet.
            int first = 0;
            while (first < static_cast<int>(audio.size()) && audio[static_cast<size_t>(first)] == 0.f) {
                ++first;
            }
            if (first > 0 && first < static_cast<int>(audio.size())) {
                audio.erase(audio.begin(), audio.begin() + first);
            }
            runPileupTick(audio);
        }
    }

    emit rxStatusChanged(tr("Ready"));
}

} // namespace deepcw
