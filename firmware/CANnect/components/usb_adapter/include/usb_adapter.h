/**
 * @file usb_adapter.h
 * @author Gustavo Menezes da Silva
 * @brief Adapter layer to decouple USB CDC communication from the CAN logic
 * @version 0.1
 * @date 2026-06-13
 * @copyright Copyright (c) 2026 Gustavo Menezes da Silva
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

esp_err_t usb_adapter_init(uint32_t usb_buff_size, QueueHandle_t can_rx_queue, uint32_t can_bitrate);