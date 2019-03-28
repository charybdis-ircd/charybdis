/*
 * "is available for help." for charybdis 3.6.
 * modular for obvious reasons. (not worth coring it)
 */

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
#include "privilege.h"
#include "s_newconf.h"
#include "newconf.h"


static void h_helpop_whois(hook_data_client *);
static void h_helpop_high_whois(hook_data_client *);
mapi_hfn_list_av1 whois_helpop_hfnlist[] = {
	{ "doing_whois",	(hookfn) h_helpop_whois },
	{ "doing_whois_global",	(hookfn) h_helpop_whois },
	{ "doing_whois_top",	(hookfn) h_helpop_high_whois },
	{ "doing_whois_top_global",	(hookfn) h_helpop_high_whois },
	{ NULL, NULL }
};

static void check_umode_change(void *data);
char *helpopstring = "";
char helpoploc = 0;

#define IsUnrealStyle()	(helpoploc != 0)
#define IsChatdStyle()	(helpoploc == 0)

static void
conf_set_helpopstring(void *data)
{
	helpopstring = rb_strdup(data);
}

static void
conf_set_helpoploc(void *data)
{
	helpoploc = *(unsigned int *)data;
}

static void
helpop_whois(hook_data_client *data)
{
	if(!EmptyString(helpopstring) && IsHelpOp(data->target))
	{
		sendto_one_numeric(data->client, RPL_WHOISSPECIAL,
				form_str(RPL_WHOISSPECIAL),
				data->target->name, helpopstring);
	}
}

static void h_helpop_whois (hook_data_client *data)
{
	if (IsChatdStyle()) helpop_whois(data);
}

static void h_helpop_high_whois (hook_data_client *data)
{
	if (IsUnrealStyle()) helpop_whois(data);
}

static int
_modinit(void)
{
	/* add the usermode to the available slot */
	add_conf_item("general", "helpopstring", CF_QSTRING, conf_set_helpopstring);
	add_conf_item("general", "helpop_unreal_loc", CF_YESNO, conf_set_helpoploc);
	construct_umodebuf();

	return 0;
}

static void
_moddeinit(void)
{
	/* disable the umode and remove it from the available list */
	remove_conf_item("general", "helpopstring");
	remove_conf_item("general", "helpop_unreal_loc");
	construct_umodebuf();
}

DECLARE_MODULE_AV1(whois_helpop, _modinit, _moddeinit, NULL, NULL,
			whois_helpop_hfnlist, "$Revision: 3526 $");

