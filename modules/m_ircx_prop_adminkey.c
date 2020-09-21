/*
 * modules/m_ircx_prop_adminkey.c
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

static const char ircx_prop_adminkey_desc[] = "Provides support for the ADMINKEY channel property";

static void h_prop_channel_join(void *);
static void h_prop_authorize(void *);

mapi_hfn_list_av1 ircx_prop_adminkey_hfnlist[] = {
	{ "channel_join", (hookfn) h_prop_channel_join },
	{ "prop_show", (hookfn) h_prop_authorize },
	{ "prop_chan_write", (hookfn) h_prop_authorize },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(ircx_prop_adminkey, NULL, NULL, NULL, NULL, ircx_prop_adminkey_hfnlist, NULL, NULL, ircx_prop_adminkey_desc);

/* handle ADMINKEY property on channel join */
static void
h_prop_channel_join(void *vdata)
{
	hook_data_channel_activity *data = (hook_data_channel_activity *)vdata;
	struct Channel *chptr = data->chptr;
	struct Client *source_p = data->client;

	if (!MyClient(source_p))
		return;

	if (rb_likely(data->key == NULL))
		return;

	rb_dlink_list *prop_list = &chptr->prop_list;
	struct Property *prop = propertyset_find(prop_list, "OWNERKEY");

	if (prop == NULL)
		prop = propertyset_find(prop_list, "ADMINKEY");

	if (prop == NULL)
		return;

	if (strcmp(prop->value, data->key))
		return;

	/* do it the hard way for now */
	struct membership *msptr = find_channel_membership(chptr, source_p);
	s_assert(msptr != NULL);

	sendto_channel_local(&me, ALL_MEMBERS, chptr, ":%s MODE %s +q %s",
			me.name, chptr->chname, source_p->name);
	sendto_server(NULL, chptr, CAP_TS6, NOCAPS,
			":%s TMODE %ld %s +q %s",
			me.id, (long) chptr->channelts, chptr->chname,
			source_p->id);
	msptr->flags |= CHFL_ADMIN;

#if 0
	const char *para[] = {"+q", source_p->name};
	set_channel_mode(source_p, &me, chptr, NULL, 2, para);
#endif
}

static void
h_prop_authorize(void *vdata)
{
	hook_data_prop_activity *data = vdata;

	if (!IsChanPrefix(*data->target))
		return;

	if (rb_strcasecmp(data->key, "ADMINKEY") || rb_strcasecmp(data->key, "OWNERKEY"))
		return;

	data->approved = data->alevel >= CHFL_ADMIN;
}
