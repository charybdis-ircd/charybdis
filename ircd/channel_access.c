/*
 * ircd/channel_access.c
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
#include "channel_access.h"
#include "s_assert.h"

struct AccessEntry *
channel_access_upsert(struct Channel *chptr, struct Client *source_p, const char *mask, unsigned int flags)
{
	s_assert(chptr != NULL);
	s_assert(source_p != NULL);
	s_assert(mask != NULL);

	/* delete any pre-existing entries */
	channel_access_delete(chptr, mask);

	struct AccessEntry *ae = rb_malloc(sizeof(*ae));
	ae->mask = rb_strdup(mask);
	ae->who = rb_strdup(source_p->name);
	ae->flags = flags;
	ae->when = rb_current_time();

	rb_dlinkAdd(ae, &ae->node, &chptr->access_list);

	return ae;
}

struct AccessEntry *
channel_access_find(struct Channel *chptr, const char *mask)
{
	s_assert(chptr != NULL);
	s_assert(mask != NULL);

	rb_dlink_node *iter;

	RB_DLINK_FOREACH(iter, chptr->access_list.head)
	{
		struct AccessEntry *ae = iter->data;

		if (!rb_strcasecmp(ae->mask, mask))
			return ae;
	}

	return NULL;
}

static void
channel_access_delete_one(struct Channel *chptr, struct AccessEntry *ae)
{
	s_assert(chptr != NULL);
	s_assert(ae != NULL);

	rb_dlinkDelete(&ae->node, &chptr->access_list);

	rb_free(ae->mask);
	rb_free(ae->who);
	rb_free(ae);
}

void
channel_access_delete(struct Channel *chptr, const char *mask)
{
	s_assert(chptr != NULL);
	s_assert(mask != NULL);

	struct AccessEntry *ae = channel_access_find(chptr, mask);
	if (ae == NULL)
		return;

	channel_access_delete_one(chptr, ae);
}

void
channel_access_clear(struct Channel *chptr)
{
	s_assert(chptr != NULL);

	rb_dlink_node *iter, *next;

	RB_DLINK_FOREACH_SAFE(iter, next, chptr->access_list.head)
	{
		channel_access_delete_one(chptr, iter->data);
	}
}

struct AccessEntry *
channel_access_match(struct Channel *chptr, struct Client *client_p)
{
	s_assert(chptr != NULL);
	s_assert(client_p != NULL);

	if (!MyClient(client_p))
		return NULL;

	struct matchset ms;
	matchset_for_client(client_p, &ms);

	rb_dlink_node *iter;
	RB_DLINK_FOREACH(iter, chptr->access_list.head)
	{
		struct AccessEntry *ae = iter->data;

		if (matches_mask(&ms, ae->mask))
			return ae;

		if (match_extban(ae->mask, client_p, chptr, CHFL_ACL))
			return ae;
	}

	return NULL;
}
