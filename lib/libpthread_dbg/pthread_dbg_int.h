/*
 * Copyright (c) 2004 David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _PTHREAD_DBG_INT_H
#define _PTHREAD_DBG_INT_H

#include <sys/queue.h>
#include <sys/ucontext.h>

struct td_proc {
	td_proc_callbacks_t		*cb;
	void				*arg;
	pid_t				pid;
	caddr_t				libkse_debug_addr;
	caddr_t				thread_list_addr;
	caddr_t				thread_listgen_addr;
	caddr_t				thread_activated_addr;
	caddr_t				thread_active_kseq_addr;
	int				thread_activated;
	int				thread_listgen;
	TAILQ_HEAD(, td_thread)		threads;
};

struct td_thread {
        td_proc_t	*proc;
        caddr_t		addr;
	caddr_t		tcb_addr;
        void		*kthread;
	int		type;
	int		step;
        TAILQ_ENTRY(td_thread) tle;
};

#define READ(proc, addr, buf, size) ((proc)->cb->proc_read((proc)->arg, (addr), (buf), (size)))
#define READ_STRING(proc, addr, buf, size) ((proc)->cb->proc_read((proc)->arg, (addr), (buf), (size)))
#define WRITE(proc, addr, buf, size) ((proc)->cb->proc_write((proc)->arg, (addr), (buf), (size)))
#define LOOKUP(proc, sym, addr) ((proc)->cb->proc_lookup((proc)->arg, (sym), (addr)))
#define GETREGS(proc, regset, lwp, buf) ((proc)->cb->proc_getregs((proc)->arg, (regset), (lwp), (buf)))
#define SETREGS(proc, regset, lwp, buf) ((proc)->cb->proc_setregs((proc)->arg, (regset), (lwp), (buf)))
#define SSTEP(proc, lwp, on) ((proc)->cb->proc_sstep((proc)->arg, (lwp), (on)))

#endif

