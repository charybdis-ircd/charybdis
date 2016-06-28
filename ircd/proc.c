/*
 *  charybdis: The fittest of the surviving IRCd
 *  proc.c: Modules in Hausdorff space.
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

#include "stdinc.h"
#include "modules.h"
#include "snomask.h"
#include "logger.h"
#include "proc.h"


typedef struct req
{
	uint64_t id;
	time_t started;
	void *state;         // available to software
}
req_t;

static req_t *req_new(void);
static void req_free(req_t *const *const);
static req_t *req_find(proc_t *, const uint64_t id);
static void req_del(proc_t *, req_t *);
static void req_add(proc_t *, req_t *);
static void req_delete(proc_t *, req_t *);
static req_t *req_create(proc_t *);

struct procs
{
    rb_dictionary *idx_module;
    rb_dictionary *idx_helper;
}
procs;

proc_t *procs_find_by_helper(const rb_helper *);
proc_t *procs_find_by_module(struct module *);
static void procs_fini(void);
static void procs_init(void);

void proc_log(proc_t *const proc, const char *const fmt, ...) AFP(2, 3);
const char *proc_reflect_event(const proc_event_t);
const char *proc_reflect_state(const proc_state_t);
static void proc_reqs_purge(proc_t *);
static bool proc_terminate(proc_t *);
static bool proc_spawn(proc_t *);
static bool proc_respawn(proc_t *);
static void handle_error(rb_helper *);
static void handle_read(rb_helper *);
static void proc_fini(proc_t *const *);
uint64_t proc_request(proc_t *, void *priv, const char *msg);
uint64_t proc_requestf(proc_t *, void *priv, const char *fmt, ...) AFP(3, 4);
void proc_delete(struct module *, proc_t *);
bool proc_create(struct module *, proc_t *);


__attribute__((constructor))
static void init(void)
{
	procs_init();
}

__attribute__((destructor))
static void fini(void)
{
	procs_fini();
}


bool proc_create(struct module *const mod,
                 proc_t *const proc)
{
	proc_t *p RB_UNIQUE_PTR(proc_fini) = proc;
	p->reqs = rb_dictionary_create("Subprocess request map", rb_uint32cmp);
	p->mod = mod;
	p->state = PROC_STATE_INIT;
	p->req_ctr = 0;

	if(!proc_spawn(p))
		return false;

	rb_dictionary_add(procs.idx_module, proc->mod, proc);
	p = NULL;
	return true;
}


void proc_delete(struct module *const mod,
                 proc_t *const proc)
{
	assert(proc->mod == mod);
	rb_dictionary_delete(procs.idx_module, proc->mod);
	proc_fini(&proc);
}


uint64_t proc_requestf(proc_t *const proc,
                       void *const priv,
                       const char *const fmt,
                       ...)
{
	va_list ap;
	va_start(ap, fmt);
	char buf[BUFSIZE];
	vsnprintf(buf, sizeof(buf), fmt, ap);
	const uint64_t ret = proc_request(proc, priv, buf);
	va_end(ap);
	return ret;
}


uint64_t proc_request(proc_t *const proc,
                      void *const priv,
                      const char *const msg)
{
	req_t *const req = req_create(proc);
	req->state = priv;
	req->started = rb_current_time();
	rb_helper_write(proc->helper, "%ld %s", req->id, msg);
	return req->id;
}


static
void proc_fini(proc_t *const *const proc)
{
	if(!proc || !*proc)
		return;

	proc_t *const p = *proc;
	proc_terminate(p);
	rb_dictionary_destroy(p->reqs, NULL, NULL);
	//note: proc itself is static in a module
}


static
void handle_read(rb_helper *const helper)
{
	char *text;
	char buf[BUFSIZE];
	size_t size = rb_helper_read(helper, buf, sizeof(buf));
	const int64_t id = strtol(buf, &text, 10);
	if(rb_unlikely(text <= buf || !text || !id || id == LONG_MIN || id == LONG_MAX))
	{
		slog(L_MAIN, SNO_GENERAL,
		     "proc_handle_read(): Invalid packet (id: %ld?) data[%s]",
		     id,
		     buf);

		return;
	}

	proc_t *const proc = procs_find_by_helper(helper);
	if(rb_unlikely(!proc))
	{
		slog(L_MAIN, SNO_GENERAL,
		     "proc_handle_read(): data with ID[%ld] helper[%p] has no associated process.",
		     id,
		     helper);

		return;
	}

	req_t *const req = req_find(proc, id);
	if(!req)
	{
		proc_log(proc, "Response ID[%ld] not found (invalid or expired, ignoring)", id);
		return;
	}

	//TODO: put this somewhere
	if(proc->state != PROC_STATE_RUNNING)
		proc->state = PROC_STATE_RUNNING;

	// Skip the space after the id
	if(rb_likely(text[0] == ' '))
		++text;

	size -= (text - buf);
	if(rb_likely(proc->ctrl.handle_reply))
		proc->ctrl.handle_reply(req->state, text, size);

	req_delete(proc, req);
}


static
void handle_error(rb_helper *const helper)
{
	proc_t *const proc = procs_find_by_helper(helper);
	if(rb_unlikely(!proc))
	{
		slog(L_MAIN, SNO_GENERAL, "proc_handle_error(): helper[%p] has no associated process.", helper);
		return;
	}

	switch(proc->state)
	{
		case PROC_STATE_RUNNING:
			proc_log(proc, "ERROR while running. Attempting restart...");
			if(!proc_respawn(proc))
				proc_log(proc, "ERROR while restarting. Giving up.");
			break;

		case PROC_STATE_EXEC:
			proc_log(proc, "ERROR during child startup. Won't restart to avoid infinite loop.");
			proc_terminate(proc);
			break;

		default:
			proc_log(proc, "ERROR in unknown state (%d). Won't restart.", proc->state);
			proc_terminate(proc);
			break;
	}
}


static
bool proc_respawn(proc_t *const proc)
{
	proc_terminate(proc);
	proc->state = PROC_STATE_INIT;
	return proc_spawn(proc);
}


static
bool proc_spawn(proc_t *const proc)
{
	if(proc->helper || proc->state > 0)
	{
		proc_log(proc, "Process is not able to spawn. (helper: %p state: %d)",
		         (const void *)proc->helper,
		         proc->state);

		return false;
	}

	struct module *const m = proc->mod;
	proc->state = PROC_STATE_EXEC;
	proc->helper = rb_helper_start(m->name, m->path, handle_read, handle_error);
	if(!proc->helper)
	{
		proc_log(proc, "Failed to spawn subprocess.");
		return false;
	}

	rb_dictionary_add(procs.idx_helper, proc->helper, proc);
	rb_helper_run(proc->helper);
	proc_log(proc, "Spawned subprocess.");
	return true;
}


static
bool proc_terminate(proc_t *const proc)
{
	if(!proc->helper)
		return false;

	proc_reqs_purge(proc);
	rb_helper_close(proc->helper);
	rb_dictionary_delete(procs.idx_helper, proc->helper);
	proc->helper = NULL;
	proc->state = PROC_STATE_DEAD;
	proc_log(proc, "Terminated subprocess.");
	return true;
}


static
void proc_reqs_purge(proc_t *const proc)
{
	req_t *req;
	rb_dictionary_iter state;
	RB_DICTIONARY_FOREACH(req, &state, proc->reqs)
	{
		if(rb_likely(proc->ctrl.handle_reply))
			proc->ctrl.handle_reply(req->state, NULL, -1);

		req_delete(proc, req);
	}
}


const char *proc_reflect_state(const proc_state_t state)
{
	switch(state)
	{
		case PROC_STATE_DEAD:     return "DEAD";
		case PROC_STATE_INIT:     return "INIT";
		case PROC_STATE_EXEC:     return "EXEC";
		case PROC_STATE_RUNNING:  return "RUNNING";
	}

	return "?????";
}


const char *proc_reflect_event(const proc_event_t event)
{
	switch(event)
	{
		case PROC_ERROR:    return "ERROR";
		case PROC_DIE:      return "DIE";
		case PROC_RESTART:  return "RESTART";
		case PROC_LOG:      return "LOG";
		case PROC_READ:     return "READ";
		case PROC_SIGNAL:   return "SIGNAL";
	}

	return "?????";
}


void proc_log(proc_t *const proc,
              const char *const fmt,
              ...)
{
	va_list ap;
	va_start(ap, fmt);

	char buf[BUFSIZE];
	vsnprintf(buf, sizeof(buf), fmt, ap);
	slog(L_MAIN, SNO_GENERAL, "proc %s: %s", proc->mod->name, buf);

	va_end(ap);
}



static
void procs_init(void)
{
	procs.idx_module = rb_dictionary_create("Subprocess index by module", rb_uint32cmp);
	procs.idx_helper = rb_dictionary_create("Subprocess index by helper", rb_uint32cmp);
}


static
void procs_fini(void)
{
	rb_dictionary_destroy(procs.idx_helper, NULL ,NULL);
	rb_dictionary_destroy(procs.idx_module, NULL, NULL);
}


proc_t *procs_find_by_module(struct module *const mod)
{
	rb_dictionary_element *const elem = rb_dictionary_find(procs.idx_module, mod);
	return elem? elem->data : NULL;
}


//TODO: rb_helper could use a priv*
proc_t *procs_find_by_helper(const rb_helper *const helper)
{
	rb_dictionary_element *const elem = rb_dictionary_find(procs.idx_helper, helper);
	return elem? elem->data : NULL;
}



static
req_t *req_create(proc_t *const proc)
{
	req_t *const req = req_new();
	req_add(proc, req);
	return req;
}


static
void req_delete(proc_t *const proc,
                req_t *const req)
{
	req_del(proc, req);
	req_free(&req);
}


static
void req_add(proc_t *const proc,
             req_t *const req)
{
	req->id = ++proc->req_ctr;
	rb_dictionary_add(proc->reqs, (const void *)req->id, req);
}


static
void req_del(proc_t *const proc,
             req_t *const req)
{
	rb_dictionary_delete(proc->reqs, (const void *)req->id);
}


static
req_t *req_find(proc_t *const proc,
                const uint64_t id)
{
	rb_dictionary_element *const elem = rb_dictionary_find(proc->reqs, (void *)id);
	return elem? elem->data : NULL;
}


static
void req_free(req_t *const *const req)
{
	if(!req || !*req)
		return;

	rb_free(*req);
}


static
req_t *req_new(void)
{
	req_t *const req = rb_malloc(sizeof(req_t));
	return req;
}
