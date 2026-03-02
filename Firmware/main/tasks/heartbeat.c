#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "boardPins.h"
#include "heartbeat.h"

static heartbeat_mode_t s_mode = HEARTBEAT_MODE_WIFI_CONFIG;

void heartbeat_set_mode(heartbeat_mode_t mode)
{
    s_mode = mode;
}

static void heartbeat_task(void *arg)
{
    gpio_reset_pin(STAT_LED);
    gpio_set_direction(STAT_LED, GPIO_MODE_OUTPUT);

    while (1) {
        heartbeat_mode_t mode = s_mode;
        TickType_t on_ms = 100;
        TickType_t off_ms = 100;

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

