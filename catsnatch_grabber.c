
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
#ifdef WITH_RFID
#include "catsnatch_rfid.h"
#endif // WITH_RFID

#define PIBORG1	4
#define PIBORG2	18
#define PIBORG3	8
#define PIBORG4	7

#define DOOR_PIN		PIBORG1
#define BACKLIGHT_PIN	PIBORG2

#define MATCH_THRESH 0.8			// The threshold signifying a good match returned by catsnatch_match.
#define DEFAULT_LOCKOUT_TIME 30		// The default lockout length after a none-match
#define DEFAULT_MATCH_WAIT 	 30		// How long to wait after a match try before we match again.

char *snout_path = NULL;
int running = 1;	// Main loop is running (SIGINT will kill it).
int show = 0;		// Show the output video (X11 only).
int show_fps = 1;	// Show FPS in log output.
int saveimg = 1;	// Save match images to disk.
int highlight_match = 0; // Highlight the match in saved images.
struct timeval now = {0, 0};

// Lockout (when there's an invalid match).
int lockout_time = DEFAULT_LOCKOUT_TIME;
int lockout = 0;
double lockout_elapsed = 0.0;
struct timeval lockout_start = {0, 0};

// Consecutive matches decides lockout status.
int matches[4];
#define MATCH_MAX_COUNT (sizeof(matches) / sizeof(matches[0]))
int match_count = 0;
struct timeval match_start;
int match_time = DEFAULT_MATCH_WAIT;
double last_match_time;

typedef struct match_image_s
{
	char path[512];
	IplImage *img;
} match_image_t;

match_image_t match_images[MATCH_MAX_COUNT];

// FPS.
unsigned int fps = 0;
struct timeval start;
struct timeval end;
unsigned int frames = 0;
double elapsed = 0.0;

#ifdef WITH_RFID
char *rfid_inner_path;
char *rfid_outer_path;
catsnatch_rfid_t rfid_in;
catsnatch_rfid_t rfid_out;
catsnatch_rfid_context_t rfid_ctx;

typedef enum rfid_direction_s
{
	RFID_DIR_UNKNOWN = -1,
	RFID_DIR_IN = 0,
	RFID_DIR_OUT = 1
} rfid_direction_t;

typedef struct rfid_match_s
{
	int triggered;
	char data[128];
	int incomplete;
	const char *time_str;
} rfid_match_t;

int rfid_direction = RFID_DIR_UNKNOWN;
rfid_match_t rfid_in_match;
rfid_match_t rfid_out_match;
#endif // WITH_RFID

static char *get_time_str_fmt(char *time_str, size_t len, const char *fmt)
{
	struct tm *tm;
	time_t t;

	t = time(NULL);
	tm = localtime(&t);

	strftime(time_str, len, fmt, tm);

	return time_str;
}

static char *get_time_str(char *time_str, size_t len)
{
	return get_time_str_fmt(time_str, len, "%Y-%m-%d %H:%M:%S");
}

static void log_print(FILE *fd, const char *fmt, ...)
{
	char time_str[256];
	va_list ap;

	get_time_str(time_str, sizeof(time_str));

	va_start(ap, fmt);
	if (show_fps)
	{
		fprintf(fd, "[%s] %d fps  ", time_str, fps);
	}
	else
	{
		fprintf(fd, "[%s]  ", time_str);
	}
	vprintf(fmt, ap);
	va_end(ap);
}

#define CATLOG(fmt, ...) log_print(stdout, fmt, ##__VA_ARGS__)
#define CATERR(fmt, ...) log_print(stderr, fmt, ##__VA_ARGS__)

static void sig_handler(int signo)
{
	if (signo == SIGINT)
	{
		CATLOG("Received SIGINT, stopping...\n");
		running = 0;
	}
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
		CATERR("Failed to export and set direction for door pin\n");
		ret = -1;
		goto fail;
	}

	if (gpio_export(BACKLIGHT_PIN) || gpio_set_direction(BACKLIGHT_PIN, OUT))
	{
		CATERR("Failed to export and set direction for backlight pin\n");
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
			CATERR("You might have to run as root!\n");
		}
	}

	return ret;
}

static void save_images()
{
	int i;

	for (i = 0; i < MATCH_MAX_COUNT; i++)
	{
		CATLOG("Saving image %s\n", match_images[i].path);
		cvSaveImage(match_images[i].path, match_images[i].img, 0);
		cvReleaseImage(&match_images[i].img);
		match_images[i].img = NULL;
	}
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

		// Now we can save the images without slowing
		// down the matching FPS.
		if (saveimg)
		{
			save_images();
		}
	}
}

static int enough_time_since_last_match()
{
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
		CATLOG("End of match wait...\n");
		match_start.tv_sec = 0;
		match_start.tv_usec = 0;

		#ifdef WITH_RFID
		memset(&rfid_in_match, 0, sizeof(rfid_in_match));
		memset(&rfid_out_match, 0, sizeof(rfid_out_match));
		rfid_direction = RFID_DIR_UNKNOWN;
		#endif // WITH_RFID

		return 1;
	}

	return 0;
}

