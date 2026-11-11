#pragma once

#include <string>
#include <vector>

namespace deepcw {

struct WavData {
    std::vector<float> samples; // mono float32 ~[-1,1]
    int sampleRate = 0;
};

/// Read PCM WAV (8/16/32-bit integer or float32); mixes stereo to mono.
bool readWavFile(const std::string &path, WavData *out, std::string *errorOut = nullptr);

/// Linear resample to targetRate (RX-1 harness; document Speex/libsamplerate later if needed).
std::vector<float> resampleLinear(const float *input, int inputLen, int inputRate,
                                  int targetRate);

/// Realtime-safe resample into a preallocated buffer. Returns samples written (0 on error).
int resampleLinearInto(const float *input, int inputLen, int inputRate, int targetRate,
                       float *out, int outCapacity);

} // namespace deepcw
