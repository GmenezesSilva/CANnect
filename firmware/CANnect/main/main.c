#include <stdio.h>
#include "usb_adapter.h"
#include "can_adapter.h"
#include "esp_twai_types.h"

#define QUEUE_LEN 10
#define USB_BUF_SIZE 64

char usb_frame[32];

static QueueHandle_t usb_tx_queue = NULL;
static QueueHandle_t usb_rx_queue = NULL;
static QueueHandle_t can_tx_queue = NULL;
static QueueHandle_t can_rx_queue = NULL;


void app_main(void)
{   
    
    QueueHandle_t usb_tx_queue = xQueueCreate(QUEUE_LEN, sizeof(usb_frame));
    QueueHandle_t usb_rx_queue = xQueueCreate(QUEUE_LEN, sizeof(usb_frame));
    QueueHandle_t can_tx_queue = xQueueCreate(QUEUE_LEN, sizeof(twai_message_t));
    QueueHandle_t can_rx_queue = xQueueCreate(QUEUE_LEN, sizeof(twai_message_t));
    
    usb_adapter_init(USB_BUF_SIZE, usb_tx_queue, usb_tx_queue);
}
