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
#include "catcierge_config.h"
#include "catcierge_args.h"
#include "catcierge_log.h"
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#include "catcierge_util.h"
#include "catcierge_template_matcher.h"
#include "catcierge_haar_matcher.h"
#include "catcierge_timer.h"

#ifdef RPI
#include "RaspiCamCV.h"
#include "catcierge_gpio.h"
#endif

#ifdef CATCIERGE_HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef CATCIERGE_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef CATCIERGE_HAVE_GRP_H
#include <grp.h>
#endif
#ifdef CATCIERGE_HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef WITH_ZMQ
#include <czmq.h>
#endif

#include "catcierge_fsm.h"
#include "catcierge_output.h"

void catcierge_trigger_event(catcierge_grb_t *grb, catcierge_event_t e, int execute)
{
	catcierge_args_t *args = &grb->args;

	// We use this wrapper function and pass a catcierge_event_t so that if
	// one specifies an event type that is not defined in "catcierge_events.h"
	// it will fail at compilation time.

	#define CATCIERGE_DEFINE_EVENT(ev_enum_name, ev_name, ev_description)	\
		if (e == ev_enum_name)												\
		{																	\
			char **cmd = NULL;												\
			size_t count = args->ev_name ## _cmd_count;						\
			if (execute) cmd = args->ev_name ## _cmd;						\
			catcierge_output_execute_list(grb, #ev_name, cmd, count);		\
			return;															\
		}
	#include "catcierge_events.h"
}

void catcierge_run_state(catcierge_grb_t *grb)
{
	assert(grb);
	assert(grb->state);

	if (grb->running)
	{
		grb->state(grb);
	}
}

const char *catcierge_get_state_string(catcierge_state_func_t state)
{
	if (state == catcierge_state_waiting) return "Waiting";
	if (state == catcierge_state_matching) return "Matching";
	if (state == catcierge_state_keepopen) return "Keep open";
	if (state == catcierge_state_lockout) return "Lockout";

	return "Initial";
}

void catcierge_print_state(catcierge_state_func_t state)
{
	log_printf(stdout, COLOR_NORMAL, "[");
	log_printf(stdout, COLOR_YELLOW, "%s",
		catcierge_get_state_string(state));
	log_printf(stdout, COLOR_NORMAL, "]");
}

void catcierge_set_state(catcierge_grb_t *grb, catcierge_state_func_t new_state)
{
	catcierge_args_t *args = &grb->args;
	assert(grb);

	// Prints timestamp.
	log_printc(stdout, COLOR_NORMAL, "");

	catcierge_print_state(grb->state);
	log_printf(stdout, COLOR_MAGNETA, " -> ");
	catcierge_print_state(new_state);
	log_printf(stdout, COLOR_NORMAL, "\n");

	grb->prev_state = grb->state;
	grb->state = new_state;

	catcierge_trigger_event(grb, CATCIERGE_STATE_CHANGE, 1);
}

#ifdef WITH_RFID
static int match_allowed_rfid(catcierge_grb_t *grb, const char *rfid_tag)
{
	int i;
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	for (i = 0; i < args->rfid_allowed_count; i++)
	{
		if (!strcmp(rfid_tag, args->rfid_allowed[i]))
		{
			return 1;
		}
	}

	return 0;
}

static void rfid_set_direction(catcierge_grb_t *grb, rfid_match_t *current, rfid_match_t *other, 
						match_direction_t dir, const char *dir_str, catcierge_rfid_t *rfid, 
						int complete, const char *data, size_t data_len)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	CATLOG("%s RFID: %s%s\n", rfid->name, data, !complete ? " (incomplete)": "");

	CATLOG("   Old data %s (%d bytes) New data %s (%d bytes)\n",
		other->complete ? "COMPLETE" : "INCOMPLETE",
		(int)other->data_len,
		complete ? "COMPLETE" : "INCOMPLETE",
		(int)data_len);

	// Update the match if we get a complete tag.
	if (complete && (data_len > other->data_len))
	{
		strncpy(current->data, data, sizeof(current->data) - 1);
		current->data_len = data_len;
		current->complete = complete;
	}

	// If we have already triggered this reader
	// then don't set the direction again.
	// (Since this could screw things up).
	if (current->triggered)
	{
		CATLOG("Already triggered!\n");
		return;
	}

	// The other reader triggered first so we know the direction.
	// TODO: It could be wise to time this out after a while...
	if (other->triggered)
	{
		grb->rfid_direction = dir;
		CATLOG("%s RFID: Direction %s\n", rfid->name, dir_str);
	}

	current->triggered = 1;
	current->complete = complete;
	strncpy(current->data, data, sizeof(current->data) - 1);
	current->data_len = data_len;
	current->is_allowed = match_allowed_rfid(grb, current->data);

	// TODO: Do we have all RFID vars for this?
	catcierge_trigger_event(grb, CATCIERGE_RFID_DETECT, 1);
}

static void rfid_inner_read_cb(catcierge_rfid_t *rfid, int complete, const char *data, size_t data_len, void *user)
{
	catcierge_grb_t *grb = user;

	// The inner RFID reader has detected a tag, we now pass that
	// match on to the code that decides which direction the cat
	// is going.
	rfid_set_direction(grb, &grb->rfid_in_match, &grb->rfid_out_match,
			MATCH_DIR_IN, "IN", rfid, complete, data, data_len);
}

static void rfid_outer_read_cb(catcierge_rfid_t *rfid, int complete, const char *data, size_t data_len, void *user)
{
	catcierge_grb_t *grb = user;
	rfid_set_direction(grb, &grb->rfid_out_match, &grb->rfid_in_match,
			MATCH_DIR_OUT, "OUT", rfid, complete, data, data_len);
}

