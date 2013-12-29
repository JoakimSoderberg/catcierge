
#ifndef __CATSNATCH_RFID_H__
#define __CATSNATCH_RFID_H__

#include <termios.h>
#include <signal.h>
#include <unistd.h> 

typedef void (*catsnatch_rfid_read_f)(int inner, const char *id);
//typedef void (*catsnatch_rfid_signal_f)(int status);

typedef struct catsnatch_rfid_s
{
	const char *serial_path;
	int fd;
	ssize_t bytes_read;
	char buf[1024];
	catsnatch_rfid_read_f cb;
} catsnatch_rfid_t;

typedef struct catsnatch_rfid_context_s
{
	fd_set readfs;
	int maxfd;
	catsnatch_rfid_t *rfid_in;
	catsnatch_rfid_t *rfid_out;
} catsnatch_rfid_context_t;

#endif // __CATSNATCH_RFID_H__
