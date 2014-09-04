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
#include <stdio.h>
#include <errno.h>
#include "catcierge_util.h"
#include "catcierge_strftime.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include "catcierge_log.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

const char *catcierge_path_sep()
{
	#ifdef _WIN32
	return "\\";
	#else
	return "/";
	#endif
}

int catcierge_make_path(const char *path)
{
	// TODO: This is kind of trivial... Do this properly instead.
	char cmd[4096];
	#ifdef WIN32
	snprintf(cmd, sizeof(cmd), "md %s", path);
	#else
	snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
	#endif
	system(cmd);

	return 0;
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
			CATLOG("Called program %s\n", command);
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
			CATLOG("Called program %s\n", command);
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

int catcierge_is_frame_obstructed(IplImage *img, int debug)
{
	CvSize size;
	int w;
	int h;
	int x;
	int y;
	int sum;
	IplImage *tmp = NULL;
	IplImage *tmp2 = NULL;
	CvRect roi = cvGetImageROI(img);

	// Get a suitable Region Of Interest (ROI)
	// in the center of the image.
	// (This should contain only the white background)
	size = cvGetSize(img);
	w = (int)(size.width * 0.5);
	h = (int)(size.height * 0.1);
	x = (size.width - w) / 2;
	y = (size.height - h) / 2;

	cvSetImageROI(img, cvRect(x, y, w, h));

	// Only covert to grayscale if needed.
	if (img->nChannels != 1)
	{
		tmp = cvCreateImage(cvSize(w, h), 8, 1);
		cvCvtColor(img, tmp, CV_BGR2GRAY);
	}
	else
	{
		tmp = img;
	}

	// Get a binary image and sum the pixel values.
	tmp2 = cvCreateImage(cvSize(w, h), 8, 1);
	cvThreshold(tmp, tmp2, 90, 255, CV_THRESH_BINARY_INV);

	sum = (int)cvSum(tmp2).val[0] / 255;

	if (debug)
	{
		printf("Sum: %d\n", sum);
		cvShowImage("obstruct", tmp2);
	}

	cvSetImageROI(img, cvRect(roi.x, roi.y, roi.width, roi.height));

	if (img->nChannels != 1)
	{
		cvReleaseImage(&tmp);
	}

	cvReleaseImage(&tmp2);

	// Spiders and other 1 pixel creatures need not bother!
	return ((int)sum > 200);
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
		CATERR("Out of memory!\n");
		return NULL;
	}

	// strtok changes its input.
	if (!(input_copy = strdup(input)))
	{
		free(list);
		*list_count = 0;
		return NULL;
	}

	s = strtok(input_copy, ",");

	for (i = 0; i < (*list_count); i++)
	{
		s = catcierge_skip_whitespace(s);

		if (!(list[i] = strdup(s)))
		{
			CATERR("Out of memory!\n");
			goto fail;
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
	if (list)
	{
		for (i = 0; i < (*list_count); i++)
		{
			if (list[i])
			{
				free(list[i]);
				list[i] = NULL;
			}
		}

		free(list);
	}

	if (input_copy)
	{
		free(input_copy);
	}

	*list_count = 0;

	return NULL;
}

