idf_component_register(SRCS "sense_air_s8.c" "sense_air_s8.h"
                       INCLUDE_DIRS "."
                       REQUIRES esp_timer esp_driver_uart esp_driver_gpio)
