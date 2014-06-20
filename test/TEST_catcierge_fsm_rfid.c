#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_fsm.h"
#include "minunit.h"
#include "catcierge_test_config.h"
#include "catcierge_test_helpers.h"
#include "catcierge_args.h"
#include "catcierge_types.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include "catcierge_test_common.h"

#ifdef WITH_RFID
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

	args->saveimg = 0;
	args->match_time = 1; // Set this so that the RFID check is triggered.
	args->lock_on_invalid_rfid = conf->check_rfid;
	args->rfid_inner_path = conf->inner_path;
	args->rfid_outer_path = conf->outer_path;
	grb.rfid_in_match.is_allowed = conf->inner_valid_rfid;
	grb.rfid_out_match.is_allowed = conf->outer_valid_rfid;
	grb.rfid_direction = conf->direction;
	args->templ.match_flipped = 1;
	args->templ.match_threshold = 0.8;
	args->templ.snout_paths[0] = CATCIERGE_SNOUT1_PATH;
	args->templ.snout_count++;
	args->templ.snout_paths[1] = CATCIERGE_SNOUT2_PATH;
	args->templ.snout_count++;

	if (catcierge_template_matcher_init(&grb.matcher, &args->templ))
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

	catcierge_template_matcher_destroy(&grb.matcher);
	catcierge_grabber_destroy(&grb);

	return NULL;
}
#endif // WITH_RFID

int TEST_catcierge_fsm_rfid(int argc, char **argv)
{
	int ret = 0;
	char *e = NULL;

	#if WITH_RFID
	{
		int i;

		rfid_test_conf_t confs[] =
		{
			{"RFID Check off", 
				0, NULL, NULL, 0, 0, MATCH_DIR_IN},
			{"RFID Check on, no readers",
				1, NULL, NULL, 0, 0, MATCH_DIR_IN},

			// Inner.
			{"RFID Check on, inner reader, IN",
				1, "something", NULL, 0, 0, MATCH_DIR_IN},
			{"RFID Check on, inner reader, inner valid, IN",
				1, "something", NULL, 1, 0, MATCH_DIR_IN},
			{"RFID Check on, inner reader, inner & outer valid, IN",
				1, "something", NULL, 1, 1, MATCH_DIR_IN},
			{"RFID Check on, inner reader, OUT",
				1, "something", NULL, 0, 0, MATCH_DIR_OUT},
			{"RFID Check on, inner reader, inner valid, OUT",
				1, "something", NULL, 1, 0, MATCH_DIR_OUT},
			{"RFID Check on, inner reader, inner & outer valid, OUT",
				1, "something", NULL, 1, 1, MATCH_DIR_OUT},

			// Outer.
			{"RFID Check on, outer reader, IN",
				1, NULL, "something", 0, 0, MATCH_DIR_IN},
			{"RFID Check on, outer reader, inner valid, IN",
				1, NULL, "something", 1, 0, MATCH_DIR_IN},
			{"RFID Check on, outer reader, inner & outer valid, IN",
				1, NULL, "something", 1, 1, MATCH_DIR_IN},
			{"RFID Check on, outer reader, OUT",
				1, NULL, "something", 0, 0, MATCH_DIR_OUT},
			{"RFID Check on, outer reader, inner valid, OUT",
				1, NULL, "something", 1, 0, MATCH_DIR_OUT},
			{"RFID Check on, outer reader, inner & outer valid, OUT",
				1, NULL, "something", 1, 1, MATCH_DIR_OUT},

			// Inner + Outer.
			{"RFID Check on, inner & outer reader, IN",
				1, "something", "something", 0, 0, MATCH_DIR_IN},
			{"RFID Check on, inner & outer reader, inner valid, IN",
				1, "something", "something", 1, 0, MATCH_DIR_IN},
			{"RFID Check on, inner & outer reader, inner & outer valid, IN",
				1, "something", "something", 1, 1, MATCH_DIR_IN},
			{"RFID Check on, inner & outer reader, OUT",
				1, "something", "something", 0, 0, MATCH_DIR_OUT},
			{"RFID Check on, inner & outer reader, inner valid, OUT",
				1, "something", "something", 1, 0, MATCH_DIR_OUT},
			{"RFID Check on, inner & outer reader, inner & outer valid, OUT",
				1, "something", "something", 1, 1, MATCH_DIR_OUT},
		};

		#define RFID_TEST_COUNT sizeof(confs) / sizeof(rfid_test_conf_t)

		for (i = 0; i < RFID_TEST_COUNT; i++)
		{
			catcierge_test_HEADLINE(confs[i].description);
			if ((e = run_rfid_tests(&confs[i])))
			{
				catcierge_test_FAILURE(e);
				ret = -1;
			}
			else
			{
				catcierge_test_SUCCESS("RFID test (off)");
			}
		}
	}
	#else
	catcierge_test_SKIPPED("RFID not compiled");
	#endif // WITH_RFID

	return ret;
}
