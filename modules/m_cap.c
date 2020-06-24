/* modules/m_cap.c
 *
 *  Copyright (C) 2005 Lee Hardy <lee@leeh.co.uk>
 *  Copyright (C) 2005 ircd-ratbox development team
 *  Copyright (C) 2016 William Pitcock <nenolod@dereferenced.org>
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
#include "class.h"
#include "client.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "s_conf.h"
#include "hash.h"

static const char cap_desc[] = "Provides the commands used for client capability negotiation";

typedef int (*bqcmp)(const void *, const void *);

static void m_cap(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message cap_msgtab = {
	"CAP", 0, 0, 0, 0,
	{{m_cap, 2}, {m_cap, 2}, mg_ignore, mg_ignore, mg_ignore, {m_cap, 2}}
};

mapi_clist_av1 cap_clist[] = { &cap_msgtab, NULL };

DECLARE_MODULE_AV2(cap, NULL, NULL, cap_clist, NULL, NULL, NULL, NULL, cap_desc);

#define IsCapableEntry(c, e)		IsCapable(c, 1 << (e)->value)
#define HasCapabilityFlag(c, f)		(c->ownerdata != NULL && (((struct ClientCapability *)c->ownerdata)->flags & (f)) == f)

static inline int
clicap_visible(struct Client *client_p, const struct CapabilityEntry *cap)
{
	struct ClientCapability *clicap;

	/* orphaned caps shouldn't be visible */
	if (cap->flags & CAP_ORPHANED)
		return 0;

	if (cap->ownerdata == NULL)
		return 1;

	clicap = cap->ownerdata;
	if (clicap->visible == NULL)
		return 1;

	return clicap->visible(client_p);
}

/* clicap_find()
 *   Used iteratively over a buffer, extracts individual cap tokens.
 *
 * Inputs: buffer to start iterating over (NULL to iterate over existing buf)
 *         int pointer to whether the cap token is negated
 *         int pointer to whether we finish with success
 * Ouputs: Cap entry if found, NULL otherwise.
 */
static struct CapabilityEntry *
clicap_find(const char *data, int *negate, int *finished)
{
	static char buf[BUFSIZE];
	static char *p;
	struct CapabilityEntry *cap;
	char *s;

	*negate = 0;

	if(data)
	{
		rb_strlcpy(buf, data, sizeof(buf));
		p = buf;
	}

	if(*finished)
		return NULL;

	/* skip any whitespace */
	while(*p && IsSpace(*p))
		p++;

	if(EmptyString(p))
	{
		*finished = 1;
		return NULL;
	}

	if(*p == '-')
	{
		*negate = 1;
		p++;

		/* someone sent a '-' without a parameter.. */
		if(*p == '\0')
			return NULL;
	}

	if((s = strchr(p, ' ')))
		*s++ = '\0';

	if((cap = capability_find(cli_capindex, p)) != NULL)
	{
		if(s)
			p = s;
		else
			*finished = 1;
	}

	return cap;
}

/* clicap_generate()
 *   Generates a list of capabilities.
 *
 * Inputs: client to send to, subcmd to send,
 *         flags to match against: 0 to do none, -1 if client has no flags
 * Outputs: None
 */
