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

#if !defined(__ESP_HASS__H__)
#define __ESP_HASS__H__

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
#include <esp_err.h>
#include <esp_websocket_client.h>
#include <freertos/queue.h>
#include <stdbool.h>

/**
 * Home Assistant message types.
 *
 * See https://developers.home-assistant.io/docs/api/websocket
 */
typedef enum {
	HASS_MESSAGE_TYPE_UNKNOWN = -1,
	HASS_MESSAGE_TYPE_AUTH = 0,
	HASS_MESSAGE_TYPE_AUTH_INVALID,
	HASS_MESSAGE_TYPE_AUTH_OK,
	HASS_MESSAGE_TYPE_AUTH_REQUIRED,
	HASS_MESSAGE_TYPE_CALL_SERVICE,
	HASS_MESSAGE_TYPE_EVENT,
	HASS_MESSAGE_TYPE_FIRE_EVENT,
	HASS_MESSAGE_TYPE_GET_CAMERA_THUMBNAIL,
	HASS_MESSAGE_TYPE_GET_CONFIG,
	HASS_MESSAGE_TYPE_GET_PANELS,
	HASS_MESSAGE_TYPE_GET_SERVICES,
	HASS_MESSAGE_TYPE_GET_STATES,
	HASS_MESSAGE_TYPE_MEDIA_PLAYER_THUMBNAIL,
	HASS_MESSAGE_TYPE_PING,
	HASS_MESSAGE_TYPE_PONG,
	HASS_MESSAGE_TYPE_RESULT,
	HASS_MESSAGE_TYPE_SUBSCRIBE_EVENTS,
	HASS_MESSAGE_TYPE_SUBSCRIBE_TRIGGER,
	HASS_MESSAGE_TYPE_UNSUBSCRIBE_EVENTS,
	HASS_MESSAGE_TYPE_VALIDATE_CONFIG,

	HASS_MESSAGE_TYPE_MAX,
} esp_hass_message_type_t;

/**
 * Types of success in response messages
 */
typedef enum {
	HASS_MESSAGE_STATUS_SUCCESS, /*!< Success, or true */
	HASS_MESSAGE_STATUS_FAIL,    /*!< Failed, or false */
	HASS_MESSAGE_STATUS_UNDEF,   /*!< Undefined, or the message does have
					`success` field, such as `pong` response
				      */
	HASS_MESSAGE_STATUS_MAX,
} esp_hass_message_status_t;

/**
 * Home Assistant Mesage
 */
typedef struct {
	esp_hass_message_type_t type; /*!< Message type */
	int id; /*!< Message ID if any. -1 if the message does not have `id`
		   field */
	bool success; /*!< Command result status */
	cJSON *json;  /*!< Pointer to cJSON struct of the message */
} esp_hass_message_t;

/**
 * esp_hass configuration
 */
typedef struct {
	const char *access_token; /*!< The access token */
	const int timeout_sec; /*!< Timeout in second when no response is back
				  from the server. */
	const esp_websocket_client_config_t
	    *ws_config; /*!< configuration of esp_websocket_client */

} esp_hass_config_t;

/**
 * The esp_hass client handle
 */
typedef struct esp_hass_client *esp_hass_client_handle_t;

/**
 * @brief Initilize hass client. This function should be called before any
 * `esp_hass_*` function.
 *
 * @param[in] config Configuration.
 * @param[in] queue Queue handle for received hass messages.
 *
 * @return
 *	- pointer to hass client handle. This handle must be passed to other
 *	`esp_hass_*` functions.
 *	- NULL if failed.
 */
esp_hass_client_handle_t esp_hass_init(esp_hass_config_t *config,
    QueueHandle_t queue);

/**
 * @brief Destroy hass client. When the client is not needed, this function
 * should be called.
 *
 * @param[in] client hass client handle.
 *
 * @return
 *	- pointer to hass client handle.
 *	- NULL if failed.
 */
esp_err_t esp_hass_destroy(esp_hass_client_handle_t client);

/**
 * @brief Start the hass client.
 *
 * @param[in] client hass client handle.
 *
 * @return
 *	- ESP_OK if successful
 */
esp_err_t esp_hass_client_start(esp_hass_client_handle_t client);

/**
 * @brief Stop the hass client.
 *
 * @param[in] client hass client handle.
 *
 * @return
 *	- ESP_OK if successful
 */
esp_err_t esp_hass_client_stop(esp_hass_client_handle_t client);

/**
 * @brief Sending ping request. See:
 * https://developers.home-assistant.io/docs/api/websocket#pings-and-pongs
 *
 * @param[in] client The hass client.
 *
 * @return
 *	- ESP_OK if successful
 *	- ESP_FAIL if failed
 */
esp_err_t esp_hass_client_ping(esp_hass_client_handle_t client);

/**
 * @brief Perform authentication. See
 * https://developers.home-assistant.io/docs/api/websocket#authentication-phase
 *
 * @param[in] client The hass client
 *
 * @return
 *   - ESP_OK if successful
 *   - ESP_FAIL if failed
 *   - ESP_ERR_INVALID_ARG if client is NULL
 */
esp_err_t esp_hass_client_auth(esp_hass_client_handle_t client);

/**
 * @brief Subscribe to events.
 *
 * @param[in] client The hass client
 * @param[in] event_type Type of event to subscribe. One of event types or
 * NULL. When NULL, Subscribes to all events. See a list of event types
 * at: https://www.home-assistant.io/docs/configuration/events/
 *
 * @return
 *   - ESP_OK if successful
 */
esp_err_t esp_hass_client_subscribe_events(esp_hass_client_handle_t client,
    char *event_type);

/**
 * @brief See if WebSocket is connected.
 *
 * @param[in] client The hass client
 *
 * @return bool
 *
 */
bool esp_hass_client_is_connected(esp_hass_client_handle_t client);

/**
 * @brief See if esp_hass client has been authenticated.
 *
 * @param[in] client The hass client
 *
 * @return bool
 */

bool esp_hass_client_is_authenticated(esp_hass_client_handle_t client);

/**
 * @brief Destroy `esp_hass_message_t`, free() the memory allocated for the
 * message. Must be called after receiving a message from the queue, and done
 * with the message.
 *
 * @return
 * - ESP_OK if success
 */
esp_err_t esp_hass_message_destroy(esp_hass_message_t *msg);

/**
 * @brief Get Home Assistant version. The version is only available after
 * authentication attempt.
 *
 * @param[in] client The hass client
 *
 * @return ha_version string
 */
char *esp_hass_client_get_ha_version(esp_hass_client_handle_t client);

#ifdef __cplusplus
}
#endif

#endif
