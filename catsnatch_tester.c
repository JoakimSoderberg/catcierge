//
// This file is part of the Catsnatch project.
//
// Copyright (c) Joakim Soderberg 2013-2014
//
//    Catsnatch is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//    Catsnatch is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Catsnatch.  If not, see <http://www.gnu.org/licenses/>.
//
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdlib.h>
#include "catsnatch.h"

int main(int argc, char **argv)
{
	catsnatch_t ctx;
	char *snout_path = NULL;
	char *img_path = NULL;
	IplImage *img = NULL;
	CvRect match_rect;
	CvScalar match_color;
	int match_res = 0;
	int i;
	int show = 0;

	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s [--show] --snout <snout image> <input image>\n", argv[0]);
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

	if (catsnatch_init(&ctx, snout_path))
	{
		fprintf(stderr, "Failed to init catsnatch lib!\n");
		return -1;
	}

	if (!(img = cvLoadImage(img_path, 1)))
	{
		fprintf(stderr, "Failed to load match image: %s\n", img_path);
		goto fail;
	}

	if ((match_res = catsnatch_match(&ctx, img, &match_rect)) < 0)
	{
		fprintf(stderr, "Something went wrong when matching image: %s\n", img_path);
		catsnatch_destroy(&ctx);
	}

	if (!match_res)
	{
		printf("Match!\n");
		match_color = CV_RGB(0, 255, 0);
	}
	else
	{
		printf("No match!\n");
		match_color = CV_RGB(255, 0, 0);
	}

	cvRectangleR(img, match_rect, match_color, 1, 8, 0);
	
	if (show)
	{
		cvShowImage("hej", img);
		cvWaitKey(0);
	}
fail:
	catsnatch_destroy(&ctx);

	return 0;
}
