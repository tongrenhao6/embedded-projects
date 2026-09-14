idf_component_register(
    SRCS "src/sensor_service.c" "src/sht30_codec.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c driver freertos log
)
