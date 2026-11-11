#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace deepcw {

/**
 * Lock-free SPSC mono float ring (ARCHITECTURE_RX_DEEPCW.md §10.1).
 * Producer: PortAudio callback (write / writeNew).
 * Consumer: DecodeEngine (snapshot / consumeNew).
 * resize()/clear() only from the GUI/control thread while capture is stopped.
 */
class AudioRingBuffer
{
public:
    explicit AudioRingBuffer(int capacitySamples = 0);

    /// Not realtime-safe — call only when capture is stopped.
    void resize(int capacitySamples);
    int capacity() const { return m_capacity.load(std::memory_order_acquire); }

    /// Not realtime-safe — call only when capture is stopped.
    void clear();

    /// Realtime-safe: no heap, no mutex.
    void write(const float *samples, int count);

    /// Copy the newest window (oldest→newest, right-aligned with leading zeros if not full).
    std::uint64_t snapshot(std::vector<float> *out) const;

    /// Append samples written since *cursor into out; advances *cursor. Realtime-safe consumer.
    void consumeNew(std::uint64_t *cursor, std::vector<float> *out) const;

    std::uint64_t writeCount() const { return m_writeCount.load(std::memory_order_acquire); }
    std::uint64_t version() const { return writeCount(); }

private:
    std::vector<float> m_data;
    std::atomic<int> m_capacity{0};
    std::atomic<std::uint64_t> m_writeCount{0};
};

} // namespace deepcw
