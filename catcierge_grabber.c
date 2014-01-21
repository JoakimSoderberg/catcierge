//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2014
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
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <stdio.h>
#include <stdlib.h>
#include <catcierge_config.h>
#include "catcierge_util.h"
#include "catcierge.h"
#include <signal.h>
#include <time.h>

#ifdef _WIN32
#include "win32/gettimeofday.h"
#else
#include <sys/time.h>
#endif

#ifdef RPI
#include "RaspiCamCV.h"
#include "catcierge_gpio.h"
#endif

#include <stdarg.h>
#include <errno.h>
#include <limits.h>

#ifdef WITH_RFID
#include "catcierge_rfid.h"
#endif // WITH_RFID

#include "catcierge_log.h"

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

#define DEFAULT_MATCH_THRESH 0.8	// The threshold signifying a good match returned by catcierge_match.
#define DEFAULT_LOCKOUT_TIME 30		// The default lockout length after a none-match
#define DEFAULT_MATCH_WAIT 	 30		// How long to wait after a match try before we match again.

typedef enum match_direction_e
{
	MATCH_DIR_IN = 0,
	MATCH_DIR_OUT = 1
} match_direction_t;

int show_cmd_help = 0;
int in_loop = 0;			// Only used for ctrl+c (SIGINT) handler.
char *snout_path = NULL;	// Path to the snout image we should match against.
char *output_path = NULL;	// Output directory for match images.
char *log_path = NULL;		// Log path.
FILE *log_file = NULL;		// Log file handle.
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
double match_threshold = DEFAULT_MATCH_THRESH;

// Lockout (when there's an invalid match).
int lockout_time = DEFAULT_LOCKOUT_TIME;
int lockout = 0;
double lockout_elapsed = 0.0;
struct timeval lockout_start = {0, 0};

// Consecutive matches decides lockout status.
int matches[4];
#define MATCH_MAX_COUNT (sizeof(matches) / sizeof(matches[0]))
int match_count = 0;
struct timeval match_start;				// The time when we started matching.
double last_match_time;					// Time since match_start in seconds.
int match_time = DEFAULT_MATCH_WAIT;	// Time to wait until we try to match again (if anything is in view).
int match_flipped = 1;					// If we fail to match, try flipping the snout and matching.

typedef struct match_image_s
{
	char path[PATH_MAX];
	IplImage *img;
	double result;
	int success;
	match_direction_t direction;
} match_image_t;

match_image_t match_images[MATCH_MAX_COUNT]; // Image cache of matches.

// FPS.
unsigned int fps = 0;
struct timeval start;
struct timeval end;
unsigned int frames = 0;
double elapsed = 0.0;

// Command execution.
char *match_cmd = NULL;
char *save_img_cmd = NULL;
char *save_imgs_cmd = NULL;
char *match_done_cmd = NULL;
char *do_lockout_cmd = NULL;
char *do_unlock_cmd = NULL;
#ifdef WITH_RFID
char *rfid_detect_cmd = NULL;
char *rfid_match_cmd = NULL;
#endif // WITH_RFID

#ifdef WITH_RFID
char *rfid_inner_path;
char *rfid_outer_path;
catcierge_rfid_t rfid_in;
catcierge_rfid_t rfid_out;
catcierge_rfid_context_t rfid_ctx;

typedef enum rfid_direction_s
{
	RFID_DIR_UNKNOWN = -1,
	RFID_DIR_IN = 0,
	RFID_DIR_OUT = 1
} rfid_direction_t;

typedef struct rfid_match_s
{
	int triggered;			// Have this rfid matcher been triggered?
	char data[128];			// The data in the match.
	int incomplete;			// Is the data incomplete?
	const char *time_str;	// Time of match.
	int is_allowed;			// Is the RFID in the allowed list?
} rfid_match_t;

int rfid_direction = RFID_DIR_UNKNOWN;	// Direction that is determined based on which RFID reader gets triggered first.
rfid_match_t rfid_in_match;				// Match struct for the inner RFID reader.
rfid_match_t rfid_out_match;			// Match struct for the outer RFID reader.
char **rfid_allowed = NULL;				// The list of allowed RFID chips.
size_t rfid_allowed_count = 0;
int lock_on_invalid_rfid = 0;			// Should we lock when no or an invalid RFID tag is found?
double rfid_lock_time = 0.0;			// The time after a camera match has been made until we check the RFID readers. (In seconds).
int checked_rfid_lock = 0;				// Did we check if we should do an RFID lock during this match timeout?

