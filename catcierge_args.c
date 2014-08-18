
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <catcierge_config.h>
#include "catcierge_args.h"
#include "catcierge_log.h"
#ifdef WITH_RFID
#include "catcierge_rfid.h"
#endif // WITH_RFID
#include "catcierge_util.h"
#include "catcierge_output.h"

#include "alini/alini.h"

#include "catcierge_template_matcher.h"
#include "catcierge_haar_matcher.h"

#ifdef WITH_RFID
static int catcierge_create_rfid_allowed_list(catcierge_args_t *args, const char *allowed)
{
	if (!(args->rfid_allowed = catcierge_parse_list(allowed, &args->rfid_allowed_count, 1)))
	{
		return -1;
	}

	return 0;
}

void catcierge_free_rfid_allowed_list(catcierge_args_t *args)
{
	catcierge_free_list(args->rfid_allowed, args->rfid_allowed_count);
	args->rfid_allowed_count = 0;
}
#endif // WITH_RFID

int catcierge_parse_setting(catcierge_args_t *args, const char *key, char **values, size_t value_count)
{
	size_t i;
	int ret;
	assert(args);
	assert(key);

	if (!strcmp(key, "matcher"))
	{
		if (value_count == 1)
		{
			args->matcher = values[0];
			if (strcmp(args->matcher, "template")
				&& strcmp(args->matcher, "haar"))
			{
				fprintf(stderr, "Invalid template type \"%s\"\n", args->matcher);
				return -1;
			}

			args->matcher_type = !strcmp(args->matcher, "template")
				? MATCHER_TEMPLATE : MATCHER_HAAR;

			return 0;
		}
		else
		{
			fprintf(stderr, "Missing value for --matcher\n");
			return -1;
		}

		return 0;
	}

	ret = catcierge_haar_matcher_parse_args(&args->haar, key, values, value_count);
	if (ret < 0) return -1;
	else if (!ret) return 0;

	ret = catcierge_template_matcher_parse_args(&args->templ, key, values, value_count);
	if (ret < 0) return -1;
	else if (!ret) return 0;

	if (!strcmp(key, "show"))
	{
		args->show = 1;
		if (value_count == 1) args->show = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "ok_matches_needed"))
	{
		if (value_count == 1)
		{
			args->ok_matches_needed = atoi(values[0]);

			if ((args->ok_matches_needed < 0)
				|| (args->ok_matches_needed > MATCH_MAX_COUNT))
			{
				fprintf(stderr, "--ok_matches_needed must be between 0 and %d\n", MATCH_MAX_COUNT);
				return -1;
			}

			return 0;
		}

		fprintf(stderr, "--ok_matches_needed missing an integer value\n");
		return -1;
	}

	if (!strcmp(key, "lockout_method"))
	{
		if (value_count == 1)
		{
			args->lockout_method = atoi(values[0]);

			if ((args->lockout_method < OBSTRUCT_OR_TIMER_1)
			 || (args->lockout_method > TIMER_ONLY_3))
			{
				fprintf(stderr, "--lockout_method needs a value between %d and %d\n",
					OBSTRUCT_OR_TIMER_1, TIMER_ONLY_3);
				return -1;
			}

			return 0;
		}

		fprintf(stderr, "--lockout_method missing an integer value\n");
		return -1;
	}

	if (!strcmp(key, "save"))
	{
		args->saveimg = 1;
		if (value_count == 1) args->saveimg = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "new_execute"))
	{
		args->new_execute = 1;
		if (value_count == 1) args->new_execute = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "highlight"))
	{
		args->highlight_match = 1;
		if (value_count == 1) args->highlight_match = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "lockout"))
	{
		args->lockout_time = DEFAULT_LOCKOUT_TIME;
		if (value_count == 1) args->lockout_time = atoi(values[0]);

		return 0;
	}

	if (!strcmp(key, "lockout_error_delay"))
	{
		if (value_count == 1)
		{
			args->consecutive_lockout_delay = atof(values[0]);
			return 0;
		}

		fprintf(stderr, "--lockout_error_delay missing a seconds value\n");
		return -1;
	}

	if (!strcmp(key, "lockout_error"))
	{
		if (value_count == 1)
		{
			args->max_consecutive_lockout_count = atoi(values[0]);
			return 0;
		}

		fprintf(stderr, "--lockout_error missing a value\n");
		return -1;
	}

	if (!strcmp(key, "lockout_dummy"))
	{
		args->lockout_dummy = 1;
		if (value_count == 1) args->lockout_dummy = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "matchtime"))
	{
		args->match_time = DEFAULT_MATCH_WAIT;
		if (value_count == 1) args->match_time = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "output"))
	{
		if (value_count == 1)
		{
			args->output_path = values[0];
			return 0;
		}

		fprintf(stderr, "--output missing path value\n");
		return -1;
	}

	if (!strcmp(key, "input"))
	{
		if (value_count == 0)
		{
			fprintf(stderr, "--input missing value\n");
			return -1;
		}

		for (i = 0; i < value_count; i++)
		{
			if (args->input_count >= MAX_INPUT_TEMPLATES)
			{
				fprintf(stderr, "Max template input reached %d\n", MAX_INPUT_TEMPLATES);
				return -1;
			}
			args->inputs[args->input_count] = values[i];
			args->input_count++;
		}

		return 0;
	}

	#ifdef WITH_RFID
	if (!strcmp(key, "rfid_in"))
	{
		if (value_count == 1)
		{
			args->rfid_inner_path = values[0];
			return 0;
		}

		fprintf(stderr, "--rfid_in missing path value\n");
		return -1;
	}

	if (!strcmp(key, "rfid_out"))
	{
		if (value_count == 1)
		{
			args->rfid_outer_path = values[0];
			return 0;
		}

		fprintf(stderr, "--rfid_out missing path value\n");
		return -1;
	}

	if (!strcmp(key, "rfid_allowed"))
	{
		if (value_count == 1)
		{
			if (catcierge_create_rfid_allowed_list(args, values[0]))
			{
				CATERR("Failed to create RFID allowed list\n");
				return -1;
			}

			return 0;
		}

		fprintf(stderr, "--rfid_allowed missing comma separated list of values\n");
		return -1;
	}

	if (!strcmp(key, "rfid_time"))
	{
		if (value_count == 1)
		{
			args->rfid_lock_time = (double)atof(values[0]);
			return 0;
		}

		fprintf(stderr, "--rfid_time missing seconds value (float)\n");
		return -1;
	}

	if (!strcmp(key, "rfid_lock"))
	{
		args->lock_on_invalid_rfid = 1;
		if (value_count == 1) args->lock_on_invalid_rfid = atoi(values[0]);
		return 0;
	}
	#endif // WITH_RFID

	if (!strcmp(key, "log"))
	{
		if (value_count == 1)
		{
			args->log_path = values[0];
			return 0;
		}

		fprintf(stderr, "--log missing path value\n");
		return -1;
	}

	if (!strcmp(key, "match_cmd"))
	{
		if (value_count == 1)
		{
			args->match_cmd = values[0];
			return 0;
		}

		fprintf(stderr, "--match_cmd missing value\n");
		return -1;
	}

	if (!strcmp(key, "save_img_cmd"))
	{
		if (value_count == 1)
		{
			args->save_img_cmd = values[0];
			return 0;
		}

		fprintf(stderr, "--save_img_cmd missing value\n");
		return -1;
	}

	if (!strcmp(key, "save_imgs_cmd"))
	{
		if (value_count == 1)
		{
			args->save_imgs_cmd = values[0];
			return 0;
		}

		fprintf(stderr, "--save_imgs_cmd missing value\n");
		return -1;
	}

	if (!strcmp(key, "match_done_cmd"))
	{
		if (value_count == 1)
		{
			args->match_done_cmd = values[0];
			return 0;
		}

		fprintf(stderr, "--match_done_cmd missing value\n");
		return -1;
	}

	if (!strcmp(key, "do_lockout_cmd"))
	{
		if (value_count == 1)
		{
			args->do_lockout_cmd = values[0];
			return 0;
		}

		fprintf(stderr, "--do_lockout_cmd missing value\n");
		return -1;
	}

	if (!strcmp(key, "do_unlock_cmd"))
	{
		if (value_count == 1)
		{
			args->do_unlock_cmd = values[0];
			return 0;
		}

		fprintf(stderr, "--do_unlock cmd missing value\n");
		return -1;
	}

	if (!strcmp(key, "nocolor"))
	{
		args->nocolor = 1;
		if (value_count == 1) args->nocolor = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "noanim"))
	{
		args->noanim = 1;
		if (value_count == 1) args->noanim = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "save_steps"))
	{
		args->save_steps = 1;
		if (value_count == 1) args->save_steps = atoi(values[0]);
		return 0;
	}

	if (!strcmp(key, "chuid"))
	{
		if (value_count == 1)
		{
			args->chuid = values[0];
			return 0;
		}

		fprintf(stderr, "--chuid missing value\n");
		return -1;
	}

	#ifdef WITH_RFID
	if (!strcmp(key, "rfid_detect_cmd"))
	{
		if (value_count == 1)
		{
			args->rfid_detect_cmd = values[0];
			return 0;
		}

		fprintf(stderr, "--rfid_detect_cmd missing value\n");
		return -1;
	}

	if (!strcmp(key, "rfid_match_cmd"))
	{
		if (value_count == 1)
		{
			args->rfid_match_cmd = values[0];
			return 0;
		}

		fprintf(stderr, "--rid_match_cmd missing value\n");
		return -1;
	}
	#endif // WITH_RFID

	#ifdef RPI
	if (!strncmp(key, "rpi-", 4))
	{
		int rpiret = 0;
		const char *rpikey = key + strlen("rpi");

		if (!raspicamcontrol_parse_cmdline(&args->rpi_settings.camera_parameters,
			rpikey, (value_count == 1) ? values[0] : NULL))
		{
			return -1;
		}
		return 0;
	}
	#endif // RPI

	return -1;
}

