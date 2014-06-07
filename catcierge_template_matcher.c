//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2014
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
#include "catcierge_template_matcher.h"
#include "catcierge_log.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <assert.h>
#ifdef _WIN32
#include <io.h>
#define F_OK 0 // Test for existance.
#define R_OK 4 // Test for read permission.
#define W_OK 2 // Test for write permission.
#else
#include <unistd.h>
#endif

static int _catcierge_prepare_img(catcierge_template_matcher_t *ctx, const IplImage *src, IplImage *dst)
{
	const IplImage *img_gray = NULL;
	IplImage *tmp = NULL;
	IplImage *tmp2 = NULL;
	assert(ctx);
	assert(ctx->kernel);
	assert(src != dst);

	if (src->nChannels != 1)
	{
		tmp = cvCreateImage(cvGetSize(src), 8, 1);
		cvCvtColor(src, tmp, CV_BGR2GRAY);
		img_gray = tmp;
	}
	else
	{
		img_gray = src;
	}

	if (ctx->erode)
	{
		// Smooths out the image using erode.
		tmp2 = cvCreateImage(cvGetSize(src), 8, 1);
		cvThreshold(img_gray, tmp2,
					ctx->low_binary_thresh,
					ctx->high_binary_thresh,
					CV_THRESH_BINARY);
		cvErode(tmp2, dst, ctx->kernel, 3);
		cvReleaseImage(&tmp2);
	}
	else
	{
		cvThreshold(img_gray, dst,
					ctx->low_binary_thresh,
					ctx->high_binary_thresh,
					CV_THRESH_BINARY);
	}

	if (src->nChannels != 1)
	{
		cvReleaseImage(&tmp);
	} 

	return 0;
}

void catcierge_template_matcher_set_erode(catcierge_template_matcher_t *ctx, int erode)
{
	assert(ctx);
	ctx->erode = erode;
}

void catcierge_template_matcher_set_match_flipped(catcierge_template_matcher_t *ctx, int match_flipped)
{
	assert(ctx);
	ctx->match_flipped = match_flipped;
}

void catcierge_template_matcher_set_match_threshold(catcierge_template_matcher_t *ctx, double match_threshold)
{
	assert(ctx);
	ctx->match_threshold = match_threshold;
}

void catcierge_template_matcher_set_binary_thresholds(catcierge_template_matcher_t *ctx, int low, int high)
{
	assert(ctx);
	ctx->low_binary_thresh = low;
	ctx->high_binary_thresh = high;
}

void catcierge_template_matcher_set_debug(catcierge_template_matcher_t *ctx, int debug)
{
	ctx->debug = debug;
}

