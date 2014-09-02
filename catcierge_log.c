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
#ifdef _WIN32
#include <Windows.h>
#include "win32/gettimeofday.h" 
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include <stdarg.h>
#include <time.h>
#include "catcierge_log.h"

int catcierge_nocolor = 0;

char *get_time_str_fmt(time_t t, struct timeval *tv, char *time_str, size_t len, const char *fmt)
{
	struct tm *tm;
	tm = localtime(&t);

	#if 0
	// TODO: Fix this (circular include)!
	if (catcierge_strftime(time_str, len, fmt, tm, tv))
	{
		return NULL;
	}
	#endif
	strftime(time_str, len, fmt, tm);

	return time_str;
}

char *get_time_str(char *time_str, size_t len)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	return get_time_str_fmt(time(NULL), &tv, time_str, len, "%Y-%m-%d %H:%M:%S");
}

void log_vprintf(FILE *target, enum catcierge_color_e print_color, const char *fmt, va_list args)
{
	#ifdef _WIN32
	WORD color = 0;

	HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hstdout, &csbi);

	switch (print_color)
	{
		default:
		case COLOR_NORMAL: break;
		case COLOR_GREEN: color = (FOREGROUND_GREEN | FOREGROUND_INTENSITY); break;
		case COLOR_RED: color = (FOREGROUND_RED | FOREGROUND_INTENSITY); break;
		case COLOR_YELLOW: color = (14 | FOREGROUND_INTENSITY); break;
		case COLOR_CYAN: color = (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY); break;
		case COLOR_MAGNETA: color =  (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY); break;
		case COLOR_BRIGHT: color = (FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED); break;
		case COLOR_STATUS: color = FOREGROUND_INTENSITY; break;
	}

	// Set color.
	if (!catcierge_nocolor && color) SetConsoleTextAttribute(hstdout, color);

	vfprintf(target, fmt, args);

	// Reset color.
	if (!catcierge_nocolor) SetConsoleTextAttribute(hstdout, csbi.wAttributes);

	#else // end WIN32

	#define ANSI_COLOR_BLACK		"\x1b[22;30m"
	#define ANSI_COLOR_RED			"\x1b[22;31m"
	#define ANSI_COLOR_GREEN		"\x1b[22;32m"
	#define ANSI_COLOR_YELLOW		"\x1b[22;33m"
	#define ANSI_COLOR_BLUE			"\x1b[22;34m"
	#define ANSI_COLOR_MAGENTA		"\x1b[22;35m"
	#define ANSI_COLOR_CYAN			"\x1b[22;36m"
	#define ANSI_COLOR_GRAY			"\x1b[22;37m"
	#define ANSI_COLOR_DARK_GRAY	"\x1b[01;30m"
	#define ANSI_COLOR_LIGHT_RED	"\x1b[01;31m"
	#define ANSI_COLOR_LIGHT_GREEN	"\x1b[01;32m"
	#define ANSI_COLOR_LIGHT_BLUE	"\x1b[01;34m"
	#define ANSI_COLOR_LIGHT_MAGNETA "\x1b[01;35m"
	#define ANSI_COLOR_LIGHT_CYAN	"\x1b[01;36m"
	#define ANSI_COLOR_WHITE		"\x1b[01;37m"
	#define ANSI_COLOR_RESET		"\x1b[0m"

	if (!catcierge_nocolor)
	{
		switch (print_color)
		{
			default:
			case COLOR_NORMAL: break;
			case COLOR_GREEN: fprintf(target, ANSI_COLOR_LIGHT_GREEN);  break;
			case COLOR_RED:	fprintf(target, ANSI_COLOR_LIGHT_RED); break;
			case COLOR_YELLOW: fprintf(target, ANSI_COLOR_YELLOW); break;
			case COLOR_CYAN: fprintf(target, ANSI_COLOR_LIGHT_CYAN); break;
			case COLOR_MAGNETA: fprintf(target, ANSI_COLOR_LIGHT_MAGNETA); break;
			case COLOR_BRIGHT: fprintf(target, ANSI_COLOR_WHITE); break;
			case COLOR_STATUS: fprintf(target, ANSI_COLOR_DARK_GRAY); break;
		}
	}

	vfprintf(target, fmt, args);

	if (!catcierge_nocolor) fprintf(target, ANSI_COLOR_RESET);

	#endif
}

void log_printf(FILE *fd, enum catcierge_color_e print_color, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_vprintf(fd, print_color, fmt, args);
	va_end(args);
}

void log_printc(FILE *fd, enum catcierge_color_e print_color, const char *fmt, ...)
{
	va_list ap;
	char time_str[256];
	get_time_str(time_str, sizeof(time_str));

	if ((fd == stdout) || (fd == stderr))
	{
		log_printf(fd, COLOR_NORMAL, "[");
		log_printf(fd, COLOR_BRIGHT, "%s", time_str);
		log_printf(fd, COLOR_NORMAL, "]  ");

		va_start(ap, fmt);
		log_vprintf(fd, print_color, fmt, ap);
		va_end(ap);
	}
	else
	{
		fprintf(fd, "[%s]  ", time_str);
		va_start(ap, fmt);
		vfprintf(fd, fmt, ap);
		va_end(ap);
	}
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

