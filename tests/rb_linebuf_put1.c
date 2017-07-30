/*
 *  rb_linebuf_put1.c: Test rb_linebuf_put* under various conditions
 *  Copyright 2017 Simon Arlott
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tap/basic.h"

#include "stdinc.h"
#include "ircd_defs.h"
#include "client.h"
#include "rb_lib.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

struct Client me;
static char tmp[2048];
static const char text[] =
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
;
char long_tags[TAGSLEN + 1];
char long_prefix[TAGSLEN + DATALEN + 1];
char long_prefixf[TAGSLEN + DATALEN + 1];

#define MKTEXT(n) &text[sizeof(text) - ((n) + 1)]

static void _rb_linebuf_put_vtags_prefix(buf_head_t *bufhead, size_t prefix_buflen, const char *prefix, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	rb_linebuf_put_vtags_prefix(bufhead, format, &args, prefix_buflen, prefix);
	va_end(args);
}

static void _rb_linebuf_put_vtags_prefixf(buf_head_t *bufhead, size_t prefix_buflen, const char *prefix, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	rb_linebuf_put_vtags_prefixf(bufhead, format, &args, prefix_buflen, prefix, 300);
	va_end(args);
}

static void _rb_linebuf_put_vmsg(buf_head_t *bufhead, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	rb_linebuf_put_vmsg(bufhead, format, &args);
	va_end(args);
}

static void _rb_linebuf_put_vmsg_prefixf(buf_head_t *bufhead, const char *prefix, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	rb_linebuf_put_vmsg_prefixf(bufhead, format, &args, prefix, 300);
	va_end(args);
}


#define CRLF "\r\n"
#define SOME_TAGS "@tag1=value1;tag2=value2 "

static void basic_vtags_prefix1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefix(&linebuf, strlen(SOME_TAGS) + DATALEN + 1, SOME_TAGS ":prefix ", "test %s %d", "TEST", 42);
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);
	is_string(SOME_TAGS ":prefix test TEST 42" CRLF, output, MSG);
}

static void long_vtags_prefix1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefix(&linebuf, strlen(SOME_TAGS) + DATALEN + 1, SOME_TAGS ":prefix ", "%s", MKTEXT(502));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, SOME_TAGS ":prefix ");
	strcat(tmp, MKTEXT(502));
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(SOME_TAGS) + DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vtags_prefix1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefix(&linebuf, strlen(SOME_TAGS) + DATALEN + 1, SOME_TAGS ":prefix ", "%s", MKTEXT(503));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, SOME_TAGS ":prefix ");
	strcat(tmp, MKTEXT(503));
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(SOME_TAGS) + DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void long_vtags_prefix2(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	strcpy(prefix, long_tags);
	strcat(prefix, ":prefix ");

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefix(&linebuf, strlen(long_tags) + DATALEN + 1, prefix, "%s", MKTEXT(502));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, long_tags);
	strcat(tmp, ":prefix ");
	strcat(tmp, MKTEXT(502));
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(long_tags) + DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vtags_prefix2(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	strcpy(prefix, long_tags);
	strcat(prefix, ":prefix ");

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefix(&linebuf, strlen(prefix) + DATALEN + 1, prefix, "%s", MKTEXT(503));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, long_tags);
	strcat(tmp, ":prefix ");
	strcat(tmp, MKTEXT(503));
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(long_tags) + DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void long_vtags_prefix3(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	strcpy(prefix, long_prefix);

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefix(&linebuf, strlen(long_prefix) + DATALEN + 1, prefix, "%s", MKTEXT(500));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, long_prefix);
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(long_prefix) + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vtags_prefix3(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	strcpy(prefix, long_prefix);
	strcat(prefix, ":");

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefix(&linebuf, strlen(long_prefix) + DATALEN + 1, prefix, "%s", MKTEXT(500));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, long_prefix);
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(long_prefix) + strlen(CRLF), strlen(output), MSG);
}

static void basic_vtags_prefixf1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefixf(&linebuf, strlen(SOME_TAGS) + DATALEN + 1, SOME_TAGS ":pre%d ", "test %s %d", "TEST", 42);
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);
	is_string(SOME_TAGS ":pre300 test TEST 42" CRLF, output, MSG);
}

static void long_vtags_prefixf1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefixf(&linebuf, strlen(SOME_TAGS) + DATALEN + 1, SOME_TAGS ":pre%d ", "%s", MKTEXT(502));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, SOME_TAGS ":pre300 ");
	strcat(tmp, MKTEXT(502));
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(SOME_TAGS) + DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vtags_prefixf1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefixf(&linebuf, strlen(SOME_TAGS) + DATALEN + 1, SOME_TAGS ":pre%d ", "%s", MKTEXT(503));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, SOME_TAGS ":pre300 ");
	strcat(tmp, MKTEXT(503));
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(SOME_TAGS) + DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void long_vtags_prefixf2(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	strcpy(prefix, long_tags);
	strcat(prefix, ":pre%d ");

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefixf(&linebuf, strlen(long_tags) + DATALEN + 1, prefix, "%s", MKTEXT(502));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, long_tags);
	strcat(tmp, ":pre300 ");
	strcat(tmp, MKTEXT(502));
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(long_tags) + DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vtags_prefixf2(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	strcpy(prefix, long_tags);
	strcat(prefix, ":pre%d ");

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefixf(&linebuf, strlen(long_tags) + DATALEN + 1, prefix, "%s", MKTEXT(503));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, long_tags);
	strcat(tmp, ":pre300 ");
	strcat(tmp, MKTEXT(503));
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(long_tags) + DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void long_vtags_prefixf3(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	strcpy(prefix, long_prefixf);

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefixf(&linebuf, strlen(prefix) + 1 + DATALEN + 1, prefix, "%s", MKTEXT(500));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, long_prefixf);
	tmp[strlen(tmp) - 2] = 0;
	strcat(tmp, "300");
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(long_prefixf) + 1 + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vtags_prefixf3(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	strcpy(prefix, long_prefixf);
	strcat(prefix, ":");

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vtags_prefixf(&linebuf, strlen(prefix) + 1 + 1 + DATALEN + 1, prefix, "%s", MKTEXT(500));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, long_prefixf);
	tmp[strlen(tmp) - 2] = 0;
	strcat(tmp, "300");
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(strlen(long_prefixf) + 1 + strlen(CRLF), strlen(output), MSG);
}

static void basic_msgf1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	rb_linebuf_put_msgf(&linebuf, ":prefix test %s %d", "TEST", 42);
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);
	is_string(":prefix test TEST 42" CRLF, output, MSG);
}

static void long_msgf1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	rb_linebuf_put_msgf(&linebuf, ":prefix %s", MKTEXT(502));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":prefix ");
	strcat(tmp, MKTEXT(502));
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_msgf1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	rb_linebuf_put_msgf(&linebuf, ":prefix %s", MKTEXT(503));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":prefix ");
	strcat(tmp, MKTEXT(503));
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void long_msgf2(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	rb_linebuf_put_msgf(&linebuf, ":prefix %s", MKTEXT(502));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":prefix ");
	strcat(tmp, MKTEXT(502));
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_msgf2(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	rb_linebuf_put_msgf(&linebuf, ":prefix %s", MKTEXT(503));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":prefix ");
	strcat(tmp, MKTEXT(503));
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void basic_vmsg1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg(&linebuf, ":prefix test %s %d", "TEST", 42);
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);
	is_string(":prefix test TEST 42" CRLF, output, MSG);
}

static void long_vmsg1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg(&linebuf, ":prefix %s", MKTEXT(502));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":prefix ");
	strcat(tmp, MKTEXT(502));
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vmsg1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg(&linebuf, ":prefix %s", MKTEXT(503));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":prefix ");
	strcat(tmp, MKTEXT(503));
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void long_vmsg2(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg(&linebuf, ":prefix %s", MKTEXT(502));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":prefix ");
	strcat(tmp, MKTEXT(502));
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vmsg2(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg(&linebuf, ":prefix %s", MKTEXT(503));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":prefix ");
	strcat(tmp, MKTEXT(503));
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void basic_vmsg_prefixf1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg_prefixf(&linebuf, ":pre%d ", "test %s %d", "TEST", 42);
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);
	is_string(":pre300 test TEST 42" CRLF, output, MSG);
}

static void long_vmsg_prefixf1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg_prefixf(&linebuf, ":pre%d ", "%s", MKTEXT(502));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":pre300 ");
	strcat(tmp, MKTEXT(502));
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vmsg_prefixf1(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg_prefixf(&linebuf, ":pre%d ", "%s", MKTEXT(503));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":pre300 ");
	strcat(tmp, MKTEXT(503));
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void long_vmsg_prefixf2(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg_prefixf(&linebuf, ":pre%d ", "%s", MKTEXT(502));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":pre300 ");
	strcat(tmp, MKTEXT(502));
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vmsg_prefixf2(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	int len;

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg_prefixf(&linebuf, ":pre%d ", "%s", MKTEXT(503));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, ":pre300 ");
	strcat(tmp, MKTEXT(503));
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void long_vmsg_prefixf3(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	strcpy(prefix, MKTEXT(507));
	strcat(prefix, "%d");

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg_prefixf(&linebuf, prefix, "%s", MKTEXT(500));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, MKTEXT(507));
	strcat(tmp, "300");
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

static void too_long_vmsg_prefixf3(void)
{
	buf_head_t linebuf;
	char output[2048] = { 0 };
	char prefix[2048] = { 0 };
	int len;

	strcpy(prefix, MKTEXT(508));
	strcat(prefix, "%d");

	rb_linebuf_newbuf(&linebuf);
	_rb_linebuf_put_vmsg_prefixf(&linebuf, prefix, "%s", MKTEXT(500));
	len = rb_linebuf_get(&linebuf, output, sizeof(output), 0, 1);
	rb_linebuf_donebuf(&linebuf);

	is_int(strlen(output), len, MSG);

	char tmp[2048];

	strcpy(tmp, MKTEXT(508));
	strcat(tmp, "300");
	tmp[strlen(tmp) - 1] = 0; /* truncated message */
	strcat(tmp, CRLF);
	is_string(tmp, output, MSG);
	is_int(DATALEN + strlen(CRLF), strlen(output), MSG);
}

