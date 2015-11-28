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

#ifdef WITH_RFID
#include "catcierge_rfid.h"
#endif // WITH_RFID

static int add_lockout_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	ret |= cargo_add_group(cargo, 0, "lockout", "Lockout settings", NULL);

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
			"<!matcher_type, matcher> --template_matcher --templ",
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
			"See --cmdhelp for available variables.");

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
			"b", &args->save_obstruct_img);

	ret |= cargo_add_option(cargo, 0,
			"<output> --template --input",
			"Path to one or more template files generated on specified events. "
			"(Not to be confused with the template matcher)",
			".[s]+", &args->inputs, &args->input_count, MAX_INPUT_TEMPLATES);

	ret |= cargo_add_option(cargo, 0,
			"<output> --output_path --output",
			"Path to where the match images and generated templates "
			"should be saved.",
			"s", &args->output_path);
	ret |= cargo_set_metavar(cargo, "--output_path", "PATH");

	ret |= cargo_add_option(cargo, 0,
			"<output> --match_output_path",
			"Override --output_path for match images and save them here instead. "
			"If --new_execute is used, this can be relative to --output_path "
			"by using %%output_path%% in the path.",
			"s", &args->match_output_path);
	ret |= cargo_set_metavar(cargo, "--match_output_path", "PATH");

	ret |= cargo_add_option(cargo, 0,
			"<output> --steps_output_path",
			"If --save_steps is enabled, save step images to this path. "
			"Same as for --match_output_path, overrides --output_path.",
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
			"i", &args->rfid_lock_time);

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
			"occur.\n"
			"Variables can be passed to these commands such as:\n"
			"  %%state%%, %%match_success%% and so on.\n"
			"To see a list of variables use --cmdhelp");

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --old_execute",
			"Turn on the old execute support.",
			"b=", &args->new_execute, 0);

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --new_execute",
			"Use the new execute support (default on).",
			"D");

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --frame_obstructed_cmd",
			"Command to run when the frame becomes obstructed "
			"and a new match is initiated. (--save_obstruct must be on).",
			"s", &args->frame_obstructed_cmd);
	ret |= cargo_set_metavar(cargo, "--frame_obstructed_cmd", "CMD");

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --match_cmd",
			"Command to run after a match is made.",
			"s", &args->match_cmd);
	ret |= cargo_set_metavar(cargo, "--match_cmd", "CMD");

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --save_img_cmd",
			"Command to run at the moment a match image is saved.",
			"s", &args->save_img_cmd);
	ret |= cargo_set_metavar(cargo, "--save_img_cmd", "CMD");

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --match_group_done_cmd",
			"Command that runs when all match images have been saved to disk.\n"
			"(This is most likely what you want to use in most cases).",
			"s", &args->match_group_done_cmd);
	ret |= cargo_set_metavar(cargo, "--match_group_done_cmd", "CMD");

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --match_done_cmd",
			"Command to run when a match is done.",
			"s", &args->match_done_cmd);
	ret |= cargo_set_metavar(cargo, "--match_done_cmd", "CMD");

	#ifdef WITH_RFID
	ret |= cargo_add_option(cargo, 0,
			"<cmd> --rfid_detect_cmd",
			"Command to run when one of the RFID readers detects a tag.",
			"s", &args->rfid_detect_cmd);
	ret |= cargo_set_metavar(cargo, "--rfid_detect_cmd", "CMD");

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --rfid_match_cmd",
			"Command that is run when a RFID match is made.",
			"s", &args->rfid_match_cmd);
	ret |= cargo_set_metavar(cargo, "--rfid_match_cmd", "CMD");
	#endif // WITH_RFID

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --do_lockout_cmd",
			"Command to run when the lockout should be performed. "
			"This will override the normal lockout method.",
			"s", &args->do_lockout_cmd);
	ret |= cargo_set_metavar(cargo, "--do_lockout_cmd", "CMD");

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --do_unlock_cmd",
			"Command that is run when we should unlock. "
			"This will override the normal unlock method.",
			"s", &args->do_unlock_cmd);
	ret |= cargo_set_metavar(cargo, "--do_unlock_cmd", "CMD");

	ret |= cargo_add_option(cargo, 0,
			"<cmd> --state_change_cmd",
			"Command to run when the state machine changes state.",
			"s", &args->state_change_cmd);
	ret |= cargo_set_metavar(cargo, "--state_change_cmd", "CMD");


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

