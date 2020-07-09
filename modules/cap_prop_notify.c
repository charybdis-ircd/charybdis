/*
 * modules/cap_prop_notify.c
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

static const char cap_prop_notify_desc[] =
	"Provides the ophion.dev/prop-notify client capability";

static void cap_prop_notify_change(hook_data_prop_activity *);
unsigned int CLICAP_PROP_NOTIFY = 0;

mapi_hfn_list_av1 cap_prop_notify_hfnlist[] = {
	{ "prop_change", (hookfn) cap_prop_notify_change },
	{ NULL, NULL }
};

mapi_cap_list_av2 cap_prop_notify_cap_list[] = {
	{ MAPI_CAP_CLIENT, "ophion.dev/prop-notify", NULL, &CLICAP_PROP_NOTIFY },
	{ 0, NULL, NULL, NULL },
};

static void
cap_prop_notify_change(hook_data_prop_activity *data)
{
	char prefix[BUFSIZE];

	// XXX: at present, we only support prop-notify for channels
	if (!IsChanPrefix(*data->target))
		return;

	if (IsPerson(data->client))
		snprintf(prefix, sizeof prefix, "%s!%s@%s",
			data->client->name, data->client->username, data->client->host);
	else
		rb_strlcpy(prefix, data->client->name, sizeof prefix);

	// XXX: deconsting is not really needed but the send API needs to be fixed
	struct Channel *chptr = (void *) data->target_ptr;
	struct Client *client = (void *) data->client;

	sendto_channel_local_with_capability(client, 0, CLICAP_PROP_NOTIFY, 0, chptr,
		":%s PROP %s %s :%s", prefix, data->target, data->key, data->value);
}

DECLARE_MODULE_AV2(cap_prop_notify, NULL, NULL, NULL, NULL, cap_prop_notify_hfnlist, cap_prop_notify_cap_list, NULL, cap_prop_notify_desc);
