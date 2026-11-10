#pragma once

#include "rx/AudioRingBuffer.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>

namespace deepcw {

struct AudioDeviceInfo {
    int index = -1; // PortAudio device index
    QString name;
    int maxInputChannels = 0;
    double defaultSampleRate = 0.0;
};

/**
 * PortAudio capture → resample to 9600 Hz → AudioRingBuffer.
 * Callback never runs ORT/STFT.
 */
class AudioCapture : public QObject
{
    Q_OBJECT

public:
    explicit AudioCapture(AudioRingBuffer *ring, QObject *parent = nullptr);
    ~AudioCapture() override;

    static QVector<AudioDeviceInfo> listInputDevices();

    bool start(int deviceIndex, QString *errorOut = nullptr);
    void stop();
    bool isRunning() const;

    void setPaused(bool paused);
    bool isPaused() const { return m_paused.load(); }

    /// Optional tap of resampled 9600 Hz mono samples (after ring write). PA callback only.
    using SampleTap = void (*)(const float *samples, int count, void *user);
    void setSampleTap(SampleTap tap, void *user);

signals:
    void captureError(const QString &message);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    AudioRingBuffer *m_ring = nullptr;
    std::atomic<bool> m_paused{false};
};

} // namespace deepcw
