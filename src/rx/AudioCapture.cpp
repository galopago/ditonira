#include "rx/AudioCapture.h"

#include "rx/DeepCwConstants.h"
#include "rx/WavIo.h"

#include <portaudio.h>

#include <cmath>
#include <vector>

namespace deepcw {
namespace {

struct StreamState {
    AudioRingBuffer *ring = nullptr;
    std::atomic<bool> *paused = nullptr;
    AudioCapture::SampleTap tap = nullptr;
    void *tapUser = nullptr;
    int deviceRate = kSampleRate;
    double resamplePos = 0.0;
    float lastSample = 0.f;
    std::vector<float> monoScratch;
};

void deliverSamples(StreamState *st, const float *samples, int count)
{
    if (!st || !samples || count <= 0) {
        return;
    }
    if (st->ring) {
        st->ring->write(samples, count);
    }
    if (st->tap) {
        st->tap(samples, count, st->tapUser);
    }
}

int paCallback(const void *input, void * /*output*/, unsigned long frameCount,
               const PaStreamCallbackTimeInfo * /*timeInfo*/,
               PaStreamCallbackFlags /*statusFlags*/, void *userData)
{
    auto *st = static_cast<StreamState *>(userData);
    if (!st || !input) {
        return paContinue;
    }
    if (st->paused && st->paused->load()) {
        return paContinue;
    }

    const float *in = static_cast<const float *>(input);
    const int frames = static_cast<int>(frameCount);

    // Device opened as mono float32.
    if (st->deviceRate == kSampleRate) {
        deliverSamples(st, in, frames);
        return paContinue;
    }

    // Chunk-wise linear resample to 9600 Hz (RX-2; document Speex later if needed).
    st->monoScratch = resampleLinear(in, frames, st->deviceRate, kSampleRate);
    if (!st->monoScratch.empty()) {
        deliverSamples(st, st->monoScratch.data(), static_cast<int>(st->monoScratch.size()));
    }
    return paContinue;
}

} // namespace

struct AudioCapture::Impl {
    bool paReady = false;
    PaStream *stream = nullptr;
    StreamState state;
};

AudioCapture::AudioCapture(AudioRingBuffer *ring, QObject *parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
    , m_ring(ring)
{
    if (Pa_Initialize() == paNoError) {
        m_impl->paReady = true;
    }
}

AudioCapture::~AudioCapture()
{
    stop();
    if (m_impl->paReady) {
        Pa_Terminate();
        m_impl->paReady = false;
    }
}

QVector<AudioDeviceInfo> AudioCapture::listInputDevices()
{
    QVector<AudioDeviceInfo> out;
    const bool needInit = (Pa_GetDeviceCount() < 0);
    if (needInit) {
        Pa_Initialize();
    }
    const int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info || info->maxInputChannels <= 0) {
            continue;
        }
        AudioDeviceInfo d;
        d.index = i;
        d.name = QString::fromUtf8(info->name);
        d.maxInputChannels = info->maxInputChannels;
        d.defaultSampleRate = info->defaultSampleRate;
        out.push_back(d);
    }
    if (needInit) {
        Pa_Terminate();
    }
    return out;
}

bool AudioCapture::isRunning() const
{
    return m_impl && m_impl->stream != nullptr;
}

void AudioCapture::setPaused(bool paused)
{
    m_paused.store(paused);
}

void AudioCapture::setSampleTap(SampleTap tap, void *user)
{
    m_impl->state.tap = tap;
    m_impl->state.tapUser = user;
}

bool AudioCapture::start(int deviceIndex, QString *errorOut)
{
    stop();
    if (!m_impl->paReady) {
        if (errorOut) {
            *errorOut = QStringLiteral("PortAudio is not initialized");
        }
        return false;
    }
    if (!m_ring) {
        if (errorOut) {
            *errorOut = QStringLiteral("No audio ring buffer");
        }
        return false;
    }

    const PaDeviceInfo *info = Pa_GetDeviceInfo(deviceIndex);
    if (!info || info->maxInputChannels <= 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not open audio device");
        }
        return false;
    }

    double rate = info->defaultSampleRate;
    if (rate < 8000.0) {
        rate = 48000.0;
    }

    PaStreamParameters params{};
    params.device = deviceIndex;
    params.channelCount = 1;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = info->defaultLowInputLatency;
    params.hostApiSpecificStreamInfo = nullptr;

    m_impl->state.ring = m_ring;
    m_impl->state.paused = &m_paused;
    m_impl->state.deviceRate = static_cast<int>(std::lround(rate));
    m_impl->state.resamplePos = 0.0;
    m_impl->state.lastSample = 0.f;

    PaError err = Pa_OpenStream(&m_impl->stream, &params, nullptr, rate,
                                static_cast<unsigned long>(kAudioChunkSamples),
                                paClipOff, paCallback, &m_impl->state);
    if (err != paNoError) {
        // Fallback: try exactly 9600 Hz.
        m_impl->state.deviceRate = kSampleRate;
        err = Pa_OpenStream(&m_impl->stream, &params, nullptr, kSampleRate,
                            static_cast<unsigned long>(kAudioChunkSamples), paClipOff,
                            paCallback, &m_impl->state);
    }
    if (err != paNoError) {
        m_impl->stream = nullptr;
        if (errorOut) {
            *errorOut = QStringLiteral("Could not open audio device: %1")
                            .arg(QString::fromUtf8(Pa_GetErrorText(err)));
        }
        return false;
    }

    err = Pa_StartStream(m_impl->stream);
    if (err != paNoError) {
        Pa_CloseStream(m_impl->stream);
        m_impl->stream = nullptr;
        if (errorOut) {
            *errorOut = QStringLiteral("Could not open audio device: %1")
                            .arg(QString::fromUtf8(Pa_GetErrorText(err)));
        }
        return false;
    }
    m_paused.store(false);
    return true;
}

void AudioCapture::stop()
{
    if (!m_impl->stream) {
        return;
    }
    Pa_StopStream(m_impl->stream);
    Pa_CloseStream(m_impl->stream);
    m_impl->stream = nullptr;
}

} // namespace deepcw
