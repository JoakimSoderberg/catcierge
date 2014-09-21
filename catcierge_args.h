
#ifndef __CATCIERGE_ARGS_H__
#define __CATCIERGE_ARGS_H__

#include <stdio.h>
#include "alini/alini.h"
#ifdef RPI
#include "RaspiCamCV.h"
#endif
#include "catcierge_matcher.h"
#include "catcierge_template_matcher.h"
#include "catcierge_haar_matcher.h"
#include "catcierge_types.h"

#define DEFAULT_LOCKOUT_TIME 30		// The default lockout length after a none-match
#define DEFAULT_MATCH_WAIT 	 0		// How long to wait after a match try before we match again.
#define DEFAULT_CONSECUTIVE_LOCKOUT_DELAY 3.0 // The time in seconds between lockouts that is considered consecutive.
#define MAX_TEMP_CONFIG_VALUES 128
#define DEFAULT_OK_MATCHES_NEEDED 2
#define MAX_INPUT_TEMPLATES 32

typedef struct catcierge_args_s
{
	alini_parser_t *parser;

	int show;
	int saveimg;
	int save_obstruct_img;
	int highlight_match;
	int show_fps;
	catcierge_lockout_method_t lockout_method;
	int lockout_time;
	double consecutive_lockout_delay;
	int max_consecutive_lockout_count;
	int lockout_dummy;
	int show_cmd_help;
	int match_time;
	char *output_path;
	int ok_matches_needed;
	int save_steps;
	int no_final_decision;

	const char *matcher;
	catcierge_matcher_type_t matcher_type;
	catcierge_template_matcher_args_t templ;
	catcierge_haar_matcher_args_t haar;

	char *log_path;
	int new_execute;
	char *match_cmd;
	char *save_img_cmd;
	char *save_imgs_cmd;
	char *match_done_cmd;
	char *do_lockout_cmd;
	char *do_unlock_cmd;
	char *frame_obstructed_cmd;
	char *config_path;
	char *chuid;
	int temp_config_count;
	char *temp_config_values[MAX_TEMP_CONFIG_VALUES];
	int nocolor;
	int noanim;
	char *inputs[MAX_INPUT_TEMPLATES];
	size_t input_count;

	#ifdef WITH_RFID
	char *rfid_detect_cmd;
	char *rfid_match_cmd;
	const char *rfid_inner_path;
	const char *rfid_outer_path;
	double rfid_lock_time;
	int lock_on_invalid_rfid;
	char **rfid_allowed;
	size_t rfid_allowed_count;
	#endif // WITH_RFID

	#ifdef RPI
	RASPIVID_SETTINGS rpi_settings;
	#endif // RPI
} catcierge_args_t;

typedef int (*catcierge_parse_args_cb)(catcierge_args_t *, char *key, char **values, size_t value_count, void *user);

int catcierge_parse_config(catcierge_args_t *args, int argc, char **argv);
int catcierge_parse_cmdargs(catcierge_args_t *args, int argc, char **argv, catcierge_parse_args_cb f, void *user);
void catcierge_show_usage(catcierge_args_t *args, const char *prog);
void catcierge_print_settings(catcierge_args_t *args);
int catcierge_parse_setting(catcierge_args_t *args, const char *key, char **values, size_t value_count);
catcierge_matcher_args_t *catcierge_get_matcher_args(catcierge_args_t *args);

int catcierge_args_init(catcierge_args_t *args);
void catcierge_args_destroy(catcierge_args_t *args);

#ifdef WITH_RFID
int catcierge_create_rfid_allowed_list(catcierge_args_t *args, const char *allowed);
void catcierge_free_rfid_allowed_list(catcierge_args_t *args);
#endif

#endif // __CATCIERGE_ARGS_H__
