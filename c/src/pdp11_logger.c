/* pdp11_logger.c - translation of pdp11_logger.py */
#include "pdp11_logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static FILE *log_file = NULL;
static int log_level = LOG_INFO;
static const char *log_path = "./pdp11.log";

void Logger(void)
{
    /* logFormat = '%(asctime)s.%(msecs)03d000Z %(filename)s:%(funcName)s %(levelname)s : %(message)s'
     * level=logging.INFO, force=True, filemode='w', converter = time.gmtime */
    if (log_file) fclose(log_file);
    log_file = fopen(log_path, "w");
    log_level = LOG_INFO;
    if (log_file) {
        char begins[64];
        snprintf(begins, sizeof begins, "%s begins", log_path);
        log_message(LOG_INFO, __FILE__, __func__, "%s", begins);
    }
}

int log_enabled(int level)
{
    /* Python's root logger with no basicConfig only shows WARNING and up on
     * stderr; the emulator always calls Logger() first, so only the file
     * case matters here. */
    return log_file != NULL && level >= log_level;
}

static const char *level_name(int level)
{
    switch (level) {
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO:  return "INFO";
    case LOG_ERROR: return "ERROR";
    default:        return "LEVEL";
    }
}

void log_message(int level, const char *file, const char *func, const char *fmt, ...)
{
    if (!log_enabled(level)) return;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r(&ts.tv_sec, &tm);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S", &tm);

    const char *base = strrchr(file, '/');
    base = base ? base + 1 : file;

    fprintf(log_file, "%s.%03ld000Z %s:%s %s : ", stamp, ts.tv_nsec / 1000000L, base, func, level_name(level));
    va_list ap;
    va_start(ap, fmt);
    vfprintf(log_file, fmt, ap);
    va_end(ap);
    fputc('\n', log_file);
    fflush(log_file);
}