void catcierge_init_rfid_readers(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);

	args = &grb->args;

	catcierge_rfid_ctx_init(&grb->rfid_ctx);

	if (args->rfid_inner_path)
	{
		memset(&grb->rfid_in_match, 0, sizeof(grb->rfid_in_match));
		catcierge_rfid_init("Inner", &grb->rfid_in, args->rfid_inner_path, rfid_inner_read_cb, grb);
		catcierge_rfid_ctx_set_inner(&grb->rfid_ctx, &grb->rfid_in);
		catcierge_rfid_open(&grb->rfid_in);
	}

	if (args->rfid_outer_path)
	{
		memset(&grb->rfid_out_match, 0, sizeof(grb->rfid_out_match));
		catcierge_rfid_init("Outer", &grb->rfid_out, args->rfid_outer_path, rfid_outer_read_cb, grb);
		catcierge_rfid_ctx_set_outer(&grb->rfid_ctx, &grb->rfid_out);
		catcierge_rfid_open(&grb->rfid_out);
	}
	
	CATLOG("Initialized RFID readers\n");
}
#endif // WITH_RFID

void catcierge_do_lockout(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	if (args->lockout_dummy)
	{
		CATLOGFPS("!LOCKOUT DUMMY!\n");
		return;
	}

	if (args->do_lockout_cmd)
	{
		catcierge_trigger_event(grb, CATCIERGE_DO_LOCKOUT, 1);
	}
	else
	{
		catcierge_trigger_event(grb, CATCIERGE_DO_LOCKOUT, 0);

		#ifdef RPI
		gpio_write(CATCIERGE_LOCKOUT_GPIO, 1);

		if (args->backlight_enable)
		{
			gpio_write(CATCIERGE_BACKLIGHT_GPIO, 1);
		}
		#endif // RPI
	}
}

void catcierge_do_unlock(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	if (args->do_unlock_cmd)
	{
		catcierge_trigger_event(grb, CATCIERGE_DO_UNLOCK, 1);
	}
	else
	{
		catcierge_trigger_event(grb, CATCIERGE_DO_UNLOCK, 0);

		#ifdef RPI
		gpio_write(CATCIERGE_LOCKOUT_GPIO, 0);
		
		if (args->backlight_enable)
		{
			gpio_write(CATCIERGE_BACKLIGHT_GPIO, 1);
		}
		#endif // RPI
	}
}

static void catcierge_path_reset(catcierge_path_t *path)
{
	assert(path);

	path->filename[0] = '\0';
	path->full[0] = '\0';
	path->dir[0] = '\0';
}

static void catcierge_cleanup_match_steps(catcierge_grb_t *grb, match_result_t *result)
{
	int j;
	match_step_t *step = NULL;
	assert(grb);
	assert(result);

	for (j = 0; j < MAX_MATCH_RECTS; j++)
	{
		step = &result->steps[j];

		if (step->img)
		{
			cvReleaseImage(&step->img);
		}

		step->description = NULL;
		step->name = NULL;
		catcierge_path_reset(&step->path);
	}

	result->step_img_count = 0;
}

static void catcierge_cleanup_imgs(catcierge_grb_t *grb)
{
	int i;
	assert(grb);

	for (i = 0; i < MATCH_MAX_COUNT; i++)
	{
		if (grb->match_group.matches[i].img)
		{
			cvReleaseImage(&grb->match_group.matches[i].img);
		}

		catcierge_cleanup_match_steps(grb, &grb->match_group.matches[i].result);
	}

	if (grb->match_group.obstruct_img)
	{
		cvReleaseImage(&grb->match_group.obstruct_img);
	}
}

void catcierge_setup_camera(catcierge_grb_t *grb)
{
	assert(grb);

	#ifdef RPI
	grb->capture = raspiCamCvCreateCameraCaptureEx(0, &grb->args.rpi_settings);
	#else
	grb->capture = cvCreateCameraCapture(0);
	cvSetCaptureProperty(grb->capture, CV_CAP_PROP_FRAME_WIDTH, 320);
	cvSetCaptureProperty(grb->capture, CV_CAP_PROP_FRAME_HEIGHT, 240);
	#endif

	if (grb->args.show)
	{
		cvNamedWindow("catcierge", 1);
	}
}

void catcierge_destroy_camera(catcierge_grb_t *grb)
{
	if (grb->args.show)
	{
		cvDestroyWindow("catcierge");
	}

	#ifdef RPI
	raspiCamCvReleaseCapture(&grb->capture);
	#else
	cvReleaseCapture(&grb->capture);
	#endif
}

int catcierge_drop_root_privileges(const char *user)
{
	#ifdef CATCIERGE_ENABLE_DROP_ROOT_PRIVILEGES
	struct passwd *pw = getpwnam(user);

	if (getuid() != 0)
	{
		CATLOG("Not running as root (no privileges to drop).\n");
		return 0;
	}

	if (initgroups(pw->pw_name, pw->pw_gid)
	 || setgid(pw->pw_gid)
	 || setuid(pw->pw_uid))
	{
		CATERR("Failed to drop root privileges '%.32s' uid=%lu gid=%lu: %s\n",
			user, (unsigned long)pw->pw_uid, (unsigned long)pw->pw_gid,
			strerror(errno));
		return -1;
	}
	#else
	CATLOG("(Drop root privileges not compiled)\n");
	#endif // CATCIERGE_ENABLE_DROP_ROOT_PRIVILEGES
	return 0;
}

