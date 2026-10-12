/* stopwatches.c - translation of stopwatches.py */
#include "stopwatches.h"
#include "pdp11_logger.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static pyint monotonic_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (pyint)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void StopWatch_init(StopWatch *self, const char *instance_id)
{
    snprintf(self->instance_id, sizeof self->instance_id, "%s", instance_id);
    self->count = 0;
    self->min = 1000000000000LL;  /* "uninitialized" */
    self->max = -1;               /* "uninitialized" */
    self->sum = 0;
    self->mean = -1;              /* "uninitialized" */
    self->start_time = monotonic_ns();
    self->duration = 0;
}

static void StopWatch_restart_watch(StopWatch *self) { self->start_time = monotonic_ns(); }

static void StopWatch_stop_watch(StopWatch *self, pyint time_stop)
{
    self->duration = time_stop - self->start_time;
    if (self->duration < self->min) self->min = self->duration;
    if (self->duration > self->max) self->max = self->duration;
    self->sum = self->sum + self->duration;
    self->count = self->count + 1;
    self->mean = (double)self->sum / (double)self->count;
}

char *StopWatch_to_string(const StopWatch *self, char *buf, size_t cap)
{
    /* pad instance_id to 20, then five '{:12.0f}' fields */
    double reportmin = (double)self->min;
    if (self->min == INT64_MAX) reportmin = -1;  /* sys.maxsize */
    snprintf(buf, cap, "%-20s%12.0f%12.0f%12.0f%12.0f%12.0f",
             self->instance_id, reportmin / 1000, self->mean / 1000,
             (double)self->max / 1000, (double)self->sum / 1000, (double)self->count);
    return buf;
}

void StopWatches_init(StopWatches *self)
{
    self->watches = NULL;
    self->count = 0;
    self->capacity = 0;
}

void StopWatches_reset(StopWatches *self)
{
    free(self->watches);
    StopWatches_init(self);
}

static StopWatch *find(StopWatches *self, const char *instance_id)
{
    for (int i = 0; i < self->count; i++)
        if (!strcmp(self->watches[i].instance_id, instance_id)) return &self->watches[i];
    return NULL;
}

void StopWatches_start(StopWatches *self, const char *instance_id)
{
    StopWatch *w = find(self, instance_id);
    if (!w) {
        if (self->count == self->capacity) {
            self->capacity = self->capacity ? self->capacity * 2 : 16;
            self->watches = realloc(self->watches, (size_t)self->capacity * sizeof *self->watches);
        }
        w = &self->watches[self->count++];
        StopWatch_init(w, instance_id);
    }
    StopWatch_restart_watch(w);
}

void StopWatches_remove(StopWatches *self, const char *instance_id)
{
    StopWatch *w = find(self, instance_id);
    if (!w) PY_RAISE("KeyError", instance_id);
    int i = (int)(w - self->watches);
    memmove(&self->watches[i], &self->watches[i + 1], (size_t)(self->count - i - 1) * sizeof *w);
    self->count--;
}

void StopWatches_restart(StopWatches *self, const char *instance_id)
{
    StopWatches_remove(self, instance_id);
    StopWatches_start(self, instance_id);
}

void StopWatches_stop(StopWatches *self, const char *instance_id)
{
    pyint stop_time = monotonic_ns();
    StopWatch *w = find(self, instance_id);
    if (w) StopWatch_stop_watch(w, stop_time);
    else logging_info("WARN: StopWatches stop could not find id %s", instance_id);
}

double StopWatches_get_mean(StopWatches *self, const char *instance_id)
{
    StopWatch *w = find(self, instance_id);
    if (w) return w->mean;
    logging_info("WARN: StopWatches get_mean could not find id %s", instance_id);
    return -1;
}

char *StopWatches_to_string(StopWatches *self, const char *instance_id, char *buf, size_t cap)
{
    StopWatch *w = find(self, instance_id);
    if (w) return StopWatch_to_string(w, buf, cap);
    snprintf(buf, cap, "WARN: StopWatches to_string could not find id %s", instance_id);
    return buf;
}

void StopWatches_report(StopWatches *self)
{
    logging_info("StopWatches Report (times in microseconds)");
    logging_info("id                      min        mean         max         sum       count");
    for (int i = 0; i < self->count; i++) {
        char line[160];
        logging_info("%s", StopWatch_to_string(&self->watches[i], line, sizeof line));
    }
    logging_info("StopWatches Report end");
}

StopWatch *StopWatches_get_watch(StopWatches *self, const char *instance_id)
{
    StopWatch *w = find(self, instance_id);
    if (!w) PY_RAISE("KeyError", instance_id);
    return w;
}

pyint StopWatches_get_duration(StopWatches *self, const char *instance_id)
{
    return StopWatches_get_watch(self, instance_id)->duration;
}
