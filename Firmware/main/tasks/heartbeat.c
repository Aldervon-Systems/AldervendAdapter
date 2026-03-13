#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"

#include "boardPins.h"
#include "heartbeat.h"

static const char *TAG = "heartbeat";

static heartbeat_mode_t s_mode = HEARTBEAT_MODE_WIFI_CONFIG;

void heartbeat_set_mode(heartbeat_mode_t mode)
{
    s_mode = mode;
}

static void wifi_factory_reset(void)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "wifi_factory_reset: failed to open NVS namespace 'wifi'");
        return;
    }
    nvs_erase_all(nvs);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGW(TAG, "WiFi config erased, rebooting");
    esp_restart();
}

static void heartbeat_task(void *arg)
{
    gpio_reset_pin(STAT_LED);
    gpio_set_direction(STAT_LED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(BOOT_BTN);
    gpio_set_direction(BOOT_BTN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOOT_BTN, GPIO_PULLUP_ONLY);

    const TickType_t hold_ticks = pdMS_TO_TICKS(10000); /* 10s */
    TickType_t press_start = 0;

    while (1) {
        heartbeat_mode_t mode = s_mode;
        TickType_t on_ms = 100;
        TickType_t off_ms = 100;

        /* Base heartbeat pattern by mode. */
        switch (mode) {
        case HEARTBEAT_MODE_WIFI_CONFIG:
            on_ms = 100;
            off_ms = 100;
            break;
        case HEARTBEAT_MODE_NORMAL:
            on_ms = 100;
            off_ms = 1400;
            break;
        case HEARTBEAT_MODE_WORKING:
            on_ms = 200;
            off_ms = 200;
            break;
        default:
            on_ms = 500;
            off_ms = 500;
            break;
        }

        /* BOOT button long-press: animate faster towards reset and wipe WiFi config. */
        int pressed = (gpio_get_level(BOOT_BTN) == 0); /* active-low */
        TickType_t now = xTaskGetTickCount();
        if (pressed) {
            if (press_start == 0) {
                press_start = now;
            } else if ((now - press_start) >= hold_ticks) {
                wifi_factory_reset();
            }

            TickType_t elapsed = press_start ? (now - press_start) : 0;
            uint32_t ms = elapsed * portTICK_PERIOD_MS;

            if (ms < 3000) {
                on_ms = 200;
                off_ms = 200;
            } else if (ms < 7000) {
                on_ms = 100;
                off_ms = 100;
            } else {
                on_ms = 50;
                off_ms = 50;
            }
        } else {
            press_start = 0;
        }

        gpio_set_level(STAT_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        gpio_set_level(STAT_LED, 0);
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}

void heartbeat_init(void)
{
    xTaskCreate(heartbeat_task, "heartbeat", 2048, NULL, 2, NULL);
}

