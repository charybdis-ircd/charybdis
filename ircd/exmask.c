/*
 * charybdis: An advanced ircd.
 * exmask.c: Management for user exemption masks.
 *
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $Id$
 */

#include "stdinc.h"
#include "exmask.h"


uint exmask_modes[256] =
{
	/* 0x00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0x0F */
	/* 0x10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0x1F */
	/* 0x20 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0x2F */
	/* 0x30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0x3F */

	0,                      /* @ */
	0,                      /* A */
	0,                      /* B */
	EX_EXTENDCHANS,         /* C */
	0,                      /* D */
	0,                      /* E */
	0,                      /* F */
	0,                      /* G */
	0,                      /* H */
	0,                      /* I */
	EX_JOINSPLIT,           /* J */
	0,                      /* K */
	0,                      /* L */
	0,                      /* M */
	0,                      /* N */
	0,                      /* O */
	0,                      /* P */
	0,                      /* Q */
	0,                      /* R */
	0,                      /* S */
	0,                      /* T */
	0,                      /* U */
	0,                      /* V */
	0,                      /* W */
	0,                      /* X */
	EX_SPAM,                /* Y */
	0,                      /* Z */

	/* 0x5B */ 0,0,0,0,0,0, /* 0x60 */

	EX_ACCEPT,              /* a */
	0,                      /* b */
	0,                      /* c */
	EX_DYNSPOOF,            /* d */
	0,                      /* e */
	EX_FLOOD,               /* f */
	0,                      /* g */
	0,                      /* h */
	EX_INVISOP,             /* i */
	EX_JUPE,                /* j */
	EX_KLINE,               /* k */
	EX_STATSL,              /* l */
	0,                      /* m */
	0,                      /* n */
	0,                      /* o */
	EX_STATSP,              /* p */
	0,                      /* q */
	EX_RESV,                /* r */
	0,                      /* s */
	0,                      /* t */
	0,                      /* u */
	0,                      /* v */
	EX_WHOISOPS,            /* w */
	0,                      /* x */
	0,                      /* y */
	0,                      /* z */

	/* 0x7B */ 0,0,0,0,0,/* 0x7F */
	/* 0x80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0x9F */
	/* 0x90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0x9F */
	/* 0xA0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0xAF */
	/* 0xB0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0xBF */
	/* 0xC0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0xCF */
	/* 0xD0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0xDF */
	/* 0xE0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,/* 0xEF */
	/* 0xF0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 /* 0xFF */
};


static char exbuf[BUFSIZE];


/*
 * inputs       - client to generate exmask string for
 * outputs      - exmask string of client
 * side effects - NONE
 */
const char *exmask_to_modes(const uint mask)
{
	char *ptr = exbuf;
	*ptr = '\0';
	*ptr++ = '+';

	for(uint i = 0; i < 128; i++)
		if(exmask_modes[i] && (mask & exmask_modes[i]))
			*ptr++ = (char)i;

	*ptr++ = '\0';
	return exbuf;
}


/*
 * inputs       - ONE flag from an exmask to find
 * outputs      - the char for this flag.
 * side effects - NONE
 */
char exmask_to_mode(const uint flag)
{
	for(uint i = 0; i < 128; i++)
		if(exmask_modes[i] && (flag & exmask_modes[i]))
			return (char)i;

	return '+';
}


/*
 * inputs       - value to alter bitmask for, exmask string
 * outputs      - replacement bitmask to set
 * side effects - NONE
 */
uint exmask_to_mask(uint val, const char *const buf)
{
	if(buf == NULL)
		return val;

	int what = 1;
	for(const char *p = buf; *p != '\0'; p++)
	{
		switch(*p)
		{
			case '+':
				what = 1;
				break;

			case '-':
				what = 0;
				break;

			default:
				if(what == 1)
					val |= exmask_modes[(uint8_t)*p];
				else if(what == 0)
					val &= ~exmask_modes[(uint8_t)*p];

				break;
		}
	}

	return val;
}


/*
 * inputs       - NONE
 * outputs      - an available exempt bit or 0 if no bits are available
 * side effects - NONE
 */
uint exmask_find_slot(void)
{
	uint all = 0;
	for(uint i = 0; i < 128; i++)
		all |= exmask_modes[i];

	uint my = 1;
	for(; my && (all & my); my <<= 1);

	return my;
}
