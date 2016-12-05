/*
 *  charybdis: porto vintage '88.
 *  m_sajoin.c: Force join a user to a channel.
 *
 * Copyright (c) 2016 Charybdis Development Team
 * Copyright (c) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for 
any
 * purpose with or without fee is hereby granted, provided that the 
above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE
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
#include "client.h"
#include "ircd.h"
#include "msg.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "logger.h"
#include "parse.h"
#include "modules.h"
#include "hash.h"
#include "cache.h"
#include "rb_dictionary.h"

const char sajoin_desc[] =
"Allows operators to force a user to join a channel.";

static void mo_sajoin(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message sajoin_msgtab =
{
	"SAJOIN", 0, 0, 0, 0,
	{
		mg_unreg,
		mg_ignore,
		mg_ignore,
		mg_ignore,
		mg_ignore,
		{ mo_sajoin, 3 }
	}
};

mapi_clist_av1 sajoin_clist[] =
{
	&sajoin_msgtab,
	NULL
};

DECLARE_MODULE_AV2
(
	sajoin,
	NULL,
	NULL,
	sajoin_clist,
	NULL,
	NULL,
	NULL,
	NULL,
	sajoin_desc
);

/* Soft join is where SAJOIN peruses as the user itself attempting
 * to join the channel and is susceptible to bans etc.
 *
 * parv[1] = user to join to channel
 * parv[2] = name of channel
 */
static void
join_soft(struct Client *const client,
          const char *const chname)
{
	static char buf[BUFSIZE];
	const size_t len = snprintf(buf, sizeof(buf), ":%s JOIN %s",
	                            use_id(client),
	                            chname);

	parse(client, buf, buf + len);
}

/* Hard join forces the target user into the channel manually,
 * with as few checks as possible to bypass any bans etc.
 *
 * parv[1] = user to join to channel
 * parv[2] = name of channel
 *
 *  !! NOT YET IMPLEMENTED !!
 */
static void
join_hard(struct Client *const client,
          const char *const chname)
{
}

/*
 * parv[1] = channel list (chan,chan,chan,...)
 * parv[2] = nick list (nick,nick,nick,...)
 * (future) parv[3] = options
 */
static void
mo_sajoin(struct MsgBuf *const msgbuf,
          struct Client *const client_p,
          struct Client *const source_p,
          int parc,
          const char **const parv)
{
	static const char *const SEP = ",";
	static char vec[BUFSIZE];
	char *ctx, *target;

	int clients = 0;
	static struct Client *client[BUFSIZE];
	rb_strlcpy(vec, parv[2], sizeof(vec));
	target = rb_strtok_r(vec, SEP, &ctx); do
	{
		if(!(client[clients] = find_person(target)))
			continue;

		clients++;		
	}
	while((target = rb_strtok_r(NULL, SEP, &ctx)));

	rb_strlcpy(vec, parv[1], sizeof(vec));
	target = rb_strtok_r(vec, SEP, &ctx); do
	{
		if(!check_channel_name(target))
			continue;

		for(size_t i = 0; i < clients; i++)
			join_soft(client[i], target);
	}
	while((target = rb_strtok_r(NULL, SEP, &ctx)));
}
