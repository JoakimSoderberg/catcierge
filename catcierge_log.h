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
#ifndef __CATCIERGE_LOG_H__
#define __CATCIERGE_LOG_H__

#include <stdio.h>
#include "catcierge_color.h"

#include <time.h>

extern int catcierge_nocolor;

char *get_time_str_fmt(char *time_str, size_t len, const char *fmt);
char *get_time_str(char *time_str, size_t len);
void log_printc(FILE *fd, enum catcierge_color_e print_color, const char *fmt, ...);
void log_print_csv(FILE *fd, const char *fmt, ...);
void log_printf(FILE *fd, enum catcierge_color_e print_color, const char *fmt, ...);

#define log_print(fd, fmt, ...) log_printc(fd, COLOR_NORMAL, fmt, ##__VA_ARGS__)
#define CATLOG(fmt, ...) log_printc(stdout, COLOR_NORMAL, fmt, ##__VA_ARGS__)
#define CATERR(fmt, ...) log_printc(stderr, COLOR_RED, fmt, ##__VA_ARGS__)

#endif // __CATCIERGE_LOG_H__
