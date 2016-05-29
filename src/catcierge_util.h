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
#ifndef __CATCIERGE_UTIL_H__
#define __CATCIERGE_UTIL_H__

#include "catcierge_types.h"
#include <time.h>
#include "catcierge_platform.h"
#include <stdio.h>

void catcierge_execute(char *command, char *fmt, ...);
void catcierge_reset_cursor_position();

int catcierge_make_path(const char *pathname, ...);

const char *catcierge_get_direction_str(match_direction_t dir);
const char *catcierge_get_left_right_str(direction_t dir);

const char *catcierge_skip_whitespace(const char *it);
char *catcierge_skip_whitespace_alt(char *it);
char **catcierge_parse_list(const char *input, size_t *list_count, int end_trim);
void catcierge_free_list(char **list, size_t count);
void catcierge_run(char *command);
const char *catcierge_path_sep();
char *catcierge_get_abs_path(const char *path, char *buf, size_t buflen);

void catcierge_xfree(void *p);

void print_line(FILE *fd, int length, const char *s);

char *catcierge_read_file(const char *filename);

char *catcierge_relative_path(const char *pfrom, const char *pto);

#endif // __CATCIERGE_UTIL_H__
