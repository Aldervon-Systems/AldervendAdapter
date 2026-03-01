idf_component_register(
    SRCS "main.c" "device_id.c" "ota.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_http_client app_update esp_partition
              mbedtls esp_wifi esp_netif driver cjson
)
