#pragma once

#include "rx/AudioRingBuffer.h"
#include "rx/MonitorFifo.h"

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
 * PortAudio capture → resample to 9600 Hz → lock-free AudioRingBuffer.
 * Also mirrors 9600 Hz samples into MonitorFifo for THRU (§7.4).
 * Callback is realtime-safe (§10.1): no heap, no mutex, no Qt.
 */
class AudioCapture : public QObject
{
    Q_OBJECT

public:
    explicit AudioCapture(AudioRingBuffer *ring, MonitorFifo *monitorFifo,
                          QObject *parent = nullptr);
    ~AudioCapture() override;

    static QVector<AudioDeviceInfo> listInputDevices();

    bool start(int deviceIndex, QString *errorOut = nullptr);
    void stop();
    bool isRunning() const;

    void setPaused(bool paused);
    bool isPaused() const { return m_paused.load(); }

signals:
    void captureError(const QString &message);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    AudioRingBuffer *m_ring = nullptr;
    MonitorFifo *m_monitorFifo = nullptr;
    std::atomic<bool> m_paused{false};
    bool m_paAcquired = false;
};

} // namespace deepcw
