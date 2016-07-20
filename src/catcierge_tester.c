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
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdlib.h>
#include "catcierge_matcher.h"
#include "catcierge_template_matcher.h"
#include "catcierge_haar_matcher.h"
#include "catcierge_util.h"
#include "catcierge_types.h"
#include "catcierge_args.h"
#ifdef _WIN32
#include <process.h>
#else
#include <limits.h>
#endif
#include <time.h>

typedef struct tester_ctx_s
{
	char **img_paths;
	size_t img_count;
	IplImage **imgs;

	int save;
	char *output_path;

	int show;
	int test_matchable;
	int debug;
	int preload;
} tester_ctx_t;

tester_ctx_t ctx;

static int add_options(catcierge_args_t *args)
{
	int ret = 0;
	cargo_t cargo = args->cargo;

	ret |= cargo_add_group(cargo, 0, 
			"test", "Test Settings",
			"Settings for testing inputing a set of images to the catcierge matcher.");

	ret |= cargo_add_option(cargo, CARGO_OPT_REQUIRED,
			"<test> --images",
			"Input images that are passed to catcierge.",
			"[s]+", &ctx.img_paths, &ctx.img_count);

	ret |= cargo_add_option(cargo, 0,
			"<test> --test_save",
			"Save the match result images.",
			"b", &ctx.save);

	ctx.output_path = strdup("output");
	ret |= cargo_add_option(cargo, 0,
			"<test> --test_output",
			"Path were to save the output when using --save.",
			"s", &ctx.output_path);

	ret |= cargo_add_option(cargo, 0,
			"<test> --test_show",
			"Show the images being matched in a window.",
			"b", &ctx.show);

	ret |= cargo_add_option(cargo, 0,
			"<test> --test_obstructed",
			"Test if the image causes what catcierge considers to be an obstruction "
			"which is what catcierge uses to decide when to start to match.",
			"b", &ctx.test_matchable);

	ret |= cargo_add_option(cargo, 0,
			"<test> --debug",
			"Turn on internal debugging in the matcher.",
			"b", &ctx.debug);

	ret |= cargo_add_option(cargo, 0,
			"<test> --preload",
			"Preload the images before we start any tests, "
			"so the speed is not affected by disk IO at the time of the matching.",
			"b", &ctx.preload);

	return ret;
}

