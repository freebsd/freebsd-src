/*
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

#ifndef _PTHREAD_MD_H_
#define	_PTHREAD_MD_H_

#include <sys/kse.h>
#include <ucontext.h>

#define	THR_GETCONTEXT(ucp)	_ia64_save_context(&(ucp)->uc_mcontext)
#define	THR_SETCONTEXT(ucp)	_ia64_restore_context(&(ucp)->uc_mcontext, \
				    0, NULL)

#define	PER_THREAD

struct kcb;
struct kse;
struct pthread;
struct tcb;
struct tdv;	/* We don't know what this is yet? */

/*
 * tp points to one of these. We define the static TLS as an array
 * of long double to enforce 16-byte alignment of the TLS memory,
 * struct ia64_tp, struct tcb and also struct kcb. Both static and
 * dynamic allocation of any of these structures will result in a
 * valid, well-aligned thread pointer.
 */
struct ia64_tp {
	struct tdv		*tp_tdv;	/* dynamic TLS */
	struct tcb		*tp_self;
	long double		tp_tls[0];	/* static TLS */
};

struct tcb {
	struct kse_thr_mailbox	tcb_tmbx;
	struct pthread		*tcb_thread;
	struct kcb		*tcb_curkcb;
	long			tcb_isfake;
	struct ia64_tp		tcb_tp;
};

struct kcb {
	struct kse_mailbox	kcb_kmbx;
	struct tcb		kcb_faketcb;
	struct tcb		*kcb_curtcb;
	struct kse		*kcb_kse;
};

register struct ia64_tp *_tp __asm("%r13");

/*
 * The kcb and tcb constructors.
 */
struct tcb	*_tcb_ctor(struct pthread *);
void		_tcb_dtor(struct tcb *);
struct kcb	*_kcb_ctor(struct kse *kse);
void		_kcb_dtor(struct kcb *);

/* Called from the KSE to set its private data. */
static __inline void
_kcb_set(struct kcb *kcb)
{
	/* There is no thread yet; use the fake tcb. */
	_tp = &kcb->kcb_faketcb.tcb_tp;
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
	return (_tp->tp_self->tcb_curkcb);
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

	tcb = _tp->tp_self;
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

	tcb = _tp->tp_self;
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

	tcb = _tp->tp_self;
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
	if (tcb == NULL) {
		kcb->kcb_curtcb = &kcb->kcb_faketcb;
		_tp = &kcb->kcb_faketcb.tcb_tp;
	}
	else {
		kcb->kcb_curtcb = tcb;
		tcb->tcb_curkcb = kcb;
		_tp = &tcb->tcb_tp;
	}
}

static __inline struct tcb *
_tcb_get(void)
{
	return (_tp->tp_self);
}

static __inline struct pthread *
_get_curthread(void)
{
	return (_tp->tp_self->tcb_thread);
}

/*
 * Get the current kse.
 *
 * Line _kcb_get(), this can only be called while in a critical region.
 */
static __inline struct kse *
_get_curkse(void)
{
	return (_tp->tp_self->tcb_curkcb->kcb_kse);
}

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
		_tp = &kcb->kcb_faketcb.tcb_tp;
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
	kcb->kcb_curtcb = tcb;
	tcb->tcb_curkcb = kcb;
	_tp = &tcb->tcb_tp;
	if (setmbox != 0)
		_ia64_restore_context(&tcb->tcb_tmbx.tm_context.uc_mcontext,
		    (intptr_t)&tcb->tcb_tmbx,
		    (intptr_t *)&kcb->kcb_kmbx.km_curthread);
	else
		_ia64_restore_context(&tcb->tcb_tmbx.tm_context.uc_mcontext,
		    0, NULL);
	/* We should not reach here. */
	return (-1);
}

#endif /* _PTHREAD_MD_H_ */
