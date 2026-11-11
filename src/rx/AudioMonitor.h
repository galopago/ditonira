#pragma once

#include "rx/AudioCapture.h"
#include "rx/BandpassFilter.h"
#include "rx/MonitorFifo.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>

namespace deepcw {

/**
 * PortAudio output of live RX audio (DeepCW THRU / ARCHITECTURE_RX_DEEPCW.md §7.4).
 * Consumes MonitorFifo filled by AudioCapture; optional streaming bandpass.
 */
class AudioMonitor : public QObject
{
    Q_OBJECT

public:
    explicit AudioMonitor(MonitorFifo *fifo, QObject *parent = nullptr);
    ~AudioMonitor() override;

    static QVector<AudioDeviceInfo> listOutputDevices();

    bool start(int deviceIndex, QString *errorOut = nullptr);
    void stop();
    bool isRunning() const;

    void setEnabled(bool enabled);
    void setFiltered(bool filtered);
    void setFilterParams(int centerHz, int bandwidthHz);
    void setMuted(bool muted);

signals:
    void monitorError(const QString &message);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    MonitorFifo *m_fifo = nullptr;
    bool m_paAcquired = false;

    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_filtered{true};
    std::atomic<bool> m_muted{false};
    std::atomic<int> m_centerHz{800};
    std::atomic<int> m_bandwidthHz{800};
};

} // namespace deepcw
