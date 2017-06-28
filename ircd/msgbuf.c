/*
 * charybdis - an advanced ircd.
 * Copyright (c) 2016 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdinc.h"
#include "ircd_defs.h"
#include "msgbuf.h"
#include "client.h"
#include "ircd.h"

static const char tag_escape_table[256] = {
	/*        x0   x1   x2   x3   x4   x5   x6   x7   x8   x9   xA   xB   xC   xD   xE   xF */
	/* 0x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 'n',   0,   0, 'r',   0,   0,
	/* 1x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 2x */ 's',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 3x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, ':',   0,   0,   0,   0,
	/* 4x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 5x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\\',   0,   0,   0,
};

static const char tag_unescape_table[256] = {
	/*        x0   x1   x2   x3   x4   x5   x6   x7   x8   x9   xA   xB   xC   xD   xE   xF */
	/* 0x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 1x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 2x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 3x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, ';',   0,   0,   0,   0,   0,
	/* 4x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 5x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\\',   0,   0,   0,
	/* 6x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\n',   0,
	/* 7x */   0,   0,'\r', ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

static void
msgbuf_unescape_value(char *value)
{
	char *in = value;
	char *out = value;

	if (value == NULL)
		return;

	while (*in != '\0') {
		if (*in == '\\') {
			const char unescape = tag_unescape_table[(unsigned char)*++in];

			/* "\\\0" is unescaped to the character itself, "\0" */
			if (*in == '\0')
				break;

			if (unescape) {
				*out++ = unescape;
				*in++;
			} else {
				*out++ = *in++;
			}
		} else {
			*out++ = *in++;
		}
	}

	/* copy final '\0' */
	*out = *in;
}

/*
 * parse a message into a MsgBuf.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_parse(struct MsgBuf *msgbuf, char *line)
{
	char *ch = line;

	msgbuf_init(msgbuf);

	if (*ch == '@') {
		char *t = ch + 1;

		ch = strchr(ch, ' ');

		/* truncate tags if they're too long */
		if ((ch != NULL && (ch - line) + 1 > TAGSLEN) || (ch == NULL && strlen(line) >= TAGSLEN)) {
			ch = &line[TAGSLEN - 1];
		}

		if (ch != NULL) {
			/* NULL terminate the tags string */
			*ch++ = '\0';

			while (1) {
				char *next = strchr(t, ';');
				char *eq = strchr(t, '=');

				if (next != NULL) {
					*next = '\0';

					if (eq > next)
						eq = NULL;
				}

				if (eq != NULL)
					*eq++ = '\0';

				if (*t != '\0') {
					msgbuf_unescape_value(eq);
					msgbuf_append_tag(msgbuf, t, eq, 0);
				}

				if (next != NULL) {
					t = next + 1;
				} else {
					break;
				}
			}
		} else {
			return 1;
		}
	}

	/* truncate message if it's too long */
	if (strlen(ch) > DATALEN) {
		ch[DATALEN] = '\0';
	}

	if (*ch == ':') {
		ch++;
		msgbuf->origin = ch;

		char *end = strchr(ch, ' ');
		if (end == NULL)
			return 4;

		*end = '\0';
		ch = end + 1;
	}

	if (*ch == '\0')
		return 2;

	msgbuf->n_para = rb_string_to_array(ch, (char **)msgbuf->para, MAXPARA);
	if (msgbuf->n_para == 0)
		return 3;

	msgbuf->cmd = msgbuf->para[0];
	return 0;
}

/*
 * Unparse msgbuf tags into a buffer
 * returns the length of the tags written
 */
