#ifndef API_H
#define API_H

#include <stdbool.h>
#include <stddef.h>

bool aldervon_http_get(const char *url, uint8_t **out_body, size_t *out_size);

/** POST body to url with optional Bearer token. Returns true if status 2xx. */
bool aldervon_http_post(const char *url, const char *bearer_token, const char *body, size_t body_len);

#endif

