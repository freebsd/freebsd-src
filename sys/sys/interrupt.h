/*-
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
 * $FreeBSD: src/sys/sys/interrupt.h,v 1.37 2007/05/06 17:02:50 piso Exp $
 */

#ifndef _SYS_INTERRUPT_H_
#define _SYS_INTERRUPT_H_

#include <sys/_lock.h>
#include <sys/_mutex.h>

struct intr_event;
struct intr_thread;
struct trapframe;

/*
 * Describe a hardware interrupt handler.
 *
 * Multiple interrupt handlers for a specific event can be chained
 * together.
 */
struct intr_handler {
	driver_filter_t	*ih_filter;	/* Filter function. */
	driver_intr_t	*ih_handler;	/* Handler function. */
	void		*ih_argument;	/* Argument to pass to handler. */
	int		 ih_flags;
	const char	*ih_name;	/* Name of handler. */
	struct intr_event *ih_event;	/* Event we are connected to. */
	int		 ih_need;	/* Needs service. */
	TAILQ_ENTRY(intr_handler) ih_next; /* Next handler for this event. */
	u_char		 ih_pri;	/* Priority of this handler. */
	struct intr_thread *ih_thread;	/* Ithread for filtered handler. */
};

/* Interrupt handle flags kept in ih_flags */
#define	IH_EXCLUSIVE	0x00000002	/* Exclusive interrupt. */
#define	IH_ENTROPY	0x00000004	/* Device is a good entropy source. */
#define	IH_DEAD		0x00000008	/* Handler should be removed. */
#define	IH_MPSAFE	0x80000000	/* Handler does not need Giant. */

/*
 * Describe an interrupt event.  An event holds a list of handlers.
 */
struct intr_event {
	TAILQ_ENTRY(intr_event) ie_list;
	TAILQ_HEAD(, intr_handler) ie_handlers; /* Interrupt handlers. */
	char		ie_name[MAXCOMLEN]; /* Individual event name. */
	char		ie_fullname[MAXCOMLEN];
	struct mtx	ie_lock;
	void		*ie_source;	/* Cookie used by MD code. */
	struct intr_thread *ie_thread;	/* Thread we are connected to. */
	void		(*ie_enable)(void *);
#ifdef INTR_FILTER
	void		(*ie_eoi)(void *);
	void		(*ie_disab)(void *);
#endif
	int		ie_flags;
	int		ie_count;	/* Loop counter. */
	int		ie_warncnt;	/* Rate-check interrupt storm warns. */
	struct timeval	ie_warntm;
};

/* Interrupt event flags kept in ie_flags. */
#define	IE_SOFT		0x000001	/* Software interrupt. */
#define	IE_ENTROPY	0x000002	/* Interrupt is an entropy source. */
#define	IE_ADDING_THREAD 0x000004	/* Currently building an ithread. */

/* Flags to pass to sched_swi. */
#define	SWI_DELAY	0x2

/*
 * Software interrupt numbers in priority order.  The priority determines
 * the priority of the corresponding interrupt thread.
 */
#define	SWI_TTY		0
#define	SWI_NET		1
#define	SWI_CAMBIO	2
#define	SWI_VM		3
#define	SWI_CLOCK	4
#define	SWI_TQ_FAST	5
#define	SWI_TQ		6
#define	SWI_TQ_GIANT	6

extern struct	intr_event *tty_intr_event;
extern struct	intr_event *clk_intr_event;
extern void	*softclock_ih;
extern void	*vm_ih;

/* Counts and names for statistics (defined in MD code). */
extern u_long 	eintrcnt[];	/* end of intrcnt[] */
extern char 	eintrnames[];	/* end of intrnames[] */
extern u_long 	intrcnt[];	/* counts for for each device and stray */
extern char 	intrnames[];	/* string table containing device names */

#ifdef DDB
void	db_dump_intr_event(struct intr_event *ie, int handlers);
#endif
#ifdef INTR_FILTER
int     intr_filter_loop(struct intr_event *ie, struct trapframe *frame, 
			 struct intr_thread **ithd);
int     intr_event_handle(struct intr_event *ie, struct trapframe *frame);
#endif
u_char	intr_priority(enum intr_type flags);
int	intr_event_add_handler(struct intr_event *ie, const char *name,
	    driver_filter_t filter, driver_intr_t handler, void *arg, 
	    u_char pri, enum intr_type flags, void **cookiep);	    
#ifndef INTR_FILTER
int	intr_event_create(struct intr_event **event, void *source,
	    int flags, void (*enable)(void *), const char *fmt, ...)
	    __printflike(5, 6);
#else
int	intr_event_create(struct intr_event **event, void *source,
	    int flags, void (*enable)(void *), void (*eoi)(void *), 
	    void (*disab)(void *), const char *fmt, ...)
	    __printflike(7, 8);
#endif
int	intr_event_destroy(struct intr_event *ie);
int	intr_event_remove_handler(void *cookie);
#ifndef INTR_FILTER
int	intr_event_schedule_thread(struct intr_event *ie);
#else
int	intr_event_schedule_thread(struct intr_event *ie,
	    struct intr_thread *ithd);
#endif
void	*intr_handler_source(void *cookie);
int	swi_add(struct intr_event **eventp, const char *name,
	    driver_intr_t handler, void *arg, int pri, enum intr_type flags,
	    void **cookiep);
void	swi_sched(void *cookie, int flags);
int	swi_remove(void *cookie);

#endif
