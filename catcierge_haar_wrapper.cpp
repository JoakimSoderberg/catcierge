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

