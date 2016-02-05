/************************************************************************
 *   IRC - Internet Relay Chat, extensions/spamfilter.c
 *   Copyright (C) 2016 Jason Volk
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 */

#include "stdinc.h"
#include "s_conf.h"
#include "modules.h"
#include "hook.h"
#include "send.h"
#include "chmode.h"
#include "newconf.h"
#include "substitution.h"
#include "spamfilter.h"
#define CHM_PRIVS chm_staff


int chm_spamfilter;
int h_spamfilter_query;
int h_spamfilter_reject;

char reject_reason[BUFSIZE - CHANNELLEN - 32];


static
void hook_privmsg_channel(hook_data_privmsg_channel *const hook)
{
	// Check for mootness by other hooks first
	if(hook->approved != 0 || EmptyString(hook->text))
		return;

	// Assess channel related
	if(~hook->chptr->mode.mode & chm_spamfilter)
		return;

	// Assess client related
	if(IsExemptSpambot(hook->source_p))
		return;

	// Assess type related
	if(hook->msgtype != MESSAGE_TYPE_NOTICE && hook->msgtype != MESSAGE_TYPE_PRIVMSG)
		return;

	// Invoke the active spamfilters
	call_hook(h_spamfilter_query,hook);
	if(hook->approved == 0)
		return;

	call_hook(h_spamfilter_reject,hook);
	sendto_realops_snomask(SNO_REJ|SNO_BOTS,L_NETWIDE,
	                       "spamfilter: REJECT %s[%s@%s] on %s to %s (%s)",
	                       hook->source_p->name,
	                       hook->source_p->username,
	                       hook->source_p->orighost,
	                       hook->source_p->servptr->name,
	                       hook->chptr->chname,
	                       hook->reason?: "filter gave no reason");

	hook->reason = reject_reason;
}


static
void substitute_reject_reason(void)
{
	rb_dlink_list subs = {0};
	substitution_append_var(&subs,"network-name",ServerInfo.network_name?: "${network-name}");
	substitution_append_var(&subs,"admin-email",AdminInfo.email?: "${admin-email}");
	const char *const substituted = substitution_parse(reject_reason,&subs);
	rb_strlcpy(reject_reason,substituted,sizeof(reject_reason));
	substitution_free(&subs);
}


static
void set_reject_reason(void *const str)
{
	rb_strlcpy(reject_reason,str,sizeof(reject_reason));
	substitute_reject_reason();
}


struct ConfEntry conf_spamfilter[] =
{
	{ "reject_reason",     CF_QSTRING,   set_reject_reason, 0,  NULL },
	{ "\0", 0, NULL, 0, NULL }
};


static
int modinit(void)
{
	chm_spamfilter = cflag_add(MODE_SPAMFILTER,CHM_PRIVS);
	if(!chm_spamfilter)
		return -1;

	add_top_conf("spamfilter",NULL,NULL,conf_spamfilter);
	return 0;
}


static
void modfini(void)
{
	remove_top_conf("spamfilter");
	cflag_orphan(MODE_SPAMFILTER);
}


mapi_clist_av1 clist[] =
{
	NULL
};

mapi_hlist_av1 hlist[] =
{
	{ "spamfilter_query", &h_spamfilter_query,        },
	{ "spamfilter_reject", &h_spamfilter_reject,      },
	{ NULL, NULL                                      }
};

mapi_hfn_list_av1 hfnlist[] =
{
	{ "privmsg_channel", (hookfn)hook_privmsg_channel      },
	{ NULL, NULL                                           }
};

DECLARE_MODULE_AV1
(
	spamfilter,
	modinit,
	modfini,
	clist,
	hlist,
	hfnlist,
	"$Revision: 0 $"
);
