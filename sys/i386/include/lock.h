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
#define IMASK_LOCK	MTX_LOCK_SPIN(_imen_mtx, 0)
#define IMASK_UNLOCK	MTX_UNLOCK_SPIN(_imen_mtx)

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
#define COM_LOCK() 	mtx_lock_spin(&com_mtx)
#define COM_UNLOCK() 	mtx_unlock_spin(&com_mtx)
#else
#define COM_LOCK()
#define COM_UNLOCK()
#endif /* USE_COMLOCK */

#else /* SMP */

#define COM_LOCK()
#define COM_UNLOCK()

#endif /* SMP */

/* global data in mp_machdep.c */
extern struct mtx	imen_mtx;
extern struct mtx	com_mtx;
extern struct mtx	mcount_mtx;
extern struct mtx	panic_mtx;

#endif /* LOCORE */

#endif /* !_MACHINE_LOCK_H_ */
