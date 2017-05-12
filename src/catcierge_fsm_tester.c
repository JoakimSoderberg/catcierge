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

typedef struct fsm_tester_ctx_s
{
	char *img_paths[MAX_IMG_PATHS];
	size_t img_count;
	double delay;
	int keep_running;
	int keep_obstructing;
} fsm_tester_ctx_t;

fsm_tester_ctx_t ctx;
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

static int add_fsm_tester_args(catcierge_args_t *args)
{
	int ret = 0;
	cargo_t cargo = args->cargo;

	ret |= cargo_add_group(cargo, 0, 
			"fsm", "Finite State Machine Tester Settings",
			"Use these settings to pass a set of images you want to test "
			"as if they were detected by catcierge, including all events "
			"that would be triggered by the state machine.\n");

	ret |= cargo_add_option(cargo, CARGO_OPT_REQUIRED,
			"<fsm> --images",
			"Input images that are passed to catcierge.",
			".[s]#", &ctx.img_paths, &ctx.img_count, MAX_IMG_PATHS);

	ret |= cargo_add_option(cargo, 0,
			"<fsm> --delay",
			"Initial delay before passing the images to catcierge.",
			"i", &ctx.delay);

	ret |= cargo_add_option(cargo, 0,
			"<fsm> --keep_running",
			"Keeps the catcierge state matchine running also after matching. "
			"This can be useful to test things such as lockout times and so on. "
			"If this is not used, immediately after the match is complete and all "
			"events have run, the program will terminate.",
			"b", &ctx.keep_running);

	ret |= cargo_add_option(cargo, 0,
			"<fsm> --keep_obstructing",
			"When --keep_running is turned on, don't clear the frame as usual. "
			"This is meant to enable simulating that the cat stays in front of "
			"the cat door, trying to get it open."
			"To clear the frame do Ctrl+C (and to terminate do it again).",
			"b", &ctx.keep_obstructing);

	// This option is defined in the catcierge lib instead, since it
	// uses variables internal to that. But we still group it with these settings.
	#ifndef _WIN32
	// TODO: base_time support in the lib is not supported on windows yet.
	ret |= cargo_group_add_option(cargo, "fsm", "--base_time");
	#endif // _WIN32

	return ret;
}

static void sig_handler(int signo)
{
	fprintf(stderr, "Received SIGINT...\n");

	if (ctx.keep_obstructing)
	{
		fprintf(stderr, "Stop obstructing frame!\n");
		ctx.keep_obstructing = 0;
	}
	else
	{
		fprintf(stderr, "Exiting!\n");
		grb.running = 0;
	}
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

	if (add_fsm_tester_args(args))
	{
		fprintf(stderr, "Failed to init fsm tester args\n");
		return -1;
	}

	if (catcierge_args_parse(args, argc, argv))
	{
		return -1;
	}

	if (ctx.img_count == 0)
	{
		cargo_print_usage(args->cargo, 0);
		fprintf(stderr, "\nNo input images specified!\n\n");
		ret = -1; goto fail;
	}

	printf("Images:\n");

	for (i = 0; i < ctx.img_count; i++)
	{
		printf("%s\n", ctx.img_paths[i]);
	}

	if (args->base_time)
	{
		printf("Setting basetime to: %s\n", args->base_time);
		catcierge_strftime_set_base_diff(args->base_time_diff);
	}

	if (catcierge_matcher_init(&grb.matcher, catcierge_get_matcher_args(args)))
	{
		fprintf(stderr, "\n\nFailed to %s init matcher\n\n", grb.matcher->name);
		return -1;
	}

	if (catcierge_output_init(&grb, &grb.output))
	{
		fprintf(stderr, "Failed to init output template system\n");
		exit(-1);
	}

	if (catcierge_output_load_templates(&grb.output,
			args->inputs, args->input_count))
	{
		fprintf(stderr, "Failed to load output templates\n");
		exit(-1);
	}

	#ifdef WITH_ZMQ
	catcierge_zmq_init(&grb);
	#endif

	catcierge_fsm_start(&grb);
	catcierge_timer_start(&grb.frame_timer);

	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
		fprintf(stderr, "Failed to set SIGINT handler\n");
	}

	if (!(clear_img = create_clear_image()))
	{
		ret = -1; goto fail;
	}

	// For delayed start we create a clear image that will
	// be fed to the state machine until we're ready to obstruct the frame.
	if (ctx.delay > 0.0)
	{
		catcierge_timer_t t;
		printf("Delaying match start by %0.2f seconds\n", ctx.delay);

		catcierge_timer_reset(&t);
		catcierge_timer_set(&t, ctx.delay);
		catcierge_timer_start(&t);
		grb.img = clear_img;

		while (!catcierge_timer_has_timed_out(&t))
		{
			catcierge_run_state(&grb);
		}

		grb.img = NULL;
	}

	// Load the first image and obstruct the frame.
	if (!(grb.img = load_image(&grb, ctx.img_paths[0])))
	{
		ret = -1; goto fail;
	}

	catcierge_run_state(&grb);
	cvReleaseImage(&grb.img);
	grb.img = NULL;

	// Load the match images.
	for (i = 0; i < ctx.img_count; i++)
	{
		if (!(grb.img = load_image(&grb, ctx.img_paths[i])))
		{
			ret = -1; goto fail;
		}

		catcierge_run_state(&grb);

		cvReleaseImage(&grb.img);
		grb.img = NULL;
	}

	// Keep running so we can test the lockout modes.
	if (ctx.keep_running)
	{
		// Load one of the obstructing images to start with
		if (!(grb.img = load_image(&grb, ctx.img_paths[0])))
		{
			ret = -1; goto fail;
		}

		while (grb.running)
		{
			if (!catcierge_timer_isactive(&grb.frame_timer))
			{
				catcierge_timer_start(&grb.frame_timer);
			}

			// If --keep_obstructing is set the user has
			// to press ctrl+c before the clear image is used.
			if ((grb.img != clear_img) && !ctx.keep_obstructing)
			{
				grb.img = clear_img;
			}

			catcierge_run_state(&grb);
		}
	}

fail:
	cvReleaseImage(&clear_img);
	#ifdef WITH_ZMQ
	catcierge_zmq_destroy(&grb);
	#endif
	catcierge_matcher_destroy(&grb.matcher);
	catcierge_output_destroy(&grb.output);
	catcierge_grabber_destroy(&grb);
	catcierge_args_destroy(&grb.args);

	return ret;
}
