/*
 * Copyright (c) 2003-2006 Marcel Moolenaar
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _PTHREAD_MD_H_
#define	_PTHREAD_MD_H_

#include <sys/kse.h>
#include <stddef.h>
#include <ucontext.h>

#define	KSE_STACKSIZE		16384
#define	DTV_OFFSET		offsetof(struct tcb, tcb_tp.tp_dtv)

#define	THR_GETCONTEXT(ucp)	_ia64_save_context(&(ucp)->uc_mcontext)
#define	THR_SETCONTEXT(ucp)	PANIC("THR_SETCONTEXT() now in use!\n")

#define	PER_THREAD

struct kcb;
struct kse;
struct pthread;
struct tcb;

/*
 * tp points to one of these. We define the TLS structure as a union
 * containing a long double to enforce 16-byte alignment. This makes
 * sure that there will not be any padding in struct tcb after the
 * TLS structure.
 */
union ia64_tp {
	void			*tp_dtv;
	long double		_align_;
};

struct tcb {
	struct kse_thr_mailbox	tcb_tmbx;
	struct pthread		*tcb_thread;
	struct kcb		*tcb_curkcb;
	long			tcb_isfake;
	union ia64_tp		tcb_tp;
};

struct kcb {
	struct kse_mailbox	kcb_kmbx;
	struct kse		*kcb_kse;
	struct tcb		*kcb_curtcb;
	struct tcb		kcb_faketcb;
};

static __inline struct tcb *
ia64_get_tcb(void)
{
	register char *tp __asm("%r13");

	return ((struct tcb *)(tp - offsetof(struct tcb, tcb_tp)));
}

static __inline void
ia64_set_tcb(struct tcb *tcb)
{
	register char *tp __asm("%r13");

	__asm __volatile("mov %0 = %1;;" : "=r"(tp) : "r"(&tcb->tcb_tp));
}

/*
 * The kcb and tcb constructors.
 */
struct tcb	*_tcb_ctor(struct pthread *, int);
void		_tcb_dtor(struct tcb *);
struct kcb	*_kcb_ctor(struct kse *kse);
void		_kcb_dtor(struct kcb *);

/* Called from the KSE to set its private data. */
static __inline void
_kcb_set(struct kcb *kcb)
{
	/* There is no thread yet; use the fake tcb. */
	ia64_set_tcb(&kcb->kcb_faketcb);
}

/*
 * Get the current kcb.
 *
 * This can only be called while in a critical region; don't
 * worry about having the kcb changed out from under us.
 */
static __inline struct kcb *
_kcb_get(void)
{
	return (ia64_get_tcb()->tcb_curkcb);
}

/*
 * Enter a critical region.
 *
 * Read and clear km_curthread in the kse mailbox.
 */
static __inline struct kse_thr_mailbox *
_kcb_critical_enter(void)
{
	struct tcb *tcb;
	struct kse_thr_mailbox *crit;
	uint32_t flags;

	tcb = ia64_get_tcb();
	if (tcb->tcb_isfake != 0) {
		/*
		 * We already are in a critical region since
		 * there is no current thread.
		 */
		crit = NULL;
	} else {
		flags = tcb->tcb_tmbx.tm_flags;
		tcb->tcb_tmbx.tm_flags |= TMF_NOUPCALL;
		crit = tcb->tcb_curkcb->kcb_kmbx.km_curthread;
		tcb->tcb_curkcb->kcb_kmbx.km_curthread = NULL;
		tcb->tcb_tmbx.tm_flags = flags;
	}
	return (crit);
}

static __inline void
_kcb_critical_leave(struct kse_thr_mailbox *crit)
{
	struct tcb *tcb;

	tcb = ia64_get_tcb();
	/* No need to do anything if this is a fake tcb. */
	if (tcb->tcb_isfake == 0)
		tcb->tcb_curkcb->kcb_kmbx.km_curthread = crit;
}

