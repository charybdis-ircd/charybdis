/*
 *  send1.c: Test sendto_* under various conditions
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

#include "ircd_util.h"
#include "client_util.h"

#include "send.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

static void sendto_one_numeric1(void)
{
	struct Client *user = make_local_person();

	sendto_one_numeric(user, 1, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " 001 " TEST_NICK " Hello World!" CRLF, user, MSG);

	remove_local_person(user);
}

static void sendto_wallops_flags1(void)
{
	struct Client *user1 = make_local_person_nick("user1");
	struct Client *user2 = make_local_person_nick("user2");
	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");
	struct Client *oper3 = make_local_person_nick("oper3");
	struct Client *oper4 = make_local_person_nick("oper4");

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);
	make_local_person_oper(oper3);
	make_local_person_oper(oper4);

	user1->umodes |= UMODE_WALLOP;
	oper1->umodes |= UMODE_WALLOP | UMODE_OPERWALL;
	oper2->umodes |= UMODE_WALLOP | UMODE_OPERWALL | UMODE_ADMIN;
	oper3->umodes |= UMODE_WALLOP;
	oper4->umodes |= UMODE_OPERWALL;

	sendto_wallops_flags(UMODE_WALLOP, oper1, "Test to users");
	is_client_sendq(":oper1!" TEST_USERNAME "@" TEST_HOSTNAME " WALLOPS :Test to users" CRLF, user1, "User is +w; " MSG);
	is_client_sendq_empty(user2, "User is -w; " MSG);
	is_client_sendq(":oper1!" TEST_USERNAME "@" TEST_HOSTNAME " WALLOPS :Test to users" CRLF, oper1, "User is +w; " MSG);
	is_client_sendq(":oper1!" TEST_USERNAME "@" TEST_HOSTNAME " WALLOPS :Test to users" CRLF, oper2, "User is +w; " MSG);
	is_client_sendq(":oper1!" TEST_USERNAME "@" TEST_HOSTNAME " WALLOPS :Test to users" CRLF, oper3, "User is +w; " MSG);
	is_client_sendq_empty(oper4, "User is -w; " MSG);

	sendto_wallops_flags(UMODE_OPERWALL, oper2, "Test to opers");
	is_client_sendq_empty(user1, "Not an oper; " MSG);
	is_client_sendq_empty(user2, "Not an oper; " MSG);
	is_client_sendq(":oper2!" TEST_USERNAME "@" TEST_HOSTNAME " WALLOPS :Test to opers" CRLF, oper1, "Oper is +z; " MSG);
	is_client_sendq(":oper2!" TEST_USERNAME "@" TEST_HOSTNAME " WALLOPS :Test to opers" CRLF, oper2, "Oper is +z; " MSG);
	is_client_sendq_empty(oper3, "Oper is -z; " MSG);
	is_client_sendq(":oper2!" TEST_USERNAME "@" TEST_HOSTNAME " WALLOPS :Test to opers" CRLF, oper4, "Oper is +z; " MSG);

	sendto_wallops_flags(UMODE_ADMIN, &me, "Test to admins");
	is_client_sendq_empty(user1, "Not an admin; " MSG);
	is_client_sendq_empty(user2, "Not an admin; " MSG);
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq(":" TEST_ME_NAME " WALLOPS :Test to admins" CRLF, oper2, MSG);
	is_client_sendq_empty(oper3, "Not an admin; " MSG);
	is_client_sendq_empty(oper4, "Not an admin; " MSG);

	remove_local_person(user1);
	remove_local_person(user2);
	remove_local_person(oper1);
	remove_local_person(oper2);
	remove_local_person(oper3);
	remove_local_person(oper4);
}

int main(int argc, char *argv[])
{
	plan_lazy();

	ircd_util_init(__FILE__);
	client_util_init();

	sendto_one_numeric1();
	sendto_wallops_flags1();

	client_util_free();
	ircd_util_free();
	return 0;
}
