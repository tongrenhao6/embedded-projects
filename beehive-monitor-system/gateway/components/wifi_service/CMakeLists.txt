idf_component_register(
    SRCS "src/wifi_service.c"
    INCLUDE_DIRS "include"
    REQUIRES app_common esp_event esp_netif esp_wifi freertos log nvs_flash
)
