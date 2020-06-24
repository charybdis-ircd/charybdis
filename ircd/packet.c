/*
 *  ircd-ratbox: A slightly useful ircd.
 *  packet.c: Packet handlers.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
#include "stdinc.h"
#include "s_conf.h"
#include "s_serv.h"
#include "client.h"
#include "ircd.h"
#include "parse.h"
#include "packet.h"
#include "match.h"
#include "hook.h"
#include "send.h"
#include "s_assert.h"

static char readBuf[READBUF_SIZE];
static void client_dopacket(struct Client *client_p, char *buffer, size_t length);

/*
 * parse_client_queued - parse client queued messages
 */
static void
parse_client_queued(struct Client *client_p)
{
	int dolen = 0;
	int allow_read;

	if(IsAnyDead(client_p))
		return;

	if(IsUnknown(client_p))
	{
		allow_read = ConfigFileEntry.client_flood_burst_max;
		for (;;)
		{
			if(client_p->localClient->sent_parsed >= allow_read)
				break;

			dolen = rb_linebuf_get(&client_p->localClient->
					    buf_recvq, readBuf, READBUF_SIZE,
					    LINEBUF_COMPLETE, LINEBUF_PARSED);

			if(dolen <= 0 || IsDead(client_p))
				break;

			client_dopacket(client_p, readBuf, dolen);
			client_p->localClient->sent_parsed++;

			/* He's dead cap'n */
			if(IsAnyDead(client_p))
				return;
			/* if theyve dropped out of the unknown state, break and move
			 * to the parsing for their appropriate status.  --fl
			 */
			if(!IsUnknown(client_p))
			{
				/* reset their flood limits, they're now
				 * graced to flood
				 */
				client_p->localClient->sent_parsed = 0;
				break;
			}

		}
		/* If sent_parsed is impossibly high, drop it down.
		 * This is useful if the configuration is changed.
		 */
		if(client_p->localClient->sent_parsed > allow_read)
			client_p->localClient->sent_parsed = allow_read;
	}

	if(IsAnyServer(client_p) || IsExemptFlood(client_p))
	{
		while (!IsAnyDead(client_p) && (dolen = rb_linebuf_get(&client_p->localClient->buf_recvq,
					   readBuf, READBUF_SIZE, LINEBUF_COMPLETE,
					   LINEBUF_PARSED)) > 0)
		{
			client_dopacket(client_p, readBuf, dolen);
		}
	}
	else if(IsClient(client_p))
	{
		if(IsFloodDone(client_p))
			allow_read = ConfigFileEntry.client_flood_burst_max;
		else
			allow_read = ConfigFileEntry.client_flood_burst_rate;
		allow_read *= ConfigFileEntry.client_flood_message_time;
		/* allow opers 4 times the amount of messages as users. why 4?
		 * why not. :) --fl_
		 */
		if(IsOper(client_p) && ConfigFileEntry.no_oper_flood)
			allow_read *= 4;
		/*
		 * Handle flood protection here - if we exceed our flood limit on
		 * messages in this loop, we simply drop out of the loop prematurely.
		 *   -- adrian
		 */
		for (;;)
		{
			/* This flood protection works as follows:
			 *
			 * A client is given allow_read lines to send to the server.  Every
			 * time a line is parsed, sent_parsed is increased.  sent_parsed
			 * is decreased by 1 every time flood_recalc is called.
			 *
			 * Thus a client can 'burst' allow_read lines to the server, any
			 * excess lines will be parsed one per flood_recalc() call.
			 *
			 * Therefore a client will be penalised more if they keep flooding,
			 * as sent_parsed will always hover around the allow_read limit
			 * and no 'bursts' will be permitted.
			 */
			if(client_p->localClient->sent_parsed >= allow_read)
				break;

			/* post_registration_delay hack. Don't process any messages from a new client for $n seconds,
			 * to allow network bots to do their thing before channels can be joined.
			 */
			if (rb_current_time() < client_p->localClient->firsttime + ConfigFileEntry.post_registration_delay)
				break;

			dolen = rb_linebuf_get(&client_p->localClient->
					    buf_recvq, readBuf, READBUF_SIZE,
					    LINEBUF_COMPLETE, LINEBUF_PARSED);

			if(!dolen)
				break;

			client_dopacket(client_p, readBuf, dolen);
			if(IsAnyDead(client_p))
				return;

			client_p->localClient->sent_parsed += ConfigFileEntry.client_flood_message_time;
		}
		/* If sent_parsed is impossibly high, drop it down.
		 * This is useful if the configuration is changed.
		 */
		if(client_p->localClient->sent_parsed > allow_read +
				ConfigFileEntry.client_flood_message_time - 1)
			client_p->localClient->sent_parsed = allow_read +
				ConfigFileEntry.client_flood_message_time - 1;
	}
}

/* flood_endgrace()
 *
 * marks the end of the clients grace period
 */
void
flood_endgrace(struct Client *client_p)
{
	SetFloodDone(client_p);

	/* sent_parsed could be way over client_flood_burst_max but under
	 * client_flood_burst_rate so reset it.
	 */
	client_p->localClient->sent_parsed = 0;
}

