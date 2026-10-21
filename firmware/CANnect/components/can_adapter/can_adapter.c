#include "can_adapter.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_log.h"
#include <string.h>

static twai_node_handle_t node_hdl = NULL;
static can_adapter_config_t adapter_config;
static SemaphoreHandle_t adapter_mutex;
static bool node_enabled;

static bool can_rx_receive_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx);

static esp_err_t create_node(void)
{
    const twai_onchip_node_config_t node_config = {
        .bit_timing.bitrate = adapter_config.can_bitrate,
        .io_cfg.rx = adapter_config.can_rx_pin,
        .io_cfg.tx = adapter_config.can_tx_pin,
        .tx_queue_depth = adapter_config.queue_size,
    };

    esp_err_t err = twai_new_node_onchip(&node_config, &node_hdl);
    if (err != ESP_OK)
        return err;

    const twai_event_callbacks_t callbacks = {
        .on_rx_done = can_rx_receive_cb,
    };
    err = twai_node_register_event_callbacks(node_hdl, &callbacks, adapter_config.can_rx_queue);
    if (err != ESP_OK) {
        twai_node_delete(node_hdl);
        node_hdl = NULL;
    }
    return err;
}

static bool can_rx_receive_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    QueueHandle_t rx_queue = (QueueHandle_t)user_ctx;
    can_adapter_frame_t can_data = {0};

    twai_frame_t rx_frame = {
        .buffer = can_data.data,
        .buffer_len = sizeof(can_data.data),
    };
    
    BaseType_t higher_priority_woken = pdFALSE;

    if(twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK)
    {
        if (rx_frame.header.dlc > CAN_ADAPTER_MAX_DATA_LEN)
            return false;

        can_data.id = rx_frame.header.id;
        can_data.dlc = rx_frame.header.dlc;
        can_data.extd = rx_frame.header.ide;
        
        if (xQueueSendFromISR(rx_queue, &can_data, &higher_priority_woken) != pdPASS)
            return false;
    }

    return higher_priority_woken;
}

static void can_tx_task(void *pvParameters)
{
    QueueHandle_t tx_queue = (QueueHandle_t)pvParameters;
    can_adapter_frame_t internal_frame = {0};
    
    while(1)
    {
        if(xQueueReceive(tx_queue, &internal_frame, portMAX_DELAY) == pdPASS)
        {
            if (xSemaphoreTake(adapter_mutex, portMAX_DELAY) != pdTRUE)
                continue;
            if (node_hdl == NULL || !node_enabled) {
                xSemaphoreGive(adapter_mutex);
                continue;
            }

            twai_frame_t tx_frame = {
                .header = {
                    .id = internal_frame.id,
                    .dlc = internal_frame.dlc,
                    .ide = internal_frame.extd,
                },
                .buffer = internal_frame.data,
                .buffer_len = internal_frame.dlc,
            };

            esp_err_t err = twai_node_transmit(node_hdl, &tx_frame, pdMS_TO_TICKS(100));
            xSemaphoreGive(adapter_mutex);
            if(err != ESP_OK)
            {
                ESP_LOGE("can_adapter", "Failed to transmit CAN frame: %s", esp_err_to_name(err));
            }
        }
    }
}

esp_err_t can_adapter_init(const can_adapter_config_t *config)
{
    if(config == NULL)
        return ESP_ERR_INVALID_ARG;

    if((config->can_bitrate == 0)    ||
      (config->can_rx_pin < 0)       || 
      (config->can_tx_pin < 0)       || 
      (config->can_rx_queue == NULL) || 
      (config->can_tx_queue == NULL) ||
      (config->queue_size == 0))
        return ESP_ERR_INVALID_ARG;

    if (node_hdl != NULL || adapter_mutex != NULL)
        return ESP_ERR_INVALID_STATE;

    adapter_config = *config;
    adapter_mutex = xSemaphoreCreateMutex();
    if (adapter_mutex == NULL)
        return ESP_ERR_NO_MEM;

    esp_err_t err = create_node();
    if (err != ESP_OK)
        goto fail;

    if (xTaskCreate(can_tx_task, "CAN Tx", CAN_TASK_SIZE, config->can_tx_queue,
                    CAN_TASK_PRIORITY, NULL) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        twai_node_delete(node_hdl);
        node_hdl = NULL;
        goto fail;
    }

    ESP_LOGI("can_adapter", "CAN adapter initialized; channel is closed");
    return ESP_OK;

fail:
    vSemaphoreDelete(adapter_mutex);
    adapter_mutex = NULL;
    return err;
}

