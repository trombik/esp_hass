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
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>
#include <esp_websocket_client.h>

typedef struct {
	const char *uri;
	const char *access_token;
	const int timeout_sec;

} esp_hass_config_t;

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
 * @brief Perform authentication. See:
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

#ifdef __cplusplus
}
#endif
