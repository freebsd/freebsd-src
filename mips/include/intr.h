/*	$NetBSD: intr.h,v 1.5 1996/05/13 06:11:28 mycroft Exp $ */

/*-
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *	JNPR: intr.h,v 1.4 2007/08/09 11:23:32 katta
 * $FreeBSD$
 */

#ifndef _MACHINE_INTR_H_
#define	_MACHINE_INTR_H_

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#ifndef _LOCORE

/*
 * Index into intrcnt[], which is defined in exceptions.S
 *	Index # = irq # - 1
 */
#define	INTRCNT_HARDCLOCK	0
#define	INTRCNT_RTC		1
#define	INTRCNT_SIO		2	/* irq 3 */
#define	INTRCNT_PE		3	/* irq 4 */
#define	INTRCNT_PICNIC		4	/* irq 5 */

extern uint32_t idle_mask;
extern void (*mips_ack_interrupt)(int, uint32_t);

typedef int	ih_func_t(void *);

struct intr_event;

struct mips_intr_handler {
	int	(*ih_func) (void *);
	void	*ih_arg;
	struct	intr_event *ih_event;
	u_int	ih_flags;
	volatile long *ih_count;
	int	ih_level;
	int	ih_irq;
	void	*frame;
};

extern struct mips_intr_handler intr_handlers[];

typedef void (*mask_fn)(void *);

void mips_mask_irq(void);
void mips_unmask_irq(void);

struct trapframe;
void	mips_set_intr(int pri, uint32_t mask,
	    uint32_t (*int_hand)(uint32_t, struct trapframe *));
uint32_t mips_handle_interrupts(uint32_t pending, struct trapframe *cf);
void	intr_enable_source(uintptr_t irq);
struct trapframe * mips_get_trapframe(void *ih_arg);
int	inthand_add(const char *name, u_int irq, void (*handler)(void *),
	    void *arg, int flags, void **cookiep);
int	inthand_remove(u_int irq, void *cookie);
void	bvif_attach(void);

#endif /* _LOCORE */

#endif /* !_MACHINE_INTR_H_ */
