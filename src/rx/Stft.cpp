#include "rx/Stft.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

int nextPowerOfTwo(int value)
{
    int power = 1;
    while (power < value) {
        power <<= 1;
    }
    return power;
}

void inverseTransform(float *complexArray, Fft &fft)
{
    const int n = fft.fftSize();
    for (int i = 0; i < n; ++i) {
        complexArray[i * 2 + 1] = -complexArray[i * 2 + 1];
    }
    fft.transform(complexArray);
    for (int i = 0; i < n; ++i) {
        complexArray[i * 2] /= static_cast<float>(n);
        complexArray[i * 2 + 1] = -complexArray[i * 2 + 1] / static_cast<float>(n);
    }
}

} // namespace

Fft::Fft(int fftSize)
    : m_fftSize(fftSize)
{
    if (fftSize <= 0) {
        throw std::invalid_argument("FFT size must be positive.");
    }
    m_isPowerOfTwo = (fftSize & (fftSize - 1)) == 0;
    if (m_isPowerOfTwo) {
        initializePowerOfTwoTables();
    } else {
        initializeBluesteinTables();
    }
}

Fft::~Fft()
{
    delete m_bluesteinFft;
    m_bluesteinFft = nullptr;
}

void Fft::initializePowerOfTwoTables()
{
    m_reverseTable.assign(static_cast<size_t>(m_fftSize), 0);
    m_sinTable.assign(static_cast<size_t>(m_fftSize), 0.f);
    m_cosTable.assign(static_cast<size_t>(m_fftSize), 0.f);

    int limit = 1;
    int bit = m_fftSize >> 1;
    while (limit < m_fftSize) {
        for (int i = 0; i < limit; ++i) {
            m_reverseTable[static_cast<size_t>(i + limit)] =
                m_reverseTable[static_cast<size_t>(i)] + static_cast<std::uint32_t>(bit);
        }
        limit <<= 1;
        bit >>= 1;
    }

    for (int i = 0; i < m_fftSize; ++i) {
        const double angle = (-2.0 * M_PI * i) / m_fftSize;
        m_sinTable[static_cast<size_t>(i)] = static_cast<float>(std::sin(angle));
        m_cosTable[static_cast<size_t>(i)] = static_cast<float>(std::cos(angle));
    }
}

void Fft::initializeBluesteinTables()
{
    const int convolutionSize = nextPowerOfTwo(m_fftSize * 2 - 1);
    m_bluesteinChirpTable.assign(static_cast<size_t>(m_fftSize * 2), 0.f);
    std::vector<float> kernel(static_cast<size_t>(convolutionSize * 2), 0.f);

    for (int i = 0; i < m_fftSize; ++i) {
        const double angle =
            (M_PI * static_cast<double>((i * i) % (m_fftSize * 2))) / m_fftSize;
        const float cosv = static_cast<float>(std::cos(angle));
        const float sinv = static_cast<float>(std::sin(angle));

        m_bluesteinChirpTable[static_cast<size_t>(i * 2)] = cosv;
        m_bluesteinChirpTable[static_cast<size_t>(i * 2 + 1)] = sinv;

        kernel[static_cast<size_t>(i * 2)] = cosv;
        kernel[static_cast<size_t>(i * 2 + 1)] = sinv;
        if (i != 0) {
            const int mirroredIndex = convolutionSize - i;
            kernel[static_cast<size_t>(mirroredIndex * 2)] = cosv;
            kernel[static_cast<size_t>(mirroredIndex * 2 + 1)] = sinv;
        }
    }

    m_bluesteinFft = new Fft(convolutionSize);
    m_bluesteinFft->transform(kernel.data());

    m_bluesteinSize = convolutionSize;
    m_bluesteinKernelFft = std::move(kernel);
}

void Fft::transform(float *complexArray)
{
    if (m_isPowerOfTwo) {
        powerOfTwoTransform(complexArray);
    } else {
        bluesteinTransform(complexArray);
    }
}

void Fft::powerOfTwoTransform(float *complexArray)
{
    for (int i = 0; i < m_fftSize; ++i) {
        const int reversedIndex = static_cast<int>(m_reverseTable[static_cast<size_t>(i)]);
        if (i < reversedIndex) {
            std::swap(complexArray[i * 2], complexArray[reversedIndex * 2]);
            std::swap(complexArray[i * 2 + 1], complexArray[reversedIndex * 2 + 1]);
        }
    }

    for (int halfSize = 1; halfSize < m_fftSize; halfSize *= 2) {
        const int step = 2 * halfSize;
        const int angleStep = m_fftSize / step;
        for (int i = 0; i < m_fftSize; i += step) {
            for (int j = 0; j < halfSize; ++j) {
                const int angleIndex = j * angleStep;
                const float wReal = m_cosTable[static_cast<size_t>(angleIndex)];
                const float wImag = m_sinTable[static_cast<size_t>(angleIndex)];

                const int i_j = (i + j) * 2;
                const int i_j_half = (i + j + halfSize) * 2;

                const float tr =
                    wReal * complexArray[i_j_half] - wImag * complexArray[i_j_half + 1];
                const float ti =
                    wReal * complexArray[i_j_half + 1] + wImag * complexArray[i_j_half];

                const float ur = complexArray[i_j];
                const float ui = complexArray[i_j + 1];

                complexArray[i_j] = ur + tr;
                complexArray[i_j + 1] = ui + ti;
                complexArray[i_j_half] = ur - tr;
                complexArray[i_j_half + 1] = ui - ti;
            }
        }
    }
}