int temp_config_count = 0;
#define MAX_TEMP_CONFIG_VALUES 128
char *temp_config_values[MAX_TEMP_CONFIG_VALUES];

static void alini_cb(alini_parser_t *parser, char *section, char *key, char *value)
{
	char *value_cpy = NULL;
	catcierge_args_t *args = (catcierge_args_t *)alini_parser_get_context(parser);

	// We must make a copy of the string and keep it so that
	// it won't dissapear after the parser has done its thing.
	if (temp_config_count >= MAX_TEMP_CONFIG_VALUES)
	{
		fprintf(stderr, "Max %d config file values allowed.\n", MAX_TEMP_CONFIG_VALUES);
		return;
	}

	if (!(value_cpy = strdup(value)))
	{
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}

	temp_config_values[temp_config_count] = value_cpy;

	if (catcierge_parse_setting(args, key, &temp_config_values[temp_config_count], 1) < 0)
	{
		fprintf(stderr, "Failed to parse setting in config: %s\n", key);
	}

	temp_config_count++;
}

static void catcierge_config_free_temp_strings(catcierge_args_t *args)
{
	int i;

	for (i = 0; i < args->temp_config_count; i++)
	{
		free(args->temp_config_values[i]);
		args->temp_config_values[i] = NULL;
	}
}

