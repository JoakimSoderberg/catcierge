
#include <opencv/cv.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	IplImage* snout = NULL;
	IplImage* img = NULL;

	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <snout image> <input image>\n");
		return -1;
	}

	snout = cvLoadImage(argv[1]);
	img = cvLoadImage(argv[2]);

	cvShowImage("hej", img);
	cvWaitKey(0);
	printf("hej\n");
	return 0;
}