static int match_allowed_rfid(const char *rfid_tag)
{
	int i;

	for (i = 0; i < rfid_allowed_count; i++)
	{
		if (!strcmp(rfid_tag, rfid_allowed[i]))
		{
			return 1;
		}
	}

	return 0;
}

static void free_rfid_allowed_list()
{
	int i;

	for (i = 0; i < rfid_allowed_count; i++)
	{
		free(rfid_allowed[i]);
		rfid_allowed[i] = NULL;
	}

	free(rfid_allowed);
	rfid_allowed = NULL;
}

static int create_rfid_allowed_list(const char *allowed)
{
	int i;
	const char *s = allowed;
	const char *s_prev = NULL;
	char *allowed_cpy = NULL;
	rfid_allowed_count = 1;

	if (!allowed || (strlen(allowed) == 0))
	{
		return -1;
	}

	while ((s = strchr(s, ',')))
	{
		rfid_allowed_count++;
		s++;
	}

	if (!(rfid_allowed = (char **)calloc(rfid_allowed_count, sizeof(char *))))
	{
		CATERR("Out of memory\n");
		return -1;
	}

	// strtok changes its input.
	if (!(allowed_cpy = strdup(allowed)))
	{
		free(rfid_allowed);
		rfid_allowed = NULL;
		rfid_allowed_count = 0;
		return -1;
	}

	s = strtok(allowed_cpy, ",");

	for (i = 0; i < rfid_allowed_count; i++)
	{
		rfid_allowed[i] = strdup(s);
		s = strtok(NULL, ",");

		if (!s)
			break;
	}

	free(allowed_cpy);

	return 0;
}

static void rfid_reset(rfid_match_t *m)
{
	memset(m, 0, sizeof(rfid_match_t));
}

static const char *rfid_get_direction_str(rfid_direction_t dir)
{
	switch (dir)
	{
		case RFID_DIR_IN: return "in";
		case RFID_DIR_OUT: return "out";
		case RFID_DIR_UNKNOWN: 
		default: return "unknown";
	}
}

static void rfid_set_direction(rfid_match_t *current, rfid_match_t *other, 
						rfid_direction_t dir, const char *dir_str, catcierge_rfid_t *rfid, 
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
	current->is_allowed = match_allowed_rfid(current->data);

	log_print_csv(log_file, "rfid, %s, %s\n", 
			current->data, (current->is_allowed > 0)? "allowed" : "rejected");

	catcierge_execute(rfid_detect_cmd, 
		"%s %s %d %d %s %d %s",
		rfid->name, 				// %0 = RFID reader name.
		rfid->serial_path,			// %1 = RFID path.
		current->is_allowed, 		// %2 = Is allowed.
		current->incomplete, 		// %3 = Is data incomplete.
		current->data,				// %4 = Tag data.
		other->triggered,			// %5 = Other reader triggered.
		rfid_get_direction_str(rfid_direction)); // %6 = Direction.
}

static void rfid_inner_read_cb(catcierge_rfid_t *rfid, int incomplete, const char *data)
{
	rfid_set_direction(&rfid_in_match, &rfid_out_match, RFID_DIR_IN, "IN", rfid, incomplete, data);
}

static void rfid_outer_read_cb(catcierge_rfid_t *rfid, int incomplete, const char *data)
{
	rfid_set_direction(&rfid_out_match, &rfid_in_match, RFID_DIR_OUT, "OUT", rfid, incomplete, data);

}
#endif // WITH_RFID

static void do_lockout()
{
	if (do_lockout_cmd)
	{
		catcierge_execute(do_lockout_cmd, "");
	}
	else
	{
		#ifdef RPI
		if (lockout_time)
		{
			gpio_write(DOOR_PIN, 1);
		}

		gpio_write(BACKLIGHT_PIN, 1);
		#endif // RPI
	}
}

static void	 do_unlock()
{
	if (do_unlock_cmd)
	{
		catcierge_execute(do_unlock_cmd, "");
	}
	else
	{
		#ifdef RPI
		gpio_write(DOOR_PIN, 0);
		gpio_write(BACKLIGHT_PIN, 1);
		#endif // RPI
	}
}

static void start_locked_state()
{
	do_lockout();
	lockout = 1;
	gettimeofday(&lockout_start, NULL);

	match_start.tv_sec = 0;
	match_start.tv_usec = 0;
}

