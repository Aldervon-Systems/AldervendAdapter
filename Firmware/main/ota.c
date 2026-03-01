#include "ota.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "mbedtls/sha256.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "ota";

/* Compare two version strings "a.b.c"; returns <0 if v1<v2, 0 if equal, >0 if v1>v2. */
static int version_compare(const char *v1, const char *v2)
{
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;
    (void)sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1);
    (void)sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2);
    if (major1 != major2) return major1 - major2;
    if (minor1 != minor2) return minor1 - minor2;
    return patch1 - patch2;
}

static esp_err_t http_event(esp_http_client_event_t *evt)
{
    return ESP_OK;
}

/* GET url and return malloc'd body; size in out_size. Caller frees. */
static uint8_t *http_get(const char *url, size_t *out_size)
{
    *out_size = 0;
    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_event,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return NULL;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return NULL;
    }
    int64_t content_len = esp_http_client_fetch_headers(client);
    if (content_len <= 0 || content_len > 8192) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return NULL;
    }
    size_t len = (size_t)content_len;
    uint8_t *body = (uint8_t *)malloc(len + 1);
    if (!body) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return NULL;
    }
    int total = 0;
    while (total < (int)len) {
        int r = esp_http_client_read(client, (char *)body + total, (int)(len - total));
        if (r <= 0) break;
        total += r;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (total != (int)len) {
        free(body);
        return NULL;
    }
    body[total] = '\0';
    *out_size = (size_t)total;
    return body;
}

/* Stream GET url to OTA partition; compute SHA256 into sha256_out (32 bytes).
 * Caller must verify hash then call ota_commit_and_reboot() to finish. */
static esp_ota_handle_t s_ota_handle;
static const esp_partition_t *s_ota_partition;

static esp_err_t http_stream_to_ota(const char *url, uint8_t *sha256_out)
{
    s_ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!s_ota_partition) {
        ESP_LOGE(TAG, "no OTA partition");
        return ESP_ERR_NOT_FOUND;
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_event,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_ota_begin(s_ota_partition, OTA_SIZE_UNKNOWN, &s_ota_handle);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_ota_abort(s_ota_handle);
        esp_http_client_cleanup(client);
        mbedtls_sha256_free(&sha);
        return err;
    }

    uint8_t buf[1024];
    bool ok = true;
    while (ok) {
        int r = esp_http_client_read(client, (char *)buf, sizeof(buf));
        if (r < 0) { ok = false; break; }
        if (r == 0) break;
        mbedtls_sha256_update(&sha, buf, (size_t)r);
        err = esp_ota_write(s_ota_handle, buf, (size_t)r);
        if (err != ESP_OK) { ok = false; break; }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (!ok) {
        esp_ota_abort(s_ota_handle);
        mbedtls_sha256_free(&sha);
        return ESP_FAIL;
    }

    mbedtls_sha256_finish(&sha, sha256_out);
    mbedtls_sha256_free(&sha);
    return ESP_OK;
}

static void ota_commit_and_reboot(void)
{
    esp_err_t err = esp_ota_end(s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
        return;
    }
    err = esp_ota_set_boot_partition(s_ota_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "OTA committed, rebooting");
    esp_restart();
}

bool ota_check_and_update(const char *version_url)
{
    if (version_url == NULL) return false;

    size_t len = 0;
    uint8_t *body = http_get(version_url, &len);
    if (!body || len == 0) {
        ESP_LOGE(TAG, "failed to fetch version");
        free(body);
        return false;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)body, len);
    free(body);
    if (!root) {
        ESP_LOGE(TAG, "invalid JSON");
        return false;
    }

    cJSON *version = cJSON_GetObjectItem(root, "version");
    cJSON *url = cJSON_GetObjectItem(root, "url");
    cJSON *sha256_str = cJSON_GetObjectItem(root, "sha256");
    if (!cJSON_IsString(version) || !cJSON_IsString(url) || !cJSON_IsString(sha256_str)) {
        ESP_LOGE(TAG, "version/url/sha256 missing or not string");
        cJSON_Delete(root);
        return false;
    }

    if (version_compare(version->valuestring, FIRMWARE_VERSION) <= 0) {
        ESP_LOGI(TAG, "current %s >= remote %s, no update", FIRMWARE_VERSION, version->valuestring);
        cJSON_Delete(root);
        return true;
    }

    ESP_LOGI(TAG, "update available: %s -> %s", FIRMWARE_VERSION, version->valuestring);

    const char *expected_sha = sha256_str->valuestring;
    if (strlen(expected_sha) != 64) {
        ESP_LOGE(TAG, "sha256 must be 64 hex chars");
        cJSON_Delete(root);
        return false;
    }

    uint8_t computed[32];
    esp_err_t err = http_stream_to_ota(url->valuestring, computed);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        return false;
    }

    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", computed[i]);
    hex[64] = '\0';
    if (strcasecmp(hex, expected_sha) != 0) {
        ESP_LOGE(TAG, "SHA256 mismatch");
        esp_ota_abort(s_ota_handle);
        cJSON_Delete(root);
        return false;
    }
    cJSON_Delete(root);
    ota_commit_and_reboot();
    return true;
}
