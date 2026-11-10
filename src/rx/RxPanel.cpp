#include "rx/RxPanel.h"

#include "rx/DeepCwConstants.h"
#include "rx/ModelPath.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextCursor>
#include <QVBoxLayout>

namespace deepcw {
namespace {

void streamingAudioTap(const float *samples, int count, void *user)
{
    if (!user) {
        return;
    }
    static_cast<DecodeEngine *>(user)->appendStreamingAudio(samples, count);
}

} // namespace

RxPanel::RxPanel(QWidget *parent)
    : QWidget(parent)
    , m_ring(std::make_unique<AudioRingBuffer>(kDefaultDecodeWindowS * kSampleRate))
{
    m_capture = new AudioCapture(m_ring.get(), this);
    m_decode = new DecodeEngine(m_ring.get(), this);
    m_capture->setSampleTap(&streamingAudioTap, m_decode);

    connect(m_capture, &AudioCapture::captureError, this, &RxPanel::onCaptureError);
    connect(m_decode, &DecodeEngine::transcriptUpdated, this, &RxPanel::onTranscriptUpdated);
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

    auto *optsRow = new QHBoxLayout();
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

    root->addWidget(new QLabel(tr("Transcript"), this));
    m_committedText = new QPlainTextEdit(this);
    m_committedText->setReadOnly(true);
    m_committedText->setPlaceholderText(tr("Committed transcript"));
    root->addWidget(m_committedText, 1);

    root->addWidget(new QLabel(tr("Live tail"), this));
    m_pendingText = new QPlainTextEdit(this);
    m_pendingText->setReadOnly(true);
    m_pendingText->setMaximumHeight(80);
    m_pendingText->setPlaceholderText(tr("Live decode (pending)"));
    root->addWidget(m_pendingText);

    m_statusLabel = new QLabel(tr("Ready"), this);
    root->addWidget(m_statusLabel);

    refreshDevices();
    applyRingCapacity();
    m_decode->setCenterHz(m_centerSpin->value());
    m_decode->setBandwidthHz(selectedBandwidthHz());
    m_decode->setWindowSeconds(selectedWindowSeconds());
}

RxPanel::~RxPanel()
{
    onStopRx();
}

void RxPanel::setPausedForTx(bool paused)
{
    m_pausedForTx = paused;
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

int RxPanel::selectedDeviceIndex() const
{
    return m_deviceCombo->currentData().toInt();
}

int RxPanel::selectedWindowSeconds() const
{
    return m_windowCombo->currentData().toInt();
}

int RxPanel::selectedBandwidthHz() const
{
    return m_bandwidthCombo->currentData().toInt();
}

void RxPanel::applyRingCapacity()
{
    const int seconds = selectedWindowSeconds();
    m_ring->resize(seconds * kSampleRate);
    m_decode->setWindowSeconds(seconds);
}

void RxPanel::clearTranscriptUi()
{
    m_committedText->clear();
    m_pendingText->clear();
}

void RxPanel::setListeningUi(bool listening)
{
    m_listening = listening;
    m_startButton->setEnabled(!listening);
    m_stopButton->setEnabled(listening);
    m_deviceCombo->setEnabled(!listening);
    m_refreshButton->setEnabled(!listening);
    m_windowCombo->setEnabled(!listening);
}

void RxPanel::onStartRx()
{
    const int device = selectedDeviceIndex();
    if (device < 0) {
        m_statusLabel->setText(tr("Could not open audio device"));
        return;
    }

    QString err;
    const QString modelPath = findDeepCwModelPath();
    if (!m_decode->loadModel(modelPath, &err)) {
        m_statusLabel->setText(tr("Could not load CW model"));
        return;
    }

    applyRingCapacity();
    m_ring->clear();
    m_waterfall->clearWaterfall();
    m_decode->resetStreaming();
    clearTranscriptUi();
    m_decode->setCenterHz(m_centerSpin->value());
    m_decode->setBandwidthHz(selectedBandwidthHz());
    m_waterfall->setCenterHz(m_centerSpin->value());
    m_waterfall->setBandwidthHz(selectedBandwidthHz());

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
    if (!m_pausedForTx) {
        m_statusLabel->setText(tr("Listening…"));
    }
}

void RxPanel::onStopRx()
{
    m_capture->stop();
    m_decode->setSpectrumPaused(true);
    if (m_decode->isRunning()) {
        m_decode->requestStop();
        m_decode->wait(5000);
    }
    m_decode->resetStreaming();
    m_waterfall->clearWaterfall();
    setListeningUi(false);
    m_statusLabel->setText(tr("Ready"));
}

void RxPanel::onWindowChanged(int)
{
    if (!m_listening) {
        applyRingCapacity();
    }
}

void RxPanel::onBandwidthChanged(int)
{
    const int bw = selectedBandwidthHz();
    m_decode->setBandwidthHz(bw);
    m_waterfall->setBandwidthHz(bw);
    m_decode->clearStreamingPending();
    m_pendingText->clear();
    m_decode->requestDecodeWake();
}

void RxPanel::onCenterChanged(int value)
{
    if (m_ignoreCenterSpin) {
        return;
    }
    m_decode->setCenterHz(value);
    m_waterfall->setCenterHz(value);
    m_decode->clearStreamingPending();
    m_pendingText->clear();
    m_decode->requestDecodeWake();
}

void RxPanel::onWaterfallCenter(int hz)
{
    m_ignoreCenterSpin = true;
    m_centerSpin->setValue(hz);
    m_ignoreCenterSpin = false;
    m_decode->setCenterHz(hz);
    m_decode->clearStreamingPending();
    m_pendingText->clear();
    m_decode->requestDecodeWake();
}

void RxPanel::onTranscriptUpdated(const QString &committed, const QString &pending)
{
    if (m_committedText->toPlainText() != committed) {
        m_committedText->setPlainText(committed);
        QTextCursor cursor = m_committedText->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_committedText->setTextCursor(cursor);
    }
    m_pendingText->setPlainText(pending);
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

} // namespace deepcw
