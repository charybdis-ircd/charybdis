/*
 * modules/cap_sts.c
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

static const char cap_sts_desc[] =
	"Provides the sts client capability";

unsigned int CLICAP_STS = 0;

static bool
sts_visible(struct Client *client_p)
{
	(void) client_p;

	return STSInfo.enabled != 0;
}

static const char *
sts_data(struct Client *client_p)
{
	static char buf[BUFSIZE];

	if (!IsSSL(client_p))
		snprintf(buf, sizeof buf, "port=%d", STSInfo.port);
	else
		snprintf(buf, sizeof buf, "duration=%ld%s",
			 STSInfo.duration, STSInfo.preload ? ",preload" : "");

	return buf;
}

static struct ClientCapability capdata_sts = {
	.visible = sts_visible,
	.data = sts_data,
	.flags = CLICAP_FLAGS_STICKY
};

mapi_cap_list_av2 cap_sts_cap_list[] = {
	{ MAPI_CAP_CLIENT, "sts", &capdata_sts, &CLICAP_STS },
	{ 0, NULL, NULL, NULL },
};

DECLARE_MODULE_AV2(cap_sts, NULL, NULL, NULL, NULL, NULL, cap_sts_cap_list, NULL, cap_sts_desc);
