/*
 * Copyright (c) 1998, 1999 Eduardo E. Horvath
 * Copyright (c) 1999 Matthew R. Green
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

#ifndef _MACHINE_OFW_UPA_H_
#define _MACHINE_OFW_UPA_H_

/*
 * These are the regs and ranges property the psycho uses. They should be
 * applicable to all UPA devices. XXX: verify this.
 */

struct upa_regs {
	u_int32_t	phys_hi;
	u_int32_t	phys_mid;
	u_int32_t	phys_lo;
	u_int32_t	size_hi;
	u_int32_t	size_lo;
};

struct upa_ranges {
	u_int32_t	cspace;
	u_int32_t	child_hi;
	u_int32_t	child_lo;
	u_int32_t	phys_hi;
	u_int32_t	phys_lo;
	u_int32_t	size_hi;
	u_int32_t	size_lo;
};

#define	UPA_RANGE_CHILD(r) \
	(((u_int64_t)(r)->child_hi << 32) | (u_int64_t)(r)->child_lo)
#define	UPA_RANGE_PHYS(r) \
	(((u_int64_t)(r)->phys_hi << 32) | (u_int64_t)(r)->phys_lo)
#define	UPA_RANGE_SIZE(r) \
	(((u_int64_t)(r)->size_hi << 32) | (u_int64_t)(r)->size_lo)
#define	UPA_RANGE_CS(r)		(((r)->cspace >> 24) & 0x03)

#endif /* !_MACHINE_OFW_UPA_H_ */
