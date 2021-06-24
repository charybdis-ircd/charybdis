/*
 *  ircd-ratbox: A slightly useful ircd.
 *  spy_stats_p_notice.c: Sends a notice when someone uses STATS p.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */
#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

static const char spy_desc[] = "Sends a notice when someone looks at the operator list";

void show_stats_p(hook_data *);

mapi_hfn_list_av1 stats_p_hfnlist[] = {
	{"doing_stats_p", (hookfn) show_stats_p},
	{NULL, NULL}
};

DECLARE_MODULE_AV2(stats_p_spy, NULL, NULL, NULL, NULL, stats_p_hfnlist, NULL, NULL, spy_desc);

void
show_stats_p(hook_data *data)
{
	sendto_realops_snomask(SNO_SPY, L_NETWIDE,
			     "STATS p requested by %s (%s@%s) [%s]",
			     data->client->name, data->client->username,
			     data->client->host, data->client->servptr->name);
}
