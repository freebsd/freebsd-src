/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)types.h	8.3 (Berkeley) 1/5/94
 * $FreeBSD$
 */

#ifndef _MACHINE_TYPES_H_
#define	_MACHINE_TYPES_H_

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
typedef	struct _physadr {
	int	r[1];
} *physadr;

typedef	struct label_t {
	int	val[6];
} label_t;
#endif

typedef	unsigned int	vm_offset_t;
typedef	__int64_t	vm_ooffset_t;
typedef	unsigned int	vm_pindex_t;
typedef	unsigned int	vm_size_t;

typedef	__int32_t	register_t;
typedef	__uint32_t	u_register_t;

#ifdef _KERNEL
typedef	int		intfptr_t;
typedef	unsigned int	uintfptr_t;
#endif

/* Critical section value */
typedef	register_t	critical_t;

/* Interrupt mask (spl, xxx_imask, etc) */
typedef	__uint32_t	intrmask_t;

/* Interrupt handler function type. */
typedef	void		ointhand2_t(int _device_id);

#endif /* !_MACHINE_TYPES_H_ */
