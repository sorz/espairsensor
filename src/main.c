#include "sm300d2.h"
#include "prometheus.h"

void app_main() {
    sm300d2_init();
    prometheus_init();
}
