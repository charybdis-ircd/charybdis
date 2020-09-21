/*
 * modules/m_ircx_access.c
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
#include "channel_access.h"

static const char ircx_access_desc[] = "Provides IRCX ACCESS command";

static int ircx_access_init(void);
static void ircx_access_deinit(void);

static void m_access(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message access_msgtab = {
	"ACCESS", 0, 0, 0, 0,
	{mg_unreg, {m_access, 2}, {m_access, 2}, mg_ignore, mg_ignore, {m_access, 2}}
};

/* :server TACCESS #channel channelTS entryTS mask level */
static void ms_taccess(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message taccess_msgtab = {
	"TACCESS", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_taccess, 5}, mg_ignore, mg_ignore}
};

mapi_clist_av1 ircx_access_clist[] = { &access_msgtab, &taccess_msgtab, NULL };

static void h_access_channel_join(void *);
static void h_access_burst_channel(void *);
static void h_access_channel_lowerts(void *);

mapi_hfn_list_av1 ircx_access_hfnlist[] = {
	{ "channel_join", (hookfn) h_access_channel_join },
	{ "burst_channel", (hookfn) h_access_burst_channel },
	{ "channel_lowerts", (hookfn) h_access_channel_lowerts },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(ircx_access, ircx_access_init, ircx_access_deinit, ircx_access_clist, NULL, ircx_access_hfnlist, NULL, NULL, ircx_access_desc);

static int
ircx_access_init(void)
{
	add_isupport("MAXACCESS", isupport_intptr, &ConfigChannel.max_bans);
}

static void
ircx_access_deinit(void)
{
	delete_isupport("MAXACCESS");
}

struct AccessLevel {
	const char *level;
	const char mode_char;
	unsigned int flag;
};

/* keep this in alphabetical order for bsearch(3)! */
static const struct AccessLevel alevel[] = {
	{"ADMIN", 'q', CHFL_ADMIN},
	{"OP", 'o', CHFL_CHANOP},
	{"OWNER", 'q', CHFL_ADMIN},
	{"VOICE", 'v', CHFL_VOICE}
};

static const char *
ae_level_name(unsigned int level)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(alevel); i++)
	{
		if ((alevel[i].flag & level) == level)
			return alevel[i].level;
	}

	s_assert("unknown level");
	return "???";
}

static const char
ae_level_char(unsigned int level)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(alevel); i++)
	{
		if ((alevel[i].flag & level) == level)
			return alevel[i].mode_char;
	}

	s_assert("unknown level");
	return 0;
}

static int
ae_pair_cmp(const void *key, const void *ptr)
{
	const struct AccessLevel *alev = ptr;
	return rb_strcasecmp(key, alev->level);
}

static unsigned int
ae_level_from_name(const char *level_name)
{
	if (level_name == NULL)
		return 0;

	const struct AccessLevel *alev = bsearch(level_name, alevel,
		ARRAY_SIZE(alevel), sizeof(alevel[0]), ae_pair_cmp);

	if (alev == NULL)
		return 0;

	return alev->flag;
}

/*
 * ACCESS command.
 *
 * parv[0] = source
 * parv[1] = object name
 * parv[2] = ADD|CLEAR|DEL[ETE]|LIST|SYNC (default LIST)
 * parv[3] = level (ADMIN|OP|VOICE) (optional for CLEAR and LIST)
 * parv[4] = mask (not used for CLEAR and LIST)
 *
 * No permissions check for remotes, op needed to write to op/voice ACL,
 * admin needed to write to admin ACL.  Op needed to read ACL.  Clearing
 * the ACL requires admin.
 */
static bool
can_read_from_access_list(struct Channel *chptr, struct Client *source_p, unsigned int level)
{
	if (!MyClient(source_p))
		return true;

	const struct membership *msptr = find_channel_membership(chptr, source_p);
	return is_admin(msptr) || is_chanop(msptr);
}

static bool
can_write_to_access_list(struct Channel *chptr, struct Client *source_p, unsigned int level)
{
	if (!MyClient(source_p))
		return true;

	const struct membership *msptr = find_channel_membership(chptr, source_p);
	if (level == CHFL_ADMIN)
		return is_admin(msptr);

	return is_admin(msptr) || is_chanop(msptr);
}

/*
 * since the channel access core uses upserts to update the channel access lists,
 * it is important to enforce write access to any previous ACL entry before doing
 * the upsert.  otherwise, a channel could be taken over. -- Ariadne
 */
static bool
can_upsert_on_access_list(struct Channel *chptr, struct Client *source_p, const char *mask, unsigned int newflags)
{
	/* first, we make sure we can read the ACL at all, to prevent bruteforcing */
	if (!can_read_from_access_list(chptr, source_p, CHFL_CHANOP))
		return false;

	struct AccessEntry *ae = channel_access_find(chptr, mask);
	if (ae == NULL)
		return can_write_to_access_list(chptr, source_p, newflags);

	/* now that we have an entry, try to enforce write access */
	if (!can_write_to_access_list(chptr, source_p, ae->flags))
		return false;

	return can_write_to_access_list(chptr, source_p, newflags);
}

