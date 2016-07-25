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
#include "alini.h"
#include "catcierge_config.h"
#include "cargo.h"
#include "cargo_ini.h"
#include "catcierge_args.h"
#include "catcierge_output.h"
#include "catcierge_log.h"
#ifdef RPI
#include "catcierge_rpi_args.h"
#endif

#ifdef WITH_RFID
#include "catcierge_rfid.h"
#endif // WITH_RFID

static int add_lockout_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	ret |= cargo_add_group(cargo, 0, "lockout", "Lockout settings",
			"These settings control how the cat door will be locked.");

	ret |= cargo_add_option(cargo, 0,
			"<lockout> --lockout_method",
			"Defines the method used to decide when to unlock:\n"
			"[1: Only use the timer, don't care about clear frame.]\n"
			"2: Wait for clear frame or that the timer has timed out.\n"
			"3: Wait for clear frame and then start unlock timer.\n",
			"i", &args->lockout_method);
	ret |= cargo_add_validation(cargo, 0, 
			"--lockout_method",
			cargo_validate_int_range(1, 3));

	ret |= cargo_add_option(cargo, 0,
			"<lockout> --lockout",
			NULL,
			"i", &args->lockout_time);
	ret |= cargo_set_option_description(cargo, "--lockout",
			"The time in seconds a lockout takes. Default %d seconds.",
			DEFAULT_LOCKOUT_TIME);

	ret |= cargo_add_option(cargo, 0,
			"<lockout> --lockout_error",
			"Number of lockouts in a row that's allowed before we "
			"consider it an error and quit the program. "
			"Default is to never do this.",
			"i", &args->max_consecutive_lockout_count);

	ret |= cargo_add_option(cargo, 0,
			"<lockout> --lockout_error_delay",
			NULL,
			"d", &args->consecutive_lockout_delay);
	ret |= cargo_set_option_description(cargo, "--lockout_error_delay",
			"The delay in seconds between lockouts that should be "
			"counted as a consecutive lockout. Default %0.1f.",
			DEFAULT_CONSECUTIVE_LOCKOUT_DELAY);

	ret |= cargo_add_option(cargo, 0,
			"<lockout> --lockout_dummy",
			"Do everything as normal, but don't actually "
			"lock the door. This is useful for testing.",
			"b", &args->lockout_dummy);

	return ret;
}

#ifdef RPI
static int add_gpio_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	ret |= cargo_add_group(cargo, 0, "gpio", "Raspberry Pi GPIO settings",
			"Settings for changing the GPIO pins used on the "
			"Raspberry Pi.\n\n"
			"Note that if you change these, you will need to set the same pin in the init "
			"script so the pin gets the correct state at boot. "
			"For permanently changing this, it's instead recommended "
			"that you change the default pin when compiling.");

	ret |= cargo_add_option(cargo, 0,
			"<gpio> --lockout_gpio_pin",
			NULL,
			"i", &args->lockout_gpio_pin);
	ret |= cargo_set_option_description(cargo, "--lockout_gpio_pin",
			"Change the Raspberry Pi GPIO pin used for triggering "
			"the lockout of the cat door.\n"
			"The default GPIO pin used: %d", CATCIERGE_LOCKOUT_GPIO);
	ret |= cargo_set_metavar(cargo, "--lockout_gpio_pin", "GPIO");

	ret |= cargo_add_option(cargo, 0,
			"<gpio> --backlight_gpio_pin",
			NULL,
			"i", &args->backlight_gpio_pin);
	ret |= cargo_set_option_description(cargo, "--backlight_gpio_pin",
			"Change the Raspberry Pi GPIO pin used for turning on the "
			"backlight (if backlight control is enabled --backlight_enable).\n"
			"The default GPIO pin used: %d", CATCIERGE_BACKLIGHT_GPIO);
	ret |= cargo_set_metavar(cargo, "--backlight_gpio_pin", "GPIO");

	ret |= cargo_add_option(cargo, 0,
			"<gpio> --backlight_enable",
			"Control the backlight via a GPIO pin? This will then always "
			"turn it on at startup. If you instead have the backlight wired "
			"to always be on, this is not needed.",
			"b", &args->backlight_enable);

	return ret;
}
#endif // PRI

static int add_presentation_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	ret |= cargo_add_group(cargo, 0, "pres", "Presentation settings", NULL);

	ret |= cargo_add_option(cargo, 0,
			"<pres> --show",
			"Show GUI of the camera feed (X11 only).",
			"b", &args->show);

	ret |= cargo_add_option(cargo, 0,
			"<pres> --highlight",
			"Highlight the best match on saved images. "
			"(Only ever use for debugging purposes, "
			"since it writes on the original image)",
			"b", &args->highlight_match);

	ret |= cargo_add_option(cargo, 0,
			"<pres> --nocolor",
			"Turn off all color output in the console.",
			"b", &args->nocolor);

	ret |= cargo_add_option(cargo, 0,
			"<pres> --noanim",
			"Turn off animations in the console.",
			"b", &args->noanim);

	return ret;
}

