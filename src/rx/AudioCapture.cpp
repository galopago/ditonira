#include "rx/AudioCapture.h"

#include "rx/DeepCwConstants.h"
#include "rx/PortAudioHost.h"
#include "rx/WavIo.h"

#include <portaudio.h>

#include <array>
#include <cmath>

namespace deepcw {
namespace {

// Worst-case: 2048 frames @ 8 kHz → ~2457 @ 9600; leave headroom for higher rates.
constexpr int kResampleScratchCap = 8192;

struct StreamState {
    AudioRingBuffer *ring = nullptr;
    MonitorFifo *monitor = nullptr;
    std::atomic<bool> *paused = nullptr;
    int deviceRate = kSampleRate;
    std::array<float, kResampleScratchCap> scratch{};
};

void mirrorMonitor(StreamState *st, const float *samples, int count)
{
    if (st && st->monitor && samples && count > 0) {
        st->monitor->write(samples, count);
    }
}

int paCallback(const void *input, void * /*output*/, unsigned long frameCount,
               const PaStreamCallbackTimeInfo * /*timeInfo*/,
               PaStreamCallbackFlags /*statusFlags*/, void *userData)
{
    auto *st = static_cast<StreamState *>(userData);
    if (!st || !input || !st->ring) {
        return paContinue;
    }
    if (st->paused && st->paused->load(std::memory_order_relaxed)) {
        return paContinue;
    }

    const float *in = static_cast<const float *>(input);
    const int frames = static_cast<int>(frameCount);
    if (frames <= 0) {
        return paContinue;
    }

    if (st->deviceRate == kSampleRate) {
        st->ring->write(in, frames);
        mirrorMonitor(st, in, frames);
        return paContinue;
    }

    const int written = resampleLinearInto(in, frames, st->deviceRate, kSampleRate,
                                           st->scratch.data(), kResampleScratchCap);
    if (written > 0) {
        st->ring->write(st->scratch.data(), written);
        mirrorMonitor(st, st->scratch.data(), written);
    }
    return paContinue;
}

} // namespace

struct AudioCapture::Impl {
    PaStream *stream = nullptr;
    StreamState state;
};

AudioCapture::AudioCapture(AudioRingBuffer *ring, MonitorFifo *monitorFifo, QObject *parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
    , m_ring(ring)
    , m_monitorFifo(monitorFifo)
{
    m_paAcquired = PortAudioHost::acquire();
}

AudioCapture::~AudioCapture()
{
    stop();
    if (m_paAcquired) {
        PortAudioHost::release();
        m_paAcquired = false;
    }
}

QVector<AudioDeviceInfo> AudioCapture::listInputDevices()
{
    QVector<AudioDeviceInfo> out;
    // Borrow PA without tearing it down afterwards (§10.1).
    if (!PortAudioHost::acquire()) {
        return out;
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
    PortAudioHost::release();
    return out;
}

bool AudioCapture::isRunning() const
{
    return m_impl && m_impl->stream != nullptr;
}

void AudioCapture::setPaused(bool paused)
{
    m_paused.store(paused, std::memory_order_relaxed);
}

bool AudioCapture::start(int deviceIndex, QString *errorOut)
{
    stop();
    if (!m_paAcquired || !PortAudioHost::isReady()) {
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

    if (m_monitorFifo) {
        m_monitorFifo->clear();
    }
    m_impl->state.ring = m_ring;
    m_impl->state.monitor = m_monitorFifo;
    m_impl->state.paused = &m_paused;
    m_impl->state.deviceRate = static_cast<int>(std::lround(rate));

    PaError err = Pa_OpenStream(&m_impl->stream, &params, nullptr, rate,
                                static_cast<unsigned long>(kAudioChunkSamples),
                                paClipOff, paCallback, &m_impl->state);
    if (err != paNoError) {
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
    m_paused.store(false, std::memory_order_relaxed);
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
