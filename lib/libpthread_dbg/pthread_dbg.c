/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <thr_private.h>
#include <sys/types.h>
#include <sys/kse.h>

#include "pthread_dbg_md.h"
#include "pthread_dbg.h"
#include "pthread_dbg_int.h"

static void td_empty_thread_list(td_proc_t *proc);
static int td_refresh_thread_list(td_proc_t *proc);
static int td_get_thread(td_proc_t *proc, caddr_t addr, int type, 
	td_thread_t **threadp);
static void td_remove_map(td_proc_t *proc, int type);

int
td_open(td_proc_callbacks_t *cb, void *arg, td_proc_t **procp)
{
#define LOOKUP_SYM(proc, sym, addr) 			\
	ret = LOOKUP(proc, sym, addr);			\
	if (ret != 0) {					\
		if (ret == TD_ERR_NOSYM)		\
			ret = TD_ERR_NOLIB;		\
		goto error;				\
	}

	td_proc_t *proc;
	int dbg;
	int ret;

	td_md_init();
	proc = malloc(sizeof(*proc));
	if (proc == NULL)
		return (TD_ERR_NOMEM);

	proc->cb = cb;
	proc->arg = arg;
	proc->thread_activated = 0;
	proc->thread_listgen = -1;
	TAILQ_INIT(&proc->threads);

	LOOKUP_SYM(proc, "_libkse_debug",	&proc->libkse_debug_addr);
	LOOKUP_SYM(proc, "_thread_list",	&proc->thread_list_addr);
	LOOKUP_SYM(proc, "_thread_listgen",	&proc->thread_listgen_addr);
	LOOKUP_SYM(proc, "_thread_activated",	&proc->thread_activated_addr);
/*	LOOKUP_SYM(proc, "_thread_active_kseq",	&proc->thread_active_kseq_addr);*/
	dbg = getpid();
	/*
	 * If this fails it probably means we're debugging a core file and
	 * can't write to it.
	 * If it's something else we'll lose the next time we hit WRITE,
	 * but not before, and that's OK.
	 */
	WRITE(proc, proc->libkse_debug_addr, &dbg, sizeof(int));

	*procp = proc;
	return (0);

error:
	free(proc);
	return (ret);
}

int
td_close(td_proc_t *proc)
{
	int dbg;

	td_empty_thread_list(proc);

	dbg = 0;
        /*
         * Error returns from this write are not really a problem;
         * the process doesn't exist any more.
         */
	WRITE(proc, proc->libkse_debug_addr, &dbg, sizeof(int));

	free(proc);
	return 0;
}

int
td_thr_iter(td_proc_t *proc, int (*call)(td_thread_t *, void *),
	    void *callarg)
{
	int ret;
	td_thread_t *thread;

	td_refresh_thread_list(proc);

	TAILQ_FOREACH(thread, &proc->threads, tle) {
		ret = (*call)(thread, callarg);
		if (ret != 0)
			return (ret);
	}
	return (0);
}

