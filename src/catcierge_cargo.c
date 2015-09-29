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

int parse_cmdargs(int argc, char **argv)
{
	int i;
	int ret = 0;
	cargo_t cargo;
	catcierge_args_t args;

	memset(&args, 0, sizeof(args));
	//args_init(&args);

	if (cargo_init(&cargo, CARGO_AUTOCLEAN, argv[0]))
	{
		fprintf(stderr, "Failed to init command line parsing\n");
		return -1;
	}

	cargo_set_description(cargo,
		"Specify a configuration file, and try overriding the values set "
		"in it using the command line options.");

	/*
	// Combine flags using OR.
	ret |= cargo_add_option(cargo, 0,
			"--verbose -v", "Verbosity level",
			"b|", &args.debug, 4, ERROR, WARN, INFO, DEBUG);

	ret |= cargo_add_option(cargo, 0,
			"--config -c", "Path to config file",
			"s", &args.config_path);

	ret |= cargo_add_group(cargo, 0, "vals", "Values", "Some options to test with.");
	ret |= cargo_add_option(cargo, 0, "<vals> --alpha -a", "Alpha", "i", &args.a);
	ret |= cargo_add_option(cargo, 0, "<vals> --beta -b", "Beta", "i", &args.b);
	ret |= cargo_add_option(cargo, 0, "<vals> --centauri", "Centauri", "i", &args.c);
	ret |= cargo_add_option(cargo, 0, "<vals> --delta -d", "Delta", "i", &args.d);
	*/
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
