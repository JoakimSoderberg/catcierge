
#include "catcierge_config.h"
#include "catcierge_args.h"
#include "catcierge_log.h"
#include <string.h>
#include <assert.h>

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#include "catcierge_util.h"
#include "catcierge.h"

#ifdef RPI
#include "RaspiCamCV.h"
#include "catcierge_gpio.h"
#endif

#ifdef WITH_RFID
#include "catcierge_rfid.h"

typedef struct rfid_match_s
{
	int triggered;			// Have this rfid matcher been triggered?
	char data[128];			// The data in the match.
	int incomplete;			// Is the data incomplete?
	const char *time_str;	// Time of match.
	int is_allowed;			// Is the RFID in the allowed list?
} rfid_match_t;
#endif // WITH_RFID

// TODO: Fix printing fps.
#if 0
#define CATLOGFPS(fmt, ...) if (show_fps) { log_print(stdout, "%d fps  " fmt, 1, ##__VA_ARGS__); else CATLOG(fmt, ##__VA_ARGS__); }
#define CATERRFPS(fmt, ...) if (show_fps) { log_print(stderr, "%d fps  " fmt, 1, ##__VA_ARGS__); else CATLOG(fmt, ##__VA_ARGS__); }
#endif
#define CATLOGFPS(fmt, ...) CATLOG(fmt, ##__VA_ARGS__)
#define CATERRFPS(fmt, ...) CATLOG(fmt, ##__VA_ARGS__)

typedef enum match_direction_e
{
	MATCH_DIR_UNKNOWN = -1,
	MATCH_DIR_IN = 0,
	MATCH_DIR_OUT = 1
} match_direction_t;

#define MATCH_MAX_COUNT 4 // The number of matches to perform before deciding the lock state.

// The state of a single match.
typedef struct match_state_s
{
	char path[1024];				// Path to where the image for this match should be saved.
	IplImage *img;					// A cached image of the match frame.
	double result;					// The match result. Normalized value between 0.0 and 1.0.
	int success;					// Is the match a success (match result >= match threshold).
	match_direction_t direction;	// The direction we think the cat went in.
} match_state_t;

struct catcierge_grb_s;
typedef int (*catcierge_state_func_t)(struct catcierge_grb_s *);

typedef struct catcierge_grb_s
{
	catcierge_state_func_t state;
	catcierge_args_t args;
	FILE *log_file;

	int running;

	#ifdef RPI
	RaspiCamCvCapture *capture;
	#else
	CvCapture *capture;
	#endif

	catcierge_t matcher;
	CvRect match_rects[MAX_SNOUT_COUNT];

	// Consecutive matches decides lockout status.
	int match_count;					// The current match count, will go up to MATCH_MAX_COUNT.
	match_state_t matches[MATCH_MAX_COUNT]; // Image cache of matches.

	#ifdef WITH_RFID
	char *rfid_inner_path;
	char *rfid_outer_path;
	catcierge_rfid_t rfid_in;
	catcierge_rfid_t rfid_out;
	catcierge_rfid_context_t rfid_ctx;

	match_direction_t rfid_direction;	// Direction that is determined based on which RFID reader gets triggered first.
	rfid_match_t rfid_in_match;			// Match struct for the inner RFID reader.
	rfid_match_t rfid_out_match;		// Match struct for the outer RFID reader.
	char **rfid_allowed;				// The list of allowed RFID chips.
	size_t rfid_allowed_count;
	int lock_on_invalid_rfid;			// Should we lock when no or an invalid RFID tag is found?
	double rfid_lock_time;				// The time after a camera match has been made until we check the RFID readers. (In seconds).
	int checked_rfid_lock;				// Did we check if we should do an RFID lock during this match timeout?

	#endif // WITH_RFID
} catcierge_grb_t;

catcierge_grb_t grb;

//struct catcierge_state_s;

/*
typedef struct catcierge_state_s
{
	catcierge_state_func_t func;
	catcierge_grb_t *grb;
} catcierge_state_t;
*/

void catcierge_run_state(catcierge_grb_t *grb)
{
	assert(grb);
	grb->state(grb);
}

void catcierge_set_state(catcierge_grb_t *grb, catcierge_state_func_t new_state)
{
	assert(grb);
	grb->state = new_state;
}

static const char *match_get_direction_str(match_direction_t dir)
{
	switch (dir)
	{
		case MATCH_DIR_IN: return "in";
		case MATCH_DIR_OUT: return "out";
		case MATCH_DIR_UNKNOWN: 
		default: return "unknown";
	}
}

#ifdef WITH_RFID
static void rfid_reset(rfid_match_t *m)
{
	memset(m, 0, sizeof(rfid_match_t));
}