#ifdef RPI
int catcierge_setup_gpio(catcierge_grb_t *grb)
{
	catcierge_args_t *args = &grb->args;
	int ret = 0;

	if (args->do_lockout_cmd)
	{
		CATLOG("Skipping GPIO setup since a custom lockout command is set:");
		CATLOG("  %s", args->do_lockout_cmd);
		return 0;
	}

	// Set export for pins.
	if (gpio_export(CATCIERGE_LOCKOUT_GPIO)
	 || gpio_set_direction(CATCIERGE_LOCKOUT_GPIO, OUT))
	{
		CATERR("Failed to export and set direction for door pin\n");
		ret = -1; goto fail;
	}

	// Start with the door open and light on.
	gpio_write(CATCIERGE_LOCKOUT_GPIO, 0);

	if (args->backlight_enable)
	{
		if (gpio_export(CATCIERGE_BACKLIGHT_GPIO)
		 || gpio_set_direction(CATCIERGE_BACKLIGHT_GPIO, OUT))
		{
			CATERR("Failed to export and set direction for backlight pin\n");
			ret = -1; goto fail;
		}
	
		gpio_write(CATCIERGE_BACKLIGHT_GPIO, 1);
	}

fail:
	if (ret)
	{
		// Check if we're root.
		if (getuid() != 0)
		{
			CATERR("###############################################\n");
			CATERR("########## You have to run as root! ###########\n");
			CATERR("###############################################\n");
		}
	}
	else if (args->chuid && (getuid() == 0))
	{
		if (!catcierge_drop_root_privileges(args->chuid))
		{
			CATLOG("###############################################\n");
			CATLOG("########## Root privileges dropped ############\n");
			CATLOG("###############################################\n");
		}
	}

	return ret;
}
#endif // RPI

IplImage *catcierge_get_frame(catcierge_grb_t *grb)
{
	assert(grb);

	#ifdef RPI
	return raspiCamCvQueryFrame(grb->capture);
	#else
	return cvQueryFrame(grb->capture);
	#endif	
}

static int catcierge_calculate_match_id(IplImage *img, match_state_t *m)
{
	assert(img);
	assert(m);

	// Get a unique match id by calculating SHA1 hash of the image data
	// as well as timestamp.
	SHA1Reset(&m->sha);
	SHA1Input(&m->sha, (const unsigned char *)img->imageData, img->imageSize);
	SHA1Input(&m->sha, (const unsigned char *)m->time_str, strlen(m->time_str));

	if (!SHA1Result(&m->sha))
	{
		return -1;
	}

	return 0;
}

static int catcierge_calculate_matchgroup_id(match_group_t *mg, IplImage *img)
{
	char time_str[512];
	assert(mg);

	get_time_str_fmt(mg->start_time, &mg->start_tv, time_str,
		sizeof(time_str), NULL);

	// We base the match group id on the obstruct image + timestamp.
	SHA1Reset(&mg->sha);
	SHA1Input(&mg->sha, (const unsigned char *)img->imageData, img->imageSize);
	SHA1Input(&mg->sha, (const unsigned char *)time_str, strlen(time_str));

	if (!SHA1Result(&mg->sha))
	{
		return -1;
	}

	return 0;
}

static void catcierge_process_match_result(catcierge_grb_t *grb, IplImage *img)
{
	size_t j;
	catcierge_args_t *args = NULL;
	match_result_t *res = NULL;
 	match_state_t *m = NULL;
	assert(grb);
	assert(img);
	assert(grb->match_group.match_count <= MATCH_MAX_COUNT);
	args = &grb->args;

	m = &grb->match_group.matches[grb->match_group.match_count - 1];
	res = &m->result;

	// Get time of match and format.
	m->img = NULL;
	m->time = time(NULL); // TODO: Get rid of this and use tv.tv_sec instead, same thing.
	gettimeofday(&m->tv, NULL);
	get_time_str_fmt(m->time, &m->tv, m->time_str,
		sizeof(m->time_str), FILENAME_TIME_FORMAT);

	// Calculate match id from time + image data.
	if (catcierge_calculate_match_id(img, m))
	{
		CATERR("Failed to calculate match id!\n");
	}

	log_printc(stdout, (res->success ? COLOR_GREEN : COLOR_RED),
		"%sMatch %s - %s (%x%x%x%x%x)\n",
		res->success ? "" : "No ",
		catcierge_get_direction_str(res->direction),
		res->description,
		m->sha.Message_Digest[0],
		m->sha.Message_Digest[1],
		m->sha.Message_Digest[2],
		m->sha.Message_Digest[3],
		m->sha.Message_Digest[4]);

	catcierge_path_reset(&m->path);

	// Save match image.
	// (We don't write to disk yet, that will slow down the matching).
	if (args->saveimg)
	{
		char base_path[1024];
		char *match_gen_output_path = NULL;

		// TODO: Set this at init instead...
		if (!args->match_output_path)
		{
			if (!(args->match_output_path = strdup(args->output_path)))
			{
				CATERR("Out of memory");
			}
		}

		if (!(match_gen_output_path = catcierge_output_generate(&grb->output, grb, args->match_output_path)))
		{
			CATERR("Failed to generate match output path from: \"%s\"\n", args->match_output_path);
		}

		// TODO: Enable setting this via template variables instead.
		snprintf(base_path,
			sizeof(base_path) - 1,
			"match_%s_%s__%d",
			res->success ? "" : "fail",
			m->time_str,
			(int)grb->match_group.match_count);

		snprintf(m->path.dir, sizeof(m->path.dir) - 1, "%s", match_gen_output_path);
		snprintf(m->path.filename, sizeof(m->path.filename) - 1, "%s.png", base_path);
		snprintf(m->path.full, sizeof(m->path.full) - 1, "%s%s%s",
				 m->path.dir, catcierge_path_sep(), m->path.filename);

		if (match_gen_output_path)
		{
			free(match_gen_output_path);
		}

		m->img = cvCloneImage(img);
		// TODO: Add option to save the image right away also.

		if (args->save_steps)
		{
			match_step_t *step;
			char *step_gen_output_path = NULL;

			// TODO: Move to init instead.
			if (!args->steps_output_path)
			{
				if (!(args->steps_output_path = strdup(args->output_path)))
				{
					CATERR("Out of memory");
				}
			}

			for (j = 0; j < m->result.step_img_count; j++)
			{
				if (!(step_gen_output_path = catcierge_output_generate(&grb->output, grb, args->steps_output_path)))
				{
					CATERR("Failed to generate step output path from: \"%s\"\n", args->steps_output_path);
				}

				step = &m->result.steps[j];
				snprintf(step->path.dir, sizeof(step->path.dir) - 1,
					"%s", step_gen_output_path);

				snprintf(step->path.filename, sizeof(step->path.filename) - 1,
					"%s_%02d_%s.png",
					base_path,
					(int)j,
					step->name);

				snprintf(step->path.full, sizeof(step->path.full) - 1, "%s%s%s",
					step->path.dir, catcierge_path_sep(), step->path.filename);

				if (step_gen_output_path)
				{
					free(step_gen_output_path);
					step_gen_output_path = NULL;
				}
			}
		}
	}
}

