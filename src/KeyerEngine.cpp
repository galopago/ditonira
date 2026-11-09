#include "KeyerEngine.h"

#include <QMutexLocker>
#include <chrono>
#include <thread>

KeyerEngine::KeyerEngine(QObject *parent)
    : QThread(parent)
{
}

KeyerEngine::~KeyerEngine()
{
    m_shuttingDown.store(true);
    requestStop();
    wait(5000);
}

void KeyerEngine::startTransmission(const Job &job)
{
    if (isRunning()) {
        return;
    }

    {
        QMutexLocker lock(&m_jobMutex);
        m_pendingJob = job;
        m_hasJob = true;
    }
    m_stopRequested.store(false);
    start();
}

void KeyerEngine::requestStop()
{
    m_stopRequested.store(true);
}

void KeyerEngine::safeKeyUp(SerialKeyLine &line)
{
    if (line.isOpen()) {
        line.keyUp();
    }
}

bool KeyerEngine::waitUnits(double unitMs, int units)
{
    using clock = std::chrono::steady_clock;
    const auto duration = std::chrono::duration<double, std::milli>(unitMs * units);
    const auto deadline = clock::now() + std::chrono::duration_cast<clock::duration>(
        std::chrono::duration<double>(duration));

    while (clock::now() < deadline) {
        if (m_stopRequested.load() || m_shuttingDown.load()) {
            return false;
        }
        const auto remaining = deadline - clock::now();
        auto sleepFor = remaining;
        const auto slice = std::chrono::milliseconds(5);
        if (sleepFor > slice) {
            sleepFor = slice;
        }
        if (sleepFor.count() > 0) {
            std::this_thread::sleep_for(sleepFor);
        }
    }
    return !m_stopRequested.load() && !m_shuttingDown.load();
}

bool KeyerEngine::playElements(SerialKeyLine &line, double unitMs,
                               const QVector<MorseEncoder::TimedElement> &elements)
{
    for (const auto &el : elements) {
        if (m_stopRequested.load() || m_shuttingDown.load()) {
            return false;
        }
        line.setKeyDown(el.keyDown);
        if (!waitUnits(unitMs, el.units)) {
            return false;
        }
    }
    return true;
}

void KeyerEngine::run()
{
    Job job;
    {
        QMutexLocker lock(&m_jobMutex);
        if (!m_hasJob) {
            return;
        }
        job = m_pendingJob;
        m_hasJob = false;
    }

    SerialKeyLine keyLine;
    QString openError;
    if (!keyLine.open(job.portName, job.line, job.invertPolarity, &openError)) {
        emit transmissionFailed(
            tr("Could not open %1: %2").arg(job.portName, openError));
        return;
    }

    const QString &text = job.text;
    emit progressChanged(0);

    // Same emptiness rule as MorseEncoder::encode (unknowns/spaces alone → nothing to key).
    if (MorseEncoder::encode(text).isEmpty()) {
        safeKeyUp(keyLine);
        keyLine.close();
        emit progressChanged(text.size());
        emit statusChanged(tr("Nothing to transmit"));
        emit transmissionFinished(false);
        return;
    }

    const double unit = MorseEncoder::unitMs(job.wpm);
    emit statusChanged(tr("Transmitting…"));

    bool needCharGap = false;
    bool pendingWordGap = false;
    bool producedAny = false;
    int pendingMappedIndex = -1; // mapped char waiting for trailing gap / EOS
    bool aborted = false;

    auto completePendingMapped = [&]() {
        if (pendingMappedIndex >= 0) {
            emit progressChanged(pendingMappedIndex + 1);
            pendingMappedIndex = -1;
        }
    };

    auto gapElements = [](MorseEncoder::Element kind, int units) {
        QVector<MorseEncoder::TimedElement> out;
        out.push_back({kind, units, false});
        return out;
    };

    for (int i = 0; i < text.size(); ++i) {
        if (m_stopRequested.load() || m_shuttingDown.load()) {
            aborted = true;
            break;
        }

        const QChar raw = text.at(i);

        if (raw.isSpace()) {
            if (needCharGap || producedAny) {
                // Word gap replaces inter-character gap; complete prior mapped char after marks.
                completePendingMapped();
                pendingWordGap = true;
                needCharGap = false;
            } else {
                // Leading whitespace produces no timing elements — advance immediately.
                emit progressChanged(i + 1);
            }
            continue;
        }

        const auto marks = MorseEncoder::characterMarks(raw);
        if (marks.isEmpty()) {
            // Unknown omitted: contiguous progress requires completing any prior mapped char first.
            completePendingMapped();
            emit progressChanged(i + 1);
            continue;
        }

        if (pendingWordGap) {
            if (!playElements(keyLine, unit, gapElements(MorseEncoder::Element::WordGap, 7))) {
                aborted = true;
                break;
            }
            // Spaces in the run before this character are now done.
            emit progressChanged(i);
            pendingWordGap = false;
            needCharGap = false;
        } else if (needCharGap) {
            if (!playElements(keyLine, unit, gapElements(MorseEncoder::Element::CharGap, 3))) {
                aborted = true;
                break;
            }
            completePendingMapped();
        }

        if (!playElements(keyLine, unit, marks)) {
            aborted = true;
            break;
        }

        producedAny = true;
        needCharGap = true;
        pendingMappedIndex = i;
    }

    if (!aborted) {
        // Trailing spaces: encode() does not emit a final WordGap — advance without keying.
        completePendingMapped();
        if (pendingWordGap || text.size() > 0) {
            emit progressChanged(text.size());
        }
    }

    safeKeyUp(keyLine);
    keyLine.close();

    if (aborted) {
        emit statusChanged(tr("Stopped"));
    } else {
        emit statusChanged(tr("Ready"));
    }
    emit transmissionFinished(aborted);
}
