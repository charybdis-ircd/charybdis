/*
 *  charybdis: A useful ircd.
 *  client.h: The ircd client header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2005 William Pitcock and Jilles Tjoelker
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#ifndef INCLUDED_client_h
#define INCLUDED_client_h

#include "defaults.h"

#include "ircd_defs.h"
#include "channel.h"
#include "dns.h"
#include "snomask.h"
#include "match.h"
#include "ircd.h"
#include "privilege.h"

/* other structs */
struct Blacklist;

/* we store ipv6 ips for remote clients, so this needs to be v6 always */
#define HOSTIPLEN	53	/* sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255.ipv6") */
#define PASSWDLEN	128
#define CIPHERKEYLEN	64	/* 512bit */

#define IDLEN		10

#define TGCHANGE_NUM		10	/* how many targets we keep track of */
#define TGCHANGE_REPLY		5	/* how many reply targets */
#define TGCHANGE_INITIAL	10	/* initial free targets (normal) */
#define TGCHANGE_INITIAL_LOW	4	/* initial free targets (possible spambot) */

/*
 * pre declare structs
 */
struct ConfItem;
struct Whowas;
struct DNSReply;
struct Listener;
struct Client;
struct User;
struct Server;
struct LocalUser;
struct PreClient;
struct ListClient;
struct scache_entry;
struct ws_ctl;

typedef int SSL_OPEN_CB(struct Client *, int status);

/*
 * Client structures
 */
struct User
{
	rb_dlink_list channel;	/* chain of channel pointer blocks */
	rb_dlink_list invited;	/* chain of invite pointer blocks */
	char *away;		/* pointer to away message */
	int refcnt;		/* Number of times this block is referenced */

	char *opername; /* name of operator{} block being used or tried (challenge) */
	struct PrivilegeSet *privset;

	char suser[NICKLEN+1];
};

struct Server
{
	struct User *user;	/* who activated this connection */
	char by[NICKLEN];
	rb_dlink_list servers;
	rb_dlink_list users;
	int caps;		/* capabilities bit-field */
	char *fullcaps;
	struct scache_entry *nameinfo;
};

struct ZipStats
{
	unsigned long long in;
	unsigned long long in_wire;
	unsigned long long out;
	unsigned long long out_wire;
	double in_ratio;
	double out_ratio;
};

struct Client
{
	rb_dlink_node node;
	rb_dlink_node lnode;
	struct User *user;	/* ...defined, if this is a User */
	struct Server *serv;	/* ...defined, if this is a server */
	struct Client *servptr;	/* Points to server this Client is on */
	struct Client *from;	/* == self, if Local Client, *NEVER* NULL! */

	rb_dlink_list whowas_clist;

	time_t tsinfo;		/* TS on the nick, SVINFO on server */
	unsigned int umodes;	/* opers, normal users subset */
	uint64_t flags;		/* client flags */

	unsigned int snomask;	/* server notice mask */

	int hopcount;		/* number of servers to this 0 = local */
	unsigned short status;	/* Client type */
	unsigned char handler;	/* Handler index */
	unsigned long serial;	/* used to enforce 1 send per nick */

	/* client->name is the unique name for a client nick or host */
	char name[NAMELEN + 1];

	/*
	 * client->username is the username from ident or the USER message,
	 * If the client is idented the USER message is ignored, otherwise
	 * the username part of the USER message is put here prefixed with a
	 * tilde depending on the I:line, Once a client has registered, this
	 * field should be considered read-only.
	 */
	char username[USERLEN + 1];	/* client's username */

	/*
	 * client->host contains the resolved name or ip address
	 * as a string for the user, it may be fiddled with for oper spoofing etc.
	 */
	char host[HOSTLEN + 1];	/* client's hostname */
	char orighost[HOSTLEN + 1]; /* original hostname (before dynamic spoofing) */
	char sockhost[HOSTIPLEN + 1]; /* clients ip */
	char info[REALLEN + 1];	/* Free form additional client info */

