/*-
 * Copyright (c) 2013 Juniper Networks, Inc.
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

#ifndef _MACHINE_FDT_H_
#define _MACHINE_FDT_H_

#include <machine/intr_machdep.h>
#include <x86/bus.h>

/* Max interrupt number. */
#define FDT_INTR_MAX	NUM_IO_INTS

/* Map phandle/intpin pair to global IRQ number */
#define	FDT_MAP_IRQ(node, pin)	\
	    (panic("%s: FDT_MAP_IRQ(%#x, %#x)", __func__, node, pin), -1)

/* Bus space tag. XXX we only support I/O port space this way. */
#define fdtbus_bs_tag	X86_BUS_SPACE_IO

struct mem_region {
	vm_offset_t	mr_start;
	vm_size_t	mr_size;
};

__BEGIN_DECLS
int x86_init_fdt(void);
__END_DECLS

#endif /* _MACHINE_FDT_H_ */
