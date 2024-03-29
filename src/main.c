#include "esp_log.h"
#include "nvs_flash.h"

#include "sm300d2.h"
#include "sense_air_s8.h"
#include "lywsd02.h"
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
        if (!sm300d2_read_data(&data, portMAX_DELAY))
            continue;
        ESP_LOGD(TAG, "SM300D2 CO2=%d CH2O=...", data.e_co2);
        metric.precision = 0;
        put_metric(data.e_co2, "espair_sm300d2_co2_ppm", "ppm");
        put_metric(data.e_ch2o, "espair_sm300d2_ch2o_ug_m3", "ug_m3");
        put_metric(data.tvoc, "espair_sm300d2_tvoc_ug_m3", "ug_m3");
        put_metric(data.pm2_5, "espair_sm300d2_pm25_ug_m3", "ug_m3");
        put_metric(data.pm10, "espair_sm300d2_pm10_ug_m3", "ug_m3");
        metric.precision = 2;
        put_metric(data.temp_centi / 100.0f, "espair_sm300d2_temp_celsius", "celsius");
        put_metric(data.humi_centi / 100.0f, "espair_sm300d2_humi_precent", "precent");
    }
}

void task_sense_air_s8() {
    metric_t metric = {
        .name = "espair_senseairs8_co2_ppm",
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

void task_lywsd02() {
    lywsd02_data_t data;
    metric_t metric = {
        .type = "gauge",
    };
    while (true) {
        if (!lywsd02_read_data(&data, portMAX_DELAY))
            continue;
        metric.precision = 2;
        put_metric(data.temp_centi / 100.0f, "espair_lywsd02_temp_celsius", "celsius");
        metric.precision = 0;
        put_metric(data.humi, "espair_lywsd02_humi_precent", "precent");
    }
}

void init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void app_main() {
    init_nvs();
    sm300d2_init();
    sense_air_s8_init();
    lywsd02_init();
    metrics_init();

    xTaskCreate(task_sm300d2, "task_sm300d2", TASK_STACK_SIZE, NULL, 10, NULL);
    xTaskCreate(task_sense_air_s8, "task_sense_air_s8", TASK_STACK_SIZE, NULL, 10, NULL);
    xTaskCreate(task_lywsd02, "task_lywsd02", TASK_STACK_SIZE, NULL, 10, NULL);
}