static void
handle_access_list(struct Channel *chptr, struct Client *source_p, const char *level)
{
	unsigned int level_match = ae_level_from_name(level);
	const rb_dlink_node *iter;

	if (!can_read_from_access_list(chptr, source_p, level_match))
	{
		sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
			me.name, source_p->name, chptr->chname);
		return;
	}

	sendto_one_numeric(source_p, RPL_ACCESSSTART, form_str(RPL_ACCESSSTART), chptr->chname);

	RB_DLINK_FOREACH(iter, chptr->access_list.head)
	{
		const struct AccessEntry *ae = iter->data;

		if (level_match && (ae->flags & level_match) != level_match)
			continue;

		sendto_one_numeric(source_p, RPL_ACCESSENTRY, form_str(RPL_ACCESSENTRY),
			chptr->chname, ae_level_name(ae->flags), ae->mask, (long) 0, ae->who, "");
	}

	sendto_one_numeric(source_p, RPL_ACCESSEND, form_str(RPL_ACCESSEND), chptr->chname);
}

static void
handle_access_clear(struct Channel *chptr, struct Client *source_p, const char *level)
{
	unsigned int level_match = ae_level_from_name(level);

	if (!can_write_to_access_list(chptr, source_p, CHFL_ADMIN))
	{
		sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
			me.name, source_p->name, chptr->chname);
		return;
	}

	if (level_match)
	{
		rb_dlink_node *iter, *next;

		RB_DLINK_FOREACH_SAFE(iter, next, chptr->access_list.head)
		{
			struct AccessEntry *ae = iter->data;

			if ((ae->flags & level_match) != level_match)
				continue;

			channel_access_delete(chptr, ae->mask);
		}
	}
	else
		channel_access_clear(chptr);

	sendto_server(source_p, chptr, CAP_TS6, NOCAPS,
		":%s ACCESS %s CLEAR %s",
		use_id(source_p), chptr->chname, level_match ? ae_level_name(level_match) : "");
}

/*
 * for deletion, we don't really care about the level, since we already know it.
 * we do, however, enforce write ACL based on the ACL level before doing the actual
 * delete.  -- Ariadne
 */
static void
handle_access_delete(struct Channel *chptr, struct Client *source_p, const char *mask)
{
	if (mask == NULL)
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name,
			   EmptyString(source_p->name) ? "*" : source_p->name,
			   "ACCESS DELETE");
		return;
	}

	/* first, we make sure we can read the ACL at all, to prevent bruteforcing */
	if (!can_read_from_access_list(chptr, source_p, CHFL_CHANOP))
	{
		sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
			me.name, source_p->name, chptr->chname);
		return;
	}

	struct AccessEntry *ae = channel_access_find(chptr, mask);
	if (ae == NULL)
	{
		sendto_one_numeric(source_p, ERR_ACCESS_MISSING, form_str(ERR_ACCESS_MISSING),
			chptr->chname, mask);
		return;
	}

	/* now that we have an entry, try to enforce write access */
	if (!can_write_to_access_list(chptr, source_p, ae->flags))
	{
		sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
			me.name, source_p->name, chptr->chname);
		return;
	}

	if (MyClient(source_p))
	{
		sendto_one_numeric(source_p, RPL_ACCESSDELETE, form_str(RPL_ACCESSDELETE),
			chptr->chname, ae_level_name(ae->flags), ae->mask);
	}

	sendto_server(source_p, chptr, CAP_TS6, NOCAPS,
		":%s ACCESS %s DELETE %s %s",
		use_id(source_p), chptr->chname, ae_level_name(ae->flags),
		ae->mask);

	channel_access_delete(chptr, ae->mask);
}

static void
handle_access_upsert(struct Channel *chptr, struct Client *source_p, const char *level, const char *mask)
{
	unsigned int newflags = ae_level_from_name(level);

	if (level == NULL || mask == NULL)
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name,
			   EmptyString(source_p->name) ? "*" : source_p->name,
			   "ACCESS ADD");
		return;
	}

	if (!can_upsert_on_access_list(chptr, source_p, mask, newflags))
	{
		sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
			me.name, source_p->name, chptr->chname);
		return;
	}

	/* only enforce ACL limit on non-upsert condition */
	if (rb_dlink_list_length(&chptr->access_list) + 1 > ConfigChannel.max_bans &&
	    channel_access_find(chptr, mask) == NULL)
	{
		sendto_one_numeric(source_p, ERR_ACCESS_TOOMANY, form_str(ERR_ACCESS_TOOMANY),
			chptr->chname);
		return;
	}

	struct AccessEntry *ae = channel_access_upsert(chptr, source_p, mask, newflags);

	if (MyClient(source_p))
	{
		sendto_one_numeric(source_p, RPL_ACCESSADD, form_str(RPL_ACCESSADD),
			chptr->chname, ae_level_name(ae->flags), ae->mask,
			(long) 0, ae->who, "");
	}

	sendto_server(source_p, chptr, CAP_TS6, NOCAPS,
		":%s ACCESS %s ADD %s %s",
		use_id(source_p), chptr->chname, ae_level_name(ae->flags), ae->mask);
}

