#include "usb_adapter.h"
#include "can_adapter.h"
#include "slcan_parser.h"
#include "esp_log.h"
#include "esp_check.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include <string.h>

#define USB_CORE 0
#define USB_PRIORITY 5
#define TASK_DEPTH 4096
#define RX_BUFFER_SIZE 64
#define SLCAN_COMMAND_SIZE 32
#define CDC_PORT TINYUSB_CDC_ACM_0

static const char *TAG = "usb_adapter";
static TaskHandle_t usb_rx_task_handle;
static SemaphoreHandle_t usb_tx_mutex;
static uint32_t configured_bitrate;
static uint32_t usb_buffer_size;
static bool channel_open;
static portMUX_TYPE channel_mux = portMUX_INITIALIZER_UNLOCKED;

static bool is_channel_open(void)
{
    bool open;
    portENTER_CRITICAL(&channel_mux);
    open = channel_open;
    portEXIT_CRITICAL(&channel_mux);
    return open;
}

static void set_channel_open(bool open)
{
    portENTER_CRITICAL(&channel_mux);
    channel_open = open;
    portEXIT_CRITICAL(&channel_mux);
}

static void usb_send(const char *data, size_t len)
{
    if (xSemaphoreTake(usb_tx_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to lock USB CDC TX");
        return;
    }

    size_t queued = tinyusb_cdcacm_write_queue(CDC_PORT, (const uint8_t *)data, len);
    if (queued != len) {
        ESP_LOGE(TAG, "USB CDC TX buffer accepted %u of %u bytes",
                 (unsigned)queued, (unsigned)len);
        xSemaphoreGive(usb_tx_mutex);
        return;
    }

    esp_err_t err = tinyusb_cdcacm_write_flush(CDC_PORT, pdMS_TO_TICKS(20));
    if (err != ESP_OK)
        ESP_LOGW(TAG, "USB CDC flush failed: %s", esp_err_to_name(err));
    xSemaphoreGive(usb_tx_mutex);
}

static void process_slcan_command(const char *command)
{
    if (slcan_parse_open(command) == SLCAN_OK) {
        esp_err_t err = can_adapter_open();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open CAN channel: %s", esp_err_to_name(err));
            usb_send("\a", 1);
            return;
        }
        set_channel_open(true);
        usb_send("\r", 1);
        return;
    }

    if (slcan_parse_close(command) == SLCAN_OK) {
        esp_err_t err = can_adapter_close();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to close CAN channel: %s", esp_err_to_name(err));
            usb_send("\a", 1);
            return;
        }
        set_channel_open(false);
        usb_send("\r", 1);
        return;
    }

    twai_timing_basic_config_t timing = { 0 };
    if (slcan_parse_bitrate(command, &timing) == SLCAN_OK) {
        esp_err_t err = can_adapter_set_bitrate(timing.bitrate);
        if (err == ESP_OK) {
            configured_bitrate = timing.bitrate;
            usb_send("\r", 1);
        } else if (err == ESP_ERR_INVALID_STATE) {
            usb_send("\a", 1);
        } else {
            ESP_LOGE(TAG, "Failed to set CAN bitrate: %s", esp_err_to_name(err));
            usb_send("\a", 1);
        }
        return;
    }

    if (command[0] == 't' || command[0] == 'T') {
        if (!is_channel_open()) {
            usb_send("\a", 1);
            return;
        }

        uint8_t data[CAN_ADAPTER_MAX_DATA_LEN];
        twai_frame_t parsed_frame;
        if (slcan_deserialize(command, &parsed_frame, data, sizeof(data)) != SLCAN_OK) {
            usb_send("\a", 1);
            return;
        }

        can_adapter_frame_t can_frame = {
            .id = parsed_frame.header.id,
            .dlc = parsed_frame.header.dlc,
            .extd = parsed_frame.header.ide,
        };
        memcpy(can_frame.data, data, can_frame.dlc);

        esp_err_t err = can_adapter_transmit(&can_frame);
        if (err == ESP_OK)
            usb_send("\r", 1);
        else {
            ESP_LOGW(TAG, "Failed to queue CAN frame: %s", esp_err_to_name(err));
            usb_send("\a", 1);
        }
        return;
    }

    if (strcmp(command, "V\r") == 0) {
        usb_send("V0100\r", sizeof("V0100\r") - 1);
        return;
    }
    if (strcmp(command, "N\r") == 0) {
        usb_send("N0001\r", sizeof("N0001\r") - 1);
        return;
    }
    if (strcmp(command, "F\r") == 0) {
        usb_send("F00\r", sizeof("F00\r") - 1);
        return;
    }

    usb_send("\a", 1);
}

