
#ifndef __CATCIERGE_FSM_H__
#define __CATCIERGE_FSM_H__

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#include <catcierge_config.h>
#include "catcierge_util.h"
#include "catcierge.h"
#include "catcierge_timer.h"
#include "catcierge_args.h"
#include "catcierge_types.h"

#ifdef RPI
#include "RaspiCamCV.h"
#include "catcierge_gpio.h"
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

	IplImage *img;
	catcierge_t matcher;
	int match_success;
	CvRect match_rects[MAX_SNOUT_COUNT];
	int consecutive_lockout_count;

	// Consecutive matches decides lockout status.
	int match_count;						// The current match count, will go up to MATCH_MAX_COUNT.
	match_state_t matches[MATCH_MAX_COUNT]; // Image cache of matches.
	catcierge_timer_t rematch_timer;
	catcierge_timer_t lockout_timer;
	catcierge_timer_t frame_timer;

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

void catcierge_do_lockout(catcierge_grb_t *grb);
void catcierge_do_unlock(catcierge_grb_t *grb);
IplImage *catcierge_get_frame(catcierge_grb_t *grb);
void catcierge_run_state(catcierge_grb_t *grb);
void catcierge_print_spinner(catcierge_grb_t *grb);
void catcierge_destroy_camera(catcierge_grb_t *grb);
int catcierge_setup_gpio(catcierge_grb_t *grb);
int catcierge_grabber_init(catcierge_grb_t *grb);
void catcierge_grabber_destroy(catcierge_grb_t *grb);
void catcierge_setup_camera(catcierge_grb_t *grb);
void catcierge_set_state(catcierge_grb_t *grb, catcierge_state_func_t new_state);
void catcierge_run_state(catcierge_grb_t *grb);

int catcierge_state_waiting(catcierge_grb_t *grb);
int catcierge_state_keepopen(catcierge_grb_t *grb);
void catcierge_state_transition_lockout(catcierge_grb_t *grb);
int catcierge_state_lockout(catcierge_grb_t *grb);
int catcierge_state_matching(catcierge_grb_t *grb);
int catcierge_state_waiting(catcierge_grb_t *grb);

#endif // __CATCIERGE_FSM_H__
