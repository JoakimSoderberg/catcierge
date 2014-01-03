//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2014
//
//    Catcierge is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//    Catcierge is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Catcierge.  If not, see <http://www.gnu.org/licenses/>.
//
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include "catcierge_log.h"

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
	fprintf(fd, "[%s]  ", time_str);

	va_start(ap, fmt);
	vfprintf(fd, fmt, ap);
	va_end(ap);
}

void log_print_csv(FILE *fd, const char *fmt, ...)
{
	char time_str[256];
	va_list ap;

	if (!fd)
		return;

	get_time_str(time_str, sizeof(time_str));
	fprintf(fd, "%s, ", time_str);

	va_start(ap, fmt);
	vfprintf(fd, fmt, ap);
	va_end(ap);
}

