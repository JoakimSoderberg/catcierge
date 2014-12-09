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

//
// Serial port communication with the Priority 1 Design
// http://www.priority1design.com.au/
// FDX-B/HDX RFID Reader Writer with external antenna and USB port
//
// Datasheet: http://www.priority1design.com.au/rfidrw-e-usb.pdf
//

#ifndef __CATCIERGE_RFID_H__
#define __CATCIERGE_RFID_H__

#include <termios.h>
#include <signal.h>
#include <unistd.h>

#define EXAMPLE_RFID_STR "999_000000001007" //_1_0_AEC4_000000"

typedef struct catcierge_rfid_s catcierge_rfid_t;

typedef void (*catcierge_rfid_read_f)(catcierge_rfid_t *rfid, int complete, const char *data, size_t data_len, void *user);

typedef enum catcierge_rfid_state_e
{
	CAT_ERROR = -1,
	CAT_DISCONNECTED = 0,
	CAT_CONNECTED = 1,
	CAT_AWAITING_TAG = 2,
	CAT_NEED_MORE = 3
} catcierge_rfid_state_t;

const char *catcierge_rfid_error_str(int errorcode);

struct catcierge_rfid_s
{
	char name[256];
	const char *serial_path;
	int fd;
	ssize_t bytes_read;
	ssize_t offset;
	char buf[1024];
	catcierge_rfid_read_f cb;
	void *user;
	catcierge_rfid_state_t state;
};

#define RFID_IN 0
#define RFID_OUT 1
#define RFID_COUNT 2

typedef struct catcierge_rfid_context_s
{
	fd_set readfs;
	int maxfd;
	catcierge_rfid_t *rfids[RFID_COUNT];
} catcierge_rfid_context_t;

int catcierge_rfid_init(const char *name, catcierge_rfid_t *rfid, 
			const char *serial_path, catcierge_rfid_read_f read_cb, void *user);

void catcierge_rfid_destroy(catcierge_rfid_t *rfid);
int catcierge_rfid_ctx_service(catcierge_rfid_context_t *ctx);
void catcierge_rfid_ctx_set_inner(catcierge_rfid_context_t *ctx, catcierge_rfid_t *rfid);
void catcierge_rfid_ctx_set_outer(catcierge_rfid_context_t * ctx, catcierge_rfid_t *rfid);
int catcierge_rfid_open(catcierge_rfid_t *rfid);
int catcierge_rfid_write_rat(catcierge_rfid_t *rfid);
int catcierge_rfid_ctx_init(catcierge_rfid_context_t *ctx);
int catcierge_rfid_ctx_destroy(catcierge_rfid_context_t *ctx);

#endif // __CATCIERGE_RFID_H__
