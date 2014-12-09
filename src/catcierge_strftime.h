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
#ifndef __CATCIERGE_STRFTIME_H__
#define __CATCIERGE_STRFTIME_H__

#include <time.h>
#include <stdlib.h>

int catcierge_strftime(char *dst, size_t dst_len, const char *fmt, const struct tm *tm, struct timeval *tv);

void catcierge_strftime_set_base_diff(long time_diff);

#endif // __CATCIERGE_STRFTIME_H__
