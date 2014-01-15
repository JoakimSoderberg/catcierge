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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <direct.h>
#define _WINSOCKAPI_
#include <Windows.h>

#define PATH_MAX MAX_PATH
#define getcwd _getcwd
#define snprintf _snprintf
#endif // _WIN32

void catcierge_execute(char *command, char *fmt, ...);

#endif // __CATCIERGE_UTIL_H__
