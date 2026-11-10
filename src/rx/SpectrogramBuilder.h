#pragma once

#include <optional>
#include <vector>

namespace deepcw {

struct SpectrogramTensor {
    std::vector<float> data; // row-major T * 65
    int timeSteps = 0;
    // ORT dims [1, 1, T, 65]
};

/**
 * Wide EN spectrogram path (ARCHITECTURE_RX_DEEPCW.md §4.3–4.6).
 * filterFreqHz < 0 skips bandpass (DeepCW null path).
 */
std::optional<SpectrogramTensor> audioToSpectrogramTensor(const float *audio, int length,
                                                          float filterFreqHz = -1.f,
                                                          float filterWidthHz = 0.f);

void normalizeSpectrogramNoiseLog1p(std::vector<float> &spectrogram,
                                    const std::vector<float> *noiseSource = nullptr);

std::vector<float> centerPad(const float *audio, int length);

} // namespace deepcw
