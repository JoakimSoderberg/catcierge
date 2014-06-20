#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_args.h"
#include "minunit.h"
#include "catcierge_test_helpers.h"
#include "catcierge_config.h"

typedef char *(*test_func)();

char *run_test1()
{
	catcierge_args_t args;

	char *test1[] = 
	{
		"catcierge",
		"--snout", "abc.png", "def.png",
		"--show"
	};
	#define TEST1_COUNT (sizeof(test1) / sizeof(char *))

	catcierge_args_init(&args);

	// Parse command line arguments.
	mu_assert("Failed to parse args", !catcierge_parse_cmdargs(&args, TEST1_COUNT, test1));

	mu_assert("Expected show to be set", args.show);
	catcierge_test_STATUS("Snout count %lu", args.templ.snout_count);
	mu_assert("Expected snout count to be 2", args.templ.snout_count == 2);
	mu_assert("Unexpected snout", !strcmp(test1[2], args.templ.snout_paths[0]));
	
	catcierge_args_destroy(&args);

	return NULL;
}

char *run_test2()
{
	catcierge_args_t args;

	char *test1[] = 
	{
		"catcierge",
		"--show",
		"--save",
		"--highlight",
		"--lockout", "5",
		"--lockout_error", "3",
		"--lockout_dummy",
		"--matchtime", "10"
		#ifdef WITH_RFID
		,"--rfid_allowed", "444,555,3333"
		#endif
	};
	#define TEST1_COUNT (sizeof(test1) / sizeof(char *))

	catcierge_args_init(&args);

	// Parse command line arguments.
	mu_assert("Failed to parse args", !catcierge_parse_cmdargs(&args, TEST1_COUNT, test1));
	
	#ifdef WITH_RFID
	mu_assert("Expected RFID allowed to be 3", args.rfid_allowed_count == 3);
	#endif // WITH_RFID
	
	catcierge_args_destroy(&args);

	return NULL;
}

static void free_values(char **values, size_t value_count)
{
	size_t i;

	for (i = 0; i < value_count; i++)
	{
		free(values[i]);
		values[i] = NULL;
	}
}

char *run_parse_args_tests()
{
	int i;
	catcierge_args_t args;
	char *key = NULL;
	char *values[4096];
	char *pch = NULL;
	int ret = 0;
	size_t value_count = 0;

	catcierge_args_init(&args);

	#define GET_KEY_VALUES(str) \
		do \
		{ \
			char s[] = str; \
			value_count = 0; \
			pch = strtok(s, " "); \
			key = pch; \
			while (pch != NULL) \
			{ \
				pch = strtok(NULL, " "); \
				if (pch) values[value_count++] = strdup(pch); \
			} \
		} while(0)

	#define PARSE_SETTING(str, expect, assert_stmt) \
		GET_KEY_VALUES(str); \
		ret = catcierge_parse_setting(&args, key, values, value_count); \
		free_values(values, value_count); \
		catcierge_test_STATUS(str ". Assert: %d <= " #assert_stmt, assert_stmt); \
		mu_assert(expect, assert_stmt); \
		catcierge_test_SUCCESS("  Parsed: \"%s\"", str)

	// Helper macro for a single setting that can either be set using:
	// "--hello" or "--hello 2".
	// Example: PARSE_SINGLE_SETTING("hello", args.hello, 2, "Expected")
	#define PARSE_SINGLE_SETTING(setting_str, setting, val) \
		PARSE_SETTING(setting_str " 1", "Expected valid parse.", \
			(ret == 0) && (setting == val)); \
		PARSE_SETTING(setting_str, "Expected valid parse.", \
			(ret == 0) && (args.haar.no_match_is_fail == 1))

	// Haar matcher settings.
	{
		PARSE_SETTING("matcher", "Expected value for matcher", (ret == -1));
		PARSE_SETTING("matcher haar", "Expected haar matcher",
			(ret == 0) &&
			!strcmp(args.matcher, "haar") &&
			(args.matcher_type == MATCHER_HAAR));
		PARSE_SETTING("matcher template", "Expected template matcher",
			(ret == 0) &&
			!strcmp(args.matcher, "template") &&
			(args.matcher_type == MATCHER_TEMPLATE));

		PARSE_SETTING("cascade /path/to/catcierge.xml", "Expected valid cascade path",
			(ret == 0) && !strcmp(args.haar.cascade, "/path/to/catcierge.xml"));
		PARSE_SETTING("cascade", "Expected failure for missing value.", (ret == -1));

		PARSE_SETTING("min_size 12x34", "Expected valid min size.",
			(ret == 0) && (args.haar.min_width == 12) && (args.haar.min_height == 34));
		PARSE_SETTING("min_size", "Expected failure for missing value.", (ret == -1));
		PARSE_SETTING("min_size kakaxpoop", "Expected failure for invalid value.", (ret == -1));

		PARSE_SETTING("prey_steps 5", "Expected valid min size.",
			(ret == 0) && (args.haar.prey_steps == 5));
		PARSE_SETTING("prey_steps", "Expected min size value.", (ret == -1));
	
		PARSE_SINGLE_SETTING("no_match_is_fail", args.haar.no_match_is_fail, 1);
		PARSE_SINGLE_SETTING("equalize_historgram", args.haar.eq_histogram, 1);

		PARSE_SETTING("in_direction left", "Expected left direction",
			(ret == 0) && (args.haar.in_direction == DIR_LEFT));
		PARSE_SETTING("in_direction right", "Expected right direction",
			(ret == 0) && (args.haar.in_direction == DIR_RIGHT));
		PARSE_SETTING("in_direction bla", "Expected failure for invalid input",
			(ret == -1));
		PARSE_SETTING("in_direction", "Expected failure for missing input",
			(ret == -1));

		PARSE_SETTING("prey_method adaptive", "Expected adaptive prey method",
			(ret == 0) && (args.haar.prey_method == PREY_METHOD_ADAPTIVE));
		PARSE_SETTING("prey_method normal", "Expected normal prey method",
			(ret == 0) && (args.haar.prey_method == PREY_METHOD_NORMAL));
		PARSE_SETTING("prey_method blarg", "Expected invalid prey method",
			(ret == -1));
		PARSE_SETTING("prey_method", "Expected failure for missing value",
			(ret == -1));
	}

	return NULL;
}

int TEST_catcierge_args(int argc, char **argv)
{
	int ret = 0;
	int i;
	test_func funcs[] = { run_test1, run_test2 };
	#define FUNC_COUNT (sizeof(funcs) / sizeof(test_func))
	char *e = NULL;
	catcierge_test_HEADLINE("TEST_catcierge_args");

	CATCIERGE_RUN_TEST((e = run_parse_args_tests()),
		"Run parse all args test",
		"Parsed all args test", &ret);

	for (i = 0; i < FUNC_COUNT; i++)
	{
		if ((e = funcs[i]())) { catcierge_test_FAILURE("%s", e); ret = -1; }
		else catcierge_test_SUCCESS("");
	}
	return ret;
}
