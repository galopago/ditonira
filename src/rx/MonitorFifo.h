#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace deepcw {

/**
 * Lock-free SPSC float FIFO for RX → monitor path (§7.4 / §10.1).
 * Capacity is power-of-two. Overflow drops oldest samples (keep latency low).
 */
class MonitorFifo
{
public:
    static constexpr int kCapacity = 8192; // ~0.85 s @ 9600 Hz

    void clear();
    void write(const float *samples, int count);
    /// Read up to maxCount samples; returns number read (may be less on underrun).
    int read(float *out, int maxCount);

private:
    std::array<float, kCapacity> m_data{};
    std::atomic<std::uint32_t> m_write{0};
    std::atomic<std::uint32_t> m_read{0};
};

} // namespace deepcw