	char id[IDLEN];	/* UID/SID, unique on the network */

	/* list of who has this client on their allow list, its counterpart
	 * is in LocalUser
	 */
	rb_dlink_list on_allow_list;

	time_t first_received_message_time;
	int received_number_of_privmsgs;
	int flood_noticed;

	struct LocalUser *localClient;
	struct PreClient *preClient;

	time_t large_ctcp_sent; /* ctcp to large group sent, relax flood checks */
	char *certfp; /* client certificate fingerprint */
};

struct LocalUser
{
	rb_dlink_node tnode;	/* This is the node for the local list type the client is on */
	rb_dlink_list connids;	/* This is the list of connids to free */

	/*
	 * The following fields are allocated only for local clients
	 * (directly connected to *this* server with a socket.
	 */
	/* Anti flooding part, all because of lamers... */
	time_t last_join_time;	/* when this client last
				   joined a channel */
	time_t last_leave_time;	/* when this client last
				 * left a channel */
	int join_leave_count;	/* count of JOIN/LEAVE in less than
				   MIN_JOIN_LEAVE_TIME seconds */
	int oper_warn_count_down;	/* warn opers of this possible
					   spambot every time this gets to 0 */
	time_t last_caller_id_time;

	time_t lasttime;	/* last time we parsed something */
	time_t firsttime;	/* time client was created */

	/* Send and receive linebuf queues .. */
	buf_head_t buf_sendq;
	buf_head_t buf_recvq;

	/*
	 * we want to use unsigned int here so the sizes have a better chance of
	 * staying the same on 64 bit machines. The current trend is to use
	 * I32LP64, (32 bit ints, 64 bit longs and pointers) and since ircd
	 * will NEVER run on an operating system where ints are less than 32 bits,
	 * it's a relatively safe bet to use ints. Since right shift operations are
	 * performed on these, it's not safe to allow them to become negative,
	 * which is possible for long running server connections. Unsigned values
	 * generally overflow gracefully. --Bleep
	 *
	 * We have modern conveniences. Let's use uint32_t. --Elizafox
	 */
	uint32_t sendM;		/* Statistics: protocol messages send */
	uint32_t sendK;		/* Statistics: total k-bytes send */
	uint32_t receiveM;	/* Statistics: protocol messages received */
	uint32_t receiveK;	/* Statistics: total k-bytes received */
	uint16_t sendB;		/* counters to count upto 1-k lots of bytes */
	uint16_t receiveB;	/* sent and received. */
	struct Listener *listener;	/* listener accepted from */
	struct ConfItem *att_conf;	/* attached conf */
	struct server_conf *att_sconf;

	struct rb_sockaddr_storage ip;
	time_t last_nick_change;
	int number_of_nick_changes;

	/*
	 * XXX - there is no reason to save this, it should be checked when it's
	 * received and not stored, this is not used after registration
	 *
	 * agreed. lets get rid of it someday! --nenolod
	 */
	char *passwd;
	char *auth_user;
	char *challenge;
	char *fullcaps;
	char *cipher_string;

	int caps;		/* capabilities bit-field */
	rb_fde_t *F;		/* >= 0, for local clients */

	/* time challenge response is valid for */
	time_t chal_time;

	time_t next_away;	/* Don't allow next away before... */
	time_t last;

	/* clients allowed to talk through +g */
	rb_dlink_list allow_list;

	/* nicknames theyre monitoring */
	rb_dlink_list monitor_list;

	/*
	 * Anti-flood stuff. We track how many messages were parsed and how
	 * many we were allowed in the current second, and apply a simple decay
	 * to avoid flooding.
	 *   -- adrian
	 */
	int sent_parsed;	/* how many messages we've parsed in this second */
	time_t last_knock;	/* time of last knock */
	uint32_t random_ping;

