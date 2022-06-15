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

#include <cJSON.h>
#include <esp_err.h>
#include <esp_log.h>

#include "esp_hass.h"

const cJSON_bool require_null_terminated = 1;
const char *TAG = "esp_hass:parser";

static esp_hass_message_type_t
string_to_type(const char *type)
{
	esp_hass_message_type_t message_type = HASS_MESSAGE_TYPE_UNKNOWN;

	/* the function expects a message from Home Assistant server only. other
	 * messages, such as `subscribe_events` or `call_service`, are ignored,
	 * and the return value is HASS_MESSAGE_TYPE_UNKNOWN.
	 */
	if (strcmp(type, "auth") == 0) {
		message_type = HASS_MESSAGE_TYPE_AUTH;
	} else if (strcmp(type, "auth_invalid") == 0) {
		message_type = HASS_MESSAGE_TYPE_AUTH_INVALID;
	} else if (strcmp(type, "auth_ok") == 0) {
		message_type = HASS_MESSAGE_TYPE_AUTH_OK;
	} else if (strcmp(type, "auth_required") == 0) {
		message_type = HASS_MESSAGE_TYPE_AUTH_REQUIRED;
	} else if (strcmp(type, "auth_invalid") == 0) {
		message_type = HASS_MESSAGE_TYPE_AUTH_INVALID;
	} else if (strcmp(type, "result") == 0) {
		message_type = HASS_MESSAGE_TYPE_RESULT;
	} else if (strcmp(type, "event") == 0) {
		message_type = HASS_MESSAGE_TYPE_EVENT;
	} else if (strcmp(type, "pong") == 0) {
		message_type = HASS_MESSAGE_TYPE_PONG;
	}

	if (message_type == HASS_MESSAGE_TYPE_UNKNOWN) {
		ESP_LOGW(TAG, "string_to_type(): Unknown message type: `%s`",
		    type);
	}
	return message_type;
}

esp_hass_message_t *
esp_hass_message_parse(char *data, int data_len)
{
	esp_hass_message_t *msg = NULL;
	cJSON *type = NULL;
	cJSON *id = NULL;
	cJSON *success = NULL;

	if (data == NULL) {
		goto fail;
	}

	msg = calloc(1, sizeof(esp_hass_message_t));
	if (msg == NULL) {
		ESP_LOGE(TAG, "calloc(): Out of memory");
		goto fail;
	}

	msg->json = cJSON_Parse(data);
	if (msg->json == NULL) {
		ESP_LOGE(TAG, "cJSON_Parse(): failed");
		goto fail;
	}

	type = cJSON_GetObjectItem(msg->json, "type");
	if (cJSON_IsString(type)) {
		msg->type = string_to_type(type->valuestring);
	} else {
		ESP_LOGW(TAG, "message type is not string");
		msg->type = HASS_MESSAGE_TYPE_UNKNOWN;
	}

	id = cJSON_GetObjectItem(msg->json, "id");
	if (cJSON_IsNumber(id)) {
		msg->id = id->valueint;
	} else {
		ESP_LOGD(TAG, "attribute `id` is not present");
		msg->id = -1;
	}

	switch (msg->type) {
	case HASS_MESSAGE_TYPE_RESULT:
		ESP_LOGD(TAG, "Message type: HASS_MESSAGE_TYPE_RESULT");
		success = cJSON_GetObjectItem(msg->json, "success");
		if (success != NULL) {
			if (cJSON_IsBool(success)) {
				ESP_LOGD(TAG, "cJSON_IsTrue(success): %s",
				    cJSON_IsTrue(success) ? "true" : "false");
				msg->success = cJSON_IsTrue(success);
			} else {
				ESP_LOGW(TAG,
				    "attribute `success` is not bool");
				msg->success = false;
			}
		} else {
			ESP_LOGD(TAG, "attribute `success` does not exist");
			msg->success = false;
		}
		break;
	default:
		break;
	}

	return msg;
fail:
	if (msg->json != NULL) {
		cJSON_Delete(msg->json);
		msg->json = NULL;
	}
	if (msg != NULL) {
		free(msg);
		msg = NULL;
	}
	return NULL;
}
