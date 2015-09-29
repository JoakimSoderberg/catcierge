//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2015
//
//    Catcierge is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//    Catcierge is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Catcierge.  If not, see <http://www.gnu.org/licenses/>.
//
#include <signal.h>
#include "catcierge_config.h"
#include "catcierge_args.h"
#include "catcierge_log.h"
#include "catcierge_fsm.h"
#include "catcierge_output.h"
#include "catcierge_matcher.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#ifndef _WIN32
#include <fcntl.h>
#endif // _WIN32

#ifdef WITH_ZMQ
#include <czmq.h>
#endif

#include <opencv2/core/version.hpp>

catcierge_grb_t grb;

#ifndef _WIN32
int pid_fd;
#define PID_PATH "/var/run/catcierge.pid"

int create_pid_file(const char *prog_name, const char *pid_path, int flags)
{
	int fd;
	char buf[128];
	struct flock fl;

	if ((fd = open(pid_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1)
	{
		fprintf(stderr, "Failed to open PID file \"%s\"\n", pid_path);
		return -1;
	}

	if (flags & FD_CLOEXEC) 
	{
		// Use fcntl instead of O_CLOEXEC on open since it is
		// supported on more systems.

		if ((flags = fcntl(fd, F_GETFD)) == -1)
		{
			fprintf(stderr, "Failed to get flags for PID file \"%s\"\n", pid_path);
		}
		else
		{
			flags |= FD_CLOEXEC;

			if (fcntl(fd, F_SETFD, flags) == -1)
			{
				fprintf(stderr, "Failed to set flags for PID file \"%s\"\n", pid_path);
			}
		}
	}

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	if (fcntl(fd, F_SETLK, &fl) == -1) 
	{
		if ((errno == EAGAIN) || (errno == EACCES))
		{
			fprintf(stderr, "PID file '%s' is locked. '%s' is probably already running.\n",
				pid_path, prog_name);
		}	
		else
		{
			fprintf(stderr, "Unable to lock PID file '%s'\n", pid_path);
		}

		close(fd);
		return -1;
	}

	if (ftruncate(fd, 0) == -1)
	{
		fprintf(stderr, "Failed to truncate PID file '%s\n'", pid_path);
		close(fd);
		return -1;
	}

	snprintf(buf, sizeof(buf), "%ld\n", (long)getpid());
	
	if (write(fd, buf, strlen(buf)) != strlen(buf))
	{
		fprintf(stderr, "Failed to write PID file '%s'\n", pid_path);
		close(fd);
		return -1;
	}

	return fd;
}
#endif // _WIN32

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

void short_usage(const char *progname)
{
	fprintf(stderr, "For more details use --help\n");
}

int main(int argc, char **argv)
{
	catcierge_args_t *args;
	args = &grb.args;

	fprintf(stderr, "\nCatcierge Grabber v" CATCIERGE_VERSION_STR " (" CATCIERGE_GIT_HASH_SHORT "");

	// Was this built with changes in the git sources?
	#ifdef CATCIERGE_GIT_TAINTED
	fprintf(stderr, "-tainted");
	#endif

	fprintf(stderr, ")\n(C) Joakim Soderberg 2013-2015\n\n");

	fprintf(stderr, "Library versions:\n");
	fprintf(stderr, " OpenCV v%d.%d.%d\n", CV_MAJOR_VERSION, CV_MINOR_VERSION, CV_SUBMINOR_VERSION);
	#ifdef WITH_ZMQ
	fprintf(stderr, "   CZMQ v%d.%d.%d\n", CZMQ_VERSION_MAJOR, CZMQ_VERSION_MINOR, CZMQ_VERSION_PATCH);
	#endif
	fprintf(stderr, "\n");

	#ifndef _WIN32
	pid_fd = create_pid_file(argv[0], PID_PATH, FD_CLOEXEC);
	#endif

	if (catcierge_grabber_init(&grb))
	{
		fprintf(stderr, "Failed to init\n");
		return -1;
	}

	// Get program settings.
	{
		if (catcierge_parse_config(args, argc, argv))
		{
			short_usage(argv[0]);
			return -1;
		}

		if (catcierge_parse_cmdargs(args, argc, argv, NULL, NULL))
		{
			short_usage(argv[0]);
			return -1;
		}

		if (args->nocolor)
		{
			catcierge_nocolor = 1;
		}

		// TODO: Add verify function for settings. Make sure we have everything we need...
		if (catcierge_validate_settings(args))
		{
			short_usage(argv[0]);
			return -1;
		}

		catcierge_print_settings(args);
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

	if (catcierge_matcher_init(&grb.matcher, catcierge_get_matcher_args(args)))
	{
		CATERR("Failed to init %s matcher\n", grb.args.matcher);
		return -1;
	}

	CATLOG("Initialized catcierge image recognition\n");

	if (catcierge_output_init(&grb.output))
	{
		CATERR("Failed to init output template system\n");
		return -1;
	}

	if (catcierge_output_load_templates(&grb.output,
			args->inputs, args->input_count))
	{
		CATERR("Failed to load output templates\n");
		return -1;
	}

	CATLOG("Initialized output templates\n");

	#ifdef WITH_RFID
	catcierge_init_rfid_readers(&grb);
	#endif

	catcierge_setup_camera(&grb);

	#ifdef WITH_ZMQ
	catcierge_zmq_init(&grb);
	#endif

	CATLOG("Starting detection!\n");
	// TODO: Create a catcierge_grb_start(grb) function that does this instead.
	catcierge_fsm_start(&grb);

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
	} while (
		grb.running
		#ifdef WITH_ZMQ
		&& !zctx_interrupted
		#endif
		);

	catcierge_matcher_destroy(&grb.matcher);
	catcierge_output_destroy(&grb.output);
	catcierge_destroy_camera(&grb);
	#ifdef WITH_ZMQ
	catcierge_zmq_destroy(&grb);
	#endif
	catcierge_grabber_destroy(&grb);

	if (grb.log_file)
	{
		fclose(grb.log_file);
	}

	#ifndef _WIN32
	if (pid_fd > 0)
	{
		close(pid_fd);
		unlink(PID_PATH);
	}
	#endif

	return 0;
}
