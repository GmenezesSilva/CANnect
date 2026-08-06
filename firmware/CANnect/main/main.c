#include <stdio.h>
#include "usb_adapter.h"
#include "can_adapter.h"
#include "esp_twai_types.h"

#define QUEUE_LEN 10
#define USB_BUF_SIZE 64

char usb_frame[32];

static QueueHandle_t usb_can_queue = NULL;
static QueueHandle_t can_usb_queue = NULL;


void app_main(void)
{   
    
    usb_can_queue = xQueueCreate(QUEUE_LEN, sizeof(usb_frame));
    can_usb_queue = xQueueCreate(QUEUE_LEN, sizeof(twai_frame_t));
    
    usb_adapter_init(USB_BUF_SIZE, usb_can_queue, can_usb_queue);
}
