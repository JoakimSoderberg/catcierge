
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdlib.h>
#include <catsnatch_config.h>
#include "catsnatch.h"
#include "RaspiCamCV.h"
#include <signal.h>
#include <time.h>
#include "catsnatch_gpio.h"
#include <stdarg.h>
#ifdef WITH_RFID
#include "catsnatch_rfid.h"
#endif // WITH_RFID

#include "catsnatch_log.h"

#define CATLOGFPS(fmt, ...) { if (show_fps) log_print(stdout, "%d fps  " fmt, fps, ##__VA_ARGS__); else CATLOG(fmt, ##__VA_ARGS__); }
#define CATERRFPS(fmt, ...) { if (show_fps) log_print(stderr, "%d fps  " fmt, fps, ##__VA_ARGS__); else CATLOG(fmt, ##__VA_ARGS__); }

#ifdef WITH_INI
#include "alini/alini.h"
#endif

//
// Piborg Picoborg pins.
// (For turning on Solenoids and other 12V-20V appliances.)
// http://www.piborg.com/picoborg
//
#define PIBORG1	4
#define PIBORG2	18
#define PIBORG3	8
#define PIBORG4	7

#define DOOR_PIN		PIBORG1		// Pin for the solenoid that locks the door.
#define BACKLIGHT_PIN	PIBORG2		// Pin that turns on the backlight.

#define DEFAULT_MATCH_THRESH 0.8	// The threshold signifying a good match returned by catsnatch_match.
#define DEFAULT_LOCKOUT_TIME 30		// The default lockout length after a none-match
#define DEFAULT_MATCH_WAIT 	 30		// How long to wait after a match try before we match again.

int in_loop = 0;			// Only used for ctrl+c (SIGINT) handler.
char *snout_path = NULL;	// Path to the snout image we should match against.
char *output_path = NULL;	// Output directory for match images.
#ifdef WITH_INI
char *config_path = NULL;	// Configuraton path.
alini_parser_t *parser;
#endif // WITH_INI
int running = 1;	// Main loop is running (SIGINT will kill it).
int show = 0;		// Show the output video (X11 only).
int show_fps = 1;	// Show FPS in log output.
int saveimg = 1;	// Save match images to disk.
int highlight_match = 0; // Highlight the match in saved images.
struct timeval now = {0, 0};
float match_threshold = DEFAULT_MATCH_THRESH;

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

int rfid_direction = RFID_DIR_UNKNOWN;	// Direction that is determined based on which RFID reader gets triggered first.
rfid_match_t rfid_in_match;				// Match struct for the inner RFID reader.
rfid_match_t rfid_out_match;			// Match struct for the outer RFID reader.
#endif // WITH_RFID

static void sig_handler(int signo)
{
	if (signo == SIGINT)
	{
		CATLOGFPS("Received SIGINT, stopping...\n");
		running = 0;

		// Force a quit if we're not in the loop.
		if (!in_loop)
			exit(0);
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
		CATERRFPS("Failed to export and set direction for door pin\n");
		ret = -1;
		goto fail;
	}

	if (gpio_export(BACKLIGHT_PIN) || gpio_set_direction(BACKLIGHT_PIN, OUT))
	{
		CATERRFPS("Failed to export and set direction for backlight pin\n");
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
			CATERRFPS("You might have to run as root!\n");
		}
	}

	return ret;
}

static void save_images()
{
	int i;

	for (i = 0; i < MATCH_MAX_COUNT; i++)
	{
		CATLOGFPS("Saving image %s\n", match_images[i].path);
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
	matches[match_count] = (int)(match_res >= match_threshold);
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
		CATLOGFPS("End of match wait...\n");
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
			CATLOGFPS("Lockout for %d more seconds.\n", (int)(lockout_time - lockout_elapsed));
		}
		else if (match_start.tv_sec)
		{
			CATLOGFPS("Waiting to match again for %d more seconds.\n", (int)(match_time - last_match_time));
		}
		else
		{
			CATLOGFPS("%c\n", spinner[spinidx++ % (sizeof(spinner) - 1)]);
		}

		printf("\033[999D");
		printf("\033[1A");
		printf("\033[0K");

		fps = frames;
		frames = 0;
		elapsed = 0.0;
	}
}

