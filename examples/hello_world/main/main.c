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

#include <esp_crt_bundle.h>
#include <esp_event.h>
#include <esp_hass.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <stdio.h>

#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define EXAMPLE_ESP_MAXIMUM_RETRY (5)
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

static const char *TAG = "example";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void
event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
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
	    ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
	    IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

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
	 * re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see
	 * above) */
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

void
app_main(void)
{
	esp_err_t err = ESP_FAIL;
	esp_hass_client_handle_t client = NULL;
	esp_websocket_client_config_t ws_config = { 0 };

	/* increase log level in esp_hass component only for debugging */
	esp_log_level_set("esp_hass", ESP_LOG_DEBUG);
	ws_config.uri = CONFIG_HASS_URI;

/* version 5.x uses my fork with a fix to crt_bundle_attach, but older
 * versions do not have the fix.
 * see https://github.com/espressif/esp-protocols/pull/49
 *
 * this means the example does not work with older esp-idf versions.
 */
#if IDF_VERSION_MAJOR >= 5
	/* use default CA bundle */
	ws_config.crt_bundle_attach = esp_crt_bundle_attach;
#endif

	/* double the default task_stack size to enable debug log */
	ws_config.task_stack = (4 * 1024) * 2;

#if IDF_VERSION_MAJOR >= 5
	/* set timeouts here to surpress warnings from WEBSOCKET_CLIENT */
	ws_config.reconnect_timeout_ms = 10000;
	ws_config.network_timeout_ms = 10000;
#endif

	esp_hass_config_t config = {
		.access_token = CONFIG_HASS_ACCESS_TOKEN,
		.timeout_sec = 30,
		.ws_config = &ws_config,
	};

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

	ESP_LOGI(TAG, "Starting loop");
	while (1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

	ESP_LOGI(TAG, "Stopping hass client");
	err = esp_hass_client_stop(client);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_client_stop(): %s",
		    esp_err_to_name(err));
	}

start_fail:
	ESP_LOGI(TAG, "Destroying hass client");
	err = esp_hass_destroy(client);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_destroy(): %s", esp_err_to_name(err));
	}

init_fail:
	ESP_LOGI(TAG, "The example terminated with an error. Please reboot.");
	while (1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
