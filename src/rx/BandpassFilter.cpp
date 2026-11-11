#include "rx/BandpassFilter.h"

#include <cmath>

namespace deepcw {

std::vector<float> applyBandpassFilter(const float *audio, int length, int sampleRate,
                                       float centerFreq, float bandwidth, int passes)
{
    const double q = static_cast<double>(centerFreq) / static_cast<double>(bandwidth);
    const double omega = (2.0 * M_PI * centerFreq) / static_cast<double>(sampleRate);
    const double sinOmega = std::sin(omega);
    const double cosOmega = std::cos(omega);
    const double alpha = sinOmega / (2.0 * q);

    const double b0 = alpha;
    const double b1 = 0.0;
    const double b2 = -alpha;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosOmega;
    const double a2 = 1.0 - alpha;

    const float normB0 = static_cast<float>(b0 / a0);
    const float normB1 = static_cast<float>(b1 / a0);
    const float normB2 = static_cast<float>(b2 / a0);
    const float normA1 = static_cast<float>(a1 / a0);
    const float normA2 = static_cast<float>(a2 / a0);

    std::vector<float> input(audio, audio + length);
    std::vector<float> output(static_cast<size_t>(length));

    for (int p = 0; p < passes; ++p) {
        float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;
        for (int i = 0; i < length; ++i) {
            const float x0 = input[static_cast<size_t>(i)];
            const float y0 =
                normB0 * x0 + normB1 * x1 + normB2 * x2 - normA1 * y1 - normA2 * y2;
            output[static_cast<size_t>(i)] = y0;
            x2 = x1;
            x1 = x0;
            y2 = y1;
            y1 = y0;
        }
        input.swap(output);
        if (p < passes - 1) {
            output.assign(static_cast<size_t>(length), 0.f);
        }
    }
    return input;
}

void StreamingBandpass::reset()
{
    for (auto &s : m_sections) {
        s.x1 = s.x2 = s.y1 = s.y2 = 0.f;
    }
    m_ready = false;
}

void StreamingBandpass::setParams(int sampleRate, float centerFreqHz, float bandwidthHz)
{
    if (sampleRate <= 0 || centerFreqHz <= 0.f || bandwidthHz <= 0.f) {
        m_ready = false;
        return;
    }
    const double q = static_cast<double>(centerFreqHz) / static_cast<double>(bandwidthHz);
    const double omega = (2.0 * M_PI * centerFreqHz) / static_cast<double>(sampleRate);
    const double sinOmega = std::sin(omega);
    const double cosOmega = std::cos(omega);
    const double alpha = sinOmega / (2.0 * q);

    const double b0 = alpha;
    const double b1 = 0.0;
    const double b2 = -alpha;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosOmega;
    const double a2 = 1.0 - alpha;

    const float nb0 = static_cast<float>(b0 / a0);
    const float nb1 = static_cast<float>(b1 / a0);
    const float nb2 = static_cast<float>(b2 / a0);
    const float na1 = static_cast<float>(a1 / a0);
    const float na2 = static_cast<float>(a2 / a0);

    for (auto &s : m_sections) {
        s.b0 = nb0;
        s.b1 = nb1;
        s.b2 = nb2;
        s.a1 = na1;
        s.a2 = na2;
        s.x1 = s.x2 = s.y1 = s.y2 = 0.f;
    }
    m_ready = true;
}

void StreamingBandpass::processInPlace(float *samples, int count)
{
    if (!m_ready || !samples || count <= 0) {
        return;
    }
    for (auto &s : m_sections) {
        for (int i = 0; i < count; ++i) {
            const float x0 = samples[i];
            const float y0 = s.b0 * x0 + s.b1 * s.x1 + s.b2 * s.x2 - s.a1 * s.y1 - s.a2 * s.y2;
            samples[i] = y0;
            s.x2 = s.x1;
            s.x1 = x0;
            s.y2 = s.y1;
            s.y1 = y0;
        }
    }
}

} // namespace deepcw

