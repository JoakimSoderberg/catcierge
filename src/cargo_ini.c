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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "cargo_ini.h"

static void alini_cb(alini_parser_t *parser,
					 char *section, char *key, char *value)
{
	conf_arg_t *it = NULL;
	conf_ini_args_t *args = (conf_ini_args_t *)alini_parser_get_context(parser);

	// Either find an existing config argument with this key.
	HASH_FIND_STR(args->config_args, key, it);

	if (!it)
	{
		// Or add a new config argument.
		if (!(it = calloc(1, sizeof(conf_arg_t))))
		{
			fprintf(stderr, "Out of memory\n");
			exit(-1);
		}

		strncpy(it->key, key, MAX_CONFIG_KEY - 1);
		HASH_ADD_STR(args->config_args, key, it);
	}

	it->key_count++;
	
	if (!(it->vals[it->val_count++] = strdup(value)))
	{
		fprintf(stderr, "Out of memory\n");
		exit(-1);
	}
}

void ini_args_destroy(conf_ini_args_t *args)
{
	size_t i;
	conf_arg_t *it = NULL;
	conf_arg_t *tmp = NULL;

	// free the hash table contents.
	HASH_ITER(hh, args->config_args, it, tmp)
	{
		HASH_DEL(args->config_args, it);
		for (i = 0; i < it->val_count; i++)
		{
			free(it->vals[i]);
			it->vals[i] = NULL;
		}

		free(it);
	}

	cargo_free_commandline(&args->config_argv, args->config_argc);
}

void print_hash(conf_arg_t *config_args)
{
	size_t i = 0;
	conf_arg_t *it = NULL;
	conf_arg_t *tmp = NULL;

	// free the hash table contents.
	HASH_ITER(hh, config_args, it, tmp)
	{
		printf("(%lu) %s = ", it->val_count, it->key);

		for (i = 0; i < it->val_count; i++)
		{
			printf("%s", it->vals[i]);
			if (i != (it->val_count - 1))
			{
				printf(",");
			}
		}

		printf("\n");
	}
}

void print_commandline(conf_ini_args_t *args)
{
	int i;
	for (i = 0; i < args->config_argc; i++)
	{
		printf("%s ", args->config_argv[i]);
	}

	printf("\n");
}

cargo_type_t guess_expanded_name(cargo_t cargo, conf_arg_t *it,
								 char *tmpkey, size_t tmpkey_len)
{
	cargo_type_t type;
	int i = 1;

	// TODO: Maybe cargo should simply have a function that gets this
	// Hack to figure out what prefix to use, "-", "--", and so on...
	do
	{
		snprintf(tmpkey, tmpkey_len, "%*.*s%s", i, i, "------", it->key);
		type = cargo_get_option_type(cargo, tmpkey);
		i++;
	}
	while (((int)type == -1) && (i < CARGO_NAME_COUNT));

	return (i < CARGO_NAME_COUNT) ? type : -1;
}

int build_config_commandline(cargo_t cargo, conf_ini_args_t *args)
{
	size_t j = 0;
	int i = 0;
	conf_arg_t *it = NULL;
	conf_arg_t *tmp = NULL;
	char tmpkey[1024];
	cargo_type_t type;
	
	args->config_argc = 0;

	HASH_ITER(hh, args->config_args, it, tmp)
	{
		// Take a keyname, for instance "delta" and try "-delta", "--delta"
		// and so on until a match is found in a hackish way...
		it->type = guess_expanded_name(cargo, it,
						it->expanded_key, sizeof(it->expanded_key) - 1);

		// Booleans can be repeated so we parse them in the form
		// key=integer
		if (it->type == CARGO_BOOL)
		{
			if (it->key_count != 1)
			{
				fprintf(stderr, "Error, multiple occurances of '%s' found. "
					"To repeat a flag please use '%s=%lu' instead.\n",
					it->key, it->key, it->key_count);
				return -1;
			}

			// TODO: Get rid of atoi use....
			if (strlen(it->vals[0]) == 0)
			{
				fprintf(stderr, "Error, please supply an integer value to "
					"the '%s' flag\n", it->key);
				return -1;
			}
			else
			{
				it->args_count = atoi(it->vals[0]);
			}
		}
		else
		{
			it->args_count = 1 + it->val_count;
		}

		args->config_argc += it->args_count;
	}

	if (!(args->config_argv = calloc(args->config_argc, sizeof(char *))))
	{
		fprintf(stderr, "Out of memory!\n");
		exit(-1);
	}

	HASH_ITER(hh, args->config_args, it, tmp)
	{
		if (it->type == CARGO_BOOL)
		{
			for (j = 0; j < it->args_count; j++)
			{
				if (!(args->config_argv[i++] = strdup(it->expanded_key)))
				{
					fprintf(stderr, "Out of memory!\n");
					exit(-1);
				}
			}
		}
		else
		{
			if (!(args->config_argv[i++] = strdup(tmpkey)))
			{
				fprintf(stderr, "Out of memory!\n");
				exit(-1);
			}

			for (j = 0; j < it->val_count; j++)
			{
				if (!(args->config_argv[i++] = strdup(it->vals[j])))
				{
					fprintf(stderr, "Out of memory!\n");
					exit(-1);
				}
			}
		}
	}

	return 0;
}

static int perform_config_parse(cargo_t cargo, const char *config_path,
								conf_ini_args_t *args)
{
	int cfg_err = 0;

	if ((cfg_err = alini_parser_create(&args->parser, config_path)) < 0)
	{
		cargo_print_usage(cargo, CARGO_USAGE_SHORT_USAGE);
		fprintf(stderr, "\nFailed to load config: %s\n", config_path);
		return -1;
	}

	alini_parser_setcallback_foundkvpair(args->parser, alini_cb);
	alini_parser_set_context(args->parser, args);
	alini_parser_start(args->parser);

	return 0;
}

int parse_config(cargo_t cargo, const char *config_path, conf_ini_args_t *args)
{
	cargo_parse_result_t err = 0;

	// Parse the ini-file and store contents in a hash table.
	if (perform_config_parse(cargo, config_path, args))
	{
		return -1;
	}

	// Build an "argv" string from the hashtable contents:
	//   key1 = 123
	//   key1 = 456
	//   key2 = 789
	// Becomes:
	//   --key1 123 456 --key2 789
	if (build_config_commandline(cargo, args))
	{
		return -1;
	}

	/*
	if (args->debug)
	{
		print_hash(args->ini_args.config_args);
		print_commandline(&args->ini_args);
	}*/

	// Parse the "fake" command line using cargo. We turn off the
	// internal error output, so the errors are more in the context
	// of a config file.
	if ((err = cargo_parse(cargo, CARGO_NOERR_OUTPUT,
							0, args->config_argc,
							args->config_argv)))
	{
		size_t k = 0;

		cargo_print_usage(cargo, CARGO_USAGE_SHORT_USAGE);

		fprintf(stderr, "Failed to parse config: %d\n", err);
		
		switch (err)
		{
			case CARGO_PARSE_UNKNOWN_OPTS:
			{
				const char *opt = NULL;
				size_t unknown_count = 0;
				const char **unknowns = cargo_get_unknown(cargo, &unknown_count);

				fprintf(stderr, "Unknown options:\n");

				for (k = 0; k < unknown_count; k++)
				{
					opt = unknowns[k] + strspn("--", unknowns[k]);
					fprintf(stderr, " %s\n", opt);
				}
				break;
			}
			default: break;
		}

		return -1;
	}

	return 0;
}


