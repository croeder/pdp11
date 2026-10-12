/* stopwatches.h - translation of stopwatches.py */
#ifndef STOPWATCHES_H
#define STOPWATCHES_H

#include "pyint.h"

typedef struct {
    char instance_id[48];
    pyint count;
    pyint min;
    pyint max;
    pyint sum;
    double mean;
    pyint start_time;
    pyint duration;
} StopWatch;

typedef struct {
    StopWatch *watches;   /* insertion order, like a Python dict */
    int count;
    int capacity;
} StopWatches;

void StopWatches_init(StopWatches *self);
void StopWatches_reset(StopWatches *self);
void StopWatches_start(StopWatches *self, const char *instance_id);
void StopWatches_remove(StopWatches *self, const char *instance_id);
void StopWatches_restart(StopWatches *self, const char *instance_id);
void StopWatches_stop(StopWatches *self, const char *instance_id);
double StopWatches_get_mean(StopWatches *self, const char *instance_id);
char *StopWatches_to_string(StopWatches *self, const char *instance_id, char *buf, size_t cap);
void StopWatches_report(StopWatches *self);
StopWatch *StopWatches_get_watch(StopWatches *self, const char *instance_id);
pyint StopWatches_get_duration(StopWatches *self, const char *instance_id);

char *StopWatch_to_string(const StopWatch *self, char *buf, size_t cap);

#endif