static size_t
msgbuf_unparse_tags(char *buf, size_t buflen, const struct MsgBuf *msgbuf, unsigned int capmask)
{
	bool has_tags = false;
	char *commit = buf;
	char *output = buf;
	const char * const end = &buf[buflen - 2]; /* this is where the final ' ' goes */

	for (size_t i = 0; i < msgbuf->n_tags; i++) {
		size_t len;

		if ((msgbuf->tags[i].capmask & capmask) == 0)
			continue;

		if (has_tags) {
			if (output >= end)
				break;
			*output++ = ';';
		} else {
			if (output >= end)
				break;
			*output++ = '@';
		}

		if (msgbuf->tags[i].key == NULL)
			continue;

		len = strlen(msgbuf->tags[i].key);
		if (len == 0)
			continue;

		if (output + len > end)
			break;
		strcat(output, msgbuf->tags[i].key);
		output += len;

		if (msgbuf->tags[i].value != NULL) {
			if (output >= end)
				break;
			*output++ = '=';

			len = strlen(msgbuf->tags[i].value);
			/* this only checks the unescaped length,
			 * but the escaped length could be longer
			 */
			if (output + len > end)
				break;

			for (size_t n = 0; n < len; n++) {
				const unsigned char c = msgbuf->tags[i].value[n];
				const char escape = tag_escape_table[c];

				if (escape) {
					if (output + 2 > end)
						break;

					*output++ = '\\';
					*output++ = escape;
				} else {
					if (output >= end)
						break;

					*output++ = c;
				}
			}
		}

		has_tags = true;
		commit = output;
	}

	if (has_tags)
		*commit++ = ' ';

	*commit = 0;
	return commit - buf;
}

/*
 * unparse a MsgBuf and message prefix into a buffer
 * if origin is NULL, me.name will be used.
 * cmd should not be NULL.
 * updates buflen to correctly allow remaining message data to be added
 */
void
msgbuf_unparse_prefix(char *buf, size_t *buflen, const struct MsgBuf *msgbuf, unsigned int capmask)
{
	size_t tags_buflen;
	size_t tags_used = 0;

	memset(buf, 0, *buflen);

	tags_buflen = *buflen;
	if (tags_buflen > TAGSLEN + 1)
		tags_buflen = TAGSLEN + 1;

	if (msgbuf->n_tags > 0)
		tags_used = msgbuf_unparse_tags(buf, tags_buflen, msgbuf, capmask);

	const size_t data_bufmax = (tags_used + DATALEN + 1);
	if (*buflen > data_bufmax)
		*buflen = data_bufmax;

	rb_snprintf_append(buf, *buflen, ":%s ", msgbuf->origin != NULL ? msgbuf->origin : me.name);

	if (msgbuf->cmd != NULL)
		rb_snprintf_append(buf, *buflen, "%s ", msgbuf->cmd);

	if (msgbuf->target != NULL)
		rb_snprintf_append(buf, *buflen, "%s ", msgbuf->target);
}

/*
 * unparse a pure MsgBuf into a buffer.
 * if origin is NULL, me.name will be used.
 * cmd should not be NULL.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_unparse(char *buf, size_t buflen, const struct MsgBuf *msgbuf, unsigned int capmask)
{
	size_t buflen_copy = buflen;

	msgbuf_unparse_prefix(buf, &buflen_copy, msgbuf, capmask);

	for (size_t i = 0; i < msgbuf->n_para; i++) {
		const char *fmt;

		if (i == (msgbuf->n_para - 1) && strchr(msgbuf->para[i], ' ') != NULL) {
			fmt = (i == 0) ? ":%s" : " :%s";
		} else {
			fmt = (i == 0) ? "%s" : " %s";
		}

		rb_snprintf_append(buf, buflen_copy, fmt, msgbuf->para[i]);
	}

	return 0;
}

/*
 * unparse a MsgBuf stem + format string into a buffer
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_vunparse_fmt(char *buf, size_t buflen, const struct MsgBuf *head, unsigned int capmask, const char *fmt, va_list va)
{
	size_t buflen_copy = buflen;
	char *ws;
	size_t prefixlen;

	msgbuf_unparse_prefix(buf, &buflen_copy, head, capmask);
	prefixlen = strlen(buf);

	ws = buf + prefixlen;
	vsnprintf(ws, buflen_copy - prefixlen, fmt, va);

	return 0;
}

/*
 * unparse a MsgBuf stem + format string into a buffer (with va_list handling)
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_unparse_fmt(char *buf, size_t buflen, const struct MsgBuf *head, unsigned int capmask, const char *fmt, ...)
{
	va_list va;
	int res;

	va_start(va, fmt);
	res = msgbuf_vunparse_fmt(buf, buflen, head, capmask, fmt, va);
	va_end(va);

	return res;
}
