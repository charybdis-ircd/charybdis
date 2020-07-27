/*
 * modules/m_svsumode.c
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
#include "hash.h"

static const char svsumode_desc[] = "Provides SVSUMODE command.";

/* :NickServ ENCAP * SVSUMODE uid umode_changes */
static void me_svsumode(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message svsumode_msgtab = {
	"SVSUMODE", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_svsumode, 2}, mg_ignore}
};

mapi_clist_av1 svsumode_clist[] = { &svsumode_msgtab, NULL };

DECLARE_MODULE_AV2(svsumode, NULL, NULL, svsumode_clist, NULL, NULL, NULL, NULL, svsumode_desc);

/*
 * SVSUMODE
 *
 * parv[0] = service
 * parv[1] = target
 * parv[2] = umode changes
 */
static void
me_svsumode(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	bool add = true;
	const char *p = parv[2];
	struct Client *target_p = find_client(parv[1]);
	int oldumodes;

	if (target_p == NULL)
		return;

	if (!IsService(source_p))
		return;

	oldumodes = target_p->umodes;

	while (*p)
	{
		const char mode = *p;

		switch (mode)
		{
		case '+':
			add = true;
			break;

		case '-':
			add = false;
			break;

		/* don't allow usermode +p to be set via SVSUMODE */
		case 'p':
			break;

		default:
			if (add)
				target_p->umodes |= user_modes[(unsigned int) *p];
			else
				target_p->umodes &= ~user_modes[(unsigned int) *p];

			break;
		}

		p++;
	}

	/* inform the local client of their usermode change */
	if (MyClient(target_p))
	{
		char chgbuf[BUFSIZE];

		send_umode(target_p, target_p, oldumodes, chgbuf);
	}
}
