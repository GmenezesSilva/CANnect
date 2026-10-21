#include "can_adapter.h"
#include "usb_adapter.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define QUEUE_LEN 10
#define USB_BUF_SIZE 64

static const char *TAG = "main";
static QueueHandle_t usb_can_queue;
static QueueHandle_t can_usb_queue;

void app_main(void)
{
    if (CONFIG_CAN_TX_GPIO < 0 || CONFIG_CAN_RX_GPIO < 0) {
        ESP_LOGE(TAG, "Configure the CAN TX and RX GPIOs in menuconfig");
        return;
    }

    usb_can_queue = xQueueCreate(QUEUE_LEN, sizeof(can_adapter_frame_t));
    can_usb_queue = xQueueCreate(QUEUE_LEN, sizeof(can_adapter_frame_t));
    if (usb_can_queue == NULL || can_usb_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create adapter queues");
        if (usb_can_queue != NULL) {
            vQueueDelete(usb_can_queue);
            usb_can_queue = NULL;
        }
        if (can_usb_queue != NULL) {
            vQueueDelete(can_usb_queue);
            can_usb_queue = NULL;
        }
        return;
    }

    const can_adapter_config_t can_config = {
        .can_tx_pin = CONFIG_CAN_TX_GPIO,
        .can_rx_pin = CONFIG_CAN_RX_GPIO,
        .can_bitrate = CONFIG_CAN_BITRATE,
        .can_rx_queue = can_usb_queue,
        .can_tx_queue = usb_can_queue,
        .queue_size = QUEUE_LEN,
    };

    ESP_ERROR_CHECK(can_adapter_init(&can_config));
    usb_adapter_init(USB_BUF_SIZE, usb_can_queue, can_usb_queue);
}
