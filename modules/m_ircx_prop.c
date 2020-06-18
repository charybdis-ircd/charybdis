/*
 * modules/m_ircx_prop.c
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include "stdinc.h"
#include "capability.h"
#include "client.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "monitor.h"
#include "numeric.h"
#include "s_assert.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "supported.h"
#include "hash.h"
#include "propertyset.h"

static const char ircx_prop_desc[] = "Provides IRCX PROP command";

static void m_prop(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message prop_msgtab = {
	"PROP", 0, 0, 0, 0,
	{mg_ignore, {m_prop, 2}, mg_ignore, mg_ignore, mg_ignore, {m_prop, 2}}
};

#ifdef NOTYET
/* :source TPROP target creationTS updateTS propName [:propValue] */
static void ms_tprop(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message tprop_msgtab = {
	"TPROP", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_tprop, 5}, mg_ignore, mg_ignore}
};
#endif

mapi_clist_av1 ircx_prop_clist[] = { &prop_msgtab, NULL };

static void h_prop_channel_join(void *);

mapi_hfn_list_av1 ircx_prop_hfnlist[] = {
	{ "channel_join", (hookfn) h_prop_channel_join },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(ircx_prop, NULL, NULL, ircx_prop_clist, NULL, ircx_prop_hfnlist, NULL, NULL, ircx_prop_desc);

static void
handle_prop_list(const char *target, const rb_dlink_list *prop_list, struct Client *source_p, const char *keys)
{
	rb_dlink_node *iter;

	s_assert(prop_list != NULL);

	RB_DLINK_FOREACH(iter, prop_list->head)
	{
		struct Property *prop = iter->data;

		if (keys != NULL && rb_strcasestr(keys, prop->name) == NULL)
			continue;

		sendto_one_numeric(source_p, RPL_PROPLIST, form_str(RPL_PROPLIST),
			target, prop->name, prop->value);
	}

	sendto_one_numeric(source_p, RPL_PROPEND, form_str(RPL_PROPEND), target);
}

static void
handle_prop_upsert_or_delete(const char *target, rb_dlink_list *prop_list, struct Client *source_p, const char *prop, const char *value)
{
	struct Property *property;

	propertyset_delete(prop_list, prop);

	/* deletion: value is empty string */
	if (! *value)
	{
		sendto_one(source_p, ":%s!%s@%s PROP %s %s :", source_p->name, source_p->username, source_p->host,
			target, prop);
		return;
	}

	property = propertyset_add(prop_list, prop, value, source_p);
	if (property == NULL)
		return;

	sendto_one(source_p, ":%s!%s@%s PROP %s %s :%s", source_p->name, source_p->username, source_p->host,
		target, property->name, property->value);
}

/*
 * LIST: PROP target [filters] (parc <= 3)
 * SET: PROP target key :value (parc == 4)
 * DELETE: PROP target key : (parc == 4)
 *
 * XXX: Like original IRCX, only channels supported at the moment.
 */
static void
m_prop(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	rb_dlink_list *prop_list;

	if (IsChanPrefix(*parv[1]))
	{
		struct Channel *chan = find_channel(parv[1]);

		if (chan == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), parv[1]);
			return;
		}

		prop_list = &chan->prop_list;
	}
	else
		return;

	switch (parc)
	{
	case 2:
		handle_prop_list(parv[1], prop_list, source_p, NULL);
		break;

	case 3:
		handle_prop_list(parv[1], prop_list, source_p, parv[2]);
		break;

	case 4:
		handle_prop_upsert_or_delete(parv[1], prop_list, source_p, parv[2], parv[3]);
		break;

	default:
		break;
	}
}

/* handle ONJOIN property */
static void
h_prop_channel_join(void *vdata)
{
	hook_data_channel_activity *data = (hook_data_channel_activity *)vdata;
	struct Channel *chptr = data->chptr;
	struct Client *source_p = data->client;

	if (!MyClient(source_p))
		return;

	rb_dlink_list *prop_list = &chptr->prop_list;
	struct Property *prop = propertyset_find(prop_list, "ONJOIN");

	if (prop != NULL)
		sendto_one(source_p, ":%s PRIVMSG %s :%s", chptr->chname, chptr->chname, prop->value);
}
