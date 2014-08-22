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

static char *run_validate_tests()
{
	catcierge_output_t o;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;
	size_t i;
	int ret;

	catcierge_grabber_init(&grb);
	{
		char *tests[] =
		{
			"unknown variable: %unknown%",
			"Not terminated %match_success% after a valid %match1_path"
		};

		for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
		{
			catcierge_test_STATUS("Test invalid template: \"%s\"", tests[i]);
			ret = catcierge_output_validate(&o, &grb, tests[i]);
			catcierge_test_STATUS("Ret %d", ret);
			mu_assert("Failed to invalidate template", ret == 0);
			catcierge_test_SUCCESS("Success!");
		}

	}
	catcierge_grabber_destroy(&grb);

	return NULL;
}

static char *run_generate_tests()
{
	catcierge_output_t o;
	catcierge_grb_t grb;
	catcierge_args_t *args = &grb.args;
	size_t i;
	char *str = NULL;

	catcierge_grabber_init(&grb);
	{
		#define _XSTR(s) _STR(s)
		#define _STR(s) #s
		output_test_t tests[] =
		{
			{ "%match_success%", "33" },
			{ "a b c %match_success%\nd e f", "a b c 33\nd e f" },
			{ "%match_success%", "33" },
			{ "abc%%def", "abc%def" },
			{ "%match1_path%", "/some/path/omg1" },
			{ "%match2_path%", "/some/path/omg2" },
			{ "%match2_success%", "0" },
			{ "%match2_description%", "prey found" },
			{ "aaa %match3_direction% bbb", "aaa in bbb" },
			{ "%match3_result%", "0.800000" },
			{ "%state%", "Waiting" },
			{ "%prev_state% %matchcur_path%", "Initial /some/path/omg3" },
			{ "%match4_path%", "" }, // Match count is only 3, so this should be empty.
			{ "%match_count%", "3" },
			{ "%match1_step1_path%", "some/step/path" },
			{ "%match1_step2_name%", "the_step_name" },
			{ "%match2_step7_desc%", "Step description" },
			{ "%match2_step7_active%", "0" },
			{ "%git_hash%", CATCIERGE_GIT_HASH },
			{ "%git_hash_short%", CATCIERGE_GIT_HASH_SHORT },
			{ "%git_tainted%", _XSTR(CATCIERGE_GIT_TAINTED) },
			{ "%version%", CATCIERGE_VERSION_STR },
			{ "%matcher%", "haar" },
			{ "%matchtime%", "13" },
			{ "%lockout_method%", "2" },
			{ "%lockout_time%", "23" },
			{ "%lockout_error%", "20" },
			{ "%lockout_error_delay%", "2.44" },
			{ "%ok_matches_needed%", "4" }
		};

		#undef _XSTR
		#undef _STR

		output_test_t fail_tests[] =
		{
			{ "%match5_path%", NULL },
			{ "%matchX_path%", NULL },
			{ "%match1_step600_path%", NULL}
		};

		grb.args.matcher = "haar";
		grb.args.match_time = 13;
		grb.args.lockout_method = 2;
		grb.args.lockout_time = 23;
		grb.args.ok_matches_needed = 4;
		grb.args.max_consecutive_lockout_count = 20;
		grb.args.consecutive_lockout_delay = 2.44;
		strcpy(grb.matches[0].result.steps[0].path, "some/step/path");
		grb.matches[0].result.steps[1].name = "the_step_name";
		grb.matches[1].result.steps[6].description = "Step description";
		strcpy(grb.matches[1].result.description, "prey found");
		strcpy(grb.matches[0].path, "/some/path/omg1");
		strcpy(grb.matches[1].path, "/some/path/omg2");
		strcpy(grb.matches[2].path, "/some/path/omg3");
		grb.matches[0].result.success = 4;
		grb.matches[2].result.direction = MATCH_DIR_IN;
		grb.matches[2].result.result = 0.8;
		grb.match_success = 33;
		grb.match_count = 3;
		catcierge_set_state(&grb, catcierge_state_waiting);

		for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
		{
			if (catcierge_output_init(&o))
			{
				return "Failed to init output context";
			}
			
			str = catcierge_output_generate(&o, &grb, tests[i].input);
			
			catcierge_test_STATUS("\"%s\" -> \"%s\" Expecting: \"%s\"",
				tests[i].input, str, tests[i].expected);
			mu_assert("Invalid template result", str && !strcmp(tests[i].expected, str));
			catcierge_test_SUCCESS("\"%s\" == \"%s\"\n", str, tests[i].expected);
			free(str);

			catcierge_output_destroy(&o);
		}

		for (i = 0; i < sizeof(fail_tests) / sizeof(fail_tests[0]); i++)
		{
			if (catcierge_output_init(&o))
			{
				return "Failed to init output context";
			}

			str = catcierge_output_generate(&o, &grb, fail_tests[i].input);

			catcierge_test_STATUS("\"%s\" -> \"%s\" Expecting: \"%s\"",
				fail_tests[i].input, str, fail_tests[i].expected);
			mu_assert("Invalid template result", str == NULL);
			catcierge_test_SUCCESS("\"%s\" == \"%s\"\n",
				fail_tests[i].input, fail_tests[i].expected);
			free(str);

			catcierge_output_destroy(&o);
		}
	}
	catcierge_grabber_destroy(&grb);

	return NULL;
}

