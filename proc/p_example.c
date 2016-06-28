/*
 *  charybdis: Defibrillation paddles on the IRC protocol CLEAR...bzzzzt
 *  p_example.c: Example subprocess module
 *
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notices, and this permission notice is present and remains
 * unmodified in all copies and derivative works.
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
 *
 */

#include "proc_module.h"


// In child
static
bool child_event(proc_event_t ev, const void *const msg, size_t size)
{
	switch(ev)
	{
		case PROC_SIGNAL:
			hlog(L_MAIN, SNO_GENERAL, "event (%d): %s [caught: %d]", ev, proc_reflect_event(ev), *(const int *)msg);
			break;

		default:
			hlog(L_MAIN, SNO_GENERAL, "event (%d): %s (%zu): %s", ev, proc_reflect_event(ev), size, (const char *)msg?: "<null>");
			break;
	}

	return false;
}


// In child
static
void child_handler(int64_t id, char *const msg, size_t size)
{
	hlog(L_MAIN, SNO_GENERAL, "request %ld (%zu)[%s]", id, size, msg);
	replyf(id, "%s", msg);
}


// In parent
static
void parent_handler(void *const state, char *const msg, ssize_t size)
{
	hlog(L_MAIN, SNO_GENERAL, "response (%zd)[%s]", size, msg);
	sendto_one_notice(state, "got back [%s] from helper", msg);
}


// In parent
static
void example(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	char buf[BUFSIZE];
	rb_array_to_string(parc, parv, buf, sizeof(buf));
	sendto_one_notice(client_p, "sending [%s] to helper", buf);
	proc_requestf(&proc, client_p, "%s", buf);
}

struct Message mtab =
{
	"EXAMPLE", 0, 0, 0, 0,
	{
		mg_unreg, { example, 0 }, mg_ignore, mg_ignore, mg_ignore, { example, 0 }
	}
};


// In child
int main(int argc, char **argv)
{
	proc.ctrl.handle_event = child_event;
	proc.ctrl.handle_request = child_handler;
	run();
	return 0;
}


// In parent
proc_t proc =
{
	.ctrl.threads = 1,
	.ctrl.timeout = 5,
	.ctrl.handle_reply = parent_handler,
};


DECLARE_MODULE_AV3
(
	MOD_ATTR { "name", "example"        },
	MOD_ATTR
	{
		"description",
		"Example subprocess module."
	},
	MOD_ATTR { "proc", &proc            },
	MOD_ATTR { "mtab", &mtab            }
);
