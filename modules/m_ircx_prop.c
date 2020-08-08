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

static int h_prop_show;
static int h_prop_chan_write;
static int h_prop_change;

mapi_hlist_av1 ircx_prop_hlist[] = {
	{ "prop_show", &h_prop_show },
	{ "prop_chan_write", &h_prop_chan_write },
	{ "prop_change", &h_prop_change },
	{ NULL, NULL }
};

static void h_prop_burst_channel(void *);
static void h_prop_burst_client(void *);
static void h_prop_channel_lowerts(void *);

mapi_hfn_list_av1 ircx_prop_hfnlist[] = {
	{ "burst_channel", (hookfn) h_prop_burst_channel },
	{ "burst_client", (hookfn) h_prop_burst_client },
	{ "channel_lowerts", (hookfn) h_prop_channel_lowerts },
	{ NULL, NULL }
};

static int
ircx_prop_init(void)
{
	add_isupport("MAXPROP", isupport_intptr, &ConfigChannel.max_prop);
}

static void
ircx_prop_deinit(void)
{
	delete_isupport("MAXPROP");
}

DECLARE_MODULE_AV2(ircx_prop, ircx_prop_init, ircx_prop_deinit, ircx_prop_clist, ircx_prop_hlist, ircx_prop_hfnlist, NULL, NULL, ircx_prop_desc);

static void
handle_prop_list(const char *target, const void *target_ptr, const rb_dlink_list *prop_list, struct Client *source_p, const char *keys, int alevel)
{
	rb_dlink_node *iter;

	s_assert(prop_list != NULL);

	RB_DLINK_FOREACH(iter, prop_list->head)
	{
		struct Property *prop = iter->data;
		hook_data_prop_activity prop_activity;

		if (keys != NULL && rb_strcasestr(keys, prop->name) == NULL)
			continue;

		prop_activity.client = source_p;
		prop_activity.target = target;
		prop_activity.prop_list = prop_list;
		prop_activity.key = prop->name;
		prop_activity.alevel = alevel;
		prop_activity.approved = 1;
		prop_activity.target_ptr = target_ptr;

		call_hook(h_prop_show, &prop_activity);

		if (!prop_activity.approved)
			continue;

		sendto_one_numeric(source_p, RPL_PROPLIST, form_str(RPL_PROPLIST),
			target, prop->name, prop->value);
	}

	sendto_one_numeric(source_p, RPL_PROPEND, form_str(RPL_PROPEND), target);
}

static void
handle_prop_upsert_or_delete(const char *target, const void *target_ptr, rb_dlink_list *prop_list, struct Client *source_p, const char *prop, const char *value)
{
	struct Property *property;
	hook_data_prop_activity prop_activity;

	propertyset_delete(prop_list, prop);

	/* deletion: value is empty string */
	if (! *value)
	{
		sendto_one(source_p, ":%s!%s@%s PROP %s %s :", source_p->name, source_p->username, source_p->host,
			target, prop);
		goto broadcast;
	}

	/* enforce MAXPROP on upsert */
	if (rb_dlink_list_length(prop_list) >= ConfigChannel.max_prop)
	{
		sendto_one_numeric(source_p, ERR_PROP_TOOMANY, form_str(ERR_PROP_TOOMANY), target);
		return;
	}

	property = propertyset_add(prop_list, prop, value, source_p);

	sendto_one(source_p, ":%s!%s@%s PROP %s %s :%s", source_p->name, source_p->username, source_p->host,
		target, property->name, property->value);

	// XXX: enforce CAP_IRCX
	// XXX: rewrite target to UID if needed

	// don't redistribute updates for local channels
	if (IsChanPrefix(*target) && *target == '&')
		goto broadcast;

	sendto_server(source_p, NULL, CAP_TS6, NOCAPS,
			":%s PROP %s %s :%s", use_id(source_p), target, property->name, property->value);

	prop = property->name;
	value = property->value;

	// broadcast the property change to local members
broadcast:
	prop_activity.client = source_p;
	prop_activity.target = target;
	prop_activity.prop_list = prop_list;
	prop_activity.key = prop;
	prop_activity.value = value;
	prop_activity.alevel = CHFL_ADMIN;
	prop_activity.approved = 1;
	prop_activity.target_ptr = target_ptr;

	call_hook(h_prop_change, &prop_activity);
}

