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

#define OperMode(c, set, attr, ch)                                            \
(                                                                             \
    HasRole((c)) &&                                                           \
    role_can_mode((c)->localClient->role, (set), (attr), (ch))                \
)

#define OperCanChmode(c, l)                                                   \
(                                                                             \
    OperMode((c), ROLE_CHMODE, ROLE_AVAIL, (l))                               \
)

#define OperCanStat(c, l)                                                     \
(                                                                             \
    OperMode((c), ROLE_STAT, ROLE_AVAIL, (l))                                 \
)


typedef enum
{
	ROLE_UMODE,         // User modes.
	ROLE_CHMODE,        // Channel modes role can manipulate.
	ROLE_STAT,          // Stats characters.
	ROLE_SNO,           // Snomask modes.
	ROLE_EXEMPT,        // Exemption mode mask.

	_ROLE_MSETS_
}
RoleModeSet;

typedef enum
{
	ROLE_AVAIL,         // Mode is available, or can be toggled.
	ROLE_DEFAULT,       // Mode is set on oper-up.
	ROLE_LOCKED,        // Mode is always forced on.

	_ROLE_MATTRS_
}
RoleModeAttr;

struct Role
{
	const char *name;                                // Name of the this role.
	const char *extends;                             // Comma delimited names extends.
	struct irc_radixtree *cmds;                      // All command paths (includes extends).
	const char *mode[_ROLE_MSETS_][_ROLE_MATTRS_];   // All mode tables (including extends).
};


/* Provides some string reflection of the above enums
 */
extern const char *const role_mode_set[_ROLE_MSETS_];
extern const char *const role_mode_attr[_ROLE_MATTRS_];


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
int role_can_mode(const struct Role *const r,
                  const RoleModeSet set,
                  const RoleModeAttr attr,
                  const char letter)
{
	return !EmptyString(r->mode[set][attr]) && strchr(r->mode[set][attr], letter);
}


static inline
int role_valid_modes(const struct Role *const r,
                     const RoleModeSet set,
                     const RoleModeAttr attr,
                     const char *const str)
{
	for(size_t i = 0; i < strlen(str); i++)
		if(!role_can_mode(r, set, attr, str[i]))
			return 0;

	return 1;
}


/* System initialization / destruction
 */
void role_reset(void);   // May be called from clear_out_old_conf() (not required)
void role_init(void);
void role_fini(void);


#endif
