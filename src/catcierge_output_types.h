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
#ifndef __CATCIERGE_OUTPUT_TYPES_H__
#define __CATCIERGE_OUTPUT_TYPES_H__

#include <stdio.h>
#include "catcierge_types.h"
#include "uthash.h"

#define CATCIERGE_OUTPUT_MAX_RECURSION 20
#define CATCIERGE_OUTPUT_MAX_VAR_LENGTH 32

typedef struct catcierge_output_settings_s
{
	char *name;
	char **event_filter;
	size_t event_filter_count;
	int nofile;
	char *filename;
	char *rootpath; // Path all templates are relative to. Default is cwd.
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

typedef struct catcierge_output_invar_s
{
	char name[CATCIERGE_OUTPUT_MAX_VAR_LENGTH];
	char *value;
	UT_hash_handle hh;
} catcierge_output_invar_t;

typedef struct catcierge_output_s
{
	char *input_path;
	catcierge_output_template_t *templates;
	size_t template_count;
	size_t template_max_count;
	int template_idx; // Index of template currently being parsed.
	int recursion;
	int recursion_error;
	int no_relative_path; // Flag to turn off relative_path calculations when
						  // running catcierge_output_generate when generating
						  // relative paths :)
	catcierge_output_invar_t *vars; // Hash table.
} catcierge_output_t;

#endif // __CATCIERGE_OUTPUT_TYPES_H__