static int parse_setting(const char *key, char *value)
{
	if (!strcmp(key, "show"))
	{
		show = 1;
		return 0;
	}

	if (!strcmp(key, "save"))
	{
		saveimg = 1;
		return 0;
	}

	if (!strcmp(key, "highlight"))
	{
		highlight_match = 1;
		return 0;
	}

	if (!strcmp(key, "show_fps"))
	{
		show_fps = 1;
		return 0;
	}

	if (!strcmp(key, "lockout"))
	{
		if (value)
		{
			lockout_time = atoi(value);
		}
		else
		{
			lockout_time = DEFAULT_LOCKOUT_TIME;
		}

		return 0;
	}

	if (!strcmp(key, "matchtime"))
	{
		if (value)
		{
			match_time = atoi(value);
		}
		else
		{
			match_time = DEFAULT_MATCH_WAIT;
		}

		return 0;
	}

	if (!strcmp(key, "output"))
	{
		if (value)
		{
			output_path = value;
			return 0;
		}

		return -1;
	}

	#ifdef WITH_RFID
	if (!strcmp(key, "rfid_in"))
	{
		if (value)
		{
			rfid_inner_path = value;
			return 0;
		}
		
		return -1;
	}

	if (!strcmp(key, "rfid_out"))
	{
		if (value)
		{
			rfid_outer_path = value;
			return 0;
		}
		
		return -1;
	}
	#endif // WITH_RFID

	if (!strcmp(key, "snout_path"))
	{
		if (value)
		{
			snout_path = value;
		}
		return 0;
	}

	if (!strcmp(key, "threshold"))
	{
		if (value)
		{
			match_threshold = atof(value);
		}
		return 0;
	}

	return -1;
}

#ifdef WITH_INI

int temp_config_count = 0;
#define MAX_TEMP_CONFIG_VALUES 128
char *temp_config_values[MAX_TEMP_CONFIG_VALUES];

static void alini_cb(alini_parser_t *parser, char *section, char *key, char *value)
{
	char *value_cpy = NULL;
	//printf("Parse config: %s = %s\n", key, value);

	// We must make a copy of the string and keep it so that
	// it won't dissapear after the parser has done its thing.
	if (temp_config_count >= MAX_TEMP_CONFIG_VALUES)
	{
		fprintf(stderr, "Max %d config file values allowed.\n", MAX_TEMP_CONFIG_VALUES);
		return;
	}

	value_cpy = strdup(value);
	temp_config_values[temp_config_count++] = value_cpy; 

	if (parse_setting(key, value_cpy) < 0)
	{
		fprintf(stderr, "Failed to parse setting in config: %s\n", key);
	}
}

static void config_free_temp_strings()
{
	int i;

	for (i = 0; i < temp_config_count; i++)
	{
		free(temp_config_values[i]);
		temp_config_values[i] = NULL;
	}
}
#endif // WITH_INI

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [options]\n\n", prog);
	fprintf(stderr, " --snout_path <path>    Path to the snout image.\n");
	fprintf(stderr, " --threshold <float>    Match threshold as a value between 0.0 and 1.0. Default %f\n", DEFAULT_MATCH_THRESH);
	fprintf(stderr, " --lockout <seconds>    The time in seconds a lockout takes. Default %ds\n", DEFAULT_LOCKOUT_TIME);
	fprintf(stderr, " --matchtime <seconds>  The time to wait after a match. Default %ds\n", DEFAULT_MATCH_WAIT);
	fprintf(stderr, " --show                 Show GUI of the camera feed (X11 only).\n");
	fprintf(stderr, " --show_fps             Show FPS.\n");
	fprintf(stderr, " --save                 Save match images (both ok and failed).\n");
	fprintf(stderr, " --highlight            Highlight the best match on saved images.\n");
	fprintf(stderr, " --output <path>        Path to where the match images should be saved.\n");
	#ifdef WITH_RFID
	fprintf(stderr, " --rfid_in <path>       Path to inner RFID reader. Example: /dev/ttyUSB0\n");
	fprintf(stderr, " --rfid_out <path>      Path to the outter RFID reader.\n");
	#endif // WITH_RFID
	#ifdef WITH_INI
	fprintf(stderr, " --config <path>        Path to config file. Default is ./catsnatch.cfg or /etc/catsnatch.cfg\n");
	#endif // WITH_INI
	fprintf(stderr, " --help                 Show this help.\n");
	fprintf(stderr, "\nThe snout image refers to the image of the cat snout that is matched against.\n");
	fprintf(stderr, "This image should be based on a 320x240 resolution image taken by the rpi camera.\n");
	fprintf(stderr, "If no path is specified \"snout.png\" in the current directory is used.\n\n");
}

static int parse_cmdargs(int argc, char **argv)
{
	int ret = 0;
	int i;
	char *val = NULL;
	char *key = NULL;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--help"))
		{
			usage(argv[0]);
			exit(1);
		}

		if (!strncmp(argv[i], "--", 2))
		{
			key = &argv[i][2];

			if (argc >= (i + 1))
			{
				i++;
				val = argv[i];
			}

			if ((ret = parse_setting(key, val)) < 0)
			{
				fprintf(stderr, "Failed to parse command line arguments\n");
				return ret;
			}
		}
	}

	return 0;
}

#ifdef WITH_RFID

