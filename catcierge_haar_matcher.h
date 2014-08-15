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
#ifndef __CATCIERGE_HAAR_MATCHER_H__
#define __CATCIERGE_HAAR_MATCHER_H__

#include <opencv2/imgproc/imgproc_c.h>
#include <stdio.h>
#include "catcierge_haar_wrapper.h"
#include "catcierge_types.h"

typedef enum catcierge_haar_prey_method_e
{
	PREY_METHOD_ADAPTIVE,
	PREY_METHOD_NORMAL
} catcierge_haar_prey_method_t;

typedef struct catcierge_haar_matcher_args_s
{
	const char *cascade;
	int min_width;
	int min_height;
	direction_t in_direction;
	int eq_histogram;
	int low_binary_thresh;
	int high_binary_thresh;
	int no_match_is_fail;
	catcierge_haar_prey_method_t prey_method;
	int prey_steps;
	int debug;
} catcierge_haar_matcher_args_t;

typedef struct catcierge_haar_matcher_s
{
	CvMemStorage *storage;
	IplConvKernel *kernel2x2;
	IplConvKernel *kernel3x3;
	IplConvKernel *kernel5x1;

	cv2CascadeClassifier *cascade;

	catcierge_haar_matcher_args_t *args;
	int debug;
} catcierge_haar_matcher_t;

typedef int (*find_prey_f)(catcierge_haar_matcher_t *ctx,
		IplImage *img, IplImage *inv_thr_img, match_result_t *result);

int catcierge_haar_matcher_init(catcierge_haar_matcher_t *ctx, catcierge_haar_matcher_args_t *args);
void catcierge_haar_matcher_destroy(catcierge_haar_matcher_t *ctx);
double catcierge_haar_matcher_match(catcierge_haar_matcher_t *ctx, IplImage *img, match_result_t *result);
void catcierge_haar_matcher_set_debug(catcierge_haar_matcher_t *ctx, int debug);

void catcierge_haar_matcher_usage();
int catcierge_haar_matcher_parse_args(catcierge_haar_matcher_args_t *args, const char *key, char **values, size_t value_count);
void catcierge_haar_matcher_args_init(catcierge_haar_matcher_args_t * args);
void catcierge_haar_matcher_print_settings(catcierge_haar_matcher_args_t *args);

#endif // __CATCIERGE_HAAR_MATCHER_H__