	/* target change stuff */
	/* targets we're aware of (fnv32(use_id(target_p))):
	 * 0..TGCHANGE_NUM-1 regular slots
	 * TGCHANGE_NUM..TGCHANGE_NUM+TGCHANGE_REPLY-1 reply slots
	 */
	uint32_t targets[TGCHANGE_NUM + TGCHANGE_REPLY];
	unsigned int targets_free;	/* free targets */
	time_t target_last;		/* last time we cleared a slot */

	/* ratelimit items */
	time_t ratelimit;
	unsigned int join_who_credits;

	struct ListClient *safelist_data;

	char *mangledhost; /* non-NULL if host mangling module loaded and
			      applicable to this client */

	struct _ssl_ctl *ssl_ctl;		/* which ssl daemon we're associate with */
	struct _ssl_ctl *z_ctl;			/* second ctl for ssl+zlib */
	struct ws_ctl *ws_ctl;			/* ctl for wsockd */
	SSL_OPEN_CB *ssl_callback;		/* ssl connection is now open */
	uint32_t localflags;
	struct ZipStats *zipstats;		/* zipstats */
	uint16_t cork_count;			/* used for corking/uncorking connections */
	struct ev_entry *event;			/* used for associated events */

	struct resume_session *resume;

	char sasl_agent[IDLEN];
	unsigned char sasl_out;
	unsigned char sasl_complete;

	unsigned int sasl_messages;
	unsigned int sasl_failures;
	time_t sasl_next_retry;
};

#define AUTHC_F_DEFERRED 0x01
#define AUTHC_F_COMPLETE 0x02

struct AuthClient
{
	uint32_t cid;	/* authd id */
	time_t timeout;	/* When to terminate authd query */
	bool accepted;	/* did authd accept us? */
	char cause;	/* rejection cause */
	char *data;	/* reason data */
	char *reason;	/* reason we were rejected */
	int flags;
};

struct PreClient
{
	char spoofnick[NICKLEN + 1];
	char spoofuser[USERLEN + 1];
	char spoofhost[HOSTLEN + 1];

	struct AuthClient auth;

	struct rb_sockaddr_storage lip; /* address of our side of the connection */

	char id[IDLEN]; /* UID/SID, unique on the network (unverified) */
};

struct ListClient
{
	char *chname;
	unsigned int users_min, users_max;
	time_t created_min, created_max, topic_min, topic_max;
	int operspy;
};

/*
 * status macros.
 */
#define STAT_CONNECTING         0x01
#define STAT_HANDSHAKE          0x02
#define STAT_ME                 0x04
#define STAT_UNKNOWN            0x08
#define STAT_REJECT		0x10
#define STAT_SERVER             0x20
#define STAT_CLIENT             0x40


#define IsRegisteredUser(x)     ((x)->status == STAT_CLIENT)
#define IsRegistered(x)         (((x)->status  > STAT_UNKNOWN) && ((x)->status != STAT_REJECT))
#define IsConnecting(x)         ((x)->status == STAT_CONNECTING)
#define IsHandshake(x)          ((x)->status == STAT_HANDSHAKE)
#define IsMe(x)                 ((x)->status == STAT_ME)
#define IsUnknown(x)            ((x)->status == STAT_UNKNOWN)
#define IsServer(x)             ((x)->status == STAT_SERVER)
#define IsClient(x)             ((x)->status == STAT_CLIENT)
#define IsReject(x)		((x)->status == STAT_REJECT)

#define IsAnyServer(x)          (IsServer(x) || IsHandshake(x) || IsConnecting(x))

#define IsOper(x)		((x)->umodes & UMODE_OPER)
#define IsAdmin(x)		((x)->umodes & UMODE_ADMIN)

#define SetReject(x)		{(x)->status = STAT_REJECT; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetConnecting(x)        {(x)->status = STAT_CONNECTING; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetHandshake(x)         {(x)->status = STAT_HANDSHAKE; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetMe(x)                {(x)->status = STAT_ME; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetUnknown(x)           {(x)->status = STAT_UNKNOWN; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetServer(x)            {(x)->status = STAT_SERVER; \
				 (x)->handler = SERVER_HANDLER; }

