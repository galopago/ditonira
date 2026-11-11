#include "rx/RxPanel.h"

#include "rx/DeepCwConstants.h"
#include "rx/ModelPath.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextCursor>
#include <QVBoxLayout>

namespace deepcw {

RxPanel::RxPanel(QWidget *parent)
    : QWidget(parent)
    , m_ring(std::make_unique<AudioRingBuffer>(kDefaultDecodeWindowS * kSampleRate))
    , m_monitorFifo(std::make_unique<MonitorFifo>())
{
    m_capture = new AudioCapture(m_ring.get(), m_monitorFifo.get(), this);
    m_monitor = new AudioMonitor(m_monitorFifo.get(), this);
    m_decode = new DecodeEngine(m_ring.get(), this);

    connect(m_capture, &AudioCapture::captureError, this, &RxPanel::onCaptureError);
    connect(m_monitor, &AudioMonitor::monitorError, this, &RxPanel::onMonitorError);
    connect(m_decode, &DecodeEngine::transcriptUpdated, this, &RxPanel::onTranscriptUpdated);
    connect(m_decode, &DecodeEngine::pileupUpdated, this, &RxPanel::onPileupUpdated);
    connect(m_decode, &DecodeEngine::rxStatusChanged, this, &RxPanel::onRxStatus);
    connect(m_decode, &DecodeEngine::decodeFailed, this, &RxPanel::onDecodeFailed);

    auto *root = new QVBoxLayout(this);

    auto *devRow = new QHBoxLayout();
    devRow->addWidget(new QLabel(tr("Input:"), this));
    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setMinimumWidth(260);
    devRow->addWidget(m_deviceCombo, 1);
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &RxPanel::refreshDevices);
    devRow->addWidget(m_refreshButton);
    root->addLayout(devRow);

    auto *outRow = new QHBoxLayout();
    outRow->addWidget(new QLabel(tr("Output:"), this));
    m_outputCombo = new QComboBox(this);
    m_outputCombo->setMinimumWidth(260);
    connect(m_outputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &RxPanel::onOutputDeviceChanged);
    outRow->addWidget(m_outputCombo, 1);
    m_refreshOutputButton = new QPushButton(tr("Refresh"), this);
    connect(m_refreshOutputButton, &QPushButton::clicked, this, &RxPanel::refreshOutputDevices);
    outRow->addWidget(m_refreshOutputButton);
    root->addLayout(outRow);

    auto *monRow = new QHBoxLayout();
    m_enableMonitorCheck = new QCheckBox(tr("Enable monitor"), this);
    connect(m_enableMonitorCheck, &QCheckBox::toggled, this, &RxPanel::onEnableMonitorToggled);
    monRow->addWidget(m_enableMonitorCheck);
    m_filteredMonitorCheck = new QCheckBox(tr("Filtered (center/BW)"), this);
    m_filteredMonitorCheck->setChecked(true);
    connect(m_filteredMonitorCheck, &QCheckBox::toggled, this, &RxPanel::onFilteredMonitorToggled);
    monRow->addWidget(m_filteredMonitorCheck);
    monRow->addStretch(1);
    root->addLayout(monRow);