int
td_thr_info(td_thread_t *thread, td_thread_info_t *info)
{
	int ret = 0;
	struct pthread pt;

	memset(info, 0, sizeof(*info));
	if (thread->type == TD_TYPE_UPCALL) {
		info->thread_id = (long)thread->kthread;
		info->thread_state = TD_STATE_RUNNING;
		info->thread_type = TD_TYPE_UPCALL;
		return (ret);
	}

	/* Handle normal thread */
	ret = READ(thread->proc, thread->addr, &pt, sizeof(pt));
	if (ret != 0)
		return (ret);

	if (pt.magic != THR_MAGIC)
		return (TD_ERR_BADTHREAD);

	info->thread_id = (long)thread->addr;
	info->thread_addr = thread->addr;
	info->thread_type = TD_TYPE_NORMAL;
	switch (pt.state) {
	case PS_RUNNING:
		info->thread_state = TD_STATE_RUNNING;
		break;
	case PS_LOCKWAIT:
		info->thread_state = TD_STATE_LOCKWAIT;
		break;
	case PS_MUTEX_WAIT:
		info->thread_state = TD_STATE_MUTEXWAIT;
		break;
	case PS_COND_WAIT:
		info->thread_state = TD_STATE_SLEEPING;
		break;
	case PS_SIGSUSPEND:
		info->thread_state = TD_STATE_SIGSUSPEND;
		break;
	case PS_SIGWAIT:
		info->thread_state = TD_STATE_SIGWAIT;
		break;
	case PS_JOIN:
		info->thread_state = TD_STATE_JOIN;
		break;
	case PS_SUSPENDED:
		info->thread_state = TD_STATE_SUSPENDED;
		break;
	case PS_DEAD:
		info->thread_state = TD_STATE_DEAD;
		break;
	case PS_DEADLOCK:
		info->thread_state = TD_STATE_DEADLOCK;
		break;
	default:
		info->thread_state = TD_STATE_UNKNOWN;
		break;
	}
	
	info->thread_scope = (pt.attr.flags & PTHREAD_SCOPE_SYSTEM) ?
		TD_SCOPE_SYSTEM : TD_SCOPE_PROCESS;
	info->thread_stack.ss_sp = pt.attr.stackaddr_attr;
	info->thread_stack.ss_size = pt.attr.stacksize_attr;
	info->thread_joiner = (caddr_t)pt.joiner;
	info->thread_errno = pt.error;
	info->thread_sigmask = pt.sigmask;
	info->thread_sigpend = pt.sigpend;
	info->thread_sigstk = pt.sigstk;
	info->thread_base_priority = pt.base_priority;
	info->thread_inherited_priority = pt.inherited_priority;
	info->thread_active_priority = pt.active_priority;
	info->thread_cancelflags = pt.cancelflags;
	info->thread_retval = pt.join_status.ret;
	info->thread_tls = (caddr_t)pt.specific;
	info->thread_tlscount = pt.specific_data_count;
	return (0);
}

int
td_thr_getregs(td_thread_t *thread, int regset, void *buf)
{
	td_proc_t *proc = thread->proc;
	caddr_t tmbx_addr, ptr;
	struct kse_thr_mailbox tmbx;
	int ret;

	if (regset != 0 && regset != 1)
		return (TD_ERR_INVAL);

	if (thread->kthread != NULL)
		return GETREGS(proc, regset, thread->kthread, buf);

	tmbx_addr = thread->tcb_addr + offsetof(struct tcb, tcb_tmbx);
	ptr = tmbx_addr + offsetof(struct kse_thr_mailbox, tm_kthread);
	ret = READ(proc, ptr, &ptr, sizeof(ptr));
	if (ret != 0)
		return (ret);
	if (ptr != NULL)
		return GETREGS(proc, regset, ptr, buf);
	ret = READ(proc, tmbx_addr, &tmbx, sizeof(tmbx));
	if (ret != 0)
		return (ret);
	if (regset == 0)
		td_ucontext_to_reg(&tmbx.tm_context, (struct reg *)buf);
	else
		td_ucontext_to_fpreg(&tmbx.tm_context, (struct fpreg *)buf);
	return (0);
}

int
td_thr_setregs(td_thread_t *thread, int regset, void *buf)
{
	td_proc_t *proc = thread->proc;
	caddr_t tmbx_addr, ptr;
	struct kse_thr_mailbox tmbx;
	int ret;
	
	if (regset != 0 && regset != 1)
		return (TD_ERR_INVAL);

	if (thread->kthread != NULL)
		return SETREGS(proc, regset, thread->kthread, buf);

	tmbx_addr = thread->tcb_addr + offsetof(struct tcb, tcb_tmbx);
	ptr = tmbx_addr + offsetof(struct kse_thr_mailbox, tm_kthread);
	ret = READ(proc, ptr, &ptr, sizeof(ptr));
	if (ret != 0)
		return (ret);
	if (ptr != NULL)
		return SETREGS(proc, regset, ptr, buf);

	/*
	 * Read a copy of context, this makes sure that registers
	 * not covered by structure reg won't be clobbered
	 */
	ret = READ(proc, tmbx_addr, &tmbx, sizeof(tmbx));
	if (ret != 0)
		return (ret);
	if (regset == 0)
		td_reg_to_ucontext((struct reg *)buf, &tmbx.tm_context);
	else
		td_fpreg_to_ucontext((struct fpreg *)buf, &tmbx.tm_context);
	return (WRITE(proc, tmbx_addr, &tmbx, sizeof(tmbx)));
}

