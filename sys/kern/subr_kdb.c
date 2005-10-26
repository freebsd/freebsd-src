/*-
 * Copyright (c) 2004 The FreeBSD Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kdb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/kdb.h>
#include <machine/pcb.h>

#ifdef SMP
#if defined (__i386__) || defined(__amd64__)
#define	HAVE_STOPPEDPCBS
#include <machine/smp.h>
#endif
#endif

int kdb_active = 0;
void *kdb_jmpbufp = NULL;
struct kdb_dbbe *kdb_dbbe = NULL;
struct pcb kdb_pcb;
struct pcb *kdb_thrctx = NULL;
struct thread *kdb_thread = NULL;
struct trapframe *kdb_frame = NULL;

KDB_BACKEND(null, NULL, NULL, NULL);
SET_DECLARE(kdb_dbbe_set, struct kdb_dbbe);

static int kdb_sysctl_available(SYSCTL_HANDLER_ARGS);
static int kdb_sysctl_current(SYSCTL_HANDLER_ARGS);
static int kdb_sysctl_enter(SYSCTL_HANDLER_ARGS);
static int kdb_sysctl_panic(SYSCTL_HANDLER_ARGS);
static int kdb_sysctl_trap(SYSCTL_HANDLER_ARGS);

SYSCTL_NODE(_debug, OID_AUTO, kdb, CTLFLAG_RW, NULL, "KDB nodes");

SYSCTL_PROC(_debug_kdb, OID_AUTO, available, CTLTYPE_STRING | CTLFLAG_RD, 0, 0,
    kdb_sysctl_available, "A", "list of available KDB backends");

SYSCTL_PROC(_debug_kdb, OID_AUTO, current, CTLTYPE_STRING | CTLFLAG_RW, 0, 0,
    kdb_sysctl_current, "A", "currently selected KDB backend");

SYSCTL_PROC(_debug_kdb, OID_AUTO, enter, CTLTYPE_INT | CTLFLAG_RW, 0, 0,
    kdb_sysctl_enter, "I", "set to enter the debugger");

SYSCTL_PROC(_debug_kdb, OID_AUTO, panic, CTLTYPE_INT | CTLFLAG_RW, 0, 0,
    kdb_sysctl_panic, "I", "set to panic the kernel");

SYSCTL_PROC(_debug_kdb, OID_AUTO, trap, CTLTYPE_INT | CTLFLAG_RW, 0, 0,
    kdb_sysctl_trap, "I", "set cause a page fault");

/*
 * Flag indicating whether or not to IPI the other CPUs to stop them on
 * entering the debugger.  Sometimes, this will result in a deadlock as
 * stop_cpus() waits for the other cpus to stop, so we allow it to be
 * disabled.
 */
#ifdef SMP
static int kdb_stop_cpus = 1;
SYSCTL_INT(_debug_kdb, OID_AUTO, stop_cpus, CTLTYPE_INT | CTLFLAG_RW,
    &kdb_stop_cpus, 0, "");
TUNABLE_INT("debug.kdb.stop_cpus", &kdb_stop_cpus);
#endif

static int
kdb_sysctl_available(SYSCTL_HANDLER_ARGS)
{
	struct kdb_dbbe *be, **iter;
	char *avail, *p;
	ssize_t len, sz;
	int error;

	sz = 0;
	SET_FOREACH(iter, kdb_dbbe_set) {
		be = *iter;
		if (be->dbbe_active == 0)
			sz += strlen(be->dbbe_name) + 1;
	}
	sz++;
	avail = malloc(sz, M_TEMP, M_WAITOK);
	p = avail;
	*p = '\0';

	SET_FOREACH(iter, kdb_dbbe_set) {
		be = *iter;
		if (be->dbbe_active == 0) {
			len = snprintf(p, sz, "%s ", be->dbbe_name);
			p += len;
			sz -= len;
		}
	}
	KASSERT(sz >= 0, ("%s", __func__));
	error = sysctl_handle_string(oidp, avail, 0, req);
	free(avail, M_TEMP);
	return (error);
}

