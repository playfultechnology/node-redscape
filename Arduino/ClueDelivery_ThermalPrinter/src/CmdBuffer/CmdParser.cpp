/* Copyright 2016 Pascal Vizeli <pvizeli@syshack.ch>
 * BSD License
 *
 * https://github.com/pvizeli/CmdParser
 */

#include "CmdParser.hpp"

CmdParser::CmdParser()
    : m_ignoreQuote(false),
      m_useKeyValue(false),
      m_seperator(CMDPARSER_CHAR_SP),
      m_buffer(NULL),
      m_bufferSize(0),
      m_paramCount(0)
{
}

uint16_t CmdParser::parseCmd(uint8_t *buffer, size_t bufferSize)
{
    bool isString = false;

    // init param count
    m_paramCount = 0;

    // buffer is not okay
    if (buffer == NULL || bufferSize == 0 || buffer[0] == 0x00) {
        return CMDPARSER_ERROR;
    }

    // init buffer
    m_buffer     = buffer;
    m_bufferSize = bufferSize;

    ////
    // Run Parser
    for (size_t i = 0; i < bufferSize; i++) {

        // end
        if (buffer[i] == 0x00 || m_paramCount == 0xFFFE) {
            if (i > 0 && buffer[i - 1] != 0x00) {
                m_paramCount++;
            }
            return m_paramCount;
        }
        // is string "xy zyx" / only the quote option is disabled
        else if (!m_ignoreQuote && buffer[i] == CMDPARSER_CHAR_DQ) {
            buffer[i] = 0x00;
            isString  = !isString;
        }
        // replace seperator with '\0'
        else if (!isString && buffer[i] == m_seperator) {
            buffer[i] = 0x00;
        }
        // replace = with '\0' if KEY=Value is set
        else if (!isString && m_useKeyValue && buffer[i] == CMDPARSER_CHAR_EQ) {
            buffer[i] = 0x00;
        }

        // count
        if (i > 0 && buffer[i] == 0x00 && buffer[i - 1] != 0x00) {
            m_paramCount++;
        }
    }

    return m_paramCount;
}

char *CmdParser::getCmdParam(uint16_t idx)
{
    uint16_t count = 0;

    // idx bigger than exists param
    if (idx > m_paramCount) {
        return NULL;
    }

    // search hole cmd buffer
    for (size_t i = 0; i < m_bufferSize; i++) {

        // find next position
        if (i > 0 && m_buffer[i] == 0x00 && m_buffer[i - 1] != 0x00) {
            count++;
        }

        // found indx with next character
        if (count == idx && m_buffer[i] != 0x00) {
            return reinterpret_cast<char *>(&m_buffer[i]);
        }
    }

    return NULL;
}

char *CmdParser::getValueFromKey(const char *key, bool progmem)
{
    bool foundKey = false;

    for (size_t i = 0; i < m_bufferSize; i++) {

        // find data
        if (m_buffer[i] == 0x00) {
            continue;
        }

        // find first element
        if (i == 0 || (m_buffer[i - 1] == 0x00 && !foundKey)) {
            // if key in SRAM
            if (!progmem &&
                strcasecmp(reinterpret_cast<char *>(&m_buffer[i]), key) == 0) {
                foundKey = true;
            }
            // if key in PROGMEM
            else if (progmem &&
                     strcasecmp_P(reinterpret_cast<char *>(&m_buffer[i]),
                                  key) == 0) {
                foundKey = true;
            }
        }
        // Next element after found key is the value...
        else if (m_buffer[i - 1] == 0x00) {
            return reinterpret_cast<char *>(&m_buffer[i]);
        }
    }

    return NULL;
}
