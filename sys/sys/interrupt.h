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

/*
 * Describe a hardware interrupt handler.
 *
 * Multiple interrupt handlers for a specific vector can be chained
 * together via the 'next' pointer.
 */

struct intrhand {
	driver_intr_t	*ih_handler;	/* code address of handler */
	void		*ih_argument;	/* argument to pass to handler */
	enum intr_type	 ih_flags;	/* flag bits (sys/bus.h) */
	char		*ih_name;	/* name of handler */
	struct ithd	*ih_ithd;	/* handler we're connected to */
	struct intrhand	*ih_next;	/* next handler for this irq */
	int		 ih_need;	/* need interrupt */
};

int	ithread_priority __P((int flags));
void	sched_swi __P((struct intrhand *, int));
#define	SWI_SWITCH	0x1
#define	SWI_NOSWITCH	0x2
#define	SWI_DELAY	0x4	/* implies NOSWITCH */

struct	intrhand * sinthand_add __P((const char *name, struct ithd **,
    driver_intr_t, void *arg, int pri, int flags));

extern struct ithd *tty_ithd;
extern struct ithd *clk_ithd;

extern struct intrhand *net_ih;
extern struct intrhand *softclock_ih;
extern struct intrhand *vm_ih;

#endif
