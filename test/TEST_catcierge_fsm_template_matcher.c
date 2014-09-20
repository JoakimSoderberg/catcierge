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

// This tests the different lockout strategies.
static char *run_lockout_tests(catcierge_grb_t *grb, int obstruct,
	catcierge_lockout_method_t lockout_method)
{
	catcierge_args_t *args = &grb->args;

	switch (lockout_method)
	{
		case OBSTRUCT_OR_TIMER_1:
			if (obstruct == 1)
			{
				// Obstruct and then un-obstruct.
				catcierge_test_STATUS("Lockout method: Obstruct or timer. Obstruct,"
					" then un-obstruct should result in unlock");

				load_test_image_and_run(grb, 1, 2); // Obstruct.
				mu_assert("Expected LOCKOUT state after obstruction",
					(grb->state == catcierge_state_lockout));

				load_test_image_and_run(grb, 1, 5); // Clear frame.
				mu_assert("Expected WAITING state after clear frame",
					(grb->state == catcierge_state_waiting));
			}
			else if (obstruct == 2)
			{
				// Keep on obstructing. Timer should unlock.
				catcierge_test_STATUS("Lockout method: Obstruct or timer. "
					"Continous obstruction, %d second timer should result in unlock",
					args->lockout_time + 1);

				load_test_image_and_run(grb, 1, 2); // Obstruct.
				mu_assert("Expected LOCKOUT state after obstruction",
					(grb->state == catcierge_state_lockout));

				sleep(args->lockout_time + 1);
				
				//catcierge_run_state(grb);
				load_test_image_and_run(grb, 1, 2);
				mu_assert("Expected WAITING state after timeout",
					(grb->state == catcierge_state_waiting));
			}
			else
			{
				catcierge_test_STATUS("Lockout method: Obstruct or timer. "
					"No obstruction. Sleeping %d seconds should result in unlock",
					args->lockout_time + 1);

				sleep(args->lockout_time + 1);

				// Clear frame.
				load_test_image_and_run(grb, 1, 5);
				mu_assert("Expected WAITING stat after timeout",
					(grb->state == catcierge_state_waiting));
			}
			break;
		case OBSTRUCT_THEN_TIMER_2:
			if (obstruct == 1)
			{
				catcierge_test_STATUS("Lockout method: Obstruct then Timer. "
					"%d second timer should start after obstruction has ended",
					args->lockout_time + 1);

				load_test_image_and_run(grb, 1, 2); // Obstruct.
				mu_assert("Expected LOCKOUT state after obstruction",
					(grb->state == catcierge_state_lockout));

				load_test_image_and_run(grb, 1, 5); // Clear frame.
				mu_assert("Expected LOCKOUT state after clear frame",
					(grb->state == catcierge_state_lockout));

				sleep(1);

				mu_assert("Expected LOCKOUT state after 1 second clear frame",
					(grb->state == catcierge_state_lockout));

				sleep(args->lockout_time);

				load_test_image_and_run(grb, 1, 5); // Clear frame.
				mu_assert("Expected WAITING state after clear frame and timeout",
					(grb->state == catcierge_state_waiting));

				catcierge_test_STATUS("Lockout timer value: %f seconds\n",
					catcierge_timer_get(&grb->lockout_timer));
			}
			else if (obstruct == 2)
			{
				catcierge_test_STATUS("Lockout method: Obstruct then Timer. "
					"Continous obstruction. Unlock should never happen");

				load_test_image_and_run(grb, 1, 2); // Obstruct.
				mu_assert("Expected LOCKOUT state after obstruction",
					(grb->state == catcierge_state_lockout));
				
				sleep(args->lockout_time + 1);
				
				load_test_image_and_run(grb, 1, 2); // Obstruct.
				mu_assert("Expected LOCKOUT state after timeout and"
					"continous obstruction",
					(grb->state == catcierge_state_lockout));

				catcierge_set_state(grb, catcierge_state_waiting);
			}
			else
			{
				catcierge_test_STATUS("Lockout method: Obstruct then Timer. "
					"No obstruction. %d second timer should unlock.",
					args->lockout_time + 1);

				load_test_image_and_run(grb, 1, 5); // Clear frame.
				mu_assert("Expected LOCKOUT state after clear frame",
					(grb->state == catcierge_state_lockout));

				sleep(args->lockout_time + 1);

				load_test_image_and_run(grb, 1, 5); // Clear frame.
				mu_assert("Expected WAITING state after clear frame and timeout",
					(grb->state == catcierge_state_waiting));
			}
			break;
		case TIMER_ONLY_3:
			// Sleep for unlock time and make sure we've unlocked.
			catcierge_test_STATUS("Lockout method: Timer only, sleep %d seconds",
				args->lockout_time + 1);

			sleep(args->lockout_time + 1);

			catcierge_run_state(grb);
			mu_assert("Expected WAITING state after timeout",
				(grb->state == catcierge_state_waiting));
			break;
	}

	return NULL;
}

