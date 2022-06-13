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

#include <assert.h>
#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_hass.h>
#include <esp_log.h>
#include <esp_tls.h>
#include <esp_websocket_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <stdbool.h>

#include "parser.h"

#define ESP_HASS_RX_BUFFER_SIZE_BYTE (1024 * 10 + 1) // 10KB + NULL
#define ESP_HASS_QUEUE_SEND_WAIT_MS (1000)
#define ESP_HASS_VERSION_STRING_MAX_LEN (32)

static const char *TAG = "esp_hass";

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
	char *rx_buffer;
	bool is_authenticated;
	char ha_version[ESP_HASS_VERSION_STRING_MAX_LEN];
	cJSON *json;
	QueueHandle_t event_queue;
	QueueHandle_t result_queue;
};

static void
shutdown_handler(TimerHandle_t xTimer)
{
	ESP_LOGW(TAG, "timeout: No data received, shuting down");
}

static void
message_handler(esp_hass_client_handle_t client, esp_hass_message_t *msg)
{
	esp_err_t err = ESP_FAIL;
	BaseType_t rtos_err = pdFALSE;

	assert(client != NULL && msg != NULL && msg->json != NULL);

	switch (msg->type) {

	/* perform authentication if necessary. do not queue auth-related
	 * messages.
	 */
	case HASS_MESSAGE_TYPE_AUTH_INVALID:
		ESP_LOGE(TAG, "Authentication failed");
		/* FALLTHROUGH */
	case HASS_MESSAGE_TYPE_AUTH_REQUIRED:
		client->is_authenticated = false;

		/* ha_version is present in auth-related messages only */
		if (strlcpy(client->ha_version,
			cJSON_GetObjectItem(msg->json, "ha_version")
			    ->valuestring,
			sizeof(client->ha_version)) >=
		    sizeof(client->ha_version)) {
			ESP_LOGW(TAG, "ha_version in response too long");
		}
		ESP_LOGD(TAG, "ha_version: `%s`", client->ha_version);

		err = esp_hass_client_auth(client);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_hass_client_auth(): %s",
			    esp_err_to_name(err));
		}
		err = esp_hass_message_destroy(msg);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_hass_message_destroy(): %s",
			    esp_err_to_name(err));
		}
		break;
	case HASS_MESSAGE_TYPE_AUTH_OK:
		ESP_LOGI(TAG, "Authentication successful");
		client->is_authenticated = true;
		err = esp_hass_message_destroy(msg);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "esp_hass_message_destroy(): %s",
			    esp_err_to_name(err));
		}
		break;
	case HASS_MESSAGE_TYPE_RESULT:
		rtos_err = xQueueSend(client->result_queue, &msg,
		    ESP_HASS_QUEUE_SEND_WAIT_MS / portTICK_PERIOD_MS);
		if (rtos_err != pdTRUE) {
			ESP_LOGW(TAG, "xQueueSend(): failed");
			err = esp_hass_message_destroy(msg);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "esp_hass_message_destroy(): %s",
				    esp_err_to_name(err));
			}
		}
		break;
	case HASS_MESSAGE_TYPE_PONG:
	case HASS_MESSAGE_TYPE_EVENT:
	default:
		rtos_err = xQueueSend(client->event_queue, &msg,
		    ESP_HASS_QUEUE_SEND_WAIT_MS / portTICK_PERIOD_MS);
		if (rtos_err != pdTRUE) {
			ESP_LOGW(TAG, "xQueueSend(): failed");
			err = esp_hass_message_destroy(msg);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "esp_hass_message_destroy(): %s",
				    esp_err_to_name(err));
			}
		}
	}
}

