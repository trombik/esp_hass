#include <esp_err.h>
#include <esp_hass.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <limits.h>
#include <unity.h>

static const char *TAG = "context";
static esp_hass_config_t client_config = ESP_HASS_CONFIG_DEFAULT();
static esp_websocket_client_config_t ws_config = { 0 };

TEST_CASE("returns NULL[esp_hass_client_init]", "[esp_hass_client_init]")
{
	ESP_LOGI(TAG, "when client_config is NULL");

	TEST_ASSERT_EQUAL(NULL, esp_hass_init(NULL));
}

TEST_CASE("returns NULL[esp_hass_client_init]", "[esp_hass_client_init]")
{
	ESP_LOGI(TAG, "when client_config has NULL result_queue");
	client_config.result_queue = NULL;

	TEST_ASSERT_EQUAL(NULL, esp_hass_init(&client_config));
}

TEST_CASE("returns NULL[esp_hass_client_init]", "[esp_hass_client_init]")
{
	ESP_LOGI(TAG, "when client_config has NULL ws_config");

	client_config.result_queue = xQueueCreate(5,
	    sizeof(struct esp_hass_message_t *));
	client_config.ws_config = NULL;

	TEST_ASSERT_EQUAL(NULL, esp_hass_init(&client_config));
	vQueueDelete(client_config.result_queue);
}

TEST_CASE("returns NULL[esp_hass_client_init]", "[esp_hass_client_init]")
{
	ESP_LOGI(TAG, "when ws_config has NULL uri");
	client_config.result_queue = xQueueCreate(5,
	    sizeof(struct esp_hass_message_t *));
	ws_config.uri = NULL;
	client_config.ws_config = &ws_config;

	TEST_ASSERT_EQUAL(NULL, esp_hass_init(&client_config));
	vQueueDelete(client_config.result_queue);
}

TEST_CASE("returns non-NULL pointer[esp_hass_client_init]",
    "[esp_hass_client_init]")
{
	client_config.result_queue = xQueueCreate(5,
	    sizeof(struct esp_hass_message_t *));
	client_config.event_queue = xQueueCreate(5,
	    sizeof(struct esp_hass_message_t *));
	ws_config.uri = "https://hass.example.org";
	client_config.ws_config = &ws_config;

	TEST_ASSERT_NOT_EQUAL(NULL, esp_hass_init(&client_config));
	vQueueDelete(client_config.result_queue);
	vQueueDelete(client_config.event_queue);
}

TEST_CASE("returns non-NULL pointer[esp_hass_client_init]",
    "[esp_hass_client_init]")
{
	ESP_LOGI(TAG, "when event_queue is NULL");
	client_config.result_queue = xQueueCreate(5,
	    sizeof(struct esp_hass_message_t *));
	client_config.event_queue = NULL;
	ws_config.uri = "https://hass.example.org";
	client_config.ws_config = &ws_config;

	TEST_ASSERT_NOT_EQUAL(NULL, esp_hass_init(&client_config));
	vQueueDelete(client_config.result_queue);
}
