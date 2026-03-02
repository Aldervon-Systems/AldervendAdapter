#ifndef APP_OTA_H
#define APP_OTA_H

#include <stdbool.h>

/**
 * Download firmware from url via esp_https_ota, write to next OTA partition,
 * set boot partition and restart. On success the device restarts (does not return).
 * expected_sha256_hex is unused; esp_https_ota performs its own image validation.
 */
bool ota_update_from_url(const char *url, const char *expected_sha256_hex);

#endif
