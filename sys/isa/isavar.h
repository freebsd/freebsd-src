/*-
 * Copyright (c) 1998 Doug Rabson
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
 *	$Id: isavar.h,v 1.7 1999/05/22 15:18:28 dfr Exp $
 */

#include "isa_if.h"

/*
 * ISA devices are partially ordered to ensure that devices which are
 * sensitive to other driver probe routines are probed first. Plug and
 * Play devices are added after devices with speculative probes so that
 * the legacy hardware can claim resources allowing the Plug and Play
 * hardware to choose different resources.
 */
#define ISA_ORDER_SENSITIVE	0 /* legacy sensitive hardware */
#define ISA_ORDER_SPECULATIVE	1 /* legacy non-sensitive hardware */
#define ISA_ORDER_PNP		2 /* plug-and-play hardware */

#define	ISA_NPORT_IVARS	2
#define	ISA_NMEM_IVARS	2
#define	ISA_NIRQ_IVARS	2
#define	ISA_NDRQ_IVARS	2

enum isa_device_ivars {
	ISA_IVAR_PORT,
	ISA_IVAR_PORT_0 = ISA_IVAR_PORT,
	ISA_IVAR_PORT_1,
	ISA_IVAR_PORTSIZE,
	ISA_IVAR_PORTSIZE_0 = ISA_IVAR_PORTSIZE,
	ISA_IVAR_PORTSIZE_1,
	ISA_IVAR_MADDR,
	ISA_IVAR_MADDR_0 = ISA_IVAR_MADDR,
	ISA_IVAR_MADDR_1,
	ISA_IVAR_MSIZE,
	ISA_IVAR_MSIZE_0 = ISA_IVAR_MSIZE,
	ISA_IVAR_MSIZE_1,
	ISA_IVAR_FLAGS,
	ISA_IVAR_IRQ,
	ISA_IVAR_IRQ_0 = ISA_IVAR_IRQ,
	ISA_IVAR_IRQ_1,
	ISA_IVAR_DRQ,
	ISA_IVAR_DRQ_0 = ISA_IVAR_DRQ,
	ISA_IVAR_DRQ_1
};

extern intrmask_t isa_irq_pending(void);
extern intrmask_t isa_irq_mask(void);
#ifdef __i386__
extern void isa_wrap_old_drivers(void);
#endif

/*
 * Simplified accessors for isa devices
 */
#define ISA_ACCESSOR(A, B, T)						\
									\
static __inline T isa_get_ ## A(device_t dev)				\
{									\
	uintptr_t v;							\
	BUS_READ_IVAR(device_get_parent(dev), dev, ISA_IVAR_ ## B, &v);	\
	return (T) v;							\
}									\
									\
static __inline void isa_set_ ## A(device_t dev, T t)			\
{									\
	u_long v = (u_long) t;						\
	BUS_WRITE_IVAR(device_get_parent(dev), dev, ISA_IVAR_ ## B, v);	\
}

ISA_ACCESSOR(port, PORT, int)
ISA_ACCESSOR(portsize, PORTSIZE, int)
ISA_ACCESSOR(irq, IRQ, int)
ISA_ACCESSOR(drq, DRQ, int)
ISA_ACCESSOR(maddr, MADDR, int)
ISA_ACCESSOR(msize, MSIZE, int)
ISA_ACCESSOR(flags, FLAGS, int)
