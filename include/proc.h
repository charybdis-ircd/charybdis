/*
 *  charybdis: Defibrillation paddles on the irc protocol CLEAR...bzzzt
 *  proc.h: Modules in Hausdorff space.
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
 *
 * Provides functionality combining helpers and modules. A version 3 module
 * created with some special attributes defined here, when loaded, will also be
 * executed as a new process. The loaded module can exert control from the parent
 * (main ircd) address space, the module can operate and crash safely out of the
 * child address space, and the logic all these things can be entirely in one place.
 *
 * !!! REMINDER !!!
 * The charybdis design spec mandates address space isolation and message passing as
 * the concurrency model, so the execution of a module as subprocess occurs in a new
 * address space. This reminder exists because code for both spaces may be next to
 * each other in the same module file. Keep in mind.
 *
 */

#ifndef INCLUDED_proc_h
#define INCLUDED_proc_h

// Usage:
// 0. Setup a module file IN THE /proc DIRECTORY, with a VERSION 3 DECLARATION
//    (i.e copy the example)
//
// 1. Include stuff, and the proc_module.h and place a proc_t structure in your module's main file:
// > proc_t proc;
//
// 2. Add the following example as a line in your module declaration:
// > MOD_ATTR { "proc", &proc }
//
//

typedef enum
{
	PROC_ERROR,
	PROC_DIE,
	PROC_RESTART,
	PROC_LOG,
	PROC_READ,
	PROC_SIGNAL,
}
proc_event_t;

typedef enum
{
	PROC_STATE_DEAD     = -1,
	PROC_STATE_INIT     = 0,
	PROC_STATE_EXEC     = 1,
	PROC_STATE_RUNNING  = 2,
}
proc_state_t;

typedef bool (proc_handle_event_t)(proc_event_t, const void *msg, size_t size);
typedef void (proc_handle_request_t)(int64_t id, char *msg, size_t size);
typedef void (proc_handle_reply_t)(void *state, char *msg, ssize_t size);
typedef struct proc_ctrl
{
	int threads;
	time_t timeout;
	const char *log_prefix;
	proc_handle_event_t *handle_event;
	proc_handle_request_t *handle_request;
	proc_handle_reply_t *handle_reply;
}
proc_ctrl_t;

typedef struct proc
{
	// public
	proc_ctrl_t ctrl;

	// caution
	proc_state_t state;
	struct module *mod;
	rb_helper *helper;
	uint64_t req_ctr;
	rb_dictionary *reqs;
}
proc_t;

proc_t *procs_find_by_helper(const rb_helper *);
proc_t *procs_find_by_module(struct module *);
void proc_log(proc_t *, const char *fmt, ...) AFP(2, 3);
const char *proc_reflect_event(const proc_event_t);
const char *proc_reflect_state(const proc_state_t);
uint64_t proc_request(proc_t *, void *priv, const char *msg);
uint64_t proc_requestf(proc_t *, void *priv, const char *fmt, ...) AFP(3, 4);
bool proc_create(struct module *module, proc_t *attr);  // ctor should only be called by module system
void proc_delete(struct module *module, proc_t *attr);  // dtor should only be called by module system


#endif // INCLUDED_proc_h