static void catcierge_save_images(catcierge_grb_t *grb, match_direction_t direction)
{
	match_group_t *mg = &grb->match_group;
	match_state_t *m;
	match_result_t *res;
	int i;
	size_t j;
	catcierge_args_t *args;
	match_step_t *step = NULL;
	assert(grb);
	args = &grb->args;

	if (args->save_obstruct_img)
	{
		CATLOG("Saving obstruct image: %s\n", mg->obstruct_path.full);
		catcierge_make_path(mg->obstruct_path.dir);
		cvSaveImage(mg->obstruct_path.full, mg->obstruct_img, 0);
		// TODO: Save obstruct step images as well?
		// TODO: Add execute event for this?

		cvReleaseImage(&mg->obstruct_img);
	}

	for (i = 0; i < MATCH_MAX_COUNT; i++)
	{
		m = &grb->match_group.matches[i];
		res = &m->result;

		CATLOG("Saving image %s\n", m->path.full);
		catcierge_make_path(m->path.dir);
		cvSaveImage(m->path.full, m->img, 0);

		if (args->save_steps)
		{
			for (j = 0; j < m->result.step_img_count; j++)
			{
				step = &m->result.steps[j];
				CATLOG("  %02d %-34s  %s\n", (int)j, step->description, step->path.full);

				if (step->img)
				{
					catcierge_make_path(step->path.dir);
					cvSaveImage(step->path.full, step->img, NULL);
				}
			}
		}

		catcierge_trigger_event(grb, CATCIERGE_SAVE_IMG, 1);

		cvReleaseImage(&m->img);
	}
}

static int catcierge_check_max_consecutive_lockouts(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	// Check how long ago we perform the last lockout.
	// If there are too many lockouts in a row, there
	// might be an error. Such as the backlight failing.
	if (args->max_consecutive_lockout_count)
	{
		double lockout_timer_val = catcierge_timer_get(&grb->lockout_timer);

		if ((lockout_timer_val
			<= (args->lockout_time + args->consecutive_lockout_delay)))
		{
			grb->consecutive_lockout_count++;
			if (grb->consecutive_lockout_count > 1)
			{
				CATLOG("Consecutive lockout! %d out of %d before quiting. "
					   "(%0.2f sec <= %0.2f sec)\n",
						grb->consecutive_lockout_count,
						args->max_consecutive_lockout_count,
						lockout_timer_val,
						(args->lockout_time + args->consecutive_lockout_delay));
				CATLOG("  (Lockout Time %0.2f + Consecutive lockout delay %0.2f = %0.2f)\n",
						args->consecutive_lockout_delay,
						args->lockout_time, args->consecutive_lockout_delay);
			}
		}
		else
		{
			grb->consecutive_lockout_count = 0;

			CATLOG("Consecutive lockout count reset. "
					"%0.2f seconds between lockouts "
					"(consecutive lockout delay = %0.2f seconds)\n",
					lockout_timer_val,
					args->consecutive_lockout_delay);
		}

		// Exit the program, we assume we have an error.
		if (grb->consecutive_lockout_count >= args->max_consecutive_lockout_count)
		{
			CATLOG("Too many lockouts in a row (%d)! Assuming something is wrong... Aborting program!\n",
						grb->consecutive_lockout_count);
			catcierge_do_unlock(grb);
			// TODO: Add a special event the user can trigger an external program with here...
			grb->running = 0;
			//exit(1);
			return -1;
		}
	}

	return 0;
}

