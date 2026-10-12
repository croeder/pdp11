/* pdp11_logger.h - translation of pdp11_logger.py plus the parts of Python's
 * logging module the emulator uses (info / debug / error to ./pdp11.log). */
#ifndef PDP11_LOGGER_H
#define PDP11_LOGGER_H

enum { LOG_DEBUG = 10, LOG_INFO = 20, LOG_ERROR = 40 };

/* Logger(): (re)open ./pdp11.log in write mode at INFO level, like
 * logging.basicConfig(..., force=True, filemode='w'). */
void Logger(void);

int  log_enabled(int level);
void log_message(int level, const char *file, const char *func, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

#define logging_info(...)  do { if (log_enabled(LOG_INFO))  log_message(LOG_INFO,  __FILE__, __func__, __VA_ARGS__); } while (0)
#define logging_debug(...) do { if (log_enabled(LOG_DEBUG)) log_message(LOG_DEBUG, __FILE__, __func__, __VA_ARGS__); } while (0)
#define logging_error(...) do { if (log_enabled(LOG_ERROR)) log_message(LOG_ERROR, __FILE__, __func__, __VA_ARGS__); } while (0)

#endif
