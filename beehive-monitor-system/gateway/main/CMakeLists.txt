idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES app_common board_support frame_codec freertos log lora_service mqtt_service
            serial_gateway wifi_service
)
