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
	{mg_ignore, {m_prop, 2}, {m_prop, 2}, mg_ignore, mg_ignore, {m_prop, 2}}
};

/* :source TPROP target creationTS updateTS propName [:propValue] */
static void ms_tprop(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message tprop_msgtab = {
	"TPROP", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_tprop, 5}, mg_ignore, mg_ignore}
};

mapi_clist_av1 ircx_prop_clist[] = { &prop_msgtab, &tprop_msgtab, NULL };

static void h_prop_burst_channel(void *);

mapi_hfn_list_av1 ircx_prop_hfnlist[] = {
	{ "burst_channel", (hookfn) h_prop_burst_channel },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(ircx_prop, NULL, NULL, ircx_prop_clist, NULL, ircx_prop_hfnlist, NULL, NULL, ircx_prop_desc);

static void
handle_prop_list(const char *target, const rb_dlink_list *prop_list, struct Client *source_p, const char *keys, int alevel)
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

	sendto_one(source_p, ":%s!%s@%s PROP %s %s :%s", source_p->name, source_p->username, source_p->host,
		target, property->name, property->value);

	// XXX: enforce CAP_IRCX
	// XXX: rewrite target to UID if needed

	// don't redistribute updates for local channels
	if (IsChanPrefix(*target) && *target == '&')
		return;

	sendto_server(source_p, NULL, CAP_TS6, NOCAPS,
			":%s PROP %s %s :%s", use_id(source_p), target, property->name, property->value);
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
	bool write_allowed = false;
	int alevel = CHFL_PEON;

	if (IsChanPrefix(*parv[1]))
	{
		struct Channel *chan = find_channel(parv[1]);
		struct membership *msptr = find_channel_membership(chan, source_p);

		if (chan == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), parv[1]);
			return;
		}

		prop_list = &chan->prop_list;

		if (!MyClient(source_p) || (msptr != NULL && (alevel = get_channel_access(source_p, chan, msptr, MODE_ADD, NULL)) >= CHFL_CHANOP))
			write_allowed = true;
	}
	else
		return;

	switch (parc)
	{
	case 2:
		handle_prop_list(parv[1], prop_list, source_p, NULL, alevel);
		break;

	case 3:
		handle_prop_list(parv[1], prop_list, source_p, parv[2], alevel);
		break;

	case 4:
		if (!write_allowed && MyClient(source_p))
		{
			sendto_one_numeric(source_p, ERR_PROPDENIED, form_str(ERR_PROPDENIED), parv[1]);
			return;
		}

		handle_prop_upsert_or_delete(parv[1], prop_list, source_p, parv[2], parv[3]);
		break;

	default:
		break;
	}
}

/* bursting */
static void
h_prop_burst_channel(void *vdata)
{
	hook_data_channel *hchaninfo = vdata;
	struct Channel *chptr = hchaninfo->chptr;
	struct Client *client_p = hchaninfo->client;
	rb_dlink_node *it;

	RB_DLINK_FOREACH(it, chptr->prop_list.head)
	{
		struct Property *prop = it->data;

		/* :source TPROP target creationTS updateTS propName [:propValue] */
		sendto_one(client_p, ":%s TPROP %s %ld %ld %s :%s",
			use_id(&me), chptr->chname, chptr->channelts, prop->set_at, prop->name, prop->value);
	}
}

/*
 * TPROP
 *
 * parv[1] = target
 * parv[2] = creation TS
 * parv[3] = modification TS
 * parv[4] = key
 * parv[5] = value
 */
static void
ms_tprop(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	rb_dlink_list *prop_list = NULL;
	time_t creation_ts = atol(parv[2]);
	time_t update_ts = atol(parv[3]);

	if (IsChanPrefix(*parv[1]))
	{
		struct Channel *chptr = find_channel(parv[1]);
		if (chptr == NULL)
			return;

		/* if creation_ts is newer than channelts, reject the TPROP */
		if (creation_ts > chptr->channelts)
			return;

		prop_list = &chptr->prop_list;
	}

	/* couldn't figure out what to mutate, bail */
	if (prop_list == NULL)
		return;

	/* do the upsert */
	struct Property *prop = propertyset_add(prop_list, parv[4], parv[5], source_p);
	prop->set_at = update_ts;

	// XXX: prop-notify???
}
