#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_hass.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <limits.h>
#include <unity.h>

static const char *TAG = "context";
static int queue_len = 5;
static QueueHandle_t event_queue = NULL;
static QueueHandle_t result_queue = NULL;
static esp_hass_client_handle_t client = NULL;

static esp_websocket_client_config_t *
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

static esp_hass_config_t *
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

TEST_CASE("return ESP_OK[esp_hass_destroy]", "[esp_hass_destroy]")
{
	ESP_LOGI(TAG, "when client is NULL");

	TEST_ASSERT_EQUAL(ESP_OK, esp_hass_destroy(NULL));
}

TEST_CASE("return ESP_OK[esp_hass_destroy]", "[esp_hass_destroy]")
{
	bool is_context_failed = false;
	result_queue = xQueueCreate(queue_len,
	    sizeof(struct esp_hass_message_t *));
	if (result_queue == NULL) {
		ESP_LOGE(TAG, "xQueueCreate(): Out of memory");
		is_context_failed = true;
		goto fail;
	}
	event_queue = xQueueCreate(queue_len,
	    sizeof(struct esp_hass_message_t *));
	if (event_queue == NULL) {
		ESP_LOGE(TAG, "xQueueCreate(): Out of memory");
		is_context_failed = true;
		goto fail;
	}
	esp_hass_config_t *client_config =
	    create_client_config(create_ws_config(), result_queue, event_queue);
	client = esp_hass_init(client_config);
	if (client == NULL) {
		ESP_LOGE(TAG, "esp_hass_init()");
		is_context_failed = true;
		goto fail;
	}

	ESP_LOGI(TAG, "when client is given");

	TEST_ASSERT_EQUAL(ESP_OK, esp_hass_destroy(client));

fail:
	if (is_context_failed) {
		TEST_FAIL_MESSAGE("context failed");
	}
	vQueueDelete(event_queue);
	vQueueDelete(result_queue);
}
