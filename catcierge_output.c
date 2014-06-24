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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "catcierge_util.h"
#include "catcierge_types.h"
#include "catcierge_output.h"

catcierge_output_var_t vars[] =
{
	{ "match_success", "Match success status."},
	{ "match#_path", "Image path for match #." },
	{ "match#_path", "Image path for match #." },
	{ "match#_success", "Success status for match #." },
	{ "match#_direction", "Direction for match #." },
	{ "match#_result", "Result for match #." }
};

int catcierge_output_init(catcierge_output_t *ctx)
{
	assert(ctx);

	return 0;
}

void catcierge_output_destroy(catcierge_output_t *ctx)
{
	assert(ctx);
}

int catcierge_output_add_template(catcierge_output_t *ctx,
		const char *template_str, char *target_path)
{
	assert(ctx);

	return 0;
}

static const char *catcierge_output_translate(catcierge_grb_t *grb,
	char *buf, size_t bufsize, char *var)
{
	if (!strcmp(var, "state"))
	{
		return catcierge_get_state_string(grb->state);
	}

	if (!strcmp(var, "match_success"))
	{
		snprintf(buf, bufsize - 1, "%d", grb->match_success);
		return buf;
	}

	if (!strncmp(var, "match", 5))
	{
		int idx = -1;
		char *subvar = var + strlen("matchX_");

		if (sscanf(var, "match%d_", &idx) == EOF)
		{
			return NULL;
		}

		if ((idx < 0) || (idx > MATCH_MAX_COUNT))
		{
			return NULL;
		}

		if (!strcmp(subvar, "path"))
		{
			return grb->matches[idx].path;
		}
		else if (!strcmp(subvar, "success"))
		{
			snprintf(buf, bufsize - 1, "%d", grb->matches[idx].success);
			return buf;
		}
		else if (!strcmp(subvar, "direction"))
		{
			return catcierge_get_direction_str(grb->matches[idx].direction);
		}
		else if (!strcmp(subvar, "result"))
		{
			snprintf(buf, bufsize - 1, "%f", grb->matches[idx].result);
			return buf; 
		}
	}

	return NULL;
}

char *catcierge_output_generate(catcierge_output_t *ctx, catcierge_grb_t *grb,
		const char *template_str)
{
	char buf[1024];
	char *s;
	char *it;
	char *output = NULL;
	char *tmp;
	size_t orig_len = strlen(template_str);
	size_t out_len = 2 * orig_len;
	size_t len;
	size_t i;
	size_t linenum;
	assert(ctx);
	assert(grb);

	if (!(output = malloc(out_len)))
	{
		return NULL;
	}

	if (!(tmp = strdup(template_str)))
	{
		free(output);
		return NULL;
	}

	len = 0;
	linenum = 0;
	it = tmp;

	while (*it)
	{
		if (*it == '\n')
		{
			linenum++;
		}

		// Replace any variables signified by %varname%
		if (*it == '%')
		{
			const char *res;
			it++;

			// %% means a literal %
			if (*it && (*it == '%'))
			{
				output[len] = *it++;
				len++;
				continue;
			}

			// Save position of beginning of var name.
			s = it;

			// Look for the ending %
			while (*it && (*it != '%'))
			{
				it++;
			}

			// Either we found it or the end of string.
			if (*it != '%')
			{
				fprintf(stderr, "Variable not terminated in output template line %d\n", (int)linenum);
				free(output);
				output = NULL;
				goto fail;
			}

			// Terminate so we get the var name in a nice comparable string.
			*it++ = '\0';

			// Find the value of the variable and append it to the output.
			if ((res = catcierge_output_translate(grb, buf, sizeof(buf), s)))
			{
				size_t reslen = strlen(res);

				// Make sure we have enough room.
				while ((len + reslen) >= out_len)
				{
					out_len *= 2;

					if (!(output = realloc(output, out_len)))
					{
						fprintf(stderr, "Out of memory\n");
						return NULL;
					}
				}

				// Append ...
				while (*res)
				{
					output[len] = *res++;
					len++;
				}
			}
			else
			{
				fprintf(stderr, "Unknown template variable \"%s\"\n", s);
			}
		}
		else
		{
			output[len] = *it++;
			len++;
		}
	}

	output[len] = '\0';

fail:
	if (tmp)
		free(tmp);

	return output;
}

int catcierge_output_generate_templates(catcierge_output_t *ctx, catcierge_grb_t *grb)
{
	assert(ctx);
	assert(grb);

	return 0;
}
