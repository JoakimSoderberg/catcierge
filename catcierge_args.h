
#ifndef __CATCIERGE_ARGS_H__
#define __CATCIERGE_ARGS_H__

#include <stdio.h>

#define MAX_SNOUT_COUNT 24
#define DEFAULT_MATCH_THRESH 0.8	// The threshold signifying a good match returned by catcierge_match.
#define DEFAULT_LOCKOUT_TIME 30		// The default lockout length after a none-match
#define DEFAULT_MATCH_WAIT 	 0		// How long to wait after a match try before we match again.

typedef struct catcierge_args_s
{
	int show;
	int saveimg;
	int highlight_match;
	int show_fps;
	int lockout_time;
	int max_consecutive_lockout_count;
	int lockout_dummy;
	int match_time;
	int match_flipped;
	char *output_path;
	char *rfid_inner_path;
	char *rfid_outer_path;
	double rfid_lock_time;
	int lock_on_invalid_rfid;
	char snout_paths[MAX_SNOUT_COUNT];
	size_t snout_count;
	double match_threshold;
	char *log_path;
	char *match_cmd;
	char *save_img_cmd;
	char *save_imgs_cmd;
	char *match_done_cmd;
	char *do_lockout_cmd;
	char *do_unlock_cmd;
	char *rfid_detect_cmd;
	char *rfid_match_cmd;
} catcierge_args_t;

#endif // __CATCIERGE_ARGS_H__
