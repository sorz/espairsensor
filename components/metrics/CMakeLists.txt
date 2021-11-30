idf_component_register(SRCS "metrics.c" "metrics.h"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_wifi esp_http_server)
