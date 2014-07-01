#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_config.h"
#include "catcierge_fsm.h"
#include "minunit.h"
#include "catcierge_test_config.h"
#include "catcierge_test_helpers.h"
#include "catcierge_util.h"
#ifdef CATCIERGE_HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef CATCIERGE_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef CATCIERGE_HAVE_PWD_H
#include <pwd.h>
#endif

char *run_droproot_test()
{
	int ret;
	char *user = NULL;
	#ifndef _WIN32
	struct passwd *p = getpwuid(getuid());
	mu_assert("Failed to get current username", p);
	catcierge_test_STATUS("User name: %s\n", p->pw_name);
	user = p->pw_name;
	#endif // !_WIN32

	ret = catcierge_drop_root_privileges(user);
	mu_assert("Failed to drop root privileges", ret == 0);

	return NULL;
}

int TEST_catcierge_fsm_droproot(int argc, char **argv)
{
	char *e = NULL;
	int ret = 0;

	// Disable for now, does not work on OSX.
	#if 1
	CATCIERGE_RUN_TEST((e = run_droproot_test()),
		"Run drop root privileges test",
		"Drop root privileges test", &ret);
	#endif

	return ret;
}


