#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "catsnatch_log.h"

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
		//ret = -2;
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

