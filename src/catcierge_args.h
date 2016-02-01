//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2015
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
#ifndef __CATCIERGE_ARGS_H__
#define __CATCIERGE_ARGS_H__

#include <stdio.h>
#include <time.h>
#include "alini/alini.h"
#ifdef RPI
#include "RaspiCamCV.h"
#endif
#include "catcierge_matcher.h"
#include "catcierge_template_matcher.h"
#include "catcierge_haar_matcher.h"
#include "catcierge_types.h"
#include "cargo.h"
#include "cargo_ini.h"

#define DEFAULT_LOCKOUT_TIME 30		// The default lockout length after a none-match
#define DEFAULT_MATCH_WAIT 	 0		// How long to wait after a match try before we match again.
#define DEFAULT_CONSECUTIVE_LOCKOUT_DELAY 3.0 // The time in seconds between lockouts that is considered consecutive.
#define MAX_TEMP_CONFIG_VALUES 128
#define DEFAULT_OK_MATCHES_NEEDED 2
#define MAX_INPUT_TEMPLATES 32
#ifdef WITH_ZMQ
#define DEFAULT_ZMQ_PORT 5556
#define DEFAULT_ZMQ_IFACE "*"
#define DEFAULT_ZMQ_TRANSPORT "tcp"
#endif // WITH_ZMQ

typedef struct catcierge_args_s
{
	cargo_t cargo;
	conf_ini_args_t ini_args;
	alini_parser_t *parser;
	char *program_name;
	int config_failure;

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
	char *match_output_path;
	char *steps_output_path;
	char *obstruct_output_path;
	char *template_output_path;
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
	char *match_group_done_cmd;
	char *match_done_cmd;
	char *do_lockout_cmd;
	char *do_unlock_cmd;
	char *frame_obstructed_cmd;
	char *state_change_cmd;
	char *config_path;
	char *chuid;
	int temp_config_count;
	char *temp_config_values[MAX_TEMP_CONFIG_VALUES];
	int nocolor;
	int noanim;
	char **inputs;
	size_t input_count;
	CvRect roi;
	int auto_roi;
	int min_backlight;
	double startup_delay;

	char *base_time;

	#ifdef WITH_RFID
	char *rfid_detect_cmd;
	char *rfid_match_cmd;
	char *rfid_inner_path;
	char *rfid_outer_path;
	double rfid_lock_time;
	int lock_on_invalid_rfid;
	char **rfid_allowed;
	size_t rfid_allowed_count;
	#endif // WITH_RFID

	#ifdef RPI
	int show_camhelp;
	RASPIVID_SETTINGS rpi_settings;
	#endif // RPI

	#ifdef WITH_ZMQ
	int zmq;
	int zmq_port;
	char *zmq_iface;
	char *zmq_transport;
	#endif // WITH_ZMQ
} catcierge_args_t;

#if 0
typedef int (*catcierge_parse_args_cb)(catcierge_args_t *, char *key, char **values, size_t value_count, void *user);

int catcierge_parse_config(catcierge_args_t *args, int argc, char **argv);
int catcierge_parse_cmdargs(catcierge_args_t *args, int argc, char **argv, catcierge_parse_args_cb f, void *user);
void catcierge_show_usage(catcierge_args_t *args, const char *prog);
void catcierge_print_settings(catcierge_args_t *args);
int catcierge_parse_setting(catcierge_args_t *args, const char *key, char **values, size_t value_count);
int catcierge_validate_settings(catcierge_args_t *args);
#endif
void catcierge_print_settings(catcierge_args_t *args);
catcierge_matcher_args_t *catcierge_get_matcher_args(catcierge_args_t *args);

#if 0
int catcierge_args_init(catcierge_args_t *args);
void catcierge_args_destroy(catcierge_args_t *args);
#endif

#ifdef WITH_RFID
int catcierge_create_rfid_allowed_list(catcierge_args_t *args, const char *allowed);
void catcierge_free_rfid_allowed_list(catcierge_args_t *args);
#endif

#endif // __CATCIERGE_ARGS_H__