static char *run_failure_tests(int obstruct, catcierge_lockout_method_t lockout_method)
{
	char *e = NULL;
	size_t i;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;

	catcierge_grabber_init(&grb);

	args->saveimg = 0;
	args->lockout_method = lockout_method;
	args->lockout_time = 2;
	args->templ.match_flipped = 1;
	args->templ.match_threshold = 0.8;
	set_default_test_snouts(args);

	if (catcierge_template_matcher_init(&grb.matcher, &grb.common_matcher, &args->templ))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_template_matcher_print_settings(&args->templ);

	catcierge_set_state(&grb, catcierge_state_waiting);

	//
	// Run the same twice so we're sure the timers work
	// several times properly.
	//
	for (i = 0; i < 2; i++)
	{
		// Obstruct the frame to begin matching.
		load_test_image_and_run(&grb, 1, 2);
		mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));
	
		// Pass 4 frames (first ok, the rest not...)
		load_test_image_and_run(&grb, 1, 2);
		mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));
	
		load_test_image_and_run(&grb, 1, 3);
		mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));
	
		load_test_image_and_run(&grb, 1, 4);
		mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));
	
		load_test_image_and_run(&grb, 1, 4);
		mu_assert("Expected LOCKOUT state", (grb.state == catcierge_state_lockout));
		catcierge_test_STATUS("Lockout state as expected");
	
		// Test the lockout logic.
		if ((e = run_lockout_tests(&grb, obstruct, lockout_method)))
		{
			return e;
		}
	}

	catcierge_template_matcher_destroy(&grb.matcher);
	catcierge_grabber_destroy(&grb);

	return NULL;
}

//
// Tests passing 1 initial image that triggers matching.
// Then pass 4 images that should result in a successful match.
// Finally if "obstruct" is set, a single image is passed that obstructs the frame.
// After that we expect to go back to waiting.
//
static char *run_success_tests(int obstruct)
{
	int i;
	int j;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;

	catcierge_grabber_init(&grb);

	args->saveimg = 0;
	args->templ.match_flipped = 1;
	args->templ.match_threshold = 0.8;
	args->templ.snout_paths[0] = CATCIERGE_SNOUT1_PATH;
	args->templ.snout_count++;
	args->templ.snout_paths[1] = CATCIERGE_SNOUT2_PATH;
	args->templ.snout_count++;

	if (catcierge_template_matcher_init(&grb.matcher, &grb.common_matcher, &args->templ))
	{
		return "Failed to init template matcher!\n";
	}

	catcierge_template_matcher_set_debug(&grb.matcher, 0);

	catcierge_template_matcher_print_settings(&args->templ);

	catcierge_set_state(&grb, catcierge_state_waiting);

	// Give the state machine a series of known images
	// and make sure the states are as expected.
	for (j = 1; j <= 5; j++)
	{
		catcierge_test_STATUS("Test series %d", j);

		// This is the initial image that obstructs the frame
		// and triggers the matching.
		load_test_image_and_run(&grb, j, 1);
		mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));

		// Match against 4 pictures, and decide the lockout status.
		for (i = 1; i <= 4; i++)
		{
			load_test_image_and_run(&grb, j, i);
		}

		mu_assert("Expected KEEP OPEN state", (grb.state == catcierge_state_keepopen));

		if (obstruct)
		{
			// First obstruct the frame.
			load_test_image_and_run(&grb, j, 1);
			mu_assert("Expected KEEP OPEN state", (grb.state == catcierge_state_keepopen));

			// And then clear it.
			load_test_image_and_run(&grb, 1, 5);
			mu_assert("Expected WAITING state", (grb.state == catcierge_state_waiting));
		}
		else
		{
			// Give it a clear frame so that it will stop
			load_test_image_and_run(&grb, 1, 5);
			mu_assert("Expected WAITING state", (grb.state == catcierge_state_waiting));
		}
	}

	catcierge_template_matcher_destroy(&grb.matcher);
	catcierge_grabber_destroy(&grb);

	return NULL;
}

void run_camera_test()
{
	catcierge_grb_t grb;
	memset(&grb, 0, sizeof(grb));
	catcierge_setup_camera(&grb);
	catcierge_destroy_camera(&grb);
}

int TEST_catcierge_fsm_template_matcher(int argc, char **argv)
{
	int ret = 0;
	char *e = NULL;
	int obstruct;

	catcierge_test_HEADLINE("TEST_catcierge_fsm_template_matcher");

	catcierge_template_matcher_usage();

	// Test without anything obstructing the frame after
	// the successful match.
	CATCIERGE_RUN_TEST((e = run_success_tests(0)),
		"Run success tests. Without obstruct",
		"Success match without obstruct", &ret);

	// Same as above, but add an extra frame obstructing at the end.
	CATCIERGE_RUN_TEST((e = run_success_tests(1)),
		"Run success tests. With obstruct",
		"Success match with obstruct", &ret);

	// Obstruct 1 means we obstruct, and then remove the obstruction.
	// Obstruct 2 keeps obstructing.
	for (obstruct = 0; obstruct <= 2; obstruct++)
	{
		size_t i;
		catcierge_lockout_method_t locks[] = {
			OBSTRUCT_OR_TIMER_1,
			OBSTRUCT_THEN_TIMER_2,
			TIMER_ONLY_3
		};

		for (i = 0; i < sizeof(locks) / sizeof(locks[0]); i++)
		{
			CATCIERGE_RUN_TEST((e = run_failure_tests(obstruct, locks[i])),
				"Run failure tests.",
				"Failure tests passed", &ret);
		}
	}

	// This fails on the Raspberry pi, the camera fails to init for some reason...
	#ifndef RPI
	run_camera_test();
	#endif

	if (ret)
	{
		catcierge_test_FAILURE("One of the tests failed!\n");
	}

	return ret;
}
