/*
 * ircd/propertyset.c
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
#include "hash.h"
#include "whowas.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_assert.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "s_conf.h"
#include "client.h"
#include "send.h"
#include "logger.h"
#include "scache.h"

#include "propertyset.h"

struct Property *
propertyset_find(const rb_dlink_list *prop_list, const char *name)
{
	rb_dlink_node *cur;

	RB_DLINK_FOREACH(cur, prop_list->head)
	{
		struct Property *prop = cur->data;

		if (!strcasecmp(prop->name, name))
			return prop;
	}

	return NULL;
}

static void
propertyset_delete_one(rb_dlink_list *prop_list, struct Property *prop)
{
	s_assert(prop != NULL);

	rb_dlinkDelete(&prop->prop_node, prop_list);

	rb_free(prop->name);
	rb_free(prop->value);
	rb_free(prop->setter);
	rb_free(prop);
}

void
propertyset_delete(rb_dlink_list *prop_list, const char *name)
{
	struct Property *prop = propertyset_find(prop_list, name);

	if (prop == NULL)
		return;

	propertyset_delete_one(prop_list, prop);
}

void
propertyset_clear(rb_dlink_list *prop_list)
{
	rb_dlink_node *cur, *next;

	RB_DLINK_FOREACH_SAFE(cur, next, prop_list->head)
		propertyset_delete_one(prop_list, cur->data);
}

struct Property *
propertyset_add(rb_dlink_list *prop_list, const char *name, const char *value, struct Client *setter_p)
{
	struct Property *prop;

	/* propertyset_add() actually behaves as an upsert. */
	propertyset_delete(prop_list, name);

	prop = rb_malloc(sizeof(*prop));
	prop->set_at = rb_current_time();
	prop->name = rb_strdup(name);
	prop->value = rb_strdup(value);
	prop->setter = rb_strdup(setter_p->name);

	rb_dlinkAdd(prop, &prop->prop_node, prop_list);
}
