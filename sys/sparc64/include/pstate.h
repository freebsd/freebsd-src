/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#ifndef	_MACHINE_PSTATE_H_
#define	_MACHINE_PSTATE_H_

#define	PSTATE_AG		(1<<0)
#define	PSTATE_IE		(1<<1)
#define	PSTATE_PRIV		(1<<2)
#define	PSTATE_AM		(1<<3)
#define	PSTATE_PEF		(1<<4)
#define	PSTATE_RED		(1<<5)

#define	PSTATE_MM_SHIFT		(6)
#define	PSTATE_MM_MASK		((1<<PSTATE_MM_SHIFT)|(1<<(PSTATE_MM_SHIFT+1)))
#define	PSTATE_MM_TSO		(0<<PSTATE_MM_SHIFT)
#define	PSTATE_MM_PSO		(1<<PSTATE_MM_SHIFT)
#define	PSTATE_MM_RMO		(2<<PSTATE_MM_SHIFT)

#define	PSTATE_TLE		(1<<8)
#define	PSTATE_CLE		(1<<9)
#define	PSTATE_MG		(1<<10)
#define	PSTATE_IG		(1<<11)

#define	TSTATE_PSTATE_SHIFT	8
#define	TSTATE_PSTATE(x)	((x) << TSTATE_PSTATE_SHIFT)
#define	TSTATE_AG		TSTATE_PSTATE(PSTATE_AG)
#define	TSTATE_IE		TSTATE_PSTATE(PSTATE_IE)
#define	TSTATE_PRIV		TSTATE_PSTATE(PSTATE_PRIV)
#define	TSTATE_AM		TSTATE_PSTATE(PSTATE_AM)
#define	TSTATE_PEF		TSTATE_PSTATE(PSTATE_PEF)
#define	TSTATE_RED		TSTATE_PSTATE(PSTATE_RED)
#define	TSTATE_TLE		TSTATE_PSTATE(PSTATE_TLE)
#define	TSTATE_CLE		TSTATE_PSTATE(PSTATE_CLE)
#define	TSTATE_MG		TSTATE_PSTATE(PSTATE_MG)
#define	TSTATE_IG		TSTATE_PSTATE(PSTATE_IG)

#define	VER_MANUF_SHIFT		(48)
#define	VER_IMPL_SHIFT		(32)
#define	VER_MASK_SHIFT		(24)
#define	VER_MAXTL_SHIFT		(8)
#define	VER_MAXWIN_SHIFT	(0)

#define	VER_MANUF_SIZE		(16)
#define	VER_IMPL_SIZE		(16)
#define	VER_MASK_SIZE		(8)
#define	VER_MAXTL_SIZE		(8)
#define	VER_MAXWIN_SIZE		(5)

#define	VER_MANUF_MASK		(((1L<<VER_MANUF_SIZE)-1)<<VER_MANUF_SHIFT)
#define	VER_IMPL_MASK		(((1L<<VER_IMPL_SIZE)-1)<<VER_IMPL_SHIFT)
#define	VER_MASK_MASK		(((1L<<VER_MASK_SIZE)-1)<<VER_MASK_SHIFT)
#define	VER_MAXTL_MASK		(((1L<<VER_MAXTL_SIZE)-1)<<VER_MAXTL_SHIFT)
#define	VER_MAXWIN_MASK		(((1L<<VER_MAXWIN_SIZE)-1)<<VER_MAXWIN_SHIFT)

#define	VER_MANUF(ver) \
	(((ver) & VER_MANUF_MASK) >> VER_MANUF_SHIFT)
#define	VER_IMPL(ver) \
	(((ver) & VER_IMPL_MASK) >> VER_IMPL_SHIFT)
#define	VER_MASK(ver) \
	(((ver) & VER_MASK_MASK) >> VER_MASK_SHIFT)
#define	VER_MAXTL(ver) \
	(((ver) & VER_MAXTL_MASK) >> VER_MAXTL_SHIFT)
#define	VER_MAXWIN(ver) \
	(((ver) & VER_MAXWIN_MASK) >> VER_MAXWIN_SHIFT)

#endif /* !_MACHINE_PSTATE_H_ */
