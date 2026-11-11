#include "rx/AudioMonitor.h"

#include "rx/DeepCwConstants.h"
#include "rx/PortAudioHost.h"
#include "rx/WavIo.h"

#include <portaudio.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace deepcw {
namespace {

constexpr int kOutScratchCap = 8192;

struct MonitorState {
    MonitorFifo *fifo = nullptr;
    StreamingBandpass *filter = nullptr;
    std::atomic<bool> *enabled = nullptr;
    std::atomic<bool> *filtered = nullptr;
    std::atomic<bool> *muted = nullptr;
    std::atomic<int> *centerHz = nullptr;
    std::atomic<int> *bandwidthHz = nullptr;
    int deviceRate = kSampleRate;
    int lastCenter = -1;
    int lastBw = -1;
    bool lastFiltered = false;
    std::array<float, kOutScratchCap> scratch{};
    std::array<float, kOutScratchCap> upsample{};
};

void maybeUpdateFilter(MonitorState *st)
{
    if (!st || !st->filter || !st->filtered || !st->centerHz || !st->bandwidthHz) {
        return;
    }
    const bool filt = st->filtered->load(std::memory_order_relaxed);
    const int center = st->centerHz->load(std::memory_order_relaxed);
    const int bw = st->bandwidthHz->load(std::memory_order_relaxed);
    if (filt == st->lastFiltered && center == st->lastCenter && bw == st->lastBw) {
        return;
    }
    st->lastFiltered = filt;
    st->lastCenter = center;
    st->lastBw = bw;
    if (filt && center > 0 && bw > 0) {
        st->filter->setParams(kSampleRate, static_cast<float>(center), static_cast<float>(bw));
    } else {
        st->filter->reset();
    }
}

int paOutCallback(const void * /*input*/, void *output, unsigned long frameCount,
                  const PaStreamCallbackTimeInfo * /*timeInfo*/,
                  PaStreamCallbackFlags /*statusFlags*/, void *userData)
{
    auto *st = static_cast<MonitorState *>(userData);
    float *out = static_cast<float *>(output);
    const int frames = static_cast<int>(frameCount);
    if (!st || !out || frames <= 0) {
        return paContinue;
    }

    const bool silent =
        !st->fifo || !st->enabled || !st->enabled->load(std::memory_order_relaxed)
        || (st->muted && st->muted->load(std::memory_order_relaxed));
    if (silent) {
        for (int i = 0; i < frames; ++i) {
            out[i] = 0.f;
        }
        return paContinue;
    }

    maybeUpdateFilter(st);

    if (st->deviceRate == kSampleRate) {
        const int n = std::min(frames, kOutScratchCap);
        const int got = st->fifo->read(st->scratch.data(), n);
        for (int i = 0; i < got; ++i) {
            out[i] = st->scratch[static_cast<size_t>(i)];
        }
        for (int i = got; i < frames; ++i) {
            out[i] = 0.f;
        }
        if (st->filtered && st->filtered->load(std::memory_order_relaxed) && st->filter) {
            st->filter->processInPlace(out, frames);
        }
        return paContinue;
    }

    // Output device rate ≠ 9600: pull enough 9600 Hz samples then upsample.
    const double ratio = static_cast<double>(kSampleRate) / static_cast<double>(st->deviceRate);
    const int need9600 =
        std::min(kOutScratchCap, std::max(1, static_cast<int>(std::ceil(frames * ratio)) + 2));
    const int got = st->fifo->read(st->scratch.data(), need9600);
    for (int i = got; i < need9600; ++i) {
        st->scratch[static_cast<size_t>(i)] = 0.f;
    }
    if (st->filtered && st->filtered->load(std::memory_order_relaxed) && st->filter) {
        st->filter->processInPlace(st->scratch.data(), need9600);
    }
    const int written = resampleLinearInto(st->scratch.data(), need9600, kSampleRate,
                                           st->deviceRate, st->upsample.data(), kOutScratchCap);
    const int n = std::min(frames, written);
    for (int i = 0; i < n; ++i) {
        out[i] = st->upsample[static_cast<size_t>(i)];
    }
    for (int i = n; i < frames; ++i) {
        out[i] = 0.f;
    }
    return paContinue;
}

} // namespace

struct AudioMonitor::Impl {
    PaStream *stream = nullptr;
    MonitorState state;
    StreamingBandpass filter;
};

