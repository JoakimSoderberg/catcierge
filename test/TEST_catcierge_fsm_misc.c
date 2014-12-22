#include <stdlib.h>
#include <stdio.h>
#include "minunit.h"
#include "catcierge_test_helpers.h"
#include "catcierge_test_config.h"
#include "catcierge_test_helpers.h"
#include "catcierge_args.h"
#include "catcierge_types.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include "catcierge_fsm.h"
#include "catcierge_test_common.h"

static char *run_tests()
{
	size_t i;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;

	catcierge_grabber_init(&grb);
	catcierge_setup_camera(&grb);
	#ifdef WITH_ZMQ
	catcierge_zmq_init(&grb);
	#endif
	catcierge_timer_set(&grb.frame_timer, 1.0);
	catcierge_timer_start(&grb.frame_timer);

	// Load images so we can test the cleanup.
	for (i = 0; i < MATCH_MAX_COUNT; i++)
	{
		grb.match_group.matches[i].img = catcierge_get_frame(&grb);
	}

	catcierge_test_STATUS("Test spinner with waiting state");
	catcierge_set_state(&grb, catcierge_state_waiting);

	sleep(1);
	catcierge_print_spinner(&grb);

	catcierge_test_STATUS("Test spinner with locked out state");
	catcierge_set_state(&grb, catcierge_state_lockout);
	catcierge_timer_start(&grb.frame_timer);
	sleep(1);
	catcierge_print_spinner(&grb);

	catcierge_test_STATUS("Test spinner with keep open state");
	catcierge_set_state(&grb, catcierge_state_keepopen);
	catcierge_timer_start(&grb.frame_timer);
	sleep(1);
	catcierge_print_spinner(&grb);

	catcierge_timer_start(&grb.frame_timer);
	catcierge_timer_set(&grb.rematch_timer, 1.0);
	catcierge_timer_start(&grb.rematch_timer);
	sleep(1);
	catcierge_print_spinner(&grb);

	args->do_lockout_cmd = "";
	args->do_unlock_cmd = "";
	catcierge_do_lockout(&grb);
	catcierge_do_unlock(&grb);
	args->new_execute = 1;
	catcierge_do_lockout(&grb);
	catcierge_do_unlock(&grb);
	args->lockout_dummy = 1;
	catcierge_do_lockout(&grb);

	args->noanim = 1;
	catcierge_timer_start(&grb.frame_timer);
	sleep(1);
	catcierge_print_spinner(&grb);

	catcierge_destroy_camera(&grb);
	#ifdef WITH_ZMQ
	catcierge_zmq_destroy(&grb);
	#endif
	catcierge_grabber_destroy(&grb);

	return NULL;
}

int TEST_catcierge_fsm_misc(int argc, char **argv)
{
	int ret = 0;
	char *e = NULL;

	catcierge_test_HEADLINE("TEST_catcierge_fsm_misc");

	CATCIERGE_RUN_TEST((e = run_tests()),
		"Misc tests",
		"Misc tests", &ret);
	
	return ret;
}
