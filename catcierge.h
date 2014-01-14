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

typedef struct catcierge_s
{
	CvMemStorage* storage;
	IplImage* snout;
	IplImage* flipped_snout;
	IplConvKernel *kernel;

	int match_flipped;
	float match_threshold;
} catcierge_t;

int catcierge_init(catcierge_t *ctx, const char *snout_path, int match_flipped, float match_threshold);

void catcierge_destroy(catcierge_t *ctx);

double catcierge_match(catcierge_t *ctx, const IplImage *img, CvRect *match_rect, int *flipped);

int catcierge_is_matchable(catcierge_t *ctx, IplImage *img);

#endif // __CATCIERGE_H__
