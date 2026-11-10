#include "rx/AudioRingBuffer.h"

#include <algorithm>
#include <cstring>

namespace deepcw {

AudioRingBuffer::AudioRingBuffer(int capacitySamples)
{
    if (capacitySamples > 0) {
        resize(capacitySamples);
    }
}

void AudioRingBuffer::resize(int capacitySamples)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_capacity = std::max(0, capacitySamples);
    m_data.assign(static_cast<size_t>(m_capacity), 0.f);
    m_writePos = 0;
    m_filled = 0;
    m_version.fetch_add(1);
}

int AudioRingBuffer::capacity() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_capacity;
}

void AudioRingBuffer::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::fill(m_data.begin(), m_data.end(), 0.f);
    m_writePos = 0;
    m_filled = 0;
    m_version.fetch_add(1);
}

void AudioRingBuffer::write(const float *samples, int count)
{
    if (!samples || count <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_capacity <= 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        m_data[static_cast<size_t>(m_writePos)] = samples[i];
        m_writePos = (m_writePos + 1) % m_capacity;
        if (m_filled < m_capacity) {
            ++m_filled;
        }
    }
    m_version.fetch_add(1);
}

std::uint64_t AudioRingBuffer::snapshot(std::vector<float> *out) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    out->assign(static_cast<size_t>(m_capacity), 0.f);
    if (m_capacity <= 0 || m_filled <= 0) {
        return m_version.load();
    }
    const int start =
        (m_filled < m_capacity) ? 0 : m_writePos;
    const int n = (m_filled < m_capacity) ? m_filled : m_capacity;
    for (int i = 0; i < n; ++i) {
        (*out)[static_cast<size_t>(i)] =
            m_data[static_cast<size_t>((start + i) % m_capacity)];
    }
    // Keep full-window length for decode (leading zeros if not yet full).
    if (m_filled < m_capacity) {
        // Move samples to the end so newest align with DeepCW "right side".
        std::vector<float> shifted(static_cast<size_t>(m_capacity), 0.f);
        const int offset = m_capacity - m_filled;
        for (int i = 0; i < m_filled; ++i) {
            shifted[static_cast<size_t>(offset + i)] = (*out)[static_cast<size_t>(i)];
        }
        *out = std::move(shifted);
    }
    return m_version.load();
}

} // namespace deepcw
