#include "SerialKeyLine.h"

#include <QIODevice>
#include <QSerialPort>

SerialKeyLine::SerialKeyLine() = default;

SerialKeyLine::~SerialKeyLine()
{
    close();
}

bool SerialKeyLine::open(const QString &portName, Line line, bool invertPolarity, QString *errorOut)
{
    close();

    m_line = line;
    m_invert = invertPolarity;
    m_port = new QSerialPort();
    m_port->setPortName(portName);
    m_port->setBaudRate(QSerialPort::Baud9600);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        const QString err = m_port->errorString();
        if (errorOut) {
            *errorOut = err;
        }
        delete m_port;
        m_port = nullptr;
        return false;
    }

    idleOtherLine();
    keyUp();
    return true;
}

void SerialKeyLine::close()
{
    if (!m_port) {
        return;
    }
    keyUp();
    m_port->close();
    delete m_port;
    m_port = nullptr;
}

bool SerialKeyLine::isOpen() const
{
    return m_port && m_port->isOpen();
}

void SerialKeyLine::setKeyDown(bool down)
{
    if (!m_port) {
        return;
    }
    const bool asserted = m_invert ? !down : down;
    applyLine(asserted);
}

void SerialKeyLine::keyUp()
{
    setKeyDown(false);
}

void SerialKeyLine::applyLine(bool asserted)
{
    if (!m_port) {
        return;
    }
    if (m_line == Line::Rts) {
        m_port->setRequestToSend(asserted);
    } else {
        m_port->setDataTerminalReady(asserted);
    }
}

void SerialKeyLine::idleOtherLine()
{
    if (!m_port) {
        return;
    }
    // Leave the unused control line deasserted (idle / key-up polarity for "normal")
    if (m_line == Line::Rts) {
        m_port->setDataTerminalReady(false);
    } else {
        m_port->setRequestToSend(false);
    }
}