static void calculate_fps()
{
	frames++;
	gettimeofday(&end, NULL);

	elapsed += (end.tv_sec - start.tv_sec) + 
				((end.tv_usec - start.tv_usec) / 1000000.0);

	if (elapsed >= 1.0)
	{
		char buf[32];
		char spinner[] = "\\|/-\\|/-";
		static int spinidx = 0;

		if (lockout)
		{
			CATLOG("Lockout for %d more seconds.\n", (int)(lockout_time - lockout_elapsed));
		}
		else if (match_start.tv_sec)
		{
			CATLOG("Waiting to match again for %d more seconds.\n", (int)(match_time - last_match_time));
		}
		else
		{
			CATLOG("%c\n", spinner[spinidx++ % (sizeof(spinner) - 1)]);
		}

		printf("\033[999D");
		printf("\033[1A");
		printf("\033[0K");

		fps = frames;
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

		if (!strcmp(argv[i], "--save"))
		{
			saveimg = 1;
		}

		if (!strcmp(argv[i], "--highlight"))
		{
			highlight_match = 1;
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

		if (!strcmp(argv[i], "--rfid_in"))
		{
			if (argc >= (i + 1))
			{
				i++;
				rfid_inner_path = argv[i];
				continue;
			}
		}

		if (!strcmp(argv[i], "--rfid_out"))
		{
			if (argc >= (i + 1))
			{
				i++;
				rfid_outer_path = argv[i];
				continue;
			}
		}

		snout_path = argv[i];
	}
}

#ifdef WITH_RFID

void rfid_set_direction(rfid_match_t *current, rfid_match_t *other, 
						rfid_direction_t dir, const char *dir_str, catsnatch_rfid_t *rfid, 
						int incomplete, const char *data)
{
	CATLOG("%s RFID: %s%s\n", rfid->name, data, incomplete ? " (incomplete)": "");

	// Update the match if we get a complete tag.
	if (current->incomplete &&  (strlen(data) > strlen(current->data)))
	{
		strcpy(current->data, data);
		current->incomplete = incomplete;
	}

	// If we have already triggered this reader
	// then don't set the direction again.
	// (Since this could screw things up).
	if (current->triggered)
		return;

	// The other reader triggered first so we know the direction.
	if (other->triggered)
	{
		rfid_direction = dir;
		CATLOG("%s RFID: Direction %s\n", rfid->name, dir_str);
	}

	current->incomplete = incomplete;
	current->triggered = 1;
	current->incomplete = incomplete;
	strcpy(current->data, data);
}

void rfid_inner_read_cb(catsnatch_rfid_t *rfid, int incomplete, const char *data)
{
	rfid_set_direction(&rfid_in_match, &rfid_out_match, RFID_DIR_IN, "IN", rfid, incomplete, data);
}

void rfid_outer_read_cb(catsnatch_rfid_t *rfid, int incomplete, const char *data)
{
	rfid_set_direction(&rfid_out_match, &rfid_in_match, RFID_DIR_OUT, "OUT", rfid, incomplete, data);

}
#endif // WITH_RFID

int main(int argc, char **argv)
{
	char time_str[256];
	catsnatch_t ctx;
	IplImage* img = NULL;
	CvRect match_rect;
	CvScalar match_color;
	double match_res = 0;
	int do_match = 0;
	int i;
	int enough_time = 0;
	RaspiCamCvCapture *capture = NULL;

	if (argc < 2)
	{
		fprintf(stderr, "Catsnatch Grabber (C) Joakim Soderberg 2013\n");
		fprintf(stderr, "Usage: %s [options] [snout image]\n\n", argv[0]);
		fprintf(stderr, " --lockout <seconds>    The time in seconds a lockout takes. Default %ds\n", DEFAULT_LOCKOUT_TIME);
		fprintf(stderr, " --matchtime <seconds>  The time to wait after a match. Default %ds\n", DEFAULT_MATCH_WAIT);
		fprintf(stderr, " --show                 Show GUI of the camera feed (X11 only).\n");
		fprintf(stderr, " --show_fps             Show FPS.\n");
		fprintf(stderr, " --save                 Save match images (both ok and failed).\n");
		fprintf(stderr, " --highlight            Highlight the best match on saved images.\n");
		#ifdef WITH_RFID
		fprintf(stderr, " --rfid_in <path>       Path to inner RFID reader. Example: /dev/ttyUSB0\n");
		fprintf(stderr, " --rfid_out <path>      Path to the outter RFID reader.\n");
		#endif // WITH_RFID
		fprintf(stderr, "\nThe snout image refers to the image of the cat snout that is matched against.\n");
		fprintf(stderr, "This image should be based on a 320x240 resolution image taken by the rpi camera.\n");
		fprintf(stderr, "If no path is specified \"snout.png\" in the current directory is used.\n\n");
		return -1;
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
		CATERR("Failed to set SIGINT handler\n");
	}

	parse_cmdargs(argc, argv);

	if (!snout_path)
	{
		snout_path = "snout.png";
	}

	printf("--------------------------------------------------------------------------------\n");
	printf("Settings:\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("    Snout image: %s\n", snout_path);
	printf("     Show video: %d\n", show);
	printf("   Save matches: %d\n", saveimg);
	printf("Highlight match: %d\n", highlight_match);
	printf("      Lock time: %d seconds\n", lockout_time);
	printf("  Match timeout: %d seconds\n", match_time);
	#ifdef WITH_RFID
	printf("     Inner RFID: %s\n", rfid_inner_path ? rfid_inner_path : "-");
	printf("     Outer RFID: %s\n", rfid_outer_path ? rfid_outer_path : "-");
	#endif // WITH_RFID
	printf("--------------------------------------------------------------------------------\n");

	#ifdef WITH_RFID
	catsnatch_rfid_ctx_init(&ctx);

	if (rfid_inner_path)
	{
		catsnatch_rfid_init("Inner", &rfid_in, rfid_inner_path, rfid_inner_read_cb);
		catsnatch_rfid_ctx_set_inner(&rfid_ctx, &rfid_in);
		catsnatch_rfid_open(&rfid_in);
	}

	if (rfid_outer_path)
	{
		catsnatch_rfid_init("Outer", &rfid_out, rfid_outer_path, rfid_outer_read_cb);
		catsnatch_rfid_ctx_set_outer(&rfid_ctx, &rfid_out);
		catsnatch_rfid_open(&rfid_out);
	}
	#endif // WITH_RFID

	if (setup_gpio())
	{
		CATERR("Failed to setup GPIO pins\n");
		return -1;
	}

	if (catsnatch_init(&ctx, snout_path))
	{
		CATERR("Failed to init catsnatch lib!\n");
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
		
		catsnatch_rfid_ctx_service(&rfid_ctx);
		img = raspiCamCvQueryFrame(capture);

		// Skip all matching in lockout.
		if (lockout)
		{
			gettimeofday(&now, NULL);

			lockout_elapsed = (now.tv_sec - lockout_start.tv_sec) + 
							 ((now.tv_usec - lockout_start.tv_usec) / 1000000.0);

			if (lockout_elapsed >= lockout_time)
			{
				CATLOG("End of lockout!\n");
				lockout = 0;
				do_unlock();
			}

			goto skiploop;
		}

		// Wait until the middle of the frame is black.
		if ((do_match = catsnatch_is_matchable(&ctx, img)) < 0)
		{
			CATERR("Failed to detect matchableness\n");
			goto skiploop;
		}

		// Wait for a timeout in that case until we try to match again.
		enough_time = enough_time_since_last_match();

		if (do_match && enough_time)
		{
			// We have something to match against. 
			if ((match_res = catsnatch_match(&ctx, img, &match_rect)) < 0)
			{
				CATERR("Error when matching frame!\n");
				goto skiploop;
			}

			CATLOG("%f %sMatch\n", match_res, (match_res >= MATCH_THRESH) ? "" : "No ");
			should_we_lockout(match_res);

			if (saveimg)
			{
				// Draw a white rectangle over the best match.
				if (highlight_match)
				{
					cvRectangleR(img, match_rect, CV_RGB(255, 255, 255), 1, 8, 0);
				}

				// Save match image.
				// (We don't write to disk yet, that will slow down the matcing).
				if (saveimg)
				{
					get_time_str_fmt(time_str, sizeof(time_str), "%Y-%m-%d_%H_%M_%S");
					snprintf(match_images[match_count].path, 
						sizeof(match_images[match_count].path), 
						"%smatch_%s__%d.png", (match_res >= MATCH_THRESH) ? "" : "no", time_str, match_count);

					match_images[match_count].img = cvCloneImage(img);
				}
			}
		}

		if (show)
		{
			// Always highlight when showing in GUI.
			if (do_match)
			{
				cvRectangleR(img, match_rect, CV_RGB(255, 255, 255), 1, 8, 0);
			}

			cvShowImage("catsnatch", img);
		}
		
skiploop:
		calculate_fps();
	} while (running);

	raspiCamCvReleaseCapture(&capture);
	
	if (show)
	{
		cvDestroyWindow("catsnatch");
	}

	if (saveimg)
	{
		for (i = 0; i < MATCH_MAX_COUNT; i++)
		{
			if (match_images[i].img)
			{
				cvReleaseImage(&match_images[i].img);
				match_images[i].img = NULL;
			}
		}
	}

	catsnatch_destroy(&ctx);

	#ifdef WITH_RFID
	if (rfid_inner_path)
	{
		catsnatch_rfid_destroy(&rfid_in);
	}

	if (rfid_outer_path)
	{
		catsnatch_rfid_destroy(&rfid_out);
	}

	catsnatch_rfid_ctx_destroy(&rfid_ctx);
	#endif // WITH_RFID

	return 0;
}