int
td_thr_getname(td_thread_t *thread, char *name, int len)
{
	int ret;
	caddr_t nameaddr;
	
	if ((ret = READ(thread->proc,
			thread->addr + offsetof(struct pthread, name),
			&nameaddr, sizeof(nameaddr))) != 0)
		return (ret);

	if (nameaddr == 0)
		name[0] = '\0';
	else if ((ret = READ_STRING(thread->proc, nameaddr,
				    name, len)) != 0)
		return (ret);

	return (0);
}

int
td_activated(td_proc_t *proc)
{
	if (proc->thread_activated)
		return (1);
	READ(proc, proc->thread_activated_addr, &proc->thread_activated,
	     sizeof(proc->thread_activated));
	return (proc->thread_activated != 0);
}

int
td_map_lwp2thr(td_proc_t *proc, void *lwp, td_thread_t **threadp)
{
	int ret;
	caddr_t next, ptr;
	td_thread_t *thread;
	TAILQ_HEAD(, pthread) thread_list;
	
	ret = READ(proc, proc->thread_list_addr, &thread_list,
		   sizeof(thread_list));
	if (ret != 0)
		return (ret);
	/*
	 * We have to iterate through thread list to find which
	 * userland thread is running on the kernel thread.
	 */
	next = (caddr_t)thread_list.tqh_first;
	while (next != NULL) {
		ret = READ(proc, next + offsetof(struct pthread, tcb),
			   &ptr, sizeof(ptr));
		if (ret != 0)
			return (ret);
		ptr += offsetof(struct tcb, tcb_tmbx.tm_kthread);
		ret = READ(proc, ptr, &ptr, sizeof(ptr));
		if (ret != 0)
			return (ret);
		if (ptr == lwp) {
			ret = td_get_thread(proc, next, TD_TYPE_NORMAL,
				 &thread);
			if (ret != 0)
				return (ret);
			*threadp = thread;
			return (0);
		}

		/* get next thread */
		ret = READ(proc,
			   next + offsetof(struct pthread, tle.tqe_next), 
			   &next, sizeof(next));
		if (ret != 0)
			return (ret);
	}
	return (TD_ERR_NOOBJ);
}

int
td_map_id2thr(td_proc_t *proc, long threadid, td_thread_t **threadp)
{
	td_thread_t *thread;

	td_refresh_thread_list(proc);
	
	TAILQ_FOREACH(thread, &proc->threads, tle) {
		if (thread->type == TD_TYPE_UPCALL) {
			if ((long)thread->kthread == threadid)
				break;
		} else if ((long)thread->addr == threadid)
			break;
	}
	if (thread) {
		*threadp = thread;
		return (0);
	}
	return (TD_ERR_NOOBJ);
}

int
td_thr_sstep(td_thread_t *thread, int step)
{
	int ret;
	uint32_t tmp;
	struct reg reg;
	td_proc_t *proc;
	caddr_t kthread;

	proc = thread->proc;
	if ((kthread=thread->kthread) != NULL)
		return SSTEP(proc, kthread, step);

	if (thread->step == step)
		return (0);

	/* Clear or set single step flag in thread mailbox */
	tmp = step ? TMDF_SSTEP : 0;
	ret = WRITE(proc, thread->tcb_addr + offsetof(struct tcb,
		   tcb_tmbx.tm_dflags), &tmp, sizeof(tmp));

	/* Get kthread */
	ret = READ(proc, thread->tcb_addr + offsetof(struct tcb,
	           tcb_tmbx.tm_kthread), &kthread, sizeof(kthread));
	if (ret != 0)
		return (ret);
	thread->step = step;
	if (kthread != NULL)
		return SSTEP(proc, kthread, step);

	/*
	 * context is in userland, some architectures store
	 * single step status in registers, we should change
	 * these registers.
	 */
	ret = td_thr_getregs(thread, 0, &reg);
	if (ret == 0) {
		/* only write out if it is really changed. */
		if (td_reg_sstep(&reg, step) != 0)
			ret = td_thr_setregs(thread, 0, &reg);
	}
	return (ret);
}

