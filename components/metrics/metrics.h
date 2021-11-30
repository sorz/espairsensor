#ifndef _LIB_METRICS_H_
#define _LIB_METRICS_H_

#include "freertos/semphr.h"

#define METRICS_MAX_NUM (CONFIG_METRICS_MAX_ITEMS)

typedef struct {
    char* name;
    char* help;
    char* type;
    char* unit;
    char value[16];
} metric_t;

typedef struct {
    metric_t items[METRICS_MAX_NUM];
    size_t len;
    SemaphoreHandle_t semphr;
} metric_list_t;

void metrics_init();

void metrics_put(metric_t *metric);

void metrics_list_init(metric_list_t *list);

#endif /* _LIB_METRICS_H_ */
