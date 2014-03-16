
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

typedef struct catcierge_grb_s
{
	catcierge_args_t args;

	int running;

	#ifdef RPI
	RaspiCamCvCapture *capture;
	#else
	CvCapture *capture;
	#endif

	catcierge_t matcher;

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

struct catcierge_state_s;
typedef void (*catcierge_state_func_t)(struct catcierge_state_s *);

typedef struct catcierge_state_s
{
	catcierge_state_func_t func;
	catcierge_grb_t *grb;
} catcierge_state_t;

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

void catcierge_run_state(catcierge_state_t *state)
{
	assert(state);
	state->func(state);
}

void catcierge_set_state(catcierge_state_t *state, catcierge_state_func_t new_state)
{
	assert(state);
	state->func = new_state;
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

void catcierge_state_waiting(catcierge_state_t *state)
{
	assert(state);
}

void catcierge_process_frame(catcierge_grb_t *grb)
{
	IplImage* img = NULL;
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

	img = catcierge_get_frame(grb);

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

	fprintf(stderr, "\nCatcierge Grabber2 v" CATCIERGE_VERSION_STR " (C) Joakim Soderberg 2013-2014\n\n");

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

	return 0;
}
