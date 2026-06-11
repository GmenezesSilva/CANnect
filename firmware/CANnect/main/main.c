#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_log.h"

#define COMM_LED_GPIO   GPIO_NUM_46
#define ERROR_LED_GPIO  GPIO_NUM_9
#define CAN_TX_GPIO     GPIO_NUM_21
#define CAN_RX_GPIO     GPIO_NUM_47
#define kb              1000
#define CAN_BITRATE     500 * kb
#define CAN_QUEUE_DEPTH 5

#define CANQueueSize 32


void app_main(void)
{   
    // Configure the communication and error LEDs
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << COMM_LED_GPIO) | (1ULL << ERROR_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    twai_node_handle_t can_node = NULL;

    twai_onchip_node_config_t can_config = {
        .io_cfg.tx = CAN_TX_GPIO,
        .io_cfg.rx = CAN_RX_GPIO,
        .bit_timing.bitrate = CAN_BITRATE,
        .tx_queue_depth = CAN_QUEUE_DEPTH,
        .timestamp_resolution_hz = 1,
    };

    ESP_ERROR_CHECK(gpio_config(&led_config));
    ESP_ERROR_CHECK(twai_new_node_onchip(&can_config, &can_node));
    ESP_ERROR_CHECK(twai_node_enable(can_node));

    // TinyUSB from USB controller used trough  


    // Create a queue to hold received CAN messages
    QueueHandle_t usb_to_can;
    QueueHandle_t can_to_usb;

    usb_to_can = xQueueCreate(CANQueueSize, sizeof(twai_message_t));
    can_to_usb = xQueueCreate(CANQueueSize, sizeof(twai_message_t));
    
    // Create CAN tasks to handle the communication
}
