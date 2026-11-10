#pragma once

#include "rx/OnnxCwModel.h"

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace deepcw {

/**
 * DeepCW-style streaming commit (ARCHITECTURE_RX_DEEPCW.md §5.2 / useStreamingDecode.ts).
 * EN-only for Ditonira RX-3.
 */
class StreamingTranscript
{
public:
    struct Update {
        bool analyzed = false;
        std::string pendingText;
        std::string newlyCommitted; // empty if nothing committed this tick
        std::string fullCommitted;
    };

    void reset();
    /// Drop pending audio / live preview; keep committed transcript (filter retune).
    void clearPendingAudio();
    void appendAudio(const float *samples, int count);

    /// Run when enough audio + interval elapsed. Thread-safe vs appendAudio.
    Update process(OnnxCwModel &model, float filterFreqHz, float filterWidthHz);

    std::string committedText() const;

private:
    static constexpr double kMaxSegmentS = 30.0;
    static constexpr double kTailGuardS = 1.25;
    static constexpr double kMinPendingS = 2.0;
    static constexpr double kMinConfirmedS = 2.0;
    static constexpr int kMinIntervalMs = 1000;
    // Web retains ~120 s of committed segment audio; we only keep pending audio.
    static constexpr int kMaxPendingS = 120;

    mutable std::mutex m_mutex;
    std::vector<float> m_pending;
    std::string m_committed;
    std::chrono::steady_clock::time_point m_lastAnalysis{};
    bool m_hasAnalyzed = false;
};

} // namespace deepcw
