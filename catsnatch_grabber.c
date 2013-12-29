
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdlib.h>
#include "catsnatch.h"
#include "RaspiCamCV.h"
#include <signal.h>
#include <time.h>
#include "catsnatch_gpio.h"

#define PIBORG1	4
#define PIBORG2	18
#define PIBORG3	8
#define PIBORG4	7

#define DOOR_PIN		PIBORG1
#define BACKLIGHT_PIN	PIBORG2

int running = 1;
int lockout_enabled = 0;

void sig_handler(int signo)
{
	if (signo == SIGINT)
	{
		printf("Received SIGINT, stopping...\n");
		running = 0;
	}
}

int do_lockout()
{
	if (lockout_enabled)
	{
		gpio_write(DOOR_PIN, 1);
	}
	
	gpio_write(BACKLIGHT_PIN, 1);
}

int do_unlock()
{
	gpio_write(DOOR_PIN, 0);
	gpio_write(BACKLIGHT_PIN, 1);
}

int setup_gpio()
{
	// Set export for pins.
	if (gpio_export(DOOR_PIN) || gpio_set_direction(DOOR_PIN, OUT))
	{
		fprintf(stderr, "Failed to export and set direction for door pin\n");
		return -1;
	}

	if (gpio_export(BACKLIGHT_PIN) || gpio_set_direction(BACKLIGHT_PIN, OUT))
	{
		fprintf(stderr, "Failed to export and set direction for backlight pin\n");
		return -1;
	}

	// Start with the door ope nand light on.
	gpio_write(DOOR_PIN, 0);
	gpio_write(BACKLIGHT_PIN, 1);

	return 0;
}

int main(int argc, char **argv)
{
	#define MATCH_THRESH 0.8
	catsnatch_t ctx;
	char *snout_path = NULL;
	IplImage* img = NULL;
	CvRect match_rect;
	CvScalar match_color;
	struct timeval start;
	struct timeval end;
	double match_res = 0;
	int show = 0;
	int do_match = 0;
	int i;
	unsigned int frames = 0;
	double elapsed = 0.0;
	RaspiCamCvCapture *capture = NULL;
	int matches[4];
	#define MATCH_MAX_COUNT (sizeof(matches) / sizeof(matches[0]))
	int match_count = 0;
	int first_match = 0;
	int lockout = 0;
	double lockout_elapsed = 0.0;
	struct timeval lockout_start = {0, 0};
	struct timeval lockout_end = {0, 0};
	#define LOCKOUT_TIME 30

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

		if (!strcmp(argv[i], "--lockout"))
		{
			lockout_enabled = 1;
		}

		snout_path = argv[i];
	}

	if (!snout_path)
	{
		snout_path = "snout.png";
	}

	if (setup_gpio())
	{
		fprintf(stderr, "Failed to setup GPIO pins\n");
		return -1;
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
		gettimeofday(&start, NULL);

		img = raspiCamCvQueryFrame(capture);

		// Skip all matching in lockout.
		if (lockout)
		{
			gettimeofday(&lockout_end, NULL);

			lockout_elapsed = (lockout_end.tv_sec - lockout_start.tv_sec) + 
							 ((lockout_end.tv_usec - lockout_start.tv_usec) / 1000000.0);

			if (lockout_elapsed >= LOCKOUT_TIME)
			{
				printf("End of lockout!\n");
				lockout = 0;
				do_unlock();
			}

			goto skiploop;
		}

		// Wait until the middle of the frame is black.
		if ((do_match = catsnatch_is_matchable(&ctx, img)) < 0)
		{
			fprintf(stderr, "Failed to detect matchableness\n");
			goto skiploop;
		}

		if (do_match)
		{
			// We have something to match against. 
			if ((match_res = catsnatch_match(&ctx, img, &match_rect)) < 0)
			{
				fprintf(stderr, "Error when matching frame!\n");
				goto skiploop;
			}

			printf("%f %sMatch\n", match_res, (match_res >= MATCH_THRESH) ? "!!!!!!!" : "No ");

			// Keep track of consecutive frames and their match status. 
			// If 3 out of 4 are OK, keep the door open, otherwise CLOSE!
			matches[match_count] = (int)(match_res >= MATCH_THRESH);
			match_count++;

			if (match_count >= MATCH_MAX_COUNT)
			{
				int count = 0;

				for (i = 0; i < MATCH_MAX_COUNT; i++)
				{
					count += matches[i];
				}

				if (count >= MATCH_MAX_COUNT)
				{
					// Make sure the door is open.
					do_unlock();
				}
				else
				{
					do_lockout();
					lockout = 1;
					gettimeofday(&lockout_start, NULL);
				}

				match_count = 0;
			}
		}
		else
		{
			//printf("Nothing in view...\n");
		}

		if (show)
		{
			if (do_match)
			{
				match_color = (match_res >= MATCH_THRESH) ? CV_RGB(255, 0, 0) : CV_RGB(0, 255, 0);
				cvRectangleR(img, match_rect, match_color, 1, 8, 0);
			}

			cvShowImage("catsnatch", img);
		}
		
skiploop:
		frames++;
		gettimeofday(&end, NULL);

		elapsed += (end.tv_sec - start.tv_sec) + 
					((end.tv_usec - start.tv_usec) / 1000000.0);

		if (elapsed >= 1.0)
		{
			if (lockout)
			{
				printf("Lockout for %f more seconds.\n", (LOCKOUT_TIME - lockout_elapsed));
			}

			printf("%d fps\n", frames);
			//printf("\033[20D"); // Move cursor to beginning of row.
			//printf("\033[1A");	// Move the cursor back up.
			frames = 0;
			elapsed = 0.0;
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

