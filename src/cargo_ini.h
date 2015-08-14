//
// The MIT License (MIT)
//
// Copyright (c) 2015 Joakim Soderberg <joakim.soderberg@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#include "alini.h"
#include "cargo.h"
#include "uthash.h"

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
	size_t config_args_count;
} conf_ini_args_t;

int parse_config(cargo_t cargo, const char *config_path, conf_ini_args_t *args);

void ini_args_destroy(conf_ini_args_t *args);

