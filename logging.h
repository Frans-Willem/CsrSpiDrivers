#ifndef _LOGGING_H
#define _LOGGING_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LOG_LEVEL_ALL = 0,
    LOG_LEVEL_ERR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,

    /* Special */
    LOG_FLAGS_DUMP = 1 << 8
};
#define LOG_LEVEL_QUIET (LOG_LEVEL_ALL)

#define LOG_LEVEL_MASK  0xff
#define LOG_MAX_LEVEL   LOG_LEVEL_DEBUG

#define LOG(lvl, ...)           _log_msg(__func__, __FILE__, __LINE__, LOG_LEVEL_ ## lvl, __VA_ARGS__)
#define DUMP(data, len, ...)    _log_hexdump(__func__, __FILE__, __LINE__, data, len, __VA_ARGS__)

void log_set_options(uint32_t lvl);
uint32_t log_get_options(void);
FILE *log_get_dest(void);
void log_set_dest(FILE *fp);
int log_set_file(const char *filename);
void _log_msg(const char *func, const char *file, int line, uint32_t level, const char *fmt, ...);
void _log_hexdump(const char *func, const char *file, int line, const void *data, size_t len, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* _LOGGING_H */