int main(int argc, char **argv)
{
	int ret = 0;
	catcierge_matcher_t *matcher = NULL;
	IplImage *img = NULL;
	CvSize img_size;
	CvScalar match_color;
	int match_success = 0;
	double match_res = 0;
	int i;
	int j;
	int success_count = 0;
	match_result_t result;

	clock_t start;
	clock_t end;
	catcierge_args_t args;
	memset(&args, 0, sizeof(args));
	memset(&result, 0, sizeof(result));

	fprintf(stderr, "Catcierge Image match Tester (C) Joakim Soderberg 2013-2016\n");

	if (catcierge_args_init(&args, argv[0]))
	{
		fprintf(stderr, "Failed to init args\n");
		return -1;
	}

	if (add_options(&args))
	{
		fprintf(stderr, "Failed to init tester args\n");
		ret = -1; goto fail;
	}

	if (catcierge_args_parse(&args, argc, argv))
	{
		ret = -1; goto fail;
	}

	// Create output directory.
	if (ctx.save)
	{
		catcierge_make_path("%s", ctx.output_path);
	}

	if (catcierge_matcher_init(&matcher, catcierge_get_matcher_args(&args)))
	{
		fprintf(stderr, "\n\nFailed to %s init matcher\n\n", matcher->name);
		return -1;
	}

	matcher->debug = ctx.debug;

	if (!(ctx.imgs = calloc(ctx.img_count, sizeof(IplImage *))))
	{
		fprintf(stderr, "Out of memory!\n");
		ret = -1;
		goto fail;
	}

	// TODO: Move to function
	// If we should preload the images or not
	// (Don't let file IO screw with benchmark)
	if (ctx.preload)
	{
		for (i = 0; i < (int)ctx.img_count; i++)
		{
			printf("Preload image %s\n", ctx.img_paths[i]);

			if (!(ctx.imgs[i] = cvLoadImage(ctx.img_paths[i], 1)))
			{
				fprintf(stderr, "Failed to load match image: %s\n", ctx.img_paths[i]);
				ret = -1;
				goto fail;
			}
		}
	}

	start = clock();

	if (ctx.test_matchable)
	{
		for (i = 0; i < (int)ctx.img_count; i++)
		{
			// This tests if an image frame is clear or not (matchable).
			int frame_obstructed;

			if ((frame_obstructed = matcher->is_obstructed(matcher, ctx.imgs[i])) < 0)
			{
				fprintf(stderr, "Failed to detect check for matchability frame\n");
				return -1;
			}

			printf("%s: Frame obstructed = %d\n",
				ctx.img_paths[i], frame_obstructed);

			if (ctx.show)
			{
				cvShowImage("image", ctx.imgs[i]);
				cvWaitKey(0);
			}
		}
	}
	else
	{
		for (i = 0; i < (int)ctx.img_count; i++)
		{
			match_success = 0;

			printf("---------------------------------------------------\n");
			printf("%s:\n", ctx.img_paths[i]);

			if (ctx.preload)
			{
				img = ctx.imgs[i];
			}
			else
			{
				if (!(img = cvLoadImage(ctx.img_paths[i], 1)))
				{
					fprintf(stderr, "Failed to load match image: %s\n", ctx.img_paths[i]);
					goto fail;
				}
			}

			img_size = cvGetSize(img);

			printf("  Image size: %dx%d\n", img_size.width, img_size.height);


			if ((match_res = matcher->match(matcher, img, &result, 0)) < 0)
			{
				fprintf(stderr, "Something went wrong when matching image: %s\n", ctx.img_paths[i]);
				catcierge_matcher_destroy(&matcher);
				return -1;
			}

			match_success = (match_res >= args.templ.match_threshold);

			if (match_success)
			{
				printf("  Match (%s)! %f\n", catcierge_get_direction_str(result.direction), match_res);
				match_color = CV_RGB(0, 255, 0);
				success_count++;
			}
			else
			{
				printf("  No match! %f\n", match_res);
				match_color = CV_RGB(255, 0, 0);
			}

			if (ctx.show || ctx.save)
			{
				for (j = 0; j < (int)result.rect_count; j++)
				{
					printf("x: %d\n", result.match_rects[j].x);
					printf("y: %d\n", result.match_rects[j].y);
					printf("w: %d\n", result.match_rects[j].width);
					printf("h: %d\n", result.match_rects[j].height);
					cvRectangleR(img, result.match_rects[j], match_color, 1, 8, 0);
				}

				if (ctx.show)
				{
					cvShowImage("image", img);
					cvWaitKey(0);
				}

				if (ctx.save)
				{
					char out_file[PATH_MAX]; 
					char tmp[PATH_MAX];
					char *filename = tmp;
					char *ext;
					char *start;

					// Get the extension.
					strncpy(tmp, ctx.img_paths[i], sizeof(tmp));
					ext = strrchr(tmp, '.');
					*ext = '\0';
					ext++;

					// And filename.
					filename = strrchr(tmp, '/');
					start = strrchr(tmp, '\\');
					if (start> filename)
						filename = start;
					filename++;

					snprintf(out_file, sizeof(out_file) - 1, "%s/match_%s__%s.%s", 
							ctx.output_path, match_success ? "ok" : "fail", filename, ext);

					printf("Saving image \"%s\"\n", out_file);

					cvSaveImage(out_file, img, 0);
				}
			}

			cvReleaseImage(&img);
		}
	}

	end = clock();

	if (!ctx.test_matchable)
	{
		if (ctx.show)
		{
			printf("Note that the duration is affected by using --show\n");
		}
		printf("%d of %d successful! (%f seconds)\n",
			success_count, (int)ctx.img_count, (float)(end - start) / CLOCKS_PER_SEC);
	}

fail:
	catcierge_matcher_destroy(&matcher);
	cvDestroyAllWindows();

	return ret;
}
