#include "stdint.h"
#include "stdbool.h"
#include "endian.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "sense_air_s8.h"
#include "modbus.h"


#define PORT_NUM         (CONFIG_SENSE_AIR_S8_UART_PORT_NUM)
#define BAUD_RATE        (9600)
#define RXD_PIN          (CONFIG_SENSE_AIR_S8_UART_RXD)
#define TXD_PIN          (CONFIG_SENSE_AIR_S8_UART_TXD)
#define MAX_WAIT_MILLIS  (500)

static const char *TAG = "SENSE_AIR_S8";

void sense_air_s8_init() {
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

    ESP_ERROR_CHECK(uart_driver_install(PORT_NUM, UART_FIFO_LEN * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(PORT_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

static const uint8_t MODBUS_READ_CO2[] = {
    0xfe, // Address: any
    0x04, // Function: read input registers
    0x00, 0x03, // Register num: space CO2
    0x00, 0x01, // Quantity of registers
    0x25, 0xc5, // CRC checksum
};

int16_t sense_air_s8_read() {
    s8_modbus_response_t resp;
    int len;

    len = uart_write_bytes(PORT_NUM, MODBUS_READ_CO2, sizeof(MODBUS_READ_CO2));
    if (len != sizeof(MODBUS_READ_CO2)) {
        ESP_LOGE(TAG, "Failed to write UART");
        return ESP_FAIL;
    }
    len = uart_read_bytes(PORT_NUM, &resp, sizeof(resp), MAX_WAIT_MILLIS / portTICK_RATE_MS);
    if (len < 0) {
        ESP_LOGW(TAG, "UART read failed, return %d", len);
        return ESP_FAIL;
    }
    if (len == 0) {
        ESP_LOGD(TAG, "UART read nothing");
        return ESP_FAIL;
    }
    if (len < sizeof(resp)) {
        ESP_LOGW(TAG, "UART insufficed bytes read (%d < %d)", len, sizeof(resp));
        return ESP_FAIL;
    }
    if (crc16((uint8_t*) &resp, sizeof(resp)) != 0x0000) {
        ESP_LOGW(TAG, "Wrong CRC-16 checksum");
        return ESP_FAIL;
    }
    if (resp.addr != 0xfe || resp.func != 0x04 || resp.size != 2) {
        ESP_LOGW(TAG, "Wrong response header: %x %x %x",
                 resp.addr, resp.func, resp.size);
        return ESP_FAIL;
    }
    return be16toh(resp.value_be);
}