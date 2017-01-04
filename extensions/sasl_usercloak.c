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

#include <stdint.h>

static void check_new_user(void *data);
mapi_hfn_list_av1 sasl_usercloak_hfnlist[] = {
	{ "new_local_user", (hookfn) check_new_user },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(sasl_usercloak, NULL, NULL, NULL, NULL,
			sasl_usercloak_hfnlist, "$Revision: 3526 $");

unsigned int fnv_hash_string(char *str)
{
	unsigned int hash = 0x811c9dc5; // Magic value for 32-bit fnv1 hash initialisation.
	unsigned char *p = (unsigned char *)str;
	while (*p)
	{
		hash += (hash<<1) + (hash<<4) + (hash<<7) + (hash<<8) + (hash<<24);
		hash ^= *p++;
	}
	return hash;
}

static void
check_new_user(void *vdata)
{
	struct Client *source_p = (void *)vdata;

	if (!IsIPSpoof(source_p))
		return;

	if (EmptyString(source_p->user->suser))
		return;

	char *accountpart = strstr(source_p->orighost, "account");
	if (!accountpart)
		return;

	char buf[HOSTLEN];
	memset(buf, 0, sizeof(buf));
	char *dst = buf;

	strncpy(buf, source_p->orighost, accountpart - source_p->orighost);
	dst += accountpart - source_p->orighost;

	int needhash = 0;

	for (char *src = source_p->user->suser; *src ; src++ )
	{
		if (dst > buf + sizeof(buf))
		{
			/* Doesn't fit. Warn opers and bail. */
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					"Couldn't fit account name part %s in hostname for %s!%s@%s",
					source_p->user->suser, source_p->name, source_p->username, source_p->orighost);
			return;
		}

		char c = ToLower(*src);

		if (IsHostChar(c))
			*dst++ = c;
		else
			needhash = 1;
	}

	if (needhash)
	{
		if (dst > buf + sizeof(buf) - 12) /* '/x-' plus eight digit hash plus null terminator */
		{
			/* Doesn't fit. Warn opers and bail. */
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					"Couldn't fit account name part %s in hostname for %s!%s@%s",
					source_p->user->suser, source_p->name, source_p->username, source_p->orighost);
			return;
		}

		*dst++ = '/';
		*dst++ = 'x';
		*dst++ = '-';

		unsigned int hashval = fnv_hash_string(source_p->user->suser);
		hashval %= 100000000; // eight digits only please.
		snprintf(dst, 9, "%08ud", hashval);
	}

	/* just in case */
	buf[HOSTLEN-1] = '\0';

	/* If hostname has been changed already (probably by services cloak on SASL login), then
	 * leave it intact. If not, change it. In either case, update the original hostname.
	 */
	if (0 == irccmp(source_p->host, source_p->orighost))
		change_nick_user_host(source_p, source_p->name, source_p->username, buf, 0, "Changing host");
	strncpy(source_p->orighost, buf, HOSTLEN);
}
