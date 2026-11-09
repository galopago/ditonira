#include "MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Ditonira CW Keyer"));
    resize(560, 420);

    m_sentFormat.setForeground(palette().color(QPalette::Link));

    m_engine = new KeyerEngine(this);
    connect(m_engine, &KeyerEngine::statusChanged, this, &MainWindow::onStatusChanged);
    connect(m_engine, &KeyerEngine::progressChanged, this, &MainWindow::onProgressChanged);
    connect(m_engine, &KeyerEngine::transmissionFinished, this, &MainWindow::onTransmissionFinished);
    connect(m_engine, &KeyerEngine::transmissionFailed, this, &MainWindow::onTransmissionFailed);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);

    // Port row
    auto *portRow = new QHBoxLayout();
    portRow->addWidget(new QLabel(tr("Port:"), central));
    m_portCombo = new QComboBox(central);
    m_portCombo->setMinimumWidth(280);
    portRow->addWidget(m_portCombo, 1);
    m_refreshButton = new QPushButton(tr("Refresh"), central);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    portRow->addWidget(m_refreshButton);
    root->addLayout(portRow);

    // Line + WPM + polarity
    auto *optsRow = new QHBoxLayout();
    optsRow->addWidget(new QLabel(tr("Line:"), central));
    m_lineCombo = new QComboBox(central);
    m_lineCombo->addItem(tr("DTR"), static_cast<int>(SerialKeyLine::Line::Dtr));
    m_lineCombo->addItem(tr("RTS"), static_cast<int>(SerialKeyLine::Line::Rts));
    optsRow->addWidget(m_lineCombo);

    optsRow->addWidget(new QLabel(tr("WPM:"), central));
    m_wpmSpin = new QSpinBox(central);
    m_wpmSpin->setRange(5, 50);
    m_wpmSpin->setValue(20);
    optsRow->addWidget(m_wpmSpin);

    m_invertCheck = new QCheckBox(tr("Invert polarity"), central);
    optsRow->addWidget(m_invertCheck);
    optsRow->addStretch(1);
    root->addLayout(optsRow);

    root->addWidget(new QLabel(tr("Text to transmit:"), central));
    m_textEdit = new QPlainTextEdit(central);
    m_textEdit->setPlaceholderText(tr("CQ CQ DE CALL K"));
    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &MainWindow::onTextEdited);
    root->addWidget(m_textEdit, 1);

    auto *btnRow = new QHBoxLayout();
    m_transmitButton = new QPushButton(tr("Transmit"), central);
    m_stopButton = new QPushButton(tr("Stop"), central);
    m_stopButton->setEnabled(false);
    connect(m_transmitButton, &QPushButton::clicked, this, &MainWindow::onTransmit);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStop);
    btnRow->addWidget(m_transmitButton);
    btnRow->addWidget(m_stopButton);
    btnRow->addStretch(1);
    root->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Ready"), central);
    root->addWidget(m_statusLabel);

    refreshPorts();
}

void MainWindow::refreshPorts()
{
    const QString current = m_portCombo->currentData().toString();
    m_portCombo->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        QString label = info.portName();
        if (!info.description().isEmpty()) {
            label += QStringLiteral(" — %1").arg(info.description());
        }
        m_portCombo->addItem(label, info.portName());
    }

    if (m_portCombo->count() == 0) {
        m_portCombo->addItem(tr("(no ports)"), QString());
        m_statusLabel->setText(tr("No serial ports"));
    } else {
        const int idx = m_portCombo->findData(current);
        if (idx >= 0) {
            m_portCombo->setCurrentIndex(idx);
        }
        if (m_statusLabel->text() == tr("No serial ports")) {
            m_statusLabel->setText(tr("Ready"));
        }
    }
}

SerialKeyLine::Line MainWindow::selectedLine() const
{
    return static_cast<SerialKeyLine::Line>(m_lineCombo->currentData().toInt());
}

void MainWindow::setUiTransmitting(bool transmitting)
{
    m_transmitting = transmitting;
    m_transmitButton->setEnabled(!transmitting);
    m_stopButton->setEnabled(transmitting);
    m_portCombo->setEnabled(!transmitting);
    m_refreshButton->setEnabled(!transmitting);
    m_lineCombo->setEnabled(!transmitting);
    m_wpmSpin->setEnabled(!transmitting);
    m_invertCheck->setEnabled(!transmitting);
    m_textEdit->setReadOnly(transmitting);
}

void MainWindow::applyProgressHighlight(int charsCompleted)
{
    m_charsCompleted = charsCompleted;
    m_sentFormat.setForeground(palette().color(QPalette::Link));

    QList<QTextEdit::ExtraSelection> selections;
    if (charsCompleted > 0) {
        QTextEdit::ExtraSelection sent;
        sent.format = m_sentFormat;
        sent.cursor = QTextCursor(m_textEdit->document());
        sent.cursor.setPosition(0);
        const int end = qMin(charsCompleted, m_textEdit->toPlainText().size());
        sent.cursor.setPosition(end, QTextCursor::KeepAnchor);
        selections.append(sent);
    }
    m_textEdit->setExtraSelections(selections);
}

void MainWindow::clearProgressHighlight()
{
    m_charsCompleted = 0;
    m_textEdit->setExtraSelections({});
}

void MainWindow::onTransmit()
{
    const QString port = m_portCombo->currentData().toString();
    if (port.isEmpty()) {
        m_statusLabel->setText(tr("Select a port"));
        return;
    }

    // Exact text-box snapshot for progress indices (ARCHITECTURE §7.3).
    const QString text = m_textEdit->toPlainText();
    if (text.trimmed().isEmpty()) {
        m_statusLabel->setText(tr("Enter text to transmit"));
        return;
    }

    KeyerEngine::Job job;
    job.portName = port;
    job.line = selectedLine();
    job.invertPolarity = m_invertCheck->isChecked();
    job.wpm = m_wpmSpin->value();
    job.text = text;

    applyProgressHighlight(0);
    setUiTransmitting(true);
    m_engine->startTransmission(job);
}

void MainWindow::onStop()
{
    m_engine->requestStop();
    m_statusLabel->setText(tr("Stopping…"));
}

void MainWindow::onStatusChanged(const QString &status)
{
    m_statusLabel->setText(status);
}

void MainWindow::onProgressChanged(int charsCompleted)
{
    applyProgressHighlight(charsCompleted);
}

void MainWindow::onTransmissionFinished(bool aborted)
{
    Q_UNUSED(aborted);
    // Partial highlight kept on Stop; full highlight kept on natural finish.
    setUiTransmitting(false);
}

void MainWindow::onTransmissionFailed(const QString &error)
{
    m_statusLabel->setText(error);
    clearProgressHighlight();
    setUiTransmitting(false);
}

void MainWindow::onTextEdited()
{
    if (m_transmitting) {
        return;
    }
    // User edited after TX — clear sent styling until the next Transmit.
    if (m_charsCompleted > 0 || !m_textEdit->extraSelections().isEmpty()) {
        clearProgressHighlight();
    }
}
