/**
 * @file main.c
 * @brief AldervendAdapter firmware - ESP32-C6 Mini, ESP-IDF, OTA framework.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "driver/gpio.h"

#include "device_id.h"
#include "ota.h"

static const char *TAG = "main";

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t s_wifi_event_group;
static int s_ota_done;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t any_id, got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                    &wifi_event_handler, NULL, &any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler, NULL, &got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta done, connecting to %s", CONFIG_WIFI_SSID);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Aldervend Adapter v%s", FIRMWARE_VERSION);

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

    s_ota_done = 0;
    if (strlen(CONFIG_WIFI_SSID) > 0) {
        wifi_init_sta();
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                              false, false, pdMS_TO_TICKS(15000));
        if (bits & WIFI_CONNECTED_BIT) {
            char version_url[192];
            snprintf(version_url, sizeof(version_url), "%s?id=%s",
                     CONFIG_VERSION_BASE_URL, device_id);
            if (ota_check_and_update(version_url)) {
                s_ota_done = 1;
            }
        } else {
            ESP_LOGW(TAG, "WiFi not connected, skipping OTA");
        }
    } else {
        ESP_LOGI(TAG, "No WiFi SSID set, skipping OTA (set in menuconfig)");
    }

    /* App loop: simple LED blink (GPIO 8 = RGB LED on C6 Mini). */
    const int STAT_LED = 8;
    gpio_reset_pin(STAT_LED);
    gpio_set_direction(STAT_LED, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(STAT_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(STAT_LED, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
