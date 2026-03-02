#ifndef APP_OTA_H
#define APP_OTA_H

#include <stdbool.h>

/**
 * Download firmware from url, verify SHA256 hex checksum, write to next OTA
 * partition, set boot partition and restart. Returns true only after restart
 * (i.e. normally does not return on success). Returns false on failure.
 */
bool ota_update_from_url(const char *url, const char *expected_sha256_hex);

#endif
