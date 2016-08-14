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

#include "catcierge_config.h"
#include <stdio.h>

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#ifdef CATCIERGE_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "catcierge_util.h"
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

	cvDestroyWindow("Auto ROI");

	*ctx = NULL;
}

static void _catcierge_display_auto_roi_images(const IplImage *img, CvSeq *biggest_contour, CvRect *r, int save)
{
	char buf[2048];
	char path[2048];
	IplImage *roi_img = NULL;
	roi_img = cvCloneImage(img);

	if (!getcwd(buf, sizeof(buf) - 1))
	{
		strcpy(buf, ".");
	}

	// Save the original unchanged image.
	if (save)
	{
		snprintf(path, sizeof(path) - 1, "%s%sauto_roi.png", buf, catcierge_path_sep());
		cvSaveImage(path, roi_img, 0);
		CATLOG("Saved auto roi image to: %s\n", path);
	}

	// Highlight ROI.
	cvDrawContours(roi_img, biggest_contour, CV_RGB(0, 255, 0), cvScalarAll(0), 0, 2, 8, cvPoint(0, 0));
	cvRectangleR(roi_img, *r, CV_RGB(255, 0, 0), 2, 8, 0);

	cvShowImage("Auto ROI", roi_img);

	if (save)
	{
		// Save another image with the Region Of Interst (ROI) highlighted.
		snprintf(path, sizeof(path) - 1, "%s%sauto_roi_highlight.png", buf, catcierge_path_sep());
		cvSaveImage(path, roi_img, 0);
		CATLOG("Saved auto roi image to (highlighted): %s\n", path);
	}

	cvReleaseImage(&roi_img);
}

int catcierge_get_back_light_area(catcierge_matcher_t *ctx, const IplImage *img, CvRect *r)
{
	int ret = 0;
	CvSeq *contours = NULL;
	double max_area = 0.0;
	double area;
	CvSeq *biggest_contour = NULL;
	CvSeq *it = NULL;
	IplImage *img_color = NULL;
	IplImage *img_gray = NULL;
	IplImage *img_thr = NULL;
	IplImage *img_eq = NULL;
	CvMemStorage *storage = NULL;
	catcierge_matcher_args_t *args = ctx->args;
	assert(ctx);
	assert(r);

	if (!(storage = cvCreateMemStorage(0)))
	{
		return -1;
	}

	// We want both color (for showing the image) and grayscale.
	if (img->nChannels != 1)
	{
		img_color = cvCloneImage(img);
		img_gray = cvCreateImage(cvGetSize(img), 8, 1);
		cvCvtColor(img, img_gray, CV_BGR2GRAY);
	}
	else
	{
		img_gray = cvCloneImage(img);
		img_color = cvCreateImage(cvGetSize(img), 8, 3);
		cvCvtColor(img_gray, img_color, CV_GRAY2BGR);
	}

	// Equalize image histogram.
	img_eq = cvCreateImage(cvGetSize(img), 8, 1);
	cvEqualizeHist(img_gray, img_eq);

	// Get a binary image.
	img_thr = cvCreateImage(cvGetSize(img), 8, 1);
	cvThreshold(img_eq, img_thr, args->auto_roi_thr, 255, 0);

	cvFindContours(img_thr, storage, &contours,
		sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, cvPoint(0, 0));

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

	_catcierge_display_auto_roi_images(img_color, biggest_contour, r, args->save_auto_roi_img);

fail:
	cvReleaseImage(&img_gray);
	cvReleaseImage(&img_color);
	cvReleaseImage(&img_thr);
	cvReleaseImage(&img_eq);
	cvReleaseMemStorage(&storage);

	return ret;
}

int catcierge_is_frame_obstructed(catcierge_matcher_t *ctx, const IplImage *img)
{
	CvSize size;
	int w;
	int h;
	int x;
	int y;
	int sum;
	IplImage *img_cpy = NULL;
	IplImage *tmp = NULL;
	IplImage *tmp2 = NULL;
	CvRect orig_roi = cvGetImageROI(img);
	CvRect *roi;
	assert(ctx);

	roi = ctx->args->roi;

	img_cpy = cvCloneImage(img);

	if (roi && (roi->width != 0) && (roi->height != 0))
	{
		cvSetImageROI(img_cpy, *roi);
	}

	// Get a suitable Region Of Interest (ROI)
	// in the center of the image.
	// (This should contain only the white background)

	size = cvGetSize(img_cpy);
	w = (int)(size.width / 2);
	h = (int)(size.height * 0.1);
	x = (roi ? roi->x : 0) + (size.width - w) / 2;
	y = (roi ? roi->y : 0) + (size.height - h) / 2;

	cvSetImageROI(img_cpy, cvRect(x, y, w, h));

	// Only covert to grayscale if needed.
	if (img_cpy->nChannels != 1)
	{
		tmp = cvCreateImage(cvSize(w, h), 8, 1);
		cvCvtColor(img_cpy, tmp, CV_BGR2GRAY);
	}
	else
	{
		tmp = img_cpy;
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
		cvShowImage("obstruct_roi", img_cpy);

		printf("\nroi: x: %d, y: %d, w: %d, h:%d\n",
			roi.x, roi.y, roi.width, roi.height);
		printf("size: w: %d, h: %d\n", size.width, size.height);
		printf("x: %d, y: %d, w: %d, h: %d\n", x, y, w, h);

		printf("Sum: %d\n", sum);
		cvShowImage("obstruct", tmp2);
		//cvWaitKey(0);
	}
	#endif

	cvSetImageROI(img_cpy, orig_roi);

	if (img->nChannels != 1)
	{
		cvReleaseImage(&tmp);
	}

	cvReleaseImage(&tmp2);
	cvReleaseImage(&img_cpy);

	// Spiders and other 1 pixel creatures need not bother!
	return ((int)sum > 200);
}
