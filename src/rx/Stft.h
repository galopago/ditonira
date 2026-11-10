#pragma once

#include <cstdint>
#include <functional>
#include <vector>

/**
 * Faithful port of web-deep-cw-decoder src/stft.ts (Hann + radix-2 / Bluestein).
 */
class Fft
{
public:
    explicit Fft(int fftSize);
    ~Fft();

    Fft(const Fft &) = delete;
    Fft &operator=(const Fft &) = delete;
    Fft(Fft &&) = delete;
    Fft &operator=(Fft &&) = delete;

    int fftSize() const { return m_fftSize; }

    /// In-place interleaved complex transform [re0,im0,re1,im1,...] length 2*fftSize.
    void transform(float *complexArray);

private:
    void initializePowerOfTwoTables();
    void initializeBluesteinTables();
    void powerOfTwoTransform(float *complexArray);
    void bluesteinTransform(float *complexArray);

    int m_fftSize = 0;
    bool m_isPowerOfTwo = false;

    std::vector<std::uint32_t> m_reverseTable;
    std::vector<float> m_sinTable;
    std::vector<float> m_cosTable;

    int m_bluesteinSize = 0;
    std::vector<float> m_bluesteinChirpTable;
    std::vector<float> m_bluesteinKernelFft;
    Fft *m_bluesteinFft = nullptr;
};

class Stft
{
public:
    Stft(int fftSize, int hopSize);
    ~Stft();

    Stft(const Stft &) = delete;
    Stft &operator=(const Stft &) = delete;

    int fftSize() const { return m_fftSize; }
    int hopSize() const { return m_hopSize; }

    int frameCount(int signalLength) const;

    /// Calls iteratee(complexFrame interleaved, frameIndex) for each frame.
    int forEachSpectrum(const float *signal, int signalLength,
                        const std::function<void(const float *complexFrame, int frameIndex)> &iteratee);

private:
    int m_fftSize = 0;
    int m_hopSize = 0;
    Fft m_fft;
    std::vector<float> m_window;
    std::vector<float> m_frame;
    std::vector<float> m_complexFrame;
};
