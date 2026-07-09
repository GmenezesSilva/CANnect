#include "slcan_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

slcan_err_t slcan_parse_open(const char *command)
{
    if (!command)
        return SLCAN_INVALID_ARG;

    if (command[0] != 'O' || command[1] != '\r')
        return SLCAN_ERROR;

    return SLCAN_OK;
}

slcan_err_t slcan_parse_close(const char *command)
{
    if (!command)
        return SLCAN_INVALID_ARG;

    if (command[0] != 'C' || command[1] != '\r')
        return SLCAN_ERROR;

    return SLCAN_OK;
}

slcan_err_t slcan_parse_bitrate(const char *command, twai_timing_basic_config_t *timing)
{
    if (!command || !timing)
        return SLCAN_INVALID_ARG;

    if (command[0] != 'S' || command[2] != '\r')
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

    if (command[0] == 't') {
        frame->header.ide = 0;
        char id[4] = {command[1], command[2], command[3], '\0'};
        frame->header.id = strtol(id, NULL, 16);
        frame->header.dlc = command[4] - '0';

        if (frame->header.dlc > 8 || frame->header.dlc > buf_len)
            return SLCAN_ERROR;

        for (int i = 0; i < frame->header.dlc; i++) {
            char byte[3] = {command[5 + i*2], command[6 + i*2], '\0'};
            buf[i] = strtol(byte, NULL, 16);
        }

    } else if (command[0] == 'T') {
        frame->header.ide = 1;
        char id[9] = {0};
        strncpy(id, command + 1, 8);
        frame->header.id = strtol(id, NULL, 16);
        frame->header.dlc = command[9] - '0';

        if (frame->header.dlc > 8 || frame->header.dlc > buf_len)
            return SLCAN_ERROR;

        for (int i = 0; i < frame->header.dlc; i++) {
            char byte[3] = {command[10 + i*2], command[11 + i*2], '\0'};
            buf[i] = strtol(byte, NULL, 16);
        }

    } else {
        return SLCAN_NOT_SUPPORTED;
    }

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

    int pos = 0;

    if (!frame->header.ide) {
        pos += snprintf(buf, len, "t%03lX%d",
                        frame->header.id,
                        frame->header.dlc);
    } else {
        pos += snprintf(buf, len, "T%08lX%d",
                        frame->header.id,
                        frame->header.dlc);
    }

    for (int i = 0; i < frame->header.dlc; i++) {
        pos += snprintf(buf + pos, len - pos, "%02X", frame->buffer[i]);
    }

    snprintf(buf + pos, len - pos, "\r");

    return SLCAN_OK;
}