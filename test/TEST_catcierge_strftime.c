#include <stdlib.h>
#include <stdio.h>
#include "catcierge_util.h"
#include "catcierge_strftime.h"
#include "minunit.h"
#include "catcierge_test_helpers.h"
#ifdef _WIN32
#include "win32/gettimeofday.h"
#else
#include <time.h>
#include <sys/time.h>
#endif

static char *run_tests()
{
	int ret;
	char buf[1024];
	struct timeval tv;
	struct tm tm;
	time_t t;
	int milliseconds;

	t = 1409661310;
	catcierge_test_STATUS("t = %ld\n", (long int)t);

	// Get UTC time instead of localtime so the unit test is reliable in all timezones.
	gmtime_r(&t, &tm);
	
	tv.tv_sec = 1409661310;
	tv.tv_usec = 451893;
	milliseconds = tv.tv_usec;

	catcierge_test_STATUS("tv.sec = %ld\ntv.u_sec = %ld\n",
		(long int)tv.tv_sec, (long int)tv.tv_usec);

	// Simple test, only milliseconds (not part of normal strftime).
	ret = catcierge_strftime(buf, sizeof(buf), "%f", &tm, &tv);
	mu_assert("Formatting failed with known input", ret >= 0);

	catcierge_test_STATUS("Got: \"%s\" Expected: \"%d\"", buf, milliseconds);
	mu_assert("Unexpected millisecond string", atoi(buf) == milliseconds);

	// Combination test, normal date + milliseconds.
	ret = catcierge_strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S.%f", &tm, &tv);
	mu_assert("Formatting failed with known input", ret >= 0);

	catcierge_test_STATUS("Got: \"%s\" Expected: \"%s\"", buf, "2014-09-02 12:35:10.451893");
	mu_assert("Unexpected millisecond string", !strcmp(buf, "2014-09-02 12:35:10.451893"));

	// Trigger a reallocation because the initial format buffer is too small.
	tv.tv_usec = 1215752191;
	ret = catcierge_strftime(buf, sizeof(buf), "%f", &tm, &tv);
	mu_assert("Formatting failed with known input", ret >= 0);

	catcierge_test_STATUS("Got: \"%s\" Expected: \"%d\"", buf, tv.tv_usec);
	mu_assert("Unexpected millisecond string", atoi(buf) == tv.tv_usec);

	return NULL;
}

int TEST_catcierge_strftime(int argc, char **argv)
{
	int ret = 0;
	char *e = NULL;

	CATCIERGE_RUN_TEST((e = run_tests()),
		"TEST_catcierge_strftime",
		"", &ret);
	
	return ret;
}
