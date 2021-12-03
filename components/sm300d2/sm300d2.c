#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "endian.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sm300d2.h"


#define PORT_NUM         (CONFIG_SM300D2_UART_PORT_NUM)
#define BAUD_RATE        (9600)
#define RXD_PIN          (CONFIG_SM300D2_UART_RXD)
#define TASK_STACK_SIZE  (CONFIG_SM300D2_TASK_STACK_SIZE)
#define AGGREGATION_SECS (CONFIG_SM300D2_AGGREGATION_SECS)

#define BUF_SIZE 

static const char *TAG = "SM300D2";

static QueueHandle_t data_queue = NULL;

bool sm300d2_check_packet(sm300d2_packet_t* packet) {
    uint8_t sum = 0x00;
    for (uint8_t i = 0; i < sizeof(*packet) - 1; i++)
    {
        sum += ((uint8_t*) packet)[i];
    }
    return sum == packet->checksum;
}

void sm300d2_parse_data(sm300d2_packet_t* packet, sm300d2_data_t* data) {
    data->e_co2 = be16toh(packet->e_co2_be);
    data->e_ch2o = be16toh(packet->e_ch2o_be);
    data->tvoc = be16toh(packet->tvoc_be);
    data->pm2_5 = be16toh(packet->pm2_5_be);
    data->pm10 = be16toh(packet->pm10_be);
    data->temp_centi = packet->temp_int * 100 + packet->temp_frac;
    data->humi_centi = packet->humi_int * 100 + packet->humi_frac;
    ESP_LOGD(TAG, "CO2=%d CH2O=%d TVOC=%d PM2.5=%d PM10=%d T=%d H=%d",
             data->e_co2, data->e_ch2o, data->tvoc, data->pm2_5,
             data->pm10, data->temp_centi, data->humi_centi);
}

void sm300d2_init() {
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(PORT_NUM, UART_FIFO_LEN * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(PORT_NUM, UART_PIN_NO_CHANGE, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    data_queue = xQueueCreate(1, sizeof(sm300d2_data_t));
    assert(data_queue != 0);

    BaseType_t ret = xTaskCreate(sm300d2_receive_task, "sensor_receive_task", TASK_STACK_SIZE, NULL, 10, NULL);
    assert(ret == pdPASS);
}

void sm300d2_receive_task() {
    sm300d2_packet_t pkt;
    sm300d2_data_t data;
    sm300d2_data_t pts_sum = {};
    size_t pts_count = 0;
    int64_t pts_since_ms = esp_timer_get_time() / 1000;

    assert(data_queue != NULL);
    ESP_LOGI(TAG, "Listening sensor data...");

    while (true) {
        int64_t now_ms = esp_timer_get_time() / 1000;
        int64_t wait_ms = AGGREGATION_SECS * 1000 + (pts_since_ms - now_ms);
        if (wait_ms <= 1) { // Aggregation period passed
            ESP_LOGD(TAG, "Aggregating %d data points", pts_count);
            if (pts_count > 0) { // Data for send exist
                pts_sum.e_co2 /= pts_count;
                pts_sum.e_ch2o /= pts_count;
                pts_sum.tvoc /= pts_count;
                pts_sum.pm2_5 /= pts_count;
                pts_sum.pm10 /= pts_count;
                pts_sum.temp_centi /= pts_count;
                pts_sum.humi_centi /= pts_count;
                xQueueOverwrite(data_queue, &pts_sum);
            }
            pts_count = 0;
            memset(&pts_sum, 0, sizeof(pts_sum));
            pts_since_ms = esp_timer_get_time() / 1000;
            continue;
        }

        ESP_LOGD(TAG, "Read UART, wait for %llims", wait_ms);
        int len = uart_read_bytes(PORT_NUM, &pkt, sizeof(pkt), wait_ms / portTICK_RATE_MS);
        ESP_LOGD(TAG, "UART read %d", len);
        if (len < 0) {
            ESP_LOGW(TAG, "UART read failed, return %d", len);
            continue;
        }
        if (len == 0) continue;
        if (len < sizeof(pkt)) {
            ESP_LOGW(TAG, "UART insufficed bytes read (%d < %d)", len, sizeof(pkt));
            continue;
        }
        if (pkt.address != SM300D2_ADDRESS) {
            ESP_LOGW(TAG, "Frame with wrong header address %x", pkt.address);
            continue;
        }
        if (!sm300d2_check_packet(&pkt)) {
            ESP_LOGW(TAG, "Frame with wrong checksum");
            continue;
        }
        if (pkt.version != SM300D2_VERSION) {
            ESP_LOGW(TAG, "Unsupport version: %d", pkt.version);
            continue;
        }
        sm300d2_parse_data(&pkt, &data);
        pts_sum.e_co2 += data.e_co2;
        pts_sum.e_ch2o += data.e_ch2o;
        pts_sum.tvoc += data.tvoc;
        pts_sum.pm2_5 += data.pm2_5;
        pts_sum.pm10 += data.pm10;
        pts_sum.temp_centi += data.temp_centi;
        pts_sum.humi_centi += data.humi_centi;
        pts_count++;
    }
}

bool sm300d2_read_data(sm300d2_data_t* data, TickType_t xTicksToWait) {
    if (data_queue == NULL) {
        ESP_LOGE(TAG, "Queue uninitialized, call sm300d2_init() first");
        return false;
    }
    BaseType_t ret = xQueueReceive(data_queue, data, xTicksToWait);
    return ret == pdTRUE;
}