#ifdef WITH_RFID
static void catcierge_should_we_rfid_lockout(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	if (!args->lock_on_invalid_rfid)
		return;

	if (!grb->checked_rfid_lock 
		&& (args->rfid_inner_path || args->rfid_outer_path))
	{
		// Have we waited long enough since the camera match was
		// complete (The cat must have moved far enough for both
		// readers to have a chance to detect it).
		if (catcierge_timer_get(&grb->rematch_timer) >= args->rfid_lock_time)
		{
			int do_rfid_lockout = 0;

			if (!grb->rfid_out_match.triggered && !grb->rfid_in_match.triggered)
			{
				CATERR("Unknown RFID direction!\n");
				grb->rfid_direction = MATCH_DIR_UNKNOWN;
			}

			if (args->rfid_inner_path && args->rfid_outer_path)
			{
				// Only require one of the readers to have a correct read.
				do_rfid_lockout = !(grb->rfid_in_match.is_allowed 
								|| grb->rfid_out_match.is_allowed);
			}
			else if (args->rfid_inner_path)
			{
				do_rfid_lockout = !grb->rfid_in_match.is_allowed;
			}
			else if (args->rfid_outer_path)
			{
				do_rfid_lockout = !grb->rfid_out_match.is_allowed;
			}

			if (do_rfid_lockout)
			{
				if (grb->rfid_direction == MATCH_DIR_OUT)
				{
					CATLOG("RFID lockout: Skipping since cat is going out\n");
				}
				else
				{
					CATLOG("RFID lockout!\n");
					catcierge_state_transition_lockout(grb);
				}
			}
			else
			{
				CATLOG("RFID OK!\n");
			}

			if (args->rfid_inner_path) CATLOG("  %s RFID: %s\n", grb->rfid_in.name, grb->rfid_in_match.triggered ? grb->rfid_in_match.data : "No tag data");
			if (args->rfid_outer_path) CATLOG("  %s RFID: %s\n", grb->rfid_out.name, grb->rfid_out_match.triggered ? grb->rfid_out_match.data : "No tag data");

			// TODO: Do we have all RFID vars for this?
			catcierge_trigger_event(grb, CATCIERGE_RFID_MATCH, 1);

			grb->checked_rfid_lock = 1;
		}
	}
}
#endif // WITH_RFID

static void catcierge_show_image(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	match_state_t *m;
	match_result_t *res;
	IplImage *img;
	IplImage *tmp_img = NULL;
	assert(grb);
	args = &grb->args;

	if (!grb->img)
		return;

	// Show the video feed.
	if (args->show)
	{
		img = grb->img;

		// Only try to show the match rectangles when we're in match mode.
		if ((grb->match_group.match_count > 0)
			&& (grb->match_group.match_count <= MATCH_MAX_COUNT))
		{
			size_t i;
			CvScalar match_color;

			// We don't want to mess with the original image when
			// drawing the match rects since that might interfer with the match.
			tmp_img = cvCloneImage(grb->img);

			// TODO: Hmmm this should not be -1 I think?
			m = &grb->match_group.matches[grb->match_group.match_count - 1];
			res = &m->result;

			#ifdef RPI
			match_color = CV_RGB(255, 255, 255); // Grayscale so don't bother with color.
			#else
			match_color = (res->success) ? CV_RGB(0, 255, 0) : CV_RGB(255, 0, 0);
			#endif

			// Always highlight when showing in GUI.
			for (i = 0; i < res->rect_count; i++)
			{
				cvRectangleR(tmp_img, res->match_rects[i], match_color, 2, 8, 0);
			}

			img = tmp_img;
		}

		cvShowImage("catcierge", img);
		cvWaitKey(10);

		if (tmp_img)
		{
			cvReleaseImage(&tmp_img);
		}
	}
}

double catcierge_do_match(catcierge_grb_t *grb)
{
	double match_res = 0.0;
	catcierge_args_t *args;
	match_group_t *mg = &grb->match_group;
	match_result_t *result;
	match_state_t *match;
	assert(grb);
	assert(mg->match_count <= MATCH_MAX_COUNT);
	args = &grb->args;

	// Clear match structs before doing a new one.
	match = &mg->matches[mg->match_count - 1];
	result = &match->result;
	catcierge_cleanup_match_steps(grb, result);
	memset(result, 0, sizeof(match_result_t));

	if ((match_res = grb->matcher->match(grb->matcher, grb->img, result, args->save_steps)) < 0.0)
	{
		CATERR("%s matcher: Error when matching frame!\n", grb->matcher->name);
	}

	return match_res;
}

static match_direction_t catcierge_guess_overall_direction(catcierge_grb_t *grb)
{
	int i;
	match_direction_t direction = MATCH_DIR_UNKNOWN;
	assert(grb);

	if (grb->args.matcher_type == MATCHER_TEMPLATE)
	{
		// Get any successful direction.
		// (It is very uncommon for 2 successful matches to give different
		// direction with the template matcher, so we can be pretty sure
		// this is correct).
		for (i = 0; i < MATCH_MAX_COUNT; i++)
		{
			if (grb->match_group.matches[i].result.success)
			{
				direction = grb->match_group.matches[i].result.direction;
			}
		}
	}
	else
	{
		// Count the actual direction counts for the haar matcher.
		int in_count = 0;
		int out_count = 0;
		int unknown_count = 0;

		for (i = 0; i < MATCH_MAX_COUNT; i++)
		{
			switch (grb->match_group.matches[i].result.direction)
			{
				case MATCH_DIR_IN: in_count++; break;
				case MATCH_DIR_OUT: out_count++; break;
				case MATCH_DIR_UNKNOWN: unknown_count++; break;
			}
		}

		if ((in_count > out_count) && (in_count > unknown_count))
		{
			direction = MATCH_DIR_IN;
		}
		else if (out_count > unknown_count)
		{
			direction = MATCH_DIR_OUT;
		}
		else
		{
			direction = MATCH_DIR_UNKNOWN;
		}
	}

	return direction;
}

void catcierge_match_group_start(match_group_t *mg, IplImage *img)
{
	assert(mg);

	gettimeofday(&mg->start_tv, NULL);
	mg->start_time = time(NULL);

	memset(&mg->end_tv, 0, sizeof(mg->end_tv));
	mg->end_time = 0;

	mg->description[0] = '\0';
	catcierge_path_reset(&mg->obstruct_path);
	mg->match_count = 0;
	mg->final_decision = 0;

	// We base the matchgroup id on the obstruct image + timestamp.
	catcierge_calculate_matchgroup_id(mg, img);

	CATLOG("\n");
	log_printc(stdout, COLOR_YELLOW, "=== Match group id: %x%x%x%x%x ===\n",
		mg->sha.Message_Digest[0],
		mg->sha.Message_Digest[1],
		mg->sha.Message_Digest[2],
		mg->sha.Message_Digest[3],
		mg->sha.Message_Digest[4]);
	CATLOG("\n");

	if (mg->obstruct_img)
	{
		cvReleaseImage(&mg->obstruct_img);
	}
}

