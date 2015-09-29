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
#include "cargo.h"
#include "cargo_ini.h"
#include "catcierge_args.h"

int add_lockout_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	ret |= cargo_add_group(cargo, 0, "lockout", "Lockout settings", NULL);

	// TODO: Verify value
	ret |= cargo_add_option(cargo, 0,
			"<lockout> --lockout_method",
			"Defines the method used to decide when to unlock:\n"
			"[1: Only use the timer, don't care about clear frame.]\n"
			"2: Wait for clear frame or that the timer has timed out.\n"
			"3: Wait for clear frame and then start unlock timer.\n",
			"i", &args->lockout_method);

	ret |= cargo_add_option(cargo, 0,
			"--show",
			"Show GUI of the camera feed (X11 only).",
			"b", &args->show);

	return ret;
}

int add_matcher_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	// TODO: Add functions for matcher arguments

	ret |= cargo_add_group(cargo, 0, "matcher_opts", "Matcher settings", NULL);

	// TODO: Verify between 0 and MATCH_MAX_COUNT
	ret |= cargo_add_option(cargo, 0,
			"<matcher_opts> --ok_matches_needed", NULL,
			"b", &args->ok_matches_needed);
	ret |= cargo_set_option_description(cargo,
			"--ok_matches_needed",
			"The number of matches out of %d matches "
			"that need to be OK for the match to be considered "
			"an over all OK match", MATCH_MAX_COUNT);

	return ret;
}

static int add_options(cargo_t cargo, catcierge_args_t *args)
{
	int ret = 0;

	ret |= cargo_add_option(cargo, 0,
			"--config -c", "Path to config file",
			"s", &args->config_path);

	ret |= cargo_add_mutex_group(cargo,
			CARGO_MUTEXGRP_ONE_REQUIRED |
			CARGO_MUTEXGRP_GROUP_USAGE, 
			"matcher", 
			"Matcher",
			"The algorithm to use when matching for the cat profile image");

	ret |= cargo_add_option(cargo, 0,
			"<!matcher> --template_matcher",
			"Template based matching algorithm",
			"b=", &args->matcher_type, MATCHER_TEMPLATE);

	ret |= cargo_add_option(cargo, 0,
			"<!matcher> --haar_matcher",
			"Haar feature based matching algorithm (recommended)",
			"b=", &args->matcher_type, MATCHER_HAAR);

	ret |= add_matcher_options(cargo, args);

	ret |= add_lockout_options(cargo, args);

	return ret;
}

int parse_cmdargs(int argc, char **argv)
{
	int i;
	int ret = 0;
	cargo_t cargo;
	catcierge_args_t args;

	memset(&args, 0, sizeof(args));

	if (cargo_init(&cargo, 0, argv[0]))
	{
		fprintf(stderr, "Failed to init command line parsing\n");
		return -1;
	}

	cargo_set_description(cargo,
		"Catcierge saves you from cleaning the floor!");

	ret = add_options(cargo, &args);

	assert(ret == 0);

	// Parse once to get --config value.
	if (cargo_parse(cargo, 0, 1, argc, argv))
	{
		goto fail;
	}

	// Read ini file and translate that into an argv that cargo can parse.
	printf("Config path: %s\n", args.config_path);
	if (args.config_path && parse_config(cargo, args.config_path, &args.ini_args))
	{
		goto fail;
	}

	// And finally parse the commandline to override config settings.
	if (cargo_parse(cargo, 0, 1, argc, argv))
	{
		goto fail;
	}

fail:
	cargo_destroy(&cargo);
	ini_args_destroy(&args.ini_args);

	return 0;
}
