
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
#define snprintf _snprintf
#define sleep(x) Sleep((DWORD)((x)*1000))
#define vsnprintf _vsnprintf 
#define strcasecmp _stricmp 
#define strncasecmp _strnicmp 
#define gmtime_r(t, tm) gmtime_s(tm, t)
#define localtime_r(t, tm) localtime_s(tm, t)

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
