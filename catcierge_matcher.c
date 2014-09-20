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
