#include "esp_log.h"

#include "sm300d2.h"
#include "metrics.h"

#define TASK_STACK_SIZE     (2048)
#define METRIC_VALID_MILLIS (1000 * 30)


static const char* TAG = "main";


#define put_metric(V, N, U) {\
    metric.name = (N);\
    metric.unit = (U);\
    metric.value = (V);\
    metrics_put(&metric, METRIC_VALID_MILLIS);\
}

void task_sm300d2() {
    sm300d2_data_t data;
    metric_t metric = {
        .type = "gauge"
    };
    while (true) {
        if (pdTRUE != sm300d2_read_data(&data, portMAX_DELAY))
            continue;
        put_metric(data.e_co2, "sm300d2_co2", "ppm");
        put_metric(data.e_ch2o, "sm300d2_ch2o", "ug/m^3");
        put_metric(data.tvoc, "sm300d2_tvoc", "ug/m^3");
        put_metric(data.pm2_5, "sm300d2_pm2-5", "ug/m^3");
        put_metric(data.pm10, "sm300d2_pm10", "ug/m^3");
        put_metric(data.temp_centi / 100.0f, "sm300d2_temp", "°C");
        put_metric(data.humi_centi / 100.0f, "sm300d2_humi", "%");
    }
}

void app_main() {
    sm300d2_init();
    metrics_init();

    xTaskCreate(task_sm300d2, "task_sm300d2", TASK_STACK_SIZE, NULL, 10, NULL);
}
