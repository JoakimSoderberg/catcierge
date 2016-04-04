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

	if (!(*ctx)->is_obstructed)
	{
		(*ctx)->is_obstructed = catcierge_is_frame_obstructed;
	}

	(*ctx)->args = args;

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

int catcierge_get_back_light_area(catcierge_matcher_t *ctx, IplImage *img, CvRect *r)
{
	int ret = 0;
	CvSeq *contours = NULL;
	double max_area = 0.0;
	double area;
	CvSeq *biggest_contour = NULL;
	CvSeq *it = NULL;
	IplImage *tmp = NULL;
	IplImage *tmp2 = NULL;
	CvMemStorage *storage = NULL;
	catcierge_matcher_args_t *args = ctx->args;
	assert(ctx);
	assert(r);

	if (!(storage = cvCreateMemStorage(0)))
	{
		return -1;
	}

	// Only covert to grayscale if needed.
	if (img->nChannels != 1)
	{
		tmp = cvCreateImage(cvGetSize(img), 8, 1);
		cvCvtColor(img, tmp, CV_BGR2GRAY);
	}
	else
	{
		tmp = img;
	}

	// Get a binary image.
	tmp2 = cvCreateImage(cvGetSize(img), 8, 1);
	cvThreshold(tmp, tmp2, args->auto_roi_thr, 255, CV_THRESH_BINARY);

	cvFindContours(tmp2, storage, &contours,
		sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));

	it = contours;
	while (it)
	{
		area = cvContourArea(it, CV_WHOLE_SEQ, 0);

		if (area > max_area)
		{
			max_area = area;
			biggest_contour = it;
		}

		it = it->h_next;
	}

	if (!biggest_contour)
	{
		CATERR("Failed to find back light!\n");
		ret = -1; goto fail;
	}

	CATLOG("Back light found with area %0.0f (which is greater than the minimum allowed %d)\n",
		max_area, ctx->args->min_backlight);

	if (max_area < (double)ctx->args->min_backlight)
	{
		CATERR("Failed to find back light!\n");
		CATERR("Back light area too small %0.0f expecting %d or bigger\n",
				max_area, ctx->args->min_backlight);
		ret = -1; goto fail;
	}

	*r = cvBoundingRect(biggest_contour, 0);

	// TODO: Make it a flag to set the path where to save (and if) this image.
	//if (ctx->args->save_auto_roi)
	{
		IplImage *roi_img = NULL;
		//catcierge_make_path(mg->obstruct_path);
		//cvSaveImage(mg->obstruct_full_path, mg->obstruct_img, 0);
		roi_img = cvCloneImage(img);
		cvDrawContours(roi_img, biggest_contour, cvScalarAll(255), cvScalarAll(0), -1, CV_FILLED, 8, cvPoint(0,0));
		cvRectangleR(roi_img, *r, CV_RGB(255, 0, 0), 2, 8, 0);
		cvSaveImage("./auto_roi.png", roi_img, 0);
		cvReleaseImage(&roi_img);
	}

fail:
	if (img->nChannels != 1)
	{
		cvReleaseImage(&tmp);
	}

	cvReleaseImage(&tmp2);
	cvReleaseMemStorage(&storage);

	return ret;
}

int catcierge_is_frame_obstructed(catcierge_matcher_t *ctx, IplImage *img)
{
	CvSize size;
	int w;
	int h;
	int x;
	int y;
	int sum;
	IplImage *tmp = NULL;
	IplImage *tmp2 = NULL;
	CvRect orig_roi = cvGetImageROI(img);
	CvRect *roi;
	assert(ctx);

	roi = ctx->args->roi;

	if (roi && (roi->width != 0) && (roi->height != 0))
	{
		cvSetImageROI(img, *roi);
	}

	// Get a suitable Region Of Interest (ROI)
	// in the center of the image.
	// (This should contain only the white background)

	size = cvGetSize(img);
	w = (int)(size.width / 2);
	h = (int)(size.height * 0.1);
	x = (roi ? roi->x : 0) + (size.width - w) / 2;
	y = (roi ? roi->y : 0) + (size.height - h) / 2;

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

	#if 0
	{
		// NOTE! Since this function this runs very often, this should
		// only ever be turned on while developing, it will spam ALOT.
		//cvRectangleR(img, cvRect(x, y, w, h), CV_RGB(255, 0, 0), 2, 8, 0);
		cvShowImage("obstruct_roi", img);

		printf("\nroi: x: %d, y: %d, w: %d, h:%d\n",
			roi.x, roi.y, roi.width, roi.height);
		printf("size: w: %d, h: %d\n", size.width, size.height);
		printf("x: %d, y: %d, w: %d, h: %d\n", x, y, w, h);

		printf("Sum: %d\n", sum);
		cvShowImage("obstruct", tmp2);
		//cvWaitKey(0);
	}
	#endif

	cvSetImageROI(img, orig_roi);

	if (img->nChannels != 1)
	{
		cvReleaseImage(&tmp);
	}

	cvReleaseImage(&tmp2);

	// Spiders and other 1 pixel creatures need not bother!
	return ((int)sum > 200);
}
