//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2014
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

#define MATCH_MAX_COUNT 4 // The number of matches to perform before deciding the lock state.

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
	OBSTRUCT_OR_TIMER_1 = 1,
	OBSTRUCT_THEN_TIMER_2 = 2,
	TIMER_ONLY_3 = 3
} catcierge_lockout_method_t;

#define MAX_STEPS 24
#define MAX_MATCH_RECTS 24

typedef struct match_step_s
{
	IplImage *img;
	char path[1024];
	const char *name;
	const char *description;
} match_step_t;

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
	char path[1024];				// Path to where the image for this match should be saved.
	IplImage *img;					// A cached image of the match frame.
	time_t time;					// The time of match.
	char time_str[1024];			// Time string of match (used in image filename).
	struct timeval tv;
	match_result_t result;
} match_state_t;

#endif // __CATCIERGE_TYPES_H__
