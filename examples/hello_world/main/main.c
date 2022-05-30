#include <stdio.h>

#include <esp_hass.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void
app_main(void)
{

    esp_hass_hello_world();
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
