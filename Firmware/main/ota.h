#ifndef OTA_H
#define OTA_H

#include <stdbool.h>

/** Current firmware version (must match version in version.json for no-update). */
#define FIRMWARE_VERSION "1.0.0"

/**
 * Check version at base_url (e.g. https://api.aldervon.com/device/version?id=xyz).
 * If remote version > FIRMWARE_VERSION: download firmware from url in JSON,
 * verify SHA256, and install (reboot into new image).
 * Returns true if no update needed or update applied; false on error.
 */
bool ota_check_and_update(const char *version_url);

#endif
