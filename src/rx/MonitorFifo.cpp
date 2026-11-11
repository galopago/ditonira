#include "rx/MonitorFifo.h"

#include <algorithm>

namespace deepcw {

void MonitorFifo::clear()
{
    m_write.store(0, std::memory_order_relaxed);
    m_read.store(0, std::memory_order_relaxed);
}

void MonitorFifo::write(const float *samples, int count)
{
    if (!samples || count <= 0) {
        return;
    }
    std::uint32_t w = m_write.load(std::memory_order_relaxed);
    std::uint32_t r = m_read.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        const std::uint32_t next = w + 1;
        if (static_cast<int>(next - r) > kCapacity) {
            // Drop oldest.
            r = next - static_cast<std::uint32_t>(kCapacity);
            m_read.store(r, std::memory_order_release);
        }
        m_data[w & (kCapacity - 1)] = samples[i];
        w = next;
    }
    m_write.store(w, std::memory_order_release);
}

int MonitorFifo::read(float *out, int maxCount)
{
    if (!out || maxCount <= 0) {
        return 0;
    }
    const std::uint32_t w = m_write.load(std::memory_order_acquire);
    std::uint32_t r = m_read.load(std::memory_order_relaxed);
    const int available = static_cast<int>(w - r);
    const int n = std::min(maxCount, std::max(0, available));
    for (int i = 0; i < n; ++i) {
        out[i] = m_data[r & (kCapacity - 1)];
        ++r;
    }
    m_read.store(r, std::memory_order_release);
    return n;
}

} // namespace deepcw
