#include "app_ota.h"
#include "aldervon_cert.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_system.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "mbedtls/sha256.h"

static const char *TAG = "app_ota";

#define OTA_BUF_SIZE 2048

static void hex_to_bin(const char *hex, uint8_t *bin, size_t bin_len)
{
    for (size_t i = 0; i < bin_len && hex[i * 2] && hex[i * 2 + 1]; i++) {
        char a = hex[i * 2];
        char b = hex[i * 2 + 1];
        uint8_t va = (a >= 'a') ? (a - 'a' + 10) : (a >= 'A') ? (a - 'A' + 10) : (a - '0');
        uint8_t vb = (b >= 'a') ? (b - 'a' + 10) : (b >= 'A') ? (b - 'A' + 10) : (b - '0');
        bin[i] = (va << 4) | vb;
    }
}

bool ota_update_from_url(const char *url, const char *expected_sha256_hex)
{
    if (!url || url[0] == '\0') return false;

    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (!update_part) {
        ESP_LOGE(TAG, "no OTA update partition");
        return false;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_part, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return false;
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .cert_pem = aldervon_root_ca_pem,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        esp_ota_abort(ota_handle);
        return false;
    }

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed");
        esp_http_client_cleanup(client);
        esp_ota_abort(ota_handle);
        return false;
    }

    int64_t content_len = esp_http_client_fetch_headers(client);
    if (content_len <= 0) {
        ESP_LOGE(TAG, "invalid content length");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        esp_ota_abort(ota_handle);
        return false;
    }

    uint8_t buf[OTA_BUF_SIZE];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0); /* 0 = SHA-256, not 224 */

    int total_written = 0;
    while (1) {
        int r = esp_http_client_read(client, (char *)buf, sizeof(buf));
        if (r < 0) break;
        if (r == 0) break;
        mbedtls_sha256_update(&sha, buf, (size_t)r);
        err = esp_ota_write(ota_handle, buf, (size_t)r);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed");
            break;
        }
        total_written += r;
    }

    uint8_t hash[32];
    mbedtls_sha256_finish(&sha, hash);
    mbedtls_sha256_free(&sha);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        esp_ota_abort(ota_handle);
        return false;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        return false;
    }

    if (expected_sha256_hex && strlen(expected_sha256_hex) >= 64) {
        uint8_t expected_bin[32];
        hex_to_bin(expected_sha256_hex, expected_bin, 32);
        if (memcmp(hash, expected_bin, 32) != 0) {
            ESP_LOGE(TAG, "firmware checksum mismatch");
            return false;
        }
    }

    err = esp_ota_set_boot_partition(update_part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "OTA update complete, restarting");
    esp_restart();
    return true;
}
