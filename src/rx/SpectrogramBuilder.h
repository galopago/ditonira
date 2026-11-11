#pragma once

#include <optional>
#include <vector>

namespace deepcw {

struct SpectrogramTensor {
    std::vector<float> data; // row-major T * freqBins
    int timeSteps = 0;
    int freqBins = 65; // 65 wide EN, 3 narrow EN
};

struct CwBinSnrEstimate {
    std::vector<float> frequenciesHz;
    std::vector<float> snrDb;
};

/**
 * Wide EN spectrogram path (ARCHITECTURE_RX_DEEPCW.md §4.3–4.6).
 * filterFreqHz < 0 skips bandpass (DeepCW null path).
 */
std::optional<SpectrogramTensor> audioToSpectrogramTensor(const float *audio, int length,
                                                          float filterFreqHz = -1.f,
                                                          float filterWidthHz = 0.f);

/// Narrow EN pileup path — shape [1,1,T,3] (§4.8).
std::optional<SpectrogramTensor> audioToNarrowShiftedSpectrogramTensor(const float *audio,
                                                                       int length,
                                                                       float targetFreqHz);

/// Per-bin CW SNR for pileup detection (§5.3 / estimateCwBinSnr).
std::optional<CwBinSnrEstimate> estimateCwBinSnr(const float *audio, int length, float minFreqHz,
                                                 float maxFreqHz);

void normalizeSpectrogramNoiseLog1p(std::vector<float> &spectrogram,
                                    const std::vector<float> *noiseSource = nullptr);

std::vector<float> centerPad(const float *audio, int length);

} // namespace deepcw
