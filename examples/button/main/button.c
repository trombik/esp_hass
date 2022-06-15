#include <esp_err.h>
#include <esp_hass.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iot_button.h>

#define BUTTON_GPIO_NUM CONFIG_EXAMPLE_BUTTON_GPIO_NUM
#define TASK_STACK_SIZE (1024 * 2)
#define RESULT_DELAY_SEC (10)

static char *TAG = "button";
esp_hass_client_handle_t client;

button_handle_t gpio_btn;
button_config_t gpio_btn_cfg = {
	    .type = BUTTON_TYPE_GPIO,
	    .gpio_button_config = {
		.gpio_num = BUTTON_GPIO_NUM,
		.active_level = 0,
	    },
	};
esp_hass_call_service_config_t call_service_config =
    ESP_HASS_CALL_SERVICE_CONFIG_DEFAULT();

static void
button_single_click_cb(void *arg)
{
	ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
	esp_hass_call_service(client, &call_service_config);
}

static void
task_button(void *args)
{
	char *entity_id = CONFIG_EXAMPLE_CALL_SERVICE_ENTITI_ID;
	char *domain = CONFIG_EXAMPLE_CALL_SERVICE_DOMAIN;
	char *service = CONFIG_EXAMPLE_CALL_SERVICE;

	client = (esp_hass_client_handle_t)args;
	call_service_config.domain = domain;
	call_service_config.service = service;
	call_service_config.entity_id = entity_id;
	call_service_config.delay = (RESULT_DELAY_SEC * 1000 /
	    portTICK_PERIOD_MS);
	gpio_btn = iot_button_create(&gpio_btn_cfg);
	if (NULL == gpio_btn) {
		ESP_LOGE(TAG, "Button create failed");
		goto fail;
	}
	iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK,
	    button_single_click_cb);

	ESP_LOGI(TAG, "Starting loop");
	for (;;) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
fail:
	vTaskDelete(NULL);
}

esp_err_t
task_button_start(esp_hass_client_handle_t client)
{
	esp_err_t err = ESP_FAIL;

	if (xTaskCreate(task_button, "button", TASK_STACK_SIZE, client, 1,
		NULL) != pdPASS) {
		ESP_LOGE(TAG, "xTaskCreate");
		err = ESP_FAIL;
		goto fail;
	}
	err = ESP_OK;

fail:
	return err;
}
