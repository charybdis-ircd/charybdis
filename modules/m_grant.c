/*
 * charybdis: an advanced ircd
 * m_grant.c: Command module to set operator access levels.
 *
 * Copyright (C) 2006 Jilles Tjoelker
 * Copyright (C) 2006 Stephen Bennett <spb@gentoo.org>
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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
#include "numeric.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "s_user.h"
#include "s_serv.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "role.h"


static
void set_mode(struct Client *const target,
              const char *const str)
{
	const char *mode[] =
	{
		target->name,
		target->name,
		str,
		NULL
	};

	user_mode(target, target, 3,mode);
}


static
void set_role(struct Client *const source,
              struct Client *const target,
              const char *const role_name)
{
	struct Role *const role = role_get(role_name);
	if(!role)
	{
		sendto_one_notice(source, ":There is no role named '%s'.", role_name);
		return;
	}

	if(IsOper(target) && target->localClient->role == role)
	{
		sendto_one_notice(source, ":%s already has role of %s.", role_name);
		return;
	}

	if(IsOper(target))
	{
		sendto_one_notice(target, ":%s has changed your role to %s.", source->name, role->name);
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s has changed %s's role to %s.", get_oper_name(source), target->name, role->name);
		target->localClient->role = role;
		return;
	}

	struct oper_conf oper =
	{
		.name = (char *)role->name,
		.role = role,
	};

	oper_up(target, &oper);
	set_mode(target, "+o");
	sendto_one_notice(target, ":%s has granted you the role of %s.", source->name, role->name);
	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s has granted %s the role of %s.", get_oper_name(source), target->name, role->name);
}


static
void deoper(struct Client *const source,
            struct Client *const target)
{
	if(!IsOper(target))
	{
		sendto_one_notice(source, ":You can't deoper someone who isn't an oper.");
		return;
	}

	set_mode(target, "-o");
	sendto_one_notice(target, ":%s has deopered you.", source->name);
	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s has deopered %s.", get_oper_name(source), target->name);
}


static
int grant(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *const target_p = find_person(parv[1]);

	if(!target_p)
	{
		if(IsPerson(source_p))
			sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);

		return 0;
	}

	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_GRANT))
	{
		sendto_one_notice(source_p, ":GRANT failed: You have no shared configuration block on this server.");
		return 0;
	}

	if(MyClient(target_p))
	{
		if(irccmp(parv[2], "deoper") == 0)
			deoper(source_p, target_p);
		else
			set_role(source_p, target_p, parv[2]);
	}
	else if(MyClient(source_p))
	{
		sendto_one(target_p, ":%s ENCAP %s GRANT %s %s",
		           get_id(source_p, target_p),
		           target_p->servptr->name,
		           get_id(target_p, target_p),
		           parv[2]);
	}

	return 0;
}

struct Message msgtab =
{
	"GRANT", 0, 0, 0, MFLG_SLOW,
	{
		mg_ignore,
		mg_not_oper,
		mg_ignore,
		mg_ignore,
		{ grant, 3 },
		{ grant, 3 }
	}
};

mapi_clist_av1 clist[] =
{
	&msgtab,
	NULL
};

DECLARE_MODULE_AV1
(
	grant,
	NULL,
	NULL,
	clist,
	NULL,
	NULL,
	"$Revision$"
);
