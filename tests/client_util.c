/*
 *  client_util.c: Utility functions for making test clients
 *  Copyright 2017 Simon Arlott
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tap/basic.h"

#include "stdinc.h"
#include "ircd_defs.h"
#include "client_util.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

struct Client *make_local_person(void)
{
	return make_local_person_nick(TEST_NICK);
}

struct Client *make_local_person_nick(const char *nick)
{
	return make_local_person_full(nick, TEST_USERNAME, TEST_HOSTNAME, TEST_IP, TEST_REALNAME);
}

struct Client *make_local_person_full(const char *nick, const char *username, const char *hostname, const char *ip, const char *realname)
{
	struct Client *client;

	client = make_client(NULL);
	rb_dlinkMoveNode(&client->localClient->tnode, &unknown_list, &lclient_list);
	client->servptr = &me;
	rb_dlinkAdd(client, &client->lnode, &client->servptr->serv->users);
	SetClient(client);
	make_user(client);

	rb_inet_pton_sock(ip, (struct sockaddr *)&client->localClient->ip);
	rb_strlcpy(client->name, nick, sizeof(client->name));
	rb_strlcpy(client->username, username, sizeof(client->username));
	rb_strlcpy(client->host, hostname, sizeof(client->host));
	rb_inet_ntop_sock((struct sockaddr *)&client->localClient->ip, client->sockhost, sizeof(client->sockhost));
	rb_strlcpy(client->info, realname, sizeof(client->info));

	return client;
}

void remove_local_person(struct Client *client)
{
	exit_client(NULL, client, &me, "Test client removed");
}

char *get_client_sendq(const struct Client *client)
{
	static char buf[EXT_BUFSIZE + sizeof(CRLF)];

	if (rb_linebuf_len(&client->localClient->buf_sendq)) {
		int ret;

		memset(buf, 0, sizeof(buf));
		ret = rb_linebuf_get(&client->localClient->buf_sendq, buf, sizeof(buf), 0, 1);

		if (is_bool(ret > 0, true, MSG)) {
			return buf;
		} else {
			return "<get_client_sendq error>";
		}
	}

	return "";
}

void client_util_init(void)
{
}

void client_util_free(void)
{
}
