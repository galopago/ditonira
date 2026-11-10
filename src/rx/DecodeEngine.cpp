#include "rx/DecodeEngine.h"

#include "rx/DeepCwConstants.h"
#include "rx/Stft.h"

#include <QMutexLocker>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

namespace deepcw {
namespace {

// Display span 100–1500 Hz at 12.5 Hz/bin → bins [8, 121).
constexpr int kDisplayStartBin = 8;
constexpr int kDisplayEndBin = 121;
constexpr int kDisplayBins = kDisplayEndBin - kDisplayStartBin; // 113

} // namespace

DecodeEngine::DecodeEngine(AudioRingBuffer *ring, QObject *parent)
    : QThread(parent)
    , m_ring(ring)
{
    qRegisterMetaType<QVector<float>>("QVector<float>");
}

DecodeEngine::~DecodeEngine()
{
    requestStop();
    wait(10000);
}

bool DecodeEngine::loadModel(const QString &modelPath, QString *errorOut)
{
    std::string err;
    if (!m_model.load(modelPath.toStdString(), &err)) {
        if (errorOut) {
            *errorOut = QString::fromStdString(err);
        }
        return false;
    }
    return true;
}

void DecodeEngine::appendStreamingAudio(const float *samples, int count)
{
    m_streaming.appendAudio(samples, count);
    m_wake.store(true);
}

void DecodeEngine::resetStreaming()
{
    m_streaming.reset();
}

void DecodeEngine::clearStreamingPending()
{
    m_streaming.clearPendingAudio();
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

    // Use the newest FFT_LENGTH samples for a live ridge (tone debug).
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

void DecodeEngine::run()
{
    m_stop.store(false);
    m_spectrumPaused.store(false);
    emit rxStatusChanged(tr("Listening…"));

    auto lastSpectrum = std::chrono::steady_clock::now();
    auto lastStreamTick = std::chrono::steady_clock::now() - std::chrono::seconds(1);

    while (!m_stop.load()) {
        const auto now = std::chrono::steady_clock::now();
        const bool paused = m_spectrumPaused.load();
        const bool wantSpectrum =
            !paused && (now - lastSpectrum) >= std::chrono::milliseconds(50); // ~20 fps
        const bool wantStream =
            !paused && m_model.isLoaded()
            && (m_wake.exchange(false)
                || (now - lastStreamTick) >= std::chrono::milliseconds(200));

        if (!wantSpectrum && !wantStream) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (m_stop.load()) {
            break;
        }

        if (wantSpectrum && m_ring) {
            std::vector<float> audio;
            m_ring->snapshot(&audio);
            if (static_cast<int>(audio.size()) > kCenterPad + 1) {
                emit spectrumUpdated(computeDisplaySpectrum(audio));
                lastSpectrum = now;
            }
        }

        if (!wantStream || !m_model.isLoaded()) {
            continue;
        }

        int centerHz = 800;
        int bandwidthHz = 800;
        {
            QMutexLocker lock(&m_paramMutex);
            centerHz = m_centerHz;
            bandwidthHz = m_bandwidthHz;
        }

        float filterFreq = -1.f;
        float filterWidth = 0.f;
        if (bandwidthHz > 0 && bandwidthHz < kDefaultBandwidthHz) {
            filterFreq = static_cast<float>(centerHz);
            filterWidth = static_cast<float>(bandwidthHz);
        }

        lastStreamTick = now;
        const auto update = m_streaming.process(m_model, filterFreq, filterWidth);
        if (!update.analyzed) {
            continue;
        }

        emit transcriptUpdated(QString::fromStdString(update.fullCommitted),
                               QString::fromStdString(update.pendingText));
        if (!update.newlyCommitted.empty() || !update.pendingText.empty()) {
            emit rxStatusChanged(tr("Decoding…"));
        } else {
            emit rxStatusChanged(tr("Listening…"));
        }
    }

    emit rxStatusChanged(tr("Ready"));
}

} // namespace deepcw
