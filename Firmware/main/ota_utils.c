#include "ota_utils.h"
#include "aldervon_cert.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ota_utils";

bool ota_parse_version(const char *s, int *major, int *minor, int *commit)
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

bool ota_version_older_than(const char *our, const char *latest)
{
    int our_major, our_minor, our_commit;
    int lat_major, lat_minor, lat_commit;
    if (!ota_parse_version(our, &our_major, &our_minor, &our_commit)) return false;
    if (!ota_parse_version(latest, &lat_major, &lat_minor, &lat_commit)) return false;
    if (our_major != lat_major) return our_major < lat_major;
    if (our_minor != lat_minor) return our_minor < lat_minor;
    return our_commit < lat_commit;
}

bool ota_update_from_url(const char *url, const char *expected_sha256_hex)
{
    (void)expected_sha256_hex;

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
