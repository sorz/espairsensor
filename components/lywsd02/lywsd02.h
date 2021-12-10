#ifndef _LIB_LYWSD02_H_
#define _LIB_LYWSD02_H_

typedef struct {
    uint16_t temp_centi;
    uint8_t humi;
} __attribute__((packed)) lywsd02_data_t;

void lywsd02_init();


#endif /* _LIB_LYWSD02_H_ */
