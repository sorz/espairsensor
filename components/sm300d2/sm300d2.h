#ifndef _LIB_SM300D2_H_
#define _LIB_SM300D2_H_

#include "stdint.h"
#include "stdbool.h"
#include "freertos/FreeRTOS.h"

#define SM300D2_ADDRESS (0x3c)
#define SM300D2_VERSION (0x02)

typedef struct {
    uint8_t address;
    uint8_t version;
    uint16_t e_co2_be;
    uint16_t e_ch2o_be;
    uint16_t tvoc_be;
    uint16_t pm2_5_be;
    uint16_t pm10_be;
    uint8_t temp_int;
    uint8_t temp_frac;
    uint8_t humi_int;
    uint8_t humi_frac;
    uint8_t checksum;
} __attribute__((packed)) sm300d2_packet_t;

typedef struct
{
    uint16_t e_co2;
    uint16_t e_ch2o;
    uint16_t tvoc;
    uint16_t pm2_5;
    uint16_t pm10;
    uint16_t temp_centi;
    uint16_t humi_centi;
} sm300d2_data_t;

bool sm300d2_check_packet(sm300d2_packet_t* packet);

void sm300d2_parse_data(sm300d2_packet_t* packet, sm300d2_data_t* data);

void sm300d2_init();

void sm300d2_receive_task();

bool sm300d2_read_data(sm300d2_data_t* data, TickType_t xTicksToWait);


#endif /* _LIB_SM300D2_H_ */
