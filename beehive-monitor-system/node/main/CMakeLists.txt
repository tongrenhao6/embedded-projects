idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES board_support frame_codec lora_transport sensor_service tilt_sensor freertos log
)
