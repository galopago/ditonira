#pragma once

#include "MorseEncoder.h"
#include "SerialKeyLine.h"

#include <QMutex>
#include <QString>
#include <QThread>
#include <atomic>

class KeyerEngine : public QThread
{
    Q_OBJECT

public:
    explicit KeyerEngine(QObject *parent = nullptr);
    ~KeyerEngine() override;

    struct Job {
        QString portName;
        SerialKeyLine::Line line = SerialKeyLine::Line::Dtr;
        bool invertPolarity = false;
        int wpm = 20;
        QString text;
    };

public slots:
    void startTransmission(const Job &job);
    void requestStop();

signals:
    void statusChanged(const QString &status);
    /// Source indices [0, charsCompleted) of Job::text are fully handled.
    void progressChanged(int charsCompleted);
    void transmissionFinished(bool aborted);
    void transmissionFailed(const QString &error);

protected:
    void run() override;

private:
    bool waitUnits(double unitMs, int units);
    bool playElements(SerialKeyLine &line, double unitMs,
                      const QVector<MorseEncoder::TimedElement> &elements);
    void safeKeyUp(SerialKeyLine &line);

    QMutex m_jobMutex;
    Job m_pendingJob;
    bool m_hasJob = false;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_shuttingDown{false};
};
