/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2022 Tomoyuki Sakurai <y@trombik.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <esp_err.h>

/* include esp_err.h before esp_crt_bundle.h.
 * see https://github.com/espressif/esp-idf/issues/8606 and
 * https://github.com/espressif/esp-idf/commit/52170fba7f43d933232d7f9579ad4d9892162935
 *
 * otherwise, older esp-idf builds fail.
 */

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_event.h>
#include <esp_hass.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <stdio.h>

#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define EXAMPLE_ESP_MAXIMUM_RETRY (5)
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#define DEFAULT_TASK_STACK_SIZE_BYTE (4 * 1024)
#define TASK_STACK_SIZE_BYTE (DEFAULT_TASK_STACK_SIZE_BYTE * 5)
#define MESSAGE_QUEUE_LEN (5)

static const char *TAG = "example";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void
wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
    void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT &&
	    event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG, "connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

static esp_err_t
wifi_init()
{
	esp_err_t err = ESP_FAIL;

	ESP_LOGI(TAG, "Initializing WiFi");

	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
	    ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
	    IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

	wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established
	 * (WIFI_CONNECTED_BIT) or connection failed for the maximum number of
	 * re-tries (WIFI_FAIL_BIT). The bits are set by wifi_event_handler()
	 * (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
	    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
	    portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned,
	 * hence we can test which event actually happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
		    CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
		    CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}

	err = ESP_OK;
	return err;
}

static void
message_handler(void *args, esp_event_base_t base, int32_t id, void *event_data)
{
	esp_hass_message_t *msg = NULL;

	msg = (esp_hass_message_t *)event_data;
	ESP_LOGI(TAG, "event id: %d", id);
	ESP_LOGI(TAG, "message type: %d", msg->type);
	ESP_LOGI(TAG, "message id: %d", msg->id);
}