int catcierge_template_matcher_init(catcierge_template_matcher_t *ctx, catcierge_template_matcher_args_t *args)
{
	int i;
	CvSize snout_size;
	CvSize matchres_size;
	IplImage *snout_prep = NULL;
	const char **snout_paths = NULL;
	int snout_count;
	assert(args);
	assert(ctx);
	memset(ctx, 0, sizeof(catcierge_template_matcher_t));

	if (args->snout_count == 0)
		return -1;

	snout_paths = args->snout_paths;
	snout_count = args->snout_count;
	ctx->match_flipped = args->match_flipped;
	ctx->match_threshold = args->match_threshold;

	ctx->low_binary_thresh = CATCIERGE_LOW_BINARY_THRESH_DEFAULT;
	ctx->high_binary_thresh = CATCIERGE_HIGH_BINARY_THRESH_DEFAULT;

	// TODO: Make these setable instead.
	ctx->width = CATCIERGE_DEFAULT_RESOLUTION_WIDTH;
	ctx->height = CATCIERGE_DEFUALT_RESOLUTION_HEIGHT;

	for (i = 0; i < snout_count; i++)
	{
		if (access(snout_paths[i], F_OK) == -1)
		{
			fprintf(stderr, "No such file %s\n", snout_paths[i]);
			return -1;
		}
	}

	if (!(ctx->storage = cvCreateMemStorage(0)))
	{
		return -1;
	}

	if (!(ctx->kernel = cvCreateStructuringElementEx(3, 3, 0, 0, CV_SHAPE_RECT, NULL)))
	{
		return -1;
	}

	ctx->snout_count = snout_count;

	// Load the snout images.
	ctx->snouts = (IplImage **)calloc(snout_count, sizeof(IplImage *));
	ctx->flipped_snouts = (IplImage **)calloc(snout_count, sizeof(IplImage *));
	ctx->matchres = (IplImage **)calloc(snout_count, sizeof(IplImage *));

	if (!ctx->snouts || !ctx->flipped_snouts || !ctx->matchres)
	{
		fprintf(stderr, "Out of memory!\n");
		exit(1);
	}

	for (i = 0; i < snout_count; i++)
	{
		if (!(snout_prep = cvLoadImage(snout_paths[i], 1)))
		{
			fprintf(stderr, "Failed to load snout image: %s\n", snout_paths[i]);
			return -1;
		}

		snout_size = cvGetSize(snout_prep);

		if (!(ctx->snouts[i] = cvCreateImage(snout_size, 8, 1)))
		{
			cvReleaseImage(&snout_prep);
			return -1;
		}

		if (_catcierge_prepare_img(ctx, snout_prep, ctx->snouts[i]))
		{
			fprintf(stderr, "Failed to prepare snout image: %s\n", snout_paths[i]);
			return -1;
		}

		// Flip so we can match for going out as well (and not fail the match).
		ctx->flipped_snouts[i] = cvCloneImage(ctx->snouts[i]);
		cvFlip(ctx->snouts[i], ctx->flipped_snouts[i], 1);

		// Setup a matchres image for each snout
		// (We can share these for normal and flipped snouts).
		matchres_size = cvSize(ctx->width  - snout_size.width + 1, 
							   ctx->height - snout_size.height + 1);
		ctx->matchres[i] = cvCreateImage(matchres_size, IPL_DEPTH_32F, 1);

		cvReleaseImage(&snout_prep);
	}

	return 0;
}

void catcierge_template_matcher_destroy(catcierge_template_matcher_t *ctx)
{
	size_t i;
	assert(ctx);

	if (ctx->snouts)
	{
		for (i = 0; i < ctx->snout_count; i++)
		{
			cvReleaseImage(&ctx->snouts[i]);
			ctx->snouts[i] = NULL;
		}

		free(ctx->snouts);
		ctx->snouts = NULL;
	}

	if (ctx->flipped_snouts)
	{
		for (i = 0; i < ctx->snout_count; i++)
		{
			cvReleaseImage(&ctx->flipped_snouts[i]);
			ctx->flipped_snouts[i] = NULL;
		}

		free(ctx->flipped_snouts);
		ctx->flipped_snouts = NULL;
	}

	if (ctx->storage)
	{
		cvReleaseMemStorage(&ctx->storage);
		ctx->storage = NULL;
	}

	if (ctx->kernel)
	{
		cvReleaseStructuringElement(&ctx->kernel);
		ctx->kernel = NULL;
	}

	if (ctx->matchres)
	{
		for (i = 0; i < ctx->snout_count; i++)
		{
			cvReleaseImage(&ctx->matchres[i]);
		}

		free(ctx->matchres);
		ctx->matchres = NULL;
	}
}

