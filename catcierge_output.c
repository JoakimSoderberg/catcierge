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

#include "catcierge_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef CATCIERGE_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef CATCIERGE_HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef CATCIERGE_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "catcierge_log.h"
#include "catcierge_util.h"
#include "catcierge_types.h"
#include "catcierge_output.h"
#include "catcierge_output_types.h"
#include "catcierge_fsm.h"

catcierge_output_var_t vars[] =
{
	{ "match_success", "Match success status."},
	{ "match#_path", "Image path for match #." },
	{ "match#_path", "Image path for match #." },
	{ "match#_success", "Success status for match #." },
	{ "match#_direction", "Direction for match #." },
	{ "match#_result", "Result for match #." },
	{ "match#_time", "Time of match" },
	{ "time", "The curent time when generating template." },
	{
		"time:<fmt>",
		"The time using the given format string"
		"using strftime formatting (replace % with @)."
		#if _WIN32
		" Note that Windows only supports a subset of formatting characters."
		#endif // _WIN32
	}
};

void catcierge_output_print_usage()
{
	size_t i;

	fprintf(stderr, "Output template variables:\n");

	for (i = 0; i < sizeof(vars) / sizeof(vars[0]); i++)
	{
		fprintf(stderr, "%20s   %s\n", vars[i].name, vars[i].description);
	}
}

int catcierge_output_init(catcierge_output_t *ctx)
{
	assert(ctx);
	memset(ctx, 0, sizeof(catcierge_output_t));
	ctx->template_max_count = 10;

	if (!(ctx->templates = calloc(ctx->template_max_count,
		sizeof(catcierge_output_template_t))))
	{
		CATERR("Out of memory\n");
		return -1;
	}

	return 0;
}

void catcierge_output_destroy(catcierge_output_t *ctx)
{
	catcierge_output_template_t *t;
	assert(ctx);

	if (ctx->templates)
	{
		size_t i;

		for (i = 0; i < ctx->template_count; i++)
		{
			t = &ctx->templates[i];
			if (t->target_path) free(t->target_path);
			t->target_path = NULL;
			if (t->tmpl) free(t->tmpl);
			t->tmpl = NULL;
		}

		free(ctx->templates);
		ctx->templates = NULL;
	}

	ctx->template_count = 0;
	ctx->template_max_count = 0;
}

int catcierge_output_add_template(catcierge_output_t *ctx,
		const char *template_str, const char *target_path)
{
	const char *path;
	catcierge_output_template_t *t;
	assert(ctx);

	if ((path = strrchr(target_path, '/')))
	{
		target_path = path + 1;
	}

	// Grow the templates array if needed.
	if (ctx->template_max_count < (ctx->template_count + 1))
	{
		ctx->template_max_count *= 2;

		if (!(ctx->templates = realloc(ctx->templates,
			ctx->template_max_count * sizeof(catcierge_output_template_t))))
		{
			CATERR("Out of memory!\n");
			return -1;
		}
	}

	t = &ctx->templates[ctx->template_count];

	if (!(t->target_path = strdup(target_path)))
	{
		CATERR("Out of memory!\n");
		return -1;
	}

	if (!(t->tmpl = strdup(template_str)))
	{
		free(t->target_path);
		t->target_path = NULL;
		CATERR("Out of memory!\n");
		return -1;
	}

	ctx->template_count++;

	return 0;
}

static char *catcierge_replace_time_format_char(char *fmt)
{
	char *s;
	s = fmt;

	while (*s)
	{
		if (*s == '@')
			*s = '%';
		s++;
	}

	return fmt;
}

static char *catcierge_get_time_var_format(char *var,
	char *buf, size_t bufsize, const char *default_fmt, time_t t)
{
	int ret;
	char *fmt = NULL;
	char *var_fmt = var + 4;
	assert(!strncmp(var, "time", 4));

	if (*var_fmt == ':')
	{
		char *s = NULL;
		var_fmt++;

		if (!(fmt = strdup(var_fmt)))
		{
			CATERR("Out of memory!\n");
			return NULL;
		}

		catcierge_replace_time_format_char(fmt);
	}
	else
	{
		if (!(fmt = strdup(default_fmt)))
		{
			CATERR("Out of memory!\n");
			return NULL;
		}
	}

	ret = catcierge_strftime(buf, bufsize - 1, fmt, localtime(&t));

	if (!ret)
	{
		CATERR("Invalid time formatting string \"%s\"\n", fmt);
		buf = NULL;
	}

	if (fmt)
	{
		free(fmt);
	}

	return buf;
}

static const char *catcierge_output_translate(catcierge_grb_t *grb,
	char *buf, size_t bufsize, char *var)
{
	// Current time.
	if (!strncmp(var, "time", 4))
	{
		return catcierge_get_time_var_format(var, buf, bufsize,
			"%Y-%m-%d %H:%M:%S", time(NULL));
	}

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
		else if (!strncmp(subvar, "time", 4))
		{
			return catcierge_get_time_var_format(subvar, buf, bufsize,
					"%Y-%m-%d %H:%M:%S", grb->matches[idx].time);
		}
	}

	return NULL;
}

