idf_component_register(
    SRCS "src/lora_payload.c" "src/lora_transport.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_uart driver freertos
)
