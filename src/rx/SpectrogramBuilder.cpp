#include "rx/SpectrogramBuilder.h"

#include "rx/BandpassFilter.h"
#include "rx/DeepCwConstants.h"
#include "rx/Stft.h"

#include <algorithm>
#include <cmath>

namespace deepcw {

std::vector<float> centerPad(const float *audio, int length)
{
    if (length <= kCenterPad + 1) {
        return {};
    }
    std::vector<float> padded(static_cast<size_t>(length + kCenterPad * 2));
    for (int i = 0; i < kCenterPad; ++i) {
        padded[static_cast<size_t>(i)] = audio[kCenterPad - i];
        padded[static_cast<size_t>(kCenterPad + length + i)] = audio[length - 2 - i];
    }
    for (int i = 0; i < length; ++i) {
        padded[static_cast<size_t>(kCenterPad + i)] = audio[i];
    }
    return padded;
}

void normalizeSpectrogramNoiseLog1p(std::vector<float> &spectrogram,
                                    const std::vector<float> *noiseSource)
{
    const std::vector<float> &src = noiseSource ? *noiseSource : spectrogram;
    if (src.empty()) {
        return;
    }
    std::vector<float> sorted(src);
    std::sort(sorted.begin(), sorted.end());
    const size_t qIndex =
        static_cast<size_t>(std::floor(kNoiseLevelQuantile * (sorted.size() - 1)));
    const float noiseLevel = std::max(
        {sorted[qIndex] / static_cast<float>(noiseLevelRayleighFactor()),
         sorted.back() * static_cast<float>(kNoiseLevelMinRelativeToPeak),
         static_cast<float>(kNoiseLevelMin)});
    for (float &x : spectrogram) {
        x = std::log1p(x / noiseLevel);
    }
}

std::optional<SpectrogramTensor> audioToSpectrogramTensor(const float *audio, int length,
                                                          float filterFreqHz,
                                                          float filterWidthHz)
{
    std::vector<float> owned;
    const float *processed = audio;
    int processedLen = length;

    if (filterFreqHz >= 0.f && filterWidthHz > 0.f) {
        owned = applyBandpassFilter(audio, length, kSampleRate, filterFreqHz, filterWidthHz);
        processed = owned.data();
        processedLen = static_cast<int>(owned.size());
    }

    const std::vector<float> padded = centerPad(processed, processedLen);
    if (padded.empty()) {
        return std::nullopt;
    }

    Stft stft(kFftLength, kHopLength);
    const int timeSteps = stft.frameCount(static_cast<int>(padded.size()));
    if (timeSteps <= 0) {
        return std::nullopt;
    }

    SpectrogramTensor out;
    out.timeSteps = timeSteps;
    out.data.assign(static_cast<size_t>(timeSteps * kCroppedBins), 0.f);

    stft.forEachSpectrum(padded.data(), static_cast<int>(padded.size()),
                         [&](const float *complexFrame, int frameIndex) {
                             const int offset = frameIndex * kCroppedBins;
                             for (int bin = kStartBin; bin < kEndBin; ++bin) {
                                 const float real = complexFrame[bin * 2];
                                 const float imag = complexFrame[bin * 2 + 1];
                                 out.data[static_cast<size_t>(offset + bin - kStartBin)] =
                                     std::sqrt(real * real + imag * imag);
                             }
                         });

    normalizeSpectrogramNoiseLog1p(out.data);
    return out;
}

} // namespace deepcw