static int parse_CvRect(cargo_t ctx, void *user, const char *optname,
                        int argc, char **argv)
{
	CvRect *rect = (CvRect *)user;

	if (argc != 4)
	{
		cargo_set_error(ctx, 0,
			"Not enough arguments for %s, expected (x, y, width, height) "
			"but only got %d values",
			optname, argc);
		return -1;
	}

	*rect = cvRect(atoi(argv[0]), atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));

	return argc;
}

static int add_roi_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	ret |= cargo_add_group(cargo, 0, "roi",
			"Region Of Interest (ROI) settings",
			"If the backlight does not take up the entire camera image, these settings "
			"can be used to set what part of the image that the matcher should "
			"look for the cat head in. The region of interest (ROI).");

	ret |= cargo_add_mutex_group(cargo, 0,
			"roi_type", 
			"ROI type",
			NULL);

	ret |= cargo_add_option(cargo, 0,
			"<roi> --startup_delay",
			"Number of seconds to wait after starting before starting "
			"to capture anything. This is so that if you have a back light "
			"that is turned on at startup, it has time to turn on, otherwise "
			"the program will think something is obstructing the image and "
			"start trying to match.",
			"d", &args->startup_delay);

	ret |= cargo_add_option(cargo, 0,
			"<!roi_type, roi> --roi",
			"Crop all input image to this region of interest. "
			"Cannot be used together with --auto_roi.",
			"[c]#", parse_CvRect, &args->roi, NULL, 4);
	ret |= cargo_set_metavar(cargo,
			"--roi",
			"X Y WIDTH HEIGHT");

	ret |= cargo_add_option(cargo, 0,
			"<!roi_type, roi> --auto_roi",
			"Automatically crop to the area covered by the backlight. "
			"This will be done after --startup_delay has ended. "
			"Cannot be used together with --roi.",
			"b", &args->auto_roi);

	ret |= cargo_add_option(cargo, 0,
			"<roi> --auto_roi_thr",
			NULL,
			"i", &args->auto_roi_thr);
	ret |= cargo_set_metavar(cargo,
			"--auto_roi_thr",
			"THRESHOLD");
	ret |= cargo_set_option_description(cargo,
			"--auto_roi_thr",
			"Set the threshold values used to find the backlight, using "
			"a binary threshold algorithm. Separate each pixel into either "
			"black or white. White if the greyscale value of the pixel is above "
			"the threshold, and black otherwise.\n\n"
			"Default value %d", DEFAULT_AUTOROI_THR);

	ret |= cargo_add_option(cargo, 0,
			"<roi> --min_backlight",
			NULL,
			"i", &args->min_backlight);
	ret |= cargo_set_option_description(cargo,
			"--min_backlight",
			"If --auto_roi is on, this sets the minimum allowed area the "
			"backlight is allowed to be before it is considered broken. "
			"If it is smaller than this, the program will exit. "
			"Default %d.",DEFAULT_MIN_BACKLIGHT);

	ret |= cargo_add_option(cargo, 0,
			"<roi> --save_auto_roi",
			"Save the image roi found by --auto_roi. Can be useful for debugging "
			"when tweaking the threshold. Result placed in --output_path.",
			"b", &args->save_auto_roi_img);

	// TODO: Add this.
	#if 0
	ret |= cargo_add_option(cargo, 0,
			"<roi> --auto_roi_output_path",
			"Path for the --auto_roi region found. Overrides --output_path.",
			"s", &args->auto_roi_output_path);
	ret |= cargo_set_metavar(cargo, "--auto_roi_output_path", "PATH");
	#endif

	return ret;
}

