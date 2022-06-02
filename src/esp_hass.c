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

#include <esp_event.h>
#include <esp_hass.h>
#include <esp_log.h>
#include <esp_websocket_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <cJSON.h>
#include <esp_tls.h>
#include <esp_tls.h>
#include <esp_crt_bundle.h>

#define NO_DATA_TIMEOUT_SEC (30)

static const char *TAG = "esp_hass";
static SemaphoreHandle_t shutdown_sema;

typedef struct {
	char *uri;
	char *access_token;
	int timeout_sec;

} hass_config_storage_t;

struct esp_hass_client {
	esp_websocket_client_handle_t ws_client_handle;
	hass_config_storage_t config;
	TimerHandle_t shutdown_signal_timer;
	cJSON *json;
};

static void
shutdown_handler(TimerHandle_t xTimer)
{
	ESP_LOGW(TAG, "timeout: No data received, shuting down");
	xSemaphoreGive(shutdown_sema);
}

static void
websocket_event_handler(void *handler_args, esp_event_base_t base,
    int32_t event_id, void *event_data)
{
	esp_hass_client_handle_t client = (esp_hass_client_handle_t)
	    handler_args;
	esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)
	    event_data;
	switch (event_id) {
	case WEBSOCKET_EVENT_CONNECTED:
		ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
		break;
	case WEBSOCKET_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
		break;
	case WEBSOCKET_EVENT_DATA:
		ESP_LOGD(TAG, "WEBSOCKET_EVENT_DATA");
		ESP_LOGD(TAG, "Received opcode=%d", data->op_code);
		if (data->op_code == 0x08 && data->data_len == 2) {
			ESP_LOGW(TAG, "Received closed message with code=%d",
			    256 * data->data_ptr[0] + data->data_ptr[1]);
		} else {
			ESP_LOGW(TAG, "Received=%.*s", data->data_len,
			    (char *)data->data_ptr);
		}
		ESP_LOGW(TAG,
		    "Total payload length=%d, data_len=%d, current payload offset=%d",
		    data->payload_len, data->data_len, data->payload_offset);
		xTimerReset(client->shutdown_signal_timer, portMAX_DELAY);
		break;
	case WEBSOCKET_EVENT_ERROR:
		ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
		break;
	default:
		ESP_LOGW(TAG, "Unknown event_id: %d", event_id);
	}
}

void
esp_hass_hello_world()
{
	ESP_LOGI(TAG, "Hello world");
}

esp_hass_client_handle_t
esp_hass_init(esp_hass_config_t *config)
{
	esp_err_t err = ESP_FAIL;
	esp_hass_client_handle_t hass_client = NULL;
	esp_websocket_client_config_t ws_config = {};

	if (config == NULL) {
		ESP_LOGE(TAG, "esp_hass_init(): Invalid arg");
		goto fail;
	}

	hass_client = calloc(1, sizeof(struct esp_hass_client));
	if (hass_client == NULL) {
		ESP_LOGE(TAG, "calloc(): Out of memory");
		goto fail;
	}

	esp_tls_init_global_ca_store();
	ws_config.uri = config->uri;
	ws_config.crt_bundle_attach = esp_crt_bundle_attach;
	ws_config.task_stack = 4 * 1024 * 2;
	ESP_LOGI(TAG, "URI: %s", config->uri);

	hass_client->ws_client_handle = esp_websocket_client_init(&ws_config);
	if (hass_client->ws_client_handle == NULL) {
		ESP_LOGE(TAG, "esp_websocket_client_init(): fail");
		goto fail;
	}

	err = esp_websocket_register_events(hass_client->ws_client_handle,
	    WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)hass_client);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_websocket_register_events(): %s",
		    esp_err_to_name(err));
		goto fail;
	}

	return hass_client;
fail:
	esp_hass_destroy(hass_client);
	return NULL;
}

esp_err_t
esp_hass_destroy(esp_hass_client_handle_t client)
{
	esp_err_t err = ESP_FAIL;

	cJSON_Delete(client->json);
	err = esp_websocket_client_destroy(client->ws_client_handle);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "esp_websocket_client_destroy(): %s",
		    esp_err_to_name(err));
	}
	free(client);
	return ESP_OK;
}

