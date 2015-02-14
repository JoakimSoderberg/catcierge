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
#include "catcierge_types.h"

#define CATCIERGE_OUTPUT_MAX_RECURSION 10

typedef struct catcierge_output_settings_s
{
	char **event_filter;
	size_t event_filter_count;
	int nofile;
	char *filename;
	#ifdef WITH_ZMQ
	char *topic; // ZMQ topic name, defaults to template name.
	int nozmq;
	#endif
} catcierge_output_settings_t;

typedef struct catcierge_output_template_s
{
	char *tmpl;
	char *generated_path;	// The last generated path.
	char *name;
	catcierge_output_settings_t settings;
} catcierge_output_template_t;

typedef struct catcierge_output_s
{
	char *input_path;
	catcierge_output_template_t *templates;
	size_t template_count;
	size_t template_max_count;
	int recursion;
	int recursion_error;
} catcierge_output_t;

#endif // __CATCIERGE_OUTPUT_TYPES_H__
