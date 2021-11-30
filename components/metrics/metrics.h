#ifndef _LIB_METRICS_H_
#define _LIB_METRICS_H_

typedef struct {
    char* name;
    char* help;
    char* type;
    char* unit;
    char value[16];
} metric_t;

void metrics_init();

void metrics_put(metric_t *metric);

#endif /* _LIB_METRICS_H_ */
