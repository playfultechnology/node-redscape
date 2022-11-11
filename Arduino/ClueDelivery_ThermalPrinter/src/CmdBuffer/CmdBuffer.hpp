/* Copyright 2016 Pascal Vizeli <pvizeli@syshack.ch>
 * BSD License
 *
 * https://github.com/pvizeli/CmdParser
 */

#ifndef _CMDBUFFER_H_
#define _CMDBUFFER_H_

#include <stdint.h>
#include <string.h>

#include <Arduino.h>

const uint8_t CMDBUFFER_CHAR_PRINTABLE = 0x1F;
const uint8_t CMDBUFFER_CHAR_LF        = 0x0A;
const uint8_t CMDBUFFER_CHAR_CR        = 0x0D;
const uint8_t CMDBUFFER_CHAR_BS        = 0x08;
const uint8_t CMDBUFFER_CHAR_DEL       = 0x7F;

/**
 *
 *
 */
class CmdBufferObject
{
  public:
    /**
     * Clear buffer and set defaults.
     */
    CmdBufferObject()
        : m_endChar(CMDBUFFER_CHAR_LF),
          m_bsChar(CMDBUFFER_CHAR_BS),
          m_dataOffset(0),
          m_echo(false)
    {
    }

    /**
     * Read data from serial communication to buffer. It read only printable
     * ASCII character from serial. All other will ignore for buffer.
     *
     * @param serial        Arduino Serial object from read commands
     * @param timeOut       Set a time out in millisec or 0 for none
     * @return              TRUE if data readed until end character or FALSE
     *                      is a timeout receive or buffer is full.
     */
    bool readFromSerial(Stream *serial, uint32_t timeOut = 0);

    /**
     * Read one char from serial communication to buffer if available.
     * It read only printable ASCII character from serial.
     * All other will ignore for buffer. This function only ready currently
     * available data and doesn't wait for a full command (= end character)
     *
     * @param serial        Arduino Serial object from read commands
     * @return              TRUE if data readed until end character or
     *                      FALSE if not.
     */
    bool readSerialChar(Stream *serial);

    /**
     * Set a ASCII character for serial cmd end.
     * Default value is LF.
     *
     * Macros for helping are:
     * - CMDBUFFER_CHAR_LF
     * - CMDBUFFER_CHAR_CR
     *
     * @param end       ASCII character
     */
    void setEndChar(uint8_t end) { m_endChar = end; }

    /**
     * Set a ASCII character for serial cmd backspace.
     * Default value is BS.
     *
     * Macros for helping are:
     * - CMDBUFFER_CHAR_BS
     * - CMDBUFFER_CHAR_DEL
     *
     * @param backspace       ASCII character
     */
    void setBackChar(uint8_t backspace) { m_bsChar = backspace; }

    /**
     * Set echo serial on (true) or off (false)
     *
     * @param echo      bool
     */
    void setEcho(bool echo) { m_echo = echo; }

    /**
     * Cast Buffer to c string.
     *
     * @return      Buffer as cstring
     */
    char *getStringFromBuffer()
    {
        return reinterpret_cast<char *>(this->getBuffer());
    }

    /**
     * Clear buffer with 0x00
     */
    virtual void clear() = 0;

    /**
     * Return a null-terminated ('\0') string
     *
     * @return             String from Buffer
     */
    virtual uint8_t *getBuffer() = 0;

    /**
     * Get size of buffer
     *
     * @return              Size of buffer
     */
    virtual size_t getBufferSize() = 0;

  private:
    /** Character for handling the end of serial data communication */
    uint8_t m_endChar;
    uint8_t m_bsChar;
    size_t  m_dataOffset;
    bool    m_echo;
};

/**
 *
 *
 */
template <size_t BUFFERSIZE>
class CmdBuffer : public CmdBufferObject
{
  public:
    /**
     * Cleanup Buffers
     */
    CmdBuffer() { this->clear(); }

    /**
     * @interface CmdBufferObject
     */
    virtual void clear() { memset(m_buffer, 0x00, BUFFERSIZE + 1); }

    /**
     * @interface CmdBufferObject
     */
    virtual uint8_t *getBuffer() { return m_buffer; }

    /**
     * @interface CmdBufferObject
     */
    virtual size_t getBufferSize() { return BUFFERSIZE; }

  private:
    /** Buffer for reading data from serial input */
    uint8_t m_buffer[BUFFERSIZE + 1];
};

#endif
