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

#ifndef	_MACHINE_VER_H_
#define	_MACHINE_VER_H_

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

extern int cpu_impl;

/* Known implementations. */
#define	CPU_IMPL_SPARC64	0x01
#define	CPU_IMPL_ULTRASPARCI	0x10
#define	CPU_IMPL_ULTRASPARCII	0x11
#define	CPU_IMPL_ULTRASPARCIIi	0x12
#define	CPU_IMPL_ULTRASPARCIIe	0x13

#endif /* !_MACHINE_VER_H_ */
