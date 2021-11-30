#ifndef _LIB_SENSOR_H_
#define _LIB_SENSOR_H_

#include "stdint.h"
#include "stdbool.h"

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
} __attribute__((packed)) sensor_packet_t;

typedef struct
{
    uint16_t e_co2;
    uint16_t e_ch2o;
    uint16_t tvoc;
    uint16_t pm2_5;
    uint16_t pm10;
    uint16_t temp_centi;
    uint16_t humi_centi;
} sensor_data_t;


bool sensor_check_packet(sensor_packet_t* packet);

void sensor_parse_data(sensor_packet_t* packet, sensor_data_t* data);

void sensor_init();

#endif /* _LIB_SENSOR_H_ */
