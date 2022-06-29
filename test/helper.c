#include <esp_err.h>

/* include esp_err.h before esp_crt_bundle.h.
 * see https://github.com/espressif/esp-idf/issues/8606 and
 * https://github.com/espressif/esp-idf/commit/52170fba7f43d933232d7f9579ad4d9892162935
 *
 * otherwise, older esp-idf builds fail.
 */

#include <esp_crt_bundle.h>

#include "helper.h"

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