    auto *optsRow = new QHBoxLayout();
    optsRow->addWidget(new QLabel(tr("Mode:"), this));
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Normal"), static_cast<int>(DecodeEngine::Mode::Normal));
    m_modeCombo->addItem(tr("Pileup"), static_cast<int>(DecodeEngine::Mode::Pileup));
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &RxPanel::onModeChanged);
    optsRow->addWidget(m_modeCombo);

    optsRow->addWidget(new QLabel(tr("Window:"), this));
    m_windowCombo = new QComboBox(this);
    for (int s : {6, 12, 18, 30}) {
        m_windowCombo->addItem(tr("%1 s").arg(s), s);
    }
    m_windowCombo->setCurrentIndex(1); // 12 s
    connect(m_windowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &RxPanel::onWindowChanged);
    optsRow->addWidget(m_windowCombo);

    optsRow->addWidget(new QLabel(tr("Bandwidth:"), this));
    m_bandwidthCombo = new QComboBox(this);
    for (int bw : {100, 150, 250, 500, 800}) {
        m_bandwidthCombo->addItem(tr("%1 Hz").arg(bw), bw);
    }
    m_bandwidthCombo->setCurrentIndex(4); // 800
    connect(m_bandwidthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &RxPanel::onBandwidthChanged);
    optsRow->addWidget(m_bandwidthCombo);

    optsRow->addWidget(new QLabel(tr("Center Hz:"), this));
    m_centerSpin = new QSpinBox(this);
    m_centerSpin->setRange(kDecodableMinFreqHz, kDecodableMaxFreqHz);
    m_centerSpin->setValue(kDefaultCenterFreqHz);
    connect(m_centerSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &RxPanel::onCenterChanged);
    optsRow->addWidget(m_centerSpin);
    optsRow->addStretch(1);
    root->addLayout(optsRow);

    auto *btnRow = new QHBoxLayout();
    m_startButton = new QPushButton(tr("Start RX"), this);
    m_stopButton = new QPushButton(tr("Stop RX"), this);
    m_stopButton->setEnabled(false);
    connect(m_startButton, &QPushButton::clicked, this, &RxPanel::onStartRx);
    connect(m_stopButton, &QPushButton::clicked, this, &RxPanel::onStopRx);
    btnRow->addWidget(m_startButton);
    btnRow->addWidget(m_stopButton);
    btnRow->addStretch(1);
    root->addLayout(btnRow);

    root->addWidget(new QLabel(tr("Waterfall (100–1500 Hz)"), this));
    m_waterfall = new RxSpectrumWidget(this);
    m_waterfall->setCenterHz(m_centerSpin->value());
    m_waterfall->setBandwidthHz(selectedBandwidthHz());
    connect(m_decode, &DecodeEngine::spectrumUpdated, m_waterfall,
            &RxSpectrumWidget::appendSpectrumColumn);
    connect(m_waterfall, &RxSpectrumWidget::centerHzSelected, this, &RxPanel::onWaterfallCenter);
    root->addWidget(m_waterfall, 2);

    m_transcriptLabel = new QLabel(tr("Transcript"), this);
    root->addWidget(m_transcriptLabel);
    m_committedText = new QPlainTextEdit(this);
    m_committedText->setReadOnly(true);
    m_committedText->setPlaceholderText(tr("Committed transcript"));
    root->addWidget(m_committedText, 1);

    m_pendingLabel = new QLabel(tr("Live tail"), this);
    root->addWidget(m_pendingLabel);
    m_pendingText = new QPlainTextEdit(this);
    m_pendingText->setReadOnly(true);
    m_pendingText->setMaximumHeight(80);
    m_pendingText->setPlaceholderText(tr("Live decode (pending)"));
    root->addWidget(m_pendingText);

    m_pileupLabel = new QLabel(tr("Pileup tracks"), this);
    root->addWidget(m_pileupLabel);
    m_pileupText = new QPlainTextEdit(this);
    m_pileupText->setReadOnly(true);
    m_pileupText->setPlaceholderText(tr("Locked tracks (Hz + text)"));
    root->addWidget(m_pileupText, 1);

    m_statusLabel = new QLabel(tr("Ready"), this);
    root->addWidget(m_statusLabel);

    refreshDevices();
    refreshOutputDevices();
    applyRingCapacity();
    applyModeUi();
    applyMonitorFilterParams();
    m_monitor->setFiltered(m_filteredMonitorCheck->isChecked());
    m_decode->setCenterHz(m_centerSpin->value());
    m_decode->setBandwidthHz(selectedBandwidthHz());
    m_decode->setWindowSeconds(selectedWindowSeconds());
    m_decode->setMode(selectedMode());
}

