#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_hass.h"

void
app_main(void)
{

	esp_hass_hello_world();
	while (1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
