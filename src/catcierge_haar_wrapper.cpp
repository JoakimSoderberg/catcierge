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
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>

using namespace std;
using namespace cv;

#include "catcierge_haar_wrapper.h"

#ifdef __cplusplus
extern "C" 
{
#endif

cv2CascadeClassifier *cv2CascadeClassifier_create()
{
	CascadeClassifier *cc = new CascadeClassifier();
	return (cv2CascadeClassifier *)cc;
}

void cv2CascadeClassifier_destroy(cv2CascadeClassifier *c)
{
	CascadeClassifier *cc = (CascadeClassifier *)c;
	delete cc;
}

int cv2CascadeClassifier_load(cv2CascadeClassifier *c, const char *filename)
{
	CascadeClassifier *cc = (CascadeClassifier *)c;

	if (!cc->load(filename))
	{
		return -1;
	}

	return 0;
}

int cv2CascadeClassifier_detectMultiScale(cv2CascadeClassifier *c,
	const IplImage *img, CvRect *objects, size_t *object_count,
	double scale_factor, int min_neighbours, int flags,
	CvSize *min_size, CvSize *max_size)
{
	size_t i;
	assert(c);
	assert(objects);
	assert(object_count);
	vector<Rect> objectVector;
	CascadeClassifier *cc = (CascadeClassifier *)c;
	Mat m = img;
	Size minSize = *min_size;
	Size maxSize = *max_size;

	// TODO: Find out why this differs from the Python version.
	cc->detectMultiScale(m, objectVector, scale_factor,
		min_neighbours, flags, minSize, maxSize);

	i = 0;
	for (vector<Rect>::const_iterator r = objectVector.begin();
		r != objectVector.end() && (i < *object_count);
		r++)
	{
		objects[i].x = r->x;
		objects[i].y = r->y;
		objects[i].width = r->width;
		objects[i].height = r->height;
		i++;
	}

	*object_count = objectVector.size();

	return 0;
}

#ifdef __cplusplus
}
#endif

