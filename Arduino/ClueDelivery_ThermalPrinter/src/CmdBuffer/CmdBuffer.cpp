/* Copyright 2016 Pascal Vizeli <pvizeli@syshack.ch>
 * BSD License
 *
 * https://github.com/pvizeli/CmdParser
 */

#include "CmdBuffer.hpp"

bool CmdBufferObject::readFromSerial(Stream *serial, uint32_t timeOut)
{
    uint32_t isTimeOut;
    uint32_t startTime;
    bool     over = false;

    // UART initialize?
    if (serial == NULL) {
        return false;
    }

    ////
    // Calc Timeout
    if (timeOut != 0) {
        startTime = millis();
        isTimeOut = startTime + timeOut;

        // overloaded
        if (isTimeOut < startTime) {
            over = true;
        } else {
            over = false;
        }
    }

    ////
    // process serial reading
    do {

        // if data in serial input buffer
        while (serial->available()) {

            if (this->readSerialChar(serial)) {
                return true;
            }
        }

        // Timeout is active?
        if (timeOut != 0) {
            // calc diff timeout
            if (over) {
                if (startTime > millis()) {
                    over = false;
                }
            }

            // timeout is receive
            if (isTimeOut <= millis() && !over) {
                return false;
            }
        }

    } while (true); // timeout

    return false;
}

bool CmdBufferObject::readSerialChar(Stream *serial)
{
    uint8_t  readChar;
    uint8_t *buffer = this->getBuffer();

    // UART initialize?
    if (serial == NULL) {
        return false;
    }

    if (serial->available()) {
        // is buffer full?
        if (m_dataOffset >= this->getBufferSize()) {
            m_dataOffset = 0;
            return false;
        }

        // read into buffer
        readChar = serial->read();

        if (m_echo) {
            serial->write(readChar);
        }

        // is that the end of command
        if (readChar == m_endChar) {
            buffer[m_dataOffset] = '\0';
            m_dataOffset         = 0;
            return true;
        }

        // is that a backspace char?
        if ((readChar == m_bsChar) && (m_dataOffset > 0)) {
            // buffer[--m_dataOffset] = 0;
            --m_dataOffset;
            if (m_echo) {
                serial->write(' ');
                serial->write(readChar);
            }
            return false;
        }

        // is a printable character
        if (readChar > CMDBUFFER_CHAR_PRINTABLE) {
            buffer[m_dataOffset++] = readChar;
        }
    }
    return false;
}
