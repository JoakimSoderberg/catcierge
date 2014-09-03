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
#include <catcierge_config.h>
#include <assert.h>
#include "catcierge_strftime.h"
#include "catcierge_platform.h"
#ifdef _WIN32
#include <crtdbg.h>  // For _CrtSetReportMode
#endif
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#if _WIN32
#include <crtdbg.h>  // For _CrtSetReportMode

static void _catcierge_invalid_param_handler(const wchar_t *expression,
	const wchar_t* function,
	const wchar_t* file,
	unsigned int line,
	uintptr_t pReserved)
{
	// Do nothing!
}
#endif // _WIN32

int catcierge_strftime(char *dst, size_t dst_len, const char *fmt, const struct tm *tm, struct timeval *tv)
{
	int ret = -1;
	char *tmp_fmt = NULL;

	#if _WIN32
	// As described in the documentation for strftime on windows:
	//   http://msdn.microsoft.com/fr-fr/library/fe06s4ak(v=vs.80).aspx
	//
	// If the formatting is invalid, the "security-enhanced CRT functions" are made to
	// do some extra parameter validation which can raise an exception and abort the program:
	// http://msdn.microsoft.com/fr-fr/library/ksazx244(v=vs.80).aspx
	//
	// We don't want this obviously and need to disable this:
	// http://msdn.microsoft.com/en-us/library/a9yf33zb.aspx
	//

	// Define our own paramter validation handler that does nothing.
	_set_invalid_parameter_handler(_catcierge_invalid_param_handler);

	// Disable the message box for assertions.
	_CrtSetReportMode(_CRT_ASSERT, 0);

	#endif // _WIN32

	// Add millisecond formatting (uses %f as python).
	if (tv)
	{
		size_t i;
		size_t j;
		int val_len;
		size_t len = strlen(fmt) + 1;
		size_t tmp_len = 2 * len;
		int millisec = tv->tv_usec / 1000;

		if (!(tmp_fmt = malloc(sizeof(char) * tmp_len)))
		{
			return -1;
		}

		i = 0;
		j = 0;
		while (i < len)
		{
			if (!strncmp(&fmt[i], "%f", 2))
			{
				while (1)
				{
					val_len = snprintf(&tmp_fmt[j], tmp_len - j - 1, "%ld", (long int)millisec);

					if ((val_len < 0))
					{
						goto fail;
					}
					else if ((size_t)val_len >= tmp_len)
					{
						// Truncated expand the buffer.
						tmp_len *= 2;

						if (!(tmp_fmt = realloc(tmp_fmt, tmp_len)))
						{
							goto fail;
						}
					}
					else
					{
						break;
					}
				}

				j += val_len; // Skip the expanded value.
				i += 2; // Skip the %f
			}
			else
			{
				tmp_fmt[j++] = fmt[i++];
			}
		}

		tmp_fmt[j] = '\0';

		fmt = tmp_fmt;
	}

	ret = strftime(dst, dst_len, fmt, tm);

fail:
	if (tmp_fmt)
	{
		free(tmp_fmt);
	}

	return ret;
}
