
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

#define EXAMPLE_RFID_STR "999_000000001007_1_0_AEC4_000000\r\n"

typedef struct catsnatch_rfid_s catsnatch_rfid_t;

typedef void (*catsnatch_rfid_read_f)(catsnatch_rfid_t *rfid, int incomplete, const char *data);

typedef enum catsnatch_rfid_state_e
{
	CAT_ERROR = -1,
	CAT_DISCONNECTED = 0,
	CAT_CONNECTED = 1,
	CAT_AWAITING_TAG = 2,
	CAT_NEED_MORE = 3
} catsnatch_rfid_state_t;

const char *catsnatch_rfid_error_str(int errorcode);

struct catsnatch_rfid_s
{
	char name[256];
	const char *serial_path;
	int fd;
	ssize_t bytes_read;
	ssize_t offset;
	char buf[1024];
	catsnatch_rfid_read_f cb;
	catsnatch_rfid_state_t state;
};

#define RFID_IN 0
#define RFID_OUT 1
#define RFID_COUNT 2

typedef struct catsnatch_rfid_context_s
{
	fd_set readfs;
	int maxfd;
	catsnatch_rfid_t *rfids[RFID_COUNT];
} catsnatch_rfid_context_t;

int catsnatch_rfid_init(const char *name, catsnatch_rfid_t *rfid, const char *serial_path, catsnatch_rfid_read_f read_cb);
void catsnatch_rfid_destroy(catsnatch_rfid_t *rfid);
int catsnatch_rfid_ctx_service(catsnatch_rfid_context_t *ctx);
void catsnatch_rfid_ctx_set_inner(catsnatch_rfid_context_t *ctx, catsnatch_rfid_t *rfid);
void catsnatch_rfid_ctx_set_outer(catsnatch_rfid_context_t * ctx, catsnatch_rfid_t *rfid);
int catsnatch_rfid_open(catsnatch_rfid_t *rfid);
int catsnatch_rfid_write_rat(catsnatch_rfid_t *rfid);

#endif // __CATSNATCH_RFID_H__
