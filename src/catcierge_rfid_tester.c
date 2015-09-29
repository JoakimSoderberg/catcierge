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
#include "catcierge_rfid.h"

void rfid_read_cb(catcierge_rfid_t *rfid, int complete, const char *data, size_t data_len, void *user)
{
	printf("%s Reply: %s\n", rfid->name, data);
}

int main(int argc, char **argv)
{
	catcierge_rfid_t rfidin;
	catcierge_rfid_t rfidout;
	catcierge_rfid_context_t ctx;

	catcierge_rfid_ctx_init(&ctx);

	catcierge_rfid_init("Inner", &rfidin, "/dev/ttyUSB0", rfid_read_cb, NULL);
	catcierge_rfid_ctx_set_inner(&ctx, &rfidin);
	catcierge_rfid_open(&rfidin);
	catcierge_rfid_init("Outer", &rfidout, "/dev/ttyUSB1", rfid_read_cb, NULL);
	catcierge_rfid_ctx_set_outer(&ctx, &rfidout);
	catcierge_rfid_open(&rfidout);

	while (1)
	{
		catcierge_rfid_ctx_service(&ctx);
		sleep(1);
	}

	catcierge_rfid_destroy(&rfidin);
	catcierge_rfid_destroy(&rfidout);
	catcierge_rfid_ctx_destroy(&ctx);

	return 0;
}

