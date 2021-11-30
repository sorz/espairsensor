#include "stdint.h"
#include "stdbool.h"
#include "endian.h"

#include "sensor.h"


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
