#include "can_adapter.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_log.h"

twai_node_handle_t node_hdl = NULL;

static void can_rx_task(void *pvParameters)
{

}

static void can_tx_task(void *pvParameters)
{

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
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));

    BaseType_t task_return = pdFAIL;
#if CONFIG_FREERTOS_UNICORE
    task_return = xTaskCreate(
        can_tx_task,
        "CAN TX",
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

    task_return = xTaskCreate(
        can_rx_task,
        "CAN RX",
        CAN_TASK_SIZE,
        config->can_rx_queue,
        CAN_TASK_PRIORITY,
        NULL
    );

    if(task_return != pdPASS)
    {
        ESP_LOGE(__FILE__, "[%s]: ERROR TO CREATE RX TASK", __func__);
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
        return ESP_ERR_INVALID_RESPONSE;
    }

    task_return = xTaskCreatePinnedToCore(
        can_rx_task,
        "CAN Rx",
        CAN_TASK_SIZE,
        config->can_rx_queue,
        CAN_TASK_PRIORITY,
        NULL, 
        CAN_CORE
    );

    if(task_return != pdPASS)
    {
        ESP_LOGE(__FILE__, "[%s]: ERROR TO CREATE RX TASK", __func__);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
#endif

    return ESP_OK;
}