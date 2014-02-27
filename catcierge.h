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
#ifndef __CATCIERGE_H__
#define __CATCIERGE_H__

#include <opencv2/imgproc/imgproc_c.h>

#define CATCIERGE_LOW_BINARY_THRESH_DEFAULT 90
#define CATCIERGE_HIGH_BINARY_THRESH_DEFAULT 255

#define CATCIERGE_DEFAULT_RESOLUTION_WIDTH 320
#define CATCIERGE_DEFUALT_RESOLUTION_HEIGHT 240

typedef struct catcierge_s
{
	CvMemStorage *storage;
	int width;
	int height;
	IplImage **snouts;
	int snout_count;
	IplImage **flipped_snouts;
	IplConvKernel *kernel;
	IplImage **matchres;

	int match_flipped;
	double match_threshold;
	int low_binary_thresh;
	int high_binary_thresh;
	int erode;

	int debug;
} catcierge_t;

int catcierge_init(catcierge_t *ctx, 
					const char **snout_paths, int snout_count);

void catcierge_set_match_flipped(catcierge_t *ctx, int match_flipped);
void catcierge_set_match_threshold(catcierge_t *ctx, double match_threshold);
void catcierge_set_binary_thresholds(catcierge_t *ctx, int low, int high);
void catcierge_set_erode(catcierge_t *ctx, int erode);
void catcierge_set_debug(catcierge_t *ctx, int debug);

void catcierge_destroy(catcierge_t *ctx);

double catcierge_match(catcierge_t *ctx, const IplImage *img, 
						CvRect *match_rects, size_t rect_count, 
						int *flipped);

int catcierge_is_matchable(catcierge_t *ctx, IplImage *img);

#endif // __CATCIERGE_H__