void catcierge_show_usage(catcierge_args_t *args, const char *prog)
{
	fprintf(stderr, "Usage: %s [options]\n\n", prog);
	fprintf(stderr, "General settings:\n");
	fprintf(stderr, "-----------------\n");
	fprintf(stderr, " --lockout_method <1|2|3>\n");
	fprintf(stderr, "                        Defines the method used to decide when to unlock:\n");
	fprintf(stderr, "                        [1: Wait for clear frame or that the timer has timed out.]\n");
	fprintf(stderr, "                         2: Wait for clear frame and then start unlock timer.\n");
	fprintf(stderr, "                         3: Only use the timer, don't care about clear frame.\n");
	fprintf(stderr, " --lockout <seconds>    The time in seconds a lockout takes. Default %d seconds.\n", DEFAULT_LOCKOUT_TIME);
	fprintf(stderr, " --lockout_error <n>    Number of lockouts in a row that's allowed before we\n");
	fprintf(stderr, "                        consider it an error and quit the program. \n");
	fprintf(stderr, "                        Default is to never do this.\n");
	fprintf(stderr, " --lockout_error_delay <seconds>\n");
	fprintf(stderr, "                        The delay in seconds between lockouts that should be\n");
	fprintf(stderr, "                        counted as a consecutive lockout. Default %0.1f\n", DEFAULT_CONSECUTIVE_LOCKOUT_DELAY);
	fprintf(stderr, " --lockout_dummy        Do everything as normal, but don't actually\n");
	fprintf(stderr, "                        lock the door. This is useful for testing.\n");
	fprintf(stderr, " --matchtime <seconds>  The time to wait after a match. Default %d seconds.\n", DEFAULT_MATCH_WAIT);
	fprintf(stderr, " --show                 Show GUI of the camera feed (X11 only).\n");
	fprintf(stderr, " --save                 Save match images (both ok and failed).\n");
	fprintf(stderr, " --save_steps           Save each step of the matching algorithm.\n");
	fprintf(stderr, "                        (--save must also be turned on)\n");
	fprintf(stderr, " --highlight            Highlight the best match on saved images.\n");
	fprintf(stderr, " --nocolor              Turn off all color output.\n");
	fprintf(stderr, " --noanim               Turn off any animation.\n");
	fprintf(stderr, " --input <path>         Path to one or more template files generated on certain events.\n");
	fprintf(stderr, " --output <path>        Path to where the match images should be saved.\n");
	fprintf(stderr, " --log <path>           Log matches and rfid readings (if enabled).\n");
	fprintf(stderr, " --config <path>        Path to config file. Default is ./catcierge.cfg\n");
	fprintf(stderr, "                        or /etc/catcierge.cfg\n");
	fprintf(stderr, "                        This is parsed as an INI file. The keys/values are\n");
	fprintf(stderr, "                        the same as these options.\n");
	fprintf(stderr, " --ok_matches_needed <number>\n");
	fprintf(stderr, "                        The number of matches out of %d matches\n", MATCH_MAX_COUNT);
	fprintf(stderr, "                        that need to be OK for the match to be considered\n");
	fprintf(stderr, "                        an over all OK match.\n");
	fprintf(stderr, " --matcher <template|haar>\n");
	fprintf(stderr, "                        The type of matcher to use. Haar cascade is more\n");
	fprintf(stderr, "                        accurate if a well trained cascade exists for your cat.\n");
	fprintf(stderr, "                        Template matcher is simpler, but doesn't require a trained cascade\n");
	fprintf(stderr, "                        this is useful while gathering enough data to train the cascade.\n");
	fprintf(stderr, "Haar cascade matcher:\n");
	fprintf(stderr, "---------------------\n");
	catcierge_haar_matcher_usage();
	fprintf(stderr, "Template matcher:\n");
	fprintf(stderr, "-----------------\n");
	fprintf(stderr, "\nThe snout image refers to the image of the cat snout that is matched against.\n");
	fprintf(stderr, "This image should be based on a 320x240 resolution image taken by the rpi camera.\n");
	fprintf(stderr, "If no path is specified \"snout.png\" in the current directory is used.\n");
	fprintf(stderr, "\n");
	catcierge_template_matcher_usage();
	#ifdef WITH_RFID
	fprintf(stderr, "\n");
	fprintf(stderr, "RFID:\n");
	fprintf(stderr, "-----\n");
	fprintf(stderr, " --rfid_in <path>       Path to inner RFID reader. Example: /dev/ttyUSB0\n");
	fprintf(stderr, " --rfid_out <path>      Path to the outter RFID reader.\n");
	fprintf(stderr, " --rfid_lock            Lock if no RFID tag present or invalid RFID tag. Default OFF.\n");
	fprintf(stderr, " --rfid_time <seconds>  Number of seconds to wait after a valid match before the\n");
	fprintf(stderr, "                        RFID readers are checked.\n");
	fprintf(stderr, "                        (This is so that there is enough time for the cat to be read by both readers)\n");
	fprintf(stderr, " --rfid_allowed <list>  A comma separated list of allowed RFID tags. Example: %s\n", EXAMPLE_RFID_STR);
	#endif // WITH_RFID
	fprintf(stderr, "\n");
	#define EPRINT_CMD_HELP(fmt, ...) if (args->show_cmd_help) fprintf(stderr, fmt, ##__VA_ARGS__);
	fprintf(stderr, "Commands (new):\n");
	fprintf(stderr, "---------------\n");
	fprintf(stderr, "This is enabled using --new_execute. %%0, %%1... are deprecated, instead variable names are used.\n");
	fprintf(stderr, "For example: %%state%%, %%match_success%% and so on.\n");
	fprintf(stderr, "See --cmdhelp for a list of variables. Otherwise this uses the same\n");
	fprintf(stderr, "command line arguments as the old commands section below.\n");
	if (args->show_cmd_help) catcierge_output_print_usage();
	fprintf(stderr, "\n");
	fprintf(stderr, "Commands (old):\n");
	fprintf(stderr, "---------------\n");
	fprintf(stderr, "(Note that %%0, %%1, ... will be replaced in the input, see --cmdhelp for details)\n");
	EPRINT_CMD_HELP("\n");
	EPRINT_CMD_HELP("   General: %%cwd will output the current working directory for this program.\n");
	EPRINT_CMD_HELP("            Any paths returned are relative to this.\n");
	EPRINT_CMD_HELP("            %%%% Produces a literal %% sign.\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --match_cmd <cmd>      Command to run after a match is made.\n");
	EPRINT_CMD_HELP("                         %%0 = [float] Match result.\n");
	EPRINT_CMD_HELP("                         %%1 = [0/1]   Success or failure.\n");
	EPRINT_CMD_HELP("                         %%2 = [path]  Path to where image will be saved\n");
	EPRINT_CMD_HELP("                                       (Not saved yet!)\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --save_img_cmd <cmd>   Command to run at the moment a match image is saved.\n");
	EPRINT_CMD_HELP("                         %%0 = [float]  Match result, 0.0-1.0\n");
	EPRINT_CMD_HELP("                         %%1 = [0/1]    Match success.\n");
	EPRINT_CMD_HELP("                         %%2 = [string] Image path (Image is saved to disk).\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --save_imgs_cmd <cmd>  Command that runs when all match images have been saved\n");
	fprintf(stderr, "                        to disk.\n");
	fprintf(stderr, "                        (This is most likely what you want to use in most cases)\n");
	EPRINT_CMD_HELP("                         %%0 = [0/1]    Match success.\n");
	EPRINT_CMD_HELP("                         %%1 = [string] Image path for first match.\n");
	EPRINT_CMD_HELP("                         %%2 = [string] Image path for second match.\n");
	EPRINT_CMD_HELP("                         %%3 = [string] Image path for third match.\n");
	EPRINT_CMD_HELP("                         %%4 = [string] Image path for fourth match.\n");
	EPRINT_CMD_HELP("                         %%5 = [float]  First image result.\n");
	EPRINT_CMD_HELP("                         %%6 = [float]  Second image result.\n");
	EPRINT_CMD_HELP("                         %%7 = [float]  Third image result.\n");
	EPRINT_CMD_HELP("                         %%8 = [float]  Fourth image result.\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --match_done_cmd <cmd> Command to run when a match is done.\n");
	EPRINT_CMD_HELP("                         %%0 = [0/1]    Match success.\n");
	EPRINT_CMD_HELP("                         %%1 = [int]    Successful match count.\n");
	EPRINT_CMD_HELP("                         %%2 = [int]    Max matches.\n");
	EPRINT_CMD_HELP("\n");
	#ifdef WITH_RFID
	fprintf(stderr, " --rfid_detect_cmd <cmd> Command to run when one of the RFID readers detects a tag.\n");
	EPRINT_CMD_HELP("                         %%0 = [string] RFID reader name.\n");
	EPRINT_CMD_HELP("                         %%1 = [string] RFID path.\n");
	EPRINT_CMD_HELP("                         %%2 = [0/1]    Is allowed.\n");
	EPRINT_CMD_HELP("                         %%3 = [0/1]    Is data incomplete.\n");
	EPRINT_CMD_HELP("                         %%4 = [string] Tag data.\n");
	EPRINT_CMD_HELP("                         %%5 = [0/1]    Other reader triggered.\n");
	EPRINT_CMD_HELP("                         %%6 = [string] Direction (in, out or uknown).\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --rfid_match_cmd <cmd> Command that is run when a RFID match is made.\n");
	EPRINT_CMD_HELP("                         %%0 = Match success.\n");
	EPRINT_CMD_HELP("                         %%1 = RFID inner in use.\n");
	EPRINT_CMD_HELP("                         %%2 = RFID outer in use.\n");
	EPRINT_CMD_HELP("                         %%3 = RFID inner success.\n");
	EPRINT_CMD_HELP("                         %%4 = RFID outer success.\n");
	EPRINT_CMD_HELP("                         %%5 = RFID inner data.\n");
	EPRINT_CMD_HELP("                         %%6 = RFID outer data.\n");
	EPRINT_CMD_HELP("\n");
	fprintf(stderr, " --do_lockout_cmd <cmd> Command to run when the lockout should be performed.\n");
	fprintf(stderr, "                        This will override the normal lockout method.\n");
	fprintf(stderr, " --do_unlock_cmd <cmd>  Command that is run when we should unlock.\n");
	fprintf(stderr, "                        This will override the normal unlock method.\n");
	fprintf(stderr, "\n");
	#endif // WITH_RFID
	fprintf(stderr, " --help                 Show this help.\n");
	fprintf(stderr, " --cmdhelp              Show extra command help.\n");
	#ifdef RPI
	fprintf(stderr, " --camhelp              Show extra Raspberry pi camera settings help.\n");
	fprintf(stderr, "                        Note that all these settings must be prepended\n");
	fprintf(stderr, "                        with \"rpi-\"\n");
	#endif
	#ifndef _WIN32
	fprintf(stderr, "Signals:\n");
	fprintf(stderr, "The program can receive signals that can be sent using the kill command.\n");
	fprintf(stderr, " SIGUSR1 = Force the cat door to unlock\n");
	fprintf(stderr, " SIGUSR2 = Force the cat door to lock (for lock timeout)\n");
	fprintf(stderr, "\n");
	#endif // _WIN32
}