#define SetClient(x)            {(x)->status = STAT_CLIENT; \
				 (x)->handler = IsOper((x)) ? \
					OPER_HANDLER : CLIENT_HANDLER; }
#define SetRemoteClient(x)	{(x)->status = STAT_CLIENT; \
				 (x)->handler = RCLIENT_HANDLER; }

#define STAT_CLIENT_PARSE (STAT_UNKNOWN | STAT_CLIENT)
#define STAT_SERVER_PARSE (STAT_CONNECTING | STAT_HANDSHAKE | STAT_SERVER)

#define PARSE_AS_CLIENT(x)      ((x)->status & STAT_CLIENT_PARSE)
#define PARSE_AS_SERVER(x)      ((x)->status & STAT_SERVER_PARSE)


/*
 * ts stuff
 */
#define TS_CURRENT	6
#define TS_MIN          6

#define TS_DOESTS       0x10000000
#define DoesTS(x)       ((x)->tsinfo & TS_DOESTS)

#define has_id(source)	((source)->id[0] != '\0')
#define use_id(source)	((source)->id[0] != '\0' ? (source)->id : (source)->name)

/* if target is TS6, use id if it has one, else name */
#define get_id(source, target) ((IsServer(target->from) && has_id(target->from)) ? \
				use_id(source) : (source)->name)

/* housekeeping flags */

#define FLAGS_PINGSENT		0x00000001	/* Unreplied ping sent */
#define FLAGS_DEAD		0x00000002	/* Local socket is dead--Exiting soon */
#define FLAGS_KILLED		0x00000004	/* Prevents "QUIT" from being sent for this */
#define FLAGS_SENTUSER		0x00000008	/* Client sent a USER command. */
#define FLAGS_CLICAP		0x00000010	/* In CAP negotiation, wait for CAP END */
#define FLAGS_CLOSING		0x00000020	/* set when closing to suppress errors */
#define FLAGS_PING_COOKIE	0x00000040	/* has sent ping cookie */
#define FLAGS_GOTID		0x00000080	/* successful ident lookup achieved */
#define FLAGS_FLOODDONE		0x00000100	/* flood grace period over / reported */
#define FLAGS_NORMALEX		0x00000200	/* Client exited normally */
#define FLAGS_MARK		0x00000400	/* marked client */
#define FLAGS_HIDDEN		0x00000800	/* hidden server */
#define FLAGS_EOB		0x00001000	/* EOB */
#define FLAGS_MYCONNECT		0x00002000	/* MyConnect */
#define FLAGS_IOERROR		0x00004000	/* IO error */
#define FLAGS_SERVICE		0x00008000	/* network service */
#define FLAGS_TGCHANGE		0x00010000	/* we're allowed to clear something */
#define FLAGS_DYNSPOOF		0x00020000	/* dynamic spoof, only opers see ip */
#define FLAGS_TGEXCESSIVE	0x00040000	/* whether the client has attemped to change targets excessively fast */
#define FLAGS_CLICAP_DATA	0x00080000	/* requested CAP LS 302 */
#define FLAGS_EXTENDCHANS	0x00100000
#define FLAGS_EXEMPTRESV	0x00200000
#define FLAGS_EXEMPTKLINE	0x00400000
#define FLAGS_EXEMPTFLOOD	0x00800000
#define FLAGS_IP_SPOOFING	0x01000000
#define FLAGS_EXEMPTSPAMBOT	0x02000000
#define FLAGS_EXEMPTSHIDE	0x04000000
#define FLAGS_EXEMPTJUPE	0x08000000


/* flags for local clients, this needs stuff moved from above to here at some point */
#define LFLAGS_SSL		0x00000001
#define LFLAGS_FLUSH		0x00000002
#define LFLAGS_CORK		0x00000004
#define LFLAGS_SCTP		0x00000008
#define LFLAGS_INSECURE	0x00000010	/* for marking SSL clients as insecure before registration */

