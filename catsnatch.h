//
// This file is part of the Catsnatch project.
//
// Copyright (c) Joakim Soderberg 2013-2014
//
//    Catsnatch is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//    Foobar is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef __CATSNATCH_H__
#define __CATSNATCH_H__

#include <opencv2/imgproc/imgproc_c.h>

typedef struct catsnatch_s
{
	CvMemStorage* storage;
	IplImage* snout;
	IplConvKernel *kernel;
} catsnatch_t;

int catsnatch_init(catsnatch_t *ctx, const char *snout_path);

void catsnatch_destroy(catsnatch_t *ctx);

double catsnatch_match(catsnatch_t *ctx, const IplImage *img, CvRect *match_rect);

int catsnatch_is_matchable(catsnatch_t *ctx, IplImage *img);

#endif // __CATSNATCH_H__
