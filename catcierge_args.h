
#ifndef __CATCIERGE_ARGS_H__
#define __CATCIERGE_ARGS_H__

#include <stdio.h>
#include "alini/alini.h"

#define MAX_SNOUT_COUNT 24
#define DEFAULT_MATCH_THRESH 0.8	// The threshold signifying a good match returned by catcierge_match.
#define DEFAULT_LOCKOUT_TIME 30		// The default lockout length after a none-match
#define DEFAULT_MATCH_WAIT 	 0		// How long to wait after a match try before we match again.
#define DEFAULT_CONSECUTIVE_LOCKOUT_DELAY 3.0
#define MAX_TEMP_CONFIG_VALUES 128

typedef struct catcierge_args_s
{
	alini_parser_t *parser;

	int show;
	int saveimg;
	int highlight_match;
	int show_fps;
	int lockout_time;
	double consecutive_lockout_delay;
	int max_consecutive_lockout_count;
	int lockout_dummy;
	int show_cmd_help;
	int match_time;
	int match_flipped;
	char *output_path;
	const char *snout_paths[MAX_SNOUT_COUNT];
	size_t snout_count;
	double match_threshold;
	char *log_path;
	char *match_cmd;
	char *save_img_cmd;
	char *save_imgs_cmd;
	char *match_done_cmd;
	char *do_lockout_cmd;
	char *do_unlock_cmd;
	char *config_path;
	int temp_config_count;
	char *temp_config_values[MAX_TEMP_CONFIG_VALUES];
	int nocolor;

	#ifdef WITH_RFID
	char *rfid_detect_cmd;
	char *rfid_match_cmd;
	char *rfid_inner_path;
	char *rfid_outer_path;
	double rfid_lock_time;
	int lock_on_invalid_rfid;
	char **rfid_allowed;
	int rfid_allowed_count;
	#endif // WITH_RFID
} catcierge_args_t;

int catcierge_parse_config(catcierge_args_t *args, int argc, char **argv);
int catcierge_parse_cmdargs(catcierge_args_t *args, int argc, char **argv);
void catcierge_show_usage(catcierge_args_t *args, const char *prog);
void catcierge_print_settings(catcierge_args_t *args);

int catcierge_args_init(catcierge_args_t *args);
void catcierge_args_destroy(catcierge_args_t *args);

#endif // __CATCIERGE_ARGS_H__