static int add_matcher_options(cargo_t cargo, catcierge_args_t *args)
{
	//
	// Adds options meant for the matcher algorithm chosen.
	//
	int ret = 0;

	ret |= cargo_add_group(cargo, 0, "matcher", "Matcher settings", NULL);

	ret |= cargo_add_mutex_group(cargo,
			CARGO_MUTEXGRP_ONE_REQUIRED,
			"matcher_type", 
			"Matcher type",
			"The algorithm to use when matching for the cat profile image.");

	ret |= cargo_add_option(cargo, 0,
			"<!matcher_type, matcher> --template_matcher --template --templ",
			"Template based matching algorithm.",
			"b=", &args->matcher_type, MATCHER_TEMPLATE);

	ret |= cargo_add_option(cargo, 0,
			"<!matcher_type, matcher> --haar_matcher --haar",
			"Haar feature based matching algorithm (recommended).",
			"b=", &args->matcher_type, MATCHER_HAAR);

	ret |= cargo_add_option(cargo, 0,
			"<matcher> --ok_matches_needed", NULL,
			"i", &args->ok_matches_needed);
	ret |= cargo_set_option_description(cargo,
			"--ok_matches_needed",
			"The number of matches out of %d matches "
			"that need to be OK for the match to be considered "
			"an over all OK match.", MATCH_MAX_COUNT);
	ret |= cargo_add_validation(cargo, 0,
			"--ok_matches_needed",
			cargo_validate_int_range(0, MATCH_MAX_COUNT));

	ret |= cargo_add_option(cargo, 0,
			"<matcher> --no_final_decision",
			"Normally after all matches in a match group has been made "
			"the matcher algorithm gets to do a final decision based on "
			"the entire group of matches which overrides the \"--ok_matches_needed\""
			"setting. This flag turns this behavior off.",
			"b", &args->no_final_decision);

	ret |= cargo_add_option(cargo, 0,
			"<matcher> --matchtime", NULL,
			"i", &args->match_time);
	ret |= cargo_set_option_description(cargo,
			"--matchtime",
			"The time to wait after a match before attemping again. "
			"Default %d seconds.", DEFAULT_MATCH_WAIT);

	ret |= catcierge_haar_matcher_add_options(cargo, &args->haar);
	ret |= catcierge_template_matcher_add_options(cargo, &args->templ);
	return ret;
}

static int add_output_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	ret |= cargo_add_group(cargo, 0, "output", "Output settings", 
			"Note that all the *_path variables below can contain "
			"variables of the format %%var%%.\n"
			"See --cmdhelp for available variables, and --eventhelp "
			"for a list of events.");

	ret |= cargo_add_option(cargo,
			CARGO_OPT_STOP | CARGO_OPT_STOP_HARD,
			"<output> --eventhelp",
			"Show a list of the events that are triggered by catcierge. "
			"Templates specified using --input can filter based on these "
			"so that they generate output only for a specific event. "
			"This also shows general help regarding input template settings.",
			"b", &args->show_event_help);

	ret |= cargo_add_option(cargo, 0,
			"<output> --save",
			"Save match images (both ok and failed).",
			"b", &args->saveimg);

	ret |= cargo_add_option(cargo, 0,
			"<output> --save_obstruct",
			"Save the image that triggered the \"frame obstructed\" event.",
			"b", &args->save_obstruct_img);

	ret |= cargo_add_option(cargo, 0,
			"<output> --save_steps",
			"Save each step of the matching algorithm. "
			"(--save must also be turned on)",
			"b", &args->save_steps);

	ret |= cargo_add_option(cargo, 0,
			"<output> --input",
			"Path to one or more template files generated on specified events. "
			"(Not to be confused with the template matcher). "
			"See --eventhelp for details on generating these on specific "
			"events as well as input template settings.",
			"[s]+", &args->inputs, &args->input_count);

	ret |= cargo_add_option(cargo, 0,
			"<output> --output_path --output",
			"Path to where the match images and generated templates "
			"should be saved.",
			"s", &args->output_path);
	ret |= cargo_set_metavar(cargo, "--output_path", "PATH");

	ret |= cargo_add_option(cargo, 0,
			"<output> --match_output_path",
			"Override --output_path for match images and save them here "
			"instead. This can be relative to --output_path by using "
			"%%output_path%% in the path. Note that this applies to all paths, "
			"other path variables can be used to create a nested structure. "
			"Just make sure you do not add a recursive dependence. "
			"Also any other %%vars%% can of course be used to build the path. "
			"See --cmdhelp for details.",
			"s", &args->match_output_path);
	ret |= cargo_set_metavar(cargo, "--match_output_path", "PATH");

	ret |= cargo_add_option(cargo, 0,
			"<output> --steps_output_path",
			"If --save_steps is enabled, save step images to this path. "
			"Same as for --match_output_path, overrides --output_path.\n"
			"Example: --steps_output_path %%match_output_path%%/steps",
			"s", &args->steps_output_path);
	ret |= cargo_set_metavar(cargo, "--steps_output_path", "PATH");

	ret |= cargo_add_option(cargo, 0,
			"<output> --obstruct_output_path",
			"Path for the obstruct images. Overrides --output_path.",
			"s", &args->obstruct_output_path);
	ret |= cargo_set_metavar(cargo, "--obstruct_output_path", "PATH");

	ret |= cargo_add_option(cargo, 0,
			"<output> --template_output_path",
			"Output path for templates (given by --template). "
			"Overrides --output_path.",
			"s", &args->template_output_path);
	ret |= cargo_set_metavar(cargo, "--template_output_path", "PATH");

	#ifdef WITH_ZMQ
	ret |= cargo_add_option(cargo, 0,
			"<output> --zmq",
			"Publish generated output templates to a ZMQ socket. "
			"If a template contains the setting 'nozmq' it will not be published.",
			"b", &args->zmq);

	ret |= cargo_add_option(cargo, 0,
			"<output> --zmq_port",
			NULL,
			"i", &args->zmq_port);
	ret |= cargo_set_option_description(cargo,
			"--zmq_port",
			"The TCP port that the ZMQ publisher listens on. Default %d",
			DEFAULT_ZMQ_PORT);

	ret |= cargo_add_option(cargo, 0,
			"<output> --zmq_iface",
			NULL,
			"s", &args->zmq_iface);
	ret |= cargo_set_option_description(cargo,
			"--zmq_iface",
			"The interface the ZMQ publisher listens on. Default %s",
			DEFAULT_ZMQ_IFACE);

	ret |= cargo_add_option(cargo, 0,
			"<output> --zmq_transport",
			NULL,
			"s", &args->zmq_transport);
	ret |= cargo_set_option_description(cargo,
			"--zmq_transport",
			"The ZMQ transport to use. Default %s",
			DEFAULT_ZMQ_TRANSPORT);

	#endif // WITH_ZMQ

	return ret;
}

