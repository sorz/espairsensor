#include "esp_log.h"

#include "sm300d2.h"
#include "sense_air_s8.h"
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
        .type = "gauge",
    };
    while (true) {
        if (pdTRUE != sm300d2_read_data(&data, portMAX_DELAY))
            continue;
        ESP_LOGD(TAG, "SM300D2 CO2=%d CH2O=...", data.e_co2);
        metric.precision = 0;
        put_metric(data.e_co2, "sm300d2_co2", "ppm");
        put_metric(data.e_ch2o, "sm300d2_ch2o", "ug/m^3");
        put_metric(data.tvoc, "sm300d2_tvoc", "ug/m^3");
        put_metric(data.pm2_5, "sm300d2_pm2-5", "ug/m^3");
        put_metric(data.pm10, "sm300d2_pm10", "ug/m^3");
        metric.precision = 2;
        put_metric(data.temp_centi / 100.0f, "sm300d2_temp", "°C");
        put_metric(data.humi_centi / 100.0f, "sm300d2_humi", "%");
    }
}

void task_sense_air_s8() {
    metric_t metric = {
        .name = "senseairs8_co2",
        .type = "gauge",
        .unit = "ppm",
        .precision = 0,
    };
    while (true) {
        int16_t value = sense_air_s8_read();
        ESP_LOGD(TAG, "SenseAir S8 CO2=%d", value);
        if (value == -1) continue;
        metric.value = value;
        metrics_put(&metric, METRIC_VALID_MILLIS);
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
}

void app_main() {
    sm300d2_init();
    sense_air_s8_init();
    metrics_init();

    xTaskCreate(task_sm300d2, "task_sm300d2", TASK_STACK_SIZE, NULL, 10, NULL);
    xTaskCreate(task_sense_air_s8, "task_sense_air_s8", TASK_STACK_SIZE, NULL, 10, NULL);
}