static void sig_handler(int signo)
{
	switch (signo)
	{
		case SIGINT:
		{
			CATLOG("Received SIGINT, stopping...\n");
			running = 0;

			// Force a quit if we're not in the loop.
			if (!in_loop)
			{
				do_unlock();
				exit(0);
			}
			break;
		}
		#ifndef _WIN32
		case SIGUSR1:
		{
			CATLOG("Received SIGUSR1, forcing unlock...\n");
			do_unlock();
			lockout = 0;
			break;
		}
		case SIGUSR2:
		{
			CATLOG("Received SIGUSR2, forcing lockout...\n");
			start_locked_state();
			break;
		}
		#endif // _WIN32
	}
}

static int setup_gpio()
{
	int ret = 0;
	#ifdef RPI

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
			CATERR("###############################################\n");
			CATERR("######## You might have to run as root! #######\n");
			CATERR("###############################################\n");
		}
	}
	#endif // RPI

	return ret;
}

static void save_images(int match_success)
{
	match_image_t *m;
	int i;

	for (i = 0; i < MATCH_MAX_COUNT; i++)
	{
		m = &match_images[i];
		CATLOGFPS("Saving image %s\n", m->path);
		cvSaveImage(m->path, m->img, 0);
		
		catcierge_execute(save_img_cmd, "%f %d %s %d", 
			m->result,		// %0 = Match result.
			m->success, 	// %1 = Match success.
			m->path,		// %2 = Image path (of now saved image).
			m->direction);	// %3 = Match direction.

		cvReleaseImage(&m->img);
		m->img = NULL;
	}

	catcierge_execute(save_imgs_cmd, "%d %s %s %s %s %f %f %f %f %d %d %d %d",  
		match_success,				// %0 = Match success.
		match_images[0].path,		// %1 = Image 1 path (of now saved image).
		match_images[1].path,		// %2 = Image 2 path (of now saved image).
		match_images[2].path,		// %3 = Image 3 path (of now saved image).
		match_images[3].path,		// %4 = Image 4 path (of now saved image).
		match_images[0].result,		// %5 = Image 1 result.
		match_images[1].result,		// %6 = Image 2 result.
		match_images[2].result,		// %7 = Image 3 result.
		match_images[3].result,		// %8 = Image 4 result.
		match_images[0].direction,	// %9 =  Image 1 direction.
		match_images[1].direction,	// %10 = Image 2 direction.
		match_images[2].direction,	// %11 = Image 3 direction.
		match_images[3].direction);	// %12 = Image 4 direction.
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
		int match_success = 0;

		for (i = 0; i < MATCH_MAX_COUNT; i++)
		{
			count += matches[i];
		}

		if (count >= (MATCH_MAX_COUNT - 2))
		{
			CATLOG("Everything OK! Door kept open...\n");

			// Make sure the door is open.
			do_unlock();

			// We have done our matches for this time, if we try to
			// match anything more we will most likely just get the
			// body of the cat which will be a no-match.
			gettimeofday(&match_start, NULL);

			#ifdef WITH_RFID
			// We only want to check for RFID lock once during each match timeout period.
			checked_rfid_lock = 0;
			#endif

			match_success = 1;
		}
		else
		{
			CATLOG("Lockout! %d out of %d matches failed.\n", count, MATCH_MAX_COUNT);
			start_locked_state();
		}

		catcierge_execute(match_done_cmd, "%d %d %d", 
			match_success, 	// %0 = Match success.
			count, 			// %1 = Successful match count.
			match_count);	// %2 = Max matches.

		match_count = 0;

		// Now we can save the images without slowing
		// down the matching FPS.
		if (saveimg)
		{
			save_images(match_success);
		}
	}
}

