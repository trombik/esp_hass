#include <cJSON.h>
#include <esp_hass.h>

static const char *TAG = "main/call_service";

static cJSON *
create_call_service_json(call_service_config_t *config)
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
call_service(esp_hass_client_handle_t client, call_service_config_t *config,
    QueueHandle_t result_queue, TickType_t delay)
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
	if (xQueueReceive(result_queue, &msg, delay) != pdTRUE) {
		ESP_LOGE(TAG, "failed to receive result: timeout");
	}
	if (msg->type != HASS_MESSAGE_TYPE_RESULT) {
		ESP_LOGE(TAG,
		    "Unexpected response from the server: result type: %d",
		    msg->type);
		goto fail;
	}
	if (msg->success) {
		ESP_LOGI(TAG, "calling %s on %s successful",
		    CONFIG_EXAMPLE_CALL_SERVICE,
		    CONFIG_EXAMPLE_CALL_SERVICE_ENTITI_ID);
	} else {
		ESP_LOGE(TAG, "server returned failure");
		json_error = cJSON_GetObjectItemCaseSensitive(msg->json,
		    "error");
		json_error_msg = cJSON_GetObjectItemCaseSensitive(json_error,
		    "message");
		if (cJSON_IsString(json_error_msg) &&
		    (error_message->valuestring != NULL)) {
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
	return err;
}
