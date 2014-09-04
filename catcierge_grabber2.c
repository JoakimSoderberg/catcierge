
#include <signal.h>
#include "catcierge_config.h"
#include "catcierge_args.h"
#include "catcierge_log.h"
#include "catcierge_fsm.h"
#include "catcierge_output.h"

catcierge_grb_t grb;

static void sig_handler(int signo)
{
	switch (signo)
	{
		case SIGINT:
		{
			static int sigint_count = 0;
			CATLOG("Received SIGINT, stopping...\n");

			// Force a quit if we're not in the loop
			// or the SIGINT is a repeat.
			if (!grb.running || (sigint_count > 0))
			{
				catcierge_do_unlock(&grb);
				exit(0);
			}

			// Simply kill the loop and let the program do normal cleanup.
			grb.running = 0;

			sigint_count++;

			break;
		}
		#ifndef _WIN32
		case SIGUSR1:
		{
			CATLOG("Received SIGUSR1, forcing unlock...\n");
			catcierge_do_unlock(&grb);
			catcierge_set_state(&grb, catcierge_state_waiting);
			break;
		}
		case SIGUSR2:
		{
			CATLOG("Received SIGUSR2, forcing lockout...\n");
			catcierge_state_transition_lockout(&grb);
			break;
		}
		#endif // _WIN32
	}
}

void setup_sig_handlers()
{
	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
		CATERR("Failed to set SIGINT handler\n");
	}

	#ifndef _WIN32
	if (signal(SIGUSR1, sig_handler) == SIG_ERR)
	{
		CATERR("Failed to set SIGUSR1 handler (used to force unlock)\n");
	}

	if (signal(SIGUSR2, sig_handler) == SIG_ERR)
	{
		CATERR("Failed to set SIGUSR2 handler (used to force lockout)\n");
	}
	#endif // _WIN32
}

int main(int argc, char **argv)
{
	catcierge_args_t *args;
	args = &grb.args;

	fprintf(stderr, "\nCatcierge Grabber v" CATCIERGE_VERSION_STR " (" CATCIERGE_GIT_HASH_SHORT "");

	if (CATCIERGE_GIT_TAINTED)
	{
		fprintf(stderr, "-tainted");
	}

	fprintf(stderr, ")\n(C) Joakim Soderberg 2013-2014\n\n");

	if (catcierge_grabber_init(&grb))
	{
		fprintf(stderr, "Failed to init\n");
		return -1;
	}

	// Get program settings.
	{
		if (catcierge_parse_config(args, argc, argv))
		{
			catcierge_show_usage(args, argv[0]);
			return -1;
		}

		if (catcierge_parse_cmdargs(args, argc, argv, NULL, NULL))
		{
			catcierge_show_usage(args, argv[0]);
			return -1;
		}

		if (args->nocolor)
		{
			catcierge_nocolor = 1;
		}

		// TODO: Add verify function for settings. Make sure we have everything we need...

		catcierge_print_settings(args);

		if (args->output_path)
		{
			CATLOG("Creating output directory: \"%s\"\n", args->output_path);
			catcierge_make_path(args->output_path);
		}
		else
		{
			args->output_path = ".";
		}
	}

	setup_sig_handlers();

	if (args->log_path)
	{
		if (!(grb.log_file = fopen(args->log_path, "a+")))
		{
			CATERR("Failed to open log file \"%s\"\n", args->log_path);
		}
	}

	#ifdef RPI
	if (catcierge_setup_gpio(&grb))
	{
		CATERR("Failed to setup GPIO pins\n");
		return -1;
	}

	CATLOG("Initialized GPIO pins\n");
	#endif // RPI

	assert((args->matcher_type == MATCHER_TEMPLATE)
		|| (args->matcher_type == MATCHER_HAAR));

	if (args->matcher_type == MATCHER_TEMPLATE)
	{
		if (catcierge_template_matcher_init(&grb.matcher, &args->templ))
		{
			CATERR("Failed to init template matcher!\n");
			return -1;
		}
	}
	else
	{
		if (catcierge_haar_matcher_init(&grb.haar, &args->haar))
		{
			CATERR("Failed to init haar matcher!\n");
			return -1;
		}
	}

	CATLOG("Initialized catcierge image recognition\n");

	if (catcierge_output_init(&grb.output))
	{
		CATERR("Failed to init output template system\n");
		exit(-1);
	}

	if (catcierge_output_load_templates(&grb.output,
			args->inputs, args->input_count))
	{
		CATERR("Failed to load output templates\n");
		exit(-1);
	}

	CATLOG("Initialized output templates\n");

	catcierge_setup_camera(&grb);
	CATLOG("Starting detection!\n");
	grb.running = 1;
	catcierge_set_state(&grb, catcierge_state_waiting);
	catcierge_timer_set(&grb.frame_timer, 1.0);

	// Run the program state machine.
	do
	{
		if (!catcierge_timer_isactive(&grb.frame_timer))
		{
			catcierge_timer_start(&grb.frame_timer);
		}

		// Always feed the RFID readers.
		#ifdef WITH_RFID
		if ((args->rfid_inner_path || args->rfid_outer_path) 
			&& catcierge_rfid_ctx_service(&grb.rfid_ctx))
		{
			CATERRFPS("Failed to service RFID readers\n");
		}
		#endif // WITH_RFID

		grb.img = catcierge_get_frame(&grb);

		catcierge_run_state(&grb);
		catcierge_print_spinner(&grb);
	} while (grb.running);

	if (args->matcher_type == MATCHER_TEMPLATE)
	{
		catcierge_template_matcher_destroy(&grb.matcher);
	}
	else
	{
		catcierge_haar_matcher_destroy(&grb.haar);
	}

	catcierge_output_destroy(&grb.output);
	catcierge_destroy_camera(&grb);
	catcierge_grabber_destroy(&grb);

	if (grb.log_file)
	{
		fclose(grb.log_file);
	}

	return 0;
}
