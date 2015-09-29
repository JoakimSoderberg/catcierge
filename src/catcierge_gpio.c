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
#include <stdlib.h>

#include "catcierge_config.h"

#ifdef CATCIERGE_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef CATCIERGE_HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <string.h>
#include "catcierge_util.h"
#include "catcierge_log.h"

static int write_num_to_file(const char *path, int num)
{
	int ret = 0;
	char buf[16];
	int fd;
	ssize_t written;

	if ((fd = open(path, O_WRONLY)) < 0)
	{
		CATERR("Failed to open \"%s\"\n", path);
		return -1;
	}

	written = snprintf(buf, sizeof(buf), "%d", num);
	
	if (write(fd, buf, strlen(buf)) < 0)
	{
		CATERR("Failed to write \"%s\" to %s\n", buf, path);
	}

	close(fd);

	return ret;
}

int gpio_export(int pin)
{
	if (write_num_to_file("/sys/class/gpio/export", pin))
	{
		CATERR("Failed to open GPIO export for writing\n");
		return -1;
	}

	return 0;
}

int gpio_set_direction(int pin, int direction)
{
	int ret = 0;
	int fd;
	char *str;
	char path[256];
	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);

	if ((fd = open(path, O_WRONLY)) < 0)
	{
		CATERR("Failed to open %s\n", path);
		return -1;
	}

	str = direction ? "in" : "out";
	
	if (write(fd, str, strlen(str)) < 0)
	{
		CATERR("Failed to write \"%s\" to %s\n", str, path);
		//ret = -2;
	}

	close(fd);

	return ret;
}

int gpio_write(int pin, int val)
{
	char path[256];

	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);

	if (write_num_to_file(path, val))
	{
		CATERR("Failed to open GPIO export for writing\n");
		return -1;
	}

	return 0;
}

