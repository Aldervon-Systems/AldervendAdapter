#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_random.h"

#include "cJSON.h"
#include "device_id.h"
#include "network.h"
#include "api.h"

static const char *TAG = "mdb";

#define BULK_PATH       "/bulk"
#define INTERVAL_MIN_MS 5000 // TODO: Remove me
#define INTERVAL_MAX_MS 90000 // TODO: Remove me
#define TEST_DATA       "Test Data..." // TODO: Remove me

static void mdb_task(void *arg)
{
    char device_id[DEVICE_ID_LEN + 1] = {0};
    device_id_get(device_id);

    while (1) {
        const char *api_base = NULL;
        const char *token = NULL;
        network_get_api_config(&api_base, &token);
        if (!api_base || !api_base[0]) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        char url[256];
        int n = snprintf(url, sizeof(url), "%s%s", api_base, BULK_PATH);
        if (n < 0 || n >= (int)sizeof(url)) {
            ESP_LOGW(TAG, "bulk URL truncated");
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        cJSON *root = cJSON_CreateObject();
        if (!root) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        cJSON_AddStringToObject(root, "device_id", device_id);
        cJSON_AddStringToObject(root, "data", TEST_DATA);
        char *body = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        if (!body) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        size_t body_len = strlen(body);
        bool ok = aldervon_http_post(url, token, body, body_len);
        free(body);
        if (ok) {
            ESP_LOGD(TAG, "bulk sent");
        } else {
            ESP_LOGW(TAG, "bulk POST failed");
        }

        uint32_t r;
        esp_fill_random(&r, sizeof(r));
        uint32_t ms = INTERVAL_MIN_MS + (r % (INTERVAL_MAX_MS - INTERVAL_MIN_MS + 1));
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}

void mdb_init(void)
{
    xTaskCreate(mdb_task, "mdb", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "MDB task started");
}
