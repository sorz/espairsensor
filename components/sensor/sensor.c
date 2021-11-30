#include "stdint.h"
#include "stdbool.h"
#include "endian.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "sensor.h"


#define SENSOR_PORT_NUM         (CONFIG_SENSOR_UART_PORT_NUM)
#define SENSOR_BAUD_RATE        9600
#define SENSOR_RXD_PIN          (CONFIG_SENSOR_UART_RXD)
#define SENSOR_TASK_STACK_SIZE  (CONFIG_SENSOR_TASK_STACK_SIZE)

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
}