static void
clicap_generate(struct Client *source_p, const char *subcmd, int flags)
{
	static char buf_prefix[DATALEN + 1];
	static char buf_list[DATALEN + 1];
	const char *str_cont = " * :";
	const char *str_final = " :";
	int len_prefix;
	int max_list;
	struct CapabilityEntry *entry;
	rb_dictionary_iter iter;

	buf_prefix[0] = '\0';
	len_prefix = rb_snprintf_try_append(buf_prefix, sizeof(buf_prefix),
			":%s CAP %s %s",
			me.name,
			EmptyString(source_p->name) ? "*" : source_p->name,
			subcmd);

	/* shortcut, nothing to do */
	if (flags == -1 || len_prefix < 0) {
		sendto_one(source_p, "%s%s", buf_prefix, str_final);
		return;
	}

	buf_list[0] = '\0';
	max_list = sizeof(buf_prefix) - len_prefix - strlen(str_cont);

	RB_DICTIONARY_FOREACH(entry, &iter, cli_capindex->cap_dict) {
		struct ClientCapability *clicap = entry->ownerdata;
		const char *data = NULL;

		if (flags && !IsCapableEntry(source_p, entry))
			continue;

		if (!clicap_visible(source_p, entry))
			continue;

		if (!flags && (source_p->flags & FLAGS_CLICAP_DATA) && clicap != NULL && clicap->data != NULL)
			data = clicap->data(source_p);

		for (int attempts = 0; attempts < 2; attempts++) {
			if (rb_snprintf_try_append(buf_list, max_list, "%s%s%s%s",
					buf_list[0] == '\0' ? "" : " ", /* space between caps */
					entry->cap,
					data != NULL ? "=" : "", /* '=' between cap and data */
					data != NULL ? data : "") < 0
					&& buf_list[0] != '\0') {

				if (!(source_p->flags & FLAGS_CLICAP_DATA)) {
					/* the client doesn't support multiple lines */
					break;
				}

				/* doesn't fit in the buffer, output what we have */
				sendto_one(source_p, "%s%s%s", buf_prefix, str_cont, buf_list);

				/* reset the buffer and go around the loop one more time */
				buf_list[0] = '\0';
			} else {
				break;
			}
		}
	}

	sendto_one(source_p, "%s%s%s", buf_prefix, str_final, buf_list);
}

static void
cap_ack(struct Client *source_p, const char *arg)
{
	struct CapabilityEntry *cap;
	int capadd = 0, capdel = 0;
	int finished = 0, negate;

	if(EmptyString(arg))
		return;

	for(cap = clicap_find(arg, &negate, &finished); cap;
	    cap = clicap_find(NULL, &negate, &finished))
	{
		/* sent an ACK for something they havent REQd */
		if(!IsCapableEntry(source_p, cap))
			continue;

		if(negate)
		{
			/* dont let them ack something sticky off */
			if(HasCapabilityFlag(cap, CLICAP_FLAGS_STICKY))
				continue;

			capdel |= (1 << cap->value);
		}
		else
			capadd |= (1 << cap->value);
	}

	source_p->localClient->caps |= capadd;
	source_p->localClient->caps &= ~capdel;
}

static void
cap_end(struct Client *source_p, const char *arg)
{
	if(IsRegistered(source_p))
		return;

	source_p->flags &= ~FLAGS_CLICAP;

	if(source_p->name[0] && source_p->flags & FLAGS_SENTUSER)
	{
		register_local_user(source_p, source_p);
	}
}

static void
cap_list(struct Client *source_p, const char *arg)
{
	/* list of what theyre currently using */
	clicap_generate(source_p, "LIST",
			source_p->localClient->caps ? source_p->localClient->caps : -1);
}

static void
cap_ls(struct Client *source_p, const char *arg)
{
	int caps_version = 301;

	if(!IsRegistered(source_p))
		source_p->flags |= FLAGS_CLICAP;

	if (!EmptyString(arg)) {
		caps_version = atoi(arg);
	}

	if (caps_version >= 302) {
		source_p->flags |= FLAGS_CLICAP_DATA;
		source_p->localClient->caps |= CLICAP_CAP_NOTIFY;
	}

	/* list of what we support */
	clicap_generate(source_p, "LS", 0);
}

