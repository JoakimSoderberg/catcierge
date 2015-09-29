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
#include <catcierge_config.h>
#include <assert.h>
#include "catcierge_timer.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>

// TODO: Replace this with a monotonic timer version instead!
void catcierge_timer_reset(catcierge_timer_t *t)
{
	assert(t);
	memset(&t->start, 0, sizeof(struct timeval));
	memset(&t->end, 0, sizeof(struct timeval));
}

int catcierge_timer_isactive(catcierge_timer_t *t)
{
	assert(t);
	return (t->start.tv_sec != 0);
}

void catcierge_timer_start(catcierge_timer_t *t)
{
	assert(t);

	gettimeofday(&t->start, NULL);
	gettimeofday(&t->end, NULL);
}

double catcierge_timer_get(catcierge_timer_t *t)
{
	assert(t);
	if (t->start.tv_sec == 0)
		return 0.0;

	gettimeofday(&t->end, NULL);

	return (double)(t->end.tv_sec - t->start.tv_sec) +
			((t->end.tv_usec - t->start.tv_usec) / 1000000.0);
}

void catcierge_timer_set(catcierge_timer_t *t, double timeout)
{
	assert(t);
	t->timeout = timeout;
}

int catcierge_timer_has_timed_out(catcierge_timer_t *t)
{
	assert(t);
	return (round(catcierge_timer_get(t)) >= t->timeout);
}
