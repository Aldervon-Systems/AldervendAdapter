#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"

#include "lwip/sockets.h"

#include "network.h"
#include "heartbeat.h"
#include "device_id.h"

static const char *TAG = "network";

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_event_group;

static bool load_wifi_config(char *ssid, size_t ssid_len, char *password, size_t pass_len)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    size_t s_len = ssid_len;
    size_t p_len = pass_len;
    esp_err_t err1 = nvs_get_str(nvs, "ssid", ssid, &s_len);
    esp_err_t err2 = nvs_get_str(nvs, "pass", password, &p_len);
    nvs_close(nvs);
    if (err1 != ESP_OK || err2 != ESP_OK) {
        return false;
    }
    return s_len > 0;
}

static void save_wifi_config(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "pass", password);
    nvs_commit(nvs);
    nvs_close(nvs);
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WiFi STA start, connecting…");
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t *e = (wifi_event_sta_disconnected_t *)data;
            ESP_LOGW(TAG, "WiFi disconnected, reason=%d", e ? e->reason : -1);
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(const char *ssid, const char *password)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (netif) {
        ESP_ERROR_CHECK(esp_netif_set_hostname(netif, "aldervend"));
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t any_id, got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                    &wifi_event_handler, NULL, &any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler, NULL, &got_ip));

    wifi_config_t wifi_config = { 0 };
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta done, connecting to %s", ssid);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    char device_id[DEVICE_ID_LEN + 1] = {0};
    device_id_get(device_id);

    const char *suffix = device_id;
    size_t len = strlen(device_id);
    if (len > 6) {
        suffix = device_id + (len - 6);
    }

    const char *revision =
#ifdef REVISION
        REVISION;
#else
        "unknown";
#endif

    const char *hash =
#ifdef HASH
        HASH;
#else
        "unknown";
#endif

    char html[1024];
    int n = snprintf(
        html, sizeof(html),
        "<!DOCTYPE html>"
        "<html><head><meta charset='utf-8'><title>Aldervend WiFi</title>"
        "<style>"
        "body{font-family:sans-serif;background:#f5f5f5;margin:0;padding:0;}"
        ".card{max-width:420px;margin:40px auto;background:#fff;border-radius:8px;"
        "box-shadow:0 2px 6px rgba(0,0,0,0.15);padding:20px 24px;}"
        "h1{font-size:20px;margin-top:0;margin-bottom:8px;}"
        "p.meta{font-size:12px;color:#555;margin:4px 0;}"
        "code{font-family:monospace;background:#f0f0f0;padding:2px 4px;border-radius:3px;}"
        "label{display:block;margin-top:12px;font-size:14px;}"
        "input{width:100%%;padding:6px 8px;margin-top:4px;box-sizing:border-box;}"
        "button{margin-top:16px;padding:8px 12px;border:none;border-radius:4px;"
        "background:#0069d9;color:#fff;font-weight:600;cursor:pointer;}"
        "button:hover{background:#0053b3;}"
        "a{color:#0069d9;text-decoration:none;font-size:12px;}"
        "a:hover{text-decoration:underline;}"
        "</style></head>"
        "<body><div class='card'>"
        "<h1>Aldervend Adapter WiFi</h1>"
        "<p class='meta'>Device ID: <code>%s</code></p>"
        "<p class='meta'>Setup SSID: <code>Aldervend-%s</code></p>"
        "<p class='meta'>Revision: <code>%s</code></p>"
        "<p class='meta'>Build: <a href='https://github.com/Aldervon-Systems/AldervendAdapter/commit/%s' target='_blank'>%s</a></p>"
        "<form method='POST' action='/save'>"
        "<label>WiFi SSID<input name='ssid'></label>"
        "<label>WiFi Password<input type='password' name='pass'></label>"
        "<button type='submit'>Save &amp; Reboot</button>"
        "</form>"
        "</div></body></html>",
        device_id,
        suffix,
        revision,
        hash,
        hash
    );
    if (n < 0 || n >= (int)sizeof(html)) {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, n);
}