static int
kdb_sysctl_current(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	int error;

	if (kdb_dbbe != NULL) {
		strncpy(buf, kdb_dbbe->dbbe_name, sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
	} else
		*buf = '\0';
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (kdb_active)
		return (EBUSY);
	return (kdb_dbbe_select(buf));
}

static int
kdb_sysctl_enter(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	error = sysctl_wire_old_buffer(req, sizeof(int));
	if (error == 0) {
		i = 0;
		error = sysctl_handle_int(oidp, &i, 0, req);
	}
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (kdb_active)
		return (EBUSY);
	kdb_enter("sysctl debug.kdb.enter");
	return (0);
}

static int
kdb_sysctl_panic(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	error = sysctl_wire_old_buffer(req, sizeof(int));
	if (error == 0) {
		i = 0;
		error = sysctl_handle_int(oidp, &i, 0, req);
	}
	if (error != 0 || req->newptr == NULL)
		return (error);
	panic("kdb_sysctl_panic");
	return (0);
}

static int
kdb_sysctl_trap(SYSCTL_HANDLER_ARGS)
{
	int error, i;
	int *addr = (int *)0x10;

	error = sysctl_wire_old_buffer(req, sizeof(int));
	if (error == 0) {
		i = 0;
		error = sysctl_handle_int(oidp, &i, 0, req);
	}
	if (error != 0 || req->newptr == NULL)
		return (error);
	return (*addr);
}

/*
 * Solaris implements a new BREAK which is initiated by a character sequence
 * CR ~ ^b which is similar to a familiar pattern used on Sun servers by the
 * Remote Console.
 *
 * Note that this function may be called from almost anywhere, with interrupts
 * disabled and with unknown locks held, so it must not access data other than
 * its arguments.  Its up to the caller to ensure that the state variable is
 * consistent.
 */

#define	KEY_CR		13	/* CR '\r' */
#define	KEY_TILDE	126	/* ~ */
#define	KEY_CRTLB	2	/* ^B */

int
kdb_alt_break(int key, int *state)
{
	int brk;

	brk = 0;
	switch (key) {
	case KEY_CR:
		*state = KEY_TILDE;
		break;
	case KEY_TILDE:
		*state = (*state == KEY_TILDE) ? KEY_CRTLB : 0;
		break;
	case KEY_CRTLB:
		if (*state == KEY_CRTLB)
			brk = 1;
		/* FALLTHROUGH */
	default:
		*state = 0;
		break;
	}
	return (brk);
}

/*
 * Print a backtrace of the calling thread. The backtrace is generated by
 * the selected debugger, provided it supports backtraces. If no debugger
 * is selected or the current debugger does not support backtraces, this
 * function silently returns.
 */

void
kdb_backtrace()
{

	if (kdb_dbbe != NULL && kdb_dbbe->dbbe_trace != NULL) {
		printf("KDB: stack backtrace:\n");
		kdb_dbbe->dbbe_trace();
	}
}

/*
 * Set/change the current backend.
 */

int
kdb_dbbe_select(const char *name)
{
	struct kdb_dbbe *be, **iter;

	SET_FOREACH(iter, kdb_dbbe_set) {
		be = *iter;
		if (be->dbbe_active == 0 && strcmp(be->dbbe_name, name) == 0) {
			kdb_dbbe = be;
			return (0);
		}
	}
	return (EINVAL);
}

/*
 * Enter the currently selected debugger. If a message has been provided,
 * it is printed first. If the debugger does not support the enter method,
 * it is entered by using breakpoint(), which enters the debugger through
 * kdb_trap().
 */

void
kdb_enter(const char *msg)
{

	if (kdb_dbbe != NULL && kdb_active == 0) {
		if (msg != NULL)
			printf("KDB: enter: %s\n", msg);
		breakpoint();
	}
}

/*
 * Initialize the kernel debugger interface.
 */

void
kdb_init()
{
	struct kdb_dbbe *be, **iter;
	int cur_pri, pri;

	kdb_active = 0;
	kdb_dbbe = NULL;
	cur_pri = -1;
	SET_FOREACH(iter, kdb_dbbe_set) {
		be = *iter;
		pri = (be->dbbe_init != NULL) ? be->dbbe_init() : -1;
		be->dbbe_active = (pri >= 0) ? 0 : -1;
		if (pri > cur_pri) {
			cur_pri = pri;
			kdb_dbbe = be;
		}
	}
	if (kdb_dbbe != NULL) {
		printf("KDB: debugger backends:");
		SET_FOREACH(iter, kdb_dbbe_set) {
			be = *iter;
			if (be->dbbe_active == 0)
				printf(" %s", be->dbbe_name);
		}
		printf("\n");
		printf("KDB: current backend: %s\n",
		    kdb_dbbe->dbbe_name);
	}
}

/*
 * Handle contexts.
 */

void *
kdb_jmpbuf(jmp_buf new)
{
	void *old;

	old = kdb_jmpbufp;
	kdb_jmpbufp = new;
	return (old);
}

void
kdb_reenter(void)
{

	if (!kdb_active || kdb_jmpbufp == NULL)
		return;

	longjmp(kdb_jmpbufp, 1);
	/* NOTREACHED */
}

/*
 * Thread related support functions.
 */

struct pcb *
kdb_thr_ctx(struct thread *thr)
{  
#ifdef HAVE_STOPPEDPCBS
	struct pcpu *pc;
	u_int cpuid;
#endif
  
	if (thr == curthread) 
		return (&kdb_pcb);

#ifdef HAVE_STOPPEDPCBS
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu)  {
		cpuid = pc->pc_cpuid;
		if (pc->pc_curthread == thr && (stopped_cpus & (1 << cpuid)))
			return (&stoppcbs[cpuid]);
	}
#endif
	return (thr->td_pcb);
}

