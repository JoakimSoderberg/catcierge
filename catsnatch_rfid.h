
#ifndef __CATSNATCH_RFID_H__
#define __CATSNATCH_RFID_H__
///
/// Serial port communication with the Priority 1 Design
/// http://www.priority1design.com.au/
/// FDX-B/HDX RFID Reader Writer with external antenna and USB port
///
/// Datasheet: http://www.priority1design.com.au/rfidrw-e-usb.pdf
///

#include <termios.h>
#include <signal.h>
#include <unistd.h> 

typedef struct catsnatch_rfid_s catsnatch_rfid_t;

typedef void (*catsnatch_rfid_read_f)(catsnatch_rfid_t *rfid, const char *cat_tag);

typedef enum catsnatch_rfid_state_e
{
	CAT_ERROR = -1,
	CAT_DISCONNECTED = 0,
	CAT_CONNECTED = 1,
	CAT_AWAITING_TAG = 2,
	CAT_NEED_MORE = 3
} catsnatch_rfid_state_t;

const char *catsnatch_rfid_error_str(int errorcode);

typedef struct catsnatch_rfid_s
{
	char name[256];
	const char *serial_path;
	int fd;
	ssize_t bytes_read;
	ssize_t offset;
	char buf[1024];
	catsnatch_rfid_read_f cb;
	catsnatch_rfid_state_t state;
} catsnatch_rfid_t;

#define RFID_IN 0
#define RFID_OUT 1

typedef struct catsnatch_rfid_context_s
{
	fd_set readfs;
	int maxfd;
	catsnatch_rfid_t *rfids[2];
} catsnatch_rfid_context_t;

#endif // __CATSNATCH_RFID_H__
