/*-
 * Copyright (c) 2002 Daniel Eischen <deischen@freebsd.org>.
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
/*
 * Machine-dependent thread prototypes/definitions for the thread kernel.
 */
#ifndef _PTHREAD_MD_H_
#define	_PTHREAD_MD_H_

#include <sys/kse.h>
#include <ucontext.h>

extern int _thr_setcontext(mcontext_t *, intptr_t, intptr_t *);
extern int _thr_getcontext(mcontext_t *);

#define	THR_GETCONTEXT(ucp)	_thr_getcontext(&(ucp)->uc_mcontext)
#define	THR_SETCONTEXT(ucp)	_thr_setcontext(&(ucp)->uc_mcontext, 0, NULL)

#define	PER_KSE
#undef	PER_THREAD

struct kse;
struct pthread;
struct tdv;

/*
 * %gs points to a struct kcb.
 */
struct kcb {
	struct tcb		*kcb_curtcb;
	struct kcb		*kcb_self;	/* self reference */
	int			kcb_ldt;
	struct kse		*kcb_kse;
	struct kse_mailbox	kcb_kmbx;
};

struct tcb {
	struct tdv		*tcb_tdv;
	struct pthread		*tcb_thread;
	void			*tcb_addr;	/* allocated tcb address */
	void			*tcb_spare;	/* align tcb_tmbx to 16 bytes */
	struct kse_thr_mailbox	tcb_tmbx;
};

/*
 * Evaluates to the byte offset of the per-kse variable name.
 */
#define	__kcb_offset(name)	__offsetof(struct kcb, name)

/*
 * Evaluates to the type of the per-kse variable name.
 */
#define	__kcb_type(name)	__typeof(((struct kcb *)0)->name)

/*
 * Evaluates to the value of the per-kse variable name.
 */
#define	KCB_GET32(name) ({					\
	__kcb_type(name) __result;				\
								\
	u_int __i;						\
	__asm __volatile("movl %%gs:%1, %0"			\
	    : "=r" (__i)					\
	    : "m" (*(u_int *)(__kcb_offset(name))));		\
	__result = *(__kcb_type(name) *)&__i;			\
								\
	__result;						\
})

/*
 * Sets the value of the per-kse variable name to value val.
 */
#define	KCB_SET32(name, val) ({					\
	__kcb_type(name) __val = (val);				\
								\
	u_int __i;						\
	__i = *(u_int *)&__val;					\
	__asm __volatile("movl %1,%%gs:%0"			\
	    : "=m" (*(u_int *)(__kcb_offset(name)))		\
	    : "r" (__i));					\
})

static __inline u_long
__kcb_readandclear32(volatile u_long *addr)
{
	u_long result;

	__asm __volatile (
	    "	xorl	%0, %0;"
	    "	xchgl	%%gs:%1, %0;"
	    "# __kcb_readandclear32"
	    : "=&r" (result)
	    : "m" (*addr));
	return (result);
}

#define	KCB_READANDCLEAR32(name) ({				\
	__kcb_type(name) __result;				\
								\
	__result = (__kcb_type(name))				\
	    __kcb_readandclear32((u_long *)__kcb_offset(name)); \
	__result;						\
})


#define	_kcb_curkcb()		KCB_GET32(kcb_self)
#define	_kcb_curtcb()		KCB_GET32(kcb_curtcb)
#define	_kcb_curkse()		((struct kse *)KCB_GET32(kcb_kmbx.km_udata))
#define	_kcb_get_tmbx()		KCB_GET32(kcb_kmbx.km_curthread)
#define	_kcb_set_tmbx(value)	KCB_SET32(kcb_kmbx.km_curthread, (void *)value)
#define	_kcb_readandclear_tmbx() KCB_READANDCLEAR32(kcb_kmbx.km_curthread)


/*
 * The constructors.
 */
struct tcb	*_tcb_ctor(struct pthread *);
void		_tcb_dtor(struct tcb *tcb);
struct kcb	*_kcb_ctor(struct kse *);
void		_kcb_dtor(struct kcb *);

/* Called from the KSE to set its private data. */
static __inline void
_kcb_set(struct kcb *kcb)
{
	int val;

	val = (kcb->kcb_ldt << 3) | 7;
	__asm __volatile("movl %0, %%gs" : : "r" (val));
}

/* Get the current kcb. */
static __inline struct kcb *
_kcb_get(void)
{
	return (_kcb_curkcb());
}

static __inline struct kse_thr_mailbox *
_kcb_critical_enter(void)
{
	struct kse_thr_mailbox *crit;

	crit = _kcb_readandclear_tmbx();
	return (crit);
}

static __inline void
_kcb_critical_leave(struct kse_thr_mailbox *crit)
{
	_kcb_set_tmbx(crit);
}

static __inline int
_kcb_in_critical(void)
{
	return (_kcb_get_tmbx() == NULL);
}

static __inline void
_tcb_set(struct kcb *kcb, struct tcb *tcb)
{
	kcb->kcb_curtcb = tcb;
}

static __inline struct tcb *
_tcb_get(void)
{
	return (_kcb_curtcb());
}

static __inline struct pthread *
_get_curthread(void)
{
	struct tcb *tcb;

	tcb = _kcb_curtcb();
	if (tcb != NULL)
		return (tcb->tcb_thread);
	else
		return (NULL);
}

static __inline struct kse *
_get_curkse(void)
{
	return ((struct kse *)_kcb_curkse());
}

void	_i386_enter_uts(struct kse_mailbox *km, kse_func_t uts, void *stack,
    size_t stacksz);

static __inline int
_thread_enter_uts(struct tcb *tcb, struct kcb *kcb)
{
	int ret;

	ret = _thr_getcontext(&tcb->tcb_tmbx.tm_context.uc_mcontext);
	if (ret == 0) {
		_i386_enter_uts(&kcb->kcb_kmbx, kcb->kcb_kmbx.km_func,
		    kcb->kcb_kmbx.km_stack.ss_sp,
		    kcb->kcb_kmbx.km_stack.ss_size);
		/* We should not reach here. */
		return (-1);
	}
	else if (ret < 0)
		return (-1);
	return (0);
}

static __inline int
_thread_switch(struct kcb *kcb, struct tcb *tcb, int setmbox)
{
	if ((kcb == NULL) || (tcb == NULL))
		return (-1);
	kcb->kcb_curtcb = tcb;
	if (setmbox != 0)
		_thr_setcontext(&tcb->tcb_tmbx.tm_context.uc_mcontext,
		    (intptr_t)&tcb->tcb_tmbx,
		    (intptr_t *)&kcb->kcb_kmbx.km_curthread);
	else
		_thr_setcontext(&tcb->tcb_tmbx.tm_context.uc_mcontext, 0, NULL);
	/* We should not reach here. */
	return (-1);
}

#endif
