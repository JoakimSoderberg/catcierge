#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_config.h"
#include "catcierge_fsm.h"
#include "minunit.h"
#include "catcierge_test_config.h"
#include "catcierge_test_helpers.h"
#include "catcierge_args.h"
#include "catcierge_types.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include "catcierge_test_common.h"

#ifdef CATCIERGE_HAVE_PTY_H
#include <pty.h>
#endif

#ifdef CATCIERGE_HAVE_UTIL_H
#include <util.h>
#endif

#ifdef WITH_RFID
static char *read_rfid_master(int master, char *expected_msg, char *expected)
{
	char buf[4096];
	ssize_t bytes_read = read(master, buf, sizeof(buf) - 1);
	catcierge_test_STATUS("RFID <- Slave:\n\"%s\"", buf);

	mu_assert(expected_msg, (bytes_read > 0) && !strncmp(expected, buf, bytes_read));

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

typedef struct rfid_pseudo_test_conf_s
{
	const char *description;
	const char *allowed_list;
	const char *outer_tag;
	const char *inner_tag;
	catcierge_state_func_t state;
	match_direction_t dir;
} rfid_pseudo_test_conf_t;

static char *run_rfid_pseudo_tests(rfid_pseudo_test_conf_t *conf)
{
	size_t i;
	catcierge_grb_t grb;
	catcierge_args_t *args;
	int in_slave = 0;
	int in_master = 0;
	int out_slave = 0;
	int out_master = 0;
	char *in_slave_name = NULL;
	char *out_slave_name = NULL;
	int ret = 0;
	char *e = NULL;
	char *return_message = NULL;

	memset(&grb, 0, sizeof(grb));
	args = &grb.args;

	// Create inner pseudo terminal.
	{
		ret = openpty(&in_master, &in_slave, NULL, NULL, NULL);
		mu_assertf("Failed to create inner pseudo terminal", ret == 0);
	
		in_slave_name = strdup(ttyname(in_slave));
		mu_assertf("Failed to get inner slave name", in_slave_name);
		catcierge_test_STATUS("Inner slave tty: %s\n", in_slave_name);
	}

	// Create outer pseudo terminal.
	{
		ret = openpty(&out_master, &out_slave, NULL, NULL, NULL);
		mu_assertf("Failed to create outer pseudo terminal", ret == 0);

		out_slave_name = strdup(ttyname(out_slave));
		mu_assertf("Failed to get outer slave name", out_slave_name);
		catcierge_test_STATUS("Outer slave tty: %s\n", out_slave_name);
	}

	catcierge_grabber_init(&grb);
	grb.running = 1;

	if (conf->allowed_list)
	{
		catcierge_create_rfid_allowed_list(args, conf->allowed_list);
	}

	args->saveimg = 0;
	args->match_time = 1; // Set this so that the RFID check is triggered.
	args->lock_on_invalid_rfid = 1;
	args->rfid_inner_path = in_slave_name;
	args->rfid_outer_path = out_slave_name;
	args->matcher = "template";
	args->matcher_type = MATCHER_TEMPLATE;
	args->templ.match_flipped = 1;
	args->templ.match_threshold = 0.8;
	args->templ.snout_paths[0] = CATCIERGE_SNOUT1_PATH;
	args->templ.snout_count++;
	args->templ.snout_paths[1] = CATCIERGE_SNOUT2_PATH;
	args->templ.snout_count++;

	catcierge_init_rfid_readers(&grb);

	// Ensure the RFID readers have been initialized.
	{
		if ((e = read_rfid_master(in_master, "Expected RAT from inner", "RAT\r\n")))
			return e;

		write_rfid_master(in_master, "OK\r\n");

		if ((e = read_rfid_master(out_master, "Expected RAT from outer", "RAT\r\n")))
			return e;

		write_rfid_master(out_master, "OK\r\n");
	}

	mu_assert("Failed to service RFID", !catcierge_rfid_ctx_service(&grb.rfid_ctx));

	if (catcierge_matcher_init(&grb.matcher, (catcierge_matcher_args_t *)&args->templ))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_test_STATUS("Initialized template matcher");

	catcierge_set_state(&grb, catcierge_state_waiting);

	// Normal match.
	{
		// Emulate the outer RFID reader detecting a tag.
		if (conf->outer_tag)
		{
			catcierge_test_STATUS("Emulate outer tag: %s", conf->outer_tag);
			write_rfid_master(out_master, conf->outer_tag);
			mu_assert("Failed to service RFID", !catcierge_rfid_ctx_service(&grb.rfid_ctx));
			//sleep(1);
		}

		load_test_image_and_run(&grb, 1, 1);
		mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));
	
		// Match against 4 pictures, and decide the lockout status.
		for (i = 1; i <= 4; i++)
		{
			load_test_image_and_run(&grb, 1, i);
		}
	
		mu_assert("Expected KEEP OPEN state", (grb.state == catcierge_state_keepopen));

		// Emulate the inner RFID reader detecting a tag.
		if (conf->inner_tag)
		{
			catcierge_test_STATUS("Emulate inner tag: %s", conf->inner_tag);
			write_rfid_master(in_master, conf->inner_tag);
			mu_assert("Failed to service RFID", !catcierge_rfid_ctx_service(&grb.rfid_ctx));
		}

		// Give it a clear frame so that it will stop
		load_test_image_and_run(&grb, 1, 5);
	}

	catcierge_test_STATUS("Expecting \"%s\" got \"%s\"",
		catcierge_get_state_string(conf->state),
		catcierge_get_state_string(grb.state));

	mu_assert("Unexpected state", (grb.state == conf->state));

	catcierge_test_STATUS("Expection directon \"%s\" got \"%s\"",
		catcierge_get_direction_str(conf->dir),
		catcierge_get_direction_str(grb.rfid_direction));

	mu_assert("Unexpected RFID direction", grb.rfid_direction == conf->dir);

