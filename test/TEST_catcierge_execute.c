#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_fsm.h"
#include "minunit.h"
#include "catcierge_test_config.h"
#include "catcierge_test_helpers.h"
#include "catcierge_util.h"

char *run_execute_test()
{
	catcierge_execute("echo %1 %0 %1 %cwd", "%s %s", "Hello", "World");
	catcierge_reset_cursor_position();
	return NULL;
}

int TEST_catcierge_execute(int argc, char **argv)
{
	char *e = NULL;
	int ret = 0;

	CATCIERGE_RUN_TEST((e = run_execute_test()),
		"Run execute test",
		"Execute test", &ret);

	return ret;
}