static char *catcierge_output_generate_ex(catcierge_output_t *ctx,
	catcierge_grb_t *grb, const char *template_str)
{
	char buf[4096];
	char *s;
	char *it;
	char *output = NULL;
	char *tmp = NULL;
	size_t orig_len = strlen(template_str);
	size_t out_len = 2 * orig_len;
	size_t len;
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
				CATERR("Variable not terminated in output template line %d\n", (int)linenum);
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
						CATERR("Out of memory\n");
						goto fail;
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
				CATERR("Unknown template variable \"%s\"\n", s);
				free(output);
				output = NULL;
				goto fail;
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

char *catcierge_output_generate(catcierge_output_t *ctx, catcierge_grb_t *grb,
		const char *template_str)
{
	return catcierge_output_generate_ex(ctx, grb, template_str);
}

int catcierge_output_validate(catcierge_output_t *ctx,
	catcierge_grb_t *grb, const char *template_str)
{
	int is_valid = 0;
	char *output = catcierge_output_generate_ex(ctx, grb, template_str);
	is_valid = (output != NULL);

	if (output)
		free(output);

	return is_valid;
}

static char *catcierge_replace_whitespace(char *path, char *extra_chars)
{
	char *p = path;
	char *e = NULL;

	while (*p)
	{
		if ((*p == ' ') || (*p == '\t') || (*p == '\n'))
		{
			*p = '_';
		}

		if (extra_chars)
		{
			e = extra_chars;
			while (*e)
			{
				if (*p == *e)
				{
					*p = '_';
				}

				e++;
			}
		}

		p++;
	}

	return 0;
}

int catcierge_output_generate_templates(catcierge_output_t *ctx, catcierge_grb_t *grb)
{
	catcierge_output_template_t *t = NULL;
	char *output = NULL;
	char *path = NULL;
	char full_path[4096];
	char *dir = NULL;
	size_t i;
	FILE *f = NULL;
	assert(ctx);
	assert(grb);

	for (i = 0; i < ctx->template_count; i++)
	{
		t = &ctx->templates[i];

		printf("Templates %s\n", t->tmpl);

		// Generate the template.
		if (!(output = catcierge_output_generate_ex(ctx, grb, t->tmpl)))
		{
			CATERR("Failed to generate output for template \"%s\"\n", t->target_path);
			return -1;
		}

		// And the target path.
		if (!(path = catcierge_output_generate_ex(ctx, grb, t->target_path)))
		{
			CATERR("Failed to generate output path for template \"%s\"\n", t->target_path);
			free(output);
			return -1;
		}

		// Replace whitespace with underscore.
		catcierge_replace_whitespace(path, ":");

		// TODO: Get the path char in a nicer way...
		// Assemble the full output path.
		snprintf(full_path, sizeof(full_path), "%s%s%s",
			grb->args.output_path,
			#ifdef _WIN32
			"\\",
			#else
			"/",
			#endif
			path);

		if (!(f = fopen(full_path, "w")))
		{
			CATERR("Failed to open template output file \"%s\" for writing\n", full_path);
		}
		else
		{
			size_t len = strlen(output);
			size_t written = fwrite(output, sizeof(char), len, f);
			fclose(f);
		}

		free(output);
		free(path);
	}

	return 0;
}

int catcierge_output_load_templates(catcierge_output_t *ctx,
		char **inputs, size_t input_count)
{
	int ret = 0;
	size_t i;
	size_t fsize;
	FILE *f = NULL;
	char *contents = NULL;

	if (input_count > 0)
	{
		CATLOG("Loading output templates:\n");
	}

	for (i = 0; i < input_count; i++)
	{
		if (!(f = fopen(inputs[i], "r")))
		{
			CATERR("Failed to open input template file \"%s\"\n", inputs[i]);
			ret = -1;
			goto fail;
		}

		// Get file size.
		fseek(f, 0, SEEK_END);
		fsize = ftell(f);
		rewind(f);

		if (!(contents = malloc(fsize)))
		{
			CATERR("Out of memory!\n");
			ret = -1;
			goto fail;
		}

		if (fread(contents, 1, fsize, f) != fsize)
		{
			CATERR("Failed to read file contents of template file \"%s\"\n", inputs[i]);
			ret = -1;
			goto fail;
		}

		CATLOG("  %s - %d bytes\n", inputs[i], fsize);

		if (catcierge_output_add_template(ctx, contents, inputs[i]))
		{
			CATERR("Failed to load template file \"%s\"\n", inputs[i]);
			ret = -1;
			goto fail;
		}

		free(contents);
		contents = NULL;
	}

fail:
	if (contents)
	{
		free(contents);
		contents = NULL;
	}

	return ret;
}

