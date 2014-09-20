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
#ifndef __CATCIERGE_TEMPLATE_MATCHER_H__
#define __CATCIERGE_TEMPLATE_MATCHER_H__

#include <opencv2/imgproc/imgproc_c.h>
#include <stdio.h>
#include "catcierge_types.h"
#include "catcierge_common_matcher.h"

#define CATCIERGE_LOW_BINARY_THRESH_DEFAULT 90
#define CATCIERGE_HIGH_BINARY_THRESH_DEFAULT 255

#define CATCIERGE_DEFAULT_RESOLUTION_WIDTH 320
#define CATCIERGE_DEFUALT_RESOLUTION_HEIGHT 240
#define DEFAULT_MATCH_THRESH 0.8	// The threshold signifying a good match returned by catcierge_match.
#define MAX_SNOUT_COUNT 24

typedef struct catcierge_template_matcher_args_s
{
	const char *snout_paths[MAX_SNOUT_COUNT];
	size_t snout_count;
	double match_threshold;
	int match_flipped;
} catcierge_template_matcher_args_t;

typedef struct catcierge_template_matcher_s
{
	CvMemStorage *storage;
	int width;
	int height;
	IplImage **snouts;
	size_t snout_count;
	IplImage **flipped_snouts;
	IplConvKernel *kernel;
	IplImage **matchres;

	int match_flipped;
	double match_threshold;
	int low_binary_thresh;
	int high_binary_thresh;
	int erode;

	int debug;
	catcierge_template_matcher_args_t *args;
} catcierge_template_matcher_t;

int catcierge_template_matcher_init(catcierge_template_matcher_t *ctx, 
					catcierge_template_matcher_args_t *args);

void catcierge_template_matcher_set_match_flipped(catcierge_template_matcher_t *ctx, int match_flipped);
void catcierge_template_matcher_set_match_threshold(catcierge_template_matcher_t *ctx, double match_threshold);
void catcierge_template_matcher_set_binary_thresholds(catcierge_template_matcher_t *ctx, int low, int high);
void catcierge_template_matcher_set_erode(catcierge_template_matcher_t *ctx, int erode);
void catcierge_template_matcher_set_debug(catcierge_template_matcher_t *ctx, int debug);

void catcierge_template_matcher_destroy(catcierge_template_matcher_t *ctx);

double catcierge_template_matcher_match(catcierge_template_matcher_t *ctx, const IplImage *img, match_result_t *result, int save_steps);

int catcierge_template_matcher_is_frame_obstructed(catcierge_template_matcher_t *ctx, IplImage *img);

void catcierge_template_matcher_usage();
int catcierge_template_matcher_parse_args(catcierge_template_matcher_args_t *args, const char *key, char **values, size_t value_count);
void catcierge_template_matcher_print_settings(catcierge_template_matcher_args_t * args);
void catcierge_template_matcher_args_init(catcierge_template_matcher_args_t *args);

#endif // __CATCIERGE_TEMPLATE_MATCHER_H__