esp_err_t can_adapter_open(void)
{
    if (adapter_mutex == NULL)
        return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(adapter_mutex, portMAX_DELAY) != pdTRUE)
        return ESP_ERR_TIMEOUT;

    esp_err_t err = node_hdl == NULL ? ESP_ERR_INVALID_STATE :
                    (node_enabled ? ESP_OK : twai_node_enable(node_hdl));
    if (err == ESP_OK)
        node_enabled = true;
    xSemaphoreGive(adapter_mutex);
    return err;
}

esp_err_t can_adapter_close(void)
{
    if (adapter_mutex == NULL)
        return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(adapter_mutex, portMAX_DELAY) != pdTRUE)
        return ESP_ERR_TIMEOUT;

    esp_err_t err = ESP_OK;
    if (node_hdl == NULL) {
        err = ESP_ERR_INVALID_STATE;
    } else if (node_enabled) {
        err = twai_node_disable(node_hdl);
        if (err == ESP_OK)
            node_enabled = false;
    }
    if (err == ESP_OK)
        xQueueReset(adapter_config.can_tx_queue);
    xSemaphoreGive(adapter_mutex);
    return err;
}

esp_err_t can_adapter_set_bitrate(uint32_t bitrate)
{
    if (bitrate == 0 || adapter_mutex == NULL)
        return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(adapter_mutex, portMAX_DELAY) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    if (node_enabled) {
        xSemaphoreGive(adapter_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t previous_bitrate = adapter_config.can_bitrate;
    if (node_hdl == NULL) {
        adapter_config.can_bitrate = bitrate;
        esp_err_t restore_err = create_node();
        if (restore_err != ESP_OK)
            adapter_config.can_bitrate = previous_bitrate;
        xSemaphoreGive(adapter_mutex);
        return restore_err;
    }

    esp_err_t err = twai_node_delete(node_hdl);
    if (err != ESP_OK) {
        xSemaphoreGive(adapter_mutex);
        return err;
    }
    node_hdl = NULL;
    adapter_config.can_bitrate = bitrate;
    err = create_node();
    if (err != ESP_OK) {
        adapter_config.can_bitrate = previous_bitrate;
        esp_err_t restore_err = create_node();
        if (restore_err != ESP_OK)
            ESP_LOGE("can_adapter", "Failed to restore CAN node after bitrate error: %s",
                     esp_err_to_name(restore_err));
    }

    xSemaphoreGive(adapter_mutex);
    return err;
}

esp_err_t can_adapter_transmit(const can_adapter_frame_t *frame)
{
    if (frame == NULL || frame->dlc > CAN_ADAPTER_MAX_DATA_LEN ||
        (!frame->extd && frame->id > 0x7FF) ||
        (frame->extd && frame->id > 0x1FFFFFFF))
        return ESP_ERR_INVALID_ARG;
    if (adapter_mutex == NULL)
        return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(adapter_mutex, portMAX_DELAY) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    if (!node_enabled) {
        xSemaphoreGive(adapter_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t queued = xQueueSend(adapter_config.can_tx_queue, frame, 0);
    xSemaphoreGive(adapter_mutex);
    return queued == pdPASS ? ESP_OK : ESP_ERR_TIMEOUT;
}