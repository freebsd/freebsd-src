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

#ifndef _SPARC64_ISA_OFW_ISA_H_
#define _SPARC64_ISA_OFW_ISA_H_

/*
 * ISA PROM structures
 */
struct isa_regs {
	u_int32_t	phys_hi;	/* high bits of physaddr */ 
	u_int32_t	phys_lo;
	u_int32_t	size;
};

#define	ISA_REG_PHYS(r) \
	((((u_int64_t)((r)->phys_hi)) << 32) | ((u_int64_t)(r)->phys_lo))

/* XXX: this is a guess. Verify... */
struct isa_ranges {
	u_int32_t	child_hi;
	u_int32_t	child_lo;
	u_int32_t	phys_hi;
	u_int32_t	phys_mid;
	u_int32_t	phys_lo;
	u_int32_t	size;
};

#define	ISA_RANGE_CHILD(r) \
	((((u_int64_t)((r)->child_hi)) << 32) | ((u_int64_t)(r)->child_lo))
#define	ISA_RANGE_PS(r)	(((r)->phys_hi >> 24) & 0x03)

struct isa_imap {
	u_int32_t	phys_hi;	/* high phys addr mask */
	u_int32_t	phys_lo;	/* low phys addr mask */
	u_int32_t	intr;		/* interrupt mask */
	int32_t		cnode;		/* child node */
	u_int32_t	cintr;		/* child interrupt */
};

struct isa_imap_msk {
	u_int32_t	phys_hi;	/* high phys addr */
	u_int32_t	phys_lo;	/* low phys addr */
	u_int32_t	intr;		/* interrupt */
};

/* Map an interrupt property to an INO */
int ofw_isa_map_intr(struct isa_imap *, int, struct isa_imap_msk *, int,
    struct isa_regs *, int);
/* Map an IO range. Returns the resource type of the range. */
int ofw_isa_map_iorange(struct isa_ranges *, int, u_long *, u_long *);

#endif /* !_SPARC64_ISA_OFW_ISA_H_ */