static void
apply_access_entries(struct Channel *chptr, struct Client *client_p)
{
	struct AccessEntry *ae = channel_access_match(chptr, client_p);
	if (ae == NULL)
		return;

	char mode_char = ae_level_char(ae->flags);
	if (!mode_char)
		return;

	struct membership *msptr = find_channel_membership(chptr, client_p);
	s_assert(msptr != NULL);

	sendto_channel_local(&me, ALL_MEMBERS, chptr, ":%s MODE %s +%c %s",
			me.name, chptr->chname, mode_char, client_p->name);
	sendto_server(NULL, chptr, CAP_TS6, NOCAPS,
			":%s TMODE %ld %s +%c %s",
			me.id, (long) chptr->channelts, chptr->chname,
			mode_char, client_p->id);
	msptr->flags |= ae->flags;
}

static void
handle_access_sync(struct Channel *chptr, struct Client *source_p)
{
	const struct membership *source_msptr = find_channel_membership(chptr, source_p);
	const rb_dlink_node *iter;

	if (source_msptr == NULL || !is_admin(source_msptr))
	{
		sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
			me.name, source_p->name, chptr->chname);
		return;
	}

	RB_DLINK_FOREACH(iter, chptr->members.head)
	{
		const struct membership *msptr = iter->data;

		apply_access_entries(chptr, msptr->client_p);
	}

	sendto_server(source_p, chptr, CAP_TS6, NOCAPS,
		":%s ACCESS %s SYNC",
		use_id(source_p), chptr->chname);
}

static void
m_access(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = find_channel(parv[1]);
	if (chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return;
	}

	if (parv[2] == NULL || !rb_strcasecmp(parv[2], "LIST"))
		handle_access_list(chptr, source_p, parv[3]);
	else if (!rb_strcasecmp(parv[2], "ADD"))
		handle_access_upsert(chptr, source_p, parv[3], parv[4]);
	else if (!rb_strcasecmp(parv[2], "DEL") || !rb_strcasecmp(parv[2], "DELETE"))
		handle_access_delete(chptr, source_p, parv[4]);
	else if (!rb_strcasecmp(parv[2], "CLEAR"))
		handle_access_clear(chptr, source_p, parv[3]);
	else if (!rb_strcasecmp(parv[2], "SYNC"))
		handle_access_sync(chptr, source_p);
}

/*
 * TACCESS command (bursting).
 *
 * parv[0] = source
 * parv[1] = channel name
 * parv[2] = channel ts
 * parv[3] = entry ts
 * parv[4] = mask
 * parv[5] = level
 */
static void
ms_taccess(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = find_channel(parv[1]);
	if (chptr == NULL)
		return;

	time_t creation_ts = atol(parv[2]);
	time_t entry_ts = atol(parv[3]);

	if (creation_ts > chptr->channelts)
		return;

	unsigned int flags = ae_level_from_name(parv[5]);
	if (flags == 0)
		return;

	struct AccessEntry *ae = channel_access_upsert(chptr, source_p, parv[4], flags);
	ae->when = entry_ts;

	sendto_server(source_p, chptr, CAP_TS6, NOCAPS,
		":%s TACCESS %s %ld %ld %s %s",
		use_id(source_p), chptr->chname, creation_ts, entry_ts,
		ae->mask, ae_level_name(ae->flags));
}

/* channel join hook */
static void
h_access_channel_join(void *vdata)
{
	hook_data_channel_activity *data = vdata;
	struct Channel *chptr = data->chptr;
	struct Client *client_p = data->client;

	apply_access_entries(chptr, client_p);
}

/* burst hook */
static void
h_access_burst_channel(void *vdata)
{
	hook_data_channel *hchaninfo = vdata;
	struct Channel *chptr = hchaninfo->chptr;
	struct Client *client_p = hchaninfo->client;
	rb_dlink_node *it;

	RB_DLINK_FOREACH(it, chptr->access_list.head)
	{
		struct AccessEntry *ae = it->data;

		sendto_one(client_p, ":%s TACCESS %s %ld %ld %s %s",
			use_id(&me), chptr->chname, (long) chptr->channelts, ae->when,
			ae->mask, ae_level_name(ae->flags));
	}
}

static void
h_access_channel_lowerts(void *vdata)
{
	hook_data_channel *hchaninfo = vdata;
	struct Channel *chptr = hchaninfo->chptr;

	channel_access_clear(chptr);
}