static int add_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;
	assert(args);

	args->new_execute = 1;

	ret |= cargo_add_option(cargo, 0,
			"--config -c",
			"Path to config file",
			"s", &args->config_path);

	ret |= cargo_add_option(cargo, 0,
			"--chuid",
			"Run the process under this user when dropping root privileges "
			"which are needed for setting GPIO pins on the Raspberry Pi.",
			"s", &args->chuid);

	ret |= cargo_add_option(cargo, 0,
			"--startup_delay",
			"Number of seconds to wait after starting before starting "
			"to capture anything. This is so that if you have a back light "
			"that is turned on at startup, it has time to turn on, otherwise "
			"the program will think something is obstructing the image and "
			"start trying to match.",
			"i", &args->startup_delay);

	ret |= cargo_add_option(cargo, 0,
			"--roi",
			"Crop all input image to this region of interest.",
			"[c]#", parse_CvRect, &args->roi, NULL, 4);
	ret |= cargo_set_metavar(cargo,
			"--roi",
			"X Y WIDTH HEIGHT");

	ret |= cargo_add_option(cargo, 0,
			"--auto_roi",
			"Automatically crop to the area covered by the backlight. "
			"This will be done after --startup_delay has ended. "
			"Overrides --roi.",
			"b", &args->chuid);

	ret |= cargo_add_option(cargo, 0,
			"--min_backlight",
			"If --auto_roi is on, this sets the minimum allowed area the "
			"backlight is allowed to be before it is considered broken. "
			"If it is smaller than this, the program will exit. Default 10000.",
			"b", &args->chuid);

	#ifdef RPI
	ret |= cargo_add_option(cargo,
			CARGO_OPT_STOP | CARGO_OPT_STOP_HARD,
			"--camhelp",
			"Show extra raspberry pi camera help",
			"b", &args->show_camhelp);
	#endif // RPI

	ret |= add_matcher_options(cargo, args);
	ret |= add_lockout_options(cargo, args);
	ret |= add_presentation_options(cargo, args);
	ret |= add_output_options(cargo, args);
	#ifdef WITH_RFID
	ret |= add_rfid_options(cargo, args);
	#endif
	ret |= add_command_options(cargo, args);

	return ret;
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

static void print_line(FILE *fd, int length, const char *s)
{
	int i;
	for (i = 0; i < length; i++)
		fputc(*s, fd);
	fprintf(fd, "\n");
}

#ifdef RPI
static void print_cam_help(cargo_t cargo)
{
	int console_width = cargo_get_width(cargo, 0) - 1;
	print_line(stderr, console_width, "-");
	fprintf(stderr, "Raspberry Pi camera settings:\n");
	fprintf(stderr, "(Prepend these with --rpi. Example: --rpi--saturation or --rpi-sa)\n");
	print_line(stderr, console_width, "-");
	raspicamcontrol_display_help();
	print_line(stderr, console_width, "-");
	fprintf(stderr, "\nNote! To use the above command line parameters\n");
	fprintf(stderr, "you must prepend them with \"rpi-\".\n");
	fprintf(stderr, "For example --rpi-ISO\n");
	print_line(stderr, console_width, "-");
}

