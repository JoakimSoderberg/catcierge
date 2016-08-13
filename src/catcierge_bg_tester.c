//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2015
//
//    Catcierge is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//    Catcierge is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Catcierge.  If not, see <http://www.gnu.org/licenses/>.
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "catcierge_fsm.h"
#include "catcierge_args.h"
#include "catcierge_types.h"
#include "catcierge_config.h"
#include "catcierge_args.h"
#include "catcierge_output.h"
#include "test/catcierge_test_common.h"
#include "catcierge_strftime.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#ifdef WITH_ZMQ
#include <czmq.h>
#endif

#define MAX_IMG_PATHS 4

typedef struct bg_tester_ctx_s
{
	//char *img_paths[MAX_IMG_PATHS];
	char *img_path;
	//size_t img_count;
} bg_tester_ctx_t;

bg_tester_ctx_t ctx;
catcierge_grb_t grb;

static IplImage *load_image(catcierge_grb_t *grb, const char *path)
{
	IplImage *img = NULL;

	if (!(img = cvLoadImage(path, 0)))
	{
		fprintf(stderr, "Failed to load image: %s\n", path);
		return NULL;
	}

	return img;
}

static int add_bg_tester_args(catcierge_args_t *args)
{
	int ret = 0;
	cargo_t cargo = args->cargo;

	ret |= cargo_add_group(cargo, 0, 
			"bg", "Background tester settings",
			"Use these settings to pass a set of images you want to test "
			"when tweaking the settings relating to finding the backlight "
			"in the background using --auto_roi and other command line options.\n");

	/*ret |= cargo_add_option(cargo, CARGO_OPT_REQUIRED,
			"<bg> --images",
			"Input images that are passed to catcierge.",
			".[s]#", &ctx.img_paths, &ctx.img_count, MAX_IMG_PATHS);*/
	ret |= cargo_add_option(cargo, CARGO_OPT_REQUIRED,
		"<bg> image",
		"Input image containig the background image used to tweak settings with.",
		"s", &ctx.img_path);

	return ret;
}

static void sig_handler(int signo)
{
	fprintf(stderr, "Received SIGINT...\n");
	fprintf(stderr, "Exiting!\n");
	grb.running = 0;
}

int main(int argc, char **argv)
{
	size_t i;
	int ret = 0;
	IplImage *clear_img = NULL;
	catcierge_args_t *args = &grb.args;

	memset(&ctx, 0, sizeof(ctx));
	catcierge_grabber_init(&grb);

	if (catcierge_args_init(args, argv[0]))
	{
		fprintf(stderr, "Failed to init args\n");
		return -1;
	}

	if (add_bg_tester_args(args))
	{
		fprintf(stderr, "Failed to init background tester args\n");
		return -1;
	}

	if (catcierge_args_parse(args, argc, argv))
	{
		return -1;
	}

	printf("Image: %s\n", ctx.img_path);


fail:
	catcierge_grabber_destroy(&grb);
	catcierge_args_destroy(&grb.args);

	return ret;
}
