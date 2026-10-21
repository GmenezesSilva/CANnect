#include "slcan_parser.h"
#include <string.h>
#include <stdio.h>

static int hex_value(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static slcan_err_t parse_hex(const char *text, size_t digits, uint32_t *value)
{
    uint32_t result = 0;
    for (size_t i = 0; i < digits; i++) {
        int digit = hex_value(text[i]);
        if (digit < 0)
            return SLCAN_ERROR;
        result = (result << 4) | (uint32_t)digit;
    }
    *value = result;
    return SLCAN_OK;
}

slcan_err_t slcan_parse_open(const char *command)
{
    if (!command)
        return SLCAN_INVALID_ARG;
    return strcmp(command, "O\r") == 0 ? SLCAN_OK : SLCAN_ERROR;
}

slcan_err_t slcan_parse_close(const char *command)
{
    if (!command)
        return SLCAN_INVALID_ARG;
    return strcmp(command, "C\r") == 0 ? SLCAN_OK : SLCAN_ERROR;
}

slcan_err_t slcan_parse_bitrate(const char *command, twai_timing_basic_config_t *timing)
{
    if (!command || !timing)
        return SLCAN_INVALID_ARG;

    if (strlen(command) != 3 || command[0] != 'S' || command[2] != '\r')
        return SLCAN_ERROR;

    switch (command[1]) {
        case '0': timing->bitrate = 10000;   break;
        case '1': timing->bitrate = 20000;   break;
        case '2': timing->bitrate = 50000;   break;
        case '3': timing->bitrate = 100000;  break;
        case '4': timing->bitrate = 125000;  break;
        case '5': timing->bitrate = 250000;  break;
        case '6': timing->bitrate = 500000;  break;
        case '7': timing->bitrate = 800000;  break;
        case '8': timing->bitrate = 1000000; break;
        default:  return SLCAN_NOT_SUPPORTED;
    }

    return SLCAN_OK;
}

slcan_err_t slcan_deserialize(const char *command, twai_frame_t *frame, uint8_t *buf, size_t buf_len)
{
    if (!command || !frame || !buf)
        return SLCAN_INVALID_ARG;

    memset(frame, 0, sizeof(*frame));
    bool extended;
    size_t id_digits;
    size_t dlc_offset;
    if (command[0] == 't') {
        extended = false;
        id_digits = 3;
        dlc_offset = 4;
    } else if (command[0] == 'T') {
        extended = true;
        id_digits = 8;
        dlc_offset = 9;
    } else {
        return SLCAN_NOT_SUPPORTED;
    }

    size_t command_len = strlen(command);
    if (command_len <= dlc_offset + 1 || command[command_len - 1] != '\r')
        return SLCAN_ERROR;

    if (command[dlc_offset] < '0' || command[dlc_offset] > '8')
        return SLCAN_ERROR;

    uint8_t dlc = (uint8_t)(command[dlc_offset] - '0');
    if (command_len != dlc_offset + 2 + (size_t)dlc * 2 || dlc > buf_len)
        return SLCAN_ERROR;

    uint32_t id;
    if (parse_hex(command + 1, id_digits, &id) != SLCAN_OK)
        return SLCAN_ERROR;
    if ((!extended && id > 0x7FF) || (extended && id > 0x1FFFFFFF))
        return SLCAN_ERROR;

    for (uint8_t i = 0; i < dlc; i++) {
        uint32_t byte;
        if (parse_hex(command + dlc_offset + 1 + (size_t)i * 2, 2, &byte) != SLCAN_OK)
            return SLCAN_ERROR;
        buf[i] = (uint8_t)byte;
    }

    frame->header.ide = extended;
    frame->header.id = id;
    frame->header.dlc = dlc;
    frame->buffer = buf;
    frame->buffer_len = frame->header.dlc;

    return SLCAN_OK;
}

slcan_err_t slcan_serialize(const twai_frame_t *frame, char *buf, size_t len)
{
    if (!frame || !buf || len == 0)
        return SLCAN_INVALID_ARG;

    if (frame->header.dlc > 8)
        return SLCAN_ERROR;
    if (frame->header.dlc > frame->buffer_len ||
        (frame->header.dlc > 0 && frame->buffer == NULL))
        return SLCAN_ERROR;
    if ((!frame->header.ide && frame->header.id > 0x7FF) ||
        (frame->header.ide && frame->header.id > 0x1FFFFFFF))
        return SLCAN_ERROR;

    int pos = snprintf(buf, len, frame->header.ide ? "T%08lX%d" : "t%03lX%d",
                       (unsigned long)frame->header.id, frame->header.dlc);
    if (pos < 0 || (size_t)pos >= len)
        return SLCAN_ERROR;

    for (uint8_t i = 0; i < frame->header.dlc; i++) {
        int written = snprintf(buf + pos, len - (size_t)pos, "%02X", frame->buffer[i]);
        if (written < 0 || (size_t)written >= len - (size_t)pos)
            return SLCAN_ERROR;
        pos += written;
    }

    if ((size_t)pos + 2 > len)
        return SLCAN_ERROR;
    buf[pos++] = '\r';
    buf[pos] = '\0';

    return SLCAN_OK;
}