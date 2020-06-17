/*
 * modules/m_ircx_base.c
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
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "supported.h"

static const char ircx_base_desc[] = "Provides core IRCX capabilities";

static unsigned int CAP_IRCX;
static int ircx_base_init(void);
static void ircx_base_deinit(void);

mapi_cap_list_av2 ircx_base_cap_list[] = {
	{ MAPI_CAP_SERVER, "IRCX", NULL, &CAP_IRCX },
	{ 0, NULL, NULL, NULL }
};

static void m_ircx(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message ircx_msgtab = {
	"IRCX", 0, 0, 0, 0,
	{{m_ircx, 0}, mg_ignore, mg_ignore, mg_ignore, mg_ignore, mg_ignore}
};

struct Message isircx_msgtab = {
	"ISIRCX", 0, 0, 0, 0,
	{mg_unreg, {m_ircx, 0}, mg_ignore, mg_ignore, mg_ignore, {m_ircx, 0}}
};

mapi_clist_av1 ircx_base_clist[] = { &ircx_msgtab, &isircx_msgtab, NULL };

DECLARE_MODULE_AV2(ircx_base, ircx_base_init, ircx_base_deinit, ircx_base_clist, NULL, NULL, ircx_base_cap_list, NULL, ircx_base_desc);

static int
ircx_base_init(void)
{
	add_isupport("IRCX", isupport_string, "");

	capability_require(serv_capindex, "IRCX");
}

static void
ircx_base_deinit(void)
{
	delete_isupport("IRCX");
}

static void
m_ircx(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	sendto_one_numeric(client_p, RPL_IRCX, form_str(RPL_IRCX));
}
