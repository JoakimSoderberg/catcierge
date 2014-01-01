
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
#include "catsnatch_log.h"

char *get_time_str_fmt(char *time_str, size_t len, const char *fmt)
{
	struct tm *tm;
	time_t t;

	t = time(NULL);
	tm = localtime(&t);

	strftime(time_str, len, fmt, tm);

	return time_str;
}

char *get_time_str(char *time_str, size_t len)
{
	return get_time_str_fmt(time_str, len, "%Y-%m-%d %H:%M:%S");
}

void log_print(FILE *fd, const char *fmt, ...)
{
	char time_str[256];
	va_list ap;

	get_time_str(time_str, sizeof(time_str));

	va_start(ap, fmt);
	fprintf(fd, "[%s]  ", time_str);
	vprintf(fmt, ap);
	va_end(ap);
}

