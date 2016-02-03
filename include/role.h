 /*
 * charybdis: an advanced ircd.
 * role.h: Role-Based Dynamic Privileges API.
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

#ifndef __CHARYBDIS_ROLE_H
#define __CHARYBDIS_ROLE_H

#include "stdinc.h"
struct irc_radixtree;


/* Path separator (not used in macro; used in conf) */
#define ROLE_PATHSEP ':'

/* Some limitations */
#define ROLE_NAME_MAXLEN 32
#define ROLE_PATH_ELEM_SIZE 32
#define ROLE_PATH_ELEMS_MAX 16


/*
 * Convenience macros
 */

#define HasRole(c)                                                            \
(                                                                             \
    (c)->localClient && (c)->localClient->role                                \
)

#define IsRole(c, _name)                                                      \
(                                                                             \
    HasRole((c)) &&                                                           \
    strcmp((c)->localClient->role->name, (_name)) == 0                        \
)

#define OperCan(c, ...)                                                       \
(                                                                             \
    HasRole((c)) &&                                                           \
    role_has_cmd((c)->localClient->role, (const char*[]){__VA_ARGS__, NULL})  \
)

#define OperCanUmode(c, l)                                                    \
(                                                                             \
    HasRole((c)) &&                                                           \
    role_has_umode((c)->localClient->role, (l))                               \
)

#define OperCanChmode(c, l)                                                   \
(                                                                             \
    HasRole((c)) &&                                                           \
    role_has_chmode((c)->localClient->role, (l))                              \
)

#define OperCanStat(c, l)                                                     \
(                                                                             \
    HasRole((c)) &&                                                           \
    role_has_stat((c)->localClient->role, (l))                                \
)

#define OperCanSnote(c, l)                                                    \
(                                                                             \
    HasRole((c)) &&                                                           \
    role_has_snote((c)->localClient->role, (l))                               \
)

#define OperCanExempt(c, l)                                                   \
(                                                                             \
    HasRole((c)) &&                                                           \
    role_has_exempt((c)->localClient->role, (l))                              \
)


struct Role
{
	const char *name;                 // Name of the role.
	const char *extends;              // Comma delimited reference of what role inherits.
	struct irc_radixtree *cmds;       // All command paths available to role (stored in key).
	const char *umodes;               // String of all user modes available to self.
	const char *chmodes;              // String of all channel modes role can manipulate.
	const char *stats;                // String of all stats characters available.
	const char *snotes;               // String of all snomask letters permitted.
	const char *exempts;              // String of all exemption letters.
};


/* The active roles are accessible for iteration.
 * Should not be manipulated. (Would be const if supported).
 */
extern struct irc_radixtree *roles;


/* Return a role by name or NULL if not found.
 * role is managed internally and must not be free'd.
 */
struct Role *role_get(const char *name);


/* Test privilege satisfaction.
 *
 *	Returns 1 on satisfaction
 *	Returns 0 otherwise
 *
 * Use the macro, ex: OperCanCmd(client_p, "SPAMEXPR", "ADD")
 */
int role_has_cmd(const struct Role *, const char *const *path);

static inline
int role_has_stat(const struct Role *const r, const char letter)
{
	return !EmptyString(r->stats) && strchr(r->stats, letter);
}

static inline
int role_has_umode(const struct Role *const r, const char letter)
{
	return !EmptyString(r->umodes) && strchr(r->umodes, letter);
}

static inline
int role_has_chmode(const struct Role *const r, const char letter)
{
	return !EmptyString(r->chmodes) && strchr(r->chmodes, letter);
}

static inline
int role_has_snote(const struct Role *const r, const char letter)
{
	return !EmptyString(r->snotes) && strchr(r->snotes, letter);
}

static inline
int role_has_exempt(const struct Role *const r, const char letter)
{
	return !EmptyString(r->exempts) && strchr(r->exempts, letter);
}

static inline
int role_valid_modes(const struct Role *const r,
                     const char *const str,
                     int (*const validator)(const struct Role *, const char))
{
	for(size_t i = 0; i < strlen(str); i++)
		if(!validator(r, str[i]))
			return 0;

	return 1;
}


/* System initialization / destruction
 */
void role_reset(void);   // May be called from clear_out_old_conf() (not required)
void role_init(void);
void role_fini(void);


#endif
