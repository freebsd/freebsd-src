/*-
 * Copyright (c) 2003 Jake Burkholder <jake@freebsd.org>.
 * Copyright (c) 2003 Marcel Moolenaar
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

/*
 * Machine-dependent thread prototypes/definitions for the thread kernel.
 */
#ifndef _PTHREAD_MD_H_
#define	_PTHREAD_MD_H_

#include <sys/kse.h>
#include <stddef.h>
#include <ucontext.h>

#define	KSE_STACKSIZE		16384
#define	DTV_OFFSET              offsetof(struct tcb, tcb_tp.tp_tdv)

int _thr_setcontext(mcontext_t *, intptr_t, intptr_t *);
int _thr_getcontext(mcontext_t *);

#define	THR_GETCONTEXT(ucp)	_thr_getcontext(&(ucp)->uc_mcontext)
#define	THR_SETCONTEXT(ucp)	_thr_setcontext(&(ucp)->uc_mcontext, 0, NULL)

#define	PER_THREAD

struct kcb;
struct kse;
struct pthread;
struct tcb;
struct tdv;	/* We don't know what this is yet? */


/*
 * %r6 points to one of these. We define the static TLS as an array
 * of long double to enforce 16-byte alignment of the TLS memory.
 *
 * XXX - Both static and dynamic allocation of any of these structures
 *       will result in a valid, well-aligned thread pointer???
 */
struct arm_tp {
	struct tdv		*tp_tdv;	/* dynamic TLS */
};

struct tcb {
	struct pthread		*tcb_thread;
	struct kcb		*tcb_curkcb;
	uint32_t		tcb_isfake;
	struct kse_thr_mailbox	tcb_tmbx;	/* needs 32-byte alignment */
	struct arm_tp		tcb_tp;
};

struct kcb {
	struct kse_mailbox	kcb_kmbx;
	struct tcb		kcb_faketcb;
	struct tcb		*kcb_curtcb;
	struct kse		*kcb_kse;
};

extern struct arm_tp **arm_tp;
#define _tp (*arm_tp)

#define	_tcb	((struct tcb*)((char*)(_tp) - offsetof(struct tcb, tcb_tp)))

/*
 * The kcb and tcb constructors.
 */
struct tcb	*_tcb_ctor(struct pthread *, int);
void		_tcb_dtor(struct tcb *);
struct kcb	*_kcb_ctor(struct kse *kse);
void		_kcb_dtor(struct kcb *);

static __inline uint32_t
__kcb_swp(uint32_t val, void *ptr)
{       
	
	__asm __volatile("swp   %0, %1, [%2]"
	    : "=r" (val) : "r" (val) , "r" (ptr) : "memory");
    	return (val);
}

/* Called from the KSE to set its private data. */
static __inline void
_kcb_set(struct kcb *kcb)
{
	/* There is no thread yet; use the fake tcb. */
	__kcb_swp((uint32_t)&kcb->kcb_faketcb.tcb_tp, &_tp);
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
	return (_tcb->tcb_curkcb);
}

/*
 * Enter a critical region.
 *
 * Read and clear km_curthread in the kse mailbox.
 */
static __inline struct kse_thr_mailbox *
_kcb_critical_enter(void)
{
	struct kse_thr_mailbox *crit;

	if (_tcb->tcb_isfake)
		return (NULL);
	crit = (struct kse_thr_mailbox *)__kcb_swp((uint32_t)NULL, 
	    &_tcb->tcb_curkcb->kcb_kmbx.km_curthread);
	return (crit);
}

static __inline void
_kcb_critical_leave(struct kse_thr_mailbox *crit)
{

	if (_tcb->tcb_isfake == 0)
		__kcb_swp((uint32_t)crit, 
		    &_tcb->tcb_curkcb->kcb_kmbx.km_curthread);
}

static __inline int
_kcb_in_critical(void)
{
	uint32_t flags;
	int ret;

	return (_tcb->tcb_curkcb->kcb_kmbx.km_curthread == NULL);
	if (_tcb->tcb_isfake != 0) {
		/*
		 * We are in a critical region since there is no
		 * current thread.
		 */
		ret = 1;
	} else {
		flags = _tcb->tcb_tmbx.tm_flags;
		_tcb->tcb_tmbx.tm_flags |= TMF_NOUPCALL;
		ret = (_tcb->tcb_curkcb->kcb_kmbx.km_curthread == NULL);
		_tcb->tcb_tmbx.tm_flags = flags;
	}
	return (ret);
}

static __inline void
_tcb_set(struct kcb *kcb, struct tcb *tcb)
{
	if (tcb == NULL) 
		tcb = &kcb->kcb_faketcb;
	__kcb_swp((uint32_t)&tcb->tcb_tp, &_tp);
	kcb->kcb_curtcb = tcb;
	tcb->tcb_curkcb = kcb;
}

static __inline struct tcb *
_tcb_get(void)
{
	return (_tcb);
}

static __inline struct pthread *
_get_curthread(void)
{
	return (_tcb->tcb_thread);
}

/*
 * Get the current kse.
 *
 * Like _kcb_get(), this can only be called while in a critical region.
 */
static __inline struct kse *
_get_curkse(void)
{
	return (_tcb->tcb_curkcb->kcb_kse);
}

void _arm_enter_uts(struct kse_mailbox *km, kse_func_t uts, void *stack,
    size_t stacksz);

static __inline int
_thread_enter_uts(struct tcb *tcb, struct kcb *kcb)
{
	int ret;
	
	if ((ret = _thr_getcontext(&tcb->tcb_tmbx.tm_context.uc_mcontext))
	    == 0) {
		kcb->kcb_curtcb = &kcb->kcb_faketcb;
		__kcb_swp((int)&kcb->kcb_faketcb.tcb_tp, &_tp);
		_arm_enter_uts(&kcb->kcb_kmbx, kcb->kcb_kmbx.km_func,
		    kcb->kcb_kmbx.km_stack.ss_sp,
		    kcb->kcb_kmbx.km_stack.ss_size);
		/* We should not reach here. */
		return (-1);
	} else if (ret < 0)
		return (-1);
	return (0);
}

static __inline int
_thread_switch(struct kcb *kcb, struct tcb *tcb, int setmbox)
{
	extern int _libkse_debug;
	mcontext_t *mc;

	if (!tcb || !kcb)
		return (-1);
	_tcb_set(kcb, tcb);
	mc = &tcb->tcb_tmbx.tm_context.uc_mcontext;
	if (_libkse_debug == 0) {
		tcb->tcb_tmbx.tm_lwp = kcb->kcb_kmbx.km_lwp;
		if (setmbox)
			_thr_setcontext(mc, (intptr_t)&tcb->tcb_tmbx,
				(intptr_t *)&kcb->kcb_kmbx.km_curthread);
		else
			_thr_setcontext(mc, 0, NULL);
	} else {
		if (setmbox)
			kse_switchin(&tcb->tcb_tmbx, KSE_SWITCHIN_SETTMBX);
		else
			kse_switchin(&tcb->tcb_tmbx, 0);
	}

	/* We should not reach here. */
	return (-1);
}

#endif /* _PTHREAD_MD_H_ */
