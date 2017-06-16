/* SSL extban type: matches ssl users */

/* This file is available under the same conditions as the rest of
   https://github.com/asterIRC/ircd-chatd, and by extension, the rest
   of Charybdis. */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static int _modinit(void);
static void _moddeinit(void);
static int eb_ssl(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV1(extb_ssl, _modinit, _moddeinit, NULL, NULL, NULL, "1.05");

static int
_modinit(void)
{
	extban_table['z'] = eb_ssl;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['z'] = NULL;
}

static int eb_ssl(const char *data, struct Client *client_p,
                  struct Channel *chptr, long mode_type)
{

	(void)chptr;
	(void)mode_type;

	if (data != NULL)
	{
		if (EmptyString(client_p->certfp))
			return EXTBAN_NOMATCH;

		return !irccmp(data, client_p->certfp) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
	}

	return IsSSLClient(client_p) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