int main(int argc, char *argv[])
{
	memset(&me, 0, sizeof(me));
	strcpy(me.name, "me.name.");

	strcpy(long_tags, "@");
	strcat(long_tags, MKTEXT(510));
	strcat(long_tags, " ");

	/* no space for any additional message */
	strcpy(long_prefix, "@");
	strcat(long_prefix, MKTEXT(510));
	strcat(long_prefix, " ");
	strcat(long_prefix, MKTEXT(510));

	strcpy(long_prefixf, "@");
	strcat(long_prefixf, MKTEXT(510));
	strcat(long_prefixf, " ");
	strcat(long_prefixf, MKTEXT(507));
	strcat(long_prefixf, "%d");

	rb_lib_init(NULL, NULL, NULL, 0, 1024, DNODE_HEAP_SIZE, FD_HEAP_SIZE);
	rb_linebuf_init(LINEBUF_HEAP_SIZE);

	plan_lazy();

	is_int(512, TAGSLEN, MSG);
	is_int(510, DATALEN, MSG);

	is_int(TAGSLEN, strlen(long_tags), MSG);
	is_int(TAGSLEN + DATALEN, strlen(long_prefix), MSG);
	/* this is a format string, "%d" -> "300", so 1 char short */
	is_int(TAGSLEN + DATALEN - 1, strlen(long_prefixf), MSG);

	basic_vtags_prefix1();
	long_vtags_prefix1();
	too_long_vtags_prefix1();
	long_vtags_prefix2();
	too_long_vtags_prefix2();
	long_vtags_prefix3();
	too_long_vtags_prefix3();

	basic_vtags_prefixf1();
	long_vtags_prefixf1();
	too_long_vtags_prefixf1();
	long_vtags_prefixf2();
	too_long_vtags_prefixf2();
	long_vtags_prefixf3();
	too_long_vtags_prefixf3();

	basic_msgf1();
	long_msgf1();
	too_long_msgf1();
	long_msgf2();
	too_long_msgf2();

	basic_vmsg1();
	long_vmsg1();
	too_long_vmsg1();
	long_vmsg2();
	too_long_vmsg2();

	basic_vmsg_prefixf1();
	long_vmsg_prefixf1();
	too_long_vmsg_prefixf1();
	long_vmsg_prefixf2();
	too_long_vmsg_prefixf2();
	long_vmsg_prefixf3();
	too_long_vmsg_prefixf3();

	return 0;
}
