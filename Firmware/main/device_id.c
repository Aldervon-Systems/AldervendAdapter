#include "device_id.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_random.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "device_id";

#define NVS_NAMESPACE "storage"
#define NVS_KEY_DEVICE_ID "device_id"
#define NVS_KEY_DEVICE_KEY "device_key"

/* Generate a URL-safe hex string ID (e.g. 12 bytes -> 24 hex chars). */
static void generate_id(char *out, size_t len)
{
    const char hex[] = "0123456789abcdef";
    uint8_t buf[DEVICE_ID_LEN / 2];
    esp_fill_random(buf, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf) && (i * 2 + 1) < len; i++) {
        out[i * 2]     = hex[buf[i] >> 4];
        out[i * 2 + 1] = hex[buf[i] & 0x0f];
    }
    out[len - 1] = '\0';
}

static void generate_key(char *out, size_t len)
{
    const char hex[] = "0123456789abcdef";
    uint8_t buf[DEVICE_KEY_LEN / 2];
    esp_fill_random(buf, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf) && (i * 2 + 1) < len; i++) {
        out[i * 2]     = hex[buf[i] >> 4];
        out[i * 2 + 1] = hex[buf[i] & 0x0f];
    }
    out[len - 1] = '\0';
}

bool device_id_get(char *id_buf)
{
    if (id_buf == NULL) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    size_t len = DEVICE_ID_LEN + 1;
    err = nvs_get_str(handle, NVS_KEY_DEVICE_ID, id_buf, &len);
    if (err == ESP_OK) {
        nvs_close(handle);
        ESP_LOGI(TAG, "device_id loaded: %s", id_buf);
        return true;
    }
    if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "nvs_get_str: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    generate_id(id_buf, DEVICE_ID_LEN + 1);
    err = nvs_set_str(handle, NVS_KEY_DEVICE_ID, id_buf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "device_id generated and stored: %s", id_buf);
    return true;
}

bool device_key_get(char *key_buf)
{
    if (key_buf == NULL) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    size_t len = DEVICE_KEY_LEN + 1;
    err = nvs_get_str(handle, NVS_KEY_DEVICE_KEY, key_buf, &len);
    if (err == ESP_OK) {
        nvs_close(handle);
        ESP_LOGI(TAG, "device_key loaded: %s", key_buf);
        return true;
    }
    if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "nvs_get_str (device_key): %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    generate_key(key_buf, DEVICE_KEY_LEN + 1);
    err = nvs_set_str(handle, NVS_KEY_DEVICE_KEY, key_buf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str (device_key): %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit (device_key): %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "device_key generated and stored: %s", key_buf);
    return true;
}
