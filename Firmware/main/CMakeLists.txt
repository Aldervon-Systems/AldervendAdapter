idf_component_register(
    SRCS "main.c" "device_id.c" "aldervon_http.c" "app_ota.c" "tasks/network.c" "tasks/heartbeat.c" "tasks/mdb.c"
    INCLUDE_DIRS "." "tasks"
    REQUIRES nvs_flash esp_http_client esp_wifi esp_netif driver app_update mbedtls
)
