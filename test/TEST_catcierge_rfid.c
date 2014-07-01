
#include <catcierge_config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_fsm.h"
#include "minunit.h"
#include "catcierge_test_config.h"
#include "catcierge_test_helpers.h"
#include "catcierge_args.h"
#include "catcierge_types.h"
#include "catcierge_test_common.h"

#ifdef CATCIERGE_HAVE_PTY_H
#include <pty.h>
#endif

#ifdef CATCIERGE_HAVE_UTIL_H
#include <util.h>
#endif

#ifndef _WIN32

char *run_pseudo_console_tests()
{
	int master;
	int slave;
	char *slave_name = NULL;
	struct termios options;
	int ret;

	ret = openpty(&master, &slave, NULL, NULL, NULL);
	mu_assert("Failed to create pseudo terminal", ret == 0);

	slave_name = strdup(ttyname(slave));
	mu_assert("Failed to get slave name", slave_name);
	catcierge_test_STATUS("Slave tty: %s\n", slave_name);

	{
		const char buf[] = "RAT\r\n";
		char result[128];
		ssize_t bytes_read;
		ssize_t bytes = write(master, buf, sizeof(buf));
		catcierge_test_STATUS("Wrote \"%s\" (%d bytes) to master\n", buf, bytes);

		bytes_read = read(slave, result, sizeof(result));
		result[bytes_read] = '\0';
		catcierge_test_STATUS("Read \"%s\" (%d bytes) from slave\n", result, bytes_read);
		mu_assert("Failed to read correct number of bytes from slave", bytes_read == 4);
	}

	close(master);
	close(slave);
	free(slave_name);

	return NULL;
}

static void rfid_read_cb(catcierge_rfid_t *rfid,
				int incomplete, const char *data, void *user)
{
	catcierge_test_STATUS("Slave received: %s\n", data);
}

static char *read_rfid_master(catcierge_rfid_context_t *ctx,
		int master, char *expected_msg, char *expected)
{
	char buf[4096];
	ssize_t bytes_read = read(master, buf, sizeof(buf));
	catcierge_test_STATUS("RFID <- Slave:\n\"%s\"", buf);

	mu_assert(expected_msg, !strncmp(expected, buf, bytes_read));

	return NULL;
}

static char *write_rfid_master(int master, const char *buf)
{
	ssize_t bytes_to_write = strlen(buf);
	ssize_t bytes_written = write(master, buf, bytes_to_write);
	catcierge_test_STATUS("RFID -> Slave:\n\"%s\"", buf);
	mu_assert("Unexpected write amount", bytes_to_write != bytes_written);

	return NULL;
}

static int init_rfid_stuff(catcierge_rfid_context_t *ctx,
		catcierge_rfid_t *rfidin, char *slave_name)
{
	catcierge_rfid_ctx_init(ctx);

	catcierge_rfid_init("Test inner", rfidin, slave_name, rfid_read_cb, NULL);
	catcierge_rfid_ctx_set_inner(ctx, rfidin);
	catcierge_rfid_open(rfidin);

	return 0;
}

char *run_rfid_tests()
{
	int master;
	int slave;
	char *slave_name = NULL;
	int ret;
	char *e = NULL;
	ssize_t bytes_read;

	ret = openpty(&master, &slave, NULL, NULL, NULL);
	mu_assert("Failed to create pseudo terminal", ret == 0);

	slave_name = strdup(ttyname(slave));
	mu_assert("Failed to get slave name", slave_name);
	catcierge_test_STATUS("Slave tty: %s\n", slave_name);

	// Test OK RAT.
	{
		catcierge_rfid_context_t ctx;
		catcierge_rfid_t rfidin;
		init_rfid_stuff(&ctx, &rfidin, slave_name);

		catcierge_rfid_ctx_service(&ctx);
		mu_assert("Expected RFID state == CAT_CONNECTED",
			rfidin.state == CAT_CONNECTED);

		if ((e = read_rfid_master(&ctx, master, "Expected RAT", "RAT\r\n")))
			return e;

		write_rfid_master(master, "OK\r\n");
		catcierge_rfid_ctx_service(&ctx);
		mu_assert("Expected RFID state == CAT_AWAITING_TAG",
			rfidin.state == CAT_AWAITING_TAG);

		write_rfid_master(master, EXAMPLE_RFID_STR"\r\n");
		catcierge_rfid_ctx_service(&ctx);

		catcierge_rfid_destroy(&rfidin);
		catcierge_rfid_ctx_destroy(&ctx);

		catcierge_test_SUCCESS("Test OK RAT\n");
	}

	// Test failed RAT.
	{
		catcierge_rfid_context_t ctx;
		catcierge_rfid_t rfidin;
		init_rfid_stuff(&ctx, &rfidin, slave_name);

		catcierge_rfid_ctx_service(&ctx);
		mu_assert("Expected RFID state == CAT_CONNECTED",
			rfidin.state == CAT_CONNECTED);

		if ((e = read_rfid_master(&ctx, master, "Expected RAT", "RAT\r\n")))
			return e;

		write_rfid_master(master, "?1\r\n");
		catcierge_rfid_ctx_service(&ctx);
		mu_assert("Expected RFID state == CAT_AWAITING_TAG",
			rfidin.state == CAT_AWAITING_TAG);

		catcierge_rfid_destroy(&rfidin);
		catcierge_rfid_ctx_destroy(&ctx);

		catcierge_test_SUCCESS("Test failed RAT\n");
	}

	close(master);
	close(slave);
	free(slave_name);

	return NULL;
}