static __inline int
_kcb_in_critical(void)
{
	struct tcb *tcb;
	uint32_t flags;
	int ret;

	tcb = ia64_get_tcb();
	if (tcb->tcb_isfake != 0) {
		/*
		 * We are in a critical region since there is no
		 * current thread.
		 */
		ret = 1;
	} else {
		flags = tcb->tcb_tmbx.tm_flags;
		tcb->tcb_tmbx.tm_flags |= TMF_NOUPCALL;
		ret = (tcb->tcb_curkcb->kcb_kmbx.km_curthread == NULL);
		tcb->tcb_tmbx.tm_flags = flags;
	}
	return (ret);
}

static __inline void
_tcb_set(struct kcb *kcb, struct tcb *tcb)
{
	if (tcb == NULL)
		tcb = &kcb->kcb_faketcb;
	kcb->kcb_curtcb = tcb;
	tcb->tcb_curkcb = kcb;
	ia64_set_tcb(tcb);
}

static __inline struct tcb *
_tcb_get(void)
{
	return (ia64_get_tcb());
}

static __inline struct pthread *
_get_curthread(void)
{
	return (ia64_get_tcb()->tcb_thread);
}

/*
 * Get the current kse.
 *
 * Like _kcb_get(), this can only be called while in a critical region.
 */
static __inline struct kse *
_get_curkse(void)
{
	return (ia64_get_tcb()->tcb_curkcb->kcb_kse);
}

void _ia64_break_setcontext(mcontext_t *mc);
void _ia64_enter_uts(kse_func_t uts, struct kse_mailbox *km, void *stack,
    size_t stacksz);
int _ia64_restore_context(mcontext_t *mc, intptr_t val, intptr_t *loc);
int _ia64_save_context(mcontext_t *mc);

static __inline int
_thread_enter_uts(struct tcb *tcb, struct kcb *kcb)
{
	if (_ia64_save_context(&tcb->tcb_tmbx.tm_context.uc_mcontext) == 0) {
		/* Make the fake tcb the current thread. */
		kcb->kcb_curtcb = &kcb->kcb_faketcb;
		ia64_set_tcb(&kcb->kcb_faketcb);
		_ia64_enter_uts(kcb->kcb_kmbx.km_func, &kcb->kcb_kmbx,
		    kcb->kcb_kmbx.km_stack.ss_sp,
		    kcb->kcb_kmbx.km_stack.ss_size);
		/* We should not reach here. */
		return (-1);
	}
	return (0);
}

static __inline int
_thread_switch(struct kcb *kcb, struct tcb *tcb, int setmbox)
{
	mcontext_t *mc;

	_tcb_set(kcb, tcb);
	mc = &tcb->tcb_tmbx.tm_context.uc_mcontext;
	if (mc->mc_flags & _MC_FLAGS_ASYNC_CONTEXT) {
		if (setmbox) {
			mc->mc_flags |= _MC_FLAGS_KSE_SET_MBOX;
			mc->mc_special.ifa =
			    (intptr_t)&kcb->kcb_kmbx.km_curthread;
			mc->mc_special.isr = (intptr_t)&tcb->tcb_tmbx;
		}
		_ia64_break_setcontext(mc);
	} else if (mc->mc_flags & _MC_FLAGS_SYSCALL_CONTEXT) {
		if (setmbox)
			kse_switchin(&tcb->tcb_tmbx, KSE_SWITCHIN_SETTMBX);
		else
			kse_switchin(&tcb->tcb_tmbx, 0);
	} else {
		if (setmbox)
			_ia64_restore_context(mc, (intptr_t)&tcb->tcb_tmbx,
			    (intptr_t *)&kcb->kcb_kmbx.km_curthread);
		else
			_ia64_restore_context(mc, 0, NULL);
	}
	/* We should not reach here. */
	return (-1);
}

#endif /* _PTHREAD_MD_H_ */