static void
websocket_event_handler(void *handler_args, esp_event_base_t base,
    int32_t event_id, void *event_data)
{
	int data_string_len;
	char *data_string;

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
			ESP_LOGV(TAG, "Received=%.*s", data->data_len,
			    (char *)data->data_ptr);
		}

		ESP_LOGD(TAG,
		    "Total payload length=%d, data_len=%d, current payload offset=%d",
		    data->payload_len, data->data_len, data->payload_offset);
		assert(
		    data->payload_offset + data->data_len <= data->payload_len);

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

		/* data->data_len is not null-terminated string, but bytes.
		 * create a string from it. responses from Home Assistant
		 * server are always a string
		 */
		data_string_len = data->data_len + 1;
		data_string = calloc(1, data_string_len);
		if (data_string == NULL) {
			ESP_LOGE(TAG, "Out of memory");
			break;
		}
		snprintf(data_string, data_string_len, "%.*s", data->data_len,
		    (char *)data->data_ptr);
		if (strlcat(client->rx_buffer, data_string,
			ESP_HASS_RX_BUFFER_SIZE_BYTE) >=
		    ESP_HASS_RX_BUFFER_SIZE_BYTE) {
			ESP_LOGE(TAG,
			    "rx_buffer overflow detected. rx_buffer size: %d, payload_len: %d",
			    ESP_HASS_RX_BUFFER_SIZE_BYTE, data->payload_len);
			strlcpy(client->rx_buffer, "",
			    ESP_HASS_RX_BUFFER_SIZE_BYTE);
			free(data_string);
			break;
		}
		free(data_string);

		if (data->payload_offset + data->data_len < data->payload_len) {

			/* expect other fragments to arrive */
			break;
		}

		/* now we have a complete json string */
		ESP_LOGV(TAG, "client->rx_buffer: `%s`", client->rx_buffer);
		hass_message = esp_hass_message_parse(client->rx_buffer,
		    strlen(client->rx_buffer));
		if (hass_message == NULL) {
			ESP_LOGE(TAG, "esp_hass_message_parse(): failed");
			break;
		} else {
			message_handler(client, hass_message);
		}
		strlcpy(client->rx_buffer, "", ESP_HASS_RX_BUFFER_SIZE_BYTE);
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
	hass_client->rx_buffer = calloc(1, ESP_HASS_RX_BUFFER_SIZE_BYTE);
	ESP_LOGD(TAG, "hass_client->rx_buffer: `%s`", hass_client->rx_buffer);
	if (hass_client->rx_buffer == NULL) {
		ESP_LOGE(TAG, "Out of memory: rx_buffer");
		goto fail;
	}
	hass_client->config.access_token = config->access_token;
	hass_client->config.ws_config = config->ws_config;
	hass_client->config.timeout_sec = config->timeout_sec;
	hass_client->event_queue = config->event_queue;
	hass_client->result_queue = config->result_queue;
	hass_client->is_authenticated = false;
	hass_client->json = NULL;
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

	hass_client->shutdown_signal_timer =
	    xTimerCreate("Websocket shutdown timer",
		hass_client->config.timeout_sec * 1000 / portTICK_PERIOD_MS,
		pdFALSE, NULL, shutdown_handler);
	if (hass_client->shutdown_signal_timer == NULL) {
		ESP_LOGE(TAG, "xTimerCreate(): fail");
		err = ESP_ERR_NO_MEM;
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

	if (client->shutdown_signal_timer != NULL &&
	    xTimerDelete(client->shutdown_signal_timer, portMAX_DELAY) !=
		pdPASS) {
		ESP_LOGW(TAG, "xTimerDelete(): fail");
	}
	client->shutdown_signal_timer = NULL;
	if (client->ws_client_handle != NULL &&
	    esp_websocket_client_destroy(client->ws_client_handle) != ESP_OK) {
		ESP_LOGW(TAG, "esp_websocket_client_destroy(): fail");
	}
	client->ws_client_handle = NULL;
	if (client->json != NULL) {
		cJSON_Delete(client->json);
		client->json = NULL;
	}

	if (client->ws_client_handle != NULL &&
	    esp_websocket_client_destroy(client->ws_client_handle) != ESP_OK) {
		ESP_LOGW(TAG, "esp_websocket_client_destroy(): %s",
		    esp_err_to_name(err));
	}

	if (client->rx_buffer != NULL) {
		free(client->rx_buffer);
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
		client->config.access_token) == NULL) {
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
	if (json_string != NULL) {
		free(json_string);
		json_string = NULL;
	}
	if (client->json != NULL) {
		cJSON_Delete(client->json);
		client->json = NULL;
	}
	return err;
}

