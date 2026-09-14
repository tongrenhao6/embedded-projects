idf_component_register(
    SRCS "src/board_info.c" "src/board_rgb.c"
    INCLUDE_DIRS "include"
    REQUIRES app_common esp_driver_rmt esp_hw_support spi_flash heap esp_system freertos log
)
