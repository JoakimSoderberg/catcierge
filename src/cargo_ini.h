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

#ifndef __CARGO_INI_H__
#define __CARGO_INI_H__

#include "alini.h"
#include "cargo.h"
#include "uthash.h"
#include "catcierge_platform.h"

#define MAX_CONFIG_KEY 1024
#define MAX_CONFIG_VAL 1024

typedef struct conf_arg_s
{
	char key[MAX_CONFIG_KEY];
	size_t key_count; // Number of occurances of this key.

	// In the ini file any prefix characters are omitted so
	// this will contain those keys with the proper prefix added.
	// For example: "verbose" -> "--verbose"
	char expanded_key[MAX_CONFIG_KEY];
	cargo_type_t type;

	// If a key is specified multiple times we accumulate
	// all the values here so we can group them in the output.
	//  mykey=1
	//  mykey=2
	//  mykey=3
	// Becomes:
	// --mykey 1 2 3
	char *vals[MAX_CONFIG_VAL];
	size_t val_count;

	int linenumber;

	// Final count of arguments to generate for key
	// (different for booleans).
	size_t args_count; 
	
	// Hash table used to keep track of duplicate keys.
	UT_hash_handle hh;
} conf_arg_t;

typedef struct conf_ini_args_s
{
	alini_parser_t *parser;

	// The final command line that represents the contents of ini file.
	char **config_argv;
	int config_argc;

	// Used in the ini parsing stage to store the variables.
	conf_arg_t *config_args;
	//size_t config_args_count;
} conf_ini_args_t;

int parse_config(cargo_t cargo, const char *config_path, conf_ini_args_t *args);

void ini_args_destroy(conf_ini_args_t *args);

#endif // __CARGO_INI_H__

