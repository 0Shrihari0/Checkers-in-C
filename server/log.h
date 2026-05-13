/*
 * log.h — Structured logging with timestamps and levels.
 */
#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

__attribute__((format(printf, 2, 3)))
static inline void log_msg(const char *level, const char *fmt, ...) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(stderr, "[%02d:%02d:%02d %s] ",
            t->tm_hour, t->tm_min, t->tm_sec, level);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

#define LOG_INFO(...)  log_msg("INFO",  __VA_ARGS__)
#define LOG_WARN(...)  log_msg("WARN",  __VA_ARGS__)
#define LOG_ERROR(...) log_msg("ERROR", __VA_ARGS__)

#endif
