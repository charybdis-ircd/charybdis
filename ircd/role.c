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


const char *const role_mode_set[_ROLE_MSETS_] =
{
	"UMODES",
	"CHMODES",
	"STAT",
	"SNO",
	"EXEMPT",
};

const char *const role_mode_attr[_ROLE_MATTRS_] =
{
	"AVAIL",
	"DEFAULT",
	"FORCED",
};


struct irc_radixtree *roles;


static
struct Role *role_create(const char *const name)
{
	struct Role *const r = rb_malloc(sizeof(struct Role));
	r->name = rb_strdup(name);
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

	rb_free((char *)r->extends);
	r->extends = NULL;

	for(size_t i = 0; i < _ROLE_MSETS_; i++)
		for(size_t j = 0; j < _ROLE_MATTRS_; j++)
			rb_free((char *)r->mode[i][j]);

	memset(r->mode, 0x0, sizeof(r->mode));
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
			else if(wildcard <= i)
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

	char *neg;
	static char buf[256];
	size_t j = rb_strlcpy(buf, *dst, sizeof(buf));
	for(size_t i = 0; src[i] && j < sizeof(buf)-1; i++)
		if(src[i] == '~' && src[++i] && (neg = strchr(buf, src[i])))
			*neg = ' ';
		else if(src[i] && !strchr(buf, src[i]))
			buf[j++] = src[i];

