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
//    Catsnatch is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Catsnatch.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef __CATSNATCH_LOG_H__
#define __CATSNATCH_LOG_H__

#include <stdio.h>

char *get_time_str_fmt(char *time_str, size_t len, const char *fmt);
char *get_time_str(char *time_str, size_t len);
void log_print(FILE *fd, const char *fmt, ...);
void log_print_csv(FILE *fd, const char *fmt, ...);

#define CATLOG(fmt, ...) log_print(stdout, fmt, ##__VA_ARGS__)
#define CATERR(fmt, ...) log_print(stderr, fmt, ##__VA_ARGS__)

#endif // __CATSNATCH_LOG_H__
