idf_component_register(SRCS "prometheus.c" "prometheus.h"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_wifi esp_http_server)