void rfid_set_direction(rfid_match_t *current, rfid_match_t *other, 
						rfid_direction_t dir, const char *dir_str, catsnatch_rfid_t *rfid, 
						int incomplete, const char *data)
{
	CATLOGFPS("%s RFID: %s%s\n", rfid->name, data, incomplete ? " (incomplete)": "");

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
		CATLOGFPS("%s RFID: Direction %s\n", rfid->name, dir_str);
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
	int cfg_err = -1;
	RaspiCamCvCapture *capture = NULL;

	fprintf(stderr, "Catsnatch Grabber v" CATSNATCH_VERSION_STR " (C) Joakim Soderberg 2013-2014\n");

	if (
		#ifdef WITH_INI
			(cfg_err = alini_parser_create(&parser, "catsnatch.cfg") < 0)
		 && (cfg_err = alini_parser_create(&parser, "/etc/catsnatch.cfg") < 0)
		 &&
		#endif // WITH_INI
		(argc < 2)
		)
	{
		usage(argv[0]);
		return -1;
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
		CATERRFPS("Failed to set SIGINT handler\n");
	}

	#ifdef WITH_INI
	// Parse once first so we can load the config path if it's missing.
	// We parse again later so that command line options override the config.
	if (cfg_err < 0)
	{
		if (parse_cmdargs(argc, argv))
		{
			return -1;
		}

		if (cfg_err = alini_parser_create(&parser, config_path))
		{
			fprintf(stderr, "Failed to open config %s\n", config_path);
			return -1;
		}
	}

	if (!cfg_err)
	{
		alini_parser_setcallback_foundkvpair(parser, alini_cb);
		alini_parser_start(parser);
	}
	#endif // WITH_INI

	if (parse_cmdargs(argc, argv))
	{
		return -1;
	}

	if (!snout_path || !snout_path[0])
	{
		snout_path = "snout.png";
	}

	if (output_path)
	{
		char cmd[1024];
		CATLOGFPS("Creating output directory: \"%s\"\n", output_path);
		snprintf(cmd, sizeof(cmd), "mkdir -p %s", output_path);
		system(cmd);
	}
	else
	{
		output_path = ".";
	}

	printf("--------------------------------------------------------------------------------\n");
	printf("Settings:\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("    Snout image: %s\n", snout_path);
	printf("Match threshold: %f\n", match_threshold);
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
	catsnatch_rfid_ctx_init(&rfid_ctx);

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
		CATERRFPS("Failed to setup GPIO pins\n");
		return -1;
	}

	if (catsnatch_init(&ctx, snout_path))
	{
		CATERRFPS("Failed to init catsnatch lib!\n");
		return -1;
	}
	
	if (show)
	{
		cvNamedWindow("catsnatch", 1);
	}

	capture = raspiCamCvCreateCameraCapture(0);

	in_loop = 1;

	do
	{
		gettimeofday(&start, NULL);
		
		#ifdef WITH_RFID
		if ((rfid_inner_path || rfid_outer_path) 
			&& catsnatch_rfid_ctx_service(&rfid_ctx))
		{
			CATERRFPS("Failed to service RFID readers\n");
		}
		#endif // WITH_RFID

		img = raspiCamCvQueryFrame(capture);

		// Skip all matching in lockout.
		if (lockout)
		{
			gettimeofday(&now, NULL);

			lockout_elapsed = (now.tv_sec - lockout_start.tv_sec) + 
							 ((now.tv_usec - lockout_start.tv_usec) / 1000000.0);

			if (lockout_elapsed >= lockout_time)
			{
				CATLOGFPS("End of lockout!\n");
				lockout = 0;
				do_unlock();
			}

			goto skiploop;
		}

		// Wait until the middle of the frame is black.
		if ((do_match = catsnatch_is_matchable(&ctx, img)) < 0)
		{
			CATERRFPS("Failed to detect matchableness\n");
			goto skiploop;
		}

		// Wait for a timeout in that case until we try to match again.
		enough_time = enough_time_since_last_match();

		if (do_match && enough_time)
		{
			// We have something to match against. 
			if ((match_res = catsnatch_match(&ctx, img, &match_rect)) < 0)
			{
				CATERRFPS("Error when matching frame!\n");
				goto skiploop;
			}

			CATLOGFPS("%f %sMatch\n", match_res, (match_res >= match_threshold) ? "" : "No ");

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
						"%s/match_%s_%s__%d.png", 
						output_path, 
						(match_res >= match_threshold) ? "" : "fail", 
						time_str, 
						match_count);

					match_images[match_count].img = cvCloneImage(img);
				}
			}

			should_we_lockout(match_res);
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

	#ifdef WITH_INI
	if (!cfg_err)
	{
		alini_parser_dispose(parser);
	}

	config_free_temp_strings();
	#endif

	return 0;
}

