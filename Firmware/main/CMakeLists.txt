idf_component_register(
    SRCS "main.c" "device_id.c" "ota.c" "tasks/network.c" "tasks/heartbeat.c"
    INCLUDE_DIRS "." "tasks"
    REQUIRES nvs_flash esp_http_client esp_wifi esp_netif driver
)
