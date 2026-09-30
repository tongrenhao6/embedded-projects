idf_component_register(
    SRCS "src/mqtt_service.c"
    INCLUDE_DIRS "include"
    REQUIRES app_common mqtt freertos frame_codec log mqtt_payload
)
