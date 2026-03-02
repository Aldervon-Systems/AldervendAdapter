/**
 * @file main.c
 * @brief AldervendAdapter firmware - ESP32-C6 Mini, ESP-IDF, OTA framework.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "device_id.h"
#include "boardPins.h"
#include "tasks/network.h"
#include "tasks/heartbeat.h"

static const char *TAG = "main";

void app_main(void)
{
    for (int i = 0; i < 10; i++) {
        gpio_set_level(STAT_LED, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(STAT_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Aldervend Adapter %s", REVISION);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    char device_id[DEVICE_ID_LEN + 1];
    if (!device_id_get(device_id)) {
        ESP_LOGE(TAG, "device_id init failed");
    }

    network_init();
    heartbeat_init();

    vTaskDelay(portMAX_DELAY);
}
