//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2015
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
#include <stdio.h>
#include <errno.h>
#include "catcierge_config.h"
#include "catcierge_util.h"
#include "catcierge_strftime.h"
#ifdef CATCIERGE_HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef CATCIERGE_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef CATCIERGE_HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include "catcierge_log.h"

const char *catcierge_path_sep()
{
	#ifdef _WIN32
	return "\\";
	#else
	return "/";
	#endif
}

mode_t catcierge_file_mode(const char *filename)
{
	// Originally from CZMQ.
	#ifdef _WIN32
	unsigned short mode;
	DWORD dwfa = GetFileAttributesA(filename);

	if (dwfa == 0xffffffff)
		return -1;

	mode = 0;

	if (dwfa & FILE_ATTRIBUTE_DIRECTORY)
		mode |= S_IFDIR;
	else
		mode |= S_IFREG;

	if (!(dwfa & FILE_ATTRIBUTE_HIDDEN))
		mode |= S_IRUSR;

	if (!(dwfa & FILE_ATTRIBUTE_READONLY))
		mode |= S_IWUSR;

	return mode;
	#else
	struct stat stat_buf;

	if (stat((char *) filename, &stat_buf) == 0)
		return stat_buf.st_mode;
	else
		return -1;
	#endif
}

//
// Format a string with variable arguments, returning a freshly allocated
// buffer. If there was insufficient memory, returns NULL. Free the returned
// string using free().
//
char *catcierge_vprintf(const char *format, va_list argptr)
{
	// Originally from CZMQ.
	int size = 256;
	int required;
	char *string = (char *)malloc(size);
	// Using argptr is destructive, so we take a copy each time we need it
	// We define va_copy for Windows in catcierge_platform.h
	va_list my_argptr;
	va_copy(my_argptr, argptr);
	required = vsnprintf(string, size, format, my_argptr);
	va_end(my_argptr);

	#ifdef _WIN32
	if (required < 0 || required >= size)
	{
		va_copy(my_argptr, argptr);
		#ifdef _MSC_VER
		required = _vscprintf(format, argptr);
		#else
		required = vsnprintf(NULL, 0, format, argptr);
		#endif
		va_end(my_argptr);
	}
	#endif // _WIN32

	// If formatted string cannot fit into small string, reallocate a
	// larger buffer for it.
	if (required >= size)
	{
		size = required + 1;
		free(string);
		string = (char *)malloc(size);

		if (string) 
		{
			va_copy(my_argptr, argptr);
			vsnprintf(string, size, format, my_argptr);
			va_end(my_argptr);
		}
	}

	return string;
}

int catcierge_make_path(const char *pathname, ...)
{
	// Originally from CZMQ.
	int ret = 0;
	mode_t mode;
	char *formatted;
	char *slash;
	va_list argptr;
	va_start(argptr, pathname);
	formatted = catcierge_vprintf(pathname, argptr);
	va_end(argptr);

	if (!formatted)
	{
		return -1;
	}

	// Create parent directory levels if needed
	slash = strchr(formatted + 1, '/');

	while (1)
	{
		if (slash)
			*slash = 0; // Cut at slash
		
		mode = catcierge_file_mode(formatted);
		
		if (mode == (mode_t)-1) 
		{
			// Does not exist, try to create it
			#ifdef _WIN32
			if (!CreateDirectoryA(formatted, NULL))
			#else
			if (mkdir(formatted, 0775))
			#endif
			{
				ret = -1; goto fail;
			}
		}
		else if ((mode & S_IFDIR) == 0) 
		{
			// Not a directory, abort
		}

		// End if last segment
		if (!slash)
			break;

		*slash = '/';
		slash = strchr(slash + 1, '/');
	}

fail:
	free(formatted);
	return ret;
}

const char *catcierge_get_direction_str(match_direction_t dir)
{
	switch (dir)
	{
		case MATCH_DIR_IN: return "in";
		case MATCH_DIR_OUT: return "out";
		case MATCH_DIR_UNKNOWN: 
		default: return "unknown";
	}
}

const char *catcierge_get_left_right_str(direction_t dir)
{
	switch (dir)
	{
		case DIR_LEFT: return "left";
		case DIR_RIGHT: return "right";
		default: return "unknown";	
	}
}

