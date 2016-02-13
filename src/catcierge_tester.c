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
#ifdef _WIN32
#include <process.h>
#else
#include <limits.h>
#endif
#include <time.h>

static int parse_arg(catcierge_template_matcher_args_t *args,
	catcierge_haar_matcher_args_t *hargs, const char *key, char **values, size_t value_count)
{
	int res;
	#if 0
	res = catcierge_template_matcher_parse_args(args, key, values, value_count);
	if (res < 0)
	{
		return -1;
	}

	res = catcierge_haar_matcher_parse_args(hargs, key, values, value_count);
	if (res < 0)
	{
		return -1;
	}
	#endif

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	catcierge_matcher_t *matcher = NULL;
	char *img_paths[4096];
	IplImage *imgs[4096];
	size_t img_count = 0;
	IplImage *img = NULL;
	CvSize img_size;
	CvScalar match_color;
	int match_success = 0;
	double match_res = 0;
	int debug = 0;
	int i;
	int j;
	int show = 0;
	int save = 0;
	char *output_path = "output"; // TODO: Fix this for cargo
	double match_threshold = 0.8;
	int success_count = 0;
	int preload = 0;
	int test_matchable = 0;
	const char *matcher_str = NULL;
	match_result_t result;

	clock_t start;
	clock_t end;
	catcierge_template_matcher_args_t args;
	catcierge_haar_matcher_args_t hargs;
	char *key = NULL;
	char *values[4096];
	size_t value_count = 0;
	memset(&args, 0, sizeof(args));
	memset(&result, 0, sizeof(result));

	fprintf(stderr, "Catcierge Image match Tester (C) Joakim Soderberg 2013-2014\n");

	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s\n"
						"          [--output [path]]\n"
						"          [--debug]\n"
						"          [--show]\n"
						"          [--match_flipped <0|1>]\n"
						"          [--threshold]\n"
						"          [--preload]\n"
						"          [--test_matchable]\n"
						"          [--snout <snout images for template matching>]\n"
						"          [--cascade <haar cascade xml>]\n"
						"           --images <input images>\n"
						"           --matcher <template|haar>\n", argv[0]);
		return -1;
	}

	catcierge_haar_matcher_args_init(&hargs);
	catcierge_template_matcher_args_init(&args);

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--show"))
		{
			show = 1;
			continue;
		}
		else if (!strcmp(argv[i], "--output"))
		{
			save = 1;

			if ((i + 1) < argc)
			{
				if (strncmp(argv[i+1], "--", 2))
				{
					i++;
					output_path = argv[i];
				}
			}
			continue;
		}
		else if (!strcmp(argv[i], "--test_matchable")
				|| !strcmp(argv[i], "--test_obstructed"))
		{
			test_matchable = 1;
			preload = 1;
		}
		else if (!strcmp(argv[i], "--debug"))
		{
			debug = 1;
		}
		else if (!strcmp(argv[i], "--images"))
		{
			while (((i + 1) < argc) 
				&& strncmp(argv[i+1], "--", 2))
			{
				i++;
				img_paths[img_count] = argv[i];
				img_count++;
			}
		}
		else if (!strcmp(argv[i], "--preload"))
		{
			if ((i + 1) < argc)
			{
				i++;
				preload = 1;
				continue;
			}
		}
		else if (!strcmp(argv[i], "--matcher"))
		{
			if ((i + 1) < argc)
			{
				if (strncmp(argv[i+1], "--", 2))
				{
					i++;
					matcher_str = argv[i];

					if (strcmp(matcher_str, "template") && strcmp(matcher_str, "haar"))
					{
						fprintf(stderr, "Invalid matcher type \"%s\"\n", matcher_str);
						return -1;
					}
				}
			}
			continue;
		}
		else if (!strncmp(argv[i], "--", 2))
		{
			int j = i + 1;
			key = &argv[i][2];
			memset(values, 0, value_count * sizeof(char *));
			value_count = 0;

			// Look for values for the option.
			// Continue fetching values until we get another option
			// or there are no more options.
			while ((j < argc) && strncmp(argv[j], "--", 2))
			{
				values[value_count] = argv[j];
				value_count++;
				i++;
				j = i + 1;
			}

			if ((ret = parse_arg(&args, &hargs, key, values, value_count)) < 0)
			{
				fprintf(stderr, "Failed to parse command line arguments for \"%s\"\n", key);
				return ret;
			}
		}
		else
		{
			fprintf(stderr, "Unknown command line argument \"%s\"\n", argv[i]);
			return -1;
		}
	}

	if (!matcher_str)
	{
		fprintf(stderr, "You must specify a matcher type\n");
		return -1;
	}

	if (!strcmp(matcher_str, "template") && (args.snout_count == 0))
	{
		fprintf(stderr, "No snout image specified\n");
		return -1;
	}

	if (!strcmp(matcher_str, "haar") && !hargs.cascade)
	{
		fprintf(stderr, "No haar cascade specified\n");
		return -1;
	}

	if (img_count == 0)
	{
		fprintf(stderr, "No input image specified\n");
		return -1;
	}

	// Create output directory.
	if (save)
	{
		catcierge_make_path("%s", output_path);
	}

	args.super.type = MATCHER_TEMPLATE;
	hargs.super.type = MATCHER_HAAR;

	if (catcierge_matcher_init(&matcher,
		(!strcmp(matcher_str, "template")
		? (catcierge_matcher_args_t *)&args
		: (catcierge_matcher_args_t *)&hargs)))
	{
		fprintf(stderr, "Failed to init %s matcher.\n", matcher_str);
			return -1;
	}

	matcher->debug = debug;
	if (!matcher->is_obstructed)
		matcher->is_obstructed = catcierge_is_frame_obstructed;
	//catcierge_set_binary_thresholds(&ctx, 90, 200);

	// If we should preload the images or not
	// (Don't let file IO screw with benchmark)
	if (preload)
	{
		for (i = 0; i < (int)img_count; i++)
		{
			printf("Preload image %s\n", img_paths[i]);

			if (!(imgs[i] = cvLoadImage(img_paths[i], 1)))
			{
				fprintf(stderr, "Failed to load match image: %s\n", img_paths[i]);
				ret = -1;
				goto fail;
			}
		}
	}

	start = clock();

	if (test_matchable)
	{
		for (i = 0; i < (int)img_count; i++)
		{
			// This tests if an image frame is clear or not (matchable).
			int frame_obstructed;

			if ((frame_obstructed = matcher->is_obstructed(matcher, imgs[i])) < 0)
			{
				fprintf(stderr, "Failed to detect check for matchability frame\n");
				return -1;
			}

			printf("%s: Frame obstructed = %d\n",
				img_paths[i], frame_obstructed);

			if (show)
			{
				cvShowImage("image", imgs[i]);
				cvWaitKey(0);
			}
		}
	}
	else
	{
		for (i = 0; i < (int)img_count; i++)
		{
			match_success = 0;

			printf("---------------------------------------------------\n");
			printf("%s:\n", img_paths[i]);

			if (preload)
			{
				img = imgs[i];
			}
			else
			{
				if (!(img = cvLoadImage(img_paths[i], 1)))
				{
					fprintf(stderr, "Failed to load match image: %s\n", img_paths[i]);
					goto fail;
				}
			}

			img_size = cvGetSize(img);

			printf("  Image size: %dx%d\n", img_size.width, img_size.height);


			if ((match_res = matcher->match(matcher, img, &result, 0)) < 0)
			{
				fprintf(stderr, "Something went wrong when matching image: %s\n", img_paths[i]);
				catcierge_matcher_destroy(&matcher);
				return -1;
			}

			match_success = (match_res >= match_threshold);

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

			if (show || save)
			{
				for (j = 0; j < (int)result.rect_count; j++)
				{
					printf("x: %d\n", result.match_rects[j].x);
					printf("y: %d\n", result.match_rects[j].y);
					printf("w: %d\n", result.match_rects[j].width);
					printf("h: %d\n", result.match_rects[j].height);
					cvRectangleR(img, result.match_rects[j], match_color, 1, 8, 0);
				}

				if (show)
				{
					cvShowImage("image", img);
					cvWaitKey(0);
				}

				if (save)
				{
					char out_file[PATH_MAX]; 
					char tmp[PATH_MAX];
					char *filename = tmp;
					char *ext;
					char *start;

					// Get the extension.
					strncpy(tmp, img_paths[i], sizeof(tmp));
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
							output_path, match_success ? "ok" : "fail", filename, ext);

					printf("Saving image \"%s\"\n", out_file);

					cvSaveImage(out_file, img, 0);
				}
			}

			cvReleaseImage(&img);
		}
	}

	end = clock();

	if (!test_matchable)
	{
		printf("Note that this time isn't useful with --show\n");
		printf("%d of %d successful! (%f seconds)\n",
			success_count, (int)img_count, (float)(end - start) / CLOCKS_PER_SEC);
	}

fail:
	catcierge_matcher_destroy(&matcher);
	cvDestroyAllWindows();

	return ret;
}
