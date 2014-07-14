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
#ifndef __CATCIERGE_OUTPUT_TYPES_H__
#define __CATCIERGE_OUTPUT_TYPES_H__

#include <stdio.h>

typedef struct catcierge_output_var_s
{
	char *name;
	char *description;
} catcierge_output_var_t;

typedef struct catcierge_output_template_s
{
	char *tmpl;
	char *target_path;		// Input target path.
	char *generated_path;	// The last generated path.
	char *name;
} catcierge_output_template_t;

typedef struct catcierge_output_s
{
	char *input_path;
	catcierge_output_template_t *templates;
	size_t template_count;
	size_t template_max_count;
} catcierge_output_t;

#endif // __CATCIERGE_OUTPUT_TYPES_H__