#ifdef RPI
static void catcierge_show_cam_help()
{
	fprintf(stderr, "--------------------------------------------------------------------------------\n");
	fprintf(stderr, "Raspberry Pi camera settings:\n");
	fprintf(stderr, "--------------------------------------------------------------------------------\n");
	raspicamcontrol_display_help();
	fprintf(stderr, "--------------------------------------------------------------------------------\n");
	fprintf(stderr, "\nNote! To use the above command line parameters\n");
	fprintf(stderr, "you must prepend them with \"rpi-\".\n");
	fprintf(stderr, "For example --rpi-ISO\n");
	fprintf(stderr, "--------------------------------------------------------------------------------\n");
}
#endif // RPI

int catcierge_parse_cmdargs(catcierge_args_t *args, int argc, char **argv)
{
	int ret = 0;
	int i;
	char *key = NULL;
	char **values = (char **)calloc(argc, sizeof(char *));
	size_t value_count = 0;

	if (!values)
	{
		fprintf(stderr, "Out of memory!\n");
		return -1;
	}

	for (i = 1; i < argc; i++)
	{
		// These are command line specific.
		if (!strcmp(argv[i], "--cmdhelp"))
		{
			args->show_cmd_help = 1;
			catcierge_show_usage(args, argv[0]);
			exit(1);
		}

		#ifdef RPI
		if (!strcmp(argv[i], "--camhelp"))
		{
			catcierge_show_cam_help();
			exit(1);
		}
		#endif // RPI

		if (!strcmp(argv[i], "--help"))
		{
			catcierge_show_usage(args, argv[0]);
			exit(1);
		}

		if (!strcmp(argv[i], "--config"))
		{
			// This isn't handled here, but don't treat it as an error.
			// (This is parsed in catcierge_parse_config).
			continue;
		}

		// Normal options. These can be parsed from the
		// config file as well.
		if (!strncmp(argv[i], "--", 2))
		{
			int j = i + 1;
			key = &argv[i][2];
			memset(values, 0, value_count * sizeof(char *));
			value_count = 0;

			// Look for values for the option.
			// Continue fetching values until we get another option
			// or there are no more options.
			while ((j < argc) && strncmp(argv[j], "--", 2))
			{
				values[value_count] = argv[j];
				value_count++;
				i++;
				j = i + 1;
			}

			if ((ret = catcierge_parse_setting(args, key, values, value_count)) < 0)
			{
				fprintf(stderr, "Failed to parse command line arguments for \"%s\"m\n", key);

				if (values)
					free(values);

				return ret;
			}
		}
	}

	free(values);

	return 0;
}