bool
esp_hass_client_is_connected(esp_hass_client_handle_t client)
{
	bool connected = false;
	if (client == NULL || client->ws_client_handle == NULL) {
		goto no;
	}
	connected = esp_websocket_client_is_connected(client->ws_client_handle);
no:
	return connected;
}

bool
esp_hass_client_is_authenticated(esp_hass_client_handle_t client)
{
	return client->is_authenticated;
}

static esp_err_t
esp_hass_create_message_subscribe_events(esp_hass_client_handle_t client,
    char *event_type)
{
	esp_err_t err = ESP_FAIL;

	if (client == NULL) {
		err = ESP_ERR_INVALID_ARG;
		goto fail;
	}

	if (client->json != NULL) {
		cJSON_Delete(client->json);
		client->json = NULL;
	}
	client->json = cJSON_CreateObject();
	if (cJSON_AddNumberToObject(client->json, "id", ++client->message_id) ==
	    NULL) {
		err = ESP_FAIL;
		goto fail;
	}
	if (cJSON_AddStringToObject(client->json, "type", "subscribe_events") ==
	    NULL) {
		err = ESP_FAIL;
		goto fail;
	}
	if (event_type != NULL) {
		if (cJSON_AddStringToObject(client->json, "event_type",
			event_type) == NULL) {
			err = ESP_FAIL;
			goto fail;
		}
	}
	err = ESP_OK;
	return err;
fail:
	if (client->json != NULL) {
		cJSON_Delete(client->json);
		client->json = NULL;
	}
	return err;
}

esp_err_t
esp_hass_client_subscribe_events(esp_hass_client_handle_t client,
    char *event_type)
{
	esp_err_t err = ESP_FAIL;
	char *json_string = NULL;
	int length, json_string_length;

	if (client == NULL) {
		err = ESP_ERR_INVALID_ARG;
		goto fail;
	}

	err = esp_hass_create_message_subscribe_events(client, event_type);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_create_message_subscribe_events(): %s",
		    esp_err_to_name(err));
		goto fail;
	}

	json_string = cJSON_Print(client->json);
	if (json_string == NULL) {
		ESP_LOGE(TAG, "cJSON_Print(): failed");
		err = ESP_FAIL;
		goto fail;
	}
	ESP_LOGI(TAG, "Sending subscribe_events command");
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
	if (json_string != NULL) {
		free(json_string);
		json_string = NULL;
	}
	if (client->json != NULL) {
		cJSON_Delete(client->json);
		client->json = NULL;
	}
	return err;
}

char *
esp_hass_client_get_ha_version(esp_hass_client_handle_t client)
{
	return client->ha_version;
}

esp_err_t
esp_hass_message_destroy(esp_hass_message_t *msg)
{
	if (msg == NULL) {
		goto success;
	}
	if (msg->json != NULL) {
		cJSON_Delete(msg->json);
		msg->json = NULL;
	}
	free(msg);
	msg = NULL;
success:
	return ESP_OK;
}

esp_err_t
esp_hass_send_message_json(esp_hass_client_handle_t client, cJSON *json)
{
	int length, json_string_length;
	esp_err_t err = ESP_FAIL;
	char *json_string = NULL;

	if (client == NULL) {
		err = ESP_ERR_INVALID_ARG;
		goto fail;
	}

	if (cJSON_AddNumberToObject(json, "id", ++client->message_id) == NULL) {
		err = ESP_FAIL;
		goto fail;
	}

	json_string = cJSON_Print(json);
	if (json_string == NULL) {
		ESP_LOGE(TAG, "cJSON_Print(): failed");
		err = ESP_FAIL;
		goto fail;
	}
	ESP_LOGI(TAG, "Sending message id: %d", client->message_id);
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
	if (json_string != NULL) {
		free(json_string);
		json_string = NULL;
	}
	return err;
}

