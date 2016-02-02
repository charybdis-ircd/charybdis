/*
 * m_role.c: Shows effective operator privileges
 *
 * Copyright (C) 2016 Jason Volk
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
#include "client.h"
#include "common.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "irc_radixtree.h"
#include "role.h"


static
int role(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *const role_name = parc >= 2? parv[1] : NULL;
	const char *const targ_name = parc <= 1? source_p->name : "*";

	struct Role *const role = role_name?              role_get(role_name):
	                          source_p->localClient?  source_p->localClient->role:
	                                                  NULL;

	if(!role)
	{
		sendto_one_numeric(source_p, RPL_ROLE, form_str(RPL_ROLE), targ_name, "-",
		                   "-",
		                   "Role not found.");
		return 0;
	}

	if(role->extends)
		sendto_one_numeric(source_p, RPL_ROLE, form_str(RPL_ROLE), targ_name, role->name,
		                   "EXTENDS",
		                   role->extends);

	if(role->umodes)
		sendto_one_numeric(source_p, RPL_ROLE, form_str(RPL_ROLE), targ_name, role->name,
		                   "UMODES",
		                   role->umodes);

	if(role->chmodes)
		sendto_one_numeric(source_p, RPL_ROLE, form_str(RPL_ROLE), targ_name, role->name,
		                   "CHMODES",
		                   role->chmodes);

	if(role->stats)
		sendto_one_numeric(source_p, RPL_ROLE, form_str(RPL_ROLE), targ_name, role->name,
		                   "STATS",
		                   role->stats);

	if(role->snotes)
		sendto_one_numeric(source_p, RPL_ROLE, form_str(RPL_ROLE), targ_name, role->name,
		                   "SNOTES",
		                   role->snotes);

	if(role->exempts)
		sendto_one_numeric(source_p, RPL_ROLE, form_str(RPL_ROLE), targ_name, role->name,
		                   "EXEMPTS",
		                   role->exempts);

	if(role->cmds)
	{
		static const char *const prefix = "COMMANDS";
		static char buf[BUFSIZE-NICKLEN-1-ROLE_NAME_MAXLEN-1-8-1-1];
		buf[0] = '\0';

		void *e;
		struct irc_radixtree_iteration_state state;
		IRC_RADIXTREE_FOREACH(e, &state, role->cmds)
		{
			const char *const key = irc_radixtree_elem_get_key(state.pspare[0]);
			if(strlen(buf) + strlen(key) + 2 >= sizeof(buf))
			{
				sendto_one_numeric(source_p, RPL_ROLE, form_str(RPL_ROLE), targ_name, role->name,
				                   prefix,
				                   buf);
				buf[0] = '\0';
			}

			rb_strlcat(buf, key, sizeof(buf));
			rb_strlcat(buf, " ", sizeof(buf));
		}

		sendto_one_numeric(source_p, RPL_ROLE, form_str(RPL_ROLE), targ_name, role->name,
		                   prefix,
		                   buf);
	}

	return 0;
}


struct Message msgtab =
{
	"ROLE", 0, 0, 0, MFLG_SLOW,
	{
		mg_unreg,
		mg_not_oper,
		mg_not_oper,
		mg_ignore,
		mg_ignore,
		{ role, 0 }
	}
};

mapi_clist_av1 clist[] =
{
	&msgtab,
	NULL
};

DECLARE_MODULE_AV1
(
	role,
	NULL,
	NULL,
	clist,
	NULL,
	NULL,
	""
);
