/*
 * Copyright (c) 2001 Wind River Systems, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/*
 * This module provides MI support for per-cpu data.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pcpu.h>

static struct globaldata *cpuid_to_globaldata[MAXCPU];
struct cpuhead cpuhead = SLIST_HEAD_INITIALIZER(cpuhead);

/*
 * Register a struct globaldata.
 */
void
globaldata_register(struct globaldata *globaldata)
{

	KASSERT(globaldata->gd_cpuid >= 0 && globaldata->gd_cpuid < MAXCPU,
	    ("globaldata_register: invalid cpuid"));
	cpuid_to_globaldata[globaldata->gd_cpuid] = globaldata;
	SLIST_INSERT_HEAD(&cpuhead, globaldata, gd_allcpu);
}

/*
 * Locate a struct globaldata by cpu id.
 */
struct globaldata *
globaldata_find(u_int cpuid)
{

	return (cpuid_to_globaldata[cpuid]);
}