static cJSON *
create_call_service_json(esp_hass_call_service_config_t *config)
{
	cJSON *json = NULL;
	cJSON *target = NULL;

	json = cJSON_CreateObject();
	if (json == NULL) {
		ESP_LOGE(TAG, "cJSON_CreateObject()");
		goto fail;
	}
	if (cJSON_AddStringToObject(json, "type", "call_service") == NULL) {
		ESP_LOGE(TAG, "cJSON_AddStringToObject()");
		goto fail;
	}
	if (cJSON_AddStringToObject(json, "domain", config->domain) == NULL) {
		ESP_LOGE(TAG, "cJSON_AddStringToObject()");
		goto fail;
	}
	if (cJSON_AddStringToObject(json, "service", config->service) == NULL) {
		ESP_LOGE(TAG, "cJSON_AddStringToObject()");
		goto fail;
	}

	target = cJSON_CreateObject();
	if (target == NULL) {
		ESP_LOGE(TAG, "cJSON_CreateObject()");
		goto fail_target;
	}
	if (cJSON_AddStringToObject(target, "entity_id", config->entity_id) ==
	    NULL) {
		ESP_LOGE(TAG, "cJSON_AddStringToObject()");
		goto fail_target;
	}
	if (cJSON_AddItemToObject(json, "target", target) != true) {
		ESP_LOGE(TAG, "cJSON_AddItemToObject()");
		goto fail_target;
	}
	return json;
fail_target:
	if (target != NULL) {
		cJSON_Delete(target);
	}
fail:
	if (json != NULL) {
		cJSON_Delete(json);
	}
	return NULL;
}

esp_err_t
esp_hass_call_service(esp_hass_client_handle_t client,
    esp_hass_call_service_config_t *config)
{
	esp_err_t err = ESP_FAIL;
	cJSON *json = NULL;
	cJSON *json_error = NULL;
	cJSON *json_error_msg = NULL;
	esp_hass_message_t *msg;

	ESP_LOGD(TAG, "domain: `%s` service: `%s` entity_id: `%s`",
	    config->domain, config->service, config->entity_id);
	json = create_call_service_json(config);
	if (json == NULL) {
		ESP_LOGE(TAG, "create_call_service_json:");
		err = ESP_FAIL;
		goto fail;
	}
	err = esp_hass_send_message_json(client, json);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_send_message_json: `%s`",
		    esp_err_to_name(err));
		goto fail;
	}
	if (xQueueReceive(config->result_queue, &msg, config->delay) !=
	    pdTRUE) {
		ESP_LOGE(TAG, "failed to receive result: timeout");
		err = ESP_FAIL;
		goto fail;
	}
	if (msg->type != HASS_MESSAGE_TYPE_RESULT) {
		ESP_LOGE(TAG,
		    "Unexpected response from the server: result type: %d",
		    msg->type);
		err = ESP_FAIL;
		goto fail;
	}
	if (msg->success) {
		ESP_LOGI(TAG, "calling service %s on entity %s successful",
		    config->service, config->entity_id);
	} else {
		ESP_LOGE(TAG, "server returned failure");
		json_error = cJSON_GetObjectItemCaseSensitive(msg->json,
		    "error");
		json_error_msg = cJSON_GetObjectItemCaseSensitive(json_error,
		    "message");
		if (cJSON_IsString(json_error_msg) &&
		    (json_error_msg->valuestring != NULL)) {
			ESP_LOGE(TAG, "error message: `%s`",
			    json_error_msg->valuestring);
		}
		err = ESP_FAIL;
		goto fail;
	}
	err = ESP_OK;
fail:
	esp_hass_message_destroy(msg);
	if (json != NULL) {
		cJSON_Delete(json);
	}
	if (json_error != NULL) {
		cJSON_Delete(json_error);
	}
	if (json_error_msg != NULL) {
		cJSON_Delete(json_error_msg);
	}
	return err;
}
