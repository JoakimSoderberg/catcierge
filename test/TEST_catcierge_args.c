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
	mu_assert("Failed to parse args", !catcierge_parse_cmdargs(&args, TEST1_COUNT, test1, NULL, NULL));

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
	mu_assert("Failed to parse args", !catcierge_parse_cmdargs(&args, TEST1_COUNT, test1, NULL, NULL));
	
	#ifdef WITH_RFID
	mu_assert("Expected RFID allowed to be 3", args.rfid_allowed_count == 3);
	#endif // WITH_RFID

	catcierge_show_usage(&args, "catcierge");
	catcierge_print_settings(&args);
	
	catcierge_args_destroy(&args);

	return NULL;
}

static void free_values(char **values, size_t value_count)
{
	size_t i;

	for (i = 0; i < value_count; i++)
	{
		if (values[i])
			free(values[i]);
		values[i] = NULL;
	}
}

char *run_parse_args_tests()
{
	catcierge_args_t args;
	char *key = NULL;
	char *values[4096];
	char *pch = NULL;
	int ret = 0;
	size_t value_count = 0;

	memset(values, 0, sizeof(values));

	catcierge_args_init(&args);

	#define xstr(a) str(a)
	#define str(a) #a

	#define GET_KEY_VALUES(str) \
		do \
		{ \
			char *s = strdup(str); \
			value_count = 0; \
			pch = strtok(s, " "); \
			key = strdup(pch); \
			while (pch != NULL) \
			{ \
				pch = strtok(NULL, " "); \
				if (pch) values[value_count++] = strdup(pch); \
			} \
			free(s); \
		} while(0)

	#define PARSE_SETTING(str, expect, assert_stmt) \
		GET_KEY_VALUES(str); \
		ret = catcierge_parse_setting(&args, key, values, value_count); \
		catcierge_test_STATUS(str ". Assert: %d <= " #assert_stmt, assert_stmt); \
		mu_assert(expect, assert_stmt); \
		catcierge_test_SUCCESS("  Parsed: \"%s\"", str); \
		free(key); \
		key = NULL; \
		free_values(values, value_count)

	// Helper macro for a single setting that can either be set using:
	// "--hello" or "--hello 2".
	// Example: PARSE_SINGLE_SETTING("hello", args.hello, 2, "Expected")
	#define PARSE_SINGLE_SETTING(setting_str, setting, val) \
		PARSE_SETTING(setting_str " " str(val), "Expected valid parse.", \
			(ret == 0) && (setting == val)); \
		PARSE_SETTING(setting_str, "Expected valid parse.", \
			(ret == 0) && (setting == val))



	PARSE_SETTING("matcher", "Expected failure for missing input value", (ret == -1));
	PARSE_SETTING("matcher bla", "Expected failure for invalid input value", (ret == -1));
	PARSE_SETTING("matcher haar", "Expected haar matcher",
		(ret == 0) &&
		!strcmp(args.matcher, "haar") &&
		(args.matcher_type == MATCHER_HAAR));
	PARSE_SETTING("matcher template", "Expected template matcher",
		(ret == 0) &&
		!strcmp(args.matcher, "template") &&
		(args.matcher_type == MATCHER_TEMPLATE));

	PARSE_SINGLE_SETTING("show", args.show, 1);

	PARSE_SETTING("ok_matches_needed 2", "Expected valid parse",
		(ret == 0) && (args.ok_matches_needed == 2));
	PARSE_SETTING("ok_matches_needed 20", "Expected too big value",
		(ret == -1));
	PARSE_SETTING("ok_matches_needed", "Expected value for input",
		(ret == -1));
	PARSE_SINGLE_SETTING("no_final_decision", args.no_final_decision, 1);

	PARSE_SETTING("lockout_method 1", "Expected valid parse OBSTRUCT_OR_TIMER_1",
		(ret == 0) && (args.lockout_method == OBSTRUCT_OR_TIMER_1));
	PARSE_SETTING("lockout_method 2", "Expected valid parse OBSTRUCT_THEN_TIMER_2",
		(ret == 0) && (args.lockout_method == OBSTRUCT_THEN_TIMER_2));
	PARSE_SETTING("lockout_method 3", "Expected valid parse TIMER_ONLY_3",
		(ret == 0) && (args.lockout_method == TIMER_ONLY_3));
	PARSE_SETTING("lockout_method 0", "Expected invalid parse for out of bounds",
		(ret == -1));
	PARSE_SETTING("lockout_method 4", "Expected invalid parse for out of bounds",
		(ret == -1));
	PARSE_SETTING("lockout_method", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SINGLE_SETTING("save", args.saveimg, 1);

	PARSE_SINGLE_SETTING("save_obstruct", args.save_obstruct_img, 1);

	PARSE_SINGLE_SETTING("new_execute", args.new_execute, 1);

	PARSE_SINGLE_SETTING("highlight", args.highlight_match, 1);

	#ifdef WITH_ZMQ
	PARSE_SINGLE_SETTING("zmq", args.zmq, 1);

	PARSE_SETTING("zmq_port 1234", "Expected a valid parse", 
		(ret == 0) && (args.zmq_port == 1234));

	PARSE_SETTING("zmq_iface eth1", "Expected a valid parse",
		(ret == 0) && !strcmp(args.zmq_iface, "eth1"));

	#endif

	PARSE_SETTING("lockout 5", "Expected a valid parse",
		(ret == 0) && (args.lockout_time == 5));
	PARSE_SETTING("lockout", "Expected a valid parse for missing value",
		(ret == 0));

	PARSE_SETTING("lockout_error_delay 5", "Expected a valid parse",
		(ret == 0) && (args.consecutive_lockout_delay == 5));
	PARSE_SETTING("lockout_error_delay", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SETTING("lockout_error 20", "Expected a valid parse",
		(ret == 0) && (args.max_consecutive_lockout_count == 20));
	PARSE_SETTING("lockout_error", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SINGLE_SETTING("lockout_dummy", args.lockout_dummy, 1);

	PARSE_SINGLE_SETTING("matchtime", args.match_time, DEFAULT_MATCH_WAIT);

	PARSE_SETTING("output /some/output/path", "Expected a valid parse",
		(ret == 0) && !strcmp(args.output_path, "/some/output/path"));
	PARSE_SETTING("output", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SETTING("output_path /some/output/path", "Expected a valid parse",
		(ret == 0) && !strcmp(args.output_path, "/some/output/path"));
	PARSE_SETTING("output_path", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SETTING("match_output_path /some/output/path", "Expected a valid parse",
		(ret == 0) && !strcmp(args.match_output_path, "/some/output/path"));
	PARSE_SETTING("match_output_path", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SETTING("steps_output_path /some/output/path", "Expected a valid parse",
		(ret == 0) && !strcmp(args.steps_output_path, "/some/output/path"));
	PARSE_SETTING("steps_output_path", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SETTING("obstruct_output_path /some/output/path", "Expected a valid parse",
		(ret == 0) && !strcmp(args.obstruct_output_path, "/some/output/path"));
	PARSE_SETTING("obstruct_output_path", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SETTING("template_output_path /some/output/path", "Expected a valid parse",
		(ret == 0) && !strcmp(args.template_output_path, "/some/output/path"));
	PARSE_SETTING("template_output_path", "Expected invalid parse for missing value",
		(ret == -1));

	{
		size_t i;

		PARSE_SETTING("input /path/to/templ%time%.tmpl", "Expected a valid parse",
			(ret == 0) && !strcmp(args.inputs[0], "/path/to/templ%time%.tmpl"));
		PARSE_SETTING("input", "Expected invalid parse for missing value",
			(ret == -1));

		for (i = 1; i < MAX_INPUT_TEMPLATES; i++)
		{
			PARSE_SETTING("input /path/to/last", "Expected a valid parse",
				(ret == 0) && !strcmp(args.inputs[i], "/path/to/last"));
		}

		PARSE_SETTING("input /path/to/templ%time%.tmpl",
			"Expected failure when exceeding MAX_INPUT_TEMPLATES",
			(ret == -1));
	}

	#ifdef WITH_RFID
	PARSE_SETTING("rfid_in /some/rfid/path", "Expected a valid parse",
		(ret == 0) && !strcmp(args.rfid_inner_path, "/some/rfid/path"));
	PARSE_SETTING("rfid_in", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SETTING("rfid_out /some/rfid/path", "Expected a valid parse",
		(ret == 0) && !strcmp(args.rfid_outer_path, "/some/rfid/path"));
	PARSE_SETTING("rfid_out", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SETTING("rfid_allowed 1,2,3", "Expected a valid parse",
		(ret == 0) &&
		!strcmp(args.rfid_allowed[0], "1") &&
		!strcmp(args.rfid_allowed[1], "2") &&
		!strcmp(args.rfid_allowed[2], "3") &&
		(args.rfid_allowed_count == 3));
	PARSE_SETTING("rfid_allowed", "Expected invalid parse for missing value",
		(ret == -1));
	catcierge_free_rfid_allowed_list(&args);
	
	PARSE_SETTING("rfid_time 3.5", "Expected a valid parse",
		(ret == 0) && (args.rfid_lock_time == 3.5));
	PARSE_SETTING("rfid_time", "Expected invalid parse for missing value",
		(ret == -1));

	PARSE_SINGLE_SETTING("rfid_lock", args.lock_on_invalid_rfid, 1);
	#else
	catcierge_test_SKIPPED("Skipping RFID args (not compiled)");
	#endif // WITH_RFID

	PARSE_SETTING("log /log/path", "Expected a valid parse",
		(ret == 0) && !strcmp(args.log_path, "/log/path"));
	PARSE_SETTING("log", "Expected invalid parse for missing value",
		(ret == -1));

	{
		#define PARSE_CMD_SETTING(cmd) \
			PARSE_SETTING(#cmd " awesome_cmd.sh", "Expected a valid parse", \
				(ret == 0) && !strcmp(args.cmd, "awesome_cmd.sh")); \
			PARSE_SETTING(#cmd, "Expected invalid parse for missing value", \
				(ret == -1)) \

		PARSE_CMD_SETTING(match_cmd);
		PARSE_CMD_SETTING(save_img_cmd);
		PARSE_CMD_SETTING(match_group_done_cmd);
		PARSE_CMD_SETTING(frame_obstructed_cmd);
		PARSE_CMD_SETTING(match_done_cmd);
		PARSE_CMD_SETTING(do_lockout_cmd);
		PARSE_CMD_SETTING(do_unlock_cmd);
		#ifdef WITH_RFID
		PARSE_CMD_SETTING(rfid_detect_cmd);
		PARSE_CMD_SETTING(rfid_match_cmd);
		#endif // WITH_RFID
	}

	PARSE_SINGLE_SETTING("nocolor", args.nocolor, 1);

	PARSE_SINGLE_SETTING("noanim", args.noanim, 1);

	PARSE_SINGLE_SETTING("save_steps", args.noanim, 1);

	PARSE_SETTING("chuid userid", "Expected a valid parse",
		(ret == 0) && !strcmp(args.chuid, "userid"));
	PARSE_SETTING("chuid", "Expected invalid parse for missing value",
		(ret == -1));

	// Template matcher settings.
	{
		PARSE_SINGLE_SETTING("match_flipped", args.templ.match_flipped, 1);

		PARSE_SETTING("snout /the/snout/path/snout.png", "Expected parse success",
			(ret == 0) &&
			!strcmp(args.templ.snout_paths[0], "/the/snout/path/snout.png") &&
			args.templ.snout_count == 1);
		PARSE_SETTING("snout", "Expected failure for missing value.", (ret == -1));

		PARSE_SINGLE_SETTING("threshold", args.templ.match_threshold, DEFAULT_MATCH_THRESH);
	}

	// Haar matcher settings.
	{
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

char *run_parse_config_tests()
{
	int ret;
	FILE *f;
	catcierge_args_t args;
	#define TEST_CONFIG_PATH "______test.cfg"
	char test_cfg[] = "matcher=template\n";
	char *argv[] = { "program", "--config", TEST_CONFIG_PATH };
	int argc = sizeof(argv) / sizeof(argv[0]);

	// Create a test config.
	f = fopen(TEST_CONFIG_PATH, "w");
	mu_assert("Failed to open test config", f);

	fwrite(test_cfg, 1, sizeof(test_cfg), f);
	fclose(f);

	catcierge_args_init(&args);

	ret = catcierge_parse_config(&args, argc, argv);
	mu_assert("Expected matcher = template after parsing config",
		!strcmp(args.matcher, "template"));

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

	CATCIERGE_RUN_TEST((e = run_parse_args_tests()),
		"Run parse all args test",
		"Parsed all args test", &ret);

	for (i = 0; i < FUNC_COUNT; i++)
	{
		if ((e = funcs[i]())) { catcierge_test_FAILURE("%s", e); ret = -1; }
		else catcierge_test_SUCCESS("");
	}

	CATCIERGE_RUN_TEST((e = run_parse_config_tests()),
		"Run parse config test",
		"Parse config test", &ret);

	if (ret)
	{
		catcierge_test_FAILURE("One or more tests failed!");
	}

	return ret;
}