#ifdef WITH_RFID
int add_rfid_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	ret |= cargo_add_group(cargo, 0, 
			"rfid", "RFID settings", 
			"Settings for using RFID tag readers from "
			"http://www.priority1design.com.au/");

	ret |= cargo_add_option(cargo, 0,
			"<rfid> --rfid_in",
			"Path to inner RFID reader. Example: /dev/ttyUSB0",
			"s", &args->rfid_inner_path);

	ret |= cargo_add_option(cargo, 0,
			"<rfid> --rfid_out",
			"Path to the outter RFID reader.",
			"s", &args->rfid_outer_path);

	ret |= cargo_add_option(cargo, 0,
			"<rfid> --rfid_lock",
			"Lock if no RFID tag present or invalid RFID tag. Default OFF.",
			"b", &args->lock_on_invalid_rfid);

	ret |= cargo_add_option(cargo, 0,
			"<rfid> --rfid_time",
			"Number of seconds to wait after a valid match before the "
			"RFID readers are checked.\n"
			"(This is so that there is enough time for the cat to be read "
			"by both readers)",
			"d", &args->rfid_lock_time);

	ret |= cargo_add_option(cargo, 0,
			"<rfid> --rfid_allowed",
			NULL,
			"[s]+", &args->rfid_allowed, &args->rfid_allowed_count);
	ret |= cargo_set_option_description(cargo,
			"--rfid_allowed",
			"A comma separated list of allowed RFID tags. Example: %s",
			EXAMPLE_RFID_STR);

	return ret;
}
#endif // WITH_RFID

static int add_command_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	ret |= cargo_add_group(cargo, 0, 
			"cmd", "Command settings", 
			"These are commands that will be executed when certain events "
			"occur.\n"
			"Variables can be passed to these commands such as:\n"
			"  %%state%%, %%match_success%% and so on.\n"
			"To see a list of variables use --cmdhelp");

	// We stop parsing when this option is hit,
	// and we disable any errors on required variables.
	ret |= cargo_add_option(cargo,
			CARGO_OPT_STOP | CARGO_OPT_STOP_HARD,
			"<cmd> --cmdhelp",
			"Shows command output variable help.",
			"b", &args->show_cmd_help);

	ret |= cargo_add_group(cargo, CARGO_GROUP_HIDE, 
			"cmd_help", "Advanced command settings", 
			"These are commands that will be executed when certain events "
			"occur. You can specify multiple commands, they will be started "
			"in the order they are added. Each process is forked and run in "
			"parallel with the others. If you want them to run sequentially "
			"you need to create a script and run that.\n"
			"Variables can be passed to these commands such as:\n"
			"  %%state%%, %%match_success%% and so on.\n"
			"To see a list of variables use --cmdhelp");

	//
	// Automatically add options for all events specified in catcierge_events.h
	//
	#define CATCIERGE_DEFINE_EVENT(ev_enum_name, ev_name, ev_description)	\
		ret |= cargo_add_option(cargo, 0,									\
			"<cmd> --" #ev_name "_cmd",										\
			"Command to run when the " #ev_name " event is triggered. "		\
			ev_description,													\
			"[s]+", &args->ev_name ## _cmd, &args->ev_name ## _cmd_count);	\
	ret |= cargo_set_metavar(cargo, "--" #ev_name "_cmd", "CMD [CMD ...]");

	#include "catcierge_events.h"

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --uservar -u",
			"Adds a user defined variable that can then be used when generating "
			"templates or executing custom commands. This is useful when passing "
			"passwords or similar, so those don't have to be defined in the script "
			"but when catcierge is started instead. And can then also be used in "
			"multiple places.",
			"[s]+", &args->user_vars, &args->user_var_count);

	return ret;
}