void catcierge_match_group_end(match_group_t *mg)
{
	assert(mg);

	gettimeofday(&mg->end_tv, NULL);
	mg->end_time = time(NULL);
}

void catcierge_decide_lock_status(catcierge_grb_t *grb)
{
	match_group_t *mg = &grb->match_group;
	catcierge_args_t *args = &grb->args;
	assert(grb);
	int i;

	mg->success = 0;
	mg->success_count = 0;

	for (i = 0; i < MATCH_MAX_COUNT; i++)
	{
		mg->success_count += !!mg->matches[i].result.success;
	}

	// Guess the direction.
	mg->direction = catcierge_guess_overall_direction(grb);

	// When going out, if only 1 image is a succesful match
	// we still count it as overall succesful so we don't get
	// so many false negatives.
	if (mg->direction == MATCH_DIR_OUT)
	{
		snprintf(mg->description, sizeof(mg->description) - 1, "Going out");
		mg->success = 1;
	}
	else
	{
		// Otherwise if enough matches (default 2) are ok.
		mg->success = (mg->success_count >= args->ok_matches_needed);

		if (!mg->success)
		{
			snprintf(mg->description, sizeof(mg->description) - 1,
				"Lockout %d of %d matches failed",
				(MATCH_MAX_COUNT - mg->success_count), MATCH_MAX_COUNT);
		}

		// Let the matcher veto if the match group was successful.
		if (!args->no_final_decision)
		{
			assert(mg->final_decision == 0);
			mg->success = grb->matcher->decide(grb->matcher, mg);

			if (mg->final_decision)
			{
				CATERR("!!! Match group vetoed match success: %s !!!\n", mg->description);
			}
		}
	}

	if (mg->success)
	{
		snprintf(mg->description, sizeof(mg->description) - 1, "Everything OK!");

		CATLOG("Everything OK! (%d out of %d matches succeeded)"
				" Door kept open...\n", mg->success_count, MATCH_MAX_COUNT);

		if (grb->consecutive_lockout_count > 0)
		{
			CATLOG("Consecutive lockout count reset (was %d)\n",
				grb->consecutive_lockout_count);

			grb->consecutive_lockout_count = 0;
		}

		// Make sure the door is open.
		catcierge_do_unlock(grb);

		#ifdef WITH_RFID
		// We only want to check for RFID lock once
		// during each match timeout period.
		grb->checked_rfid_lock = 0;
		#endif

		catcierge_timer_reset(&grb->rematch_timer);
		catcierge_set_state(grb, catcierge_state_keepopen);
	}
	else
	{
		CATLOG("Lockout! %d out of %d matches failed (for %d seconds).\n",
				(MATCH_MAX_COUNT - mg->success_count), MATCH_MAX_COUNT,
				args->lockout_time);

		// Only do the lockout if something isn't wrong.
		if (catcierge_check_max_consecutive_lockouts(grb))
		{
			catcierge_set_state(grb, catcierge_state_waiting);
		}
		else
		{
			catcierge_state_transition_lockout(grb);
		}
	}

	catcierge_match_group_end(mg);

	// Now we can save the images that we cached earlier 
	// without slowing down the matching FPS.
	if (args->saveimg)
	{
		catcierge_save_images(grb, mg->direction);
	}

	catcierge_trigger_event(grb, CATCIERGE_MATCH_GROUP_DONE, 1);

	assert(mg->match_count <= MATCH_MAX_COUNT);
}

void catcierge_save_obstruct_image(catcierge_grb_t *grb)
{
	if (grb->args.saveimg && grb->args.save_obstruct_img)
	{
		char *gen_output_path = NULL;
		char time_str[1024];
		catcierge_args_t *args = &grb->args;
		match_group_t *mg = &grb->match_group;

		if (mg->obstruct_img)
		{
			cvReleaseImage(&mg->obstruct_img);
		}

		mg->obstruct_img = cvCloneImage(grb->img);

		mg->obstruct_time = time(NULL);
		gettimeofday(&mg->obstruct_tv, NULL);
		get_time_str_fmt(mg->obstruct_time, &mg->obstruct_tv, time_str,
			sizeof(time_str), FILENAME_TIME_FORMAT);

		// TODO: Move to init
		if (!args->obstruct_output_path)
		{
			if (!(args->obstruct_output_path = strdup(args->output_path)))
			{
				CATERR("Out of memory");
			}
		}

		// TODO: Break this out into a function and reuse for all saved images.
		if (!(gen_output_path = catcierge_output_generate(&grb->output, grb, args->obstruct_output_path)))
		{
			CATERR("Failed to generate output path from: \"%s\"\n", args->obstruct_output_path);
		}

		snprintf(mg->obstruct_path.dir, sizeof(mg->obstruct_path.dir) - 1, "%s", gen_output_path);
		snprintf(mg->obstruct_path.filename, sizeof(mg->obstruct_path.filename) - 1,
			"match_obstruct_%s.png", time_str);

		snprintf(mg->obstruct_path.full, sizeof(mg->obstruct_path.full) - 1, "%s%s%s",
				 mg->obstruct_path.dir, catcierge_path_sep(), mg->obstruct_path.filename);

		if (gen_output_path)
		{
			free(gen_output_path);
		}
	}
}

void catcierge_fsm_start(catcierge_grb_t *grb)
{
	grb->running = 1;
	catcierge_set_state(grb, catcierge_state_waiting);
	catcierge_timer_set(&grb->frame_timer, 1.0);
	catcierge_timer_set(&grb->startup_timer, grb->args.startup_delay);
	catcierge_timer_start(&grb->startup_timer);
}

