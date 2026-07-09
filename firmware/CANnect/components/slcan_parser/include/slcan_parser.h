/**
 * @file slcan_parser.h
 * @author Gustavo Menezes
 * @brief Parser for the SLCAN ASCII protocol received via USB CDC
 * @details 
 * Implements parsing of the SLCAN (Serial Line CAN) ASCII protocol
 * as used by python-can and compatible tools. Each command is
 * terminated by '\r' and dispatched to a dedicated parse function.
 *
 * Supported commands:
 *   - O  : Open channel
 *   - C  : Close channel
 *   - Sn : Set bitrate (n = 0..8)
 *   - t  : Transmit standard frame (11-bit ID)
 *   - T  : Transmit extended frame (29-bit ID)
 *
 * @version 0.1
 * @date 2026-06-24
 * @copyright Copyright (c) 2026 Gustavo Menezes da Silva
 * SPDX-License-Identifier: MIT
 */
#pragma once

#pragma once

#include "esp_twai_types.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Status codes returned by SLCAN functions
 */
typedef enum {
    SLCAN_OK,            /**< Operation completed successfully */
    SLCAN_ERROR,         /**< Generic error */
    SLCAN_INVALID_ARG,   /**< Invalid or null argument */
    SLCAN_NOT_SUPPORTED, /**< Command or bitrate not supported */
} slcan_err_t;

/**
 * @brief Validates the CAN channel open command (O\r)
 *
 * @param command SLCAN string terminated with '\r'
 * @return SLCAN_OK if valid, SLCAN_INVALID_ARG or SLCAN_ERROR otherwise
 */
slcan_err_t slcan_parse_open(const char *command);

/**
 * @brief Validates the CAN channel close command (C\r)
 *
 * @param command SLCAN string terminated with '\r'
 * @return SLCAN_OK if valid, SLCAN_INVALID_ARG or SLCAN_ERROR otherwise
 */
slcan_err_t slcan_parse_close(const char *command);

/**
 * @brief Parses the bitrate configuration command (Sn\r)
 *        and fills the TWAI basic timing struct
 *
 * @param command SLCAN string in the format "Sn\r" where n is 0-8
 * @param timing  Pointer to twai_timing_basic_config_t to be filled
 * @return SLCAN_OK if valid, SLCAN_INVALID_ARG, SLCAN_ERROR or SLCAN_NOT_SUPPORTED
 */
slcan_err_t slcan_parse_bitrate(const char *command, twai_timing_basic_config_t *timing);

/**
 * @brief Converts an SLCAN transmit string to a twai_frame_t
 *        Supports standard (t) and extended (T) frames
 *
 * @param command SLCAN string in the format "tIIILDD...\r" or "TIIIIIIIILDD...\r"
 * @param frame   Pointer to twai_frame_t to be filled
 * @param buf     External buffer to hold frame data bytes
 * @param buf_len Size of the external buffer (minimum 8 bytes)
 * @return SLCAN_OK if valid, SLCAN_INVALID_ARG, SLCAN_ERROR or SLCAN_NOT_SUPPORTED
 */
slcan_err_t slcan_deserialize(const char *command, twai_frame_t *frame, uint8_t *buf, size_t buf_len);

/**
 * @brief Converts a twai_frame_t received from the bus to an SLCAN string
 *        Supports standard (t) and extended (T) frames
 *
 * @param frame Pointer to twai_frame_t to be serialized
 * @param buf   Output buffer for the SLCAN string
 * @param len   Output buffer size
 * @return SLCAN_OK if valid, SLCAN_INVALID_ARG or SLCAN_ERROR
 */
slcan_err_t slcan_serialize(const twai_frame_t *frame, char *buf, size_t len);