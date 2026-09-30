idf_component_register(
    SRCS "src/tilt_sensor.c" "src/tilt_sensor_algorithm.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c log
)