void catcierge_reset_cursor_position()
{
	#ifdef _WIN32
	COORD pos;
	CONSOLE_SCREEN_BUFFER_INFO info;
	HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

	// Get current position.
	GetConsoleScreenBufferInfo(output, &info);
	pos.X = 0;
	pos.Y = info.dwCursorPosition.Y - 1; // Move one up.

	SetConsoleCursorPosition(output, pos);

	#else // _WIN32

	printf("\033[999D"); // Move cursor back 999 steps (beginning of line).
	printf("\033[1A"); // Move cursor up 1 step.
	printf("\033[0K"); // Clear from cursor to end of line.

	#endif // !_WIN32
}

void catcierge_run(char *command)
{
	#ifndef _WIN32
	{
		char *argv[4] = {0};
		pid_t pid;
		argv[0] = "/bin/sh";
		argv[1] = "-c";
		argv[2] = command;
		argv[3] = NULL;

		// Fork a child process.
		if ((pid = fork()) < 0)
		{
			CATERR("Forking child process failed: %d, %s\n", errno, strerror(errno));
		}
		else if (pid == 0)
		{
			// For the child process.

			// Execute the command.
			if (execvp(*argv, argv) < 0)
			{
				CATERR("Exec \"%s\" failed: %d, %s\n", command, errno, strerror(errno));
				exit(1);
			}
		}
		else
		{
			CATLOG("Called program \"%s\"\n", command);
		}
	}
	#else // _WIN32
	{
		char tmp[4096];
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		memset(&si, 0, sizeof(si));
		si.dwXSize = sizeof(si);
		memset(&pi, 0, sizeof(pi));

		snprintf(tmp, sizeof(tmp) - 1, "c:\\Windows\\system32\\cmd.exe /c %s", command);

		if (!CreateProcess(
				NULL, 
				tmp,			// Commandline
				NULL,           // Process handle not inheritable
				NULL,           // Thread handle not inheritable
				FALSE,          // Set handle inheritance to FALSE
				0,              // No creation flags
				NULL,           // Use parent's environment block
				NULL,           // Use parent's starting directory 
				&si,            // Pointer to STARTUPINFO structure
				&pi ))          // Pointer to PROCESS_INFORMATION structure
		{
			CATERR("Error %d, Failed to run command %s\n", GetLastError(), command);
		}
		else
		{
			CATLOG("Called program \"%s\"\n", command);
		}
	}
	#endif // _WIN32
}

//
// The string "command" contains a commandline that will be
// executed. This can contain variable references (%0, %1, %2, ...) 
// that are different depending on in what context the command
// is exectued in. These are called OUTPUT variables.
//
// The format string "fmt" contains a format list
// same as for printf delimited by spaces. Each format specifier
// refers to the value of one of the OUTPUT variables given in the
// varargs list.
//
// Example call:
// %0 = Match result. (m->result is a float so %f is used as format specifier).
// %1 = Match success.
// %2 = Image path (of now saved image).
// catcierge_execute(save_img_cmd, "%f %d %s", m->result, m->success, m->path);
//

void catcierge_execute(char *command, char *fmt, ...)
{
	int i;
	int extra_opt = 0;
	static char buf[2048];
	static char tmp[2048];
	char *s;
	int cur_opt = 0;
	char *extras[32];
	int count = 0;
	va_list args;

	if (!command)
		return;

	// Get the variable arguments into a nice array.

	// Format the input into a string.
	va_start(args, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, args);
	va_end(args);

	// Tokenize on space so we get the variables
	// in a nice array.
	s = strtok(tmp, " ");
	count = 0;

	while (s)
	{
		extras[count] = s;
		s = strtok(NULL, " ");
		
		count++;
	}

	// Finally use the array to replace the variables
	// in the command input.
	i = 0;

	while (*command)
	{
		// Replace any variables in the command input.
		if ((count > 0) && (*command == '%'))
		{
			command++;

			// Two percentages in a row = literal % sign.
			if (*command != '%')
			{
				if (!strncmp(command, "cwd", 3))
				{
					// Special case, will return the current directory.
					char cwd[PATH_MAX];

					if (getcwd(cwd, sizeof(cwd)))
					{
						strcpy(&buf[i], cwd);
						i += strlen(cwd);
					}

					command += 3; // Skip "cwd"
					continue;
				}
				else
				{
					// Otherwise we expect a digit that refers to
					// the given variables in "extras".
					// %0, %1 .. and so on.
					cur_opt = -1;

					if (sscanf(command, "%d", &cur_opt) == 0)
					{
						command--; // Reclaim the %
						CATERR("Invalid variable \"%.10s...\". "
								"Did you mean to use --new_execute?\n", command);
						return;
					}

					while (isdigit(*command)) command++;

					if ((cur_opt >= 0) && (cur_opt < count))
					{
						strcpy(&buf[i], extras[cur_opt]);
						i += strlen(extras[cur_opt]);
					}
				}
			}
		}

		buf[i] = *command;
		i++;

		command++;
	}

	catcierge_run(buf);
}

