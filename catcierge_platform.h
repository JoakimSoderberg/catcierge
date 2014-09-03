
#ifndef __CATCIERGE_PLATFORM_H__
#define __CATCIERGE_PLATFORM_H__

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
#define gmtime_r(t, tm) gmtime_s(tm, t)
#define localtime_r(t, tm) localtime_s(tm, t)

#include "win32/gettimeofday.h"
#endif // _WIN32


#endif // __CATCIERGE_PLATFORM_H__