#ifdef WITH_RFID
static void should_we_rfid_lockout(double last_match_time)
{
	if (!lock_on_invalid_rfid)
		return;

	if (!checked_rfid_lock && (rfid_inner_path || rfid_outer_path))
	{
		if (last_match_time >= rfid_lock_time)
		{
			int do_rfid_lockout = 0;

			if (rfid_inner_path && rfid_outer_path)
			{
				// Only require one of the readers to have a correct read.
				do_rfid_lockout = !(rfid_in_match.is_allowed || rfid_out_match.is_allowed);
			}
			else if (rfid_inner_path)
			{
				do_rfid_lockout = !rfid_in_match.is_allowed;
			}
			else if (rfid_outer_path)
			{
				do_rfid_lockout = !rfid_out_match.is_allowed;
			}

			if (do_rfid_lockout)
			{
				if (rfid_direction == RFID_DIR_OUT)
				{
					CATLOG("RFID lockout: Skipping since cat is going out\n");
				}
				else
				{
					CATLOG("RFID lockout!\n");
					log_print_csv(log_file, "rfid_check, lockout\n");
					start_locked_state();
				}
			}
			else
			{
				CATLOG("RFID OK!\n");
				log_print_csv(log_file, "rfid_check, ok\n");
			}

			if (rfid_inner_path) CATLOG("  %s RFID: %s\n", rfid_in.name, rfid_in_match.triggered ? rfid_in_match.data : "No tag data");
			if (rfid_outer_path) CATLOG("  %s RFID: %s\n", rfid_out.name, rfid_out_match.triggered ? rfid_out_match.data : "No tag data");

			// %0 = Match success.
			// %1 = RFID inner in use.
			// %2 = RFID outer in use.
			// %3 = RFID inner success.
			// %4 = RFID outer success.
			// %5 = RFID inner data.
			// %6 = RFID outer data.
			catcierge_execute(rfid_match_cmd, 
				"%d %d %d %d %s %s", 
				!do_rfid_lockout,
				(rfid_inner_path != NULL),
				(rfid_outer_path != NULL),
				rfid_in_match.is_allowed,
				rfid_out_match.is_allowed,
				rfid_in_match.data,
				rfid_out_match.data);

			checked_rfid_lock = 1;
		}
	}
}
#endif // WITH_RFID

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

	#ifdef WITH_RFID
	// A short while after a valid match has been completed
	// the cat should have passed both RFID readers, so we
	// can now check if any of them found a valid RFID tag.
	// If not, we do a lockout.
	should_we_rfid_lockout(last_match_time);
	#endif // WITH_RFID	

	// Check if it's time to do a match again.
	if (last_match_time >= match_time)
	{
		CATLOGFPS("End of match wait...\n");
		match_start.tv_sec = 0;
		match_start.tv_usec = 0;

		#ifdef WITH_RFID
		rfid_reset(&rfid_in_match);
		rfid_reset(&rfid_out_match);
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
		if (value) show = atoi(value);
		return 0;
	}

	if (!strcmp(key, "save"))
	{
		saveimg = 1;
		if (value) saveimg = atoi(value);
		return 0;
	}

	if (!strcmp(key, "highlight"))
	{
		highlight_match = 1;
		if (value)highlight_match = atoi(value);
		return 0;
	}

	if (!strcmp(key, "show_fps"))
	{
		show_fps = 1;
		if (value) show_fps = atoi(value);
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

	if (!strcmp(key, "match_flipped"))
	{
		if (value)
		{
			match_flipped = atoi(value);
		}
		else
		{
			match_flipped = 1;
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

	if (!strcmp(key, "rfid_allowed"))
	{
		if (value)
		{
			if (create_rfid_allowed_list(value))
			{
				CATERR("Failed to create RFID allowed list\n");
				return -1;
			}

			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "rfid_time"))
	{
		if (value)
		{
			printf("val: %s\n", value);
			rfid_lock_time = (double)atof(value);
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "rfid_lock"))
	{
		lock_on_invalid_rfid = 1;
		if (value) lock_on_invalid_rfid = atoi(value);
		return 0;
	}
	#endif // WITH_RFID

	if (!strcmp(key, "snout_path"))
	{
		if (value)
		{
			snout_path = value;
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "threshold"))
	{
		if (value)
		{
			match_threshold = atof(value);
		}
		else
		{
			match_threshold = DEFAULT_MATCH_THRESH;
		}

		return 0;
	}

	if (!strcmp(key, "log"))
	{
		if (value)
		{
			log_path = value;
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "match_cmd"))
	{
		if (value)
		{
			match_cmd = value;
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "save_img_cmd"))
	{
		if (value)
		{
			save_img_cmd = value;
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "save_imgs_cmd"))
	{
		if (value)
		{
			save_imgs_cmd = value;
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "match_done_cmd"))
	{
		if (value)
		{
			match_done_cmd = value;
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "do_lockout_cmd"))
	{
		if (value)
		{
			do_lockout_cmd = value;
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "do_unlock_cmd"))
	{
		if (value)
		{
			do_unlock_cmd = value;
			return 0;
		}

		return -1;
	}

	#ifdef WITH_RFID
	if (!strcmp(key, "rfid_detect_cmd"))
	{
		if (value)
		{
			rfid_detect_cmd = value;
			return 0;
		}

		return -1;
	}

	if (!strcmp(key, "rfid_match_cmd"))
	{
		if (value)
		{
			rfid_match_cmd = value;
			return 0;
		}

		return -1;
	}
	#endif // WITH_RFID

	return -1;
}

#ifdef WITH_INI

int temp_config_count = 0;
#define MAX_TEMP_CONFIG_VALUES 128
char *temp_config_values[MAX_TEMP_CONFIG_VALUES];

static void alini_cb(alini_parser_t *parser, char *section, char *key, char *value)
{
	char *value_cpy = NULL;

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
	fprintf(stderr, " --threshold <float>    Match threshold as a value between 0.0 and 1.0. Default %.1f\n", DEFAULT_MATCH_THRESH);
	fprintf(stderr, " --lockout <seconds>    The time in seconds a lockout takes. Default %ds\n", DEFAULT_LOCKOUT_TIME);
	fprintf(stderr, " --matchtime <seconds>  The time to wait after a match. Default %ds\n", DEFAULT_MATCH_WAIT);
	fprintf(stderr, " --match_flipped <0|1>  Match a flipped version of the snout\n");
	fprintf(stderr, "                        (don't consider going out a failed match). Default on.\n");
	fprintf(stderr, " --show                 Show GUI of the camera feed (X11 only).\n");
	fprintf(stderr, " --show_fps             Show FPS.\n");
	fprintf(stderr, " --save                 Save match images (both ok and failed).\n");
	fprintf(stderr, " --highlight            Highlight the best match on saved images.\n");
	fprintf(stderr, " --output <path>        Path to where the match images should be saved.\n");
	fprintf(stderr, " --log <path>           Log matches and rfid readings (if enabled).\n");
	#ifdef WITH_INI
	fprintf(stderr, " --config <path>        Path to config file. Default is ./catcierge.cfg or /etc/catcierge.cfg\n");
	fprintf(stderr, "                        This is parsed as an INI file. The keys/values are the same as these options.\n");
	#endif // WITH_INI
	#ifdef WITH_RFID
	fprintf(stderr, "\n");
	fprintf(stderr, "RFID:\n");
	fprintf(stderr, " --rfid_in <path>       Path to inner RFID reader. Example: /dev/ttyUSB0\n");
	fprintf(stderr, " --rfid_out <path>      Path to the outter RFID reader.\n");
	fprintf(stderr, " --rfid_lock            Lock if no RFID tag present or invalid RFID tag. Default OFF.\n");
	fprintf(stderr, " --rfid_time <seconds>  Number of seconds to wait after a valid match before the\n");
	fprintf(stderr, "                        RFID readers are checked.\n");
	fprintf(stderr, "                        (This is so that there is enough time for the cat to be read by both readers)\n");
	fprintf(stderr, " --rfid_allowed <list>  A comma separated list of allowed RFID tags. Example: %s\n", EXAMPLE_RFID_STR);
	#endif // WITH_RFID
	fprintf(stderr, "\n");
	fprintf(stderr, "Commands:\n");
	fprintf(stderr, "   (Note that %%0, %%1, %%2... will be replaced in the input, see --cmdhelp for details)\n");
	#define EPRINT_CMD_HELP(fmt, ...) if (show_cmd_help) fprintf(stderr, fmt, ##__VA_ARGS__);
	EPRINT_CMD_HELP("\n");
	EPRINT_CMD_HELP("   General: %%cwd will output the current working directory for this program.\n");
	EPRINT_CMD_HELP("            Any paths returned are relative to this. %%%% Produces a literal %% sign.\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --match_cmd <cmd>      Command to run after a match is made.\n");
	EPRINT_CMD_HELP("                         %%0 = [float] Match result.\n");
	EPRINT_CMD_HELP("                         %%1 = [0/1]   Success or failure.\n");
	EPRINT_CMD_HELP("                         %%2 = [path]  Path to where image will be saved (Not saved yet!)\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --save_img_cmd <cmd>   Command to run at the moment a match image is saved.\n");
	EPRINT_CMD_HELP("                         %%0 = [float]  Match result, 0.0-1.0\n");
	EPRINT_CMD_HELP("                         %%1 = [0/1]    Match success.\n");
	EPRINT_CMD_HELP("                         %%2 = [string] Image path (Image is saved to disk).\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --save_imgs_cmd <cmd>  Command that runs when all match images have been saved to disk.\n");
	EPRINT_CMD_HELP("                         %%0 = [0/1]    Match success.\n");
	EPRINT_CMD_HELP("                         %%1 = [string] Image path for first match.\n");
	EPRINT_CMD_HELP("                         %%2 = [string] Image path for second match.\n");
	EPRINT_CMD_HELP("                         %%3 = [string] Image path for third match.\n");
	EPRINT_CMD_HELP("                         %%4 = [string] Image path for fourth match.\n");
	EPRINT_CMD_HELP("                         %%5 = [float]  First image result.\n");
	EPRINT_CMD_HELP("                         %%6 = [float]  Second image result.\n");
	EPRINT_CMD_HELP("                         %%7 = [float]  Third image result.\n");
	EPRINT_CMD_HELP("                         %%8 = [float]  Fourth image result.\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --match_done_cmd <cmd> Command to run when a match is done.\n");
	EPRINT_CMD_HELP("                         %%0 = [0/1]    Match success.\n");
	EPRINT_CMD_HELP("                         %%1 = [int]    Successful match count.\n");
	EPRINT_CMD_HELP("                         %%2 = [int]    Max matches.\n");
	EPRINT_CMD_HELP("\n");
	#ifdef WITH_RFID
	fprintf(stderr, " --rfid_detect_cmd <cmd> Command to run when one of the RFID readers detects a tag.\n");
	EPRINT_CMD_HELP("                         %%0 = [string] RFID reader name.\n");
	EPRINT_CMD_HELP("                         %%1 = [string] RFID path.\n");
	EPRINT_CMD_HELP("                         %%2 = [0/1]    Is allowed.\n");
	EPRINT_CMD_HELP("                         %%3 = [0/1]    Is data incomplete.\n");
	EPRINT_CMD_HELP("                         %%4 = [string] Tag data.\n");
	EPRINT_CMD_HELP("                         %%5 = [0/1]    Other reader triggered.\n");
	EPRINT_CMD_HELP("                         %%6 = [string] Direction (in, out or uknown).\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --rfid_match_cmd <cmd> Command that is run when a RFID match is made.\n");
	EPRINT_CMD_HELP("                         %%0 = Match success.\n");
	EPRINT_CMD_HELP("                         %%1 = RFID inner in use.\n");
	EPRINT_CMD_HELP("                         %%2 = RFID outer in use.\n");
	EPRINT_CMD_HELP("                         %%3 = RFID inner success.\n");
	EPRINT_CMD_HELP("                         %%4 = RFID outer success.\n");
	EPRINT_CMD_HELP("                         %%5 = RFID inner data.\n");
	EPRINT_CMD_HELP("                         %%6 = RFID outer data.\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --do_lockout_cmd <cmd> Command to run when the lockout should be performed.\n");
	fprintf(stderr, "                        This will override the normal lockout method.\n");
	fprintf(stderr, " --do_unlock_cmd <cmd>  Command that is run when we should unlock.\n");
	fprintf(stderr, "                        This will override the normal unlock method.\n");
	fprintf(stderr, "\n");
	#endif // WITH_RFID
	fprintf(stderr, " --help                 Show this help.\n");
	fprintf(stderr, " --cmdhelp              Show extra command help.\n");
	fprintf(stderr, "\nThe snout image refers to the image of the cat snout that is matched against.\n");
	fprintf(stderr, "This image should be based on a 320x240 resolution image taken by the rpi camera.\n");
	fprintf(stderr, "If no path is specified \"snout.png\" in the current directory is used.\n");
	fprintf(stderr, "\n");
	#ifndef _WIN32
	fprintf(stderr, "Signals:\n");
	fprintf(stderr, "The program can receive signals that can be sent using the kill command.\n");
	fprintf(stderr, " SIGUSR1 = Force the cat door to unlock\n");
	fprintf(stderr, " SIGUSR2 = Force the cat door to lock (for lock timeout)\n");
	fprintf(stderr, "\n");
	#endif // _WIN32
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

		if (!strcmp(argv[i], "--cmdhelp"))
		{
			show_cmd_help = 1;
			usage(argv[0]);
			exit(1);
		}

		if (!strcmp(argv[i], "--config"))
		{
			if (argc >= (i + 1))
			{
				i++;
				config_path = argv[i];
				continue;
			}
			else
			{
				fprintf(stderr, "No config path specified\n");
				return -1;
			}
		}

		if (!strncmp(argv[i], "--", 2))
		{
			key = &argv[i][2];
			val = NULL;

			if ((i + 1) < argc)
			{
				// Make sure the value is not another option.
				if (strncmp(argv[i+1], "--", 2))
				{
					i++;
					val = argv[i];
				}
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

int main(int argc, char **argv)
{
	char time_str[256];
	catcierge_t ctx;
	IplImage* img = NULL;
	CvRect match_rect;
	CvScalar match_color;
	double match_res = 0;
	int do_match = 0;
	int i;
	int enough_time = 0;
	int cfg_err = -1;
	int match_success;
	int going_out = 0;
	#ifdef RPI
	RaspiCamCvCapture *capture = NULL;
	#else
	CvCapture *capture = NULL;
	#endif

	fprintf(stderr, "Catcierge Grabber v" CATCIERGE_VERSION_STR " (C) Joakim Soderberg 2013-2014\n");

	if (
		#ifdef WITH_INI
			(cfg_err = alini_parser_create(&parser, "catcierge.cfg") < 0)
		 && (cfg_err = alini_parser_create(&parser, "/etc/catcierge.cfg") < 0)
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
		CATERR("Failed to set SIGINT handler\n");
	}

	#ifndef _WIN32
	if (signal(SIGUSR1, sig_handler) == SIG_ERR)
	{
		CATERR("Failed to set SIGUSR1 handler (used to force unlock)\n");
	}

	if (signal(SIGUSR2, sig_handler) == SIG_ERR)
	{
		CATERR("Failed to set SIGUSR2 handler (used to force unlock)\n");
	}
	#endif // _WIN32

	#ifdef WITH_INI
	// Parse once first so we can load the config path if it's missing.
	// We parse again later so that command line options override the config.
	if (cfg_err < 0)
	{
		if (parse_cmdargs(argc, argv))
		{
			return -1;
		}

		if ((cfg_err = alini_parser_create(&parser, config_path)))
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
		CATLOG("Creating output directory: \"%s\"\n", output_path);
		#ifdef WIN32
		snprintf(cmd, sizeof(cmd), "md %s", output_path);
		#else
		snprintf(cmd, sizeof(cmd), "mkdir -p %s", output_path);
		#endif
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
	printf("Match threshold: %.2f\n", match_threshold);
	printf("  Match flipped: %d\n", match_flipped);
	printf("     Show video: %d\n", show);
	printf("   Save matches: %d\n", saveimg);
	printf("Highlight match: %d\n", highlight_match);
	printf("      Lock time: %d seconds\n", lockout_time);
	printf("  Match timeout: %d seconds\n", match_time);
	printf("       Log file: %s\n", log_path ? log_path : "-");
	#ifdef WITH_RFID
	printf("     Inner RFID: %s\n", rfid_inner_path ? rfid_inner_path : "-");
	printf("     Outer RFID: %s\n", rfid_outer_path ? rfid_outer_path : "-");
	printf("Lock on no RFID: %d\n", lock_on_invalid_rfid);
	printf(" RFID lock time: %.2f seconds\n", rfid_lock_time);
	printf("   Allowed RFID: %s\n", (rfid_allowed_count <= 0) ? "-" : rfid_allowed[0]);
	for (i = 1; i < rfid_allowed_count; i++)
	{
		printf("                 %s\n", rfid_allowed[i]);
	}
	#endif // WITH_RFID
	printf("--------------------------------------------------------------------------------\n");

	if (log_path)
	{
		if (!(log_file = fopen(log_path, "a+")))
		{
			CATERR("Failed to open log file \"%s\"\n", log_path);
		}
	}

	#ifdef WITH_RFID
	catcierge_rfid_ctx_init(&rfid_ctx);

	if (rfid_inner_path)
	{
		catcierge_rfid_init("Inner", &rfid_in, rfid_inner_path, rfid_inner_read_cb);
		catcierge_rfid_ctx_set_inner(&rfid_ctx, &rfid_in);
		catcierge_rfid_open(&rfid_in);
	}

	if (rfid_outer_path)
	{
		catcierge_rfid_init("Outer", &rfid_out, rfid_outer_path, rfid_outer_read_cb);
		catcierge_rfid_ctx_set_outer(&rfid_ctx, &rfid_out);
		catcierge_rfid_open(&rfid_out);
	}
	#endif // WITH_RFID

	if (setup_gpio())
	{
		CATERR("Failed to setup GPIO pins\n");
		return -1;
	}

	if (catcierge_init(&ctx, snout_path, match_flipped, match_threshold))
	{
		CATERR("Failed to init catcierge lib!\n");
		return -1;
	}
	
	if (show)
	{
		cvNamedWindow("catcierge", 1);
	}

	#ifdef RPI
	capture = raspiCamCvCreateCameraCapture(0);
	// TODO: Implement the cvSetCaptureProperty stuff below fo raspicamcv.
	#else
	capture = cvCreateCameraCapture(0);
	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, 320);
	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, 240);
	#endif

	in_loop = 1;

	do
	{
		gettimeofday(&start, NULL);
		
		#ifdef WITH_RFID
		if ((rfid_inner_path || rfid_outer_path) 
			&& catcierge_rfid_ctx_service(&rfid_ctx))
		{
			CATERRFPS("Failed to service RFID readers\n");
		}
		#endif // WITH_RFID

		#ifdef RPI
		img = raspiCamCvQueryFrame(capture);
		#else
		img = cvQueryFrame(capture);
		#endif

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
		if ((do_match = catcierge_is_matchable(&ctx, img)) < 0)
		{
			CATERRFPS("Failed to detect matchableness\n");
			goto skiploop;
		}

		// Wait for a timeout in that case until we try to match again.
		enough_time = enough_time_since_last_match();

		if (do_match && enough_time)
		{
			// We have something to match against. 
			if ((match_res = catcierge_match(&ctx, img, &match_rect, &going_out)) < 0)
			{
				CATERRFPS("Error when matching frame!\n");
				goto skiploop;
			}

			match_success = (match_res >= match_threshold);

			CATLOGFPS("%f %sMatch%s\n", match_res, match_success ? "" : "No ", going_out ? " OUT" : " IN");

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
						match_success ? "" : "fail", 
						time_str, 
						match_count);

					match_images[match_count].img = cvCloneImage(img);
					match_images[match_count].result = match_res;
					match_images[match_count].success = match_success;
					match_images[match_count].direction = going_out ? MATCH_DIR_OUT : MATCH_DIR_IN;
				}
			}

			// Log match to file.
			log_print_csv(log_file, "match, %s, %f, %f, %s, %s\n",
				 match_success ? "success" : "failure", 
				 match_res, match_threshold,
				 saveimg ? match_images[match_count].path : "-",
				 (match_images[match_count].direction == MATCH_DIR_IN) ? "in" : "out");

			catcierge_execute(match_cmd, "%f %d %s %d",  
					match_res, 										// %0 = Match result.
					match_success,									// %1 = 0/1 succes or failure.
					saveimg ? match_images[match_count].path : "",	// %2 = Image path if saveimg is turned on.
					match_images[match_count].direction);			// %3 = Direction, 0 = in, 1 = out.

			should_we_lockout(match_res);
		}
skiploop:

		if (show)
		{
			#ifdef RPI
			match_color = CV_RGB(255, 255, 255); // Grayscale so don't bother with color.
			#else
			match_color = (match_success) ? CV_RGB(0, 255, 0) : CV_RGB(255, 0, 0);
			#endif

			// Always highlight when showing in GUI.
			if (do_match)
			{
				cvRectangleR(img, match_rect, match_color, 2, 8, 0);
			}

			cvShowImage("catcierge", img);
			cvWaitKey(10);
		}
		
		calculate_fps();
	} while (running);

	// Make sure we always open the door on exit.
	do_unlock();

	#ifdef RPI
	raspiCamCvReleaseCapture(&capture);
	#else
	cvReleaseCapture(&capture);
	#endif
	
	if (show)
	{
		cvDestroyWindow("catcierge");
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

	catcierge_destroy(&ctx);

	#ifdef WITH_RFID
	if (rfid_inner_path)
	{
		catcierge_rfid_destroy(&rfid_in);
	}

	if (rfid_outer_path)
	{
		catcierge_rfid_destroy(&rfid_out);
	}

	catcierge_rfid_ctx_destroy(&rfid_ctx);

	free_rfid_allowed_list();
	#endif // WITH_RFID

	#ifdef WITH_INI
	if (!cfg_err)
	{
		alini_parser_dispose(parser);
	}

	config_free_temp_strings();
	#endif // WITH_INI

	if (log_file)
	{
		fclose(log_file);
	}

	return 0;
}

