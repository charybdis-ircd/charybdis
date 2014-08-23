/* 
 * Charybdis: an advanced ircd
 * ip_cloaking.c: provide user hostname cloaking
 *
 * Written originally by nenolod, altered to use FNV by Elizabeth in 2008
 * altered some more by groente
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

char *secretsalt = "32qwnqoWI@DpMd&w";

static void
conf_set_secretsalt(void *data)
{
	secretsalt = rb_strdup(data);
}

static int
_modinit(void)
{
	/* add the usermode to the available slot */
	user_modes['h'] = find_umode_slot();
	construct_umodebuf();

        add_top_conf("cloaking", NULL, NULL, NULL);
        add_conf_item("cloaking", "secretsalt", CF_QSTRING, conf_set_secretsalt);

	return 0;
}

static void
_moddeinit(void)
{
	/* disable the umode and remove it from the available list */
	user_modes['h'] = 0;
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
			ip_cloaking_hfnlist, "$Revision: 3526 $");

static void
distribute_hostchange(struct Client *client_p, char *newhost)
{
	if (newhost != client_p->orighost)
		sendto_one_numeric(client_p, RPL_HOSTHIDDEN, "%s :is now your hidden host",
			newhost);
	else
		sendto_one_numeric(client_p, RPL_HOSTHIDDEN, "%s :hostname reset",
			newhost);

	sendto_server(NULL, NULL,
		CAP_EUID | CAP_TS6, NOCAPS, ":%s CHGHOST %s :%s",
		use_id(&me), use_id(client_p), newhost);
	sendto_server(NULL, NULL,
		CAP_TS6, CAP_EUID, ":%s ENCAP * CHGHOST %s :%s",
		use_id(&me), use_id(client_p), newhost);

	change_nick_user_host(client_p, client_p->name, client_p->username, newhost, 0, "Changing host");

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

	hash = HMAC(EVP_sha256(), secretsalt, strlen(secretsalt), (unsigned char*)inbuf, strlen(inbuf), NULL, NULL);

	output[0]=0;

        for (i = 0; i < 32; i++) {
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

	/* didn't change +h umode, we don't need to do anything */
	if (!((data->oldumodes ^ source_p->umodes) & user_modes['h']))
		return;

	if (source_p->umodes & user_modes['h'])
	{
		if (IsIPSpoof(source_p) || source_p->localClient->mangledhost == NULL || (IsDynSpoof(source_p) && strcmp(source_p->host, source_p->localClient->mangledhost)))
		{
			source_p->umodes &= ~user_modes['h'];
			return;
		}
		if (strcmp(source_p->host, source_p->localClient->mangledhost))
		{
			distribute_hostchange(source_p, source_p->localClient->mangledhost);
		}
		else /* not really nice, but we need to send this numeric here */
			sendto_one_numeric(source_p, RPL_HOSTHIDDEN, "%s :is now your hidden host",
				source_p->host);
	}
	else if (!(source_p->umodes & user_modes['h']))
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
		source_p->umodes &= ~user_modes['h'];
		return;
	}
	source_p->localClient->mangledhost = rb_malloc(HOSTLEN + 1);
	do_host_cloak(source_p->orighost, source_p->localClient->mangledhost);
	if (IsDynSpoof(source_p))
		source_p->umodes &= ~user_modes['h'];
	if (source_p->umodes & user_modes['h'])
	{
		rb_strlcpy(source_p->host, source_p->localClient->mangledhost, sizeof(source_p->host));
		if (irccmp(source_p->host, source_p->orighost))
			SetDynSpoof(source_p);
	}
}
