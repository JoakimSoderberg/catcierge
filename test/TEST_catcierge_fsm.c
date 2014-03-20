#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_fsm.h"
#include "minunit.h"
#include "catcierge_test_config.h"
#include "catcierge_test_helpers.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

static IplImage *open_test_image(int series, int i)
{
	char path[1024];

	snprintf(path, sizeof(path), "../examples/real/series/%04d_%02d.png", series, i);
	catcierge_test_STATUS("%s", path);

	return cvLoadImage(path, 0);
}

static void free_test_image(catcierge_grb_t *grb)
{
	cvReleaseImage(&grb->img);
	grb->img = NULL;
}

//
// Helper function for loading an image from disk and running the state machine.
//
static void load_test_image_and_run(catcierge_grb_t *grb, int series, int i)
{
	grb->img = open_test_image(series, i);
	catcierge_run_state(grb);
	free_test_image(grb);		
}

//
// Tests passing 1 initial image that triggers matching.
// Then pass 4 images that should result in a successful match.
// Finally if "obstruct" is set, a single image is passed that obstructs the frame.
// After that we expect to go back to waiting.
//
static char *run_normal_tests(int obstruct)
{
	int i;
	int j;
	catcierge_grb_t grb;
	catcierge_args_t *args;
	args = &grb.args;

	catcierge_grabber_init(&grb);

	args->snout_paths[0] = CATCIERGE_SNOUT1_PATH;
	args->snout_count++;
	args->snout_paths[1] = CATCIERGE_SNOUT2_PATH;
	args->snout_count++;

	if (catcierge_init(&grb.matcher, args->snout_paths, args->snout_count))
	{
		return "Failed to init catcierge lib!\n";
	}

	catcierge_set_match_flipped(&grb.matcher, 1);
	catcierge_set_match_threshold(&grb.matcher, 0.8);

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
			mu_assert("Expected WAITING state", (grb.state == catcierge_state_keepopen));

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

	catcierge_grabber_destroy(&grb);

	return NULL;
}

int TEST_catcierge_fsm(int argc, char **argv)
{
	int ret = 0;
	char *e = NULL;

	catcierge_test_HEADLINE("TEST_catcierge_fsm");

	catcierge_test_STATUS("Run normal tests. Without obstruct");
	if ((e = run_normal_tests(0))) { catcierge_test_FAILURE(e); ret = -1; }
	else catcierge_test_SUCCESS("");

	catcierge_test_STATUS("Run normal tests. With obstruct");
	if ((e = run_normal_tests(1))) { catcierge_test_FAILURE(e); ret = -1; }
	else catcierge_test_SUCCESS("");

	return ret;
}
