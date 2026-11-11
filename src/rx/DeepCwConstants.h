#pragma once

#include <cmath>
#include <cstddef>

/**
 * DeepCW Normal-mode constants — must match docs/ARCHITECTURE_RX_DEEPCW.md §3
 * and web-deep-cw-decoder src/const.ts / spectrogramUtils.ts.
 */
namespace deepcw {

constexpr int kSampleRate = 9600;
constexpr int kFftLength = 768;
constexpr int kHopLength = 144;
constexpr int kOutputTimeStride = 2;
constexpr int kOutputHopLength = kHopLength * kOutputTimeStride; // 288
constexpr int kCenterPad = kFftLength / 2;                       // 384
constexpr int kAudioChunkSamples = 2048;

constexpr double kBinResolution = static_cast<double>(kSampleRate) / kFftLength; // 12.5

constexpr int kDecodableMinFreqHz = 400;
constexpr int kDecodableMaxFreqHz = 1200;
constexpr int kDefaultCenterFreqHz = 800;
constexpr int kDefaultBandwidthHz = 800;

// ceil(400/12.5)=32; floor(1200/12.5)+1=97
constexpr int kStartBin = 32;
constexpr int kEndBin = 97;
constexpr int kCroppedBins = kEndBin - kStartBin; // 65
constexpr int kNormalCenterBin = 64;              // round((32+97-1)/2)
constexpr int kTotalBins = kFftLength / 2 + 1;    // 385

constexpr int kNarrowShiftBins = 3;
constexpr int kNarrowCenterIdx = kNarrowShiftBins / 2; // 1

constexpr int kCwDetectHopLength = 192;

// Pileup (ARCHITECTURE_RX_DEEPCW.md §5.3 / web const.ts)
constexpr double kPileupFilterWidthHz = kBinResolution * 4.0; // 50
constexpr int kPileupWindowS = 8;
constexpr int kPileupMinFreqHz = 100;
constexpr int kPileupMaxFreqHz = 2000;
constexpr int kPileupMaxPeaks = 5;
constexpr double kPileupMatchHz = kBinResolution * 1.2; // 15
constexpr double kPileupLockSnrThresholdDb = 12.0;
constexpr double kPileupReleaseSnrThresholdDb = 9.0;
constexpr int kPileupTextHoldMs = 1500;
constexpr int kPileupConfirmMs = 400;
constexpr int kPileupHoldMs = 5000;

constexpr int kDefaultDecodeWindowS = 12;

constexpr double kNoiseLevelQuantile = 0.9;
// sqrt(-2 * ln(1 - 0.9))
inline double noiseLevelRayleighFactor()
{
    return std::sqrt(-2.0 * std::log(1.0 - kNoiseLevelQuantile));
}
constexpr double kNoiseLevelMinRelativeToPeak = 1e-3;
constexpr double kNoiseLevelMin = 1e-5;

constexpr int kEnBlankIndex = 41;
constexpr int kEnNumClasses = 42;
constexpr int kEnVocabSize = 41;

// Indices 0..40; blank is 41 (not in this table).
inline const char *const kEnVocab[kEnVocabSize] = {
    ",", ".", "/", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "?",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N",
    "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", " "};

static_assert(kCroppedBins == 65, "Wide EN model expects 65 frequency bins");

} // namespace deepcw
