#include "rx/AudioRingBuffer.h"

#include <algorithm>

namespace deepcw {

AudioRingBuffer::AudioRingBuffer(int capacitySamples)
{
    if (capacitySamples > 0) {
        resize(capacitySamples);
    }
}

void AudioRingBuffer::resize(int capacitySamples)
{
    const int cap = std::max(0, capacitySamples);
    m_data.assign(static_cast<size_t>(cap), 0.f);
    m_capacity.store(cap, std::memory_order_release);
    m_writeCount.store(0, std::memory_order_release);
}

void AudioRingBuffer::clear()
{
    std::fill(m_data.begin(), m_data.end(), 0.f);
    m_writeCount.store(0, std::memory_order_release);
}

void AudioRingBuffer::write(const float *samples, int count)
{
    if (!samples || count <= 0) {
        return;
    }
    const int cap = m_capacity.load(std::memory_order_acquire);
    if (cap <= 0 || m_data.empty()) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        const std::uint64_t w = m_writeCount.load(std::memory_order_relaxed);
        m_data[static_cast<size_t>(w % static_cast<std::uint64_t>(cap))] = samples[i];
        m_writeCount.store(w + 1, std::memory_order_release);
    }
}

std::uint64_t AudioRingBuffer::snapshot(std::vector<float> *out) const
{
    const int cap = m_capacity.load(std::memory_order_acquire);
    out->assign(static_cast<size_t>(std::max(0, cap)), 0.f);
    if (cap <= 0 || m_data.empty()) {
        return m_writeCount.load(std::memory_order_acquire);
    }

    // Seqlock-ish: retry if writer advances a lot mid-copy.
    for (int attempt = 0; attempt < 3; ++attempt) {
        const std::uint64_t end = m_writeCount.load(std::memory_order_acquire);
        const std::uint64_t available = std::min(end, static_cast<std::uint64_t>(cap));
        const std::uint64_t start = end - available;
        for (std::uint64_t i = 0; i < available; ++i) {
            const std::uint64_t abs = start + i;
            (*out)[static_cast<size_t>(cap - available + i)] =
                m_data[static_cast<size_t>(abs % static_cast<std::uint64_t>(cap))];
        }
        const std::uint64_t end2 = m_writeCount.load(std::memory_order_acquire);
        if (end2 - end < static_cast<std::uint64_t>(cap / 4 + 1)) {
            return end2;
        }
    }
    return m_writeCount.load(std::memory_order_acquire);
}

void AudioRingBuffer::consumeNew(std::uint64_t *cursor, std::vector<float> *out) const
{
    out->clear();
    if (!cursor) {
        return;
    }
    const int cap = m_capacity.load(std::memory_order_acquire);
    if (cap <= 0 || m_data.empty()) {
        return;
    }
    const std::uint64_t end = m_writeCount.load(std::memory_order_acquire);
    std::uint64_t start = *cursor;
    if (end < start) {
        // Counter reset (clear/resize).
        start = end;
    }
    if (end - start > static_cast<std::uint64_t>(cap)) {
        // Consumer fell behind — skip to newest window.
        start = end - static_cast<std::uint64_t>(cap);
    }
    const std::uint64_t n = end - start;
    out->resize(static_cast<size_t>(n));
    for (std::uint64_t i = 0; i < n; ++i) {
        (*out)[static_cast<size_t>(i)] =
            m_data[static_cast<size_t>((start + i) % static_cast<std::uint64_t>(cap))];
    }
    *cursor = end;
}

} // namespace deepcw
