#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "catcierge_fsm.h"
#include "minunit.h"
#include "catcierge_test_config.h"
#include "catcierge_test_helpers.h"
#include "catcierge_args.h"
#include "catcierge_types.h"
#include "catcierge_test_common.h"
#include "catcierge_output.h"

typedef struct output_test_s
{
	const char *input;
	const char *expected;
} output_test_t;

static char *run_generate_tests()
{
	catcierge_output_t o;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;
	size_t i;
	char *str = NULL;

	catcierge_grabber_init(&grb);
	{
		output_test_t tests[] =
		{
			{ "%match_success%", "33" },
			{ "a b c %match_success%\nd e f", "a b c 33\nd e f" },
			{ "%match_success%", "33" },
			{ "abc%%def", "abc%def" },
			{ "%match0_path%", "/some/path/omg1" },
			{ "%match1_path%", "/some/path/omg2" },
			{ "%match1_success%", "0" },
			{ "aaa %match3_direction% bbb", "aaa in bbb" },
			{ "%match3_result%", "0.800000" },
			{ "%state%", "Waiting" }
		};

		strcpy(grb.matches[0].path, "/some/path/omg1");
		strcpy(grb.matches[1].path, "/some/path/omg2");
		grb.matches[0].success = 4;
		grb.matches[3].direction = MATCH_DIR_IN;
		grb.matches[3].result = 0.8;
		grb.match_success = 33;
		catcierge_set_state(&grb, catcierge_state_waiting);
		
		for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
		{
			if (catcierge_output_init(&o))
			{
				return "Failed to init output context";
			}
			
			str = catcierge_output_generate(&o, &grb, tests[i].input);
			
			catcierge_test_STATUS("\"%s\" -> \"%s\" Expecting: \"%s\"", tests[i].input, str, tests[i].expected);
			mu_assert("Invalid template result", str && !strcmp(tests[i].expected, str));
			catcierge_test_SUCCESS("\"%s\" == \"%s\"\n", str, tests[i].expected);
			free(str);

			catcierge_output_destroy(&o);
		}
	}
	catcierge_grabber_destroy(&grb);

	return NULL;
}

char *run_add_and_generate_tests()
{
	catcierge_output_t o;
	catcierge_grb_t grb;

	catcierge_grabber_init(&grb);
	{
		if (catcierge_output_init(&o))
			return "Failed to init output context";

		catcierge_test_STATUS("Add one template");
		{
			if (catcierge_output_add_template(&o, "%match_success%", "output/path"))
				return "Failed to add template";
			mu_assert("Expected template count 1", o.template_count == 1);

			if (catcierge_output_generate_templates(&o, &grb))
				return "Failed to generate templates";
		}

		catcierge_test_STATUS("Add another template");
		{
			if (catcierge_output_add_template(&o, "hej %match_success%", "output/path"))
				return "Failed to add template";
			mu_assert("Expected template count 2", o.template_count == 2);

			if (catcierge_output_generate_templates(&o, &grb))
				return "Failed to generate templates";
		}

		catcierge_output_destroy(&o);
	}
	catcierge_grabber_destroy(&grb);

	return NULL;
}

int TEST_catcierge_output(int argc, char **argv)
{
	char *e = NULL;
	int ret = 0;

	catcierge_test_HEADLINE("TEST_catcierge_output");

	CATCIERGE_RUN_TEST((e = run_generate_tests()),
		"Run generate tests.",
		"Generate tests", &ret);

	CATCIERGE_RUN_TEST((e = run_add_and_generate_tests()),
		"Run add and generate tests.",
		"Add and generate tests", &ret);

	return ret;
}
