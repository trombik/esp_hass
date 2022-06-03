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
#include <esp_tls.h>
#include <esp_websocket_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

#include "parser.h"

static const char *TAG = "esp_hass";
static SemaphoreHandle_t shutdown_sema;

typedef struct {
	const char *access_token;
	int timeout_sec;
	const esp_websocket_client_config_t *ws_config;

} hass_config_storage_t;

struct esp_hass_client {
	esp_websocket_client_handle_t ws_client_handle;
	hass_config_storage_t config;
	TimerHandle_t shutdown_signal_timer;
	int message_id;
	cJSON *json;
};

static void
shutdown_handler(TimerHandle_t xTimer)
{
	ESP_LOGW(TAG, "timeout: No data received, shuting down");
	xSemaphoreGive(shutdown_sema);
}

static void
message_handler(esp_hass_client_handle_t client, esp_hass_message_t *msg)
{
	esp_err_t err = ESP_FAIL;

	switch (msg->type) {
	case HASS_MESSAGE_TYPE_AUTH_REQUIRED:
		ESP_LOGI(TAG, "Server requested auth");
		err = esp_hass_client_auth(client);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_hass_client_auth(): %s",
			    esp_err_to_name(err));
		}
		break;
	case HASS_MESSAGE_TYPE_AUTH_OK:
		ESP_LOGI(TAG,
		    "Server accepted auth, successfully authenticated");
		break;
	case HASS_MESSAGE_TYPE_AUTH_INVALID:
		ESP_LOGI(TAG, "Server rejected auth, failed to authenticate");
		break;
	case HASS_MESSAGE_TYPE_PONG:
		ESP_LOGI(TAG, "Server returned pong");
		break;
	default:
		ESP_LOGW(TAG, "Unknown message type received, ignoring");
	}
}

static void
websocket_event_handler(void *handler_args, esp_event_base_t base,
    int32_t event_id, void *event_data)
{
	esp_hass_client_handle_t client = (esp_hass_client_handle_t)
	    handler_args;
	esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)
	    event_data;
	esp_hass_message_t *hass_message;
	switch (event_id) {
	case WEBSOCKET_EVENT_CONNECTED:
		ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
		break;
	case WEBSOCKET_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
		break;
	case WEBSOCKET_EVENT_DATA:
		xTimerReset(client->shutdown_signal_timer, portMAX_DELAY);
		ESP_LOGD(TAG, "WEBSOCKET_EVENT_DATA");
		ESP_LOGD(TAG, "Received opcode=%d", data->op_code);
		if (data->op_code == 0x08 && data->data_len == 2) {
			ESP_LOGW(TAG, "Received closed message with code=%d",
			    256 * data->data_ptr[0] + data->data_ptr[1]);
		} else {
			ESP_LOGI(TAG, "Received=%.*s", data->data_len,
			    (char *)data->data_ptr);
		}
		ESP_LOGD(TAG,
		    "Total payload length=%d, data_len=%d, current payload offset=%d",
		    data->payload_len, data->data_len, data->payload_offset);

		/* ignore PONG frames because they are handled by websocket
		 * client
		 */
		if (data->op_code == 0x0a) {
			ESP_LOGD(TAG, "PONG received");
			break;
		}

		/* ignore empty payload */
		if (data->payload_len == 0) {
			ESP_LOGD(TAG, "empty payload received");
			break;
		}
		hass_message = esp_hass_message_parse((char *)data->data_ptr,
		    data->data_len);
		if (hass_message == NULL) {
			ESP_LOGE(TAG, "esp_hass_message_parse(): failed");
			break;
		} else {
			ESP_LOGI(TAG, "HASS Message type: %d",
			    hass_message->type);
			message_handler(client, hass_message);
		}
		esp_hass_message_destroy(hass_message);
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

	if (config == NULL) {
		ESP_LOGE(TAG, "esp_hass_init(): Invalid arg");
		goto fail;
	}

	hass_client = calloc(1, sizeof(struct esp_hass_client));
	if (hass_client == NULL) {
		ESP_LOGE(TAG, "calloc(): Out of memory");
		goto fail;
	}

	hass_client->message_id = 0;
	hass_client->config.access_token = config->access_token;
	hass_client->config.ws_config = config->ws_config;
	hass_client->config.timeout_sec = config->timeout_sec;
	ESP_LOGI(TAG, "API URI: %s", hass_client->config.ws_config->uri);
	ESP_LOGI(TAG, "API access token: ****** (deducted)");
	ESP_LOGI(TAG, "Websocket shutdown timeout: %d sec",
	    hass_client->config.timeout_sec);

	esp_tls_init_global_ca_store();

	hass_client->ws_client_handle = esp_websocket_client_init(
	    config->ws_config);
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
	client->json = NULL;
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
	    client->config.timeout_sec * 1000 / portTICK_PERIOD_MS, pdFALSE,
	    NULL, shutdown_handler);
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

	ESP_LOGI(TAG, "Connecting to %s", client->config.ws_config->uri);

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

	if (client->json != NULL) {
		cJSON_Delete(client->json);
		client->json = NULL;
	}
	client->json = cJSON_CreateObject();
	if (cJSON_AddStringToObject(client->json, "type", "auth") == NULL) {
		ESP_LOGE(TAG, "cJSON_AddStringToObject(): type auth");
		err = ESP_FAIL;
		goto fail;
	}
	if (cJSON_AddStringToObject(client->json, "access_token",
		CONFIG_HASS_ACCESS_TOKEN) == NULL) {
		ESP_LOGE(TAG, "cJSON_AddStringToObject(): type access_token");
		err = ESP_FAIL;
		goto fail;
	}

	err = ESP_OK;
fail:
	return err;
}

esp_err_t
esp_hass_client_auth(esp_hass_client_handle_t client)
{
	esp_err_t err = ESP_FAIL;
	char *json_string = NULL;
	int length, json_string_length;

	if (client == NULL) {
		err = ESP_ERR_INVALID_ARG;
		goto fail;
	}

	err = esp_hass_create_message_auth(client);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_create_message_auth(): %s",
		    esp_err_to_name(err));
		goto fail;
	}

	json_string = cJSON_Print(client->json);
	if (json_string == NULL) {
		ESP_LOGE(TAG, "cJSON_Print(): failed");
		err = ESP_FAIL;
		goto fail;
	}
	ESP_LOGI(TAG, "Sending auth message");
	json_string_length = strlen(json_string);
	length = esp_websocket_client_send_text(client->ws_client_handle,
	    json_string, json_string_length, portMAX_DELAY);
	if (length < 0) {
		ESP_LOGE(TAG, "esp_websocket_client_send_text(): failed");
		err = ESP_FAIL;
		goto fail;
	} else if (json_string_length != length) {
		ESP_LOGE(TAG,
		    "esp_websocket_client_send_text(): failed: data: %d bytes, data actually sent %d bytes",
		    json_string_length, length);
		err = ESP_FAIL;
		goto fail;
	}

	err = ESP_OK;
fail:
	return err;
}
