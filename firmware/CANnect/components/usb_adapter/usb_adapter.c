#include "usb_adapter.h"
#include "esp_log.h"
#include "esp_err.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"

#define USB_CORE 0
#define TX_BUFFER_SIZE 64
#define RX_BUFFER_SIZE 64
#define QUEUE_LEN 10
#define CDC_PORT TINYUSB_CDC_ACM_0
#define USB_PRIORITY 5
#define TASK_DEPTH 4096

// Task Handlers
static TaskHandle_t usb_rx_task_handle = NULL;

// USB tasks
static void usb_rx_task(void *pvParameters)
{
    QueueHandle_t to_can = (QueueHandle_t) pvParameters;
    uint8_t rx_buffer[64] = {0};


    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (tud_cdc_available())
        {
            uint32_t count = tud_cdc_read(rx_buffer, sizeof(rx_buffer));
            if(count > 0)
            {
                xQueueSend(to_can, rx_buffer, portMAX_DELAY);
        
            }
        }
    }
    
}

static void usb_tx_task(void *pvParameters)
{
    QueueHandle_t from_can = (QueueHandle_t) pvParameters;
    uint8_t buffer[TX_BUFFER_SIZE] = {0};
    size_t len = 0;

    while(1)
    {
        if(xQueueReceive(from_can, buffer, portMAX_DELAY))
        {
            len = strlen((char *) buffer);
            tinyusb_cdcacm_write_queue(CDC_PORT, buffer, len);
            tinyusb_cdcacm_write_flush(CDC_PORT, pdMS_TO_TICKS(20));
        }
    }
}

static void tusb_cdc_rx_cb(BaseType_t intflag, cdcacm_event_t *event)
{
    BaseType_t high_priority = pdFALSE;
    vTaskNotifyGiveFromISR(usb_rx_task_handle, &high_priority);
    portYIELD_FROM_ISR(high_priority);
}

void usb_adapter_init(uint32_t usb_buff_size, QueueHandle_t to_can_queue, QueueHandle_t from_can_queue)
{
    tinyusb_config_t tusb_config = {0};
    const tinyusb_config_cdcacm_t acm_cfg = {
    .cdc_port = CDC_PORT,
    .callback_rx = tusb_cdc_rx_cb,
    };
    
    ESP_LOGI(__FILE__, "TinyUSB CDC initializing...");
    
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_config));
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&acm_cfg));

    xTaskCreatePinnedToCore(
        usb_rx_task,
        "USB RX task",
        TASK_DEPTH,           
        to_can_queue,
        USB_PRIORITY,               
        &usb_rx_task_handle,
        USB_CORE
    );

    xTaskCreatePinnedToCore(
        usb_tx_task,
        "USB TX task",
        TASK_DEPTH,          
        from_can_queue,
        USB_PRIORITY,               
        NULL,
        USB_CORE
    );
}