static int match_allowed_rfid(catcierge_grb_t *grb, const char *rfid_tag)
{
	int i;
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	for (i = 0; i < args->rfid_allowed_count; i++)
	{
		if (!strcmp(rfid_tag, args->rfid_allowed[i]))
		{
			return 1;
		}
	}

	return 0;
}

static void rfid_set_direction(catcierge_grb_t *grb, rfid_match_t *current, rfid_match_t *other, 
						match_direction_t dir, const char *dir_str, catcierge_rfid_t *rfid, 
						int incomplete, const char *data)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

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
	// TODO: It could be wise to time this out after a while...
	if (other->triggered)
	{
		grb->rfid_direction = dir;
		CATLOGFPS("%s RFID: Direction %s\n", rfid->name, dir_str);
	}

	current->triggered = 1;
	current->incomplete = incomplete;
	strcpy(current->data, data);
	current->is_allowed = match_allowed_rfid(grb, current->data);

	//log_print_csv(log_file, "rfid, %s, %s\n", 
	//		current->data, (current->is_allowed > 0)? "allowed" : "rejected");

	catcierge_execute(args->rfid_detect_cmd, 
		"%s %s %d %d %s %d %s",
		rfid->name, 				// %0 = RFID reader name.
		rfid->serial_path,			// %1 = RFID path.
		current->is_allowed, 		// %2 = Is allowed.
		current->incomplete, 		// %3 = Is data incomplete.
		current->data,				// %4 = Tag data.
		other->triggered,			// %5 = Other reader triggered.
		match_get_direction_str(grb->rfid_direction)); // %6 = Direction.
}

static void rfid_inner_read_cb(catcierge_rfid_t *rfid, int incomplete, const char *data, void *user)
{
	catcierge_grb_t *grb = user;

	// The inner RFID reader has detected a tag, we now pass that
	// match on to the code that decides which direction the cat
	// is going.
	rfid_set_direction(grb, &grb->rfid_in_match, &grb->rfid_out_match,
			MATCH_DIR_IN, "IN", rfid, incomplete, data);
}

static void rfid_outer_read_cb(catcierge_rfid_t *rfid, int incomplete, const char *data, void *user)
{
	catcierge_grb_t *grb = user;
	rfid_set_direction(grb, &grb->rfid_out_match, &grb->rfid_in_match,
			MATCH_DIR_OUT, "OUT", rfid, incomplete, data);
}

static void catcierge_init_rfid_readers(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);

	args = &grb->args;

	catcierge_rfid_ctx_init(&grb->rfid_ctx);

	if (args->rfid_inner_path)
	{
		catcierge_rfid_init("Inner", &grb->rfid_in, args->rfid_inner_path, rfid_inner_read_cb, grb);
		catcierge_rfid_ctx_set_inner(&grb->rfid_ctx, &grb->rfid_in);
		catcierge_rfid_open(&grb->rfid_in);
	}

	if (args->rfid_outer_path)
	{
		catcierge_rfid_init("Outer", &grb->rfid_out, args->rfid_outer_path, rfid_outer_read_cb, grb);
		catcierge_rfid_ctx_set_outer(&grb->rfid_ctx, &grb->rfid_out);
		catcierge_rfid_open(&grb->rfid_out);
	}
	
	CATLOG("Initialized RFID readers\n");
}
#endif // WITH_RFID

static void catcierge_do_lockout(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	if (args->lockout_dummy)
	{
		CATLOGFPS("!LOCKOUT DUMMY!\n");
		return;
	}

	if (args->do_lockout_cmd)
	{
		catcierge_execute(args->do_lockout_cmd, "");
	}
	else
	{
		#ifdef RPI
		if (args->lockout_time)
		{
			gpio_write(DOOR_PIN, 1);
		}

		gpio_write(BACKLIGHT_PIN, 1);
		#endif // RPI
	}
}

static void	catcierge_do_unlock(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	if (args->do_unlock_cmd)
	{
		catcierge_execute(args->do_unlock_cmd, "");
	}
	else
	{
		#ifdef RPI
		gpio_write(DOOR_PIN, 0);
		gpio_write(BACKLIGHT_PIN, 1);
		#endif // RPI
	}
}

static void catcierge_cleanup_imgs(catcierge_grb_t *grb)
{
	int i;
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	if (args->saveimg)
	{
		for (i = 0; i < MATCH_MAX_COUNT; i++)
		{
			if (grb->matches[i].img)
			{
				cvReleaseImage(&grb->matches[i].img);
				grb->matches[i].img = NULL;
			}
		}
	}
}