RxPanel::~RxPanel()
{
    onStopRx();
    stopMonitor();
}

void RxPanel::applyMonitorFilterParams()
{
    m_monitor->setFilterParams(m_centerSpin->value(), selectedBandwidthHz());
}

void RxPanel::stopMonitor()
{
    m_monitor->setEnabled(false);
    m_monitor->stop();
}

void RxPanel::syncMonitorPlayback()
{
    if (!m_enableMonitorCheck->isChecked()) {
        stopMonitor();
        return;
    }

    const int outDev = selectedOutputDeviceIndex();
    if (outDev < 0) {
        stopMonitor();
        m_statusLabel->setText(tr("Could not open audio output"));
        return;
    }

    applyMonitorFilterParams();
    m_monitor->setFiltered(m_filteredMonitorCheck->isChecked());
    m_monitor->setMuted(m_pausedForTx || !m_listening);
    m_monitor->setEnabled(true);

    if (!m_monitor->isRunning()) {
        QString err;
        if (!m_monitor->start(outDev, &err)) {
            m_monitor->setEnabled(false);
            m_statusLabel->setText(err.isEmpty() ? tr("Could not open audio output") : err);
        }
    }
}

void RxPanel::setPausedForTx(bool paused)
{
    m_pausedForTx = paused;
    m_monitor->setMuted(paused || !m_listening);
    if (!m_listening) {
        return;
    }
    m_capture->setPaused(paused);
    m_decode->setSpectrumPaused(paused);
    if (paused) {
        m_statusLabel->setText(tr("RX paused while transmitting"));
    } else {
        m_statusLabel->setText(tr("Listening…"));
        m_decode->requestDecodeWake();
    }
}

void RxPanel::refreshDevices()
{
    const int previous = selectedDeviceIndex();
    m_deviceCombo->clear();
    const auto devices = AudioCapture::listInputDevices();
    for (const AudioDeviceInfo &d : devices) {
        m_deviceCombo->addItem(d.name, d.index);
    }
    if (m_deviceCombo->count() == 0) {
        m_deviceCombo->addItem(tr("(no input devices)"), -1);
        m_statusLabel->setText(tr("Could not open audio device"));
    } else {
        const int idx = m_deviceCombo->findData(previous);
        if (idx >= 0) {
            m_deviceCombo->setCurrentIndex(idx);
        }
    }
}

void RxPanel::refreshOutputDevices()
{
    const int previous = selectedOutputDeviceIndex();
    m_ignoreOutputCombo = true;
    m_outputCombo->clear();
    const auto devices = AudioMonitor::listOutputDevices();
    for (const AudioDeviceInfo &d : devices) {
        m_outputCombo->addItem(d.name, d.index);
    }
    if (m_outputCombo->count() == 0) {
        m_outputCombo->addItem(tr("(no output devices)"), -1);
    } else {
        const int idx = m_outputCombo->findData(previous);
        if (idx >= 0) {
            m_outputCombo->setCurrentIndex(idx);
        }
    }
    m_ignoreOutputCombo = false;
    if (m_enableMonitorCheck->isChecked()) {
        // Device list changed — reopen if needed.
        stopMonitor();
        syncMonitorPlayback();
    }
}

int RxPanel::selectedDeviceIndex() const
{
    return m_deviceCombo->currentData().toInt();
}

int RxPanel::selectedOutputDeviceIndex() const
{
    return m_outputCombo->currentData().toInt();
}

int RxPanel::selectedWindowSeconds() const
{
    return m_windowCombo->currentData().toInt();
}

int RxPanel::selectedBandwidthHz() const
{
    return m_bandwidthCombo->currentData().toInt();
}

DecodeEngine::Mode RxPanel::selectedMode() const
{
    return static_cast<DecodeEngine::Mode>(m_modeCombo->currentData().toInt());
}