#ifdef WITH_ZMQ

void catcierge_zmq_destroy(catcierge_grb_t *grb)
{
	if (grb->zmq_ctx && grb->zmq_pub)
	{
		zsocket_destroy(grb->zmq_ctx, grb->zmq_pub);
		grb->zmq_pub = NULL;
	}

	if (grb->zmq_ctx)
	{
		zctx_destroy(&grb->zmq_ctx);
		grb->zmq_ctx = NULL;
	}
}

int catcierge_zmq_init(catcierge_grb_t *grb)
{
	catcierge_args_t *args = &grb->args;
	assert(grb);

	if (!args->zmq)
	{
		return 0;
	}

	if (!(grb->zmq_ctx = zctx_new()))
	{
		CATERR("Failed to create ZMQ context\n");
		return -1;
	}

	if (!(grb->zmq_pub = zsocket_new(grb->zmq_ctx, ZMQ_PUB)))
	{
		CATERR("Failed to create ZMQ publisher socket\n");
		goto fail;
	}

	if (zsocket_bind(grb->zmq_pub, "%s://%s:%d",
		args->zmq_transport, args->zmq_iface, args->zmq_port) < 0)
	{
		CATERR("Failed to bind to ZMQ publisher to %s://%s:%d\n",
			args->zmq_transport, args->zmq_iface, args->zmq_port);
		goto fail;
	}

	CATLOG("ZMQ publish to %s://%s:%d\n",
			args->zmq_transport, args->zmq_iface, args->zmq_port);

	return 0;

fail:
	catcierge_zmq_destroy(grb);
	return -1;
}
#endif // WITH_ZMQ 

// =============================================================================
// States
// =============================================================================
int catcierge_state_waiting(catcierge_grb_t *grb);

int catcierge_state_keepopen(catcierge_grb_t *grb)
{
	catcierge_args_t *args = &grb->args;
	assert(grb);

	catcierge_show_image(grb);

	// Wait until the frame is clear before we start the timer.
	// When this timer ends, we will go back to the WAITING state.
	if (!catcierge_timer_isactive(&grb->rematch_timer))
	{
		// We have successfully matched a valid cat :D
		int frame_obstructed;

		if ((frame_obstructed = grb->matcher->is_obstructed(grb->matcher, grb->img)) < 0)
		{
			CATERR("Failed to run check for obstructed frame\n");
			return -1;
		}

		if (frame_obstructed)
		{
			return 0;
		}

		CATLOG("Frame is clear, start successful match timer...\n");
		catcierge_timer_set(&grb->rematch_timer, grb->args.match_time);
		catcierge_timer_start(&grb->rematch_timer);
	}

	if (catcierge_timer_has_timed_out(&grb->rematch_timer))
	{
		CATLOG("Go back to waiting...\n");
		catcierge_set_state(grb, catcierge_state_waiting);
		return 0;
	}

	#ifdef WITH_RFID
	// The check to block on RFID is performed at a delay after the
	// image matching has been performed, to give the cat time to
	// pass both RFID readers.
	catcierge_should_we_rfid_lockout(grb);
	#endif

	return 0;
}

int catcierge_state_lockout(catcierge_grb_t *grb)
{
	int frame_obstructed;
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	catcierge_show_image(grb);

	if (args->lockout_method == OBSTRUCT_OR_TIMER_3)
	{
		// Stop the lockout when frame is clear
		// OR if the lockout timer ends.

		if ((frame_obstructed = grb->matcher->is_obstructed(grb->matcher, grb->img)) < 0)
		{
			CATERR("Failed to run check for obstructed frame\n");
			return -1;
		}

		if (!frame_obstructed || catcierge_timer_has_timed_out(&grb->lockout_timer))
		{
			CATLOG("End of lockout! (timed out after %.2f seconds)\n",
				catcierge_timer_get(&grb->lockout_timer));
			catcierge_do_unlock(grb);
			catcierge_set_state(grb, catcierge_state_waiting);
			return 0;
		}
	}
	else if (args->lockout_method == OBSTRUCT_THEN_TIMER_2)
	{
		// Don't start the lockout timer until the frame becomes clear.

		if (!catcierge_timer_isactive(&grb->lockout_timer))
		{
			if ((frame_obstructed = grb->matcher->is_obstructed(grb->matcher, grb->img)) < 0)
			{
				CATERR("Failed to run check for obstructed frame\n");
				return -1;
			}

			if (frame_obstructed)
			{
				return 0;
			}

			CATLOG("Frame is clear, start lockout timer for %d seconds...\n\n",
				args->lockout_time);
			catcierge_timer_set(&grb->lockout_timer, args->lockout_time);
			catcierge_timer_start(&grb->lockout_timer);
		}
	}

	// TIMER_ONLY_1
	// Start the lockout timer right away and unlock after that.
	if (catcierge_timer_has_timed_out(&grb->lockout_timer))
	{
		CATLOG("End of lockout! (timed out after %.2f seconds)\n",
			catcierge_timer_get(&grb->lockout_timer));

		catcierge_do_unlock(grb);
		catcierge_set_state(grb, catcierge_state_waiting);
	}
	return 0;
}