/* umodes, settable flags */
/* lots of this moved to snomask -- jilles */
#define UMODE_SERVNOTICE   0x0001	/* server notices */
#define UMODE_WALLOP       0x0002	/* send wallops to them */
#define UMODE_OPERWALL     0x0004	/* Operwalls */
#define UMODE_INVISIBLE    0x0008	/* makes user invisible */
#define UMODE_CALLERID     0x0010	/* block unless caller id's */
#define UMODE_LOCOPS       0x0020	/* show locops */
#define UMODE_SERVICE      0x0040
#define UMODE_DEAF	   0x0080
#define UMODE_NOFORWARD    0x0100	/* don't forward */
#define UMODE_REGONLYMSG   0x0200	/* only allow logged in users to msg */

/* user information flags, only settable by remote mode or local oper */
#define UMODE_OPER         0x1000	/* Operator */
#define UMODE_ADMIN        0x2000	/* Admin on server */
#define UMODE_SSLCLIENT    0x4000	/* using SSL */

#define DEFAULT_OPER_UMODES (UMODE_SERVNOTICE | UMODE_OPERWALL | \
                             UMODE_WALLOP | UMODE_LOCOPS)
#define DEFAULT_OPER_SNOMASK SNO_GENERAL

/*
 * flags macros.
 */
#define IsPerson(x)             (IsClient(x) && (x)->user != NULL)
#define HasServlink(x)          ((x)->flags &  FLAGS_SERVLINK)
#define SetServlink(x)          ((x)->flags |= FLAGS_SERVLINK)
#define MyConnect(x)		((x)->flags & FLAGS_MYCONNECT)
#define SetMyConnect(x)		((x)->flags |= FLAGS_MYCONNECT)
#define ClearMyConnect(x)	((x)->flags &= ~FLAGS_MYCONNECT)

#define MyClient(x)             (MyConnect(x) && IsClient(x))
#define SetMark(x)		((x)->flags |= FLAGS_MARK)
#define ClearMark(x)		((x)->flags &= ~FLAGS_MARK)
#define IsMarked(x)		((x)->flags & FLAGS_MARK)
#define SetHidden(x)		((x)->flags |= FLAGS_HIDDEN)
#define ClearHidden(x)		((x)->flags &= ~FLAGS_HIDDEN)
#define IsHidden(x)		((x)->flags & FLAGS_HIDDEN)
#define ClearEob(x)		((x)->flags &= ~FLAGS_EOB)
#define SetEob(x)		((x)->flags |= FLAGS_EOB)
#define HasSentEob(x)		((x)->flags & FLAGS_EOB)
#define IsDead(x)          	((x)->flags &  FLAGS_DEAD)
#define SetDead(x)         	((x)->flags |= FLAGS_DEAD)
#define IsClosing(x)		((x)->flags & FLAGS_CLOSING)
#define SetClosing(x)		((x)->flags |= FLAGS_CLOSING)
#define IsIOError(x)		((x)->flags & FLAGS_IOERROR)
#define SetIOError(x)		((x)->flags |= FLAGS_IOERROR)
#define IsAnyDead(x)		(IsIOError(x) || IsDead(x) || IsClosing(x))
#define IsTGChange(x)		((x)->flags & FLAGS_TGCHANGE)
#define SetTGChange(x)		((x)->flags |= FLAGS_TGCHANGE)
#define ClearTGChange(x)	((x)->flags &= ~FLAGS_TGCHANGE)
#define IsDynSpoof(x)		((x)->flags & FLAGS_DYNSPOOF)
#define SetDynSpoof(x)		((x)->flags |= FLAGS_DYNSPOOF)
#define ClearDynSpoof(x)	((x)->flags &= ~FLAGS_DYNSPOOF)
#define IsTGExcessive(x)	((x)->flags & FLAGS_TGEXCESSIVE)
#define SetTGExcessive(x)	((x)->flags |= FLAGS_TGEXCESSIVE)
#define ClearTGExcessive(x)	((x)->flags &= ~FLAGS_TGEXCESSIVE)

