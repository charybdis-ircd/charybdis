/*
 * charybdis: an advanced ircd.
 * role.c: Role-Based Dynamic Privileges API.
 *
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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

#include <stdinc.h>
#include "newconf.h"
#include "s_conf.h"
#include "role.h"
#include "numeric.h"
#include "s_assert.h"
#include "logger.h"
#include "send.h"
#include "irc_radixtree.h"
#include "hook.h"
#include "s_user.h"


struct irc_radixtree *roles;


static
struct Role *role_create(const char *const name)
{
	struct Role *const r = rb_malloc(sizeof(struct Role));
	r->name = rb_strdup(name);
	r->extends = NULL;
	r->cmds = NULL;
	r->umodes = NULL;
	r->chmodes = NULL;
	r->stats = NULL;
	r->snotes = NULL;
	r->exempts = NULL;
	return r;
}


static
void role_clear(struct Role *const r)
{
	if(r->cmds)
	{
		irc_radixtree_destroy(r->cmds, NULL, NULL);
		r->cmds = NULL;
	}

	rb_free((char *)r->extends);   r->extends = NULL;
	rb_free((char *)r->umodes);    r->umodes = NULL;
	rb_free((char *)r->chmodes);   r->chmodes = NULL;
	rb_free((char *)r->stats);     r->stats = NULL;
	rb_free((char *)r->snotes);    r->snotes = NULL;
	rb_free((char *)r->exempts);   r->exempts = NULL;
}


static
void role_destroy(struct Role *const r)
{
	role_clear(r);
	rb_free((char *)r->name);
	rb_free(r);
}


struct Role *role_get(const char *const name)
{
	return irc_radixtree_retrieve(roles, name);
}


static
int role_add(struct Role *const role)
{
	return irc_radixtree_add(roles, role->name, role);
}


static
int role_remove_from_opers(const struct Role *const role)
{
	int ret = 0;
	rb_dlink_node *ptr;
	RB_DLINK_FOREACH(ptr, oper_list.head)
	{
		struct Client *const client = ptr->data;
		if(client->localClient && client->localClient->role == role)
		{
			client->localClient->role = NULL;
			ret++;
		}
	}

	return ret;
}


static
int role_remove(struct Role *const role)
{
	role_remove_from_opers(role);
	return irc_radixtree_delete(roles, role->name) != NULL;
}


static
size_t parse_path(const char *const path,
                  char ret[ROLE_PATH_ELEMS_MAX][ROLE_PATH_ELEM_SIZE],
                  int *const wildcard)
{
	size_t j = 0;
	const size_t len = strlen(path);
	for(size_t i = 0, k = 0; i <= len && j < ROLE_PATH_ELEMS_MAX; i++)
	{
		if(!path[i] || path[i] == ROLE_PATHSEP || k >= ROLE_PATH_ELEM_SIZE-1)
		{
			ret[j][k] = '\0';

			if(k == 0 && j == 0)
				break;

			if(k == 0 && wildcard)
				*wildcard = j;

			k = 0;
			j++;
		}
		else ret[j][k++] = path[i];
	}

	return j;
}


int role_has_cmd(const struct Role *const r,
                 const char *const *const label)
{
	static char path[ROLE_PATH_ELEMS_MAX][ROLE_PATH_ELEM_SIZE];

	ssize_t labels = -1;                  // Number of label arguments passed by the macro
	while(label[++labels]);

	void *e;
	struct irc_radixtree_iteration_state state;
	IRC_RADIXTREE_FOREACH(e, &state, r->cmds) /* y u no have trie lower_bound?? */
	{
		int wildcard = 0;
		const char *const key = irc_radixtree_elem_get_key(state.pspare[0]);
		const ssize_t elems = parse_path(key, path, &wildcard);
		for(ssize_t i = 1; i <= elems && i <= labels; i++)
		{
			const int cmp = irccmp(label[i-1], path[i-1]);
			if(cmp == 0 && i == elems && i == labels)
				return 1;
			else if(cmp == 0)
				continue;
			else if(wildcard == i)
				return 1;
			else
				break;
		}
	}

	return 0;
}


static
int role_del_cmd(struct Role *const r,
                 const char *const path)
{
	return r->cmds? irc_radixtree_delete(r->cmds, path) != NULL : 0;
}


