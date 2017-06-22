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

#ifndef __CATCIERGE_TYPES_H__
#define __CATCIERGE_TYPES_H__

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <time.h>

#include "catcierge_platform.h"
#include "sha1.h"

#define MATCH_MAX_COUNT 4 // The number of matches to perform before deciding the lock state.

#define CATCIERGE_DEFINE_EVENT(ev_enum_name, ev_name, ev_description)	\
	ev_enum_name,

typedef enum catcierge_event_e
{
	#include "catcierge_events.h"
} catcierge_event_t;

#define CATCIERGE_SIGUSR_BEHAVIOR(sigusr_enum_name, sigusr_name, sigusr_description)	\
	sigusr_enum_name,

typedef enum catcierge_sigusr_behavior_e
{
	#include "catcierge_sigusr.h"
} catcierge_sigusr_behavior_t;

typedef struct catcierge_output_var_s
{
	char *name;
	char *description;
} catcierge_output_var_t;

typedef enum match_direction_e
{
	MATCH_DIR_UNKNOWN = -1,
	MATCH_DIR_IN = 0,
	MATCH_DIR_OUT = 1
} match_direction_t;

typedef enum direction_e
{
	DIR_LEFT = 0,
	DIR_RIGHT = 1
} direction_t;

typedef enum catcierge_matcher_type_e
{
	MATCHER_TEMPLATE,
	MATCHER_HAAR
} catcierge_matcher_type_t;

typedef enum catcierge_lockout_method_s
{
	TIMER_ONLY_1 = 1,
	OBSTRUCT_THEN_TIMER_2 = 2,
	OBSTRUCT_OR_TIMER_3 = 3
} catcierge_lockout_method_t;

#define MAX_STEPS 24
#define MAX_MATCH_RECTS 24

typedef struct catcierge_path_s
{
	char full[2048];		// Directory + filename.
	char filename[1024];	// Filename.
	char dir[2048];			// Directory.
} catcierge_path_t;

typedef struct match_step_s
{
	IplImage *img;
	catcierge_path_t path;
	const char *name;
	const char *description;
} match_step_t;

// TODO: Maybe merge this with match_state_t.
// This struct is passed to the matcher algorithm.
typedef struct match_result_s
{
	double result;
	int success;
	char description[2048];
	CvRect match_rects[MAX_MATCH_RECTS];
	size_t rect_count;
	match_direction_t direction;
	match_step_t steps[MAX_STEPS];	// Step by step images+description for the matching algorithm.
	size_t step_img_count;			// The number of step images.
} match_result_t;

// The state of a single match.
typedef struct match_state_s
{
	catcierge_path_t path;			// Path info where to save the image.
	IplImage *img;					// A cached image of the match frame.
	struct timeval tv;
	time_t time;					// We need this on Windows. 
									// Since tv_sec in struct timeval is a long (32-bit) and time_t
									// might be a 64-bit integer.
	char time_str[1024];			// Time string of match (used in image filename).
	match_result_t result;			// Updated by the matcher algorithm.
	SHA1Context sha;				// Used to generate match ID.
} match_state_t;

typedef struct match_group_s
{
	SHA1Context sha;				// Used to generate match group ID.
	match_state_t matches[MATCH_MAX_COUNT];
	size_t match_count;				// The current match count, will go up to MATCH_MAX_COUNT.
	int success;
	int success_count;
	int final_decision;				// Was the match decision overriden by the matcher?
	char description[512];
	match_direction_t direction;
	
	struct timeval start_tv;
	time_t start_time;
	struct timeval end_tv;
	time_t end_time;

	IplImage *obstruct_img;
	catcierge_path_t obstruct_path;
	struct timeval obstruct_tv;
	time_t obstruct_time;
} match_group_t;

#endif // __CATCIERGE_TYPES_H__
