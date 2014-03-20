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
	catcierge_test_STATUS("Snout count %lu", args.snout_count);
	mu_assert("Expected snout count to be 2", args.snout_count == 2);
	mu_assert("Unexpected snout", !strcmp(test1[2], args.snout_paths[0]));
	
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

int TEST_catcierge_args(int argc, char **argv)
{
	int ret = 0;
	int i;
	test_func funcs[] = { run_test1, run_test2 };
	#define FUNC_COUNT (sizeof(funcs) / sizeof(test_func))
	char *e = NULL;
	catcierge_test_HEADLINE("TEST_catcierge_args");

	for (i = 0; i < FUNC_COUNT; i++)
	{
		if ((e = funcs[i]())) { catcierge_test_FAILURE("%s", e); ret = -1; }
		else catcierge_test_SUCCESS("");
	}
	return ret;
}