/*
 * flood_recalc
 *
 * recalculate the number of allowed flood lines. this should be called
 * once a second on any given client. We then attempt to flush some data.
 */
void
flood_recalc(void *unused)
{
	rb_dlink_node *ptr, *next;
	struct Client *client_p;

	RB_DLINK_FOREACH_SAFE(ptr, next, lclient_list.head)
	{
		client_p = ptr->data;

		if(rb_unlikely(IsMe(client_p)))
			continue;

		if(rb_unlikely(client_p->localClient == NULL))
			continue;

		if(IsFloodDone(client_p))
			client_p->localClient->sent_parsed -= ConfigFileEntry.client_flood_message_num;
		else
			client_p->localClient->sent_parsed = 0;

		if(client_p->localClient->sent_parsed < 0)
			client_p->localClient->sent_parsed = 0;

		parse_client_queued(client_p);

		if(rb_unlikely(IsAnyDead(client_p)))
			continue;

	}

	RB_DLINK_FOREACH_SAFE(ptr, next, unknown_list.head)
	{
		client_p = ptr->data;

		if(client_p->localClient == NULL)
			continue;

		client_p->localClient->sent_parsed--;

		if(client_p->localClient->sent_parsed < 0)
			client_p->localClient->sent_parsed = 0;

		parse_client_queued(client_p);
	}
}

/*
 * read_packet - Read a 'packet' of data from a connection and process it.
 */
void
read_packet(rb_fde_t * F, void *data)
{
	struct Client *client_p = data;
	int length = 0;
	int binary = 0;

	while(1)
	{
		if(IsAnyDead(client_p))
			return;

		/*
		 * Read some data. We *used to* do anti-flood protection here, but
		 * I personally think it makes the code too hairy to make sane.
		 *     -- adrian
		 */
		length = rb_read(client_p->localClient->F, readBuf, READBUF_SIZE);

		if(length < 0)
		{
			if(rb_ignore_errno(errno))
				rb_setselect(client_p->localClient->F,
						RB_SELECT_READ, read_packet, client_p);
			else
				error_exit_client(client_p, length);
			return;
		}
		else if(length == 0)
		{
			error_exit_client(client_p, length);
			return;
		}

		if(client_p->localClient->lasttime < rb_current_time())
			client_p->localClient->lasttime = rb_current_time();
		client_p->flags &= ~FLAGS_PINGSENT;

		/*
		 * Before we even think of parsing what we just read, stick
		 * it on the end of the receive queue and do it when its
		 * turn comes around.
		 */
		if(IsHandshake(client_p) || IsUnknown(client_p))
			binary = 1;

		(void) rb_linebuf_parse(&client_p->localClient->buf_recvq, readBuf, length, binary);

		if(IsAnyDead(client_p))
			return;

		/* Attempt to parse what we have */
		parse_client_queued(client_p);

		if(IsAnyDead(client_p))
			return;

		if(client_p->localClient->F == NULL)
			return;

		/* Check to make sure we're not flooding */
		if(!IsAnyServer(client_p) &&
		   (rb_linebuf_alloclen(&client_p->localClient->buf_recvq) > ConfigFileEntry.client_flood_max_lines))
		{
			if(!(ConfigFileEntry.no_oper_flood && IsOper(client_p)))
			{
				exit_client(client_p, client_p, client_p, "Excess Flood");
				return;
			}
		}

		/* bail if short read, but not for SCTP as it returns data in packets */
		if (length < READBUF_SIZE && !(rb_get_type(client_p->localClient->F) & RB_FD_SCTP)) {
			rb_setselect(client_p->localClient->F, RB_SELECT_READ, read_packet, client_p);
			return;
		}
	}
}

/*
 * client_dopacket - copy packet to client buf and parse it
 *      client_p - pointer to client structure for which the buffer data
 *             applies.
 *      buffer - pointr to the buffer containing the newly read data
 *      length - number of valid bytes of data in the buffer
 *
 * Note:
 *      It is implicitly assumed that dopacket is called only
 *      with client_p of "local" variation, which contains all the
 *      necessary fields (buffer etc..)
 */
void
client_dopacket(struct Client *client_p, char *buffer, size_t length)
{
	s_assert(client_p != NULL);
	s_assert(buffer != NULL);

	if(client_p == NULL || buffer == NULL)
		return;
	if(IsAnyDead(client_p))
		return;
	/*
	 * Update messages received
	 */
	++me.localClient->receiveM;
	++client_p->localClient->receiveM;

	/*
	 * Update bytes received
	 */
	client_p->localClient->receiveB += length;

	if(client_p->localClient->receiveB > 1023)
	{
		client_p->localClient->receiveK += (client_p->localClient->receiveB >> 10);
		client_p->localClient->receiveB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
	}

	me.localClient->receiveB += length;

	if(me.localClient->receiveB > 1023)
	{
		me.localClient->receiveK += (me.localClient->receiveB >> 10);
		me.localClient->receiveB &= 0x03ff;
	}

	parse(client_p, buffer, buffer + length);
}
