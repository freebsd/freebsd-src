/*-
 * Copyright 2008-2009 Stacey Son <sson@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

/*
 * Backend for the lock tracing (lockstat) kernel support. This is required 
 * to allow a module to load even though DTrace kernel support may not be 
 * present. 
 *
 */

#ifdef KDTRACE_HOOKS

#include <sys/time.h>
#include <sys/types.h>
#include <sys/lockstat.h>

/*
 * The following must match the type definition of dtrace_probe.  It is  
 * defined this way to avoid having to rely on CDDL code.
 */
uint32_t lockstat_probemap[LS_NPROBES];
void (*lockstat_probe_func)(uint32_t, uintptr_t, uintptr_t,
    uintptr_t, uintptr_t, uintptr_t);
int lockstat_enabled = 0;

uint64_t 
lockstat_nsecs(void)
{
	struct bintime bt;
	uint64_t ns;

	if (!lockstat_enabled)
		return (0);

	binuptime(&bt);
	ns = bt.sec * (uint64_t)1000000000;
	ns += ((uint64_t)1000000000 * (uint32_t)(bt.frac >> 32)) >> 32;
	return (ns);
}

#endif /* KDTRACE_HOOKS */