void RxPanel::applyRingCapacity()
{
    const bool pileup = selectedMode() == DecodeEngine::Mode::Pileup;
    const int seconds = pileup ? kPileupWindowS : selectedWindowSeconds();
    m_ring->resize(seconds * kSampleRate);
    m_decode->setWindowSeconds(seconds);
}

void RxPanel::applyModeUi()
{
    const bool pileup = selectedMode() == DecodeEngine::Mode::Pileup;
    m_transcriptLabel->setVisible(!pileup);
    m_committedText->setVisible(!pileup);
    m_pendingLabel->setVisible(!pileup);
    m_pendingText->setVisible(!pileup);
    m_pileupLabel->setVisible(pileup);
    m_pileupText->setVisible(pileup);
    m_windowCombo->setEnabled(!pileup && !m_listening);
    m_bandwidthCombo->setEnabled(!pileup);
    m_centerSpin->setEnabled(!pileup);
    if (pileup) {
        m_waterfall->setBandwidthHz(static_cast<int>(kPileupFilterWidthHz));
    } else {
        m_waterfall->setBandwidthHz(selectedBandwidthHz());
        m_waterfall->setPileupMarkers({});
    }
}

void RxPanel::clearDecodeUi()
{
    m_committedText->clear();
    m_pendingText->clear();
    m_pileupText->clear();
    m_waterfall->setPileupMarkers({});
}

void RxPanel::setListeningUi(bool listening)
{
    m_listening = listening;
    m_startButton->setEnabled(!listening);
    m_stopButton->setEnabled(listening);
    m_deviceCombo->setEnabled(!listening);
    m_refreshButton->setEnabled(!listening);
    m_modeCombo->setEnabled(!listening);
    m_windowCombo->setEnabled(!listening && selectedMode() == DecodeEngine::Mode::Normal);
    m_monitor->setMuted(m_pausedForTx || !listening);
}

void RxPanel::onStartRx()
{
    const int device = selectedDeviceIndex();
    if (device < 0) {
        m_statusLabel->setText(tr("Could not open audio device"));
        return;
    }

    QString err;
    const QString widePath = findDeepCwModelPath();
    const QString narrowPath = findDeepCwNarrowModelPath();
    if (!m_decode->loadModels(widePath, narrowPath, &err)) {
        m_statusLabel->setText(tr("Could not load CW model"));
        return;
    }

    applyRingCapacity();
    m_ring->clear();
    m_monitorFifo->clear();
    m_waterfall->clearWaterfall();
    m_decode->resetStreaming();
    m_decode->resetPileup();
    clearDecodeUi();
    m_decode->setMode(selectedMode());
    m_decode->setCenterHz(m_centerSpin->value());
    m_decode->setBandwidthHz(selectedBandwidthHz());
    m_waterfall->setCenterHz(m_centerSpin->value());
    applyModeUi();
    applyMonitorFilterParams();

    if (!m_capture->start(device, &err)) {
        m_statusLabel->setText(err.isEmpty() ? tr("Could not open audio device") : err);
        return;
    }

    m_decode->setSpectrumPaused(m_pausedForTx);
    if (m_pausedForTx) {
        m_capture->setPaused(true);
        m_statusLabel->setText(tr("RX paused while transmitting"));
    }

    if (!m_decode->isRunning()) {
        m_decode->start();
    } else {
        m_decode->requestDecodeWake();
    }

    setListeningUi(true);
    syncMonitorPlayback();
    if (!m_pausedForTx) {
        m_statusLabel->setText(tr("Listening…"));
    }
}

void RxPanel::onStopRx()
{
    m_capture->stop();
    m_monitor->setMuted(true);
    m_decode->setSpectrumPaused(true);
    if (m_decode->isRunning()) {
        m_decode->requestStop();
        m_decode->wait(5000);
    }
    m_decode->resetStreaming();
    m_decode->resetPileup();
    m_waterfall->clearWaterfall();
    setListeningUi(false);
    m_statusLabel->setText(tr("Ready"));
}