static bool
can_write_to_channel_property(struct Client *source_p, struct Channel *chptr, const char *key, int alevel)
{
	hook_data_prop_activity prop_activity;

	prop_activity.client = source_p;
	prop_activity.target = chptr->chname;
	prop_activity.prop_list = &chptr->prop_list;
	prop_activity.key = key;
	prop_activity.alevel = alevel;
	prop_activity.approved = alevel >= CHFL_CHANOP;
	prop_activity.target_ptr = chptr;

	call_hook(h_prop_chan_write, &prop_activity);

	return prop_activity.approved;
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
	void *target_ptr;
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

		target_ptr = chan;
		prop_list = &chan->prop_list;

		if (msptr != NULL)
			alevel = get_channel_access(source_p, chan, msptr, MODE_ADD, NULL);

		if (parc == 4)
		{
			if (!MyClient(source_p))
				write_allowed = true;
			else
				write_allowed = can_write_to_channel_property(source_p, chan, parv[2], alevel);
		}
	}
	else
		return;

	switch (parc)
	{
	case 2:
		handle_prop_list(parv[1], target_ptr, prop_list, source_p, NULL, alevel);
		break;

	case 3:
		handle_prop_list(parv[1], target_ptr, prop_list, source_p, parv[2], alevel);
		break;

	case 4:
		if (!write_allowed && MyClient(source_p))
		{
			sendto_one_numeric(source_p, ERR_PROPDENIED, form_str(ERR_PROPDENIED), parv[1]);
			return;
		}

		handle_prop_upsert_or_delete(parv[1], target_ptr, prop_list, source_p, parv[2], parv[3]);
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

static void
h_prop_burst_client(void *vdata)
{
	hook_data_client *hclientinfo = vdata;
	struct Client *client_p = hclientinfo->client;
	struct Client *burst_p = hclientinfo->target;
	rb_dlink_node *it;

	if (burst_p->user == NULL)
		return;

	RB_DLINK_FOREACH(it, burst_p->user->prop_list.head)
	{
		struct Property *prop = it->data;

		/* :source TPROP target creationTS updateTS propName [:propValue] */
		sendto_one(client_p, ":%s TPROP %s %ld %ld %s :%s",
			use_id(&me), use_id(burst_p), burst_p->tsinfo, prop->set_at, prop->name, prop->value);
	}
}

static void
h_prop_channel_lowerts(void *vdata)
{
	hook_data_channel *hchaninfo = vdata;
	struct Channel *chptr = hchaninfo->chptr;

	propertyset_clear(&chptr->prop_list);
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
	struct Channel *chptr = NULL;
	void *target_ptr = NULL;
	hook_data_prop_activity prop_activity;

	if (IsChanPrefix(*parv[1]))
	{
		chptr = find_channel(parv[1]);
		if (chptr == NULL)
			return;

		/* if creation_ts is newer than channelts, reject the TPROP */
		if (creation_ts > chptr->channelts)
			return;

		prop_list = &chptr->prop_list;
		target_ptr = chptr;
	}
	else
	{
		struct Client *target_p = find_client(parv[1]);
		if (target_p == NULL || target_p->user == NULL)
			return;

		/* if creation_ts does not match nick TS, reject the TPROP */
		if (creation_ts != target_p->tsinfo)
			return;

		prop_list = &target_p->user->prop_list;
		target_ptr = target_p;
	}

	/* couldn't figure out what to mutate, bail */
	if (prop_list == NULL)
		return;

	/* do the upsert */
	struct Property *prop = propertyset_add(prop_list, parv[4], parv[5], source_p);
	prop->set_at = update_ts;

	sendto_server(source_p, chptr, CAP_TS6, NOCAPS,
		":%s TPROP %s %ld %ld %s :%s",
		use_id(&me), parv[1], creation_ts, prop->set_at, prop->name, prop->value);

	// broadcast the property change to local members
	prop_activity.client = &me;
	prop_activity.target = parv[1];
	prop_activity.prop_list = prop_list;
	prop_activity.key = prop->name;
	prop_activity.value = prop->value;
	prop_activity.alevel = CHFL_ADMIN;
	prop_activity.approved = 1;
	prop_activity.target_ptr = target_ptr;

	call_hook(h_prop_change, &prop_activity);
}
