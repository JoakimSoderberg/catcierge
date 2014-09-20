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
#ifndef __CATCIERGE_COMMON_MATCHER_H__
#define __CATCIERGE_COMMON_MATCHER_H__

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#include "catcierge_types.h"

typedef double (*catcierge_match_func_t)(void *ctx,
		IplImage *img, match_result_t *result, int save_steps);

typedef int (*catcierge_decide_func_t)(void *ctx, match_group_t *mg);

// TODO: Totally abstract the matchers into this file. External code should just have to ask for a matcher type. How to solve for the args parsing?
typedef struct catcierge_common_matcher_s
{
	catcierge_matcher_type_t type;
	catcierge_match_func_t match;
	catcierge_decide_func_t decide;
} catcierge_matcher_t;

typedef struct catcierge_matcher_args_s
{
	catcierge_matcher_type_t type;
} catcierge_matcher_args_t;

int catcierge_is_frame_obstructed(IplImage *img, int debug);

int catcierge_matcher_init(catcierge_matcher_t *ctx, catcierge_matcher_args_t *args);
void catcierge_matcher_destroy(catcierge_matcher_t *ctx);


#endif // __CATCIERGE_COMMON_MATCHER_H__