void catcierge_setup_camera(catcierge_grb_t *grb)
{
	assert(grb);

	if (grb->args.show)
	{
		cvNamedWindow("catcierge", 1);
	}

	#ifdef RPI
	grb->capture = raspiCamCvCreateCameraCapture(0);
	// TODO: Implement the cvSetCaptureProperty stuff below fo raspicamcv.
	#else
	grb->capture = cvCreateCameraCapture(0);
	cvSetCaptureProperty(grb->capture, CV_CAP_PROP_FRAME_WIDTH, 320);
	cvSetCaptureProperty(grb->capture, CV_CAP_PROP_FRAME_HEIGHT, 240);
	#endif
}

void catcierge_destroy_camera(catcierge_grb_t *grb)
{
	if (grb->args.show)
	{
		cvDestroyWindow("catcierge");
	}

	#ifdef RPI
	raspiCamCvReleaseCapture(&grb->capture);
	#else
	cvReleaseCapture(&grb->capture);
	#endif
}

static void sig_handler(int signo)
{
	switch (signo)
	{
		case SIGINT:
		{
			static int sigint_count = 0;
			CATLOG("Received SIGINT, stopping...\n");

			// Force a quit if we're not in the loop
			// or the SIGINT is a repeat.
			if (!grb.running || (sigint_count > 0))
			{
				catcierge_do_unlock(&grb);
				exit(0);
			}

			// Simply kill the loop and let the program do normal cleanup.
			grb.running = 0;

			sigint_count++;

			break;
		}
		#ifndef _WIN32
		case SIGUSR1:
		{
			CATLOG("Received SIGUSR1, forcing unlock...\n");
			catcierge_do_unlock(&grb);
			// TODO: Fix state change here...
			//state = STATE_WAITING;
			break;
		}
		case SIGUSR2:
		{
			CATLOG("Received SIGUSR2, forcing lockout...\n");
			//start_locked_state();
			break;
		}
		#endif // _WIN32
	}
}

static int catcierge_setup_gpio(catcierge_grb_t *grb)
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

static IplImage *catcierge_get_frame(catcierge_grb_t *grb)
{
	assert(grb);

	#ifdef RPI
	return raspiCamCvQueryFrame(grb->capture);
	#else
	return cvQueryFrame(grb->capture);
	#endif	
}

match_direction_t get_match_direction(int match_success, int going_out)
{
	if (match_success)
		return (going_out ? MATCH_DIR_OUT : MATCH_DIR_IN);

	return  MATCH_DIR_UNKNOWN;
}

static void catcierge_process_match_result(catcierge_grb_t *grb,
				IplImage *img, int match_success,
				double match_res, int going_out)
{
	int i;
	char time_str[256];
	catcierge_args_t *args;
	assert(grb);
	assert(img);
	args = &grb->args;

	CATLOGFPS("%f %sMatch%s\n", match_res,
			match_success ? "" : "No ",
			going_out ? " OUT" : " IN");

	// Save the current image match status.
	grb->matches[grb->match_count].result = match_res;
	grb->matches[grb->match_count].success = match_success;
	grb->matches[grb->match_count].direction
								= get_match_direction(match_success, going_out);
	grb->matches[grb->match_count].img = NULL;

	// Save match image.
	// (We don't write to disk yet, that will slow down the matcing).
	if (args->saveimg)
	{
		// Draw a white rectangle over the best match.
		if (args->highlight_match)
		{
			for (i = 0; i < args->snout_count; i++)
			{
				cvRectangleR(img, grb->match_rects[i],
						CV_RGB(255, 255, 255), 2, 8, 0);
			}
		}

		get_time_str_fmt(time_str, sizeof(time_str), "%Y-%m-%d_%H_%M_%S");
		snprintf(grb->matches[grb->match_count].path,
			sizeof(grb->matches[grb->match_count].path),
			"%s/match_%s_%s__%d.png",
			args->output_path,
			match_success ? "" : "fail",
			time_str,
			grb->match_count);

		grb->matches[grb->match_count].img = cvCloneImage(img);
	}

	// Log match to file.
	log_print_csv(grb->log_file, "match, %s, %f, %f, %s, %s\n",
		 match_success ? "success" : "failure",
		 match_res, args->match_threshold,
		 args->saveimg ? grb->matches[grb->match_count].path : "-",
		 (grb->matches[grb->match_count].direction == MATCH_DIR_IN) ? "in" : "out");

	// Runs the --match_cmd progam specified.
	catcierge_execute(args->match_cmd, "%f %d %s %d",
			match_res, 											// %0 = Match result.
			match_success,										// %1 = 0/1 succes or failure.
			args->saveimg ? grb->matches[grb->match_count].path : "",	// %2 = Image path if saveimg is turned on.
			grb->matches[grb->match_count].direction);			// %3 = Direction, 0 = in, 1 = out.
}