// TODO: Replace match_rects with a match_result struct that includes
// match_rect and result value for each snout image.
double catcierge_template_matcher_match(catcierge_template_matcher_t *ctx, const IplImage *img,
						CvRect *match_rects, size_t rect_count, 
						int *flipped)
{
	IplImage *img_cpy = NULL;
	IplImage *img_prep = NULL;
	CvPoint min_loc;
	CvPoint max_loc;
	CvSize img_size = cvGetSize(img);
	CvSize snout_size;
	double min_val;
	double max_val;
	double match_sum = 0.0;
	double match_avg = 0.0;
	size_t i;
	assert(ctx);

	if ((img_size.width != ctx->width)
	 || (img_size.height != ctx->height))
	{
		fprintf(stderr, "Match image must have size %dx%d\n", ctx->width, ctx->height);
		return -1;
	}

	img_cpy = cvCreateImage(img_size, 8, 1);

	if (!(img_prep = cvCloneImage(img)))
	{
		fprintf(stderr, "Failed to clone match image\n");
		return -1;
	}

	if (_catcierge_prepare_img(ctx, img_prep, img_cpy))
	{
		fprintf(stderr, "Failed to prepare match image\n");
		cvReleaseImage(&img_cpy);
		cvReleaseImage(&img_prep);
		return -1;
	}

	if (flipped)
	{
		*flipped = 0;
	}

	// First check normal facing snouts.
	for (i = 0; i < ctx->snout_count; i++)
	{
		// This is only used for returning match_rect.
		snout_size = cvGetSize(ctx->snouts[i]);

		// Try to match the snout with the image.
		// If we find it, the max_val should be close to 1.0
		cvMatchTemplate(img_cpy, ctx->snouts[i], ctx->matchres[i], CV_TM_CCOEFF_NORMED);
		cvMinMaxLoc(ctx->matchres[i], &min_val, &max_val, &min_loc, &max_loc, NULL);

		if (ctx->debug)
		{
			cvShowImage("Match image", img_cpy);
			cvShowImage("Match template", ctx->matchres[i]);
		}

		match_sum += max_val;

		if (i < rect_count) 
		{
			match_rects[i] = cvRect(max_loc.x, max_loc.y, snout_size.width, snout_size.height);
		}
	}

	match_avg = match_sum / ctx->snout_count;

	// TODO: Do all normal facing snouts first, and THEN if the
	// average isn't a good match, try the flipped snouts instead..

	// If we fail the match, try the flipped snout as well.
	if (ctx->match_flipped 
		&& ctx->flipped_snouts
		&& (match_avg < ctx->match_threshold))
	{
		match_sum = 0.0;

		for (i = 0; i < ctx->snout_count; i++)
		{
			snout_size = cvGetSize(ctx->flipped_snouts[i]);
			cvMatchTemplate(img_cpy, ctx->flipped_snouts[i], ctx->matchres[i], CV_TM_CCOEFF_NORMED);
			cvMinMaxLoc(ctx->matchres[i], &min_val, &max_val, &min_loc, &max_loc, NULL);

			match_sum += max_val;

			if (i < rect_count) 
			{
				match_rects[i] = cvRect(max_loc.x, max_loc.y, snout_size.width, snout_size.height);
			}
		}

		match_avg = match_sum / ctx->snout_count;

		// Only qualify as OUT if it was a good match.
		if ((match_avg >= ctx->match_threshold) && flipped)
		{
			*flipped = 1;
		}
	}

	cvReleaseImage(&img_cpy);
	cvReleaseImage(&img_prep);

	return match_avg;
}

int catcierge_template_matcher_is_frame_obstructed(catcierge_template_matcher_t *ctx, IplImage *img)
{
	CvSize size;
	int w;
	int h;
	int x;
	int y;
	int expected_sum;
	IplImage *tmp = NULL;
	IplImage *tmp2 = NULL;
	CvScalar sum;
	assert(ctx);

	// Get a suitable Region Of Interest (ROI)
	// in the center of the image.
	// (This should contain only the white background)
	size = cvGetSize(img);
	w = (int)(size.width * 0.5);
	h = (int)(size.height * 0.1);
	x = (size.width - w) / 2;
	y = (size.height - h) / 2;

	// When there is nothing in our ROI, we
	// expect the thresholded image to be completely
	// white. 255 signifies white.
	expected_sum = (w * h) * 255;

	cvSetImageROI(img, cvRect(x, y, w, h));

	// Only covert to grayscale if needed.
	if (img->nChannels != 1)
	{
		tmp = cvCreateImage(cvSize(w, h), 8, 1);
		cvCvtColor(img, tmp, CV_BGR2GRAY);
	}
	else
	{
		tmp = img;
	}

	// Get a binary image and sum the pixel values.
	tmp2 = cvCreateImage(cvSize(w, h), 8, 1);
	cvThreshold(tmp, tmp2,
		ctx->low_binary_thresh,
		ctx->high_binary_thresh,
		CV_THRESH_BINARY);

	sum = cvSum(tmp2);

	if (ctx->debug)
	{
		cvShowImage("Matchable", tmp2);
	}

	cvSetImageROI(img, cvRect(0, 0, size.width, size.height));

	if (img->nChannels != 1)
	{
		cvReleaseImage(&tmp);
	}

	cvReleaseImage(&tmp2);

	return ((int)sum.val[0] != expected_sum);
}

