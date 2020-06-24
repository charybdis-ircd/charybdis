/*
 * Copyright (C) 2004-2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2004-2005 ircd-ratbox development team
 */
#ifndef INCLUDED_HOOK_H
#define INCLUDED_HOOK_H

typedef struct
{
	char *name;
	rb_dlink_list hooks;
} hook;

enum hook_priority
{
	HOOK_LOWEST = 10,
	HOOK_LOW = 20,
	HOOK_NORMAL = 30,
	HOOK_HIGH = 40,
	HOOK_HIGHEST = 50,
	HOOK_MONITOR = 100
};

typedef void (*hookfn) (void *data);

extern int h_iosend_id;
extern int h_iorecv_id;
extern int h_iorecvctrl_id;

extern int h_burst_client;
extern int h_burst_channel;
extern int h_burst_finished;
extern int h_server_introduced;
extern int h_server_eob;
extern int h_client_exit;
extern int h_after_client_exit;
extern int h_umode_changed;
extern int h_new_local_user;
extern int h_new_remote_user;
extern int h_introduce_client;
extern int h_can_kick;
extern int h_privmsg_channel;
extern int h_privmsg_user;
extern int h_conf_read_start;
extern int h_conf_read_end;
extern int h_outbound_msgbuf;
extern int h_rehash;
extern int h_cap_change;
extern int h_sendq_cleared;

void init_hook(void);
int register_hook(const char *name);
void add_hook(const char *name, hookfn fn);
void add_hook_prio(const char *name, hookfn fn, enum hook_priority priority);
void remove_hook(const char *name, hookfn fn);
void call_hook(int id, void *arg);

typedef struct
{
	struct Client *client;
	void *arg1;
	void *arg2;
} hook_data;

typedef struct
{
	struct Client *client;
	const void *arg1;
	const void *arg2;
} hook_cdata;

typedef struct
{
	struct Client *client;
	const void *arg1;
	int arg2;
	int result;
} hook_data_int;

typedef struct
{
	struct Client *client;
	struct Client *target;
	struct Channel *chptr;
	int approved;
} hook_data_client;

typedef struct
{
	struct Client *client;
	struct Channel *chptr;
	int approved;
} hook_data_channel;

typedef struct
{
	struct Client *client;
	struct Channel *chptr;
	const char *key;
} hook_data_channel_activity;

typedef struct
{
	struct Client *client;
	struct Channel *chptr;
	struct membership *msptr;
	struct Client *target;
	int approved;
	int dir;
	const char *modestr;
	const char *error;
} hook_data_channel_approval;

typedef struct
{
	struct Client *client;
	struct Client *target;
	int approved;
} hook_data_client_approval;

typedef struct
{
	struct Client *local_link; /* local client originating this, or NULL */
	struct Client *target; /* dying client */
	struct Client *from; /* causing client (could be &me or target) */
	const char *comment;
} hook_data_client_exit;

typedef struct
{
	struct Client *client;
	const char *reason;
	const char *orig_reason;
} hook_data_client_quit;

typedef struct
{
	struct Client *client;
	unsigned int oldumodes;
	unsigned int oldsnomask;
} hook_data_umode_changed;

typedef struct
{
	struct Client *client;
	int oldcaps;
	int add;
	int del;
} hook_data_cap_change;

enum message_type {
	MESSAGE_TYPE_NOTICE,
	MESSAGE_TYPE_PRIVMSG,
	MESSAGE_TYPE_PART,
	MESSAGE_TYPE_COUNT
};

typedef struct
{
	enum message_type msgtype;
	struct Client *source_p;
	struct Channel *chptr;
	const char *text;
	int approved;
} hook_data_privmsg_channel;

typedef struct
{
	enum message_type msgtype;
	struct Client *source_p;
	struct Client *target_p;
	const char *text;
	int approved;
} hook_data_privmsg_user;

typedef struct
{
	bool signal;
} hook_data_rehash;

#endif