/* local flags */

#define IsSSL(x)		((x)->localClient->localflags & LFLAGS_SSL)
#define SetSSL(x)		((x)->localClient->localflags |= LFLAGS_SSL)
#define ClearSSL(x)		((x)->localClient->localflags &= ~LFLAGS_SSL)

#define IsFlush(x)		((x)->localClient->localflags & LFLAGS_FLUSH)
#define SetFlush(x)		((x)->localClient->localflags |= LFLAGS_FLUSH)
#define ClearFlush(x)		((x)->localClient->localflags &= ~LFLAGS_FLUSH)

#define IsSCTP(x)		((x)->localClient->localflags & LFLAGS_SCTP)
#define SetSCTP(x)		((x)->localClient->localflags |= LFLAGS_SCTP)
#define ClearSCTP(x)		((x)->localClient->localflags &= ~LFLAGS_SCTP)

#define IsInsecure(x)		((x)->localClient->localflags & LFLAGS_INSECURE)
#define SetInsecure(x)		((x)->localClient->localflags |= LFLAGS_INSECURE)
#define ClearInsecure(x)	((x)->localClient->localflags &= ~LFLAGS_INSECURE)

/* oper flags */
#define MyOper(x)               (MyConnect(x) && IsOper(x))

#define SetOper(x)              {(x)->umodes |= UMODE_OPER; \
				 if (MyClient((x))) (x)->handler = OPER_HANDLER;}

#define ClearOper(x)            {(x)->umodes &= ~(UMODE_OPER|UMODE_ADMIN); \
				 if (MyClient((x)) && !IsOper((x)) && !IsServer((x))) \
				  (x)->handler = CLIENT_HANDLER; }

/* umode flags */
#define IsInvisible(x)          ((x)->umodes & UMODE_INVISIBLE)
#define SetInvisible(x)         ((x)->umodes |= UMODE_INVISIBLE)
#define ClearInvisible(x)       ((x)->umodes &= ~UMODE_INVISIBLE)
#define IsSSLClient(x)		((x)->umodes & UMODE_SSLCLIENT)
#define SetSSLClient(x)		((x)->umodes |= UMODE_SSLCLIENT)
#define ClearSSLClient(x)	((x)->umodes &= ~UMODE_SSLCLIENT)
#define SendWallops(x)          ((x)->umodes & UMODE_WALLOP)
#define SendLocops(x)           ((x)->umodes & UMODE_LOCOPS)
#define SendServNotice(x)       ((x)->umodes & UMODE_SERVNOTICE)
#define SendOperwall(x)         ((x)->umodes & UMODE_OPERWALL)
#define IsSetCallerId(x)	((x)->umodes & UMODE_CALLERID)
#define IsService(x)		((x)->umodes & UMODE_SERVICE)
#define IsDeaf(x)		((x)->umodes & UMODE_DEAF)
#define IsNoForward(x)		((x)->umodes & UMODE_NOFORWARD)
#define IsSetRegOnlyMsg(x)	((x)->umodes & UMODE_REGONLYMSG)

#define SetGotId(x)             ((x)->flags |= FLAGS_GOTID)
#define IsGotId(x)              (((x)->flags & FLAGS_GOTID) != 0)