// TODO: Add windows support.
#ifndef _WIN32
static int parse_base_time(cargo_t ctx, void *user, const char *optname,
                      	   int argc, char **argv)
{
	struct tm base_time_tm;
	time_t base_time_t;
	struct timeval base_time_now;
	catcierge_args_t *args = (catcierge_args_t *)user;

	memset(&base_time_tm, 0, sizeof(base_time_tm));
	memset(&base_time_now, 0, sizeof(base_time_now));

	if (argc != 1)
	{
		cargo_set_error(ctx, 0, "%s expected 1 argument got %d", optname, argc);
		return -1;
	}

	catcierge_xfree(&args->base_time);

	if (!(args->base_time = strdup(argv[0])))
	{
		goto fail;
	}

	if (!strptime(args->base_time, "%Y-%m-%dT%H:%M:%S", &base_time_tm))
	{
		goto fail;
	}

	if ((base_time_t = mktime(&base_time_tm)) == -1)
	{
		goto fail;
	}

	gettimeofday(&base_time_now, NULL);
	args->base_time_diff = base_time_now.tv_sec - base_time_t;

	return 0;

fail:
	catcierge_xfree(&args->base_time);
	cargo_set_error(ctx, 0,
		"Failed to parse timestamp \"%s\" for %s, expected format: YYYY-mm-ddTHH:MM:SS",
		argv[0], optname);
	return -1;
}
#endif // _WIN32

static int add_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;
	assert(args);

	// If a config is specified, we stop parsing any other arguments.
	// In this way we can use two parse passes. First one to read the
	// config path, then read the config. And finally overwrite any settings
	// from the config by re-parsing the command line.
	ret |= cargo_add_option(cargo,
			CARGO_OPT_UNIQUE | CARGO_OPT_STOP | CARGO_OPT_STOP_HARD,
			"--config -c",
			NULL,
			"s", &args->config_path);
	ret |= cargo_set_option_description(cargo,
			"--config",
			"Path to the catcierge config file. Catcierge looks for %s "
			"by default, unless --no_default_config has been specified. "
			"Setting this overrides the default config.\n"
			#ifdef RPI
			"NOTE: Options for the raspberry pi camera settings --rpi, "
			"cannot be used in this config. Please see --config_rpi and "
			"--camhelp for details."
			#endif
			, CATCIERGE_CONF_PATH);

	#ifdef RPI
	ret |= cargo_add_option(cargo,
			CARGO_OPT_UNIQUE,
			"--config_rpi",
			NULL,
			"s", &args->config_path);
	ret |= cargo_set_option_description(cargo,
			"--config_rpi",
			"Path to config file for raspberry pi camera settings. See "
			"--camhelp for details. Default location: %s",
			CATCIERGE_RPI_CONF_PATH);
	#endif // RPI

	ret |= cargo_add_option(cargo, 0,
			"--no_default_config",
			NULL,
			"b", &args->no_default_config);
	ret |= cargo_set_option_description(cargo,
			"--no_default_config",
			"Do not load the default config (%s) if none is specified using --config",
			CATCIERGE_CONF_PATH);

	ret |= cargo_add_option(cargo, 0,
			"--chuid",
			"Run the process under this user when dropping root privileges "
			"which are needed for setting GPIO pins on the Raspberry Pi.",
			"s", &args->chuid);

	// Meant for the fsm_tester
	#ifndef _WIN32
	ret |= cargo_add_option(cargo, 0,
			"--base_time",
			"The base date time we should use instead of the current time. "
			"Only meant to be used when testing the code to have a repeatable "
			"time for replaying events.",
			"c", parse_base_time, args);
	#endif // _WIN32

	#ifdef RPI
	ret |= cargo_add_option(cargo,
			CARGO_OPT_STOP | CARGO_OPT_STOP_HARD,
			"--camhelp",
			"Show extra raspberry pi camera help",
			"b", &args->show_camhelp);
	#endif // RPI

	ret |= add_roi_options(cargo, args);
	ret |= add_matcher_options(cargo, args);
	ret |= add_lockout_options(cargo, args);
	#ifdef RPI
	ret |= add_gpio_options(cargo, args);
	#endif
	ret |= add_presentation_options(cargo, args);
	ret |= add_output_options(cargo, args);
	#ifdef WITH_RFID
	ret |= add_rfid_options(cargo, args);
	#endif
	ret |= add_command_options(cargo, args);

	return ret;
}

