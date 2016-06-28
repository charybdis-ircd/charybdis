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

#ifndef INCLUDED_proc_module_h
#define INCLUDED_proc_module_h

#include "stdinc.h"
#include "snomask.h"
#include "logger.h"
#include "send.h"
#include "msg.h"
#include "proc.h"
#include "modules.h"


extern proc_t proc;
extern volatile bool signaled[32];


static void hlog(const unsigned int lfile, const unsigned int snomask, const char *fmt, ...) AFP(3, 4);
uint64_t request_count(void);
uint64_t reply_count(void);

void reply_raw(const char *msg);
void reply(const int64_t id, const char *msg);
void vreply(const int64_t id, const char *fmt, va_list ap);
void replyf(const int64_t id, const char *fmt, ...) AFP(2, 3);

int run(void);  // returns interrupting signal number as reason for ret.

// MUST BE DEFINED:
int main(int argc, char **argv);


// static to be callable from the parent and the child!
// (parent is not linked with proc_module.o)
static
void hlog(const ilogfile ilf,
          const unsigned int snomask,
          const char *const fmt,
          ...)
//TODO: snomask from helper
{
    va_list ap;
    va_start(ap, fmt);

    char buf[BUFSIZE];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    ilog(L_MAIN, "Process %s(%d): %s",
         proc.ctrl.log_prefix?: "",
         getpid(),
         buf);

    va_end(ap);
}


#endif // INCLUDED_proc_module_h
