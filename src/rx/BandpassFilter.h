#pragma once

#include <vector>

/**
 * RBJ bandpass, 4 passes — port of web-deep-cw-decoder audioFilters.ts.
 */
namespace deepcw {

std::vector<float> applyBandpassFilter(const float *audio, int length, int sampleRate,
                                       float centerFreq, float bandwidth, int passes = 4);

/**
 * Stateful streaming bandpass for RT monitor path (no heap).
 * Call setParams from control thread; process() from PortAudio callback.
 */
class StreamingBandpass
{
public:
    static constexpr int kPasses = 4;

    void reset();
    void setParams(int sampleRate, float centerFreqHz, float bandwidthHz);
    void processInPlace(float *samples, int count);

private:
    struct Section {
        float b0 = 0.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;
    };
    Section m_sections[kPasses]{};
    bool m_ready = false;
};

} // namespace deepcw

