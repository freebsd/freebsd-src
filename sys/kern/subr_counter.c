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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <vm/uma.h>

#define IN_SUBR_COUNTER_C
#include <sys/counter.h>
 
static uma_zone_t uint64_pcpu_zone;

void
counter_u64_zero(counter_u64_t c)
{

	counter_u64_zero_inline(c);
}

uint64_t
counter_u64_fetch(counter_u64_t c)
{

	return (counter_u64_fetch_inline(c));
}

counter_u64_t
counter_u64_alloc(int flags)
{
	counter_u64_t r;

	r = uma_zalloc(uint64_pcpu_zone, flags);
	if (r != NULL)
		counter_u64_zero(r);

	return (r);
}

void
counter_u64_free(counter_u64_t c)
{

	uma_zfree(uint64_pcpu_zone, c);
}

int
sysctl_handle_counter_u64(SYSCTL_HANDLER_ARGS)
{
	uint64_t out;
	int error;

	out = counter_u64_fetch(*(counter_u64_t *)arg1);

	error = SYSCTL_OUT(req, &out, sizeof(uint64_t));

	if (error || !req->newptr)
		return (error);

	/*
	 * Any write attempt to a counter zeroes it.
	 */
	counter_u64_zero(*(counter_u64_t *)arg1);

	return (0);
}

static void
counter_startup(void)
{

	uint64_pcpu_zone = uma_zcreate("uint64 pcpu", sizeof(uint64_t),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_PCPU);
}
SYSINIT(counter, SI_SUB_KMEM, SI_ORDER_ANY, counter_startup, NULL);
