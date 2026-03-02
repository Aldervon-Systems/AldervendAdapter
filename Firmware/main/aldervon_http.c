#include "esp_http_client.h"
#include "esp_log.h"
#include "aldervon_cert.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static esp_err_t http_event(esp_http_client_event_t *evt)
{
    return ESP_OK;
}

static uint8_t *http_get(const char *url, size_t *out_size)
{
    *out_size = 0;
    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_event,
        .cert_pem = aldervon_root_ca_pem,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return NULL;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return NULL;
    }
    int64_t content_len = esp_http_client_fetch_headers(client);
    if (content_len <= 0 || content_len > 8192) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return NULL;
    }
    size_t len = (size_t)content_len;
    uint8_t *body = (uint8_t *)malloc(len + 1);
    if (!body) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return NULL;
    }
    int total = 0;
    while (total < (int)len) {
        int r = esp_http_client_read(client, (char *)body + total, (int)(len - total));
        if (r <= 0) break;
        total += r;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (total != (int)len) {
        free(body);
        return NULL;
    }
    body[total] = '\0';
    *out_size = (size_t)total;
    return body;
}

/* Simple HTTPS GET helper for aldervon APIs. */
bool aldervon_http_get(const char *url, uint8_t **out_body, size_t *out_size)
{
    if (!url || !out_body || !out_size) {
        return false;
    }
    *out_body = http_get(url, out_size);
    return *out_body != NULL && *out_size > 0;
}

/* POST JSON to url with optional Bearer token. Returns true if status 2xx. */
bool aldervon_http_post(const char *url, const char *bearer_token, const char *body, size_t body_len)
{
    if (!url || !body) return false;

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event,
        .cert_pem = aldervon_root_ca_pem,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;

    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (bearer_token && bearer_token[0]) {
        char auth[128];
        snprintf(auth, sizeof(auth), "Bearer %s", bearer_token);
        esp_http_client_set_header(client, "Authorization", auth);
    }
    esp_http_client_set_post_field(client, body, body_len);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    return (err == ESP_OK && status >= 200 && status < 300);
}
