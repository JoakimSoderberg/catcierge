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
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include "catcierge_log.h"

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
	pid_t pid;
	int cur_opt = 0;
	char *argv[3] = {0};
	char *extras[32];
	int count = 0;
	va_list args;

	if (!command)
		return;

	argv[0] = "/bin/sh";
	argv[1] = "-c";

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

					if (!getcwd(cwd, sizeof(cwd)))
					{
						strcpy(&buf[i], cwd);
						i += strlen(cwd);
					}
				}
				else
				{
					// Otherwise we expect a digit that refers to
					// the given variables in "extras".
					// %0, %1 .. and so on.
					cur_opt = atoi(command);
					while (*command && isdigit(*command)) command++;

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

	argv[2] = buf;

	// Run the command.

	// Fork a child process.
	if ((pid = fork()) < 0) 
	{    
		CATERR("Forking child process failed: %d, %s\n", errno, strerror(errno));
		exit(1);
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
		CATLOG("Called program %s\n", buf);
	}
}
