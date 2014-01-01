
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
#include <sys/socket.h>
#include "catsnatch_rfid.h"
#include "catsnatch_log.h"

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
	rfid->state = CAT_DISCONNECTED;

	return 0;
}

void catsnatch_rfid_destroy(catsnatch_rfid_t *rfid)
{
	assert(rfid);

	if (rfid->fd > 0)
	{
		close(rfid->fd);
		rfid->fd = -1;
	}

	CATLOG("Disconnected %s RFID reader (%s)\n", rfid->name, rfid->serial_path);

	rfid->state = CAT_DISCONNECTED;
}

static int catsnatch_rfid_read(catsnatch_rfid_t *rfid)
{
	int ret = 0;
	int is_error = 0;
	int errorcode;
	const char *error_msg = NULL;

	if ((rfid->bytes_read = read(rfid->fd, rfid->buf, 
								sizeof(rfid->buf))) < 0)
	{
		if ((errno != EWOULDBLOCK) && (errno != EAGAIN))
		{
			CATERR("%s RFID Reader: Read error %d, %s\n", rfid->name, errno, strerror(errno));
			ret = -1;
			goto fail;
		}

		CATERR("%s RFID Reader: Need more\n");
		// TODO: Add CAT_NEED_MORE state here and handle that.
		return -1;
	}

	rfid->buf[rfid->bytes_read] = '\0';
	CATLOG("%s RFID Reader: %ld bytes: %s\n", rfid->name, rfid->bytes_read, rfid->buf);

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

		CATERR("%s RFID reader: error %d on read, %s\n", 
				rfid->name, errorcode, error_msg);
	}

	if (rfid->state == CAT_CONNECTED)
	{
		CATLOG("%s RFID Reader: Started listening for cats on %s\n", rfid->name, rfid->serial_path);
		rfid->state = CAT_AWAITING_TAG;
	}
	else if (rfid->state == CAT_AWAITING_TAG)
	{
		if (!is_error)
		{
			int incomplete = (rfid->bytes_read < 17);
			rfid->cb(rfid, incomplete, rfid->buf);
		}
		else
		{
			CATERR("%s RFID Reader: Failed reading tag, %s\n", rfid->name, error_msg);
		}
	}
	else
	{
		CATERR("%s RFID Reader: Invalid state on read, %d\n", rfid->name, rfid->state);
	}

fail:
	rfid->bytes_read = 0;
	rfid->offset = 0;
	return ret;
}

static void _set_maxfd(catsnatch_rfid_context_t *ctx)
{
	int i;
	int max_fd = 0;

	for (i = 0; i < RFID_COUNT; i++)
	{
		if (ctx->rfids[i] && (ctx->rfids[i]->fd > max_fd))
		{
			max_fd = ctx->rfids[i]->fd;
		}
	}

	ctx->maxfd = max_fd + 1;
}

int catsnatch_rfid_ctx_init(catsnatch_rfid_context_t *ctx)
{
	memset(ctx, 0, sizeof(catsnatch_rfid_context_t));
	return 0;
}

int catsnatch_rfid_ctx_destroy(catsnatch_rfid_context_t *ctx)
{
	memset(ctx, 0, sizeof(catsnatch_rfid_context_t));
	return 0;
}

int catsnatch_rfid_ctx_service(catsnatch_rfid_context_t *ctx)
{
	int i;
	int res = 0;
	struct timeval tv = {0, 0};

	_set_maxfd(ctx);

	if (!ctx->rfids[RFID_IN] && !ctx->rfids[RFID_OUT])
	{
		return -1;
	}

	FD_ZERO(&ctx->readfs);

	for (i = 0; i < RFID_COUNT; i++)
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

	for (i = 0; i < RFID_COUNT; i++)
	{
		if (ctx->rfids[i] && FD_ISSET(ctx->rfids[i]->fd, &ctx->readfs))
		{
			if (catsnatch_rfid_read(ctx->rfids[i]) < 0)
			{
				return -1;
			}
		}
	}

	return 0;
}

void catsnatch_rfid_ctx_set_inner(catsnatch_rfid_context_t *ctx, catsnatch_rfid_t *rfid)
{
	assert(ctx);
	ctx->rfids[RFID_IN] = rfid;
	_set_maxfd(ctx);
}

void catsnatch_rfid_ctx_set_outer(catsnatch_rfid_context_t *ctx, catsnatch_rfid_t *rfid)
{
	assert(ctx);
	ctx->rfids[RFID_OUT] = rfid;
	_set_maxfd(ctx);
}

int catsnatch_rfid_write_rat(catsnatch_rfid_t *rfid)
{
	const char buf[] = "RAT\r\n";
	ssize_t bytes = write(rfid->fd, buf, sizeof(buf));

	CATLOG("%s RFID Reader: Sent RAT request\n", rfid->name);

	if (bytes < 0)
	{
		CATERR("%s RFID Reader: Failed to write %d, %s\n", rfid->name, errno, strerror(errno));
		return -1;
	}

	return 0;
}

int catsnatch_rfid_open(catsnatch_rfid_t *rfid)
{
	struct termios options;
	assert(rfid);

	if ((rfid->fd = open(rfid->serial_path, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
	{
		CATERR("%s RFID Reader: Failed to open RFID serial port %s\n", rfid->name, rfid->serial_path);
		return -1;
	}

	CATLOG("%s RFID Reader: Opened serial port %s on fd %d\n", 
			rfid->name, rfid->serial_path, rfid->fd);

	fcntl(rfid->fd, F_SETFL, 0);

	memset(&options, 0, sizeof(options));

	// Set baud rate.
	cfsetispeed(&options, B9600);
	cfsetospeed(&options, B9600);

	options.c_iflag = 0;
    options.c_oflag = 0; // Raw output.
    options.c_cflag = CS8 | CREAD | CLOCAL;
    options.c_lflag = 0;

	options.c_cc[VTIME]	= 0;
	options.c_cc[VMIN]	= 1;

	// Flush the line and set the options.
	tcflush(rfid->fd, TCIFLUSH);
	tcsetattr(rfid->fd, TCSANOW, &options);

	// Set the reader in RAT mode.
	if (catsnatch_rfid_write_rat(rfid))
	{
		CATERR("%s RFID Reader: Failed to write RAT command\n", rfid->name);
		catsnatch_rfid_destroy(rfid);
		return -1;
	}

	rfid->state = CAT_CONNECTED;

	return 0;
}

