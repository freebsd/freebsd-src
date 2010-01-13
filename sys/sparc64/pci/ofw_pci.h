/*-
 * Copyright (c) 1999, 2000 Matthew R. Green
 * Copyright (c) 2001, 2003 by Thomas Moestl <tmm@FreeBSD.org>
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
 *	from: NetBSD: psychoreg.h,v 1.8 2001/09/10 16:17:06 eeh Exp
 *
 * $FreeBSD$
 */

#ifndef _SPARC64_PCI_OFW_PCI_H_
#define	_SPARC64_PCI_OFW_PCI_H_

#include <machine/ofw_bus.h>

typedef uint32_t ofw_pci_intr_t;

/* PCI range child spaces. XXX: are these MI? */
#define	OFW_PCI_CS_CONFIG	0x00
#define	OFW_PCI_CS_IO		0x01
#define	OFW_PCI_CS_MEM32	0x02
#define	OFW_PCI_CS_MEM64	0x03

/* OFW device types */
#define	OFW_TYPE_PCI		"pci"
#define	OFW_TYPE_PCIE		"pciex"

struct ofw_pci_ranges {
	uint32_t	cspace;
	uint32_t	child_hi;
	uint32_t	child_lo;
	uint32_t	phys_hi;
	uint32_t	phys_lo;
	uint32_t	size_hi;
	uint32_t	size_lo;
};

#define	OFW_PCI_RANGE_CHILD(r) \
	(((uint64_t)(r)->child_hi << 32) | (uint64_t)(r)->child_lo)
#define	OFW_PCI_RANGE_PHYS(r) \
	(((uint64_t)(r)->phys_hi << 32) | (uint64_t)(r)->phys_lo)
#define	OFW_PCI_RANGE_SIZE(r) \
	(((uint64_t)(r)->size_hi << 32) | (uint64_t)(r)->size_lo)
#define	OFW_PCI_RANGE_CS(r)	(((r)->cspace >> 24) & 0x03)

/* default values */
#define	OFW_PCI_LATENCY	64

#endif /* ! _SPARC64_PCI_OFW_PCI_H_ */
