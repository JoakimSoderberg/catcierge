
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdlib.h>
#include "catsnatch.h"
#include "RaspiCamCV.h"
#include <signal.h>

int running = 1;

void sig_handler(int signo)
{
	if (signo == SIGINT)
	{
		printf("Received SIGINT, stopping...\n");
		running = 0;
	}
}

int main(int argc, char **argv)
{
	catsnatch_t ctx;
	char *snout_path = NULL;
	IplImage* img = NULL;
	CvRect match_rect;
	CvScalar match_color;
	int match_res = 0;
	int show = 0;
	int i;
	RaspiCamCvCapture *capture = NULL;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s [--show] [snout image]\n", argv[0]);
		return -1;
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
		fprintf(stderr, "Failed to set SIGINT handler\n");
	}

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--show"))
		{
			show = 1;
		}

		snout_path = argv[i];
	}

	if (!snout_path)
	{
		snout_path = "snout.png";
	}

	if (catsnatch_init(&ctx, snout_path))
	{
		fprintf(stderr, "Failed to init catsnatch lib!\n");
		return -1;
	}
	
	if (show)
	{
		cvNamedWindow("catsnatch", 1);
	}

	capture = raspiCamCvCreateCameraCapture(0);

	do
	{
		img = raspiCamCvQueryFrame(capture);

		// Wait until the middle frame

		if ((match_res = catsnatch_match(&ctx, img, &match_rect)) < 0)
		{
			fprintf(stderr, "Error when matching frame!\n");
			continue;
		}

		if (match_res)
		{
			printf("Match!\n");
		}
		else
		{
			printf("No match!\n");
		}

		if (show)
		{
			match_color = match_res ? CV_RGB(255, 0, 0) : CV_RGB(0, 255, 0);
			cvRectangleR(img, match_rect, match_color, 1, 8, 0);
			cvShowImage("catsnatch", img);
		}
	} while (running);

	raspiCamCvReleaseCapture(&capture);
	
	if (show)
	{
		cvDestroyWindow("catsnatch");
	}

	catsnatch_destroy(&ctx);

	return 0;
}

