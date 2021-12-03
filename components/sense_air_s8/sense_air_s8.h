
#ifndef _LIB_SENSE_AIR_S8_H_
#define _LIB_SENSE_AIR_S8_H_
#include "stdint.h"
#include "stdbool.h"
#include "freertos/FreeRTOS.h"

typedef struct {
    uint8_t addr;
    uint8_t func;
    uint8_t size;
    uint16_t value_be;
    uint16_t crc32_le;
} __attribute__((packed)) s8_modbus_response_t;

void sense_air_s8_init();

int16_t sense_air_s8_read();

#endif /* _LIB_SENSE_AIR_S8_H_ */
