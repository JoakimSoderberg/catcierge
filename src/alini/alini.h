/*
 * alini.h
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

#ifndef __ALINI_H__
#define __ALINI_H__

#include <stdio.h>

/* parser typedef */
typedef struct alini_parser_s alini_parser_t;

/* found key-value-pair callback */
typedef void (*alini_parser_foundkvpair_callback)(alini_parser_t *parser, char *section, char *key, char *value);

/* parser struct */
struct alini_parser_s {
	char on;
	char *path;
	char *activesection;
	FILE *file;
	alini_parser_foundkvpair_callback foundkvpair_callback;
	void *ctx;
};

/* create parser */
int alini_parser_create(alini_parser_t **parser, char *path);

/* set `found key-value pair` callback */
int alini_parser_setcallback_foundkvpair(alini_parser_t *parser, alini_parser_foundkvpair_callback callback);

/* set user context */
void alini_parser_set_context(alini_parser_t *parser, void *ctx);

/* get user context */
void *alini_parser_get_context(alini_parser_t *parser);

/* parse one step */
int alini_parser_step(alini_parser_t *parser);

/* parse entire file */
int alini_parser_start(alini_parser_t *parser);

/* halt parser */
void alini_parser_halt(alini_parser_t *parser);

/* dispose parser */
int alini_parser_dispose(alini_parser_t *parser);

#endif
