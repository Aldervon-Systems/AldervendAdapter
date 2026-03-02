#ifndef API_H
#define API_H

#include <stdbool.h>
#include <stddef.h>

bool aldervon_http_get(const char *url, uint8_t **out_body, size_t *out_size);

#endif

