#include "esp_log.h"

#include "sm300d2.h"
#include "metrics.h"

static const char* TAG = "main";


void app_main() {
    sm300d2_init();
    metrics_init();

    metric_t test = {
        .name = "test",
        .help = "This is a test metric",
        .type = "counter",
        .unit = NULL,
        .value = "0"
    };
    metrics_put(&test, 5000);
    test.name = "test-2";
    metrics_put(&test, 5000);
    test.value[0] = '1';
    test.unit = "unit";
    metrics_put(&test, 20000);
}