static void url_decode(char *s)
{
    char *o = s;
    while (*s) {
        if (*s == '+') {
            *o++ = ' ';
            s++;
        } else if (*s == '%' && s[1] && s[2]) {
            int v = 0;
            for (int i = 1; i <= 2; i++) {
                char c = s[i];
                if (c >= '0' && c <= '9') v = (v << 4) | (c - '0');
                else if (c >= 'A' && c <= 'F') v = (v << 4) | (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f') v = (v << 4) | (c - 'a' + 10);
            }
            *o++ = (char)v;
            s += 3;
        } else {
            *o++ = *s++;
        }
    }
    *o = '\0';
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return httpd_resp_send_500(req);
    }
    buf[len] = '\0';

    char ssid[33] = {0};
    char pass[65] = {0};

    char *ssid_p = strstr(buf, "ssid=");
    char *pass_p = strstr(buf, "pass=");
    if (ssid_p) {
        ssid_p += 5;
        char *end = strchr(ssid_p, '&');
        size_t l = end ? (size_t)(end - ssid_p) : strlen(ssid_p);
        if (l >= sizeof(ssid)) l = sizeof(ssid) - 1;
        memcpy(ssid, ssid_p, l);
        ssid[l] = '\0';
        url_decode(ssid);
    }
    if (pass_p) {
        pass_p += 5;
        char *end = strchr(pass_p, '&');
        size_t l = end ? (size_t)(end - pass_p) : strlen(pass_p);
        if (l >= sizeof(pass)) l = sizeof(pass) - 1;
        memcpy(pass, pass_p, l);
        pass[l] = '\0';
        url_decode(pass);
    }

    if (ssid[0] == '\0') {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    save_wifi_config(ssid, pass);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, "Saved. Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

static void dns_captive_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        vTaskDelete(NULL);
        return;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    uint8_t buf[512];
    while (1) {
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&from, &fromlen);
        if (len < 12) {
            continue;
        }
        uint8_t resp[512];
        memcpy(resp, buf, len);
        resp[2] = 0x81;
        resp[3] = 0x80;
        resp[4] = buf[4];
        resp[5] = buf[5];
        resp[6] = 0x00;
        resp[7] = 0x01;
        resp[8] = resp[9] = resp[10] = resp[11] = 0x00;

        int i = 12;
        while (i < len && resp[i] != 0) {
            i += resp[i] + 1;
        }
        if (i + 5 >= len) {
            continue;
        }
        i += 5;
        int qlen = i;

        int p = qlen;
        resp[p++] = 0xC0;
        resp[p++] = 0x0C;
        resp[p++] = 0x00;
        resp[p++] = 0x01;
        resp[p++] = 0x00;
        resp[p++] = 0x01;
        resp[p++] = 0x00;
        resp[p++] = 0x00;
        resp[p++] = 0x00;
        resp[p++] = 0x3C;
        resp[p++] = 0x00;
        resp[p++] = 0x04;

        resp[p++] = 192;
        resp[p++] = 168;
        resp[p++] = 4;
        resp[p++] = 1;

        sendto(sock, resp, p, 0, (struct sockaddr *)&from, fromlen);
    }
}

static void start_config_portal(void)
{
    ESP_LOGI(TAG, "Starting WiFi config portal (AP + HTTP)...");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    (void)ap_netif;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    char device_id[DEVICE_ID_LEN + 1] = {0};
    device_id_get(device_id);
    const char *suffix = device_id;
    size_t len = strlen(device_id);
    if (len > 6) {
        suffix = device_id + (len - 6);
    }

    wifi_config_t ap_cfg = (wifi_config_t){ 0 };
    snprintf((char *)ap_cfg.ap.ssid, sizeof(ap_cfg.ap.ssid), "Aldervend-%s", suffix);
    ap_cfg.ap.ssid_len = strlen((char *)ap_cfg.ap.ssid);
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    xTaskCreate(dns_captive_task, "dns_captive", 2048, NULL, 3, NULL);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL
        };
        httpd_uri_t save = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = save_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &save);
    }
}

void network_init(void)
{
    char cfg_ssid[33] = {0};
    char cfg_pass[65] = {0};
    bool have_nvs_wifi = load_wifi_config(cfg_ssid, sizeof(cfg_ssid), cfg_pass, sizeof(cfg_pass));

    const char *ssid = NULL;
    const char *pass = NULL;

    if (have_nvs_wifi) {
        ssid = cfg_ssid;
        pass = cfg_pass;
        ESP_LOGI(TAG, "Using WiFi from NVS: %s", ssid);
    } else if (strlen(CONFIG_WIFI_SSID) > 0) {
        ssid = CONFIG_WIFI_SSID;
        pass = CONFIG_WIFI_PASSWORD;
        ESP_LOGI(TAG, "Using WiFi from menuconfig: %s", ssid);
    }

    if (ssid && ssid[0] != '\0') {
        wifi_init_sta(ssid, pass ? pass : "");
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                              false, false, pdMS_TO_TICKS(15000));
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi connected, ready for API traffic");
            heartbeat_set_mode(HEARTBEAT_MODE_NORMAL);
        } else {
            ESP_LOGW(TAG, "WiFi not connected");
        }
    } else {
        ESP_LOGI(TAG, "No WiFi configured; starting config portal");
        start_config_portal();
        heartbeat_set_mode(HEARTBEAT_MODE_WIFI_CONFIG);
    }
}

