/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#ifndef _SYS_INTERRUPT_H_
#define _SYS_INTERRUPT_H_

#include <sys/_lock.h>
#include <sys/_mutex.h>

/*
 * Describe a hardware interrupt handler.
 *
 * Multiple interrupt handlers for a specific vector can be chained
 * together.
 */
struct intrhand {
	driver_intr_t	*ih_handler;	/* Handler function. */
	void		*ih_argument;	/* Argument to pass to handler. */
	int		 ih_flags;
	const char	*ih_name;	/* Name of handler. */
	struct ithd	*ih_ithread;	/* Ithread we are connected to. */
	int		 ih_need;	/* Needs service. */
	TAILQ_ENTRY(intrhand) ih_next;	/* Next handler for this vector. */
	u_char		 ih_pri;	/* Priority of this handler. */
};

/* Interrupt handle flags kept in ih_flags */
#define	IH_FAST		0x00000001	/* Fast interrupt. */
#define	IH_EXCLUSIVE	0x00000002	/* Exclusive interrupt. */
#define	IH_ENTROPY	0x00000004	/* Device is a good entropy source. */
#define	IH_DEAD		0x00000008	/* Handler should be removed. */
#define	IH_MPSAFE	0x80000000	/* Handler does not need Giant. */

/*
 * Describe an interrupt thread.  There is one of these per interrupt vector.
 * Note that this actually describes an interrupt source.  There may or may
 * not be an actual kernel thread attached to a given source.
 */
struct ithd {
	struct	mtx it_lock;
	struct	thread *it_td;		/* Interrupt process. */
	LIST_ENTRY(ithd) it_list;	/* All interrupt threads. */
	TAILQ_HEAD(, intrhand) it_handlers; /* Interrupt handlers. */
	struct	ithd *it_interrupted;	/* Who we interrupted. */
	void	(*it_disable)(int);	/* Enable interrupt source. */
	void	(*it_enable)(int);	/* Disable interrupt source. */
	void	*it_md;			/* Hook for MD interrupt code. */
	int	it_flags;		/* Interrupt-specific flags. */
	int	it_need;		/* Needs service. */
	int	it_vector;
	char	it_name[MAXCOMLEN + 1];
};

/* Interrupt thread flags kept in it_flags */
#define	IT_SOFT		0x000001	/* Software interrupt. */
#define	IT_ENTROPY	0x000002	/* Interrupt is an entropy source. */
#define	IT_DEAD		0x000004	/* Thread is waiting to exit. */

/* Flags to pass to sched_swi. */
#define	SWI_DELAY	0x2

/*
 * Software interrupt bit numbers in priority order.  The priority only
 * determines which swi will be dispatched next; a higher priority swi
 * may be dispatched when a nested h/w interrupt handler returns.
 */
#define	SWI_TTY		0
#define	SWI_NET		1
#define	SWI_CAMNET	2
#define	SWI_CAMBIO	3
#define	SWI_VM		4
#define	SWI_TQ_FAST	5
#define	SWI_TQ_GIANT	6
#define	SWI_TQ		7
#define	SWI_CLOCK	8

extern struct	ithd *tty_ithd;
extern struct	ithd *clk_ithd;
extern void	*net_ih;
extern void	*softclock_ih;
extern void	*vm_ih;

/* Counts and names for statistics (defined in MD code). */
extern u_long 	eintrcnt[];	/* end of intrcnt[] */
extern char 	eintrnames[];	/* end of intrnames[] */
extern u_long 	intrcnt[];	/* counts for for each device and stray */
extern char 	intrnames[];	/* string table containing device names */

#ifdef DDB
void	db_dump_ithread(struct ithd *ithd, int handlers);
#endif
int	ithread_create(struct ithd **ithread, int vector, int flags,
	    void (*disable)(int), void (*enable)(int), const char *fmt, ...)
	    __printflike(6, 7);
int	ithread_destroy(struct ithd *ithread);
u_char	ithread_priority(enum intr_type flags);
int	ithread_add_handler(struct ithd *ithread, const char *name,
	    driver_intr_t handler, void *arg, u_char pri, enum intr_type flags,
	    void **cookiep);
int	ithread_remove_handler(void *cookie);
int	ithread_schedule(struct ithd *ithread, int do_switch);
int     swi_add(struct ithd **ithdp, const char *name,
	    driver_intr_t handler, void *arg, int pri, enum intr_type flags,
	    void **cookiep);
void	swi_sched(void *cookie, int flags);

#endif