cleanup:
	if (in_master) close(in_master);
	if (out_master) close(out_master);
	if (in_slave) close(in_slave);
	if (out_slave) close(out_slave);

	if (out_slave_name) free(out_slave_name);
	if (in_slave_name) free(in_slave_name);

	catcierge_template_matcher_destroy(&grb.matcher);
	catcierge_grabber_destroy(&grb);

	return return_message;
}


typedef struct rfid_test_conf_s
{
	const char *description;
	int check_rfid;
	const char *inner_path;
	const char *outer_path;
	int inner_valid_rfid;
	int outer_valid_rfid;
	match_direction_t direction;
} rfid_test_conf_t;

static char* run_rfid_tests(rfid_test_conf_t *conf)
{
	int i;
	catcierge_grb_t grb;
	catcierge_args_t *args;
	args = &grb.args;

	catcierge_grabber_init(&grb);
	grb.running = 1;

	args->saveimg = 0;
	args->match_time = 1; // Set this so that the RFID check is triggered.
	args->lock_on_invalid_rfid = conf->check_rfid;
	args->rfid_inner_path = conf->inner_path;
	args->rfid_outer_path = conf->outer_path;
	grb.rfid_in_match.is_allowed = conf->inner_valid_rfid;
	grb.rfid_out_match.is_allowed = conf->outer_valid_rfid;
	grb.rfid_direction = conf->direction;
	if (conf->direction != MATCH_DIR_UNKNOWN)
	{
		grb.rfid_in_match.triggered = 1;
		grb.rfid_out_match.triggered = 1;
	}
	args->matcher = "template";
	args->matcher_type = MATCHER_TEMPLATE;
	args->templ.match_flipped = 1;
	args->templ.match_threshold = 0.8;
	args->templ.snout_paths[0] = CATCIERGE_SNOUT1_PATH;
	args->templ.snout_count++;
	args->templ.snout_paths[1] = CATCIERGE_SNOUT2_PATH;
	args->templ.snout_count++;

	// Note! We don't want to init the RFID readers for real.
	// Instead we simply set the result structs manually to test
	// the lockout logic.
	//catcierge_init_rfid_readers(&grb);

	if (catcierge_matcher_init(&grb.matcher, (catcierge_matcher_args_t *)&args->templ))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_set_state(&grb, catcierge_state_waiting);

	// Do a normal match. But also make sure the RFID check is performed.
	// This is done by setting args->match_time, so that the rematch timer
	// will let the RFID check run.
	{
		// This is the initial image that obstructs the frame
		// and triggers the matching.
		load_test_image_and_run(&grb, 1, 1);
		mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));

		// Match against 4 pictures, and decide the lockout status.
		for (i = 1; i <= 4; i++)
		{
			load_test_image_and_run(&grb, 1, i);
		}

		mu_assert("Expected KEEP OPEN state", (grb.state == catcierge_state_keepopen));

		// Give it a clear frame so that it will stop
		load_test_image_and_run(&grb, 1, 5);

		if (!conf->check_rfid)
		{
			mu_assert("Expected WAITING state", (grb.state == catcierge_state_keepopen));
		}
		else
		{
			if (conf->direction == MATCH_DIR_OUT
				|| (conf->inner_path && conf->inner_valid_rfid)
				|| (conf->outer_path && conf->outer_valid_rfid))
			{
				mu_assert("Expected WAITING state", (grb.state == catcierge_state_keepopen));
			}
			else
			{
				if (conf->inner_path || conf->outer_path)
				{
					mu_assert("Expected LOCKOUT state", (grb.state == catcierge_state_lockout));
				}
				else
				{
					mu_assert("Expected WAITING state", (grb.state == catcierge_state_keepopen));
				}
			}
		}
	}

	catcierge_matcher_destroy(&grb.matcher);
	catcierge_grabber_destroy(&grb);

	return NULL;
}
#endif // WITH_RFID

