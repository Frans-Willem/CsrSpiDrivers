#ifndef _LOGGING_H
#define _LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LOG_LEVEL_QUIET = 0,
    LOG_LEVEL_ERR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,

    /* Special */
    LOG_FLAGS_DUMP = 1 << 8
};

#define LOG_LEVEL_MASK  0xff

#define log_msg(lvl, ...)       _log_hexdump_msg(__func__, __FILE__, __LINE__, lvl, NULL, 0, __VA_ARGS__)
#define LOG(lvl, ...)           _log_hexdump_msg(__func__, __FILE__, __LINE__, LOG_LEVEL_ ## lvl, NULL, 0, __VA_ARGS__)

#define log_hexdump(lvl, data, len, ...) _log_hexdump_msg(__func__, __FILE__, __LINE__, lvl, data, len, __VA_ARGS__)
#define DUMP(data, len, ...)    _log_hexdump_msg(__func__, __FILE__, __LINE__, LOG_FLAGS_DUMP, data, len, __VA_ARGS__)

void log_set_options(uint32_t lvl);
void log_set_dest(FILE *fp);
int log_set_file(const char *filename);
void _log_hexdump_msg(const char *func, const char *file, int line, uint32_t level, const void *data, size_t len, const char *fmt, ...);
uint32_t crc32(uint32_t crc, const void *buf, size_t size);
uint32_t buf_crc32(const void *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* _LOGGING_H */
