#include <esp_err.h>

/* include esp_err.h before esp_crt_bundle.h.
 * see https://github.com/espressif/esp-idf/issues/8606 and
 * https://github.com/espressif/esp-idf/commit/52170fba7f43d933232d7f9579ad4d9892162935
 *
 * otherwise, older esp-idf builds fail.
 */

#include <esp_crt_bundle.h>
#include <esp_hass.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "helper.h"

static int queue_len = 5;
static char *TAG = "helper";

QueueHandle_t
create_event_queue()
{
	static QueueHandle_t event_queue = NULL;
	event_queue = xQueueCreate(queue_len,
	    sizeof(struct esp_hass_message_t *));
	if (event_queue == NULL) {
		ESP_LOGE(TAG, "Out of memory");
	}
	return event_queue;
}

QueueHandle_t
create_result_queue()
{
	static QueueHandle_t result_queue = NULL;
	result_queue = xQueueCreate(queue_len,
	    sizeof(struct esp_hass_message_t *));
	if (result_queue == NULL) {
		ESP_LOGE(TAG, "Out of memory");
	}
	return result_queue;
}

void
delete_queue(QueueHandle_t result_queue)
{
	vQueueDelete(result_queue);
}

esp_websocket_client_config_t *
create_ws_config()
{
	static esp_websocket_client_config_t ws_config = { 0 };
	ws_config.uri = "https://hass.example.org/api/websocket";
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	ws_config.crt_bundle_attach = esp_crt_bundle_attach;
	ws_config.reconnect_timeout_ms = 10000;
	ws_config.network_timeout_ms = 10000;
#endif

	return &ws_config;
}

esp_hass_config_t *
create_client_config(esp_websocket_client_config_t *ws_config,
    QueueHandle_t result_queue, QueueHandle_t event_queue)
{
	static esp_hass_config_t client_config = ESP_HASS_CONFIG_DEFAULT();
	client_config.access_token = "foobar";
	client_config.timeout_sec = 30;
	client_config.ws_config = ws_config;
	client_config.result_queue = result_queue;
	client_config.event_queue = event_queue;

	return &client_config;
}