int TEST_catcierge_fsm_rfid(int argc, char **argv)
{
	int ret = 0;
	char *e = NULL;

	#if WITH_RFID
	// Create dummy serial ports that emulate the RFID readers
	// so we can test the actual callbacks and receiving data
	// from them.
	{
		int i;

		rfid_pseudo_test_conf_t confs[] =
		{
			{
				"No RFIDs allowed",
				NULL, 
				"999_000000001007", "999_000000001007",
				catcierge_state_lockout,
				MATCH_DIR_IN
			},
			{
				"RFIDs allowed",
				"999_000000001007", 
				"999_000000001007", "999_000000001007",
				catcierge_state_keepopen,
				MATCH_DIR_IN
			},
			{
				"RFIDs allowed, incomplete + incomplete",
				"999_000000001007", 
				"999_000000001", "999_000000",
				catcierge_state_lockout,
				MATCH_DIR_IN
			},
			{
				"RFIDs allowed, incomplete + complete",
				"999_000000001007", 
				"999_000000001", "999_000000001007",
				catcierge_state_keepopen,
				MATCH_DIR_IN
			},
			{
				"RFIDs allowed, complete + complete",
				"999_000000001007", 
				"999_000000001007", "999_000000001007",
				catcierge_state_keepopen,
				MATCH_DIR_IN
			},
			{
				"RFIDs allowed, complete + incomplete",
				"999_000000001007", 
				"999_000000001007", "999_000000",
				catcierge_state_keepopen,
				MATCH_DIR_IN
			}
			,
			{
				"RFIDs allowed, no RFID detection",
				"999_000000001007", 
				NULL, NULL,
				catcierge_state_lockout,
				MATCH_DIR_UNKNOWN
			}
		};

		#define RFID_PSEUDO_TEST_COUNT sizeof(confs) / sizeof(rfid_pseudo_test_conf_t)

		for (i = 0; i < RFID_PSEUDO_TEST_COUNT; i++)
		{
			CATCIERGE_RUN_TEST((e = run_rfid_pseudo_tests(&confs[i])),
				confs[i].description,
				confs[i].description, &ret);
		}
	}

	#if 1
	// Run tests for the lockout logic.
	// These only tests the rfid code inside of catcierge_fsm.c,
	// and don't initialize the actual catcierge_rfid code.
	{
		int i;

		rfid_test_conf_t confs[] =
		{
			{
				"RFID Check off", 
				0, NULL, NULL, 0, 0, MATCH_DIR_IN
			},
			{
				"RFID Check on, no readers",
				1, NULL, NULL, 0, 0, MATCH_DIR_IN
			},

			// Inner.
			{
				"RFID Check on, inner reader, IN",
				1, "something", NULL, 0, 0, MATCH_DIR_IN
			},
			{
				"RFID Check on, inner reader, inner valid, IN",
				1, "something", NULL, 1, 0, MATCH_DIR_IN
			},
			{
				"RFID Check on, inner reader, inner & outer valid, IN",
				1, "something", NULL, 1, 1, MATCH_DIR_IN
			},
			{
				"RFID Check on, inner reader, OUT",
				1, "something", NULL, 0, 0, MATCH_DIR_OUT
			},
			{
				"RFID Check on, inner reader, inner valid, OUT",
				1, "something", NULL, 1, 0, MATCH_DIR_OUT
			},
			{
				"RFID Check on, inner reader, inner & outer valid, OUT",
				1, "something", NULL, 1, 1, MATCH_DIR_OUT
			},

			// Outer.
			{
				"RFID Check on, outer reader, IN",
				1, NULL, "something", 0, 0, MATCH_DIR_IN
			},
			{
				"RFID Check on, outer reader, inner valid, IN",
				1, NULL, "something", 1, 0, MATCH_DIR_IN
			},
			{
				"RFID Check on, outer reader, inner & outer valid, IN",
				1, NULL, "something", 1, 1, MATCH_DIR_IN
			},
			{
				"RFID Check on, outer reader, OUT",
				1, NULL, "something", 0, 0, MATCH_DIR_OUT
			},
			{
				"RFID Check on, outer reader, inner valid, OUT",
				1, NULL, "something", 1, 0, MATCH_DIR_OUT
			},
			{
				"RFID Check on, outer reader, inner & outer valid, OUT",
				1, NULL, "something", 1, 1, MATCH_DIR_OUT
			},

			// Inner + Outer.
			{
				"RFID Check on, inner & outer reader, IN",
				1, "something", "something", 0, 0, MATCH_DIR_IN
			},
			{
				"RFID Check on, inner & outer reader, inner valid, IN",
				1, "something", "something", 1, 0, MATCH_DIR_IN
			},
			{
				"RFID Check on, inner & outer reader, inner & outer valid, IN",
				1, "something", "something", 1, 1, MATCH_DIR_IN
			},
			{
				"RFID Check on, inner & outer reader, OUT",
				1, "something", "something", 0, 0, MATCH_DIR_OUT
			},
			{
				"RFID Check on, inner & outer reader, inner valid, OUT",
				1, "something", "something", 1, 0, MATCH_DIR_OUT
			},
			{
				"RFID Check on, inner & outer reader, inner & outer valid, OUT",
				1, "something", "something", 1, 1, MATCH_DIR_OUT
			}
		};

		#define RFID_TEST_COUNT sizeof(confs) / sizeof(rfid_test_conf_t)

		for (i = 0; i < RFID_TEST_COUNT; i++)
		{
			CATCIERGE_RUN_TEST((e = run_rfid_tests(&confs[i])),
				confs[i].description,
				confs[i].description, &ret);
		}
	}
	#endif
	
	if (ret)
	{
		catcierge_test_FAILURE("One or more tests failed");
	}

	#else
	catcierge_test_SKIPPED("RFID not compiled");
	#endif // WITH_RFID

	return ret;
}
