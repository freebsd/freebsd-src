/*
 * Copyright (c) 1999, 2000 Matthew R. Green
 * Copyright (c) 2001 Thomas Moestl <tmm@FreeBSD.org>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: ebus.c,v 1.26 2001/09/10 16:27:53 eeh Exp
 *
 * $FreeBSD$
 */

/*
 * Helper functions which can be used in both ISA and EBus code.
 */

#include <sys/param.h>
#include <sys/bus.h>

#include <ofw/openfirm.h>
#include <ofw/ofw_pci.h>

#include <machine/resource.h>
#include <machine/ofw_bus.h>

#include <sparc64/isa/ofw_isa.h>
#include <sparc64/pci/ofw_pci.h>

/*
 * This applies only for an ISA/EBus with an own interrupt-map property.
 */
int
ofw_isa_map_intr(struct isa_imap *imap, int nimap, struct isa_imap_msk *imapmsk,
    int intr, struct isa_regs *regs, int nregs)
{
	char regm[8];

	return (ofw_bus_route_intr(intr, regs, sizeof(*regs), 8, nregs,
	    imap, nimap, imapmsk, regm));
}

/* XXX: this only supports PCI as parent bus right now. */
int
ofw_isa_map_iorange(struct isa_ranges *range, int nrange, u_long *start,
    u_long *end)
{
	u_int64_t offs, cstart, cend;
	int i;

	for (i = 0; i < nrange; i++) {
		cstart = ((u_int64_t)range[i].child_hi << 32) |
		    range[i].child_lo;
		cend = cstart + range[i].size;
		if (*start < cstart || *start > cend)
			continue;
		if (*end < cstart || *end > cend) {
			panic("ofw_isa_map_iorange: iorange crosses pci "
			    "ranges (%#lx not in %#lx - %#lx)", *end, cstart,
			    cend);
		}
		offs = (((u_int64_t)range[i].phys_mid << 32) |
		    range[i].phys_lo);
		*start = *start + offs - cstart;
		*end  = *end + offs - cstart;
		/* Isolate address space and find the right tag */
		switch (ISA_RANGE_PS(&range[i])) {
		case PCI_CS_IO:
			return (SYS_RES_IOPORT);
		case PCI_CS_MEM32:
			return (SYS_RES_MEMORY);
		default:
			panic("ofw_isa_map_iorange: illegal space %x",
			    ISA_RANGE_PS(&range[i]));
			break;
		}
	}
	panic("ofw_isa_map_iorange: could not map range %#lx - %#lx",
	    *start, *end);
}
