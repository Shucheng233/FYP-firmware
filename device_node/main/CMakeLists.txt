idf_component_register(
    SRCS "esp32_controller.c" "device_control.c"
    INCLUDE_DIRS "."
    REQUIRES driver led_strip
)