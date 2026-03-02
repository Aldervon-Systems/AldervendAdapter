/**
 * @file main.c
 * @brief AldervendAdapter firmware - ESP32-C6 Mini, ESP-IDF, OTA framework.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "tasks/mdb.h"
#include "app_ota.h"

static const char *TAG = "main";

#define POST_INIT_STACK 8192

/* Parse "vMAJOR.MINOR-COMMIT-..." into major, minor, commit. Returns true on success. */
static bool parse_version(const char *s, int *major, int *minor, int *commit)
{
    if (!s || s[0] != 'v') return false;
    char *end;
    long m = strtol(s + 1, &end, 10);
    if (end == s + 1 || *end != '.') return false;
    long mi = strtol(end + 1, &end, 10);
    if (end == s + 1 || *end != '-') return false;
    long c = strtol(end + 1, &end, 10);
    if (end == s + 1) return false;
    *major = (int)m;
    *minor = (int)mi;
    *commit = (int)c;
    return true;
}

/* True if (our major, minor, commit) is strictly older than latest. */
static bool version_older_than(const char *our, const char *latest)
{
    int our_major, our_minor, our_commit;
    int lat_major, lat_minor, lat_commit;
    if (!parse_version(our, &our_major, &our_minor, &our_commit)) return false;
    if (!parse_version(latest, &lat_major, &lat_minor, &lat_commit)) return false;
    if (our_major != lat_major) return our_major < lat_major;
    if (our_minor != lat_minor) return our_minor < lat_minor;
    return our_commit < lat_commit;
}

static void post_init_task(void *arg)
{
    const char *fw_version = NULL;
    const char *fw_url = NULL;
    const char *fw_checksum = NULL;
    network_get_firmware_info(&fw_version, &fw_url, &fw_checksum);
    if (fw_version && fw_version[0] != '\0' && fw_url && fw_url[0] != '\0') {
#ifdef REVISION
        if (version_older_than(REVISION, fw_version)) {
            ESP_LOGI(TAG, "revision %s older than latest %s, attempting OTA", REVISION, fw_version);
            if (!ota_update_from_url(fw_url, fw_checksum)) {
                ESP_LOGW(TAG, "OTA update failed, continuing with current firmware");
            }
        }
#else
        (void)fw_version;
#endif
    }
    mdb_init();
    vTaskDelay(portMAX_DELAY);
}

void app_main(void)
{
    gpio_reset_pin(STAT_LED);
    gpio_set_direction(STAT_LED, GPIO_MODE_OUTPUT);

    for (int i = 0; i < 10; i++) {
        gpio_set_level(STAT_LED, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(STAT_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Aldervend Adapter %s", REVISION);

    // Initialize NVS
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

    heartbeat_init();
    network_init();

    /* Version check, OTA, and MDB run in a task with larger stack (OTA uses ~2KB buffer + HTTP/SHA). */
    xTaskCreate(post_init_task, "post_init", POST_INIT_STACK, NULL, 5, NULL);
    vTaskDelay(portMAX_DELAY);
}
