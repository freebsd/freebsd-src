/*	$NetBSD: obiovar.h,v 1.4 2003/06/16 17:40:53 thorpej Exp $	*/

/*-
 * Copyright (c) 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef _RT305X_OBIOVAR_H_
#define	_RT305X_OBIOVAR_H_

#include <sys/rman.h>

/* Number of IRQs */
#define	NIRQS	32


struct obio_softc {
	struct rman		oba_mem_rman;
	struct rman		oba_irq_rman;
	struct rman		oba_gpio_rman;
	struct intr_event	*sc_eventstab[NIRQS]; /* IRQ events structs */
	struct resource		*sc_irq;	/* IRQ resource */
	void			*sc_ih;		/* interrupt cookie */
	struct resource		*sc_fast_irq;	/* IRQ resource */
	void			*sc_fast_ih;	/* interrupt cookie */
};

struct obio_ivar {
	struct resource_list	resources;
};

#endif /* _RT305X_OBIOVAR_H_ */
