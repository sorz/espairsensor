#include "sm300d2.h"
#include "metrics.h"

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
    metrics_put(&test);
    test.name = "test-2";
    metrics_put(&test);
    test.value[0] = '1';
    test.unit = "unit";
    metrics_put(&test);
}
