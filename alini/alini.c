/*
 * alini.c
 *
 * Copyright (c) 2003-2009 Denis Defreyne
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "alini.h"

/* strips whitespace at beginning and end of string */
static char *stripws(char *str, size_t len)
{
	int i = 0;
	int j = 0;
	char *newstr;

	/* find whitespace at start of string */
	for(;isspace(str[i]);i++) ;

	/* find whitespace at end of string */
	for(j = len - 1; j > 0 && isspace(str[j]); j--) ;
	++j;

	/* make new string */
	newstr = (char *)calloc(j-i+1, sizeof(char));
	if(!newstr) return NULL;
	memcpy(newstr, str+i, j-i);
	newstr[j-i] = '\0';

	return newstr;
}

/* create parser */
int alini_parser_create(alini_parser_t **parser, char *path)
{
	assert(parser);
	assert(path);
	
	/* allocate new parser */
	*parser = (alini_parser_t *)calloc(1, sizeof(alini_parser_t));
	if(!(*parser)) return -1;
	
	/* init */
	(*parser)->activesection = NULL;
	(*parser)->on = 1;
	
	/* copy path */
	(*parser)->path = strdup(path);
	
	/* open file */
	(*parser)->file = fopen(path, "r");
	if(!(*parser)->file) return -1;
	
	return 0;
}

/* set `found key-value pair` callback */
int alini_parser_setcallback_foundkvpair(alini_parser_t *parser, alini_parser_foundkvpair_callback callback)
{
	assert(parser);
	
	/* set callback */
	parser->foundkvpair_callback = callback;
	
	return 0;
}

void alini_parser_set_context(alini_parser_t *parser, void *ctx)
{
	assert(parser);
	parser->ctx = ctx;
}

void *alini_parser_get_context(alini_parser_t *parser)
{
	assert(parser);
	return parser->ctx;
}

/* parse one step */
int alini_parser_step(alini_parser_t *parser)
{
	char		line[1025];
	char		*tmpline;
	unsigned	len;
	unsigned	linenumber			= 0;
	char		signisfound			= 0;
	char		sectionhdrisfound	= 0;
	char		iswspace;
	unsigned	i;
	unsigned	j;
	char		*key				= NULL;
	char		*value				= NULL;
	
	assert(parser);
	
	while(1)
	{
		/* get a line */
		if(!fgets(line, 1024, parser->file))
			return -1;
		linenumber++;
		
		/* skip comments and empty lines */
		if(line[0] == '#' || line[0] == ';' || line[0] == '\n' || line[0] == '\r')
			continue;
		
		/* skip lines containing whitespace */
		len = strlen(line);
		iswspace = 1;
		for(j = 0 ; j < len && iswspace; j++)
		{
			if(!isspace(line[j]) && line[j] !='\n' && line[j] !='\r')
				iswspace = 0;
		}
		if(iswspace) continue;
		
		/* search '[...]' */
		sectionhdrisfound = 0;
		tmpline = stripws(line, strlen(line));
		len = strlen(tmpline);
		if(len > 2)
		{
			if(tmpline[0] == '[')
			{
				if(tmpline[len-1] == ']')
				{
					sectionhdrisfound = 1;
					if(parser->activesection) free(parser->activesection);
					parser->activesection = stripws(tmpline + 1, strlen(tmpline) - 2);
				}
				else
				{
					fprintf(stderr, "alini: parse error at %s:%d: end token `]' not found", parser->path, linenumber);
					return -1;
				}
			}
		}
		free(tmpline);
		
		if(!sectionhdrisfound)
		{
			/* search '=' */
			signisfound = 0;
			for(i = 0; i < len && !signisfound; i++)
			{
				if(line[i] != '=') continue;
				
				signisfound = 1;
			}
			if(!signisfound)
			{
				fprintf(stderr, "alini: parse error at %s:%d: token `=' not found", parser->path, linenumber);
				return -1;
			}
			
			/* trim key and value */
			key = stripws(line, i - 1);
			value = stripws(line + i, strlen(line) - i - 1);
			
			/* call callback */
			parser->foundkvpair_callback(parser, parser->activesection, key, value);
			
			/* cleanup */
			free(key);
			free(value);
			
			break;
		}
	}
	
	return 0;
}

/* parse entire file */
int alini_parser_start(alini_parser_t *parser)
{
	assert(parser);
	
	while(parser->on && alini_parser_step(parser) == 0)
		;
	
	return 0;
}

/* halt parser */
void alini_parser_halt(alini_parser_t *parser)
{
	assert(parser);
	
	parser->on = 0;
}

/* dispose parser */
int alini_parser_dispose(alini_parser_t *parser)
{
	assert(parser);
	
	/* close file */
	if (parser->file)
		fclose(parser->file);

	if (parser->path)
		free(parser->path);
	
	/* free parser */
	free(parser);
	
	return 0;
}
