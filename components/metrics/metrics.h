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
    int64_t update_at;
    size_t buf_size;
} metric_meta_t;

typedef struct {
    metric_t items[METRICS_MAX_NUM];
    metric_meta_t meta[METRICS_MAX_NUM];
    size_t len;
    SemaphoreHandle_t semphr;
} metric_list_t;

void metrics_init();

void metrics_put(metric_t *metric);

void metrics_list_init(metric_list_t *list);

void metrics_list_update_at(metric_list_t *list, size_t idx, metric_t *item);

#endif /* _LIB_METRICS_H_ */
