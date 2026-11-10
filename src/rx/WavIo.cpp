#include "rx/WavIo.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace deepcw {
namespace {

bool readExact(std::ifstream &in, void *dst, std::streamsize n)
{
    in.read(reinterpret_cast<char *>(dst), n);
    return static_cast<bool>(in) && in.gcount() == n;
}

uint16_t rd16(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t rd32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

} // namespace

bool readWavFile(const std::string &path, WavData *out, std::string *errorOut)
{
    if (!out) {
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (errorOut) {
            *errorOut = "Could not open WAV file";
        }
        return false;
    }

    uint8_t riff[12];
    if (!readExact(in, riff, 12) || std::memcmp(riff, "RIFF", 4) != 0
        || std::memcmp(riff + 8, "WAVE", 4) != 0) {
        if (errorOut) {
            *errorOut = "Not a RIFF/WAVE file";
        }
        return false;
    }

    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    std::vector<uint8_t> dataBytes;

    while (in) {
        uint8_t hdr[8];
        if (!readExact(in, hdr, 8)) {
            break;
        }
        const uint32_t chunkSize = rd32(hdr + 4);
        if (std::memcmp(hdr, "fmt ", 4) == 0) {
            std::vector<uint8_t> fmt(chunkSize);
            if (!readExact(in, fmt.data(), static_cast<std::streamsize>(chunkSize))) {
                if (errorOut) {
                    *errorOut = "Truncated fmt chunk";
                }
                return false;
            }
            if (chunkSize < 16) {
                if (errorOut) {
                    *errorOut = "Invalid fmt chunk";
                }
                return false;
            }
            audioFormat = rd16(fmt.data());
            numChannels = rd16(fmt.data() + 2);
            sampleRate = rd32(fmt.data() + 4);
            bitsPerSample = rd16(fmt.data() + 14);
            if (chunkSize % 2 == 1) {
                in.ignore(1);
            }
        } else if (std::memcmp(hdr, "data", 4) == 0) {
            dataBytes.resize(chunkSize);
            if (!readExact(in, dataBytes.data(), static_cast<std::streamsize>(chunkSize))) {
                if (errorOut) {
                    *errorOut = "Truncated data chunk";
                }
                return false;
            }
            if (chunkSize % 2 == 1) {
                in.ignore(1);
            }
            break;
        } else {
            in.seekg(chunkSize + (chunkSize % 2), std::ios::cur);
        }
    }

    if (dataBytes.empty() || numChannels == 0 || sampleRate == 0 || bitsPerSample == 0) {
        if (errorOut) {
            *errorOut = "Missing WAV fmt/data";
        }
        return false;
    }

    // 1=PCM, 3=IEEE float
    if (audioFormat != 1 && audioFormat != 3) {
        if (errorOut) {
            *errorOut = "Unsupported WAV format (need PCM or float)";
        }
        return false;
    }

    const int bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample <= 0) {
        if (errorOut) {
            *errorOut = "Invalid bits per sample";
        }
        return false;
    }
    const size_t frameBytes = static_cast<size_t>(bytesPerSample) * numChannels;
    const size_t frames = dataBytes.size() / frameBytes;
    out->sampleRate = static_cast<int>(sampleRate);
    out->samples.clear();
    out->samples.reserve(frames);

    for (size_t f = 0; f < frames; ++f) {
        float mono = 0.f;
        for (uint16_t ch = 0; ch < numChannels; ++ch) {
            const uint8_t *p = dataBytes.data() + f * frameBytes + ch * bytesPerSample;
            float s = 0.f;
            if (audioFormat == 3 && bitsPerSample == 32) {
                std::memcpy(&s, p, 4);
            } else if (audioFormat == 1 && bitsPerSample == 16) {
                const int16_t v = static_cast<int16_t>(rd16(p));
                s = static_cast<float>(v) / 32768.f;
            } else if (audioFormat == 1 && bitsPerSample == 8) {
                s = (static_cast<float>(p[0]) - 128.f) / 128.f;
            } else if (audioFormat == 1 && bitsPerSample == 32) {
                const int32_t v = static_cast<int32_t>(rd32(p));
                s = static_cast<float>(v) / 2147483648.f;
            } else {
                if (errorOut) {
                    *errorOut = "Unsupported PCM bit depth";
                }
                return false;
            }
            mono += s;
        }
        mono /= static_cast<float>(numChannels);
        out->samples.push_back(mono);
    }
    return true;
}

std::vector<float> resampleLinear(const float *input, int inputLen, int inputRate,
                                  int targetRate)
{
    if (inputLen <= 0 || inputRate <= 0 || targetRate <= 0) {
        return {};
    }
    if (inputRate == targetRate) {
        return std::vector<float>(input, input + inputLen);
    }
    const double ratio = static_cast<double>(inputRate) / static_cast<double>(targetRate);
    const int outLen = std::max(1, static_cast<int>(std::floor(inputLen / ratio)));
    std::vector<float> out(static_cast<size_t>(outLen));
    for (int i = 0; i < outLen; ++i) {
        const double src = i * ratio;
        const int i0 = static_cast<int>(std::floor(src));
        const int i1 = std::min(i0 + 1, inputLen - 1);
        const float t = static_cast<float>(src - i0);
        out[static_cast<size_t>(i)] = input[i0] * (1.f - t) + input[i1] * t;
    }
    return out;
}

} // namespace deepcw
