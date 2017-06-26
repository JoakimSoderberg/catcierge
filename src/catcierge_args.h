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
	int no_unlock_after_lockout;
	double consecutive_lockout_delay;
	int max_consecutive_lockout_count;
	int lockout_dummy;
	int show_cmd_help;
	int show_event_help;
	int match_time;
	char *output_path;
	char *match_output_path;
	char *steps_output_path;
	char *obstruct_output_path;
	char *template_output_path;
	int ok_matches_needed;
	int save_steps;
	int no_final_decision;

	catcierge_matcher_type_t matcher_type;
	catcierge_template_matcher_args_t templ;
	catcierge_haar_matcher_args_t haar;

	char *log_path; // TODO: Remove this.

	// Generate all event cmd args
	#define CATCIERGE_DEFINE_EVENT(ev_enum_name, ev_name, ev_description) \
		char **ev_name ## _cmd;											  \
		size_t ev_name ## _cmd_count;
	#include "catcierge_events.h"

	char **user_vars;
	size_t user_var_count;

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
	int auto_roi_thr;
	int save_auto_roi_img;
	char *auto_roi_output_path;
	int min_backlight;
	double startup_delay;
	int no_default_config;

	char *base_time;
	long base_time_diff;

	#ifdef WITH_RFID
	char *rfid_inner_path;
	char *rfid_outer_path;
	double rfid_lock_time;
	int lock_on_invalid_rfid;
	char **rfid_allowed;
	size_t rfid_allowed_count;
	#endif // WITH_RFID

	int lockout_gpio_pin;
	int backlight_gpio_pin;
	int backlight_enable;

	#ifndef _WIN32
	char *sigusr1_str;
	char *sigusr2_str;
	#endif // _WIN32

	#ifdef RPI
	char *rpi_config_path;
	int show_camhelp;
	RASPIVID_SETTINGS rpi_settings;
	int non_rpi_cam;
	#endif // RPI

	int camera_index;

	#ifdef WITH_ZMQ
	int zmq;
	int zmq_port;
	char *zmq_iface;
	char *zmq_transport;
	#endif // WITH_ZMQ
} catcierge_args_t;

int catcierge_args_init(catcierge_args_t *args, const char *progname);
int catcierge_args_parse(catcierge_args_t *args, int argc, char **argv);
void catcierge_args_destroy(catcierge_args_t *args);

void catcierge_args_init_vars(catcierge_args_t *args);
void catcierge_args_destroy_vars(catcierge_args_t *args);

void catcierge_print_settings(catcierge_args_t *args);
catcierge_matcher_args_t *catcierge_get_matcher_args(catcierge_args_t *args);

#endif // __CATCIERGE_ARGS_H__
