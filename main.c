
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

	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <snout image> <input image>\n", argv[0]);
		return -1;
	}

	snout_path = argv[1];
	img_path = argv[2];

	if (catsnatch_init(&ctx, snout_path))
	{
		fprintf(stderr, "Failed to init catsnatch lib!\n");
		return -1;
	}

	if (!(img = cvLoadImage(img_path, 1)))
	{
		fprintf(stderr, "Failed to load match image: %s\n", img_path);
	}

	if ((match_res = catsnatch_match(&ctx, img, &match_rect)) < 0)
	{
		fprintf(stderr, "Something went wrong when matching image: %s\n", img_path);
		catsnatch_destroy(&ctx);
	}

	if (!match_res)
	{
		match_color = CV_RGB(0, 255, 0);
	}
	else
	{
		match_color = CV_RGB(255, 0, 0);
	}

	cvRectangleR(img, match_rect, match_color, 1, 8, 0);
	cvShowImage("hej", img);
	cvWaitKey(0);

	catsnatch_destroy(&ctx);

	return 0;
}
