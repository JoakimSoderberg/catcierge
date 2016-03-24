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
#ifndef __CATCIERGE_TIMER_H__
#define __CATCIERGE_TIMER_H__

#include "catcierge_util.h"
#include <time.h>

#ifdef _WIN32
#include "win32/gettimeofday.h"
#else
#include <unistd.h>
#include <sys/time.h>
#endif

typedef struct catcierge_timer_s
{
	struct timeval start;
	struct timeval end;
	double timeout;
} catcierge_timer_t;

void catcierge_timer_reset(catcierge_timer_t *t);

int catcierge_timer_isactive(catcierge_timer_t *t);

void catcierge_timer_start(catcierge_timer_t *t);

double catcierge_timer_get(catcierge_timer_t *t);

void catcierge_timer_set(catcierge_timer_t *t, double timeout);

int catcierge_timer_has_timed_out(catcierge_timer_t *t);


#endif // __CATCIERGE_TIMER_H__
