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
 *
 * `esp_hass` is a library to communicate with a Home Assistant, based on
 * ESP WebSocket Client.
 *
 * ESP WebSocket Client is a part of `esp-idf` version 4.x. After version 5,
 * it is a part of which is an `esp-idf` component.
 *
 * For `esp-idf` version 4.x, see the official documentation at:
 * https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/api-reference/protocols/esp_websocket_client.html
 *
 * For `esp-idf` version 5.x and newer, see the official documentation at:
 * https://espressif.github.io/esp-protocols/esp_websocket_client/index.html
 *
 * `esp_hass_client` needs uses two queues for communication; an event message
 * queue, and an command message queue. The client enqueues event messages from
 * Home Assistant to the event message queue, and command result messages to
 * the command message queue.
 *
 * To receive event messages, the client must send a subscribe command to
 * events.
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
	char *access_token; /*!< The access token */
	int timeout_sec;    /*!< Timeout in second when no response is back
				     from the server. */
	int command_send_timeout_sec; /*!< Timeout in second when a command is
					 sent */
	int result_recv_timeout_sec;  /*!< Timeout in second when a command is
					 sent, but no response is back from the
					 server */
	esp_websocket_client_config_t
	    *ws_config; /*!< configuration of esp_websocket_client */
	QueueHandle_t result_queue; /*!< queue handle for results */
	QueueHandle_t event_queue;  /*!< queue handle for events */
} esp_hass_config_t;

/**
 * A macro to initialize esp_hass_config_t with defaults.
 */
#define ESP_HASS_CONFIG_DEFAULT()                                              \
	{                                                                      \
		.access_token = NULL, .timeout_sec = 10, .ws_config = NULL,    \
		.result_queue = NULL, .event_queue = NULL,                     \
		.command_send_timeout_sec = 10, .result_recv_timeout_sec = 10, \
	}

/**
 * esp_hass_call_service configuration. The API accepts more options, but they
 * are not supported yet.
 */
typedef struct {
	char *domain;		    /*!< domain name */
	char *service;		    /*!< service name */
	char *entity_id;	    /*!< entity_id */
	TickType_t delay;	    /*!< timeout for receiving the result */
	QueueHandle_t result_queue; /*!< result queue */
} esp_hass_call_service_config_t;

/**
 * A macro to initialize esp_hass_call_service_config_t with defaults.
 */
#define ESP_HASS_CALL_SERVICE_CONFIG_DEFAULT()                      \
	{                                                           \
		.domain = NULL, .service = NULL, .entity_id = NULL, \
		.delay = portMAX_DELAY, .result_queue = NULL,       \
	}

/**
 * The esp_hass client handle
 */
typedef struct esp_hass_client *esp_hass_client_handle_t;

/**
 * @brief Initilize hass client. This function should be called before any
 * `esp_hass_*` function.
 *
 * @param[in] config Configuration.
 *
 * @return
 *	- pointer to hass client handle. This handle must be passed to other
 *	`esp_hass_*` functions.
 *	- NULL if failed.
 */
esp_hass_client_handle_t esp_hass_init(esp_hass_config_t *config);

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

/**
 * @brief Send a JSON message.
 *
 * @param[in] client The hass client
 * @param[in] json cJSON object
 *
 * @return
 * - ESP_OK if success
 */
esp_err_t esp_hass_send_message_json(esp_hass_client_handle_t client,
    cJSON *json);

/**
 * @brief Call a service
 *
 * @param[in] client The hass client
 * @param[in] config esp_hass_call_service_config_t
 *
 * @return
 * - ESP_OK if success
 */
esp_err_t esp_hass_call_service(esp_hass_client_handle_t client,
    esp_hass_call_service_config_t *config);

/**
 * @brief Register a message hander function.
 *
 * The message hander function is called with three arguments:
 *
 * message_handler(void *args, esp_event_base_t base, int32_t id, void
 * *event_data);
 *
 * `args` is esp_hass_client_handle_t. `base` is the base event, and `id` is
 * event id, and `event_data` is `esp_hass_message_t *`.
 *
 * The handler is called by `esp_hass_task_event_source` task, which keeps
 * feeding messages into the handler.
 *
 * @param[in] client The hass client.
 * @param[in] callback A callback function
 */
esp_err_t esp_hass_event_handler_register(esp_hass_client_handle_t client,
    esp_event_handler_t callback);

#ifdef __cplusplus
}
#endif

#endif