static void print_event_help(cargo_t cargo, catcierge_args_t *args)
{
	cargo_print_usage(cargo, 0);

	// TODO: The output settings here should be generated automatically.
	printf("Catcierge events and input templates:\n"
		   "\n"
		   "These events are triggered by catcierge at certain significant "
		   "moments during execution.\n"
		   "\n"
		   "On each event you can choose to run one or more commands "
		   "(see --cmdhelp), as well as generate text files containing "
		   "the state of catcierge (using templates specified via --input). "
		   "Also if ZMQ is enabled, the template can be published via that.\n"
		   "\n"
		   "Input templates can contain settings. These must be specified at "
		   "the top of the file, and are prefixed with \"%%!\".\n"
		   "\n"
		   "For example to filter on events in a template use this:\n"
		   "\n"
		   " %%!event name_of_event, name_of_event\n"
		   "\n"
		   "You can use '*' or 'all' to trigger on all events.\n"
		   "Note that you can have multiple input templates, each filtering "
		   "on different events.\n"
		   "\n"
		   "Other available input template settings are:\n"
		   " %%!name        The name of the template. Useful when passing on to "
		                   "scripts specified via the --xxx_cmd options. "
		                   "Use %%template_path:<name>%%.\n"
		   " %%!filename    The filename of the generated files, variables "
		                   "can be used here, see --cmdhelp for a list.\n"
		   " %%!topic       When ZMQ is enabled, this is the ZMQ topic used.\n"
		   " %%!nozmq       Don't publish this template via ZMQ.\n"
		   " %%!nofile      Don't output any file for this template "
		                  "(if you only want to publish it via ZMQ)\n"
		   " %%!rootpath    Make all paths relative to this path.\n"
		   "\n"
		   "List of events:\n");

	#define CATCIERGE_DEFINE_EVENT(ev_enum_name, ev_name, ev_description)	\
		printf(" %-20s %s\n", #ev_name, ev_description);
	#include "catcierge_events.h"

	printf("\n\n");
}

static void print_cmd_help(cargo_t cargo, catcierge_args_t *args)
{
	cargo_print_usage(cargo, 0);
	catcierge_output_print_usage();
	printf("\n");
	catcierge_template_output_print_usage();
	printf("\n");
	catcierge_haar_output_print_usage();
}

void catcierge_args_init_vars(catcierge_args_t *args)
{
	memset(args, 0, sizeof(catcierge_args_t));

	catcierge_template_matcher_args_init(&args->templ);
	catcierge_haar_matcher_args_init(&args->haar);
	args->config_path = strdup(CATCIERGE_CONF_PATH);
	args->saveimg = 1;
	args->save_obstruct_img = 0;
	args->match_time = DEFAULT_MATCH_WAIT;
	args->lockout_method = TIMER_ONLY_1;
	args->lockout_time = DEFAULT_LOCKOUT_TIME;
	args->consecutive_lockout_delay = DEFAULT_CONSECUTIVE_LOCKOUT_DELAY;
	args->ok_matches_needed = DEFAULT_OK_MATCHES_NEEDED;
	args->output_path = strdup(".");
	args->min_backlight = DEFAULT_MIN_BACKLIGHT;

	#ifdef RPI
	{
		RASPIVID_SETTINGS *settings = &args->rpi_settings;
		settings->width = 320;
		settings->height = 240;
		settings->bitrate = 500000;
		settings->framerate = 30;
		settings->immutableInput = 1;
		settings->graymode = 1;
		raspiCamCvSetDefaultCameraParameters(&settings->camera_parameters);
	
		args->lockout_gpio_pin = CATCIERGE_LOCKOUT_GPIO;
		args->backlight_gpio_pin = CATCIERGE_BACKLIGHT_GPIO;
		args->backlight_enable = 0;

		args->rpi_config_path = strdup(CATCIERGE_RPI_CONF_PATH);
	}
	#endif // RPI

	#ifdef WITH_ZMQ
	args->zmq_port = DEFAULT_ZMQ_PORT;
	args->zmq_iface = strdup(DEFAULT_ZMQ_IFACE);
	args->zmq_transport = strdup(DEFAULT_ZMQ_TRANSPORT);
	#endif
}

void catcierge_args_destroy_vars(catcierge_args_t *args)
{
	size_t i;

	catcierge_xfree(&args->program_name);

	catcierge_xfree(&args->output_path);
	catcierge_xfree(&args->match_output_path);
	catcierge_xfree(&args->steps_output_path);
	catcierge_xfree(&args->obstruct_output_path);
	catcierge_xfree(&args->template_output_path);

	#ifdef WITH_ZMQ
	catcierge_xfree(&args->zmq_iface);
	catcierge_xfree(&args->zmq_transport);
	#endif

	for (i = 0; i < args->input_count; i++)
	{
		catcierge_xfree(&args->inputs[i]);
	}

	args->input_count = 0;
	catcierge_xfree(&args->inputs);

	catcierge_xfree(&args->log_path);

	#define CATCIERGE_DEFINE_EVENT(ev_enum_name, ev_name, ev_description) 		\
		catcierge_xfree(&args->ev_name ## _cmd);								\
		catcierge_free_list(args->ev_name ## _cmd, args->ev_name ## _cmd_count);\
		args->ev_name ## _cmd_count = 0;										\
		args->ev_name ## _cmd = NULL;
	#include "catcierge_events.h"

	catcierge_xfree(&args->config_path);
	catcierge_xfree(&args->chuid);

	for (i = 0; i < (size_t)args->temp_config_count; i++)
	{
		catcierge_xfree(&args->temp_config_values[i]);
	}

	catcierge_xfree(&args->temp_config_values);
	args->temp_config_count = 0;

	catcierge_xfree(&args->base_time);

	#ifdef WITH_RFID
	catcierge_xfree(&args->rfid_inner_path);
	catcierge_xfree(&args->rfid_outer_path);

	catcierge_free_list(args->rfid_allowed, args->rfid_allowed_count);
	args->rfid_allowed_count = 0;
	args->rfid_allowed = NULL;
	#endif // WITH_RFID

	catcierge_haar_matcher_args_destroy(&args->haar);
	catcierge_template_matcher_args_destroy(&args->templ);
}

int catcierge_args_init(catcierge_args_t *args, const char *progname)
{
	int ret = 0;
	assert(args);

	catcierge_args_init_vars(args);

	if (cargo_init(&args->cargo, 0, "%s", progname))
	{
		CATERR("Failed to init command line parsing\n");
		return -1;
	}

	cargo_set_description(args->cargo,
		"Catcierge saves you from cleaning the floor!");

	cargo_set_epilog(args->cargo,
		"Signals:\n"
		"The program can receive signals that can be sent using the kill command.\n"
		" SIGUSR1 = Force the cat door to unlock\n"
		" SIGUSR2 = Force the cat door to lock (for lock timeout)\n");

	ret = add_options(args->cargo, args);

	assert(ret == 0);

	return 0;
}

int catcierge_args_parse(catcierge_args_t *args, int argc, char **argv)
{
	int ret = 0;
	cargo_t cargo = args->cargo;
	assert(args);

	#ifdef RPI
	if (args->rpi_config_path)
	{
		int is_default_rpi_config = !strcmp(args->rpi_config_path, CATCIERGE_RPI_CONF_PATH);

		if (catcierge_parse_rpi_config(args->rpi_config_path, args))
		{
			if (!is_default_rpi_config)
			{
				ret = -1; goto fail;
			}
		}
	}

	// Let the raspicam software parse any --rpi settings
	// and remove them from the list of arguments (argv is replaced).
	if (!(argv = catcierge_parse_rpi_args(&argc, argv, args)))
	{
		CATERR("Failed to parse Raspberry Pi camera settings. See --camhelp\n");
		ret = -1; goto fail;
	}
	#endif // RPI

	// Parse once to get --config value.
	if (cargo_parse(cargo, CARGO_SKIP_CHECK_MUTEX | CARGO_SKIP_CHECK_REQUIRED, 1, argc, argv))
	{
		ret = -1; goto fail;
	}

	// Parse config (if it is set).
	if (args->config_path)
	{
		int confret = 0;
		int is_default_config = !strcmp(args->config_path, CATCIERGE_CONF_PATH);
		int skip_default_config = (args->no_default_config || (getenv("CATCIERGE_NO_DEFAULT_CONFIG") != NULL));

		if (is_default_config)
		{
			CATLOG("Using default config\n");
		}

		if (is_default_config && skip_default_config)
		{
			CATLOG("Default config turned off, ignoring: %s\n", CATCIERGE_CONF_PATH);
		}
		else
		{
			CATLOG("Config path: %s\n", args->config_path);

			// Read ini file and translate that into an argv that cargo can parse.
			confret = parse_config(cargo, args->config_path, &args->ini_args);

			if (confret < 0)
			{
				CATERR("Failed to parse config\n");
				ret = -1; goto fail;
			}
			else if ((confret == 1) && !is_default_config)
			{
				CATERR("WARNING: Specified config %s does not exist\n", args->config_path);
			}
			else
			{
				CATLOG("Parsed config\n");
			}
		}
	}
	else
	{
		CATLOG("No config file specified\n");
	}

	// And finally parse the commandline to override config settings.
	if (cargo_parse(cargo, 0, 1, argc, argv))
	{
		ret = -1; goto fail;
	}

	if (args->show_cmd_help)
	{
		print_cmd_help(cargo, args);
		ret = -1; goto fail;
	}

	if (args->show_event_help)
	{
		print_event_help(cargo, args);
		ret = -1; goto fail;
	}

	#ifdef RPI
	if (args->show_camhelp)
	{
		print_cam_help(cargo);
		ret = -1; goto fail;
	}
	#endif // RPI

fail:
	#ifdef RPI
	cargo_free_commandline(&argv, argc);
	#endif // RPI

	return ret;
}

void catcierge_print_settings(catcierge_args_t *args)
{
	#ifdef WITH_RFID
	size_t i;
	#endif

	print_line(stdout, 80, "-");
	printf("Settings:\n");
	print_line(stdout, 80, "-");
	printf("General:\n");
	printf("       Startup delay: %0.1f seconds\n", args->startup_delay);
	printf("            Auto ROI: %d\n", args->auto_roi);
	if (args->auto_roi)
	{
	printf("  Auto ROI threshold: %d\n", args->auto_roi_thr);
	printf(" Min. backlight area: %d\n", args->min_backlight);
	}
	printf("          Show video: %d\n", args->show);
	printf("        Save matches: %d\n", args->saveimg);
	printf("       Save obstruct: %d\n", args->save_obstruct_img);
	printf("          Save steps: %d\n", args->save_steps);
	printf("     Highlight match: %d\n", args->highlight_match);
	printf("       Lockout dummy: %d\n", args->lockout_dummy);
	printf("      Lockout method: %d\n", args->lockout_method);
	printf("           Lock time: %d seconds\n", args->lockout_time);
	printf("       Lockout error: %d %s\n", args->max_consecutive_lockout_count,
							(args->max_consecutive_lockout_count == 0) ? "(off)" : "");
	printf("   Lockout err delay: %0.1f\n", args->consecutive_lockout_delay);
	printf("       Match timeout: %d seconds\n", args->match_time);
	printf("            Log file: %s\n", args->log_path ? args->log_path : "-");
	printf("            No color: %d\n", args->nocolor);
	printf("        No animation: %d\n", args->noanim);
	printf("   Ok matches needed: %d\n", args->ok_matches_needed);
	printf("         Output path: %s\n", args->output_path);
	if (args->match_output_path && strcmp(args->output_path, args->match_output_path))
	printf("   Match output path: %s\n", args->match_output_path);
	if (args->steps_output_path && strcmp(args->output_path, args->steps_output_path))
	printf("   Steps output path: %s\n", args->steps_output_path);
	if (args->obstruct_output_path && strcmp(args->output_path, args->obstruct_output_path))
	printf("Obstruct output path: %s\n", args->obstruct_output_path);
	if (args->template_output_path && strcmp(args->output_path, args->template_output_path))
	printf("Template output path: %s\n", args->template_output_path);
	#ifdef WITH_ZMQ
	printf("       ZMQ publisher: %d\n", args->zmq);
	printf("            ZMQ port: %d\n", args->zmq_port);
	printf("       ZMQ interface: %s\n", args->zmq_iface);
	printf("       ZMQ transport: %s\n", args->zmq_transport);
	#endif // WITH_ZMQ
	printf("\n"); 
	if (args->matcher_type == MATCHER_TEMPLATE)
	{
		printf("        Matcher type: template\n");
		catcierge_template_matcher_print_settings(&args->templ);
	}
	else
	{
		printf("        Matcher type: haar\n");
		catcierge_haar_matcher_print_settings(&args->haar);
	}
	#ifdef WITH_RFID
	printf("RFID:\n");
	printf("          Inner RFID: %s\n", args->rfid_inner_path ? args->rfid_inner_path : "-");
	printf("          Outer RFID: %s\n", args->rfid_outer_path ? args->rfid_outer_path : "-");
	printf("     Lock on no RFID: %d\n", args->lock_on_invalid_rfid);
	printf("      RFID lock time: %.2f seconds\n", args->rfid_lock_time);
	printf("        Allowed RFID: %s\n", (args->rfid_allowed_count <= 0) ? "-" : args->rfid_allowed[0]);
	for (i = 1; i < args->rfid_allowed_count; i++)
	{
		printf("                 %s\n", args->rfid_allowed[i]);
	}
	#endif // WITH_RFID
	print_line(stdout, 80, "-");
}

catcierge_matcher_args_t *catcierge_get_matcher_args(catcierge_args_t *args)
{
	catcierge_matcher_args_t *margs = NULL;

	if (args->matcher_type == MATCHER_TEMPLATE)
	{
		margs = (catcierge_matcher_args_t *)&args->templ;
	}
	else if (args->matcher_type == MATCHER_HAAR)
	{
		margs = (catcierge_matcher_args_t *)&args->haar;
	}

	// TODO: This is an ugly way to pass this on... But whatever for now.
	if (margs)
	{
		margs->roi = &args->roi;
		margs->min_backlight = args->min_backlight;
		margs->auto_roi_thr = args->auto_roi_thr;
		margs->save_auto_roi_img = args->save_auto_roi_img;
	}

	return margs;
}

void catcierge_args_destroy(catcierge_args_t *args)
{
	cargo_destroy(&args->cargo);
	ini_args_destroy(&args->ini_args);
	catcierge_args_destroy_vars(args);
}
