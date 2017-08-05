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

#include "stdinc.h"
#include "ircd_defs.h"
#include "msg.h"
#include "client.h"

#define TEST_NICK "test"
#define TEST_USERNAME "username"
#define TEST_HOSTNAME "example.test"
#define TEST_IP "2001:db8::1:5ee:bad:c0de"
#define TEST_REALNAME "Test user"

#define CRLF "\r\n"

void client_util_init(void);
void client_util_free(void);

struct Client *make_local_person(void);
struct Client *make_local_person_nick(const char *nick);
struct Client *make_local_person_full(const char *nick, const char *username, const char *hostname, const char *ip, const char *realname);
void remove_local_person(struct Client *client);

char *get_client_sendq(const struct Client *client);

#define is_client_sendq_empty(client, message, ...) do { \
		is_string("", get_client_sendq(client), message, ##__VA_ARGS__); \
	} while (0)

#define is_client_sendq(queue, client, message, ...) do { \
		is_string(queue, get_client_sendq(client), message, ##__VA_ARGS__); \
		is_client_sendq_empty(client, message, ##__VA_ARGS__); \
	} while (0)
