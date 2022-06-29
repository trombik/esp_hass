#include <esp_err.h>
#include <esp_hass.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <unity.h>

#include "helper.h"

static const char *TAG = "context";
static int queue_len = 5;
static QueueHandle_t event_queue = NULL;
static QueueHandle_t result_queue = NULL;
static esp_hass_client_handle_t client = NULL;

TEST_CASE("return ESP_ERR_INVALID_ARG[esp_hass_client_start]",
    "[esp_hass_client_start]")
{
	ESP_LOGI(TAG, "when client is NULL");

	TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_hass_client_start(NULL));
}

TEST_CASE("return ESP_OK[esp_hass_client_start]", "[esp_hass_client_start]")
{
	bool is_context_failed = false;
	esp_hass_config_t *client_config =
	    create_client_config(create_ws_config(), result_queue, event_queue);

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
	client = esp_hass_init(client_config);
	if (client == NULL) {
		ESP_LOGE(TAG, "esp_hass_init()");
		is_context_failed = true;
		goto fail;
	}

	ESP_LOGI(TAG, "when client is given");

	TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_hass_client_start(NULL));
fail:
	if (is_context_failed) {
		TEST_FAIL_MESSAGE("context failed");
	}
	if (client != NULL) {
		esp_hass_destroy(client);
		client = NULL;
	}
	if (event_queue != NULL) {
		vQueueDelete(event_queue);
		event_queue = NULL;
	}
	if (result_queue != NULL) {
		vQueueDelete(result_queue);
		result_queue = NULL;
	}
}