static void
cap_req(struct Client *source_p, const char *arg)
{
	static char buf_prefix[DATALEN + 1];
	static char buf_list[2][DATALEN + 1];
	const char *str_cont = " * :";
	const char *str_final = " :";
	int len_prefix;
	int max_list;
	struct CapabilityEntry *cap;
	int i = 0;
	int capadd = 0, capdel = 0;
	int finished = 0, negate;
	hook_data_cap_change hdata;

	if(!IsRegistered(source_p))
		source_p->flags |= FLAGS_CLICAP;

	if(EmptyString(arg))
		return;

	buf_prefix[0] = '\0';
	len_prefix = rb_snprintf_try_append(buf_prefix, sizeof(buf_prefix),
			":%s CAP %s ACK",
			me.name,
			EmptyString(source_p->name) ? "*" : source_p->name);

	buf_list[0][0] = '\0';
	buf_list[1][0] = '\0';
	max_list = sizeof(buf_prefix) - len_prefix - strlen(str_cont);

	for(cap = clicap_find(arg, &negate, &finished); cap;
	    cap = clicap_find(NULL, &negate, &finished))
	{
		const char *type;

		if(negate)
		{
			if(HasCapabilityFlag(cap, CLICAP_FLAGS_STICKY))
			{
				finished = 0;
				break;
			}

			type = "-";
			capdel |= (1 << cap->value);
		}
		else
		{
			if (!clicap_visible(source_p, cap))
			{
				finished = 0;
				break;
			}

			type = "";
			capadd |= (1 << cap->value);
		}

		/* XXX this probably should exclude REQACK'd caps from capadd/capdel, but keep old behaviour for now */
		if(HasCapabilityFlag(cap, CLICAP_FLAGS_REQACK))
		{
			type = "~";
		}

		for (int attempts = 0; attempts < 2; attempts++) {
			if (rb_snprintf_try_append(buf_list[i], max_list, "%s%s%s",
					buf_list[i][0] == '\0' ? "" : " ", /* space between caps */
					type,
					cap->cap) < 0
					&& buf_list[i][0] != '\0') {

				if (!(source_p->flags & FLAGS_CLICAP_DATA)) {
					/* the client doesn't support multiple lines */
					break;
				}

				/* doesn't fit in the buffer, move to the next one */
				if (i < 2) {
					i++;
				} else {
					/* uh-oh */
					break;
				}

				/* reset the buffer and go around the loop one more time */
				buf_list[i][0] = '\0';
			} else {
				break;
			}
		}
	}

	if(!finished)
	{
		sendto_one(source_p, ":%s CAP %s NAK :%s",
			me.name, EmptyString(source_p->name) ? "*" : source_p->name, arg);
		return;
	}

	if (i) {
		sendto_one(source_p, "%s%s%s", buf_prefix, str_cont, buf_list[0]);
		sendto_one(source_p, "%s%s%s", buf_prefix, str_final, buf_list[1]);
	} else {
		sendto_one(source_p, "%s%s%s", buf_prefix, str_final, buf_list[0]);
	}

	hdata.client = source_p;
	hdata.oldcaps = source_p->localClient->caps;
	hdata.add = capadd;
	hdata.del = capdel;

	source_p->localClient->caps |= capadd;
	source_p->localClient->caps &= ~capdel;

	call_hook(h_cap_change, &hdata);
}

static struct clicap_cmd
{
	const char *cmd;
	void (*func)(struct Client *source_p, const char *arg);
} clicap_cmdlist[] = {
	/* This list *MUST* be in alphabetical order */
	{ "ACK",	cap_ack		},
	{ "END",	cap_end		},
	{ "LIST",	cap_list	},
	{ "LS",		cap_ls		},
	{ "REQ",	cap_req		},
};

static int
clicap_cmd_search(const char *command, struct clicap_cmd *entry)
{
	return irccmp(command, entry->cmd);
}

static void
m_cap(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct clicap_cmd *cmd;

	if(!(cmd = bsearch(parv[1], clicap_cmdlist,
				sizeof(clicap_cmdlist) / sizeof(struct clicap_cmd),
				sizeof(struct clicap_cmd), (bqcmp) clicap_cmd_search)))
	{
		sendto_one(source_p, form_str(ERR_INVALIDCAPCMD),
				me.name, EmptyString(source_p->name) ? "*" : source_p->name,
				parv[1]);
		return;
	}

	(cmd->func)(source_p, parv[2]);
}
