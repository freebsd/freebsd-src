/*-
 * Copyright (c) 2014 Gleb Smirnoff <glebius@FreeBSD.org>
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

#ifndef __SYS_LWREF_H__
#define __SYS_LWREF_H__

#include <sys/counter.h>

struct lwref;
typedef struct lwref * lwref_t;

lwref_t	lwref_alloc(void *, int);
int lwref_change(lwref_t, void *, void(*)(void *, void *), void *);

/* asm */
void *lwref_acquire(lwref_t, counter_u64_t *);
extern char lwref_acquire_ponr[];

extern char timerint_ret[];
extern char apic_isr1_ret[];
extern char apic_isr2_ret[];
extern char apic_isr3_ret[];
extern char apic_isr4_ret[];
extern char apic_isr5_ret[];
extern char apic_isr6_ret[];
extern char apic_isr7_ret[];
extern char ipi_intr_bitmap_handler_ret[];

#ifdef INVARIANTS
#define lwref_release(p, c)	do {	\
	p = NULL;			\
	counter_u64_add(c, -1);		\
} while (0)
#else
#define lwref_release(p, c)	counter_u64_add(c, -1)
#endif

#endif	/* ! __SYS_LWREF_H__ */
