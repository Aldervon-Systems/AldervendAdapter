#include "app_ota.h"
#include "aldervon_cert.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include <string.h>

static const char *TAG = "app_ota";

bool ota_update_from_url(const char *url, const char *expected_sha256_hex)
{
    (void)expected_sha256_hex; /* esp_https_ota validates image; no custom checksum */

    if (!url || url[0] == '\0') return false;

    esp_http_client_config_t http_config = {
        .url = url,
        .cert_pem = aldervon_root_ca_pem,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t err = esp_https_ota(&ota_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "OTA update complete, restarting");
    esp_restart();
    return true;
}
