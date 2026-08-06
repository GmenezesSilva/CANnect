/**
 * @file can_adapter.h
 * @author Gustavo Menezes da Silva
 * @brief Adapter layer to manage CAN hardware and route frames to queues
 * @version 0.1
 * @date 2026-06-13
 * @copyright Copyright (c) 2026 Gustavo Menezes da Silva
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "hal/gpio_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#define CAN_TASK_SIZE 4096
#define CAN_CORE 0
#define CAN_TASK_PRIORITY 5
#define CAN_ADAPTER_MAX_DATA_LEN 8  // 8 is from std CAN and 2.0, need to change to 64 for CANFD

typedef struct
{
    gpio_num_t can_tx_pin; 
    gpio_num_t can_rx_pin;
    uint32_t can_bitrate;
    QueueHandle_t can_rx_queue;
    QueueHandle_t can_tx_queue;
    size_t queue_size;
}can_adapter_config_t;


typedef struct {
    uint32_t id;
    uint8_t dlc;
    bool extd;
    uint8_t data[CAN_ADAPTER_MAX_DATA_LEN];
} can_adapter_frame_t;


esp_err_t can_adapter_init(const can_adapter_config_t *config);