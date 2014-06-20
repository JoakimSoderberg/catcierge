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

static char *run_success_tests()
{
	int i;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;

	catcierge_grabber_init(&grb);

	args->saveimg = 0;
	args->matcher_type = MATCHER_HAAR;
	args->haar.cascade = CATCIERGE_CASCADE;

	if (catcierge_haar_matcher_init(&grb.haar, &args->haar))
	{
		return "Failed to init haar matcher";
	}

	catcierge_set_state(&grb, catcierge_state_waiting);

	// This is the initial image that obstructs the frame
	// and triggers the matching.
	load_test_image_and_run(&grb, 6, 1);
	mu_assert("Expected MATCHING state", (grb.state == catcierge_state_matching));

	// Match against 4 pictures, and decide the lockout status.
	for (i = 1; i <= 4; i++)
	{
		load_test_image_and_run(&grb, 6, i);
	}

	catcierge_haar_matcher_destroy(&grb.haar);
	catcierge_grabber_destroy(&grb);

	return NULL;
}

int TEST_catcierge_fsm_haar_matcher(int argc, char **argv)
{
	char *e = NULL;
	int ret = 0;
	catcierge_test_HEADLINE("TEST_catcierge_fsm_haar_matcher");

	CATCIERGE_RUN_TEST((e = run_success_tests()),
		"Run success tests. Without obstruct",
		"Success match without obstruct", &ret);

	return ret;
}
