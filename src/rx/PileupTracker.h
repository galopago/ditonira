#pragma once

#include "rx/DeepCwConstants.h"

#include <cstdint>
#include <vector>

namespace deepcw {

struct PileupCandidate {
    float frequencyHz = 0.f;
    float snrDb = 0.f;
};

struct TrackedPileupCandidate {
    int id = 0;
    float frequencyHz = 0.f;
    float snrDb = 0.f;
    std::int64_t firstSeenMs = 0;
    std::int64_t lastSeenMs = 0;
};

struct PileupTrack {
    int id = 0;
    int frequencyHz = 0;
};

/// Raw SNR bins → lock/release candidates (inferenceWorker handleDetectCwBins).
std::vector<PileupCandidate> detectCwBinCandidates(
    const std::vector<float> &frequenciesHz, const std::vector<float> &snrDb,
    const std::vector<float> &lockedFrequenciesHz,
    double lockSnrThresholdDb = kPileupLockSnrThresholdDb,
    double releaseSnrThresholdDb = kPileupReleaseSnrThresholdDb);

class PileupTracker
{
public:
    void reset();
    /// Update tracking from a detect tick; returns confirmed tracks (sorted by Hz).
    std::vector<PileupTrack> update(const std::vector<PileupCandidate> &raw,
                                    std::int64_t nowMs);

private:
    std::vector<TrackedPileupCandidate> m_tracked;
    std::vector<PileupTrack> m_reported;
    int m_nextId = 1;
};

} // namespace deepcw