char *run_add_and_generate_tests()
{
	catcierge_grb_t grb;
	catcierge_output_t *o = &grb.output;

	catcierge_grabber_init(&grb);
	{
		grb.args.output_path = "template_tests";

		if (catcierge_output_init(o))
			return "Failed to init output context";

		catcierge_test_STATUS("Add one template");
		{
			if (catcierge_output_add_template(o,
				"%!event all\n"
				"%match_success%", 
				"firstoutputpath"))
			{
				return "Failed to add template";
			}
			mu_assert("Expected template count 1", o->template_count == 1);

			if (catcierge_output_generate_templates(o, &grb, grb.args.output_path, "all"))
				return "Failed to generate templates";
		}

		catcierge_test_STATUS("Add another template");
		{
			if (catcierge_output_add_template(o,
				"%!event all   \n"
				"hej %match_success%",
				"outputpath is here %match_success% %time%"))
			{
				return "Failed to add template";
			}
			mu_assert("Expected template count 2", o->template_count == 2);

			if (catcierge_output_generate_templates(o, &grb, grb.args.output_path, "all"))
				return "Failed to generate templates";
		}

		catcierge_test_STATUS("Add a third template");
		{
			grb.matches[1].time = time(NULL);
			strcpy(grb.matches[1].path, "blafile");
			grb.match_count = 3;

			if (catcierge_output_add_template(o,
				"%!event all\n"
				"Some awesome %match2_path% template. "
				"Advanced time format is here: %time:Week @W @H:@M%\n"
				"And match time, %match2_time:@H:@M%",
				"the path"))
			{
				return "Failed to add template";
			}
			mu_assert("Expected template count 3", o->template_count == 3);

			if (catcierge_output_generate_templates(o, &grb, grb.args.output_path, "all"))
				return "Failed to generate templates";
		}

		catcierge_test_STATUS("Add a named template");
		{
			char buf[1024];
			const char *named_template_path = NULL;
			const char *default_template_path = NULL;
			grb.matches[1].time = time(NULL);
			strcpy(grb.matches[1].path, "thematchpath");
			grb.match_count = 2;

			if (catcierge_output_add_template(o,
				"%!event all\n"
				"Some awesome %match2_path% template. "
				"Advanced time format is here: %time:Week @W @H:@M%\n"
				"And match time, %match2_time:@H:@M%",
				"[arne]the path")) // Here the name is specified with [arne]
			{
				return "Failed to add template";
			}
			mu_assert("Expected template count 4", o->template_count == 4);
			mu_assert("Expected named template", !strcmp(o->templates[3].name, "arne"));

			if (catcierge_output_generate_templates(o, &grb, grb.args.output_path, "all"))
				return "Failed to generate templates";

			// Try getting the template_path for the arne template.
			named_template_path = catcierge_output_translate(&grb, buf, sizeof(buf), "template_path:arne");
			catcierge_test_STATUS("Got named template path for \"arne\": %s", named_template_path);
			mu_assert("Got null named template path", named_template_path != NULL);
			#ifdef _WIN32
			mu_assert("Unexpected named template path", !strcmp("template_tests\\the_path", named_template_path));
			#else
			mu_assert("Unexpected named template path", !strcmp("template_tests/the_path", named_template_path));
			#endif
			// Also try a non-existing name.
			named_template_path = catcierge_output_translate(&grb, buf, sizeof(buf), "template_path:bla");
			catcierge_test_STATUS("Tried to get non-existing template path: %s", named_template_path);
			mu_assert("Got non-null for non-existing named template path", named_template_path == NULL);

			// This should simply get the path for the first template.
			default_template_path = catcierge_output_translate(&grb, buf, sizeof(buf), "template_path");
			catcierge_test_STATUS("Got default template path: %s", default_template_path);
			mu_assert("Got null default template path", default_template_path != NULL);
			#ifdef _WIN32
			mu_assert("Unexpected default template path", !strcmp("template_tests\\firstoutputpath", default_template_path));
			#else
			mu_assert("Unexpected default template path", !strcmp("template_tests/firstoutputpath", default_template_path));
			#endif
		}

		catcierge_output_destroy(o);
	}
	catcierge_grabber_destroy(&grb);

	return NULL;
}