int catcierge_parse_config(catcierge_args_t *args, int argc, char **argv)
{
	int i;
	int cfg_err = 0;
	assert(args);

	// If the user has supplied us with a config use that.
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--config"))
		{
			if (argc >= (i + 1))
			{
				i++;
				args->config_path = argv[i];
				continue;
			}
			else
			{
				fprintf(stderr, "No config path specified\n");
				return -1;
			}
		}
	}

	if (args->config_path)
	{
		printf("Using config: %s\n", args->config_path);

		if ((cfg_err = alini_parser_create(&args->parser, args->config_path)) < 0)
		{
			fprintf(stderr, "Failed to load config %s\n", args->config_path);
			return -1;
		}
	}
	else
	{
		// Use default.
		if ((cfg_err = alini_parser_create(&args->parser, "catcierge.cfg")) < 0)
		{
			cfg_err = alini_parser_create(&args->parser, "/etc/catcierge.cfg");
		}
	}

	if (!cfg_err)
	{
		alini_parser_setcallback_foundkvpair(args->parser, alini_cb);
		alini_parser_set_context(args->parser, args);
		alini_parser_start(args->parser);
	}
	else
	{
		fprintf(stderr, "No config file found\n");
	}

	return 0;
}

void catcierge_print_settings(catcierge_args_t *args)
{
	#ifdef WITH_RFID
	size_t i;
	#endif

	printf("--------------------------------------------------------------------------------\n");
	printf("Settings:\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("General:\n");
	printf("        Show video: %d\n", args->show);
	printf("      Save matches: %d\n", args->saveimg);
	printf("   Highlight match: %d\n", args->highlight_match);
	printf("     Lockout dummy: %d\n", args->lockout_dummy);
	printf("    Lockout method: %d\n", args->lockout_method);
	printf("         Lock time: %d seconds\n", args->lockout_time);
	printf("     Lockout error: %d %s\n", args->max_consecutive_lockout_count,
							(args->max_consecutive_lockout_count == 0) ? "(off)" : "");
	printf(" Lockout err delay: %0.1f\n", args->consecutive_lockout_delay);
	printf("     Match timeout: %d seconds\n", args->match_time);
	printf("          Log file: %s\n", args->log_path ? args->log_path : "-");
	printf("          No color: %d\n", args->nocolor);
	printf("      No animation: %d\n", args->noanim);
	printf(" Ok matches needed: %d\n", args->ok_matches_needed);
	printf("       Output path: %s\n", args->output_path);
	printf("\n");
	if (!args->matcher || !strcmp(args->matcher, "template"))
		catcierge_template_matcher_print_settings(&args->templ);
	else
		catcierge_haar_matcher_print_settings(&args->haar);
	#ifdef WITH_RFID
	printf("RFID:\n");
	printf("        Inner RFID: %s\n", args->rfid_inner_path ? args->rfid_inner_path : "-");
	printf("        Outer RFID: %s\n", args->rfid_outer_path ? args->rfid_outer_path : "-");
	printf("   Lock on no RFID: %d\n", args->lock_on_invalid_rfid);
	printf("    RFID lock time: %.2f seconds\n", args->rfid_lock_time);
	printf("      Allowed RFID: %s\n", (args->rfid_allowed_count <= 0) ? "-" : args->rfid_allowed[0]);
	for (i = 1; i < args->rfid_allowed_count; i++)
	{
		printf("                 %s\n", args->rfid_allowed[i]);
	}
	#endif // WITH_RFID
	printf("--------------------------------------------------------------------------------\n");

}

int catcierge_args_init(catcierge_args_t *args)
{
	memset(args, 0, sizeof(catcierge_args_t));

	catcierge_template_matcher_args_init(&args->templ);
	catcierge_haar_matcher_args_init(&args->haar);
	args->saveimg = 1;
	args->match_time = DEFAULT_MATCH_WAIT;
	args->lockout_method = OBSTRUCT_OR_TIMER_1;
	args->lockout_time = DEFAULT_LOCKOUT_TIME;
	args->consecutive_lockout_delay = DEFAULT_CONSECUTIVE_LOCKOUT_DELAY;
	args->ok_matches_needed = DEFAULT_OK_MATCHES_NEEDED;

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

	return 0;
}

void catcierge_args_destroy(catcierge_args_t *args)
{
	assert(args);

	catcierge_config_free_temp_strings(args);

	#ifdef WITH_RFID
	catcierge_free_rfid_allowed_list(args);
	#endif

	if (args->parser)
	{
		alini_parser_dispose(args->parser);
	}
}

