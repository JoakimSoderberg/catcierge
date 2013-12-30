
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h> 
#include "catsnatch_rfid.h"

#define _POSIX_SOURCE 1 // POSIX compliant source.

const char *catsnatch_rfid_errors[] = 
{
	"Command not understood",				// ?0
	"Tag not present",						// ?1
	"Tag failure to Read/Write",			// ?2
	"Access to Block 0 not allowed",		// ?3
	"Page address invalid for this tag"		// ?4
};

const char *catsnatch_rfid_error_str(int errorcode)
{
	return catsnatch_rfid_errors[errorcode % (sizeof(catsnatch_rfid_errors) / sizeof(char *))];
}

int catsnatch_rfid_init(const char *name, catsnatch_rfid_t *rfid, const char *serial_path, catsnatch_rfid_read_f read_cb)
{
	strcpy(rfid->name, name);
	rfid->serial_path = serial_path;
	rfid->fd = -1;
	rfid->cb = read_cb;
	rfid->bytes_read = 0;
	rfid->offset = 0;

	return 0;
}

void catsnatch_rfid_destroy(catsnatch_rfid_t *rfid)
{
	assert(rfid);

	if (rfid->fd > 0)
	{
		close(rfid->fd);
	}
}

static int catsnatch_rfid_read(catsnatch_rfid_t *rfid)
{
	int is_error = 0;
	int errorcode;
	const char *error_msg = NULL;

	if ((rfid->bytes_read = read(rfid->fd, rfid->buf, 
								sizeof(rfid->buf))) < 0)
	{
		if ((errno != EWOULDBLOCK) && (errno != EAGAIN))
		{
			fprintf(stderr, "Read error %d: %s\n", errno, strerror(errno));
			return -1;
		}

		// TODO: Add CAT_NEED_MORE state here and handle that.
		return -1;
	}

	if (rfid->bytes_read == 0)
	{
		// TODO: EOF, do something special here?
		return 0;
	}

	// Check for error.
	if (rfid->buf[0] == '?')
	{
		is_error = 1;
		errorcode = atoi(&rfid->buf[1]);
		error_msg = catsnatch_rfid_error_str(errorcode);

		fprintf(stderr, "%s RFID reader: Error %d on read: %s\n", 
				rfid->name, errorcode, error_msg);
	}

	if (rfid->state == CAT_CONNECTED)
	{
		// We have issued a RAT (Read Animal Tag) command to the
		// RFID reader, and are awaiting an OK.
		if (!strncmp(rfid->buf, "OK", 2))
		{
			printf("Listening for Animal Tags on %s RFID reader", rfid->name);
			rfid->state = CAT_AWAITING_TAG;
			return 0;
		}
		else
		{
			fprintf(stderr, "Failed to issue RAT (Read Animal Tag) command.\n");
			rfid->state = CAT_ERROR;
			return -1;
		}
	}
	else if (rfid->state == CAT_AWAITING_TAG)
	{
		char *end = strstr(rfid->buf, "\r\n");
		int found = (end != NULL);

		*end = '\0';
		
		if (!found)
		{
			fprintf(stderr, "Got incomplete tag string: %s\n", rfid->buf);
		}
		else
		{
			rfid->cb(rfid, rfid->buf);
			return 0;
		}
	}

	return -1;
}

int catsnatch_rfid_ctx_service(catsnatch_rfid_context_t *ctx)
{
	int i;
	int res = 0;
	struct timeval tv = {0, 0};

	if (!ctx->rfids[RFID_IN] && !ctx->rfids[RFID_OUT])
	{
		return -1;
	}

	for (i = 0; i < 2; i++)
	{
		if (ctx->rfids[i] && (ctx->rfids[i]->fd > 0))
		{
			FD_SET(ctx->rfids[i]->fd, &ctx->readfs);
		}
	}
	
	if (!(res = select(ctx->maxfd, &ctx->readfs, NULL, NULL, &tv)))
	{
		return 0; // No input available.
	}

	for (i = 0; i < 2; i++)
	{
		if (ctx->rfids[i] && FD_ISSET(ctx->rfids[i]->fd, &ctx->readfs))
		{
			if (catsnatch_rfid_read(ctx->rfids[i]) < 0)
			{

			}
		}
	}

	return 0;
}

static void _set_maxfd(catsnatch_rfid_context_t *ctx)
{
	if (ctx->rfids[RFID_IN] && ctx->rfids[RFID_OUT])
	{
		ctx->maxfd = MAX(ctx->rfids[RFID_IN]->fd, ctx->rfids[RFID_OUT]->fd) + 1;
	}
	else if (ctx->rfids[RFID_IN])
	{
		ctx->maxfd = ctx->rfids[RFID_IN]->fd + 1;
	}
	else if (ctx->rfids[RFID_OUT])
	{
		ctx->maxfd = ctx->rfids[RFID_OUT]->fd + 1;
	}
}

void catsnatch_rfix_ctx_set_inner(catsnatch_rfid_context_t *ctx, catsnatch_rfid_t *rfid)
{
	assert(ctx);
	ctx->rfids[RFID_IN] = rfid;
	_set_maxfd(ctx);
}

void catsnatch_rfid_ctx_set_outter(catsnatch_rfid_context_t * ctx, catsnatch_rfid_t *rfid)
{
	assert(ctx);
	ctx->rfids[RFID_OUT] = rfid;
	_set_maxfd(ctx);
}

int catsnatch_rfid_open(catsnatch_rfid_t *rfid)
{
	struct termios options;
	assert(rfid);

	if ((rfid->fd = open(rfid->serial_path, O_RDWR | O_NOCTTY | O_NONBLOCK /*O_NDELAY*/)) < 0)
	{
		fprintf(stderr, "Failed top open RFID serial port %s\n", rfid->serial_path);
		return -1;
	}

	// Non-blocking.
	//fcntl(rfid->fd, F_SETFL, FNDELAY);

	fcntl(rfid->fd, F_SETFL, FASYNC);

	tcgetattr(rfid->fd, &options);

	// Set baud rate.
	cfsetispeed(&options, B9600);
	cfsetospeed(&options, B9600);

	/*
	// Set the Charactor size
	options.c_cflag &= ~CSIZE; // Mask the character size bits.
	options.c_cflag |= CS8;    // Select 8 data bits.
	*/
	// Set parity - No Parity (8N1).
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;

	options.c_oflag = 0; // Raw output.
	options.c_lflag = ICANON; // Cannonical processing. Disable echo, and don't send signals.

	options.c_iflag = IGNPAR // Ignore bytes with parity error.
					| ICRNL; // Convert carriage return to newline.

	// Flush the line and set the options.
	tcflush(rfid->fd, TCIFLUSH);
	tcsetattr(rfid->fd, TCSANOW, &options);

	return 0;
}