static
int role_add_cmd(struct Role *const r,
                 const char *const path)
{
	if(!r->cmds)
		r->cmds = irc_radixtree_create("cmds", irc_radixtree_irccasecanon);

	if(strlen(path) > 1 && path[0] == '~')
	{
		if(!role_del_cmd(r, path+1))
		{
			conf_report_error("Failed to remove command '%s' from the set. Try listing extends= earlier.", path);
			return 0;
		}
		else return 1;
	}
	else return irc_radixtree_add(r->cmds, path, (void*)1);
}



/*
 * newconf parsing
 */

struct Role *role_cur_conf;
struct irc_radixtree *role_conf_recent;


/* Reallocates dst */
static
void merge_mode_string(const char **const dst,
                       const char *const src)
{
	if(EmptyString(src))
		return;

	if(!*dst)
	{
		*dst = rb_strdup(src);
		return;
	}

	static char buf[256];
	rb_strlcpy(buf, *dst, sizeof(buf));

	size_t j = strlen(buf);
	for(size_t i = 0; i < strlen(src) && j < sizeof(buf)-1; i++)
		if(!strchr(buf, src[i]))
			buf[j] = src[i];

	buf[j+1] = '\0';
	rb_free((void *)*dst);
	*dst = rb_strdup(buf);
}


static
void role_set_conf_cmds(void *const val)
{
	if(!role_cur_conf)
		return;

	for(conf_parm_t *args = val; args; args = args->next)
		role_add_cmd(role_cur_conf, args->v.string);
}

static
void role_set_conf_stats(void *const val)
{
	if(!role_cur_conf)
		return;

	merge_mode_string(&role_cur_conf->stats, val);
}

static
void role_set_conf_umodes(void *const val)
{
	if(!role_cur_conf)
		return;

	merge_mode_string(&role_cur_conf->umodes, val);
}

static
void role_set_conf_chmodes(void *const val)
{
	if(!role_cur_conf)
		return;

	merge_mode_string(&role_cur_conf->chmodes, val);
}

static
void role_set_conf_snotes(void *const val)
{
	if(!role_cur_conf)
		return;

	merge_mode_string(&role_cur_conf->snotes, val);
}

static
void role_set_conf_exempts(void *const val)
{
	if(!role_cur_conf)
		return;

	merge_mode_string(&role_cur_conf->exempts, val);
}


static
void cur_conf_extend(const struct Role *const src)
{
	void *e;
	struct irc_radixtree_iteration_state state;
	IRC_RADIXTREE_FOREACH(e, &state, src->cmds)
	{
		const char *const key = irc_radixtree_elem_get_key(state.pspare[0]);
		role_add_cmd(role_cur_conf, key);
	}

	merge_mode_string(&role_cur_conf->umodes, src->umodes);
	merge_mode_string(&role_cur_conf->chmodes, src->chmodes);
	merge_mode_string(&role_cur_conf->stats, src->stats);
	merge_mode_string(&role_cur_conf->snotes, src->snotes);
	merge_mode_string(&role_cur_conf->exempts, src->exempts);
}


static
void role_set_conf_extends(void *const val)
{
	if(!role_cur_conf)
		return;

	static char buf[BUFSIZE];
	buf[0] = '\0';

	for(conf_parm_t *args = val; args; args = args->next)
	{
		struct Role *const src = role_get(args->v.string);
		if(!src)
		{
			conf_report_error("Cannot find role '%s' to inherit into '%s'",
			                  args->v.string,
			                  role_cur_conf->name);
			continue;
		}

		if(strnlen(buf, sizeof(buf)) + strlen(src->name) >= sizeof(buf)-1)
		{
			conf_report_error("You're overthinking this. (too many extends to role '%s').", role_cur_conf->name);
			continue;
		}

		cur_conf_extend(src);
		rb_strlcat(buf, src->name, sizeof(buf));
		rb_strlcat(buf, ", ", sizeof(buf));
	}

	if(role_cur_conf->extends)
	{
		const size_t newstrsz = strlen(role_cur_conf->extends) + 2 + strlen(buf) + 1;
		char *const newstr = rb_malloc(newstrsz);
		rb_strlcpy(newstr, role_cur_conf->extends, newstrsz);
		rb_strlcat(newstr, ", ", newstrsz);
		rb_strlcat(newstr, buf, newstrsz);
		rb_free((char *)role_cur_conf->extends);
		role_cur_conf->extends = newstr;
	}
	else role_cur_conf->extends = rb_strdup(buf);
}


