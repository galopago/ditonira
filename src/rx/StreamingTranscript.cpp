#include "rx/StreamingTranscript.h"

#include "rx/CtcDecoder.h"
#include "rx/DeepCwConstants.h"
#include "rx/SpectrogramBuilder.h"

#include <algorithm>
#include <cmath>

namespace deepcw {
namespace {

std::string normalizeDecodedSegment(const std::string &text)
{
    std::string out;
    out.reserve(text.size());
    bool pendingSpace = false;
    bool any = false;
    for (char ch : text) {
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            if (any) {
                pendingSpace = true;
            }
            continue;
        }
        if (pendingSpace) {
            out.push_back(' ');
            pendingSpace = false;
        }
        out.push_back(ch);
        any = true;
    }
    return out;
}

int spanSplitSample(const WordSpaceSpan &span)
{
    const double midFrame = (span.startFrame + span.endFrame) / 2.0;
    return static_cast<int>(std::lround(midFrame * kOutputHopLength));
}

struct ConfirmedSplit {
    int sample = -1;
    int endFrame = -1;
};

ConfirmedSplit findConfirmedSplit(const std::vector<WordSpaceSpan> &spans, int analysisLength,
                                  bool allowNearEnd)
{
    const int minConfirmed = static_cast<int>(kSampleRate * 2.0);
    const int tailGuard = static_cast<int>(kSampleRate * 1.25);
    const int maxCommitted =
        allowNearEnd ? analysisLength
                     : std::max(minConfirmed, analysisLength - tailGuard);

    for (int i = static_cast<int>(spans.size()) - 1; i >= 0; --i) {
        const int splitSample = spanSplitSample(spans[static_cast<size_t>(i)]);
        if (splitSample >= minConfirmed && splitSample <= maxCommitted) {
            return {splitSample, spans[static_cast<size_t>(i)].endFrame};
        }
    }
    return {};
}

CtcDecodeResult trimToFrame(const CtcDecodeResult &in, int endFrame)
{
    CtcDecodeResult out;
    for (const auto &span : in.characterSpans) {
        if (span.endFrame <= endFrame) {
            out.characterSpans.push_back(span);
            out.plainText.push_back(span.ch);
        }
    }
    out.displayText = out.plainText;
    for (const auto &span : in.wordSpaceSpans) {
        if (span.endFrame <= endFrame) {
            out.wordSpaceSpans.push_back(span);
        }
    }
    return out;
}

} // namespace

void StreamingTranscript::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.clear();
    m_committed.clear();
    m_hasAnalyzed = false;
}

void StreamingTranscript::clearPendingAudio()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.clear();
    m_hasAnalyzed = false;
}

void StreamingTranscript::appendAudio(const float *samples, int count)
{
    if (!samples || count <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending.insert(m_pending.end(), samples, samples + count);
    const int maxPending = kSampleRate * kMaxPendingS;
    if (static_cast<int>(m_pending.size()) > maxPending) {
        const int drop = static_cast<int>(m_pending.size()) - maxPending;
        m_pending.erase(m_pending.begin(), m_pending.begin() + drop);
    }
}

std::string StreamingTranscript::committedText() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_committed;
}

StreamingTranscript::Update StreamingTranscript::process(OnnxCwModel &model, float filterFreqHz,
                                                         float filterWidthHz)
{
    Update update;
    std::vector<float> pendingCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        update.fullCommitted = m_committed;
        const int minPending = static_cast<int>(kSampleRate * kMinPendingS);
        if (static_cast<int>(m_pending.size()) < minPending) {
            update.analyzed = true;
            update.pendingText.clear();
            return update;
        }
        const auto now = std::chrono::steady_clock::now();
        if (m_hasAnalyzed) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     now - m_lastAnalysis)
                                     .count();
            if (elapsed < kMinIntervalMs) {
                return update; // analyzed=false → UI keeps last pending
            }
        }
        pendingCopy = m_pending;
        m_lastAnalysis = now;
        m_hasAnalyzed = true;
    }

    const int maxSeg = static_cast<int>(kSampleRate * kMaxSegmentS);
    const int analysisLength = std::min(static_cast<int>(pendingCopy.size()), maxSeg);
    const float *analysisAudio = pendingCopy.data();

    update.analyzed = true;

    auto spectrogram =
        audioToSpectrogramTensor(analysisAudio, analysisLength, filterFreqHz, filterWidthHz);
    if (!spectrogram || !model.isLoaded()) {
        return update;
    }

    InferenceResult infer;
    std::string err;
    if (!model.run(*spectrogram, &infer, &err)) {
        return update;
    }

    const CtcDecodeResult detailed =
        decodeEnLogProbs(infer.logProbs.data(), infer.timeSteps, infer.numClasses);
    update.pendingText = normalizeDecodedSegment(detailed.plainText);

    auto split = findConfirmedSplit(detailed.wordSpaceSpans, analysisLength, false);
    if (split.sample < 0 && static_cast<int>(pendingCopy.size()) >= maxSeg) {
        split = findConfirmedSplit(detailed.wordSpaceSpans, analysisLength, true);
    }

    const int minConfirmed = static_cast<int>(kSampleRate * kMinConfirmedS);
    int confirmedLength = split.sample;
    if (confirmedLength < 0 && static_cast<int>(pendingCopy.size()) >= maxSeg) {
        confirmedLength = maxSeg;
    }
    if (confirmedLength < minConfirmed) {
        std::lock_guard<std::mutex> lock(m_mutex);
        update.fullCommitted = m_committed;
        return update;
    }

    std::string commitText;
    if (split.sample >= 0) {
        commitText = normalizeDecodedSegment(trimToFrame(detailed, split.endFrame).plainText);
    } else {
        auto headSpec =
            audioToSpectrogramTensor(analysisAudio, confirmedLength, filterFreqHz, filterWidthHz);
        if (headSpec) {
            InferenceResult headInfer;
            if (model.run(*headSpec, &headInfer, &err)) {
                commitText = normalizeDecodedSegment(
                    decodeEnLogProbs(headInfer.logProbs.data(), headInfer.timeSteps,
                                     headInfer.numClasses)
                        .plainText);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (static_cast<int>(m_pending.size()) >= confirmedLength) {
            m_pending.erase(m_pending.begin(), m_pending.begin() + confirmedLength);
        } else {
            m_pending.clear();
        }
        if (!commitText.empty()) {
            if (!m_committed.empty()) {
                m_committed.push_back(' ');
            }
            m_committed += commitText;
            update.newlyCommitted = commitText;
        }
        update.fullCommitted = m_committed;
        // Clear preview so committed text does not appear twice until next analysis.
        update.pendingText.clear();
    }
    return update;
}

} // namespace deepcw