void RxPanel::onEnableMonitorToggled(bool)
{
    syncMonitorPlayback();
}

void RxPanel::onFilteredMonitorToggled(bool checked)
{
    m_monitor->setFiltered(checked);
    applyMonitorFilterParams();
}

void RxPanel::onOutputDeviceChanged(int)
{
    if (m_ignoreOutputCombo) {
        return;
    }
    if (m_enableMonitorCheck->isChecked()) {
        stopMonitor();
        syncMonitorPlayback();
    }
}

void RxPanel::onModeChanged(int)
{
    m_decode->setMode(selectedMode());
    applyModeUi();
    applyRingCapacity();
    clearDecodeUi();
    m_decode->resetStreaming();
    m_decode->resetPileup();
    m_decode->requestDecodeWake();
}

void RxPanel::onWindowChanged(int)
{
    if (!m_listening) {
        applyRingCapacity();
    }
}

void RxPanel::onBandwidthChanged(int)
{
    if (selectedMode() == DecodeEngine::Mode::Pileup) {
        return;
    }
    const int bw = selectedBandwidthHz();
    m_decode->setBandwidthHz(bw);
    m_waterfall->setBandwidthHz(bw);
    applyMonitorFilterParams();
    m_decode->clearStreamingPending();
    m_pendingText->clear();
    m_decode->requestDecodeWake();
}

void RxPanel::onCenterChanged(int value)
{
    if (m_ignoreCenterSpin || selectedMode() == DecodeEngine::Mode::Pileup) {
        return;
    }
    m_decode->setCenterHz(value);
    m_waterfall->setCenterHz(value);
    applyMonitorFilterParams();
    m_decode->clearStreamingPending();
    m_pendingText->clear();
    m_decode->requestDecodeWake();
}

void RxPanel::onWaterfallCenter(int hz)
{
    if (selectedMode() == DecodeEngine::Mode::Pileup) {
        return;
    }
    m_ignoreCenterSpin = true;
    m_centerSpin->setValue(hz);
    m_ignoreCenterSpin = false;
    m_decode->setCenterHz(hz);
    applyMonitorFilterParams();
    m_decode->clearStreamingPending();
    m_pendingText->clear();
    m_decode->requestDecodeWake();
}

void RxPanel::onTranscriptUpdated(const QString &committed, const QString &pending)
{
    if (selectedMode() != DecodeEngine::Mode::Normal) {
        return;
    }
    if (m_committedText->toPlainText() != committed) {
        m_committedText->setPlainText(committed);
        QTextCursor cursor = m_committedText->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_committedText->setTextCursor(cursor);
    }
    m_pendingText->setPlainText(pending);
}

void RxPanel::onPileupUpdated(const QVector<PileupTrackText> &tracks)
{
    if (selectedMode() != DecodeEngine::Mode::Pileup) {
        return;
    }
    QStringList lines;
    QVector<int> markers;
    for (const auto &t : tracks) {
        markers.push_back(t.frequencyHz);
        if (t.text.isEmpty()) {
            lines << tr("%1 Hz").arg(t.frequencyHz);
        } else {
            lines << tr("%1 Hz  %2").arg(t.frequencyHz).arg(t.text);
        }
    }
    m_pileupText->setPlainText(lines.join(QLatin1Char('\n')));
    m_waterfall->setPileupMarkers(markers);
}

void RxPanel::onRxStatus(const QString &status)
{
    if (m_pausedForTx && m_listening) {
        return;
    }
    m_statusLabel->setText(status);
}

void RxPanel::onDecodeFailed(const QString &error)
{
    m_statusLabel->setText(error);
}

void RxPanel::onCaptureError(const QString &error)
{
    m_statusLabel->setText(error);
}

void RxPanel::onMonitorError(const QString &error)
{
    m_statusLabel->setText(error);
}

} // namespace deepcw
