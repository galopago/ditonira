#include "rx/PileupTracker.h"

#include <algorithm>
#include <cmath>

namespace deepcw {
namespace {

template <typename T>
std::vector<T> selectStrongSeparated(const std::vector<T> &candidates, int maxCandidates,
                                     double minSeparationHz)
{
    std::vector<T> selected;
    for (const auto &candidate : candidates) {
        if (static_cast<int>(selected.size()) >= maxCandidates) {
            break;
        }
        const bool ok = std::all_of(selected.begin(), selected.end(), [&](const T &cur) {
            return std::fabs(cur.frequencyHz - candidate.frequencyHz) >= minSeparationHz;
        });
        if (ok) {
            selected.push_back(candidate);
        }
    }
    return selected;
}

} // namespace

std::vector<PileupCandidate> detectCwBinCandidates(const std::vector<float> &frequenciesHz,
                                                   const std::vector<float> &snrDb,
                                                   const std::vector<float> &lockedFrequenciesHz,
                                                   double lockSnrThresholdDb,
                                                   double releaseSnrThresholdDb)
{
    std::vector<PileupCandidate> scored;
    const size_t n = std::min(frequenciesHz.size(), snrDb.size());
    scored.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        scored.push_back({frequenciesHz[i], snrDb[i]});
    }
    std::sort(scored.begin(), scored.end(),
              [](const PileupCandidate &a, const PileupCandidate &b) { return a.snrDb > b.snrDb; });

    std::vector<PileupCandidate> aboveLock;
    for (const auto &c : scored) {
        if (c.snrDb >= lockSnrThresholdDb) {
            aboveLock.push_back(c);
        }
    }

    std::vector<PileupCandidate> retained;
    std::vector<float> retainedFreqs;
    for (float locked : lockedFrequenciesHz) {
        for (const auto &c : scored) {
            bool already = false;
            for (float rf : retainedFreqs) {
                if (std::fabs(rf - c.frequencyHz) < 1e-3f) {
                    already = true;
                    break;
                }
            }
            if (already) {
                continue;
            }
            if (std::fabs(c.frequencyHz - locked) <= kPileupMatchHz
                && c.snrDb >= releaseSnrThresholdDb) {
                retained.push_back(c);
                retainedFreqs.push_back(c.frequencyHz);
                break;
            }
        }
    }
    std::sort(retained.begin(), retained.end(),
              [](const PileupCandidate &a, const PileupCandidate &b) { return a.snrDb > b.snrDb; });

    std::vector<PileupCandidate> filteredNew;
    for (const auto &c : aboveLock) {
        const bool far = std::all_of(retained.begin(), retained.end(), [&](const PileupCandidate &r) {
            return std::fabs(r.frequencyHz - c.frequencyHz) >= kPileupFilterWidthHz;
        });
        if (far) {
            filteredNew.push_back(c);
        }
    }

    auto newLocks = selectStrongSeparated(
        filteredNew, std::max(0, kPileupMaxPeaks - static_cast<int>(retained.size())),
        kPileupFilterWidthHz);

    std::vector<PileupCandidate> out = retained;
    out.insert(out.end(), newLocks.begin(), newLocks.end());
    return out;
}

void PileupTracker::reset()
{
    m_tracked.clear();
    m_reported.clear();
    m_nextId = 1;
}

std::vector<PileupTrack> PileupTracker::update(const std::vector<PileupCandidate> &raw,
                                               std::int64_t nowMs)
{
    // Use vector<char>, not vector<bool> (proxy specialization caused SEGV under ASAN / ENG-009).
    std::vector<char> matched(m_tracked.size(), 0);
    std::vector<PileupCandidate> newcomers;

    for (const auto &candidate : raw) {
        int matchIndex = -1;
        for (size_t i = 0; i < m_tracked.size(); ++i) {
            if (matched[i]) {
                continue;
            }
            if (std::fabs(m_tracked[i].frequencyHz - candidate.frequencyHz) < kPileupMatchHz) {
                matchIndex = static_cast<int>(i);
                break;
            }
        }
        if (matchIndex >= 0) {
            matched[static_cast<size_t>(matchIndex)] = 1;
            auto &t = m_tracked[static_cast<size_t>(matchIndex)];
            t.frequencyHz = t.frequencyHz * 0.3f + candidate.frequencyHz * 0.7f;
            t.snrDb = candidate.snrDb;
            t.lastSeenMs = nowMs;
        } else {
            newcomers.push_back(candidate);
        }
    }

    for (const auto &candidate : newcomers) {
        m_tracked.push_back({m_nextId++, candidate.frequencyHz, candidate.snrDb, nowMs, nowMs});
    }

    m_tracked.erase(std::remove_if(m_tracked.begin(), m_tracked.end(),
                                   [&](const TrackedPileupCandidate &t) {
                                       return (nowMs - t.lastSeenMs) >= kPileupHoldMs;
                                   }),
                    m_tracked.end());

    std::vector<TrackedPileupCandidate> confirmed;
    for (const auto &t : m_tracked) {
        if ((nowMs - t.firstSeenMs) >= kPileupConfirmMs) {
            confirmed.push_back(t);
        }
    }
    std::sort(confirmed.begin(), confirmed.end(),
              [](const TrackedPileupCandidate &a, const TrackedPileupCandidate &b) {
                  return a.snrDb > b.snrDb;
              });

    auto selected = selectStrongSeparated(confirmed, kPileupMaxPeaks, kPileupFilterWidthHz);
    std::vector<PileupTrack> tracks;
    tracks.reserve(selected.size());
    for (const auto &t : selected) {
        tracks.push_back({t.id, static_cast<int>(std::lround(t.frequencyHz))});
    }
    std::sort(tracks.begin(), tracks.end(),
              [](const PileupTrack &a, const PileupTrack &b) { return a.frequencyHz < b.frequencyHz; });

    const bool changed = [&]() {
        if (tracks.size() != m_reported.size()) {
            return true;
        }
        for (size_t i = 0; i < tracks.size(); ++i) {
            if (tracks[i].id != m_reported[i].id) {
                return true;
            }
            if (std::abs(tracks[i].frequencyHz - m_reported[i].frequencyHz)
                >= static_cast<int>(kPileupMatchHz)) {
                return true;
            }
        }
        return false;
    }();

    if (changed) {
        m_reported = tracks;
    }
    return m_reported;
}

} // namespace deepcw