void
app_main(void)
{
	bool is_test_failed = true;
	int event_queue_len = MESSAGE_QUEUE_LEN;
	int result_queue_len = MESSAGE_QUEUE_LEN;
	uint32_t initial_heep_size, current_heep_size;
	char *json_string = NULL;
	esp_err_t err = ESP_FAIL;
	esp_hass_client_handle_t client = NULL;
	esp_websocket_client_config_t ws_config = { 0 };
	QueueHandle_t event_queue = NULL;
	QueueHandle_t result_queue = NULL;
	esp_hass_call_service_config_t call_service_config =
	    ESP_HASS_CALL_SERVICE_CONFIG_DEFAULT();

	/* define your domain, entity_id, and service here.
	 *
	 * the service is called twice before the loop.
	 */
	char *entity_id = CONFIG_EXAMPLE_CALL_SERVICE_ENTITI_ID;
	char *domain = CONFIG_EXAMPLE_CALL_SERVICE_DOMAIN;
	char *service = CONFIG_EXAMPLE_CALL_SERVICE;

	/* increase log level in esp_hass component only for debugging */
	esp_log_level_set("esp_hass", ESP_LOG_VERBOSE);
	esp_log_level_set("esp_hass:parser", ESP_LOG_VERBOSE);
	ws_config.uri = CONFIG_EXAMPLE_HASS_URI;

/* version 5.x uses my fork with a fix to crt_bundle_attach, but older
 * versions do not have the fix.
 * see https://github.com/espressif/esp-protocols/pull/49
 *
 * this means the example does not work with older esp-idf versions.
 */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	/* use default CA bundle */
	ws_config.crt_bundle_attach = esp_crt_bundle_attach;
#endif

	/* increase the default task_stack size to enable debug log */
	ws_config.task_stack = TASK_STACK_SIZE_BYTE;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	/* set timeouts here to surpress warnings from WEBSOCKET_CLIENT */
	ws_config.reconnect_timeout_ms = 10000;
	ws_config.network_timeout_ms = 10000;
#endif

	event_queue = xQueueCreate(event_queue_len,
	    sizeof(struct esp_hass_message_t *));
	if (event_queue == NULL) {
		ESP_LOGE(TAG, "xQueueCreate(): Out of memory");
		goto init_fail;
	}
	result_queue = xQueueCreate(result_queue_len,
	    sizeof(struct esp_hass_message_t *));
	if (result_queue == NULL) {
		ESP_LOGE(TAG, "xQueueCreate(): Out of memory");
		goto init_fail;
	}

	/* use ESP_HASS_CONFIG_DEFAULT() macro to initialize
	 * esp_hass_config_t for forward compatibiity.
	 */
	esp_hass_config_t config = ESP_HASS_CONFIG_DEFAULT();
	config.access_token = CONFIG_EXAMPLE_HASS_ACCESS_TOKEN;
	config.timeout_sec = 30;
	config.ws_config = &ws_config;
	config.event_queue = event_queue;
	config.result_queue = result_queue;

	/* Initialize NVS */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
	    ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	err = wifi_init();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "wifi_init(): %s", esp_err_to_name(err));
		goto init_fail;
	}

	ESP_LOGI(TAG, "Initializing hass client");
	client = esp_hass_init(&config);
	if (client == NULL) {
		ESP_LOGE(TAG, "esp_hass_init(): failed");
		goto init_fail;
	}

	ESP_LOGI(TAG, "Starting hass client");
	err = esp_hass_client_start(client);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_client_start(): %s",
		    esp_err_to_name(err));
		goto start_fail;
	}

	ESP_LOGI(TAG, "Waiting for WebSocket connection");
	do {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	} while (!esp_hass_client_is_connected(client));

	ESP_LOGI(TAG, "Waiting for client to be authenticated");
	do {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	} while (!esp_hass_client_is_authenticated(client));

	/* ha_version is available after authentication attempt */
	ESP_LOGI(TAG, "Home assisstant version: %s",
	    esp_hass_client_get_ha_version(client));

	ESP_LOGI(TAG, "Subscribe to all events");
	err = esp_hass_client_subscribe_events(client, NULL);
	if (err != ESP_OK) {
		goto fail;
	}

	/* call a service, receive the result */
	ESP_LOGI(TAG, "Call a service");
	call_service_config.domain = domain;
	call_service_config.service = service;
	call_service_config.entity_id = entity_id;
	call_service_config.delay = portMAX_DELAY;

	err = esp_hass_call_service(client, &call_service_config);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_call_service: `%s`",
		    esp_err_to_name(err));
		goto fail;
	}

	/* call the service (`toggle`) again so that the service retains the
	 * original state.
	 */
	err = esp_hass_call_service(client, &call_service_config);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_call_service: `%s`",
		    esp_err_to_name(err));
		goto fail;
	}

	/* register message_handler */
	err = esp_hass_event_handler_register(client, message_handler);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_event_handler_register(): %s",
		    esp_err_to_name(err));
		goto fail;
	}

	/* listen to event messages, and print received messages. */
	ESP_LOGI(TAG, "Starting loop");
	initial_heep_size = esp_get_free_heap_size();
	for (int i = 0; i < 600; i++) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

	current_heep_size = esp_get_free_heap_size();
	ESP_LOGI(TAG,
	    "initial_heep_size %d, current heep size %d, difference %d",
	    initial_heep_size, current_heep_size,
	    initial_heep_size - current_heep_size);

	is_test_failed = false;
fail:
	ESP_LOGI(TAG, "Stopping hass client");
	err = esp_hass_client_stop(client);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_client_stop(): %s",
		    esp_err_to_name(err));
	}
start_fail:
	if (json_string != NULL) {
		free(json_string);
		json_string = NULL;
	}
	ESP_LOGI(TAG, "Destroying hass client");
	err = esp_hass_destroy(client);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_destroy(): %s", esp_err_to_name(err));
	}

init_fail:
	if (event_queue != NULL) {
		vQueueDelete(event_queue);
	}
	if (result_queue != NULL) {
		vQueueDelete(result_queue);
	}
	ESP_LOGI(TAG, "The example terminated %s error. Please reboot.",
	    is_test_failed ? "with" : "without");
	while (1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
