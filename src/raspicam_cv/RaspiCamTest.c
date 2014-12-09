/*

 Copyright (c) by Emil Valkov,
 All rights reserved.

 License: http://www.opensource.org/licenses/bsd-license.php

*/

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#include "RaspiCamCV.h"

int main(int argc, const char** argv)
{
    RaspiCamCvCapture * capture = raspiCamCvCreateCameraCapture(0); // Index doesn't really matter
	cvNamedWindow("RaspiCamTest", 1);
	do 
	{
		IplImage* image = raspiCamCvQueryFrame(capture);
		cvShowImage("RaspiCamTest", image);
	} while (cvWaitKey(10) < 0);

	cvDestroyWindow("RaspiCamTest");
	raspiCamCvReleaseCapture(&capture);
	return 0;
}
