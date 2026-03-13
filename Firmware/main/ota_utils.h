#ifndef OTA_UTILS_H
#define OTA_UTILS_H

#include <stdbool.h>

/**
 * Parse "vMAJOR.MINOR-COMMIT-..." into major, minor, commit.
 * Returns true on success.
 */
bool ota_parse_version(const char *s, int *major, int *minor, int *commit);

/**
 * True if our version string is strictly older than latest (by major, minor, commit).
 */
bool ota_version_older_than(const char *our, const char *latest);

/**
 * Download firmware from url via esp_https_ota, write to next OTA partition,
 * set boot partition and restart. On success the device restarts (does not return).
 * expected_sha256_hex is unused; esp_https_ota performs its own image validation.
 */
bool ota_update_from_url(const char *url, const char *expected_sha256_hex);

#endif