// =============================================================================
// States
// =============================================================================

int catcierge_state_matching(catcierge_grb_t *grb)
{
	int match_success;
	IplImage* img = NULL;
	int frame_obstructed;
	int going_out;
	double match_res;
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	img = catcierge_get_frame(grb);

	// We have something to match against.
	if ((match_res = catcierge_match(&grb->matcher, img,
		grb->match_rects, args->snout_count, &going_out)) < 0)
	{
		CATERRFPS("Error when matching frame!\n");
		return -1;
	}

	match_success = (match_res >= args->match_threshold);

	// Save the match state, execute external processes and so on...
	catcierge_process_match_result(grb, img, match_success,
		match_res, going_out);
	grb->match_count++;

	// This will set STATE_SUCCESS or STATE_LOCKOUT
	// when enough images has been captured.
	//should_we_lockout(match_res);

	if (grb->match_count < MATCH_MAX_COUNT)
	{
		return 0;
	}

	return 0;
}

int catcierge_state_waiting(catcierge_grb_t *grb)
{
	IplImage* img = NULL;
	int frame_obstructed;
	assert(grb);

	img = catcierge_get_frame(grb);

	// Wait until the middle of the frame is black
	// before we try to match anything.
	if ((frame_obstructed = catcierge_is_matchable(&grb->matcher, img)) < 0)
	{
		CATERRFPS("Failed to detect check for obstructed frame\n");
		return -1;
	}

	if (frame_obstructed)
	{
		CATLOG("Something in frame! Start matching...\n");
		grb->match_count = 0;
		catcierge_set_state(grb, catcierge_state_matching);
	}

	return 0;
}

void catcierge_process_frame(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	// Always feed the RFID readers and read a frame.
	#ifdef WITH_RFID
	if ((args->rfid_inner_path || args->rfid_outer_path) 
		&& catcierge_rfid_ctx_service(&grb->rfid_ctx))
	{
		CATERRFPS("Failed to service RFID readers\n");
	}
	#endif // WITH_RFID

	// TODO: State machine.
}

int catcierge_grabber_init(catcierge_grb_t *grb)
{
	assert(grb);

	memset(grb, 0, sizeof(catcierge_grb_t));
	
	if (catcierge_args_init(&grb->args))
	{
		return -1;
	}

	return 0;
}

void catcierge_grabber_destroy(catcierge_grb_t *grb)
{
	catcierge_args_destroy(&grb->args);
	catcierge_cleanup_imgs(grb);
}

int main(int argc, char **argv)
{
	catcierge_args_t *args;
	args = &grb.args;

	fprintf(stderr, "\nCatcierge Grabber2 v" CATCIERGE_VERSION_STR
					" (C) Joakim Soderberg 2013-2014\n\n");

	if (catcierge_grabber_init(&grb))
	{
		fprintf(stderr, "Failed to init\n");
		return -1;
	}

	// Get program settings.
	{
		if (catcierge_parse_config(args, argc, argv))
		{
			catcierge_show_usage(args, argv[0]);
			return -1;
		}

		if (catcierge_parse_cmdargs(args, argc, argv))
		{
			catcierge_show_usage(args, argv[0]);
			return -1;
		}

		catcierge_print_settings(args);
	}

	if (args->log_path)
	{
		if (!(grb.log_file = fopen(args->log_path, "a+")))
		{
			CATERR("Failed to open log file \"%s\"\n", args->log_path);
		}
	}

	if (catcierge_setup_gpio(&grb))
	{
		CATERR("Failed to setup GPIO pins\n");
		return -1;
	}

	CATLOG("Initialized GPIO pins\n");

	if (catcierge_init(&grb.matcher, args->snout_paths, args->snout_count))
	{
		CATERR("Failed to init catcierge lib!\n");
		return -1;
	}

	catcierge_set_match_flipped(&grb.matcher, args->match_flipped);
	catcierge_set_match_threshold(&grb.matcher, args->match_threshold);

	CATLOG("Initialized catcierge image recognition\n");

	catcierge_setup_camera(&grb);
	CATLOG("Starting detection!\n");
	grb.running = 1;

	// Run the program state machine.
	do
	{
		catcierge_process_frame(&grb);
	} while (grb.running);

	catcierge_destroy_camera(&grb);
	catcierge_grabber_destroy(&grb);

	if (grb.log_file)
	{
		fclose(grb.log_file);
	}

	return 0;
}
