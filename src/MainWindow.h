#pragma once

#include "KeyerEngine.h"

#include <QMainWindow>
#include <QTextCharFormat>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QPlainTextEdit;
class QPushButton;
class QLabel;

namespace deepcw {
class RxPanel;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshPorts();
    void onTransmit();
    void onStop();
    void onStatusChanged(const QString &status);
    void onProgressChanged(int charsCompleted);
    void onTransmissionFinished(bool aborted);
    void onTransmissionFailed(const QString &error);
    void onTextEdited();

private:
    void setUiTransmitting(bool transmitting);
    void applyProgressHighlight(int charsCompleted);
    void clearProgressHighlight();
    SerialKeyLine::Line selectedLine() const;

    KeyerEngine *m_engine = nullptr;
    deepcw::RxPanel *m_rxPanel = nullptr;

    QComboBox *m_portCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QComboBox *m_lineCombo = nullptr;
    QSpinBox *m_wpmSpin = nullptr;
    QCheckBox *m_invertCheck = nullptr;
    QPlainTextEdit *m_textEdit = nullptr;
    QPushButton *m_transmitButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QTextCharFormat m_sentFormat;
    int m_charsCompleted = 0;
    bool m_transmitting = false;
};
