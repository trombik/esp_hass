
#if !defined(__EXAMPLE_CALL_SERVICE__H__)
#define __EXAMPLE_CALL_SERVICE__H__

#include <esp_err.h>
#include <esp_hass.h>

typedef struct {
	char *domain;
	char *service;
	char *entity_id;
} call_service_config_t;

esp_err_t call_service(esp_hass_client_handle_t client,
    call_service_config_t *config);

#endif
