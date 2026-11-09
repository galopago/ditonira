#pragma once

#include <QString>

class QSerialPort;

/**
 * Opens a serial port and keys CW via RTS or DTR with configurable polarity.
 */
class SerialKeyLine
{
public:
    enum class Line {
        Rts,
        Dtr
    };

    SerialKeyLine();
    ~SerialKeyLine();

    SerialKeyLine(const SerialKeyLine &) = delete;
    SerialKeyLine &operator=(const SerialKeyLine &) = delete;

    bool open(const QString &portName, Line line, bool invertPolarity, QString *errorOut = nullptr);
    void close();
    bool isOpen() const;

    void setKeyDown(bool down);
    void keyUp();

    Line line() const { return m_line; }
    bool invertPolarity() const { return m_invert; }

private:
    void applyLine(bool asserted);
    void idleOtherLine();

    QSerialPort *m_port = nullptr;
    Line m_line = Line::Dtr;
    bool m_invert = false;
};
