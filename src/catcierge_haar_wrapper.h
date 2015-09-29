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
#ifndef __CATCIERGE_HAAR_WRAPPER_H__
#define __CATCIERGE_HAAR_WRAPPER_H__

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

// TODO: Fix this properly!
//#include <opencv2/objdetect/objdetect_c.h>

#ifndef CV_HAAR_SCALE_IMAGE
#define CV_HAAR_SCALE_IMAGE 2
#endif

typedef void cv2CascadeClassifier;

#ifdef __cplusplus
extern "C" 
{
#endif

cv2CascadeClassifier *cv2CascadeClassifier_create();
void cv2CascadeClassifier_destroy(cv2CascadeClassifier *c);
int cv2CascadeClassifier_load(cv2CascadeClassifier *c, const char *filename);
int cv2CascadeClassifier_detectMultiScale(cv2CascadeClassifier *c,
	const IplImage *img, CvRect *objects, size_t *object_count,
	double scale_factor, int min_neighbours, int flags,
	CvSize *min_size, CvSize *max_size);

#ifdef __cplusplus
}
#endif

#endif // __CATCIERGE_HAAR_WRAPPER_H__