static
int role_legal_name(const char *const str)
{
	if(EmptyString(str))
	{
		conf_report_error("Unnamed role ignored.");
		return 0;
	}

	// Piggyback off this validator for now...
	if(!valid_username(str))
	{
		conf_report_error("The role name is not valid.");
		return 0;
	}

	return 1;
}


static
int role_conf_begin(struct TopConf *const tc)
{
	if(!role_legal_name(conf_cur_block_name))
		return -1;

	if((role_cur_conf = role_get(conf_cur_block_name)))
		role_clear(role_cur_conf);
	else
		role_cur_conf = role_create(conf_cur_block_name);

	return 0;
}


static
int role_conf_end(struct TopConf *const tc)
{
	if(role_cur_conf)
	{
		irc_radixtree_add(role_conf_recent, role_cur_conf->name, (void *)1);
		role_add(role_cur_conf);
		role_cur_conf = NULL;
	}

	return 0;
}


/* nlogn search to prevent roles that disappeared from the conf becoming leaks.
 * Must also scavenge the oper_list and clean up the dangling reference.
 *
 * TODO: local_oper_list vs. oper_list??
 */
static
void role_conf_all_end(void *const data)
{
	struct Role *role;
	struct irc_radixtree_iteration_state state;
	IRC_RADIXTREE_FOREACH(role, &state, roles)
	{
		const char *const key = irc_radixtree_elem_get_key(state.pspare[0]);
		if(irc_radixtree_retrieve(role_conf_recent, key) == NULL)
			role_remove(role);
	}

	irc_radixtree_destroy(role_conf_recent, NULL, NULL);
	role_conf_recent = NULL;
}


static
void role_conf_all_start(void *const data)
{
	role_conf_recent = irc_radixtree_create("conf_roles", irc_radixtree_irccasecanon);
}


/*******************************************************************************
 * Deprecated
 * TODO: offer backwards compat
 */

static
void role_set_privset_extends(void *const data)
{
}

static
void role_set_privset_privs(void *const data)
{
}

static
int role_conf_privset_begin(struct TopConf *const tc)
{
	conf_report_error("Privsets are deprecated. You want to convert to the new role {} now.");
	return 0;
}

static
int role_conf_privset_end(struct TopConf *const tc)
{
	return 0;
}

struct ConfEntry role_conf_privset_table[] =
{
	{ "extends",   CF_QSTRING,              role_set_privset_extends,  0, NULL },
	{ "privs",     CF_STRING | CF_FLIST,    role_set_privset_privs,    0, NULL },
	{ "\0", 0, NULL, 0, NULL }
};

/******************************************************************************/


struct ConfEntry role_conf_table[] =
{
	{ "extends",   CF_STRING | CF_FLIST,    role_set_conf_extends,     0, NULL },
	{ "cmds",      CF_STRING | CF_FLIST,    role_set_conf_cmds,        0, NULL },
	{ "umodes",    CF_QSTRING,              role_set_conf_umodes,      0, NULL },
	{ "chmodes",   CF_QSTRING,              role_set_conf_chmodes,     0, NULL },
	{ "stats",     CF_QSTRING,              role_set_conf_stats,       0, NULL },
	{ "snotes",    CF_QSTRING,              role_set_conf_snotes,      0, NULL },
	{ "exempts",   CF_QSTRING,              role_set_conf_exempts,     0, NULL },
	{ "\0", 0, NULL, 0, NULL }
};


static
void roles_dtor(const char *const key, void *const data, void *const privdata)
{
	struct Role *const role = data;
	role_remove_from_opers(role);
	role_destroy(role);
}


void role_reset(void)
{
	irc_radixtree_destroy(roles, roles_dtor, NULL);
	roles = irc_radixtree_create("roles", irc_radixtree_irccasecanon);
}


void role_fini(void)
{
	remove_hook("conf_read_end", role_conf_all_end);
	remove_hook("conf_read_start", role_conf_all_start);
	remove_top_conf("role");
	irc_radixtree_destroy(roles, roles_dtor, NULL);

	// Deprecated
	remove_top_conf("privset");
}


void role_init(void)
{
	roles = irc_radixtree_create("roles", irc_radixtree_irccasecanon);
	add_top_conf("role", role_conf_begin, role_conf_end, role_conf_table);
	add_hook("conf_read_start", role_conf_all_start);
	add_hook("conf_read_end", role_conf_all_end);

	// Deprecated
	add_top_conf("privset", role_conf_privset_begin, role_conf_privset_end, role_conf_privset_table);
}
