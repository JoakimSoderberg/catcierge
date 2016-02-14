
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>
#include <io.h>
#endif
#include "catcierge_test_helpers.h"

static int verbose;
static int log_on;
static int nocolor;

int catcierge_test_verbose()
{
	return verbose;
}

int catcierge_test_log_on()
{
	return log_on;
}

void catcierge_test_nocolor(int _nocolor)
{
	nocolor = _nocolor;
}

int catcierge_test_init(int argc, char **argv)
{
	catcierge_test_parse_cmdline(argc, argv);

	return 0;
}

int catcierge_test_parse_cmdline(int argc, char **argv)
{
	int i;

	for (i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "--verbose"))
		{
			verbose = 1;
		}
		else if (!strcmp(argv[i], "--log"))
		{
			log_on = 1;
		}
	}

	return 0;
}

void catcierge_test_vprintf(FILE *target, enum catcierge_color_e test_color, const char *fmt, va_list args)
{
	#ifdef _WIN32
	WORD color = 0;
	
	HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hstdout, &csbi);

	switch (test_color)
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
	if (!nocolor && color) SetConsoleTextAttribute(hstdout, color);

	vfprintf(target, fmt, args);

	// Reset color.
	if (!nocolor) SetConsoleTextAttribute(hstdout, csbi.wAttributes);

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

	if (!nocolor)
	{
		switch (test_color)
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

	if (!nocolor) fprintf(target, ANSI_COLOR_RESET);
	
	#endif
}

void catcierge_test_printf(FILE *target, enum catcierge_color_e test_color, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	catcierge_test_vprintf(target, test_color, fmt, args);

	va_end(args);
}

void catcierge_test_SUCCESS(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	catcierge_test_printf(stdout, COLOR_BRIGHT, "[");
	catcierge_test_printf(stdout, COLOR_GREEN, "SUCCESS");
	catcierge_test_printf(stdout, COLOR_BRIGHT, "] ");
	vfprintf(stdout, fmt, args);
	printf("\n");

	va_end(args);
}

void catcierge_test_FAILURE(const char *fmt, ...)
{
	// TODO: Make a version of this that saves the output to a buffer instead. 
	// And queues all log messages, so that we can present all failure at the end
	// of a long test!
	va_list args;
	va_start(args, fmt);
	catcierge_test_printf(stdout, COLOR_BRIGHT, "[");
	catcierge_test_printf(stdout, COLOR_RED, "FAILURE");
	catcierge_test_printf(stdout, COLOR_BRIGHT, "] ");
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");

	va_end(args);
}

void catcierge_test_STATUS_ex(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	catcierge_test_vprintf(stdout, COLOR_STATUS, fmt, args);

	va_end(args);
}

void catcierge_test_STATUS(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	catcierge_test_vprintf(stdout, COLOR_STATUS, fmt, args);
	printf("\n");

	va_end(args);
}

void catcierge_test_SKIPPED(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	catcierge_test_printf(stdout, COLOR_BRIGHT, "[");
	catcierge_test_printf(stdout, COLOR_YELLOW, "SKIPPED");
	catcierge_test_printf(stdout, COLOR_BRIGHT, "] ");
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");

	va_end(args);
}

void catcierge_test_HEADLINE(const char *headline)
{
	#define MAX_HEADLINE_WIDTH 80
	size_t i;
	size_t len = strlen(headline);
	size_t start = (MAX_HEADLINE_WIDTH - len) / 2;

	for (i = 0; i < (start - 1); i++)
	{
		catcierge_test_printf(stdout, COLOR_BRIGHT, "=");
	}

	catcierge_test_printf(stdout, COLOR_MAGNETA, " %s ", headline);

	for (i = start + len + 1; i < MAX_HEADLINE_WIDTH; i++)
	{
		catcierge_test_printf(stdout, COLOR_BRIGHT, "=");
	}

	fprintf(stdout, "\n");
}

static int malloc_fail_count;
static int malloc_current;
static int realloc_fail_count;
static int realloc_current;

void catcierge_test_set_malloc_fail_count(int count)
{
	malloc_current = 0;
	malloc_fail_count = count;
}

void *catcierge_test_malloc(size_t sz)
{
	malloc_current++;

	if ((malloc_fail_count > 0)
		&& (malloc_current >= malloc_fail_count))
		return NULL;

	return malloc(sz);
}

void catcierge_test_set_realloc_fail_count(int count)
{
	realloc_current = 0;
	realloc_fail_count = count;
}

void *catcierge_test_realloc(void *ptr, size_t sz)
{	
	realloc_current++;

	if ((realloc_fail_count > 0)
		&& (realloc_current >= realloc_fail_count))
		return NULL;

	return realloc(ptr, sz);
}


