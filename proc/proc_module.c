/*
 *  charybdis: The fittest of the surviving IRCd
 *  proc_module.h: Must be, and only be, included by subprocess modules.
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
 * Include this file at least where you DECLARE_MODULE_AV3.
 */

#include "proc_module.h"

sigset_t sigs;
volatile bool signaled[32];
volatile int interruption;
uint64_t requests;
uint64_t replies;

uint64_t request_count(void);
uint64_t reply_count(void);
void reply_raw(const char *msg);
void reply(const int64_t id, const char *msg);
void vreply(const int64_t id, const char *fmt, va_list ap);
void replyf(const int64_t id, const char *fmt, ...) AFP(2, 3);
static void log_strerror(const char *const prefix);
static void log_strerror_fatal(const char *const prefix) __attribute__((noreturn));
static void handle_sig(int signo);
static int interrupted(void);
static void handle_read(rb_helper *const helper);
static void handle_error(rb_helper *const helper);
static void handle_log(const char *const buf);
static void handle_restart(const char *const buf);
static void handle_die(const char *const buf);
static void init_signals(void);
static void init_log(void);
static void init_rb(void);
int run(void);
void _start(void) __attribute__((noreturn));


void _start(void)
{
	proc.helper = rb_helper_child(handle_read,
	                              handle_error,
	                              handle_log,
	                              handle_restart,
	                              handle_die,
	                              256, 256, 256);
	init_rb();
	init_log();
	init_signals();

	const int ret = main(0, NULL);        //TODO: main args?
	if(ret != 0)
		hlog(L_MAIN, SNO_GENERAL,
		     "Terminating with non-zero (%d) main() return value",
		     ret);

	exit(ret);
}


int run(void)
{
	int ret = 0;
	rb_helper_run(proc.helper);
	for(; !ret; ret = interrupted())
		rb_select(-1);

	return ret;
}


static
void init_rb(void)
{
	rb_set_time();
	rb_init_prng(NULL, RB_PRNG_DEFAULT);
	// rb_linebuf_init(LINEBUF_HEAP_SIZE);   // rb_helper_child()
}


static
void init_log(void)
{
	logFileName = LPATH;
	init_main_logfile();
}


#ifndef _WIN32
static
void init_signals(void)
{
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGPIPE);

	sigset_t mask;
	sigfillset(&mask);
	struct sigaction sa =
	{
		.sa_handler = handle_sig,
		.sa_mask = mask,
	};
	for(int i = 1; i < 32; ++i)
		if(sigismember(&sigs, i))
			if(sigaction(i, &sa, NULL) == -1)
				log_strerror_fatal("sigaction");
}
#else
static
void init_signals(void)
{
}
#endif


static
void handle_die(const char *const buf)
{
	if(proc.ctrl.handle_event)
		if(proc.ctrl.handle_event(PROC_DIE, buf, strlen(buf)))
			return;

	hlog(L_MAIN, SNO_GENERAL, "ratbox die: %s", buf);
	exit(0);
}


static
void handle_restart(const char *const buf)
{
	if(proc.ctrl.handle_event)
		if(proc.ctrl.handle_event(PROC_RESTART, buf, strlen(buf)))
			return;

	hlog(L_MAIN, SNO_GENERAL, "ratbox restart: %s", buf);
	exit(0);
}


static
void handle_log(const char *const buf)
{
	if(proc.ctrl.handle_event)
		if(proc.ctrl.handle_event(PROC_LOG, buf, strlen(buf)))
			return;

	hlog(L_MAIN, SNO_GENERAL, "ratbox: %s", buf);
}


static
void handle_error(rb_helper *const helper)
{
	if(proc.ctrl.handle_event)
		if(proc.ctrl.handle_event(PROC_ERROR, NULL, 0))
			return;

	hlog(L_MAIN, SNO_GENERAL, "ratbox error (ircd died?)");
	exit(1);
}


static
void handle_read(rb_helper *const helper)
{
	char buf[BUFSIZE];
	const int size = rb_helper_read(helper, buf, sizeof(buf));

	if(proc.ctrl.handle_event)
		if(proc.ctrl.handle_event(PROC_READ, buf, size))
			return;

	char *text;
	const int64_t id = strtol(buf, &text, 10);
	ssize_t text_size = text - buf;
	if(rb_unlikely(text == buf))
	{
		hlog(L_MAIN, SNO_GENERAL, "Ignoring something without an ID prefix [%s]", buf);
		return;
	}

	if(rb_unlikely(!text || !id || id == LONG_MIN || id == LONG_MAX || text_size <= 0))
	{
		hlog(L_MAIN, SNO_GENERAL, "Got id[%ld] and payload is invalid [%s]", id, buf);
		return;
	}

	// Skip the space after the id
	if(rb_likely(text[0] == ' '))
	{
		++text;
		--text_size;
		assert(text_size >= 0);
	}

	++requests;
	if(proc.ctrl.handle_request)
		proc.ctrl.handle_request(id, text, text_size);
}


#ifndef _WIN32
static
int interrupted(void)
{
	int ret = 0;
	if(rb_likely(!interruption))
		return ret;

	sigset_t ours, theirs;
	sigfillset(&ours);
	if(sigprocmask(SIG_BLOCK, &ours, &theirs) == -1)
		log_strerror_fatal("sigprocmask");

	if(proc.ctrl.handle_event)
		for(int i = 1; i < 32; ++i)
			if(signaled[i])
				proc.ctrl.handle_event(PROC_SIGNAL, &i, sizeof(i));

	for(int i = 1; i < 32; ++i)
		if(sigismember(&sigs, i) && signaled[i])
		{
			signaled[i] = false;
			ret = i;
			break;
		}

	--interruption;
	if(sigprocmask(SIG_SETMASK, &theirs, NULL) == -1)
		log_strerror_fatal("sigprocmask");

	return ret;
}
#else
static
int interrupted(void)
{
	return 0;
}
#endif


#ifndef _WIN32
static
void handle_sig(int signo)
{
	assert(signo < 32);
	switch(signo)
	{
		case SIGINT:
		case SIGTERM:
		case SIGPIPE:
			++interruption;
			break;

		default:
			break;
	}

	signaled[signo] = true;
}
#else
static
void handle_sig(int signo)
{
}
#endif


static
void log_strerror_fatal(const char *const prefix)
{
	log_strerror(prefix);
	exit(-1);
}


static
void log_strerror(const char *const prefix)
{
	hlog(L_MAIN, SNO_GENERAL, "%s: (%d): %s",
		 prefix,
		 errno,
		 strerror(errno));
}


//TODO: All of these could benefit from an rb_helper iface redux
void replyf(const int64_t id,
            const char *const fmt,
            ...)
{
	va_list ap;
	va_start(ap, fmt);
	vreply(id, fmt, ap);
	va_end(ap);
}


void vreply(const int64_t id,
            const char *const fmt,
            va_list ap)
{
	char buf[BUFSIZE];
	const ssize_t size = snprintf(buf, sizeof(buf), "%ld ", id);
	vsnprintf(buf + size, sizeof(buf) - size, fmt, ap);
	reply_raw(buf);
}


void reply(const int64_t id,
           const char *const msg)
{
	char buf[BUFSIZE];
	snprintf(buf, sizeof(buf), "%ld %s", id, msg);
	reply_raw(buf);
}


void reply_raw(const char *msg)
{
	rb_helper_write(proc.helper, msg);
	++replies;
}


uint64_t reply_count(void)
{
	return replies;
}


uint64_t request_count(void)
{
	return requests;
}
