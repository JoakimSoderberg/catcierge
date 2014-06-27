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
#ifndef __CATCIERGE_UTIL_H__
#define __CATCIERGE_UTIL_H__

#include "catcierge_types.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <direct.h>
#define _WINSOCKAPI_
#include <Windows.h>

#define PATH_MAX MAX_PATH
#define getcwd _getcwd
#define snprintf _snprintf
#define sleep(x) Sleep((DWORD)((x)*1000))
#define vsnprintf _vsnprintf 
#define strcasecmp _stricmp 
#define strncasecmp _strnicmp 
#endif // _WIN32
#include <time.h>

void catcierge_execute(char *command, char *fmt, ...);
void catcierge_reset_cursor_position();

int catcierge_make_path(const char *path);

const char *catcierge_get_direction_str(match_direction_t dir);
int catcierge_is_frame_obstructed(IplImage *img, int debug);

int catcierge_strftime(char *dst, size_t dst_len, const char *fmt, const struct tm *tm);

#endif // __CATCIERGE_UTIL_H__
