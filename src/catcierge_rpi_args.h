//
// This file is part of the Catcierge project.
//
// Copyright (c) Joakim Soderberg 2013-2016
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

#ifndef __CARGO_RPI_ARGS_H__
#define __CARGO_RPI_ARGS_H__

#include "catcierge_args.h"

void print_cam_help(cargo_t cargo);
char **catcierge_parse_rpi_args(int *argc, char **argv, catcierge_args_t *args);
int catcierge_parse_rpi_config(const char *config_path, catcierge_args_t *args);

#endif // __CARGO_RPI_ARGS_H__
