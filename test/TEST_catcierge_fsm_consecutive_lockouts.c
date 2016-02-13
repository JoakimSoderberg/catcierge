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
#include "catcierge_cargo.h"

static char *run_consecutive_lockout_abort_tests()
{
	int ret = 0;
	char *return_message = NULL;
	catcierge_grb_t grb;
	catcierge_args_t *args;
	args = &grb.args;

	catcierge_grabber_init(&grb);
	catcierge_args_init(args, "catcierge");

	{
		char *argv[256] =
		{
			"catcierge",
			"--lockout_method", "1",
			"--lockout_error", "3",
			// It's important we change this from the default 3 seconds.
			// Since when the tests run under valgrind they are much slower
			// we need to extend the time that is counted as consecutive tests
			// otherwise the consecutive count will become invalid.
			"--lockout_error_delay", "10",
			"--lockout", "0",
			"--templ",
			"--match_flipped",
			"--threshold", "0.8",
			"--snout", CATCIERGE_SNOUT1_PATH, CATCIERGE_SNOUT2_PATH,
			NULL
		};
		int argc = get_argc(argv);

		ret = catcierge_args_parse(args, argc, argv);
		mu_assertf("Failed to parse command line", ret == 0);
	}

	if (catcierge_matcher_init(&grb.matcher, (catcierge_matcher_args_t *)&args->templ))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_set_state(&grb, catcierge_state_waiting);

	grb.running = 1;

	// Now trigger "max consecutive lockouts" but add a success in the middle
	// to make sure the consecutive count is reset.
	{
		load_test_image_and_run(&grb, 1, 2); // Obstruct.
		load_test_image_and_run(&grb, 1, 2); // Pass 4 images (invalid).
		load_test_image_and_run(&grb, 1, 3);
		load_test_image_and_run(&grb, 1, 4);
		load_test_image_and_run(&grb, 1, 4);
		catcierge_run_state(&grb);
		catcierge_test_STATUS("Consecutive lockout %d", grb.consecutive_lockout_count);
		mu_assertf("Expected 1 consecutive lockout count", (grb.consecutive_lockout_count == 1));

		load_test_image_and_run(&grb, 1, 2); // Obstruct.
		load_test_image_and_run(&grb, 1, 2); // Pass 4 images (invalid).
		load_test_image_and_run(&grb, 1, 3);
		load_test_image_and_run(&grb, 1, 4);
		load_test_image_and_run(&grb, 1, 4);
		catcierge_run_state(&grb);
		catcierge_test_STATUS("Consecutive lockout %d", grb.consecutive_lockout_count);
		mu_assertf("Expected 2 consecutive lockout count", (grb.consecutive_lockout_count == 2));

		// Here we expect the lockout count to be reset.
		// Note that we must sleep at least args->consecutive_lockout_delay seconds
		// before causing another lockout, otherwise we will still count it as
		// a consecutive lockout...
		load_test_image_and_run(&grb, 1, 1); // Obstruct.
		load_test_image_and_run(&grb, 1, 1); // Pass 4 images (valid).
		load_test_image_and_run(&grb, 1, 2);
		load_test_image_and_run(&grb, 1, 3);
		load_test_image_and_run(&grb, 1, 4);
		load_test_image_and_run(&grb, 1, 5); // Clear frame.
		catcierge_test_STATUS("Consecutive lockout reset");

		catcierge_test_STATUS("Sleep %0.1f seconds so that the next lockout isn't counted as consecutive", args->consecutive_lockout_delay);
		sleep(args->consecutive_lockout_delay);

		load_test_image_and_run(&grb, 1, 2); // Obstruct.
		load_test_image_and_run(&grb, 1, 2); // Pass 4 images (invalid).
		load_test_image_and_run(&grb, 1, 3);
		load_test_image_and_run(&grb, 1, 4);
		load_test_image_and_run(&grb, 1, 4);
		catcierge_run_state(&grb);
		
		catcierge_test_STATUS("Consecutive lockout %d", grb.consecutive_lockout_count);
		mu_assertf("Expected 0 lockout count", (grb.consecutive_lockout_count == 0));
	}

	mu_assert("Expected program to be in running state after consecutive lockout count is aborted",
		(grb.running == 1));

cleanup:
	catcierge_matcher_destroy(&grb.matcher);
	catcierge_args_destroy(args);
	catcierge_grabber_destroy(&grb);

	return return_message;
}

static char *run_consecutive_lockout_tests()
{
	int i;
	int ret = 0;
	catcierge_grb_t grb;
	catcierge_args_t *args;
	args = &grb.args;
	char *return_message = NULL;

	catcierge_grabber_init(&grb);
	catcierge_args_init(args, "catcierge");

	// TODO: Exact same command line as other test, make function.
	{
		char *argv[256] =
		{
			"catcierge",
			"--lockout_method", "1",
			"--lockout_error", "3",
			// It's important we change this from the default 3 seconds.
			// Since when the tests run under valgrind they are much slower
			// we need to extend the time that is counted as consecutive tests
			// otherwise the consecutive count will become invalid.
			"--lockout_error_delay", "10",
			"--lockout", "0",
			"--templ",
			"--match_flipped",
			"--threshold", "0.8",
			"--snout", CATCIERGE_SNOUT1_PATH, CATCIERGE_SNOUT2_PATH,
			NULL
		};
		int argc = get_argc(argv);

		ret = catcierge_args_parse(args, argc, argv);
		mu_assertf("Failed to parse command line", ret == 0);
	}

	if (catcierge_matcher_init(&grb.matcher, (catcierge_matcher_args_t *)&args->templ))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_set_state(&grb, catcierge_state_waiting);

	grb.running = 1;

	// Cause "max consecutive lockout count" of lockouts
	// to make sure an abort is triggered.
	for (i = 0; i < 3; i++)
	{
		// Obstruct the frame to begin matching.
		load_test_image_and_run(&grb, 1, 2);

		load_test_image_and_run(&grb, 1, 2);
		load_test_image_and_run(&grb, 1, 3);
		load_test_image_and_run(&grb, 1, 4);
		load_test_image_and_run(&grb, 1, 4);

		// Run the state machine to end lockout
		// without passing a new image.
		catcierge_run_state(&grb);
		catcierge_test_STATUS("Consecutive lockout: %d, Expecting: %d, Running: %d",
			grb.consecutive_lockout_count, i+1, grb.running);
		mu_assertf("Unexpected consecutive lockout count", (grb.consecutive_lockout_count == (i+1)));
	}

	mu_assertf("Expected program to be in non-running state after max_consecutive_lockout_count",
		(grb.running == 0));

cleanup:
	catcierge_matcher_destroy(&grb.matcher);
	catcierge_args_destroy(args);
	catcierge_grabber_destroy(&grb);

	return return_message;
}

int TEST_catcierge_fsm_consecutive_lockouts(int argc, char **argv)
{
	int ret = 0;
	char *e = NULL;
	catcierge_test_HEADLINE("TEST_catcierge_fsm_consecutive_lockouts");

	// Trigger max consecutive lockout.
	CATCIERGE_RUN_TEST((e = run_consecutive_lockout_tests()),
		"Run consecutive lockout tests.",
		"Consecutive lockout tests", &ret);

	// Trigger max consecutive lockout.
	CATCIERGE_RUN_TEST((e = run_consecutive_lockout_abort_tests()),
		"Run consecutive lockout test. Reset counter",
		"Consecutive lockout test (with reset counter)", &ret);

	return ret;
}
