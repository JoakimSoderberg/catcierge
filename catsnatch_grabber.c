
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdlib.h>
#include "catsnatch.h"
#include "RaspiCamCV.h"
#include <signal.h>
#include <time.h>
#include "catsnatch_gpio.h"
#include <stdarg.h>

#define PIBORG1	4
#define PIBORG2	18
#define PIBORG3	8
#define PIBORG4	7

#define DOOR_PIN		PIBORG1
#define BACKLIGHT_PIN	PIBORG2

#define MATCH_THRESH 0.8			// The threshold signifying a good match returned by catsnatch_match.
#define DEFAULT_LOCKOUT_TIME 30		// The default lockout length after a none-match
#define DEFAULT_MATCH_WAIT 	 30				// How long to wait after a match try before we match again.

char *snout_path = NULL;
int running = 1;	// Main loop is running (SIGINT will kill it).
int show = 0;		// Show the output video (X11 only).
int show_fps = 1;
struct timeval now = {0, 0};

// Lockout (when there's an invalid match).
int lockout_time = DEFAULT_LOCKOUT_TIME;
int lockout = 0;
double lockout_elapsed = 0.0;
struct timeval lockout_start = {0, 0};

// Consecutive matches decide lockout status.
int matches[4];
#define MATCH_MAX_COUNT (sizeof(matches) / sizeof(matches[0]))
IplImage *match_images[MATCH_MAX_COUNT];
int match_count = 0;
struct timeval match_start;
int match_time = DEFAULT_MATCH_WAIT;

// FPS.
unsigned int fps = 0;
struct timeval start;
struct timeval end;
unsigned int frames = 0;
double elapsed = 0.0;

static void sig_handler(int signo)
{
	if (signo == SIGINT)
	{
		printf("Received SIGINT, stopping...\n");
		running = 0;
	}
}

static void log_print(const char *fmt, ...)
{

}

static int do_lockout()
{
	if (lockout_time)
	{
		gpio_write(DOOR_PIN, 1);
	}

	gpio_write(BACKLIGHT_PIN, 1);
}

static int do_unlock()
{
	gpio_write(DOOR_PIN, 0);
	gpio_write(BACKLIGHT_PIN, 1);
}

static int setup_gpio()
{
	int ret = 0;

	// Set export for pins.
	if (gpio_export(DOOR_PIN) || gpio_set_direction(DOOR_PIN, OUT))
	{
		fprintf(stderr, "Failed to export and set direction for door pin\n");
		ret = -1;
		goto fail;
	}

	if (gpio_export(BACKLIGHT_PIN) || gpio_set_direction(BACKLIGHT_PIN, OUT))
	{
		fprintf(stderr, "Failed to export and set direction for backlight pin\n");
		ret = -1;
		goto fail;
	}

	// Start with the door open and light on.
	gpio_write(DOOR_PIN, 0);
	gpio_write(BACKLIGHT_PIN, 1);

fail:
	if (ret)
	{
		// Check if we're root.
		if (getuid() != 0)
		{
			fprintf(stderr, "You might have to run as root!\n");
		}
	}

	return ret;
}

static void should_we_lockout(double match_res)
{
	int i;

	// Keep track of consecutive frames and their match status. 
	// If 2 out of 4 are OK, keep the door open, otherwise CLOSE!
	matches[match_count] = (int)(match_res >= MATCH_THRESH);
	match_count++;

	if (match_count >= MATCH_MAX_COUNT)
	{
		int count = 0;

		for (i = 0; i < MATCH_MAX_COUNT; i++)
		{
			count += matches[i];
		}

		if (count >= (MATCH_MAX_COUNT - 2))
		{
			// Make sure the door is open.
			do_unlock();

			// We have done our matches for this time, if we try to
			// match anything more we will most likely just get the
			// body of the cat which will be a no-match.
			gettimeofday(&match_start, NULL);
		}
		else
		{
			do_lockout();
			lockout = 1;
			gettimeofday(&lockout_start, NULL);

			match_start.tv_sec = 0;
			match_start.tv_usec = 0;
		}

		match_count = 0;
	}
}

static int enough_time_since_last_match()
{
	double last_match_time;

	// No match has been made so it's time!
	if (match_start.tv_sec == 0)
	{
		return 1;
	}

	gettimeofday(&now, NULL);

	last_match_time = (now.tv_sec - match_start.tv_sec) + 
					 ((now.tv_usec - match_start.tv_usec) / 1000000.0);

	if (last_match_time >= match_time)
	{
		match_start.tv_sec = 0;
		match_start.tv_usec = 0;
		return 1;
	}

	return 0;
}

