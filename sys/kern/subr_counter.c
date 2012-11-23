/*-
 * Copyright (c) 2012 Gleb Smirnoff <glebius@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/counter.h>
#include <vm/uma.h>
 
static uma_zone_t uint64_pcpu_zone;

void
counter_u64_zero(counter_u64_t c)
{

	for (int i = 0; i < mp_ncpus; i++)
		*(uint64_t *)((char *)c + sizeof(struct pcpu) * i) = 0;
}

uint64_t
counter_u64_fetch(counter_u64_t c)
{
	uint64_t r;

	r = 0;
	for (int i = 0; i < mp_ncpus; i++)
		r += *(uint64_t *)((char *)c + sizeof(struct pcpu) * i);

	return (r);
}

counter_u64_t
counter_u64_alloc(int flags)
{
	counter_u64_t r;

	r = uma_zalloc(uint64_pcpu_zone, flags);
	if (r)
		counter_u64_zero(r);

	return (r);
}

void
counter_u64_free(counter_u64_t c)
{

	uma_zfree(uint64_pcpu_zone, c);
}

static void
counter_startup()
{

	uint64_pcpu_zone = uma_zcreate("uint64 pcpu", sizeof(uint64_t),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_PCPU);
}
SYSINIT(counter, SI_SUB_KMEM, SI_ORDER_ANY, counter_startup, NULL);