struct thread *
kdb_thr_first(void)
{
	struct proc *p;
	struct thread *thr;

	p = LIST_FIRST(&allproc);
	while (p != NULL) {
		if (p->p_sflag & PS_INMEM) {
			thr = FIRST_THREAD_IN_PROC(p);
			if (thr != NULL)
				return (thr);
		}
		p = LIST_NEXT(p, p_list);
	}
	return (NULL);
}

struct thread *
kdb_thr_from_pid(pid_t pid)
{
	struct proc *p;

	p = LIST_FIRST(&allproc);
	while (p != NULL) {
		if (p->p_sflag & PS_INMEM && p->p_pid == pid)
			return (FIRST_THREAD_IN_PROC(p));
		p = LIST_NEXT(p, p_list);
	}
	return (NULL);
}

struct thread *
kdb_thr_lookup(lwpid_t tid)
{
	struct thread *thr;

	thr = kdb_thr_first();
	while (thr != NULL && thr->td_tid != tid)
		thr = kdb_thr_next(thr);
	return (thr);
}

struct thread *
kdb_thr_next(struct thread *thr)
{
	struct proc *p;

	p = thr->td_proc;
	thr = TAILQ_NEXT(thr, td_plist);
	do {
		if (thr != NULL)
			return (thr);
		p = LIST_NEXT(p, p_list);
		if (p != NULL && (p->p_sflag & PS_INMEM))
			thr = FIRST_THREAD_IN_PROC(p);
	} while (p != NULL);
	return (NULL);
}

int
kdb_thr_select(struct thread *thr)
{
	if (thr == NULL)
		return (EINVAL);
	kdb_thread = thr;
	kdb_thrctx = kdb_thr_ctx(thr);
	return (0);
}

/*
 * Enter the debugger due to a trap.
 */

int
kdb_trap(int type, int code, struct trapframe *tf)
{
#ifdef SMP
	int did_stop_cpus;
#endif
	int handled;

	if (kdb_dbbe == NULL || kdb_dbbe->dbbe_trap == NULL)
		return (0);

	/* We reenter the debugger through kdb_reenter(). */
	if (kdb_active)
		return (0);

	critical_enter();

	kdb_active++;

#ifdef SMP
	if ((did_stop_cpus = kdb_stop_cpus) != 0)
		stop_cpus(PCPU_GET(other_cpus));
#endif

	kdb_frame = tf;

	/* Let MD code do its thing first... */
	kdb_cpu_trap(type, code);

	makectx(tf, &kdb_pcb);
	kdb_thr_select(curthread);

	handled = kdb_dbbe->dbbe_trap(type, code);

#ifdef SMP
	if (did_stop_cpus)
		restart_cpus(stopped_cpus);
#endif

	kdb_active--;

	critical_exit();

	return (handled);
}
