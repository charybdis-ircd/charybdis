/*
 * modules/umode_regnick.c
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
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "s_user.h"
#include "numeric.h"
#include "inline/stringops.h"
#include "logger.h"

static const char umode_regnick_desc[] =
	"Adds user mode +r which indicates a user is identified to a registered nickname.";

static void umode_regnick_validate_umode_change(hook_data_umode_changed *data);
static void umode_regnick_nick_change(hook_cdata *data);

mapi_hfn_list_av1 umode_regnick_hfnlist[] = {
	{ "umode_changed", (hookfn) umode_regnick_validate_umode_change },
	{ "local_nick_change", (hookfn) umode_regnick_nick_change },
	{ "remote_nick_change", (hookfn) umode_regnick_nick_change },
	{ NULL, NULL }
};

static int
umode_regnick_modinit(void)
{
	user_modes['r'] = find_umode_slot();

	if (!user_modes['r'])
	{
		ierror("umode_regnick: could not find a usermode slot for +r");
		return -1;
	}

	construct_umodebuf();
	return 0;
}

static void
umode_regnick_moddeinit(void)
{
	user_modes['r'] = 0;
	construct_umodebuf();
}

static void
umode_regnick_validate_umode_change(hook_data_umode_changed *data)
{
	bool changed = ((data->oldumodes ^ data->client->umodes) & user_modes['r']);

	if (!MyClient(data->client))
		return;

	/* users cannot change usermode +r themselves, so undo whatever they did */
	if (!changed)
		return;
	else
	{
		if (data->oldumodes & user_modes['r'])
			data->client->umodes |= user_modes['r'];
		else
			data->client->umodes &= ~user_modes['r'];
	}
}

static void
umode_regnick_nick_change(hook_cdata *data)
{
	char buf[BUFSIZE];
	int old = data->client->umodes;
	const char *oldnick = data->arg1, *newnick = data->arg2;

	if (!irccmp(oldnick, newnick))
		return;

	data->client->umodes &= ~user_modes['r'];

	if (MyClient(data->client))
		send_umode(data->client, data->client, old, buf);
}

DECLARE_MODULE_AV2(umode_regnick, umode_regnick_modinit, umode_regnick_moddeinit, NULL, NULL, umode_regnick_hfnlist, NULL, NULL, umode_regnick_desc);