const char *catcierge_skip_whitespace(const char *it)
{
	while ((*it == ' ') || (*it == '\t'))
	{
		it++;
	}

	return it;
}

char *catcierge_skip_whitespace_alt(char *it)
{
	while ((*it == ' ') || (*it == '\t'))
	{
		it++;
	}

	return it;
}

void catcierge_free_list(char **list, size_t count)
{
	size_t i;

	if (!list)
		return;

	for (i = 0; i < count; i++)
	{
		if (list[i])
		{
			free(list[i]);
			list[i] = NULL;
		}
	}

	free(list);
}

void catcierge_end_trim_whitespace(char *s)
{
	size_t len = strlen(s);
	char *end = s + len - 1;

	if (len == 0)
		return;

	while ((end > s) && ((*end == ' ') || (*end == '\t')))
	{
		*end = '\0';
		end--;
	}
}

char **catcierge_parse_list(const char *input, size_t *list_count, int end_trim)
{
	size_t i;
	const char *s = input;
	char **list = NULL;
	char *input_copy = NULL;
	assert(list_count);
	*list_count = 0;

	if (!input || (strlen(input) == 0))
	{
		return NULL;
	}

	// TODO: Enable using different delimeter here...
	while ((s = strchr(s, ',')))
	{
		(*list_count)++;
		s++;
	}

	// 1,2,3 -> 3 values.
	(*list_count)++;

	if (!(list = (char **)calloc(*list_count, sizeof(char *))))
	{
		CATERR("Out of memory!\n"); return NULL;
	}

	// strtok changes its input.
	if (!(input_copy = strdup(input)))
	{
		CATERR("Out of memory!\n"); goto fail;
	}

	s = strtok(input_copy, ",");

	for (i = 0; i < (*list_count); i++)
	{
		s = catcierge_skip_whitespace(s);

		if (!(list[i] = strdup(s)))
		{
			CATERR("Out of memory!\n"); goto fail;
		}

		if (end_trim)
		{
			catcierge_end_trim_whitespace(list[i]);
		}

		s = strtok(NULL, ",");

		if (!s)
			break;
	}

	free(input_copy);
	return list;

fail:
	catcierge_free_list(list, *list_count);
	*list_count = 0;

	if (input_copy)
	{
		free(input_copy);
	}

	return NULL;
}

char *catcierge_get_abs_path(const char *path, char *buf, size_t buflen)
{
	#ifdef _WIN32
	DWORD ret = 0;
	buf[0] = '\0';

	if (!path || (buflen <= 0))
		return NULL;

	if (!(ret = GetFullPathName(path, buflen, buf, NULL)))
	{
		return NULL;
	}

	if (ret >= buflen)
		return NULL;

	return buf;

	#else

	char *real_path = NULL;
	assert(buf);
	buflen--; // To fit '\0'

	if (!path || (buflen <= 0))
		return NULL;

	if (!(real_path = realpath(path, NULL)))
	{
		return NULL;
	}

	if (strlen(real_path) > buflen)
	{
		free(real_path);
		return NULL;
	}

	strncpy(buf, real_path, buflen);
	buf[buflen] = '\0';
	free(real_path);

	return buf;
	#endif // _WIN32
}