static void print_status()
{
	frames++;
	gettimeofday(&end, NULL);

	elapsed += (end.tv_sec - start.tv_sec) + 
				((end.tv_usec - start.tv_usec) / 1000000.0);

	if (elapsed >= 1.0)
	{
		char buf[32];

		if (lockout)
		{
			printf("Lockout for %f more seconds.\n", (lockout_time - lockout_elapsed));
		}

		fps = frames;
		printf("%d fps\n", fps);
		/*
		snprintf(buf, sizeof(buf), "%d fps", frames);
		printf("\033[1;40H");
		printf("%s", buf);
		//printf("%d fps\n", frames);
		//printf("\033[20D"); // Move cursor to beginning of row.
		//printf("\033[1A");	// Move the cursor back up.
		*/
		frames = 0;
		elapsed = 0.0;
	}
}

static void parse_cmdargs(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--show"))
		{
			show = 1;
		}

		if (!strcmp(argv[i], "--show_fps"))
		{
			show_fps = 1;
		}

		if (!strcmp(argv[i], "--lockout"))
		{
			if (argc >= (i + 1))
			{
				i++;
				lockout_time = atoi(argv[i]);
				continue;
			}
			else
			{
				lockout_time = DEFAULT_LOCKOUT_TIME;
			}
		}

		if (!strcmp(argv[i], "--matchtime"))
		{
			if (argc >= (i + 1))
			{
				i++;
				match_time = atoi(argv[i]);
				continue;
			}
			else
			{
				match_time = DEFAULT_MATCH_WAIT;
			}
		}

		snout_path = argv[i];
	}
}

int main(int argc, char **argv)
{
	catsnatch_t ctx;
	IplImage* img = NULL;
	CvRect match_rect;
	CvScalar match_color;
	double match_res = 0;
	int do_match = 0;
	int i;
	RaspiCamCvCapture *capture = NULL;

	if (argc < 2)
	{
		fprintf(stderr, "Catsnatch Grabber (C) Joakim Soderberg 2013\n");
		fprintf(stderr, "Usage: %s [--lockout <seconds>] [--show] [snout image]\n\n", argv[0]);
		fprintf(stderr, " --lockout <seconds>    The time in seconds a lockout takes. Default %ds\n", DEFAULT_LOCKOUT_TIME);
		fprintf(stderr, " --matchtime <seconds>  The time to wait after a match. Default %ds\n", DEFAULT_MATCH_WAIT);
		fprintf(stderr, " --show                 Show GUI of the camera feed (X11 only).\n");
		fprintf(stderr, " --show_fps             Show FPS.\n");
		fprintf(stderr, "\nThe snout image refers to the image of the cat snout that is matched against.\n");
		fprintf(stderr, "This image should be based on a 320x240 resolution image taken by the rpi camera.\n");
		fprintf(stderr, "If no path is specified \"snout.png\" in the current directory is used.\n\n");
		return -1;
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
		fprintf(stderr, "Failed to set SIGINT handler\n");
	}

	parse_cmdargs(argc, argv);

	if (!snout_path)
	{
		snout_path = "snout.png";
	}

	printf("--------------------------------------------------------------------------------\n");
	printf("Settings:\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("  Snout image: %s\n", snout_path);
	printf("  Show video: %d\n", show);
	printf("  Lock time: %d seconds\n", lockout_time);
	printf("  Match timeout: %d seconds\n", match_time);
	printf("--------------------------------------------------------------------------------\n");

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
			gettimeofday(&now, NULL);

			lockout_elapsed = (now.tv_sec - lockout_start.tv_sec) + 
							 ((now.tv_usec - lockout_start.tv_usec) / 1000000.0);

			if (lockout_elapsed >= lockout_time)
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
			// Wait for a timeout in that case until we try to match again.
			if (enough_time_since_last_match())
			{
				// We have something to match against. 
				if ((match_res = catsnatch_match(&ctx, img, &match_rect)) < 0)
				{
					fprintf(stderr, "Error when matching frame!\n");
					goto skiploop;
				}

				printf("%d fps    %f %sMatch\n", fps, match_res, (match_res >= MATCH_THRESH) ? "!!!!!!!" : "No ");

				should_we_lockout(match_res);
			}
		}

		if (show)
		{
			// Draw the match with a green rectangle. (Red if no match).
			if (do_match)
			{
				match_color = (match_res >= MATCH_THRESH) ? CV_RGB(255, 0, 0) : CV_RGB(0, 255, 0);
				cvRectangleR(img, match_rect, match_color, 1, 8, 0);
			}

			cvShowImage("catsnatch", img);
		}
		
skiploop:
		print_status();
	} while (running);

	raspiCamCvReleaseCapture(&capture);
	
	if (show)
	{
		cvDestroyWindow("catsnatch");
	}

	catsnatch_destroy(&ctx);

	return 0;
}

