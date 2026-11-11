#pragma once

#include "rx/AudioCapture.h"
#include "rx/AudioMonitor.h"
#include "rx/AudioRingBuffer.h"
#include "rx/DecodeEngine.h"
#include "rx/MonitorFifo.h"
#include "rx/RxSpectrumWidget.h"

#include <QWidget>
#include <memory>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QPushButton;
class QPlainTextEdit;
class QLabel;

namespace deepcw {

class RxPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RxPanel(QWidget *parent = nullptr);
    ~RxPanel() override;

    /// Pause capture while TX is active (ARCHITECTURE_RX_DEEPCW.md §6).
    void setPausedForTx(bool paused);

private slots:
    void refreshDevices();
    void refreshOutputDevices();
    void onStartRx();
    void onStopRx();
    void onModeChanged(int index);
    void onWindowChanged(int index);
    void onBandwidthChanged(int index);
    void onCenterChanged(int value);
    void onWaterfallCenter(int hz);
    void onEnableMonitorToggled(bool checked);
    void onFilteredMonitorToggled(bool checked);
    void onOutputDeviceChanged(int index);
    void onTranscriptUpdated(const QString &committed, const QString &pending);
    void onPileupUpdated(const QVector<deepcw::PileupTrackText> &tracks);
    void onRxStatus(const QString &status);
    void onDecodeFailed(const QString &error);
    void onCaptureError(const QString &error);
    void onMonitorError(const QString &error);

private:
    void setListeningUi(bool listening);
    void applyModeUi();
    int selectedDeviceIndex() const;
    int selectedOutputDeviceIndex() const;
    int selectedWindowSeconds() const;
    int selectedBandwidthHz() const;
    DecodeEngine::Mode selectedMode() const;
    void applyRingCapacity();
    void clearDecodeUi();
    void applyMonitorFilterParams();
    void syncMonitorPlayback();
    void stopMonitor();

    std::unique_ptr<AudioRingBuffer> m_ring;
    std::unique_ptr<MonitorFifo> m_monitorFifo;
    AudioCapture *m_capture = nullptr;
    AudioMonitor *m_monitor = nullptr;
    DecodeEngine *m_decode = nullptr;

    QComboBox *m_deviceCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QComboBox *m_outputCombo = nullptr;
    QPushButton *m_refreshOutputButton = nullptr;
    QCheckBox *m_enableMonitorCheck = nullptr;
    QCheckBox *m_filteredMonitorCheck = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_windowCombo = nullptr;
    QComboBox *m_bandwidthCombo = nullptr;
    QSpinBox *m_centerSpin = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    RxSpectrumWidget *m_waterfall = nullptr;
    QLabel *m_transcriptLabel = nullptr;
    QPlainTextEdit *m_committedText = nullptr;
    QLabel *m_pendingLabel = nullptr;
    QPlainTextEdit *m_pendingText = nullptr;
    QLabel *m_pileupLabel = nullptr;
    QPlainTextEdit *m_pileupText = nullptr;
    QLabel *m_statusLabel = nullptr;

    bool m_listening = false;
    bool m_pausedForTx = false;
    bool m_ignoreCenterSpin = false;
    bool m_ignoreOutputCombo = false;
};

} // namespace deepcw
