
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "catsnatch_log.h"

void catsnatch_execute(char *command, char *fmt, ...)
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
			cur_opt = atoi(command);
			while (*command && isdigit(*command)) command++;

			if ((cur_opt >= 0) && (cur_opt < count))
			{
				strcpy(&buf[i], extras[cur_opt]);
				i += strlen(extras[cur_opt]);
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