esp_err_t
esp_hass_client_start(esp_hass_client_handle_t client)
{
	esp_err_t err = ESP_FAIL;

	if (client == NULL) {
		err = ESP_ERR_INVALID_ARG;
		goto fail;
	}

	client->shutdown_signal_timer = xTimerCreate("Websocket shutdown timer",
	    NO_DATA_TIMEOUT_SEC * 1000 / portTICK_PERIOD_MS, pdFALSE, NULL,
	    shutdown_handler);
	if (client->shutdown_signal_timer == NULL) {
		ESP_LOGE(TAG, "xTimerCreate(): fail");
		err = ESP_ERR_NO_MEM;
		goto fail;
	}

	shutdown_sema = xSemaphoreCreateBinary();
	if (shutdown_sema == NULL) {
		ESP_LOGE(TAG, "xSemaphoreCreateBinary(): fail");
		err = ESP_ERR_NO_MEM;
		goto fail;
	}

	//ESP_LOGI(TAG, "Connecting to %s", client->config->uri);

	err = esp_websocket_client_start(client->ws_client_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_websocket_client_start(): %s",
		    esp_err_to_name(err));
		goto fail;
	}

	if (xTimerStart(client->shutdown_signal_timer, portMAX_DELAY) !=
	    pdPASS) {
		ESP_LOGE(TAG, "xTimerStart(): fail");
		err = ESP_FAIL;
		goto fail;
	}
	if (xSemaphoreTake(shutdown_sema, portMAX_DELAY) != pdTRUE) {
		ESP_LOGE(TAG, "xSemaphoreTake(): failed");
		err = ESP_FAIL;
		goto fail;
	}

	err = ESP_OK;

fail:
	return err;
}

esp_err_t
esp_hass_client_stop(esp_hass_client_handle_t client)
{
	esp_err_t err = ESP_FAIL;

	if (client == NULL) {
		err = ESP_ERR_INVALID_ARG;
		goto fail;
	}

	err = esp_websocket_client_stop(client->ws_client_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_websocket_client_stop(): %s",
		    esp_err_to_name(err));
		goto fail;
	}

fail:
	return err;
}

static esp_err_t
esp_hass_create_message_auth(esp_hass_client_handle_t client)
{
	esp_err_t err = ESP_FAIL;

	if (client == NULL) {
		ESP_LOGE(TAG, "client is NULL");
		err = ESP_ERR_INVALID_ARG;
		goto fail;
	}

	client->json = cJSON_CreateObject();
	if (cJSON_AddStringToObject(client->json, "type", "auth") == NULL) {
		ESP_LOGE(TAG, "cJSON_AddStringToObject(): type auth");
		err = ESP_FAIL;
		goto fail;
	}
	if (cJSON_AddStringToObject(client->json, "access_token", CONFIG_HASS_ACCESS_TOKEN) == NULL) {
		ESP_LOGE(TAG, "cJSON_AddStringToObject(): type access_token");
		err = ESP_FAIL;
		goto fail;
	}

	err = ESP_OK;
fail:
	return err;
}

esp_err_t esp_hass_client_auth(esp_hass_client_handle_t client)
{
	esp_err_t err = ESP_FAIL;
	char *json_string = NULL;

	if (client == NULL) {
		err = ESP_ERR_INVALID_ARG;
		goto fail;
	}

	err = esp_hass_create_message_auth(client);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_create_message_auth(): %s", esp_err_to_name(err));
		goto fail;
	}

	json_string = cJSON_Print(client->json);
	ESP_LOGI(TAG, "auth message: %s", json_string);

	err = ESP_OK;
fail:
	return err;

}

esp_err_t esp_hass_client_ping(esp_hass_client_handle_t client)
{
	esp_err_t err = ESP_FAIL;

	if (client == NULL) {
		err = ESP_ERR_INVALID_ARG;
		goto fail;
	}

	if (esp_websocket_client_is_connected(client->ws_client_handle) != true) {
		ESP_LOGE(TAG, "Not connected");
		err = ESP_FAIL;
		goto fail;
	}

	err = ESP_OK;
fail:
	return err;
}
