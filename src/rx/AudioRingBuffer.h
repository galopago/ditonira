#pragma once

#include <atomic>
#include <mutex>
#include <vector>

namespace deepcw {

/**
 * Fixed-length mono float ring used by the audio callback and decode thread.
 * Snapshot copies under a short mutex; version bumps on each write.
 */
class AudioRingBuffer
{
public:
    explicit AudioRingBuffer(int capacitySamples = 0);

    void resize(int capacitySamples);
    int capacity() const;

    void clear();
    void write(const float *samples, int count);

    /// Copy the full buffer contents (oldest→newest) into out; return version.
    std::uint64_t snapshot(std::vector<float> *out) const;

    std::uint64_t version() const { return m_version.load(); }

private:
    mutable std::mutex m_mutex;
    std::vector<float> m_data;
    int m_capacity = 0;
    int m_writePos = 0;
    int m_filled = 0;
    std::atomic<std::uint64_t> m_version{0};
};

} // namespace deepcw
