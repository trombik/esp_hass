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

#if !defined __PARSER__H__
#define __PARSER__H__

#include "esp_hass.h"

/**
 * @brief Parse string as JSON. The returned pointer must be destroyed with
 * `esp_hass_message_destroy`.
 *
 * @param[in] data JSON string.
 * @param[in] data_len length of `data`.
 *
 * @return
 *  - Pointer to `esp_hass_message_t` if successfully parsed, or NULL.
 */
esp_hass_message_t *esp_hass_message_parse(char *data, int data_len);

/**
 * @brief Destroy `esp_hass_message_t`, free() the memory allocated by
 * `esp_hass_message_parse()`. Must be called by the caller of
 * `esp_hass_message_parse()`.
 *
 * @return
 * - ESP_OK if success
 */
esp_err_t esp_hass_message_destroy(esp_hass_message_t *msg);

#endif