void Fft::bluesteinTransform(float *complexArray)
{
    const int convolutionSize = m_bluesteinSize;
    std::vector<float> work(static_cast<size_t>(convolutionSize * 2), 0.f);

    for (int i = 0; i < m_fftSize; ++i) {
        const float inputReal = complexArray[i * 2];
        const float inputImag = complexArray[i * 2 + 1];
        const float chirpReal = m_bluesteinChirpTable[static_cast<size_t>(i * 2)];
        const float chirpImag = m_bluesteinChirpTable[static_cast<size_t>(i * 2 + 1)];

        work[static_cast<size_t>(i * 2)] = inputReal * chirpReal + inputImag * chirpImag;
        work[static_cast<size_t>(i * 2 + 1)] = inputImag * chirpReal - inputReal * chirpImag;
    }

    m_bluesteinFft->transform(work.data());
    for (int i = 0; i < convolutionSize; ++i) {
        const float workReal = work[static_cast<size_t>(i * 2)];
        const float workImag = work[static_cast<size_t>(i * 2 + 1)];
        const float kernelReal = m_bluesteinKernelFft[static_cast<size_t>(i * 2)];
        const float kernelImag = m_bluesteinKernelFft[static_cast<size_t>(i * 2 + 1)];

        work[static_cast<size_t>(i * 2)] = workReal * kernelReal - workImag * kernelImag;
        work[static_cast<size_t>(i * 2 + 1)] = workReal * kernelImag + workImag * kernelReal;
    }

    inverseTransform(work.data(), *m_bluesteinFft);
    for (int i = 0; i < m_fftSize; ++i) {
        const float workReal = work[static_cast<size_t>(i * 2)];
        const float workImag = work[static_cast<size_t>(i * 2 + 1)];
        const float chirpReal = m_bluesteinChirpTable[static_cast<size_t>(i * 2)];
        const float chirpImag = m_bluesteinChirpTable[static_cast<size_t>(i * 2 + 1)];

        complexArray[i * 2] = workReal * chirpReal + workImag * chirpImag;
        complexArray[i * 2 + 1] = workImag * chirpReal - workReal * chirpImag;
    }
}

Stft::Stft(int fftSize, int hopSize)
    : m_fftSize(fftSize)
    , m_hopSize(hopSize)
    , m_fft(fftSize)
{
    if (fftSize <= 0 || hopSize <= 0) {
        throw std::invalid_argument("STFT sizes must be positive.");
    }
    m_window.resize(static_cast<size_t>(m_fftSize));
    for (int i = 0; i < m_fftSize; ++i) {
        m_window[static_cast<size_t>(i)] = static_cast<float>(
            0.5 * (1.0 - std::cos((2.0 * M_PI * i) / m_fftSize)));
    }
    m_frame.resize(static_cast<size_t>(m_fftSize));
    m_complexFrame.resize(static_cast<size_t>(m_fftSize * 2));
}

Stft::~Stft() = default;

int Stft::frameCount(int signalLength) const
{
    if (signalLength < m_fftSize) {
        return 0;
    }
    return (signalLength - m_fftSize) / m_hopSize + 1;
}

int Stft::forEachSpectrum(
    const float *signal, int signalLength,
    const std::function<void(const float *complexFrame, int frameIndex)> &iteratee)
{
    const int frames = frameCount(signalLength);
    if (frames == 0) {
        return 0;
    }

    int frameIndex = 0;
    for (int i = 0; i + m_fftSize <= signalLength; i += m_hopSize) {
        for (int j = 0; j < m_fftSize; ++j) {
            m_frame[static_cast<size_t>(j)] =
                signal[i + j] * m_window[static_cast<size_t>(j)];
        }
        for (int j = 0; j < m_fftSize; ++j) {
            m_complexFrame[static_cast<size_t>(j * 2)] = m_frame[static_cast<size_t>(j)];
            m_complexFrame[static_cast<size_t>(j * 2 + 1)] = 0.f;
        }
        m_fft.transform(m_complexFrame.data());
        iteratee(m_complexFrame.data(), frameIndex);
        ++frameIndex;
    }
    return frameIndex;
}
