/*
 * Copyright (c) 1997, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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


#ifndef _MACHINE_LOCK_H_
#define _MACHINE_LOCK_H_


#ifdef LOCORE

#ifdef SMP

#define	MPLOCKED	lock ;

/*
 * Protects the IO APIC and apic_imen as a critical region.
 */
#define IMASK_LOCK							\
	pushl	$_imen_lock ;			/* address of lock */	\
	call	_s_lock ;			/* MP-safe */		\
	addl	$4, %esp

#define IMASK_UNLOCK							\
	movl	$0, _imen_lock

#else  /* SMP */

#define	MPLOCKED				/* NOP */

#define IMASK_LOCK				/* NOP */
#define IMASK_UNLOCK				/* NOP */

#endif /* SMP */

#else /* LOCORE */

#ifdef SMP

#include <machine/smptests.h>			/** xxx_LOCK */

/*
 * sio/cy lock.
 * XXX should rc (RISCom/8) use this?
 */
#ifdef USE_COMLOCK
#define COM_LOCK() 	s_lock(&com_lock)
#define COM_UNLOCK() 	s_unlock(&com_lock)
#else
#define COM_LOCK()
#define COM_UNLOCK()
#endif /* USE_COMLOCK */

#else /* SMP */

#define COM_LOCK()
#define COM_UNLOCK()

#endif /* SMP */

/*
 * Simple spin lock.
 * It is an error to hold one of these locks while a process is sleeping.
 */
struct simplelock {
	volatile int	lock_data;
};

/* functions in simplelock.s */
void	s_lock_init		__P((struct simplelock *));
void	s_lock			__P((struct simplelock *));
int	s_lock_try		__P((struct simplelock *));
void	ss_lock			__P((struct simplelock *));
void	ss_unlock		__P((struct simplelock *));
void	s_lock_np		__P((struct simplelock *));
void	s_unlock_np		__P((struct simplelock *));

/* inline simplelock functions */
static __inline void
s_unlock(struct simplelock *lkp)
{
	lkp->lock_data = 0;
}

/* global data in mp_machdep.c */
extern struct simplelock	imen_lock;
extern struct simplelock	cpl_lock;
extern struct simplelock	fast_intr_lock;
extern struct simplelock	intr_lock;
extern struct simplelock	com_lock;
extern struct simplelock	mpintr_lock;
extern struct simplelock	mcount_lock;
extern struct simplelock	panic_lock;

#if !defined(SIMPLELOCK_DEBUG) && MAXCPU > 1
/*
 * This set of defines turns on the real functions in i386/isa/apic_ipl.s.
 */
#define	simple_lock_init(alp)	s_lock_init(alp)
#define	simple_lock(alp)	s_lock(alp)
#define	simple_lock_try(alp)	s_lock_try(alp)
#define	simple_unlock(alp)	s_unlock(alp)

#endif /* !SIMPLELOCK_DEBUG && MAXCPU > 1 */

#endif /* LOCORE */

#endif /* !_MACHINE_LOCK_H_ */
