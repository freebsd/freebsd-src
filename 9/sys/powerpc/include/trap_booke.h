/*-
 * Copyright (C) 2006 Semihalf, Rafal Jaworowski <raj@semihalf.com>
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
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_POWERPC_TRAP_H_
#define	_POWERPC_TRAP_H_

#define	EXC_CRIT	0
#define	EXC_MCHK	1
#define	EXC_DSI		2
#define	EXC_ISI		3
#define	EXC_EXI		4
#define	EXC_ALI		5
#define	EXC_PGM		6
#define	EXC_FPU		7	/* e500: not supported */
#define	EXC_SC		8
#define	EXC_APU		9	/* e500: not supported */
#define	EXC_DECR	10
#define	EXC_FIT		11
#define	EXC_WDOG	12
#define	EXC_DTMISS	13
#define	EXC_ITMISS	14
#define	EXC_DEBUG	15

#define	EXC_SAU		32	/* e500-specific: SPE APU unavailable */
#define	EXC_PERF	35	/* e500-specific: Performance monitor */

#define	EXC_LAST	255

#endif	/* _POWERPC_TRAP_H_ */