static char **parse_rpi_args(int *argc, char **argv, catcierge_args_t *args)
{
	int i;
	int j;
	int rpi_count = 0;
	char **nargv;
	int nargc = 0;
	int used = 0;
	assert(argc);
	assert(argv);
	assert(args);

	if (!(nargv = calloc(*argc, sizeof(char *))))
	{
		return NULL;
	}

	for (i = 0, j = 0; i < *argc; i++)
	{
		if (!strncmp(argv[i], "--rpi-", sizeof("--rpi-") - 1))
		{
			int rpiret = 0;
			char *next_arg = NULL;
			const char *rpikey = argv[i] + sizeof("--rpi") - 1;

			if ((i + 1) < *argc)
			{
				next_arg = argv[i + 1];

				if (*next_arg == '-')
				{
					next_arg = NULL;
					i++;
				}
			}

			if ((used = raspicamcontrol_parse_cmdline(
				&args->rpi_settings.camera_parameters,
				rpikey, next_arg)) == 0)
			{
				fprintf(stderr, "Error parsing rpi-cam option:\n  %s\n", rpikey);
				goto fail;
			}

			rpi_count += used;
		}
		else
		{
			nargv[j++] = strdup(argv[i]);
		}
	}

	*argc -= rpi_count;

	return nargv;
fail:
	if (nargv)
	{
		cargo_free_commandline(&nargv, nargc);
	}

	return NULL;
}
#endif // RPI

void catcierge_args_init_vars(catcierge_args_t *args)
{
	memset(args, 0, sizeof(catcierge_args_t));

	catcierge_template_matcher_args_init(&args->templ);
	catcierge_haar_matcher_args_init(&args->haar);
	args->saveimg = 1;
	args->save_obstruct_img = 0;
	args->match_time = DEFAULT_MATCH_WAIT;
	args->lockout_method = TIMER_ONLY_1;
	args->lockout_time = DEFAULT_LOCKOUT_TIME;
	args->consecutive_lockout_delay = DEFAULT_CONSECUTIVE_LOCKOUT_DELAY;
	args->ok_matches_needed = DEFAULT_OK_MATCHES_NEEDED;
	args->output_path = strdup(".");
	args->min_backlight = 10000;

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
		printf("Free %s, loc %p\n", args->inputs[i], &args->inputs[i]);
		catcierge_xfree(&args->inputs[i]);
	}

	args->input_count = 0;
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

	ret = add_options(args->cargo, args);

	assert(ret == 0);

	return 0;
}

int catcierge_args_parse(catcierge_args_t *args, int argc, char **argv)
{
	int i;
	int ret = 0;
	cargo_t cargo = args->cargo;
	assert(args);

	//ret = catcierge_args_init(args, argv[0]);
	//assert(ret == 0);

	#ifdef RPI
	// Let the raspicam software parse any --rpi settings
	// and remove them from the list of arguments (argv is replaced).
	if (!(argv = parse_rpi_args(&argc, argv, args)))
	{
		CATERR("Failed to parse Raspberry Pi camera settings. See --camhelp\n");
		goto fail; ret = -1;
	}
	#endif // RPI

	// Parse once to get --config value.
	if (cargo_parse(cargo, 0, 1, argc, argv))
	{
		ret = -1; goto fail;
	}

	// Read ini file and translate that into an argv that cargo can parse.
	if (args->config_path)
	{
		CATLOG("Config path: %s\n", args->config_path);

		if (parse_config(cargo, args->config_path, &args->ini_args))
		{
			ret = -1; goto fail;
		}
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

	#ifdef RPI
	if (args->show_camhelp)
	{
		print_cam_help(cargo);
	}
	#endif // RPI

fail:
	//cargo_destroy(&cargo);
	//ini_args_destroy(&args->ini_args);
	#ifdef RPI
	cargo_free_commandline(&argv, argc);
	#endif // RPI

	return ret;
}

void catcierge_args_destroy(catcierge_args_t *args)
{
	cargo_destroy(&args->cargo);
	ini_args_destroy(&args->ini_args);
	catcierge_args_destroy_vars(args);
}