AudioMonitor::AudioMonitor(MonitorFifo *fifo, QObject *parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
    , m_fifo(fifo)
{
    m_paAcquired = PortAudioHost::acquire();
    m_impl->state.fifo = fifo;
    m_impl->state.filter = &m_impl->filter;
    m_impl->state.enabled = &m_enabled;
    m_impl->state.filtered = &m_filtered;
    m_impl->state.muted = &m_muted;
    m_impl->state.centerHz = &m_centerHz;
    m_impl->state.bandwidthHz = &m_bandwidthHz;
}

AudioMonitor::~AudioMonitor()
{
    stop();
    if (m_paAcquired) {
        PortAudioHost::release();
        m_paAcquired = false;
    }
}

QVector<AudioDeviceInfo> AudioMonitor::listOutputDevices()
{
    QVector<AudioDeviceInfo> out;
    if (!PortAudioHost::acquire()) {
        return out;
    }
    const int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info || info->maxOutputChannels <= 0) {
            continue;
        }
        AudioDeviceInfo d;
        d.index = i;
        d.name = QString::fromUtf8(info->name);
        d.maxInputChannels = info->maxOutputChannels;
        d.defaultSampleRate = info->defaultSampleRate;
        out.push_back(d);
    }
    PortAudioHost::release();
    return out;
}

bool AudioMonitor::isRunning() const
{
    return m_impl && m_impl->stream != nullptr;
}

void AudioMonitor::setEnabled(bool enabled)
{
    m_enabled.store(enabled, std::memory_order_relaxed);
}

void AudioMonitor::setFiltered(bool filtered)
{
    m_filtered.store(filtered, std::memory_order_relaxed);
}

void AudioMonitor::setFilterParams(int centerHz, int bandwidthHz)
{
    m_centerHz.store(centerHz, std::memory_order_relaxed);
    m_bandwidthHz.store(std::max(1, bandwidthHz), std::memory_order_relaxed);
}

void AudioMonitor::setMuted(bool muted)
{
    m_muted.store(muted, std::memory_order_relaxed);
}

bool AudioMonitor::start(int deviceIndex, QString *errorOut)
{
    stop();
    if (!m_paAcquired || !PortAudioHost::isReady()) {
        if (errorOut) {
            *errorOut = QStringLiteral("PortAudio is not initialized");
        }
        return false;
    }
    if (!m_fifo) {
        if (errorOut) {
            *errorOut = QStringLiteral("No monitor buffer");
        }
        return false;
    }

    const PaDeviceInfo *info = Pa_GetDeviceInfo(deviceIndex);
    if (!info || info->maxOutputChannels <= 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not open audio output");
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
    params.suggestedLatency = info->defaultLowOutputLatency;
    params.hostApiSpecificStreamInfo = nullptr;

    m_fifo->clear();
    m_impl->filter.reset();
    m_impl->state.deviceRate = kSampleRate;
    m_impl->state.lastCenter = -1;
    m_impl->state.lastBw = -1;
    m_impl->state.lastFiltered = false;

    PaError err = Pa_OpenStream(&m_impl->stream, nullptr, &params, kSampleRate,
                                static_cast<unsigned long>(kAudioChunkSamples), paClipOff,
                                paOutCallback, &m_impl->state);
    if (err != paNoError) {
        m_impl->state.deviceRate = static_cast<int>(std::lround(rate));
        err = Pa_OpenStream(&m_impl->stream, nullptr, &params, rate,
                            static_cast<unsigned long>(kAudioChunkSamples), paClipOff,
                            paOutCallback, &m_impl->state);
    }
    if (err != paNoError) {
        m_impl->stream = nullptr;
        if (errorOut) {
            *errorOut = QStringLiteral("Could not open audio output: %1")
                            .arg(QString::fromUtf8(Pa_GetErrorText(err)));
        }
        return false;
    }

    err = Pa_StartStream(m_impl->stream);
    if (err != paNoError) {
        Pa_CloseStream(m_impl->stream);
        m_impl->stream = nullptr;
        if (errorOut) {
            *errorOut = QStringLiteral("Could not open audio output: %1")
                            .arg(QString::fromUtf8(Pa_GetErrorText(err)));
        }
        return false;
    }
    return true;
}

void AudioMonitor::stop()
{
    if (!m_impl->stream) {
        return;
    }
    Pa_StopStream(m_impl->stream);
    Pa_CloseStream(m_impl->stream);
    m_impl->stream = nullptr;
    if (m_fifo) {
        m_fifo->clear();
    }
}

} // namespace deepcw
