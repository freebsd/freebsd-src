/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/atomic.h>
#include <machine/globaldata.h>
#include <machine/pal.h>
#include <machine/pmap.h>
#include <machine/clock.h>

static void ipi_send(u_int64_t, u_int64_t);

u_int64_t	cpu_to_lid[MAXCPU];
int		ncpus;

int
cpu_mp_probe()
{
	all_cpus = 1;	/* Needed for MB init code */
	return (0);
}

void
cpu_mp_start()
{
}

void
cpu_mp_announce()
{
}

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(u_int cpus, u_int64_t ipi)
{
	u_int mask;
	int cpu;

	for (mask = 1, cpu = 0; cpu < ncpus; mask <<= 1, cpu++) {
		if (cpus & mask)
			ipi_send(cpu_to_lid[cpu], ipi);
	}
}

/*
 * send an IPI INTerrupt containing 'vector' to all CPUs, including myself
 */
void
ipi_all(u_int64_t ipi)
{
	int cpu;

	for (cpu = 0; cpu < ncpus; cpu++)
		ipi_send(cpu_to_lid[cpu], ipi);
}

/*
 * send an IPI to all CPUs EXCEPT myself
 */
void
ipi_all_but_self(u_int64_t ipi)
{
	u_int64_t lid;
	int cpu;

	for (cpu = 0; cpu < ncpus; cpu++) {
		lid = cpu_to_lid[cpu];
		if (lid != ia64_get_lid())
			ipi_send(lid, ipi);
	}
}

/*
 * send an IPI to myself
 */
void
ipi_self(u_int64_t ipi)
{

	ipi_send(ia64_get_lid(), ipi);
}

/*
 * Send an IPI to the specified processor. The lid parameter holds the
 * cr.lid (CR64) contents of the target processor. Only the id and eid
 * fields are used here.
 */
static void
ipi_send(u_int64_t lid, u_int64_t ipi)
{
	volatile u_int64_t *pipi;

	CTR2(KTR_SMP, __func__ ": lid=%lx, ipi=%lx", lid, ipi);
	pipi = ia64_memory_address(PAL_PIB_DEFAULT_ADDR |
	    ((lid >> 12) & 0xFFFF0L));
	*pipi = ipi & 0xFF;
}
