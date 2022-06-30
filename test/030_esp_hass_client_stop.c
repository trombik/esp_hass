#include <esp_hass.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <unity.h>

#include "helper.h"

static QueueHandle_t event_queue = NULL;
static QueueHandle_t result_queue = NULL;
static esp_hass_client_handle_t client = NULL;
static const char *TAG = "context";

TEST_CASE("when client given, return ESP_OK[esp_hass_client_start]",
    "[esp_hass_client_start]")
{
	bool is_context_failed = false;
	esp_hass_config_t *client_config = NULL;

	result_queue = create_result_queue();
	if (result_queue == NULL) {
		ESP_LOGE(TAG, "create_result_queue()");
		is_context_failed = true;
		goto fail;
	}
	event_queue = create_event_queue();
	if (event_queue == NULL) {
		ESP_LOGE(TAG, "create_event_queue()");
		is_context_failed = true;
		goto fail;
	}
	client_config = create_client_config(create_ws_config(), result_queue,
	    event_queue);
	client = esp_hass_init(client_config);
	if (client == NULL) {
		ESP_LOGE(TAG, "esp_hass_init()");
		is_context_failed = true;
		goto fail;
	}

	if (esp_hass_client_start(client) != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_client_start()");
		is_context_failed = true;
		goto fail;
	}
	TEST_ASSERT_EQUAL(ESP_OK, esp_hass_client_stop(client));
fail:
	if (is_context_failed) {
		TEST_FAIL_MESSAGE("context failed");
	}
	if (client != NULL) {
		esp_hass_destroy(client);
		client = NULL;
	}
	if (event_queue != NULL) {
		delete_queue(event_queue);
		event_queue = NULL;
	}
	if (result_queue != NULL) {
		delete_queue(result_queue);
		result_queue = NULL;
	}
}