static char *run_load_templates_test()
{
	catcierge_output_t o;
	catcierge_grb_t grb;

	catcierge_grabber_init(&grb);
	{
		size_t i;
		FILE *f;
		catcierge_output_template_t templs[] =
		{
			{ 
				"%! event all\n"
				"Arne weise %time% %match1_path% julafton", // Contents.
				"a_%time%", // Path.
			},
			{
				"%! event *\n"
				"the template contents\nis %time:@c%",
				"b_%time%"
			},
			{
				"%! event all\n",
				"c_%time%"
			},
			{ 
				"%! event arne, weise\n"
				"Hello world\n",
				"abc_%time%"
			},
			{ 
				"%! event    arne   ,    weise   \n\n\n"
				"Hello world\n",
				"abc_%time%"
			},
			{
				"%!bla\n"
				"Some cool template",
				"123_abc_%time%"
			},
			{
				"%!event tut   \n"
				"%!    nop    \n"
				"Some cool template",
				"two_events_%time%"
			},
			{
				"%!    nop    \n"
				"%!event tut   \n"
				"Some cool template",
				"two_events_rev_%time%"
			}
		};
		#define TEST_TEMPLATE_COUNT sizeof(templs) / sizeof(templs[0])
		char *inputs[TEST_TEMPLATE_COUNT];
		size_t input_count = 0;

		// Create temporary files to use as test templates.
		for (i = 0; i < TEST_TEMPLATE_COUNT; i++)
		{
			inputs[i] = templs[i].target_path;
			input_count++;

			f = fopen(templs[i].target_path, "w");
			mu_assert("Failed to open target path", f);

			fwrite(templs[i].tmpl, 1, strlen(templs[i].tmpl), f);
			fclose(f);
		}

		if (catcierge_output_init(&o))
			return "Failed to init output context";

		// Try loading the first 3 test templates without settings.
		if (catcierge_output_load_templates(&o, inputs, 3))
		{
			return "Failed to load templates";
		}

		mu_assert("Expected template count to be 3", o.template_count == 3);
		catcierge_test_SUCCESS("Successfully loaded 3 templates without settings");

		// Test loading a template with settings.
		if (catcierge_output_load_template(&o, inputs[3]))
		{
			return "Failed to load template with settings";
		}

		mu_assert("Expected template count to be 4", o.template_count == 4);
		mu_assert("Expected 2 event filters",
			o.templates[3].settings.event_filter_count == 2);
		catcierge_test_SUCCESS("Successfully loaded template with settings");

		// Same template but with some extra whitespace.
		if (catcierge_output_load_template(&o, inputs[4]))
		{
			return "Failed to load template with settings + whitespace";
		}

		mu_assert("Expected template count to be 5", o.template_count == 5);
		mu_assert("Expected 2 event filters",
			o.templates[4].settings.event_filter_count == 2);
		catcierge_test_SUCCESS("Successfully loaded template with settings + whitespace");

		// Invalid setting.
		if (!catcierge_output_load_template(&o, inputs[5]))
		{
			return "Successfully loaded template with invalid setting";
		}

		mu_assert("Expected template count to be 5", o.template_count == 5);
		catcierge_test_SUCCESS("Failed to load template with invalid setting");

		// Two settings.
		if (catcierge_output_load_template(&o, inputs[6]))
		{
			return "Failed to load event + nop settings";
		}

		mu_assert("Expected template count to be 6", o.template_count == 6);
		catcierge_test_SUCCESS("Loaded two settings, event+nop");

		// Two settings reversed.
		if (catcierge_output_load_template(&o, inputs[7]))
		{
			return "Failed to load nop + event settings";
		}

		mu_assert("Expected template count to be 7", o.template_count == 7);
		mu_assert("Expected 1 event filter count", o.templates[6].settings.event_filter_count == 1);
		catcierge_test_STATUS("Event filter: \"%s\"\n", o.templates[6].settings.event_filter[0]);
		mu_assert("Expected event list to contain \"tut\"", !strcmp(o.templates[6].settings.event_filter[0], "tut"));
		catcierge_test_SUCCESS("Loaded two settings, nop+event");

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

	catcierge_output_print_usage();

	CATCIERGE_RUN_TEST((e = run_generate_tests()),
		"Run generate tests.",
		"Generate tests", &ret);

	CATCIERGE_RUN_TEST((e = run_add_and_generate_tests()),
		"Run add and generate tests.",
		"Add and generate tests", &ret);

	CATCIERGE_RUN_TEST((e = run_validate_tests()),
		"Run validation tests.",
		"Validation tests", &ret);

	CATCIERGE_RUN_TEST((e = run_load_templates_test()),
		"Run load templates tests.",
		"Load templates tests", &ret);

	// TODO: Add a test for template paths in other directory.

	if (ret)
	{
		catcierge_test_FAILURE("Failed some tests!\n");
	}

	return ret;
}
