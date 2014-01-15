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
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdlib.h>
#include "catcierge.h"

int main(int argc, char **argv)
{
	catcierge_t ctx;
	char *snout_path = NULL;
	char *img_path = NULL;
	IplImage *img = NULL;
	CvRect match_rect;
	CvScalar match_color;
	double match_res = 0;
	int i;
	int show = 0;
	double match_threshold = 0.8;
	int match_flipped = 1;
	int was_flipped = 0;

	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s [--show] [--match_flipped <0|1>] [--threshold] --snout <snout image> <input image>\n", argv[0]);
		return -1;
	}

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--show"))
		{
			show = 1;
		}
		else if (!strcmp(argv[i], "--snout"))
		{
			if (argc >= (i + 1))
			{
				i++;
				snout_path = argv[i];
				continue;
			}
		}
		else if (!strcmp(argv[i], "--threshold"))
		{
			if (argc >= (i + 1))
			{
				i++;
				match_threshold = atof(argv[i]);
				continue;
			}
		}
		else if (!strcmp(argv[i], "--match_flipped"))
		{
			if (argc >= (i + 1))
			{
				i++;
				match_flipped = atoi(argv[i]);
				continue;
			}
		}

		img_path = argv[i];
	}

	if (!snout_path)
	{
		fprintf(stderr, "No snout image specified\n");
		return -1;
	}

	if (!img_path)
	{
		fprintf(stderr, "No input image specified\n");
		return -1;
	}

	if (catcierge_init(&ctx, snout_path, match_flipped, match_threshold))
	{
		fprintf(stderr, "Failed to init catcierge lib!\n");
		return -1;
	}

	if (!(img = cvLoadImage(img_path, 1)))
	{
		fprintf(stderr, "Failed to load match image: %s\n", img_path);
		goto fail;
	}

	if ((match_res = catcierge_match(&ctx, img, &match_rect, &was_flipped)) < 0)
	{
		fprintf(stderr, "Something went wrong when matching image: %s\n", img_path);
		catcierge_destroy(&ctx);
	}

	if (match_res >= match_threshold)
	{
		printf("Match%s! %f\n", was_flipped ? " (Flipped)": "", match_res);
		match_color = CV_RGB(0, 255, 0);
	}
	else
	{
		printf("No match! %f\n", match_res);
		match_color = CV_RGB(255, 0, 0);
	}

	cvRectangleR(img, match_rect, match_color, 1, 8, 0);
	
	if (show)
	{
		cvShowImage("hej", img);
		cvWaitKey(0);
	}
fail:
	catcierge_destroy(&ctx);

	return 0;
}
