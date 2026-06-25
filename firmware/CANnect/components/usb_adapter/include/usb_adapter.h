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
#include "freertos/task.h"
#include "freertos/queue.h"

static void usb_rx_task(void *pvParameters);
static void usb_tx_task(void *pvParameters);

/**
 * @brief Initialize all USB dependencies to the project
 * 
 */
void usb_adapter_init(uint32_t usb_buff_size, QueueHandle_t rx_queue, QueueHandle_t tx_queue);