static void consume_usb_bytes(const uint8_t *data, size_t len)
{
    static char command[SLCAN_COMMAND_SIZE];
    static size_t command_len;
    static bool discard_command;

    for (size_t i = 0; i < len; i++) {
        char byte = (char)data[i];
        if (byte == '\n')
            continue;
        if (byte == '\r') {
            if (discard_command) {
                usb_send("\a", 1);
            } else if (command_len + 1 < sizeof(command)) {
                command[command_len++] = '\r';
                command[command_len] = '\0';
                process_slcan_command(command);
            } else {
                usb_send("\a", 1);
            }
            command_len = 0;
            discard_command = false;
            continue;
        }

        if (discard_command)
            continue;
        if (command_len >= sizeof(command) - 1) {
            discard_command = true;
            continue;
        }
        command[command_len++] = byte;
    }
}

static void usb_rx_task(void *pvParameters)
{
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    size_t read_size = usb_buffer_size < sizeof(rx_buffer) ? usb_buffer_size : sizeof(rx_buffer);

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        size_t received = 0;
        do {
            esp_err_t err = tinyusb_cdcacm_read(CDC_PORT, rx_buffer, read_size, &received);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "USB CDC read failed: %s", esp_err_to_name(err));
                break;
            }
            if (received > 0)
                consume_usb_bytes(rx_buffer, received);
        } while (received == read_size);
    }
}

static void usb_tx_task(void *pvParameters)
{
    QueueHandle_t from_can = (QueueHandle_t)pvParameters;
    can_adapter_frame_t can_frame;
    char slcan_frame[SLCAN_COMMAND_SIZE];

    while (true) {
        if (xQueueReceive(from_can, &can_frame, portMAX_DELAY) != pdPASS)
            continue;
        if (!is_channel_open())
            continue;

        twai_frame_t frame = {
            .header = {
                .id = can_frame.id,
                .dlc = can_frame.dlc,
                .ide = can_frame.extd,
            },
            .buffer = can_frame.data,
            .buffer_len = sizeof(can_frame.data),
        };
        slcan_err_t err = slcan_serialize(&frame, slcan_frame, sizeof(slcan_frame));
        if (err != SLCAN_OK) {
            ESP_LOGE(TAG, "Failed to serialize received CAN frame");
            continue;
        }

        usb_send(slcan_frame, strlen(slcan_frame));
    }
}

static void tusb_cdc_rx_cb(BaseType_t intflag, cdcacm_event_t *event)
{
    (void)intflag;
    (void)event;

    BaseType_t higher_priority_woken = pdFALSE;
    if (usb_rx_task_handle != NULL)
        vTaskNotifyGiveFromISR(usb_rx_task_handle, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

esp_err_t usb_adapter_init(uint32_t buffer_size, QueueHandle_t can_rx_queue, uint32_t can_bitrate)
{
    if (buffer_size == 0 || can_rx_queue == NULL || can_bitrate == 0)
        return ESP_ERR_INVALID_ARG;

    usb_buffer_size = buffer_size;
    configured_bitrate = can_bitrate;

    const tinyusb_config_t tusb_config = {
        .task = {
            .size = TASK_DEPTH,
            .priority = USB_PRIORITY,
            .xCoreID = USB_CORE,
        }
    };
    const tinyusb_config_cdcacm_t acm_config = {
        .cdc_port = CDC_PORT,
        .callback_rx = tusb_cdc_rx_cb,
    };

    ESP_LOGI(TAG, "Initializing TinyUSB CDC");
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_config), TAG, "TinyUSB install failed");
    ESP_RETURN_ON_ERROR(tinyusb_cdcacm_init(&acm_config), TAG, "CDC ACM initialization failed");

    usb_tx_mutex = xSemaphoreCreateMutex();
    if (usb_tx_mutex == NULL)
        return ESP_ERR_NO_MEM;

    if (xTaskCreatePinnedToCore(usb_rx_task, "USB RX", TASK_DEPTH, NULL,
                                USB_PRIORITY, &usb_rx_task_handle, USB_CORE) != pdPASS)
    {
        vSemaphoreDelete(usb_tx_mutex);
        usb_tx_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreatePinnedToCore(usb_tx_task, "USB TX", TASK_DEPTH, can_rx_queue,
                                USB_PRIORITY, NULL, USB_CORE) != pdPASS) {
        vTaskDelete(usb_rx_task_handle);
        usb_rx_task_handle = NULL;
        vSemaphoreDelete(usb_tx_mutex);
        usb_tx_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
