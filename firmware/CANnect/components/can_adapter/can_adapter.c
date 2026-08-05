#include "can_adapter.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_log.h"

static twai_node_handle_t node_hdl = NULL;

static bool can_rx_receive_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    QueueHandle_t rx_queue = (QueueHandle_t)user_ctx;
    uint8_t recv_buff[8];
    twai_frame_t rx_frame = {
        .buffer = recv_buff,
        .buffer_len = sizeof(recv_buff),
    };
    BaseType_t higher_priority_woken = pdFALSE;

    if(twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK)
    {
        xQueueSendFromISR(rx_queue, &rx_frame, &higher_priority_woken);
    }

    return higher_priority_woken;
}

static void can_tx_task(void *pvParameters)
{
    while(1)
    {
        vTaskDelay(10000);
    }
}

esp_err_t can_adapter_init(const can_adapter_config_t *config)
{
    if(config == NULL)
        return ESP_ERR_INVALID_ARG;

    if((config->can_bitrate <= 0) || 
      (config->can_rx_pin < 0) || 
      (config->can_tx_pin < 0) || 
      (config->can_rx_queue == NULL) || 
      (config->can_tx_queue == NULL))
        return ESP_ERR_INVALID_ARG;

    twai_onchip_node_config_t node_config =
    {
        .bit_timing.bitrate = config->can_bitrate,
        .io_cfg.rx          = config->can_rx_pin,
        .io_cfg.tx          = config->can_tx_pin,
        .tx_queue_depth     = config->queue_size,
    };
    
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));

    twai_event_callbacks_t can_receive_cbs = {
        .on_rx_done = can_rx_receive_cb,
    };

    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &can_receive_cbs, config->can_rx_queue));
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));

    BaseType_t task_return = pdFAIL;

#if CONFIG_FREERTOS_UNICORE
    task_return = xTaskCreate(
        can_tx_task,
        "CAN Tx",
        CAN_TASK_SIZE,
        config->can_tx_queue,
        CAN_TASK_PRIORITY,
        NULL
    );

    if(task_return != pdPASS)
    {
        ESP_LOGE(__FILE__, "[%s]: ERROR TO CREATE TX TASK", __func__);
        return ESP_ERR_INVALID_RESPONSE;
    }

#else
    task_return = xTaskCreatePinnedToCore(
        can_tx_task,
        "CAN Tx",
        CAN_TASK_SIZE,
        config->can_tx_queue,
        CAN_TASK_PRIORITY,
        NULL, 
        CAN_CORE
    );

    if(task_return != pdPASS)
    {
        ESP_LOGE(__FILE__, "[%s]: ERROR TO CREATE TX TASK", __func__);
        twai_node_disable(node_hdl);
        twai_del_node_onchip(node_hdl);
        return ESP_ERR_INVALID_RESPONSE;
    }
#endif

    ESP_LOGI(__FILE__, "[%s]: CAN adapter initialized successfully",__func__);
    return ESP_OK;
}