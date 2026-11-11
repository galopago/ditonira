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
    out.freqBins = kCroppedBins;
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

std::optional<SpectrogramTensor> audioToNarrowShiftedSpectrogramTensor(const float *audio,
                                                                       int length,
                                                                       float targetFreqHz)
{
    const std::vector<float> padded = centerPad(audio, length);
    if (padded.empty()) {
        return std::nullopt;
    }

    Stft stft(kFftLength, kHopLength);
    const int timeSteps = stft.frameCount(static_cast<int>(padded.size()));
    if (timeSteps <= 0) {
        return std::nullopt;
    }

    const int targetBin = static_cast<int>(std::lround(targetFreqHz / kBinResolution));
    SpectrogramTensor out;
    out.timeSteps = timeSteps;
    out.freqBins = kNarrowShiftBins;
    out.data.assign(static_cast<size_t>(timeSteps * kNarrowShiftBins), 0.f);

    stft.forEachSpectrum(padded.data(), static_cast<int>(padded.size()),
                         [&](const float *complexFrame, int frameIndex) {
                             const int offset = frameIndex * kNarrowShiftBins;
                             for (int d = -kNarrowCenterIdx; d <= kNarrowCenterIdx; ++d) {
                                 const int srcBin = targetBin + d;
                                 const int destIdx = kNarrowCenterIdx + d;
                                 if (srcBin >= 0 && srcBin < kTotalBins) {
                                     const float real = complexFrame[srcBin * 2];
                                     const float imag = complexFrame[srcBin * 2 + 1];
                                     out.data[static_cast<size_t>(offset + destIdx)] =
                                         std::sqrt(real * real + imag * imag);
                                 }
                             }
                         });

    normalizeSpectrogramNoiseLog1p(out.data);
    return out;
}

std::optional<CwBinSnrEstimate> estimateCwBinSnr(const float *audio, int length, float minFreqHz,
                                                 float maxFreqHz)
{
    if (!audio || length <= 0) {
        return std::nullopt;
    }

    Stft stft(kFftLength, kCwDetectHopLength);
    const int timeSteps = stft.frameCount(length);
    if (timeSteps <= 0) {
        return std::nullopt;
    }

    const int minBin =
        std::max(0, static_cast<int>(std::ceil(std::max(0.f, minFreqHz) / kBinResolution)));
    const int maxBin = std::min(
        kTotalBins - 1,
        static_cast<int>(
            std::floor(std::min(static_cast<float>(kSampleRate) / 2.f, maxFreqHz) / kBinResolution)));
    const int binCount = maxBin - minBin + 1;
    if (binCount <= 0) {
        return std::nullopt;
    }

    std::vector<float> powers(static_cast<size_t>(binCount * timeSteps), 0.f);
    stft.forEachSpectrum(audio, length, [&](const float *complexFrame, int frameIndex) {
        for (int bin = minBin; bin <= maxBin; ++bin) {
            const float real = complexFrame[bin * 2];
            const float imag = complexFrame[bin * 2 + 1];
            powers[static_cast<size_t>((bin - minBin) * timeSteps + frameIndex)] =
                real * real + imag * imag;
        }
    });

    std::vector<float> allPowers = powers;
    std::sort(allPowers.begin(), allPowers.end());
    const float noisePower =
        std::max(allPowers[allPowers.size() / 2], 1e-12f);

    std::vector<float> snrDb(static_cast<size_t>(binCount), 0.f);
    for (int index = 0; index < binCount; ++index) {
        std::vector<float> binPowers(
            powers.begin() + static_cast<std::ptrdiff_t>(index * timeSteps),
            powers.begin() + static_cast<std::ptrdiff_t>((index + 1) * timeSteps));
        std::sort(binPowers.begin(), binPowers.end());
        const size_t onIndex =
            static_cast<size_t>(std::floor(0.9 * (static_cast<double>(timeSteps) - 1.0)));
        const float onPower = binPowers[onIndex];
        snrDb[static_cast<size_t>(index)] =
            10.f * std::log10(std::max(onPower, 1e-12f) / noisePower);
    }

    constexpr float kSidebandMaskHz = 150.f;
    constexpr float kSidebandMaskDb = 18.f;
    const int maskBins = static_cast<int>(std::lround(kSidebandMaskHz / kBinResolution));
    std::vector<float> masked = snrDb;
    for (int index = 0; index < binCount; ++index) {
        const int lo = std::max(0, index - maskBins);
        const int hi = std::min(binCount - 1, index + maskBins);
        for (int other = lo; other <= hi; ++other) {
            if (snrDb[static_cast<size_t>(other)] - snrDb[static_cast<size_t>(index)]
                > kSidebandMaskDb) {
                masked[static_cast<size_t>(index)] = 0.f;
                break;
            }
        }
    }

    CwBinSnrEstimate out;
    out.snrDb = std::move(masked);
    out.frequenciesHz.resize(static_cast<size_t>(binCount));
    for (int index = 0; index < binCount; ++index) {
        out.frequenciesHz[static_cast<size_t>(index)] =
            static_cast<float>((minBin + index) * kBinResolution);
    }
    return out;
}

} // namespace deepcw
