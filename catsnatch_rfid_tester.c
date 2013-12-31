
#include <stdio.h>
#include <stdlib.h>
#include "catsnatch_rfid.h"


void rfid_read_cb(catsnatch_rfid_t *rfid, int incomplete, const char *data)
{
	printf("%s Reply: %s\n", rfid->name, data);
}



int main(int argc, char **argv)
{
	catsnatch_rfid_t rfidin;
	catsnatch_rfid_t rfidout;
	catsnatch_rfid_context_t ctx;

	catsnatch_rfid_ctx_init(&ctx);

	catsnatch_rfid_init("Inner", &rfidin, "/dev/ttyUSB0", rfid_read_cb);
	catsnatch_rfid_ctx_set_inner(&ctx, &rfidin);
	catsnatch_rfid_open(&rfidin);
	catsnatch_rfid_init("Outer", &rfidout, "/dev/ttyUSB1", rfid_read_cb);
	catsnatch_rfid_ctx_set_outer(&ctx, &rfidout);
	catsnatch_rfid_open(&rfidout);

	while (1)
	{
		catsnatch_rfid_ctx_service(&ctx);
		sleep(1);
	}

	catsnatch_rfid_destroy(&rfidin);
	catsnatch_rfid_destroy(&rfidout);
	catsnatch_rfid_ctx_destroy(&ctx);

	return 0;
}

