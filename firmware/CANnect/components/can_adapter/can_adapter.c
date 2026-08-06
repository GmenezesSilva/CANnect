#include "can_adapter.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_log.h"

static twai_node_handle_t node_hdl = NULL;

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
        can_data.id = rx_frame.header.id;
        can_data.dlc = rx_frame.header.dlc;
        can_data.extd = rx_frame.header.ide;
        
        xQueueSendFromISR(rx_queue, &can_data, &higher_priority_woken);
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
            twai_frame_t tx_frame = {
                .header = {
                    .id = internal_frame.id,
                    .dlc = internal_frame.dlc,
                    .ide = internal_frame.extd,
                },
                .buffer = internal_frame.data,
                .buffer_len = internal_frame.dlc,
            };

            if(twai_node_transmit(node_hdl, &tx_frame, pdMS_TO_TICKS(100)) != ESP_OK)
            {
                ESP_LOGE(__FILE__, "[%s]: Fail to transmit can frame", __func__);
            }
        }
    }
}

esp_err_t can_adapter_init(const can_adapter_config_t *config)
{
    // Driver currently supports only one CAN controller instance.    
    if(node_hdl != NULL)
    {
        ESP_LOGW(__FILE__,"[%s]: CAN Adapter already initialized", __func__);
        return ESP_ERR_INVALID_ARG;
    }

    if(config == NULL)
        return ESP_ERR_INVALID_ARG;

    if((config->can_bitrate <= 0)    || 
      (config->can_rx_pin < 0)       || 
      (config->can_tx_pin < 0)       || 
      (config->can_rx_queue == NULL) || 
      (config->can_tx_queue == NULL) ||
      (config->queue_size <= 0))
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
        twai_node_disable(node_hdl);
        twai_node_delete(node_hdl);
        node_hdl = NULL;
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
        twai_node_delete(node_hdl);
        node_hdl = NULL;
        return ESP_ERR_INVALID_RESPONSE;
    }
#endif

    ESP_LOGI(__FILE__, "[%s]: CAN adapter initialized successfully",__func__);
    return ESP_OK;
}