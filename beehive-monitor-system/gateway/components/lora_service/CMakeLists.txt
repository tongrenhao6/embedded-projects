idf_component_register(
    SRCS "src/lora_service.c" "src/lora_stream_parser.c"
    INCLUDE_DIRS "include"
    REQUIRES app_common esp_driver_uart freertos frame_codec log
)
