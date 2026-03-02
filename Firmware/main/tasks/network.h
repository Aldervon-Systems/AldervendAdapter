#ifndef NETWORK_H
#define NETWORK_H

void network_init(void);

/** api_base and token from check-in */
void network_get_api_config(const char **out_api_base, const char **out_token);

/** Latest firmware info from check-in */
void network_get_firmware_info(const char **out_version, const char **out_url, const char **out_checksum);

#endif

