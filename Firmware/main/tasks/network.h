#ifndef NETWORK_H
#define NETWORK_H

void network_init(void);

/** Session-only api_base and token from check-in (this boot). Not persisted. */
void network_get_api_config(const char **out_api_base, const char **out_token);

#endif

