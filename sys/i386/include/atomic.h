/*-
 * Copyright (c) 1998 Doug Rabson
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
 *	$Id$
 */
#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

/*
 * Various simple arithmetic on memory which is atomic in the presence
 * of interrupts.
 *
 * Note: these versions are not SMP safe.
 */

#define atomic_set_char(P, V)		(*(u_char*)(P) |= (V))
#define atomic_clear_char(P, V)		(*(u_char*)(P) &= ~(V))
#define atomic_add_char(P, V)		(*(u_char*)(P) += (V))
#define atomic_subtract_char(P, V)	(*(u_char*)(P) -= (V))

#define atomic_set_short(P, V)		(*(u_short*)(P) |= (V))
#define atomic_clear_short(P, V)	(*(u_short*)(P) &= ~(V))
#define atomic_add_short(P, V)		(*(u_short*)(P) += (V))
#define atomic_subtract_short(P, V)	(*(u_short*)(P) -= (V))

#define atomic_set_int(P, V)		(*(u_int*)(P) |= (V))
#define atomic_clear_int(P, V)		(*(u_int*)(P) &= ~(V))
#define atomic_add_int(P, V)		(*(u_int*)(P) += (V))
#define atomic_subtract_int(P, V)	(*(u_int*)(P) -= (V))

#define atomic_set_long(P, V)		(*(u_long*)(P) |= (V))
#define atomic_clear_long(P, V)		(*(u_long*)(P) &= ~(V))
#define atomic_add_long(P, V)		(*(u_long*)(P) += (V))
#define atomic_subtract_long(P, V)	(*(u_long*)(P) -= (V))

#endif /* ! _MACHINE_ATOMIC_H_ */