void catcierge_template_matcher_usage()
{
	fprintf(stderr, " --snout <paths>        Path to the snout images to use. If more than \n");
	fprintf(stderr, "                        one path is given, the average match result is used.\n");
	fprintf(stderr, " --threshold <float>    Match threshold as a value between 0.0 and 1.0.\n");
	fprintf(stderr, "                        Default %.1f\n", DEFAULT_MATCH_THRESH);
	fprintf(stderr, " --match_flipped <0|1>  Match a flipped version of the snout\n");
	fprintf(stderr, "                        (don't consider going out a failed match). Default on.\n");
	fprintf(stderr, "\n");
}

int catcierge_template_matcher_parse_args(catcierge_template_matcher_args_t *args, const char *key, char **values, size_t value_count)
{
	int i;

	printf("Parse: \"%s\" %s %zu\n", key, values[0], value_count);

	if (!strcmp(key, "match_flipped"))
	{
		if (value_count == 1)
		{
			args->match_flipped = atoi(values[0]);
		}
		else
		{
			args->match_flipped = 1;
		}

		return 0;
	}

	// On the command line you can specify multiple snouts like this:
	// --snout_paths <path1> <path2> <path3>
	// or 
	// --snout_path <path1> --snout_path <path2>
	// In the config file you do
	// snout_path=<path1>
	// snout_path=<path2>
	if (!strcmp(key, "snout") || 
		!strcmp(key, "snout_paths") || 
		!strcmp(key, "snout_path"))
	{
		if (value_count == 0)
		{
			fprintf(stderr, "No value given\n");
			return -1;
		}

		for (i = 0; i < value_count; i++)
		{
			if (args->snout_count >= MAX_SNOUT_COUNT)
			{
				fprintf(stderr, "Max snout count reached %d\n", MAX_SNOUT_COUNT);
				return -1;
			}
			args->snout_paths[args->snout_count] = values[i];
			args->snout_count++;
		}

		return 0;
	}

	if (!strcmp(key, "threshold"))
	{
		if (value_count == 1)
		{
			args->match_threshold = atof(values[0]);
		}
		else
		{
			args->match_threshold = DEFAULT_MATCH_THRESH;
		}

		return 0;
	}

	return 1;
}

void catcierge_template_matcher_print_settings(catcierge_template_matcher_args_t *args)
{
	size_t i;
	assert(args);

	printf("Template Matcher:\n");
	printf(" Snout image(s): %s\n", args->snout_paths[0]);
	for (i = 1; i < args->snout_count; i++)
	{
		printf("                 %s\n", args->snout_paths[i]);
	}
	printf("  Match threshold: %.2f\n", args->match_threshold);
	printf("    Match flipped: %d\n", args->match_flipped);
	printf("\n");
}

void catcierge_template_matcher_args_init(catcierge_template_matcher_args_t *args)
{
	assert(args);
	memset(args, 0, sizeof(catcierge_template_matcher_args_t));
	args->match_threshold = DEFAULT_MATCH_THRESH;
	args->match_flipped = 1;
	args->snout_count = 0;
}


