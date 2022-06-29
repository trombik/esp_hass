#if !defined(__HELPER_H__)
#define __HELPER_H__

#include <esp_hass.h>
#include <esp_websocket_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

esp_websocket_client_config_t *create_ws_config();

esp_hass_config_t *
create_client_config(esp_websocket_client_config_t *ws_config,
    QueueHandle_t result_queue, QueueHandle_t event_queue);

#endif
