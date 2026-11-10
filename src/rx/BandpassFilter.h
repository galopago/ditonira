#pragma once

#include <vector>

/**
 * RBJ bandpass, 4 passes — port of web-deep-cw-decoder audioFilters.ts.
 */
namespace deepcw {

std::vector<float> applyBandpassFilter(const float *audio, int length, int sampleRate,
                                       float centerFreq, float bandwidth, int passes = 4);

} // namespace deepcw
