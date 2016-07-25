#include <stdlib.h>
#include <stdio.h>
#include "minunit.h"
#include "catcierge_test_helpers.h"
#include "catcierge_test_config.h"
#include "catcierge_test_helpers.h"
#include "catcierge_args.h"
#include "catcierge_types.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include "catcierge_fsm.h"
#include "catcierge_test_common.h"

static char *do_run_delayed_start_tests(int test_no_backlight)
{
	int ret = 0;
	size_t i;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;

	if (test_no_backlight)
	{
		catcierge_test_HEADLINE("Delayed start, back light OFF");
	}
	else
	{
		catcierge_test_HEADLINE("Delayed start, back light ON");
	}

	catcierge_grabber_init(&grb);
	{
		catcierge_args_init(args, "delayed_start_tests");
		args->saveimg = 0;
		args->matcher_type = MATCHER_HAAR;
		args->haar.cascade = strdup(CATCIERGE_CASCADE);
		mu_assert("Out of memory", args->haar.cascade);
		args->auto_roi = 1;
		args->save_auto_roi_img = 1;
		args->startup_delay = 1;

		if (catcierge_matcher_init(&grb.matcher, catcierge_get_matcher_args(args)))
		{
			return "Failed to init catcierge lib!\n";
		}

		ret = catcierge_output_init(&grb, &grb.output);
		mu_assert("Failed to init output context", ret == 0);

		catcierge_fsm_start(&grb);

		// Clear images to run the state machine some.
		for (i = 0; i < 2; i++)
		{
			if (test_no_backlight)
			{
				if (load_black_test_image_and_run(&grb))
				{
					return "Failed to load black test image";
				}
			}
			else
			{
				if (load_test_image_and_run(&grb, 1, 5))
				{
					return "Failed to load clear test image";
				}
			}
			catcierge_print_spinner(&grb);
			sleep(1);
		}

		if (test_no_backlight)
		{
			mu_assert("Expected forced exit", grb.running == 0);
		}
		else
		{
			mu_assert("Expected fsm to run", grb.running == 1);
		}

		catcierge_output_destroy(&grb.output);
		catcierge_matcher_destroy(&grb.matcher);
		catcierge_args_destroy(args);
	}
	catcierge_grabber_destroy(&grb);

	return NULL;
}

static char *run_delayed_start_tests()
{
	char *e = NULL;
	if ((e = do_run_delayed_start_tests(0))) return e;
	if ((e = do_run_delayed_start_tests(1))) return e;

	return e;
}

static char *run_backlight_tests()
{
	CvSize img_size;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;
	int ret;

	catcierge_test_HEADLINE("Look for backlight tests");

	catcierge_grabber_init(&grb);
	{
		catcierge_args_init(args, "backlight_tests");
		args->saveimg = 0;
		args->matcher_type = MATCHER_HAAR;
		args->haar.cascade = strdup(CATCIERGE_CASCADE);
		mu_assert("Out of memory", args->haar.cascade);

		if (catcierge_matcher_init(&grb.matcher, (catcierge_matcher_args_t *)&args->haar))
		{
			return "Failed to init catcierge lib!\n";
		}

		catcierge_fsm_start(&grb);

		catcierge_test_STATUS("Testing clear image");
		grb.img = create_clear_image();
		img_size = cvGetSize(grb.img);
		catcierge_test_STATUS("  Image size: %dx%d", img_size.width, img_size.height);

		ret = catcierge_get_back_light_area(grb.matcher, grb.img, &args->roi);
		catcierge_test_STATUS("  Backlight ROI: x = %d, y = %d, w = %d, h = %d",
			args->roi.x, args->roi.y, args->roi.width, args->roi.height);

		mu_assert("Expected ret == 0 for catcierge_get_back_light_area", (ret == 0));
		
		// Due to how the opencv api works, it shrinks the roi slightly...
		mu_assert("Expected roi to be same as image size",
			(args->roi.x == 1) &&
			(args->roi.y == 1) &&
			(args->roi.width == img_size.width - 2) &&
			(args->roi.height == img_size.height - 2));
		catcierge_test_SUCCESS("ROI same size as image as expected (only a couple of pixels smaller)\n");
		cvReleaseImage(&grb.img);

		catcierge_test_STATUS("Testing black image (as if we had a broken back light)");
		grb.img = create_black_image();
		ret = catcierge_get_back_light_area(grb.matcher, grb.img, &args->roi);
		mu_assert("Expected ret < 0 for catcierge_get_back_light_area", (ret < 0));
		catcierge_test_SUCCESS("Failed to find back light as expected\n");

		// Draw a small white rectangle on the black background.
		catcierge_test_STATUS("Testing black image with small back light (too small!)");
		cvRectangleR(grb.img, cvRect(40, 40, 40, 40), CV_RGB(255, 255, 255), 1, 8, 0);
		args->haar.super.min_backlight = 2000;
		ret = catcierge_get_back_light_area(grb.matcher, grb.img, &args->roi);
		mu_assert("Expected ret < 0 for catcierge_get_back_light_area", (ret < 0));
		catcierge_test_SUCCESS("Failed to find back light as expected\n");

		cvReleaseImage(&grb.img);

		catcierge_matcher_destroy(&grb.matcher);
		catcierge_args_destroy(args);
	}
	catcierge_grabber_destroy(&grb);

	return NULL;
}

int TEST_catcierge_matcher(int argc, char **argv)
{
	int ret = 0;
	char *e = NULL;
	catcierge_test_HEADLINE("TEST_catcierge_matcher");

	CATCIERGE_RUN_TEST((e = run_backlight_tests()),
		"Run back light tests.",
		"Back light tests", &ret);

	CATCIERGE_RUN_TEST((e = run_delayed_start_tests()),
		"Run delayed start tests.",
		"Delayed start tests", &ret);

	return ret;
}
