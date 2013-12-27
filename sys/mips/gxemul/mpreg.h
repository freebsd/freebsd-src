/*-
 * Copyright (c) 2004-2012 Juli Mallett <jmallett@FreeBSD.org>
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

#ifndef	_MIPS_GXEMUL_MPREG_H_
#define	_MIPS_GXEMUL_MPREG_H_

#define	GXEMUL_MP_DEV_BASE	0x11000000

#define	GXEMUL_MP_DEV_WHOAMI	0x0000
#define	GXEMUL_MP_DEV_NCPUS	0x0010
#define	GXEMUL_MP_DEV_START	0x0020
#define	GXEMUL_MP_DEV_STARTADDR	0x0030
#define	GXEMUL_MP_DEV_STACK	0x0070
#define	GXEMUL_MP_DEV_RANDOM	0x0080
#define	GXEMUL_MP_DEV_MEMORY	0x0090
#define	GXEMUL_MP_DEV_IPI_ONE	0x00a0
#define	GXEMUL_MP_DEV_IPI_MANY	0x00b0
#define	GXEMUL_MP_DEV_IPI_READ	0x00c0
#define	GXEMUL_MP_DEV_CYCLES	0x00d0

#ifdef _LP64
#define	GXEMUL_MP_DEV_FUNCTION(f)					\
	(volatile uint64_t *)MIPS_PHYS_TO_DIRECT_UNCACHED(GXEMUL_MP_DEV_BASE + (f))
#define	GXEMUL_MP_DEV_READ(f)						\
	(volatile uint64_t)*GXEMUL_MP_DEV_FUNCTION(f)
#else
#define	GXEMUL_MP_DEV_FUNCTION(f)					\
	(volatile uint32_t *)MIPS_PHYS_TO_DIRECT_UNCACHED(GXEMUL_MP_DEV_BASE + (f))
#define	GXEMUL_MP_DEV_READ(f)						\
	(volatile uint32_t)*GXEMUL_MP_DEV_FUNCTION(f)
#endif
#define	GXEMUL_MP_DEV_WRITE(f, v)					\
	*GXEMUL_MP_DEV_FUNCTION(f) = (v)

#define	GXEMUL_MP_DEV_IPI_INTERRUPT	(6)

#endif /* !_MIPS_GXEMUL_MPREG_H */
