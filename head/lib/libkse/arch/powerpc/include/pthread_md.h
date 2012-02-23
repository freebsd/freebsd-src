/*
 * Copyright 2004 by Peter Grehan.
 * Copyright 2006 Marcel Moolenaar
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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

extern void _ppc32_enter_uts(struct kse_mailbox *, kse_func_t, void *, size_t);
extern int  _ppc32_setcontext(mcontext_t *, intptr_t, intptr_t *);
extern int  _ppc32_getcontext(mcontext_t *);

#define	KSE_STACKSIZE		16384
#define	DTV_OFFSET		offsetof(struct tcb, tcb_tp.tp_dtv)

#define	THR_GETCONTEXT(ucp)	_ppc32_getcontext(&(ucp)->uc_mcontext)
#define	THR_SETCONTEXT(ucp)	_ppc32_setcontext(&(ucp)->uc_mcontext, 0, NULL)

#define	PER_THREAD

struct kcb;
struct kse;
struct pthread;
struct tcb;

/*
 * %r2 points to the following.
 */
struct ppc32_tp {
	void		*tp_dtv;	/* dynamic thread vector */
	uint32_t	_reserved_;
	double		tp_tls[0];	/* static TLS */
};

struct tcb {
	struct kse_thr_mailbox	tcb_tmbx;
	struct pthread		*tcb_thread;
	struct kcb		*tcb_curkcb;
	long			tcb_isfake;
	long			tcb_spare[3];
	struct ppc32_tp		tcb_tp;
};

struct kcb {
	struct kse_mailbox	kcb_kmbx;
	struct kse		*kcb_kse;
	struct tcb		*kcb_curtcb;
	struct tcb		kcb_faketcb;
};

/*
 * From the PowerPC32 TLS spec:
 *
 * "r2 is the thread pointer, and points 0x7000 past the end of the
 * thread control block." Or, 0x7008 past the start of the 8-byte tcb
 */
#define TP_OFFSET	0x7008

static __inline char *
ppc_get_tp(void)
{
	register char *r2 __asm__("%r2");

	return (r2 - TP_OFFSET);
}

static __inline void
ppc_set_tp(char *tp)
{
	register char *r2 __asm__("%r2");
	__asm __volatile("mr %0,%1" : "=r"(r2) : "r"(tp + TP_OFFSET));
}

static __inline struct tcb *
ppc_get_tcb(void)
{
	return ((struct tcb *)(ppc_get_tp() - offsetof(struct tcb, tcb_tp)));
}

static __inline void
ppc_set_tcb(struct tcb *tcb)
{
	ppc_set_tp((char*)&tcb->tcb_tp);
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
	ppc_set_tcb(&kcb->kcb_faketcb);
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
	return (ppc_get_tcb()->tcb_curkcb);
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
	struct tcb *tcb;
	uint32_t flags;

	tcb = ppc_get_tcb();
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

	tcb = ppc_get_tcb();

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

	tcb = ppc_get_tcb();
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
	ppc_set_tcb(tcb);
}

static __inline struct tcb *
_tcb_get(void)
{
	return (ppc_get_tcb());
}

static __inline struct pthread *
_get_curthread(void)
{
	return (ppc_get_tcb()->tcb_thread);
}

/*
 * Get the current kse.
 *
 * Like _kcb_get(), this can only be called while in a critical region.
 */
static __inline struct kse *
_get_curkse(void)
{
	return (ppc_get_tcb()->tcb_curkcb->kcb_kse);
}

static __inline int
_thread_enter_uts(struct tcb *tcb, struct kcb *kcb)
{
	if (_ppc32_getcontext(&tcb->tcb_tmbx.tm_context.uc_mcontext) == 0) {
		/* Make the fake tcb the current thread. */
		kcb->kcb_curtcb = &kcb->kcb_faketcb;
		ppc_set_tcb(&kcb->kcb_faketcb);
		_ppc32_enter_uts(&kcb->kcb_kmbx, kcb->kcb_kmbx.km_func,
		    kcb->kcb_kmbx.km_stack.ss_sp,
		    kcb->kcb_kmbx.km_stack.ss_size - 32);
		/* We should not reach here. */
		return (-1);
	}
	return (0);
}

static __inline int
_thread_switch(struct kcb *kcb, struct tcb *tcb, int setmbox)
{
	mcontext_t *mc;
	extern int _libkse_debug;

	_tcb_set(kcb, tcb);
	mc = &tcb->tcb_tmbx.tm_context.uc_mcontext;

	/*
	 * A full context needs a system call to restore, so use
	 * kse_switchin. Otherwise, the partial context can be
	 * restored with _ppc32_setcontext
	 */
	if (mc->mc_vers != _MC_VERSION_KSE && _libkse_debug != 0) {
		if (setmbox)
			kse_switchin(&tcb->tcb_tmbx, KSE_SWITCHIN_SETTMBX);
		else
			kse_switchin(&tcb->tcb_tmbx, 0);
	} else {
		tcb->tcb_tmbx.tm_lwp = kcb->kcb_kmbx.km_lwp;
		if (setmbox)
			_ppc32_setcontext(mc, (intptr_t)&tcb->tcb_tmbx,
			    (intptr_t *)(void *)&kcb->kcb_kmbx.km_curthread);
		else
			_ppc32_setcontext(mc, 0, NULL);
	}

	/* We should not reach here. */
	return (-1);
}

#endif /* _PTHREAD_MD_H_ */
