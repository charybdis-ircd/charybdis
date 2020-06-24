/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * hook.c - code for dealing with the hook system
 *
 * This code is basically a slow leaking array.  Events are simply just a
 * position in this array.  When hooks are added, events will be created if
 * they dont exist - this means modules with hooks can be loaded in any
 * order, and events are preserved through module reloads.
 *
 * Copyright (C) 2004-2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2004-2005 ircd-ratbox development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
#include "hook.h"
#include "match.h"

hook *hooks;

#define HOOK_INCREMENT 1000

struct hook_entry
{
	rb_dlink_node node;
	hookfn fn;
	enum hook_priority priority;
};

int num_hooks = 0;
int last_hook = 0;
int max_hooks = HOOK_INCREMENT;

int h_burst_client;
int h_burst_channel;
int h_burst_finished;
int h_server_introduced;
int h_server_eob;
int h_client_exit;
int h_after_client_exit;
int h_umode_changed;
int h_new_local_user;
int h_new_remote_user;
int h_introduce_client;
int h_can_kick;
int h_privmsg_user;
int h_privmsg_channel;
int h_conf_read_start;
int h_conf_read_end;
int h_outbound_msgbuf;
int h_rehash;
int h_cap_change;
int h_sendq_cleared;

void
init_hook(void)
{
	hooks = rb_malloc(sizeof(hook) * HOOK_INCREMENT);

	h_burst_client = register_hook("burst_client");
	h_burst_channel = register_hook("burst_channel");
	h_burst_finished = register_hook("burst_finished");
	h_server_introduced = register_hook("server_introduced");
	h_server_eob = register_hook("server_eob");
	h_client_exit = register_hook("client_exit");
	h_after_client_exit = register_hook("after_client_exit");
	h_umode_changed = register_hook("umode_changed");
	h_new_local_user = register_hook("new_local_user");
	h_new_remote_user = register_hook("new_remote_user");
	h_introduce_client = register_hook("introduce_client");
	h_can_kick = register_hook("can_kick");
	h_privmsg_user = register_hook("privmsg_user");
	h_privmsg_channel = register_hook("privmsg_channel");
	h_conf_read_start = register_hook("conf_read_start");
	h_conf_read_end = register_hook("conf_read_end");
	h_outbound_msgbuf = register_hook("outbound_msgbuf");
	h_rehash = register_hook("rehash");
	h_cap_change = register_hook("cap_change");
	h_sendq_cleared = register_hook("sendq_cleared");
}

/* grow_hooktable()
 *   Enlarges the hook table by HOOK_INCREMENT
 */
static void
grow_hooktable(void)
{
	hook *newhooks;

	newhooks = rb_malloc(sizeof(hook) * (max_hooks + HOOK_INCREMENT));
	memcpy(newhooks, hooks, sizeof(hook) * num_hooks);

	rb_free(hooks);
	hooks = newhooks;
	max_hooks += HOOK_INCREMENT;
}

/* find_freehookslot()
 *   Finds the next free slot in the hook table, given by an entry with
 *   h->name being NULL.
 */
static int
find_freehookslot(void)
{
	int i;

	if((num_hooks + 1) > max_hooks)
		grow_hooktable();

	for(i = 0; i < max_hooks; i++)
	{
		if(!hooks[i].name)
			return i;
	}

	/* shouldnt ever get here */
	return(max_hooks - 1);
}

/* find_hook()
 *   Finds an event in the hook table.
 */
static int
find_hook(const char *name)
{
	int i;

	for(i = 0; i < max_hooks; i++)
	{
		if(!hooks[i].name)
			continue;

		if(!irccmp(hooks[i].name, name))
			return i;
	}

	return -1;
}

/* register_hook()
 *   Finds an events position in the hook table, creating it if it doesnt
 *   exist.
 */
int
register_hook(const char *name)
{
	int i;

	if((i = find_hook(name)) < 0)
	{
		i = find_freehookslot();
		hooks[i].name = rb_strdup(name);
		num_hooks++;
	}

	return i;
}

/* add_hook()
 *   Adds a hook to an event in the hook table, creating event first if
 *   needed.
 */
void
add_hook(const char *name, hookfn fn)
{
	add_hook_prio(name, fn, HOOK_NORMAL);
}

/* add_hook_prio()
 *   Adds a hook with the specified priority
 */
void
add_hook_prio(const char *name, hookfn fn, enum hook_priority priority)
{
	rb_dlink_node *ptr;
	struct hook_entry *entry = rb_malloc(sizeof *entry);
	int i;

	i = register_hook(name);
	entry->fn = fn;
	entry->priority = priority;

	RB_DLINK_FOREACH(ptr, hooks[i].hooks.head)
	{
		struct hook_entry *o = ptr->data;
		if (entry->priority <= o->priority)
		{
			rb_dlinkAddBefore(ptr, entry, &entry->node, &hooks[i].hooks);
			return;
		}
	}

	rb_dlinkAddTail(entry, &entry->node, &hooks[i].hooks);
}

/* remove_hook()
 *   Removes a hook from an event in the hook table.
 */
void
remove_hook(const char *name, hookfn fn)
{
	rb_dlink_node *ptr, *scratch;
	int i;

	if((i = find_hook(name)) < 0)
		return;

	RB_DLINK_FOREACH_SAFE(ptr, scratch, hooks[i].hooks.head)
	{
		struct hook_entry *entry = ptr->data;
		if (entry->fn == fn)
		{
			rb_dlinkDelete(ptr, &hooks[i].hooks);
			return;
		}
	}
}

/* call_hook()
 *   Calls functions from a given event in the hook table.
 */
void
call_hook(int id, void *arg)
{
	rb_dlink_node *ptr;

	/* The ID we were passed is the position in the hook table of this
	 * hook
	 */
	RB_DLINK_FOREACH(ptr, hooks[id].hooks.head)
	{
		struct hook_entry *entry = ptr->data;
		entry->fn(arg);
	}
}

