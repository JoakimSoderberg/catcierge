
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

int catsnatch_rfid_init(catsnatch_rfid_t *rfid, const char *serial_path, catsnatch_rfid_read_f read_cb)
{
	rfid->serial_path = serial_path;
	rfid->fd = -1;
	rfid->cb = read_cb;

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
	if ((rfid->bytes_read = read(rfid->fd, rfid->buf, sizeof(rfid->buf))) < 0)
	{
		if ((errno != EWOULDBLOCK) && (errno != EAGAIN))
		{
			fprintf(stderr, "Read error %d: %s\n", errno, strerror(errno));
			return -1;
		}
	}

	return 0;
}

int catsnatch_rfid_ctx_service(catsnatch_rfid_context_t *ctx)
{
	int res = 0;
	struct timeval tv = {0, 0};

	if (!ctx->rfid_in && !ctx->rfid_out)
	{
		return -1;
	}

	if (ctx->rfid_in && (ctx->rfid_in->fd > 0))
	{
		FD_SET(ctx->rfid_in->fd, &ctx->readfs);
	}

	if (ctx->rfid_out && (ctx->rfid_out->fd > 0))
	{
		FD_SET(ctx->rfid_out->fd, &ctx->readfs);
	}
	
	if (!(res = select(ctx->maxfd, &ctx->readfs, NULL, NULL, &tv)))
	{
		return 0;
	}

	if (ctx->rfid_in && FD_ISSET(ctx->rfid_in->fd, &ctx->readfs))
	{
		catsnatch_rfid_read(ctx->rfid_in);
	}

	if (ctx->rfid_out && FD_ISSET(ctx->rfid_out->fd, &ctx->readfs))
	{
		catsnatch_rfid_read(ctx->rfid_out);
	}

	return 0;
}

static void _set_maxfd(catsnatch_rfid_context_t *ctx)
{
	if (ctx->rfid_in && ctx->rfid_out)
	{
		ctx->maxfd = MAX(ctx->rfid_out->fd, ctx->rfid_in->fd) + 1;
	}
	else if (ctx->rfid_in)
	{
		ctx->maxfd = ctx->rfid_in->fd + 1;
	}
	else if (ctx->rfid_out)
	{
		ctx->maxfd = ctx->rfid_out->fd + 1;
	}
}

void catsnatch_rfix_ctx_set_inner(catsnatch_rfid_context_t *ctx, catsnatch_rfid_t *rfid)
{
	assert(ctx);
	ctx->rfid_in = rfid;
	_set_maxfd(ctx);
}

void catsnatch_rfid_ctx_set_outter(catsnatch_rfid_context_t * ctx, catsnatch_rfid_t *rfid)
{
	assert(ctx);
	ctx->rfid_out = rfid;
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

