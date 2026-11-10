#pragma once

#include "rx/AudioCapture.h"
#include "rx/AudioRingBuffer.h"
#include "rx/DecodeEngine.h"
#include "rx/RxSpectrumWidget.h"

#include <QWidget>
#include <memory>

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
    void onStartRx();
    void onStopRx();
    void onWindowChanged(int index);
    void onBandwidthChanged(int index);
    void onCenterChanged(int value);
    void onWaterfallCenter(int hz);
    void onTranscriptUpdated(const QString &committed, const QString &pending);
    void onRxStatus(const QString &status);
    void onDecodeFailed(const QString &error);
    void onCaptureError(const QString &error);

private:
    void setListeningUi(bool listening);
    int selectedDeviceIndex() const;
    int selectedWindowSeconds() const;
    int selectedBandwidthHz() const;
    void applyRingCapacity();
    void clearTranscriptUi();

    std::unique_ptr<AudioRingBuffer> m_ring;
    AudioCapture *m_capture = nullptr;
    DecodeEngine *m_decode = nullptr;

    QComboBox *m_deviceCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QComboBox *m_windowCombo = nullptr;
    QComboBox *m_bandwidthCombo = nullptr;
    QSpinBox *m_centerSpin = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    RxSpectrumWidget *m_waterfall = nullptr;
    QPlainTextEdit *m_committedText = nullptr;
    QPlainTextEdit *m_pendingText = nullptr;
    QLabel *m_statusLabel = nullptr;

    bool m_listening = false;
    bool m_pausedForTx = false;
    bool m_ignoreCenterSpin = false;
};

} // namespace deepcw
