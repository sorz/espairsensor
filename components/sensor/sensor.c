#include "stdint.h"
#include "stdbool.h"
#include "endian.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sensor.h"


#define SENSOR_PORT_NUM         (CONFIG_SENSOR_UART_PORT_NUM)
#define SENSOR_BAUD_RATE        (9600)
#define SENSOR_RXD_PIN          (CONFIG_SENSOR_UART_RXD)
#define SENSOR_TASK_STACK_SIZE  (CONFIG_SENSOR_TASK_STACK_SIZE)

#define BUF_SIZE 

static const char *TAG = "SENSOR";

static QueueHandle_t data_queue = NULL;

bool sensor_check_packet(sensor_packet_t* packet) {
    uint8_t sum = 0x00;
    for (uint8_t i = 0; i < sizeof(packet) - 1; i++)
    {
        sum += ((uint8_t*) packet)[i];
    }
    return sum == packet->checksum;
}

void sensor_parse_data(sensor_packet_t* packet, sensor_data_t* data) {
    data->e_co2 = be16toh(packet->e_co2_be);
    data->e_ch2o = be16toh(packet->e_ch2o_be);
    data->tvoc = be16toh(packet->tvoc_be);
    data->pm2_5 = be16toh(packet->pm2_5_be);
    data->pm10 = be16toh(packet->pm10_be);
    data->temp_centi = packet->temp_int * 100 + packet->temp_frac;
    data->humi_centi = packet->humi_int * 100 + packet->humi_frac;
}

void sensor_init() {
    uart_config_t uart_config = {
        .baud_rate = SENSOR_BAUD_RATE,
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

    ESP_ERROR_CHECK(uart_driver_install(SENSOR_PORT_NUM, UART_FIFO_LEN * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(SENSOR_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(SENSOR_PORT_NUM, UART_PIN_NO_CHANGE, SENSOR_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    data_queue = xQueueCreate(1, sizeof(sensor_data_t));
    assert(data_queue != 0);

    BaseType_t ret = xTaskCreate(sensor_receive_task, "sensor_receive_task", SENSOR_TASK_STACK_SIZE, NULL, 10, NULL);
    assert(ret == pdPASS);
}

void sensor_receive_task() {
    sensor_packet_t pkt;
    sensor_data_t data;

    assert(data_queue != NULL);

    while (true) {
        int len = uart_read_bytes(SENSOR_PORT_NUM, &pkt, sizeof(pkt), portMAX_DELAY);
        if (len < 0) {
            ESP_LOGW(TAG, "UART read failed, return %d", len);
            continue;
        }
        if (len == 0) continue;
        if (len < sizeof(pkt)) {
            ESP_LOGW(TAG, "UART insufficed bytes read (%d < %d)", len, sizeof(pkt));
            continue;
        }
        if (pkt.address != SENSOR_ADDRESS || !sensor_check_packet(&pkt)) {
            ESP_LOGW(TAG, "Frame with wrong checksum or header");
            continue;
        }
        if (pkt.version != SENSOR_VERSION) {
            ESP_LOGW(TAG, "Unsupport version: %d", pkt.version);
            continue;
        }
        sensor_parse_data(&pkt, &data);
        xQueueOverwrite(data_queue, &data);
    }
}

bool sensor_read_data(sensor_data_t* data, TickType_t xTicksToWait) {
    if (data_queue == NULL) {
        ESP_LOGE(TAG, "Queue uninitialized, call sensor_init() first");
        return false;
    }
    BaseType_t ret = xQueueReceive(data_queue, &data, xTicksToWait);
    return ret == pdTRUE;
}