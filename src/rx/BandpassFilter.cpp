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

} // namespace deepcw