void catcierge_state_transition_lockout(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;
	catcierge_timer_reset(&grb->lockout_timer);
	assert((args->lockout_method >= TIMER_ONLY_1) && (args->lockout_method <= OBSTRUCT_OR_TIMER_3));

	// Start the timer up front depending on lockout method.
	switch (args->lockout_method)
	{
		case OBSTRUCT_THEN_TIMER_2:
			CATLOG("Waiting to start lockout timer until frame clears (Lockout method 2)\n");
			break;
		case OBSTRUCT_OR_TIMER_3:
			CATLOG("Wait for lockout timer to finish OR frame to clear (Lockout method 3)\n");
			catcierge_timer_set(&grb->lockout_timer, args->lockout_time);
			catcierge_timer_start(&grb->lockout_timer);
			break;
		case TIMER_ONLY_1:
			CATLOG("Waiting for lockout timer only (Lockout method 1)\n");
			catcierge_timer_set(&grb->lockout_timer, args->lockout_time);
			catcierge_timer_start(&grb->lockout_timer);
			break;
	}

	catcierge_set_state(grb, catcierge_state_lockout);
	catcierge_do_lockout(grb);
}

int catcierge_state_matching(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	match_group_t *mg = &grb->match_group;
	assert(grb);
	args = &grb->args;

	grb->match_group.match_count++;

	// We have something to match against.
	if (catcierge_do_match(grb) < 0)
	{
		CATERR("Error when matching frame!\n"); return -1;
	}

	catcierge_process_match_result(grb, grb->img);

	catcierge_trigger_event(grb, CATCIERGE_MATCH_DONE, 1);

	catcierge_show_image(grb);

	if (mg->match_count < MATCH_MAX_COUNT)
	{
		// Continue until we have enough matches for a decision.
		return 0;
	}
	else
	{
		catcierge_decide_lock_status(grb);
	}

	return 0;
}

int catcierge_state_waiting(catcierge_grb_t *grb)
{
	int frame_obstructed;
	catcierge_args_t *args = &grb->args;
	match_group_t *mg = &grb->match_group;
	assert(grb);

	catcierge_show_image(grb);

	if (catcierge_timer_isactive(&grb->startup_timer))
	{
		if (!catcierge_timer_has_timed_out(&grb->startup_timer))
		{
			return 0;
		}

		catcierge_timer_reset(&grb->startup_timer);
		CATLOG("Startup delay of %0.2f seconds has ended!\n", args->startup_delay);

		if (args->auto_roi)
		{
			CATLOG("Automatically setting the frame obstruction Region Of Interest (ROI) to the back light area.\n");

			if (catcierge_get_back_light_area(grb->matcher, grb->img, &args->roi))
			{
				CATERR("Forcing Exit!\n");
				grb->running = 0; return -1;
			}
		}

		if ((args->roi.width != 0) && (args->roi.height != 0))
		{
			CATLOG("Obstruction Region Of Interest (ROI): x: %d y: %d w: %d h: %d\n",
						args->roi.x, args->roi.y,
						args->roi.width, args->roi.height);
		}
	}

	// Wait until the middle of the frame is black
	// before we try to match anything.
	if ((frame_obstructed = grb->matcher->is_obstructed(grb->matcher, grb->img)) < 0)
	{
		CATERR("Failed to perform check for obstructed frame\n"); return -1;
	}

	if (frame_obstructed)
	{
		CATLOG("Something in frame! Start matching...\n");

		catcierge_match_group_start(mg, grb->img);

		// Save the obstruct image.
		catcierge_save_obstruct_image(grb);

		catcierge_trigger_event(grb, CATCIERGE_FRAME_OBSTRUCTED, 1);

		catcierge_set_state(grb, catcierge_state_matching);
	}

	return 0;
}

void catcierge_print_spinner(catcierge_grb_t *grb)
{
	catcierge_args_t *args;
	assert(grb);
	args = &grb->args;

	if (catcierge_timer_has_timed_out(&grb->frame_timer))
	{
		char spinner[] = "\\|/-\\|/-";
		static int spinidx = 0;
		catcierge_timer_reset(&grb->frame_timer);

		if (args->noanim)
		{
			return;
		}

		// This prints the log timestamp.
		log_printc(stdout, COLOR_NORMAL, "");
		catcierge_print_state(grb->state);
		log_printf(stdout, COLOR_NORMAL, "  ");

		if (grb->state == catcierge_state_lockout)
		{
			log_printf(stdout, COLOR_RED, "Lockout for %d more seconds.\n",
				(int)(args->lockout_time - catcierge_timer_get(&grb->lockout_timer)));
		}
		else if (grb->state == catcierge_state_keepopen)
		{
			if (catcierge_timer_isactive(&grb->rematch_timer))
			{
				log_printf(stdout, COLOR_RED, "Waiting to match again for %d more seconds.\n",
					(int)(args->match_time - catcierge_timer_get(&grb->rematch_timer)));
			}
			else
			{
				log_printf(stdout, COLOR_NORMAL, "Frame is obstructed. Waiting for it to clear...\n");
			}
		}
		else if (catcierge_timer_isactive(&grb->startup_timer))
		{
			log_printf(stdout, COLOR_NORMAL, "Waiting for the startup delay %d seconds left.\n",
				(int)(args->startup_delay - catcierge_timer_get(&grb->startup_timer)));
		}
		else
		{
			log_printf(stdout, COLOR_CYAN, "%c\n", spinner[spinidx++ % (sizeof(spinner) - 1)]);
		}

		// Moves the cursor back so that we print the spinner in place.
		catcierge_reset_cursor_position();
	}
}

int catcierge_grabber_init(catcierge_grb_t *grb)
{
	assert(grb);

	memset(grb, 0, sizeof(catcierge_grb_t));
	#if 0
	if (catcierge_args_init(&grb->args))
	{
		return -1;
	}
	#endif

	return 0;
}

void catcierge_grabber_destroy(catcierge_grb_t *grb)
{
	// Always make sure we unlock.
	catcierge_do_unlock(grb);
	catcierge_cleanup_imgs(grb);
	cvDestroyAllWindows();
}
