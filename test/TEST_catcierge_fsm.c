#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_fsm.h"
#include "minunit.h"
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

static char *run_tests()
{
	int i;
	int j;
	catcierge_grb_t grb;
	catcierge_args_t *args;
	args = &grb.args;

	catcierge_grabber_init(&grb);

	args->snout_paths[0] = "../examples/snout/snout320x240.png";
	args->snout_count++;
	args->snout_paths[1] = "../examples/snout/snout320x240_04b.png";
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
		
		// Give it a clear frame so that it will stop
		load_test_image_and_run(&grb, 1, 5);
		mu_assert("Expected WAITING state", (grb.state == catcierge_state_waiting));
	}

	catcierge_grabber_destroy(&grb);

	return NULL;
}

int TEST_catcierge_fsm(int argc, char **argv)
{
	char *e = NULL;

	catcierge_test_HEADLINE("TEST_catcierge_fsm");

	if ((e = run_tests())) catcierge_test_FAILURE(e);
	else catcierge_test_SUCCESS("");

	return 0;
}
