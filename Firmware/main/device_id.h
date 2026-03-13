#ifndef DEVICE_ID_H
#define DEVICE_ID_H

#include <stdint.h>
#include <stdbool.h>

#define DEVICE_ID_LEN 24
#define DEVICE_KEY_LEN 64

/**
 * Get device ID from NVS, or generate and store a new random one.
 * id_buf must be at least DEVICE_ID_LEN + 1 bytes (null-terminated).
 * Returns true if id was loaded/created and written to id_buf.
 */
bool device_id_get(char *id_buf);

/**
 * Get device key from NVS, or generate and store a new random one.
 * key_buf must be at least DEVICE_KEY_LEN + 1 bytes (null-terminated).
 * Returns true if key was loaded/created and written to key_buf.
 */
bool device_key_get(char *key_buf);

#endif
