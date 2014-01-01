
#ifndef __CATSNATCH_LOG_H__
#define __CATSNATCH_LOG_H__

#include <stdio.h>

char *get_time_str_fmt(char *time_str, size_t len, const char *fmt);
char *get_time_str(char *time_str, size_t len);
void log_print(FILE *fd, const char *fmt, ...);

#define CATLOG(fmt, ...) log_print(stdout, fmt, ##__VA_ARGS__)
#define CATERR(fmt, ...) log_print(stderr, fmt, ##__VA_ARGS__)

#endif // __CATSNATCH_LOG_H__
