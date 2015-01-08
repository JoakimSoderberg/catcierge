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

#include <stdio.h>

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#include "catcierge_matcher.h"
#include "catcierge_template_matcher.h"
#include "catcierge_haar_matcher.h"
#include "catcierge_log.h"

int catcierge_matcher_init(catcierge_matcher_t **ctx, catcierge_matcher_args_t *args)
{
	*ctx = NULL;

	if (args->type == MATCHER_TEMPLATE)
	{
		if (catcierge_template_matcher_init(ctx, args))
		{
			return -1;
		}
	}
	else if (args->type == MATCHER_HAAR)
	{
		if (catcierge_haar_matcher_init(ctx, args))
		{
			return -1;
		}
	}
	else
	{
		CATERR("Failed to init matcher. Invalid matcher type given\n");
		return -1;
	}

	return 0;
}

void catcierge_matcher_destroy(catcierge_matcher_t **ctx)
{
	if (*ctx)
	{
		catcierge_matcher_t *c = *ctx;

		if (c->type == MATCHER_TEMPLATE)
		{
			catcierge_template_matcher_destroy(ctx);
		}
		else if (c->type == MATCHER_HAAR)
		{
			catcierge_haar_matcher_destroy(ctx);
		}
	}

	*ctx = NULL;
}

int catcierge_is_frame_obstructed(IplImage *img, int debug)
{
	CvSize size;
	int w;
	int h;
	int x;
	int y;
	int sum;
	IplImage *tmp = NULL;
	IplImage *tmp2 = NULL;
	CvRect roi = cvGetImageROI(img);

	// Get a suitable Region Of Interest (ROI)
	// in the center of the image.
	// (This should contain only the white background)
	size = cvGetSize(img);
	w = (int)(size.width * 0.5);
	h = (int)(size.height * 0.1);
	x = (size.width - w) / 2;
	y = (size.height - h) / 2;

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
	cvThreshold(tmp, tmp2, 90, 255, CV_THRESH_BINARY_INV);

	sum = (int)cvSum(tmp2).val[0] / 255;

	if (debug)
	{
		printf("Sum: %d\n", sum);
		cvShowImage("obstruct", tmp2);
	}

	cvSetImageROI(img, cvRect(roi.x, roi.y, roi.width, roi.height));

	if (img->nChannels != 1)
	{
		cvReleaseImage(&tmp);
	}

	cvReleaseImage(&tmp2);

	// Spiders and other 1 pixel creatures need not bother!
	return ((int)sum > 200);
}
