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
#ifndef __CATCIERGE_CARGO_H__
#define __CATCIERGE_CARGO_H__

#include "catcierge_args.h"

int catcierge_args_init(catcierge_args_t *args, const char *progname);
int catcierge_args_parse(catcierge_args_t *args, int argc, char **argv);
void catcierge_args_destroy(catcierge_args_t *args);

void catcierge_args_init_vars(catcierge_args_t *args);
void catcierge_args_destroy_vars(catcierge_args_t *args);

#endif // __CATCIERGE_CARGO_H__
