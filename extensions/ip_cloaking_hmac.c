/**
 * Cloaks a users hostname using HMAC-MD5.
 *
 * Copyright (C) 2015 Patrick Godschalk
 *
 * This file is part of charybdis.
 *
 * charybdis is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * charybdis is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with charybdis.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <openssl/hmac.h>
#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "hash.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "newconf.h"

// This could be a config option, but since cloaking is not intended
// for proper security to begin with, we don't really care.
char *secretsalt = "32qwnqoWI@DpMd&w";

static void
conf_set_secretsalt(void *data)
{
	secretsalt = rb_strdup(data);
}

static int
_modinit(void)
{
	user_modes['x'] = find_umode_slot();
	construct_umodebuf();

    add_top_conf("cloaking", NULL, NULL, NULL);
    add_conf_item("cloaking", "secretsalt", CF_QSTRING, conf_set_secretsalt);

	return 0;
}

static void
_moddeinit(void)
{
	user_modes['x'] = 0;
	construct_umodebuf();

    add_top_conf("cloaking", NULL, NULL, NULL);
    add_conf_item("cloaking", "secretsalt", CF_QSTRING, conf_set_secretsalt);

}

static void check_umode_change(void *data);
static void check_new_user(void *data);
mapi_hfn_list_av1 ip_cloaking_hfnlist[] = {
	{ "umode_changed", (hookfn) check_umode_change },
	{ "new_local_user", (hookfn) check_new_user },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(ip_cloaking, _modinit, _moddeinit, NULL, NULL,
                   ip_cloaking_hfnlist, "ip_cloaking_hmac");

static void
distribute_hostchange(struct Client *client_p, char *newhost)
{
	if (newhost != client_p->orighost)
		sendto_one_numeric(client_p, RPL_HOSTHIDDEN,
		                   "%s :is now your hidden host", newhost);
	else
		sendto_one_numeric(client_p, RPL_HOSTHIDDEN, "%s :hostname reset",
		                   newhost);

	sendto_server(NULL, NULL, CAP_EUID | CAP_TS6, NOCAPS, ":%s CHGHOST %s :%s",
                  use_id(&me), use_id(client_p), newhost);
	sendto_server(NULL, NULL, CAP_TS6, CAP_EUID, ":%s ENCAP * CHGHOST %s :%s",
                  use_id(&me), use_id(client_p), newhost);

	change_nick_user_host(client_p, client_p->name, client_p->username, newhost,
	                      0, "Changing host");

	if (newhost != client_p->orighost)
		SetDynSpoof(client_p);
	else
		ClearDynSpoof(client_p);
}

static void
do_host_cloak(const char *inbuf, char *outbuf)
{
	unsigned char *hash;
	char buf[3];
	char output[HOSTLEN+1];
	int i;

	hash = HMAC(EVP_md5(), secretsalt, strlen(secretsalt),
	            (unsigned char*)inbuf, strlen(inbuf), NULL, NULL);

	output[0]=0;

    for (i = 0; i < 8; i++) {
        sprintf(buf, "%.2x", hash[i]);
        strcat(output,buf);
    }

	rb_strlcpy(outbuf,output,HOSTLEN+1);
}

static void
check_umode_change(void *vdata)
{
	hook_data_umode_changed *data = (hook_data_umode_changed *)vdata;
	struct Client *source_p = data->client;

	if (!MyClient(source_p))
		return;

	if (!((data->oldumodes ^ source_p->umodes) & user_modes['x']))
		return;

	if (source_p->umodes & user_modes['x'])
	{
		if (IsIPSpoof(source_p)
			|| source_p->localClient->mangledhost == NULL
			|| (IsDynSpoof(source_p) && strcmp(source_p->host,
			                                   source_p->localClient->mangledhost)))
		{
			source_p->umodes &= ~user_modes['x'];
			return;
		}
		if (strcmp(source_p->host, source_p->localClient->mangledhost))
		{
			distribute_hostchange(source_p, source_p->localClient->mangledhost);
		}
		else
			sendto_one_numeric(source_p, RPL_HOSTHIDDEN,
			                   "%s :is now your hidden host", source_p->host);
	}
	else if (!(source_p->umodes & user_modes['x']))
	{
		if (source_p->localClient->mangledhost != NULL &&
			!strcmp(source_p->host, source_p->localClient->mangledhost))
		{
			distribute_hostchange(source_p, source_p->orighost);
		}
	}
}

static void
check_new_user(void *vdata)
{
	struct Client *source_p = (void *)vdata;

	if (IsIPSpoof(source_p))
	{
		source_p->umodes &= ~user_modes['x'];
		return;
	}

	source_p->localClient->mangledhost = rb_malloc(HOSTLEN + 1);
	do_host_cloak(source_p->orighost, source_p->localClient->mangledhost);
	if (IsDynSpoof(source_p))
		source_p->umodes &= ~user_modes['x'];
	if (source_p->umodes & user_modes['x'])
	{
		rb_strlcpy(source_p->host, source_p->localClient->mangledhost,
		           sizeof(source_p->host));
		if (irccmp(source_p->host, source_p->orighost))
			SetDynSpoof(source_p);
	}
}