char *run_double_tests()
{
	int in_master;
	int in_slave;
	int out_master;
	int out_slave;
	char *in_slave_name = NULL;
	char *out_slave_name = NULL;
	int ret;
	char *e = NULL;
	ssize_t bytes_read;

	// Create inner pseudo terminal.
	{
		ret = openpty(&in_master, &in_slave, NULL, NULL, NULL);
		mu_assert("Failed to create inner pseudo terminal", ret == 0);
	
		in_slave_name = strdup(ttyname(in_slave));
		mu_assert("Failed to get inner slave name", in_slave_name);
		catcierge_test_STATUS("Inner slave tty: %s\n", in_slave_name);
	}

	// Create outer pseudo terminal.
	{
		ret = openpty(&out_master, &out_slave, NULL, NULL, NULL);
		mu_assert("Failed to create outer pseudo terminal", ret == 0);

		out_slave_name = strdup(ttyname(out_slave));
		mu_assert("Failed to get outer slave name", out_slave_name);
		catcierge_test_STATUS("Outer slave tty: %s\n", out_slave_name);
	}

	// Init RFID contexts.
	{
		catcierge_rfid_context_t ctx;
		catcierge_rfid_t rfidin;
		catcierge_rfid_t rfidout;

		catcierge_rfid_ctx_init(&ctx);

		catcierge_rfid_init("Test inner", &rfidin, in_slave_name, rfid_read_cb, NULL); 
		catcierge_rfid_ctx_set_inner(&ctx, &rfidin);
		catcierge_rfid_open(&rfidin);

		catcierge_rfid_init("Test inner", &rfidout, out_slave_name, rfid_read_cb, NULL);
		catcierge_rfid_ctx_set_outer(&ctx, &rfidout);
		catcierge_rfid_open(&rfidout);

		// TODO: Do some more tests here.
		catcierge_rfid_ctx_service(&ctx);

		catcierge_rfid_destroy(&rfidin);
		catcierge_rfid_destroy(&rfidout);
		catcierge_rfid_ctx_destroy(&ctx);	
	}

	close(in_master);
	close(out_master);
	close(in_slave);
	close(out_slave);

	free(out_slave_name);
	free(in_slave_name);

	return NULL;	
}

#endif // !_WIN32

int TEST_catcierge_rfid(int argc, char **argv)
{
	int ret = 0;
	char *e = NULL;
	
	catcierge_test_HEADLINE("TEST_catcierge_rfid");

	#ifndef _WIN32

	CATCIERGE_RUN_TEST((e = run_pseudo_console_tests()),
		"Run pseudo console tests",
		"Pseudo console tests", &ret);

	if (ret)
	{
		catcierge_test_SKIPPED("Failed to run pseudo console tests. RFID tests won't be reliable\n");
		return 0;
	}

	CATCIERGE_RUN_TEST((e = run_rfid_tests()),
		"Run RFID tests",
		"RFID tests", &ret);

	CATCIERGE_RUN_TEST((e = run_double_tests()),
		"Run RFID double tests",
		"RFID double tests", &ret);

	#else
	catcierge_test_SKIPPED("RFID not supported on windows!\n");
	#endif // !_WIN32

	return ret;
}
