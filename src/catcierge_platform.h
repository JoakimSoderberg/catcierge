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
#ifndef __CATCIERGE_PLATFORM_H__
#define __CATCIERGE_PLATFORM_H__

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <direct.h>
#define _WINSOCKAPI_
#include <Windows.h>

typedef int mode_t;

#define PATH_MAX MAX_PATH
#define getcwd _getcwd

#define sleep(x) Sleep((DWORD)((x)*1000))
#if _MSC_VER < 1900
#define vsnprintf _vsnprintf
#define snprintf _snprintf
#endif
#define strcasecmp _stricmp 
#define strncasecmp _strnicmp 
#define gmtime_r(t, tm) gmtime_s(tm, t)
#define localtime_r(t, tm) localtime_s(tm, t)
#define strndup catcierge_strndup

#include "win32/gettimeofday.h"
#endif // _WIN32

#if (!defined (va_copy))
	#define va_copy(dest, src) (dest) = (src)
#endif

//  Add missing defines for non-POSIX systems
#ifndef S_IRUSR
#   define S_IRUSR S_IREAD
#endif
#ifndef S_IWUSR
#   define S_IWUSR S_IWRITE 
#endif
#ifndef S_ISDIR
#   define S_ISDIR(m) (((m) & S_IFDIR) != 0)
#endif
#ifndef S_ISREG
#   define S_ISREG(m) (((m) & S_IFREG) != 0)
#endif



#endif // __CATCIERGE_PLATFORM_H__
