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

IplImage *open_test_image(int series, int i)
{
	IplImage *img = NULL;
	char path[1024];

	snprintf(path, sizeof(path), "%s/real/series/%04d_%02d.png", CATCIERGE_IMG_ROOT, series, i);
	catcierge_test_STATUS("%s", path);

	if (!(img = cvLoadImage(path, 0)))
	{
		catcierge_test_FAILURE("Failed to load image %s", path);
	}

	return img;
}

void set_default_test_snouts(catcierge_args_t *args)
{
	assert(args);
	args->templ.snout_paths[0] = CATCIERGE_SNOUT1_PATH;
	args->templ.snout_count++;
	args->templ.snout_paths[1] = CATCIERGE_SNOUT2_PATH;
	args->templ.snout_count++;
}

void free_test_image(catcierge_grb_t *grb)
{
	cvReleaseImage(&grb->img);
	grb->img = NULL;
}

//
// Helper function for loading an image from disk and running the state machine.
//
int load_test_image_and_run(catcierge_grb_t *grb, int series, int i)
{
	if ((grb->img = open_test_image(series, i)) == NULL)
	{
		return -1;
	}

	catcierge_run_state(grb);
	free_test_image(grb);
	return 0;
}