#define IsExemptKline(x)        ((x)->flags & FLAGS_EXEMPTKLINE)
#define SetExemptKline(x)       ((x)->flags |= FLAGS_EXEMPTKLINE)
#define IsExemptFlood(x)        ((x)->flags & FLAGS_EXEMPTFLOOD)
#define SetExemptFlood(x)       ((x)->flags |= FLAGS_EXEMPTFLOOD)
#define IsExemptSpambot(x)	((x)->flags & FLAGS_EXEMPTSPAMBOT)
#define SetExemptSpambot(x)	((x)->flags |= FLAGS_EXEMPTSPAMBOT)
#define IsExemptShide(x)	((x)->flags & FLAGS_EXEMPTSHIDE)
#define SetExemptShide(x)	((x)->flags |= FLAGS_EXEMPTSHIDE)
#define IsExemptJupe(x)		((x)->flags & FLAGS_EXEMPTJUPE)
#define SetExemptJupe(x)	((x)->flags |= FLAGS_EXEMPTJUPE)
#define IsExemptResv(x)		((x)->flags & FLAGS_EXEMPTRESV)
#define SetExemptResv(x)	((x)->flags |= FLAGS_EXEMPTRESV)
#define IsIPSpoof(x)            ((x)->flags & FLAGS_IP_SPOOFING)
#define SetIPSpoof(x)           ((x)->flags |= FLAGS_IP_SPOOFING)
#define IsExtendChans(x)	((x)->flags & FLAGS_EXTENDCHANS)
#define SetExtendChans(x)	((x)->flags |= FLAGS_EXTENDCHANS)

/* for local users: flood grace period is over
 * for servers: mentioned in networknotice.c notice
 */
#define IsFloodDone(x)          ((x)->flags & FLAGS_FLOODDONE)
#define SetFloodDone(x)         ((x)->flags |= FLAGS_FLOODDONE)

/* These also operate on the uplink from which it came */
#define IsCork(x)		(MyConnect(x) ? (x)->localClient->cork_count : (x)->from->localClient->cork_count)
#define SetCork(x)		(MyConnect(x) ? (x)->localClient->cork_count++ : (x)->from->localClient->cork_count++ )
#define ClearCork(x)		(MyConnect(x) ? (x)->localClient->cork_count-- : (x)->from->localClient->cork_count--)

/*
 * definitions for get_client_name
 */
#define HIDE_IP 0
#define SHOW_IP 1
#define MASK_IP 2

enum
{
	D_LINED,
	K_LINED
};

extern void check_banned_lines(void);
extern void check_klines(void);
extern void check_one_kline(struct ConfItem *kline);
extern void check_dlines(void);
extern void check_xlines(void);
extern void resv_nick_fnc(const char *mask, const char *reason, int temp_time);

extern const char *get_client_name(struct Client *client, int show_ip);
extern const char *log_client_name(struct Client *, int);
extern int is_remote_connect(struct Client *);
extern void init_client(void);
extern struct Client *make_client(struct Client *from);
extern void free_pre_client(struct Client *client);

extern void notify_banned_client(struct Client *, struct ConfItem *, int ban);
extern int exit_client(struct Client *, struct Client *, struct Client *, const char *);

extern void error_exit_client(struct Client *, int);

extern void count_local_client_memory(size_t * count, size_t * memory);
extern void count_remote_client_memory(size_t * count, size_t * memory);

extern int clean_nick(const char *, int loc_client);

extern struct Client *find_chasing(struct Client *, const char *, int *);
extern struct Client *find_person(const char *);
extern struct Client *find_named_person(const char *);
extern struct Client *next_client(struct Client *, const char *);

#define accept_message(s, t) ((s) == (t) || (rb_dlinkFind((s), &((t)->localClient->allow_list))))
extern void del_all_accepts(struct Client *client_p);

extern void dead_link(struct Client *client_p, int sendqex);
extern int show_ip(struct Client *source_p, struct Client *target_p);
extern int show_ip_conf(struct ConfItem *aconf, struct Client *source_p);
extern int show_ip_whowas(struct Whowas *whowas, struct Client *source_p);

extern void free_user(struct User *, struct Client *);
extern struct User *make_user(struct Client *);
extern struct Server *make_server(struct Client *);
extern void close_connection(struct Client *);
extern void init_uid(void);
extern char *generate_uid(void);

void allocate_away(struct Client *);
void free_away(struct Client *);

uint32_t connid_get(struct Client *client_p);
void connid_put(uint32_t id);
void client_release_connids(struct Client *client_p);

#endif /* INCLUDED_client_h */
