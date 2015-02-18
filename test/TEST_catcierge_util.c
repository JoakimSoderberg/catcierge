#include <stdlib.h>
#include <stdio.h>
#include "catcierge_util.h"
#include "catcierge_log.h"
#include "minunit.h"
#include "catcierge_test_helpers.h"

char *run_test_catcierge_get_abs_path()
{
	char buf[4096];
	char medium_buf[4];
	char small_buf[1];
	char *res = NULL;
	char *filename = "path_test_file";

	FILE *f = NULL;

	if (!(f = fopen("path_test_file", "w")))
	{
		return "Failed to open test file";
	}

	res = catcierge_get_abs_path(filename, buf, sizeof(buf));
	catcierge_test_STATUS("Large buffer: %s -> %s", filename, res);
	mu_assert("Expected valid path with large buffer", res != NULL);

	res = catcierge_get_abs_path(filename, medium_buf, sizeof(medium_buf));
	catcierge_test_STATUS("Medium buffer: %s -> %s", filename, res);
	mu_assert("Expected invalid path with too small buffer", res == NULL);

	res = catcierge_get_abs_path(filename, small_buf, sizeof(small_buf));
	catcierge_test_STATUS("Small buffer: %s -> %s", filename, res);
	mu_assert("Expected invalid path with too small buffer", res == NULL);

	log_printc(f, COLOR_NORMAL, "123abc");

	fclose(f);

	return NULL;
}

static char *run_make_path_tests()
{
	#ifdef _WIN32
	// Because you cannot use the "\\?\" prefix with a relative path,
	// relative paths are always limited to a total of MAX_PATH characters.
	https ://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
	#define LONG "loooooooooooooooooooooooooooooong"
	#else
	#define LONG "loooooooooooooooooooooooooooooooooooooooooooooooooooong"
	#endif
	if (catcierge_make_path("%s/%s/%s/%s/%s/path", LONG, LONG, LONG, LONG, LONG))
	{
		return "Failed to create long path";
	}

	return NULL;
}

int TEST_catcierge_util(int argc, char *argv[])
{
	int ret = 0;
	char *e = NULL;

	CATCIERGE_RUN_TEST((e = run_test_catcierge_get_abs_path()),
		"catcierge_get_abs_path",
		"catcierge_get_abs_path", &ret);

	CATCIERGE_RUN_TEST((e = run_make_path_tests()),
		"catcierge_make_path tests",
		"catcierge_make_path tests", &ret);

	if (ret)
	{
		catcierge_test_FAILURE("One or more tests failed!");
	}

	return ret;
}