void
td_remove_lwp_map(td_proc_t *proc)
{
	td_remove_map(proc, TD_TYPE_UPCALL);
}

static void
td_remove_map(td_proc_t *proc, int type)
{
	td_thread_t *thread, *next;

	for (thread = TAILQ_FIRST(&proc->threads); thread; thread = next) {
		next = TAILQ_NEXT(thread, tle);
		if (type == type)
			TAILQ_REMOVE(&proc->threads, thread, tle);
	}
}

static int
td_refresh_thread_list(td_proc_t *proc)
{
	int ret, gen;
	caddr_t next;
	td_thread_t *thread;
	TAILQ_HEAD(, pthread) thread_list;
	
	ret = READ(proc, proc->thread_listgen_addr, &gen, sizeof(gen));
	if (ret != 0)
		return (ret);
	if (gen == proc->thread_listgen)
		return (TD_ERR_OK);
	proc->thread_listgen = gen;

	td_remove_map(proc, TD_TYPE_NORMAL);

	ret = READ(proc, proc->thread_list_addr, &thread_list,
		 sizeof(thread_list));
	if (ret != 0)
		return (ret);
	next = (caddr_t)thread_list.tqh_first;
	while (next != NULL) {
		ret = td_get_thread(proc, next, TD_TYPE_NORMAL, &thread);
		if (ret != 0)
			return (ret);
		ret = READ(proc,
			   next + offsetof(struct pthread, tle.tqe_next), 
			   &next, sizeof(next));
		if (ret != 0)
			return (ret);
	}
	return (0);
}

static void
td_empty_thread_list(td_proc_t *proc)
{
	td_thread_t *thread;

	while ((thread = TAILQ_FIRST(&proc->threads)) != NULL) {
		TAILQ_REMOVE(&proc->threads, thread, tle);
		free(thread);
	}
}

static int
td_get_thread(td_proc_t *proc, caddr_t addr, int type, td_thread_t **threadp)
{
	td_thread_t *thread;
	caddr_t tcb_addr;
	int ret;

	TAILQ_FOREACH(thread, &proc->threads, tle) {
		if (type == TD_TYPE_UPCALL) {
			/* match upcall thread */
			if (thread->kthread == addr)
				break;
		} else {
			/* match normal thread */
			if (thread->addr == addr)
				break;
		}
	}

	if (thread == NULL) {
		tcb_addr = NULL;
		if (type != TD_TYPE_UPCALL) {
			ret = READ(proc, addr + offsetof(struct pthread, tcb),
			     &tcb_addr,	sizeof(tcb_addr));
			if (ret)
				return (ret);
		}
		thread = malloc(sizeof(*thread));
		if (thread == NULL)
			return (TD_ERR_NOMEM);
		thread->proc = proc;
		thread->step = -1;
		thread->tcb_addr = tcb_addr;
		if (type == TD_TYPE_NORMAL) {
			thread->addr = addr;
			thread->kthread = NULL;
		} else {
			thread->addr = NULL;
			thread->kthread = addr;
		}
		thread->type = type;
		TAILQ_INSERT_TAIL(&proc->threads, thread, tle);
	}
	*threadp = thread;
	return (0);
}

struct string_map {
	int num;
	char *str;
};

char *
td_err_string (int errcode)
{
	static struct string_map err_table[] = {
	 {TD_ERR_OK, "generic \"call succeeded\""},
	 {TD_ERR_ERR, "generic error."},
	 {TD_ERR_NOSYM, "symbol not found"},
	 {TD_ERR_NOOBJ, "no object can be found to satisfy query"},
	 {TD_ERR_BADTHREAD, "thread can not answer request"},
	 {TD_ERR_INUSE, "debugging interface already in use for this process"},
	 {TD_ERR_NOLIB, "process is not using libpthread"},
	 {TD_ERR_NOMEM, "out of memory"},
	 {TD_ERR_IO, "process callback error"},
	 {TD_ERR_INVAL, "invalid argument"},
	};

	const int err_size = sizeof(err_table) / sizeof (struct string_map);
	int i;
	static char buf[90];

	for (i = 0; i < err_size; i++)
		if (err_table[i].num == errcode)
			return err_table[i].str;

	sprintf (buf, "Unknown thread library debug error code: %d", errcode);

	return buf;
}