	buf[j+1] = '\0';
	while((neg = strchr(buf, ' ')))
		memmove(neg, neg+1, buf+strlen(buf)-neg);

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
void set_conf_mode(const RoleModeSet set,
                   void *const val)
{
	if(!role_cur_conf)
		return;

	// The FLIST is stacked and must be properly ordered with completion.
	// The UI in the configuration is much easier this way than making more
	// variables, but if the list implementation changes this will break :-/

	size_t i = 0;
	char *list[_ROLE_MATTRS_] = {0};
	for(conf_parm_t *args = val; args && i < _ROLE_MATTRS_; args = args->next, i++)
		list[i] = args->v.string;

	for(ssize_t j = i - 1, i = 0; j > -1; j--, i++)
		merge_mode_string(&role_cur_conf->mode[set][j], list[i]);
}

static
void role_set_conf_umodes(void *const val)
{
	set_conf_mode(ROLE_UMODE, val);
}

static
void role_set_conf_chmodes(void *const val)
{
	set_conf_mode(ROLE_CHMODE, val);
}

static
void role_set_conf_stats(void *const val)
{
	set_conf_mode(ROLE_STAT, val);
}

static
void role_set_conf_snotes(void *const val)
{
	set_conf_mode(ROLE_SNO, val);
}

static
void role_set_conf_exempts(void *const val)
{
	set_conf_mode(ROLE_EXEMPT, val);
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

	for(size_t i = 0; i < _ROLE_MSETS_; i++)
		for(size_t j = 0; j < _ROLE_MATTRS_; j++)
			merge_mode_string(&role_cur_conf->mode[i][j], src->mode[i][j]);
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

	if(!EmptyString(role_cur_conf->extends))
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


struct ConfEntry role_conf_table[] =
{
	{ "extends",         CF_STRING | CF_FLIST,    role_set_conf_extends,          0, NULL },
	{ "cmds",            CF_STRING | CF_FLIST,    role_set_conf_cmds,             0, NULL },
	{ "umodes",          CF_QSTRING | CF_FLIST,   role_set_conf_umodes,           0, NULL },
	{ "chmodes",         CF_QSTRING | CF_FLIST,   role_set_conf_chmodes,          0, NULL },
	{ "stats",           CF_QSTRING | CF_FLIST,   role_set_conf_stats,            0, NULL },
	{ "snotes",          CF_QSTRING | CF_FLIST,   role_set_conf_snotes,           0, NULL },
	{ "exempts",         CF_QSTRING | CF_FLIST,   role_set_conf_exempts,          0, NULL },
	{ "\0", 0, NULL, 0, NULL }
};



/*******************************************************************************
 * Deprecated
 */

struct Privset
{
	char privs[1024];
};

struct irc_radixtree *privsets;
struct Privset *cur_privset;

static
void privset_set_extends(void *const data)
{
	if(!cur_privset)
		return;

	const char *const name = data;
	const struct Privset *const parent = irc_radixtree_retrieve(privsets, name);
	if(!parent)
	{
		conf_report_error("Cannot find privset '%s' to inherit into '%s'", name, conf_cur_block_name);
		return;
	}

	if(strlen(cur_privset->privs) + strlen(parent->privs) >= sizeof(cur_privset->privs))
	{
		conf_report_error("Too many privs inherited into '%s' (cannot inherit from '%s')", conf_cur_block_name, name);
		return;
	}

	static char buf[1024];
	rb_strlcpy(buf, parent->privs, sizeof(buf));
	char *ctx, *tok = strtok_r(buf, " ", &ctx); do
	{
		if(strstr(cur_privset->privs, tok))
			continue;

		rb_strlcat(cur_privset->privs, tok, sizeof(cur_privset->privs));
		rb_strlcat(cur_privset->privs, " ", sizeof(cur_privset->privs));
	}
	while((tok = strtok_r(NULL, " ", &ctx)) != NULL);
}

static
void privset_set_privs(void *const val)
{
	if(!cur_privset)
		return;

	for(conf_parm_t *args = val; args; args = args->next)
	{
		if(strlen(cur_privset->privs) + strlen(args->v.string) >= sizeof(cur_privset->privs))
		{
			conf_report_error("Too many privs inherited into '%s'", conf_cur_block_name);
			return;
		}

		rb_strlcat(cur_privset->privs, args->v.string, sizeof(cur_privset->privs));
		rb_strlcat(cur_privset->privs, " ", sizeof(cur_privset->privs));
	}
}

static
void privset_make_role(const struct Privset *const privset)
{
	role_conf_begin(NULL);

	size_t i = 0;
	static char buf[1024];
	static conf_parm_t parm[64];
	rb_strlcpy(buf, privset->privs, sizeof(buf));
	char *ctx, *tok = strtok_r(buf, " ", &ctx); do
	{
		parm[i].v.string = tok;
		parm[i].next = parm + i + 1;
		i++;
	}
	while((tok = strtok_r(NULL, " ", &ctx)) != NULL);

	if(i)
	{
		parm[i - 1].next = NULL;
		role_set_conf_extends(parm);
	}

	role_conf_end(NULL);
}

static
int privset_conf_begin(struct TopConf *const tc)
{
	//conf_report_error("Privsets are deprecated. You want to convert to the new role {} now. ");
	cur_privset = rb_malloc(sizeof(struct Privset));
	return 0;
}

static
int privset_conf_end(struct TopConf *const tc)
{
	if(!cur_privset)
		return -1;

	irc_radixtree_add(privsets, conf_cur_block_name, cur_privset);
	privset_make_role(cur_privset);
	cur_privset = NULL;
	return 0;
}

struct ConfEntry privset_conf_table[] =
{
	{ "extends",   CF_QSTRING,              privset_set_extends,  0, NULL },
	{ "privs",     CF_STRING | CF_FLIST,    privset_set_privs,    0, NULL },
	{ "\0", 0, NULL, 0, NULL }
};


static
void privset_init_default_modes(struct Role *const r)
{
	r->mode[ROLE_UMODE][ROLE_AVAIL] = rb_strdup("eloswz");
	r->mode[ROLE_UMODE][ROLE_DEFAULT] = rb_strdup("eloswz");

	r->mode[ROLE_SNO][ROLE_AVAIL] = rb_strdup("Cbcdfkrsux");
	r->mode[ROLE_SNO][ROLE_DEFAULT] = rb_strdup("s");

	r->mode[ROLE_EXEMPT][ROLE_AVAIL] = rb_strdup("CJYadfjkrw");
	r->mode[ROLE_EXEMPT][ROLE_DEFAULT] = rb_strdup("CJYadfjkrw");

	r->mode[ROLE_STAT][ROLE_AVAIL] = rb_strdup("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	r->mode[ROLE_CHMODE][ROLE_AVAIL] = rb_strdup("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
}

static
void privset_init_default_cmds(struct Role *const r)
{
	role_add_cmd(r, "CHANTRACE");
	role_add_cmd(r, "ETRACE");
	role_add_cmd(r, "FINDFORWARDS");
	role_add_cmd(r, "MASKTRACE");
	role_add_cmd(r, "OJOIN");
	role_add_cmd(r, "OMODE");
	role_add_cmd(r, "OPME");
	role_add_cmd(r, "SCAN");
	role_add_cmd(r, "ROLE");
	role_add_cmd(r, "WHOIS::");
}

static
struct Role *privset_init_role(const char *const name)
{
	struct Role *const r = role_create(name);
	privset_init_default_modes(r);
	privset_init_default_cmds(r);
	role_add(r);
	return r;
}

static
void privset_init(void)
{
	privsets = irc_radixtree_create("privsets", irc_radixtree_irccasecanon);

	{
		struct Role *const r = privset_init_role("oper:global_kill");
		role_add_cmd(r, "KILL:remote");
	}
	{
		struct Role *const r = privset_init_role("oper:local_kill");
		role_add_cmd(r, "KILL");
	}
	{
		struct Role *const r = privset_init_role("oper:routing");
		role_add_cmd(r, "CONNECT::");
		role_add_cmd(r, "SQUIT::");
	}
	{
		struct Role *const r = privset_init_role("oper:unkline");
		role_add_cmd(r, "UNKLINE");
		role_add_cmd(r, "HEAL");
		role_add_cmd(r, "SENDBANS");
	}
	{
		struct Role *const r = privset_init_role("snomask:nick_changes");
		merge_mode_string(&r->mode[ROLE_SNO][ROLE_AVAIL], "n");
	}
	{
		struct Role *const r = privset_init_role("oper:kline");
		role_add_cmd(r, "KLINE");
		role_add_cmd(r, "SENDBANS");
	}
	{
		struct Role *const r = privset_init_role("oper:xline");
		role_add_cmd(r, "XLINE");
		role_add_cmd(r, "UNXLINE");
		role_add_cmd(r, "SENDBANS");
	}
	{
		// must have chmodes/chm_staff
		struct Role *const r = privset_init_role("oper:resv");
		role_add_cmd(r, "RESV");
		role_add_cmd(r, "UNRESV");
		role_add_cmd(r, "SENDBANS");
	}
	{
		struct Role *const r = privset_init_role("oper:die");
		role_add_cmd(r, "RESTART::");
		role_add_cmd(r, "DIE::");
	}
	{
		struct Role *const r = privset_init_role("oper:rehash");
		role_add_cmd(r, "REHASH");
	}
	{
		struct Role *const r = privset_init_role("oper:hidden_admin");
		merge_mode_string(&r->mode[ROLE_UMODE][ROLE_AVAIL], "a");
		merge_mode_string(&r->mode[ROLE_UMODE][ROLE_DEFAULT], "a");
		merge_mode_string(&r->mode[ROLE_EXEMPT][ROLE_AVAIL], "ilp");
		merge_mode_string(&r->mode[ROLE_EXEMPT][ROLE_DEFAULT], "ilp");
	}
	{
		struct Role *const r = privset_init_role("oper:admin");
		merge_mode_string(&r->mode[ROLE_UMODE][ROLE_AVAIL], "a");
		merge_mode_string(&r->mode[ROLE_UMODE][ROLE_DEFAULT], "a");
		role_add_cmd(r, "REHASH");
		role_add_cmd(r, "MODLOAD");
		role_add_cmd(r, "MODUNLOAD");
		role_add_cmd(r, "MODRELOAD");
		role_add_cmd(r, "MODLIST");
		role_add_cmd(r, "MODRESTART");
	}
	{
		struct Role *const r = privset_init_role("oper:operwall");
		merge_mode_string(&r->mode[ROLE_UMODE][ROLE_AVAIL], "z");
		merge_mode_string(&r->mode[ROLE_UMODE][ROLE_DEFAULT], "z");
		role_add_cmd(r, "OPERWALL::");
	}
	{
		struct Role *const r = privset_init_role("oper:spy");
		role_add_cmd(r, "MODE:spy");
		role_add_cmd(r, "CHANTRACE:spy");
		role_add_cmd(r, "MASKTRACE:spy");
		role_add_cmd(r, "LIST:spy");
		role_add_cmd(r, "UMODES:spy");
		role_add_cmd(r, "TOPIC:spy");
		role_add_cmd(r, "WHO:spy");
	}
	{
		struct Role *const r = privset_init_role("oper:hidden");
		merge_mode_string(&r->mode[ROLE_EXEMPT][ROLE_AVAIL], "ilp");
		merge_mode_string(&r->mode[ROLE_EXEMPT][ROLE_DEFAULT], "ilp");
	}
	{
		struct Role *const r = privset_init_role("oper:remoteban");
		role_add_cmd(r, "DLINE:remote");
		role_add_cmd(r, "UNDLINE:remote");
		role_add_cmd(r, "KLINE:remote");
		role_add_cmd(r, "UNKLINE:remote");
		role_add_cmd(r, "RESV:remote");
		role_add_cmd(r, "UNRESV:remote");
		role_add_cmd(r, "XLINE:remote");
		role_add_cmd(r, "UNXLINE:remote");
		role_add_cmd(r, "REHASH:remote");
		role_add_cmd(r, "SENDBANS");
	}
	{
		struct Role *const r = privset_init_role("oper:mass_notice");
		role_add_cmd(r, "WALLOPS");
		role_add_cmd(r, "MSG::");
	}
	{
		struct Role *const r = privset_init_role("usermode:helpops");
		merge_mode_string(&r->mode[ROLE_UMODE][ROLE_AVAIL], "H");
		merge_mode_string(&r->mode[ROLE_UMODE][ROLE_DEFAULT], "H");
	}
	{
		struct Role *const r = privset_init_role("oper:extendchans");
		role_add_cmd(r, "EXTENDCHANS");
	}
	{
		struct Role *const r = privset_init_role("oper:grant");
		merge_mode_string(&r->mode[ROLE_SNO][ROLE_AVAIL], "Zy");
		role_add_cmd(r, "GRANT");
	}
	{
		struct Role *const r = privset_init_role("oper:override");
		merge_mode_string(&r->mode[ROLE_UMODE][ROLE_AVAIL], "p");
		merge_mode_string(&r->mode[ROLE_UMODE][ROLE_DEFAULT], "p");
	}
}

static
void privset_privsets_dtor(const char *const key, void *const data, void *const privdata)
{
	struct Privset *const privset = data;
	rb_free(privset);
}

static
void privset_fini(void)
{
	irc_radixtree_destroy(privsets, privset_privsets_dtor, NULL);
}

/******************************************************************************/


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
		{
			role_remove(role);
			role_destroy(role);
		}
	}

	privset_fini();
	irc_radixtree_destroy(role_conf_recent, NULL, NULL);
	role_conf_recent = NULL;
}


static
void role_conf_all_start(void *const data)
{
	role_conf_recent = irc_radixtree_create("conf_roles", irc_radixtree_irccasecanon);
	privset_init();
}


static
void roles_dtor(const char *const key, void *const data, void *const privdata)
{
	struct Role *const role = data;
	role_remove_from_opers(role);
	role_destroy(role);
}


void role_reset(void)
{
	privset_fini();
	irc_radixtree_destroy(roles, roles_dtor, NULL);
	roles = irc_radixtree_create("roles", irc_radixtree_irccasecanon);
	privset_init();
}


void role_fini(void)
{
	remove_hook("conf_read_end", role_conf_all_end);
	remove_hook("conf_read_start", role_conf_all_start);
	remove_top_conf("privset");
	remove_top_conf("role");
	irc_radixtree_destroy(roles, roles_dtor, NULL);
}


void role_init(void)
{
	roles = irc_radixtree_create("roles", irc_radixtree_irccasecanon);
	add_top_conf("role", role_conf_begin, role_conf_end, role_conf_table);
	add_top_conf("privset", privset_conf_begin, privset_conf_end, privset_conf_table);
	add_hook("conf_read_start", role_conf_all_start);
	add_hook("conf_read_end", role_conf_all_end);
}
