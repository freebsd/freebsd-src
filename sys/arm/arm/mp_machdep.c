/*-
 * Copyright (c) 2011 Semihalf.
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
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <sys/smp.h>

#include <machine/smp.h>

/* used to hold the AP's until we are ready to release them */
static struct mtx ap_boot_mtx;

/* Determine if we running MP machine */
int
cpu_mp_probe(void)
{
	CPU_SETOF(0, &all_cpus);

	return (mp_ncpus > 1);
}

/* Initialize and fire up non-boot processors */
void
cpu_mp_start(void)
{
	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	return;
}

/* Introduce rest of cores to the world */
void
cpu_mp_announce(void)
{
	return;
}

struct cpu_group *
cpu_topo(void)
{
	return (smp_topo_1level(CG_SHARE_L2, 4, 0));
}

void
cpu_mp_setmaxid(void)
{
	mp_ncpus = 4;
	mp_maxid = 3;
}

/* Sending IPI */
void
ipi_all_but_self(u_int ipi)
{
	return;
}

void
ipi_cpu(int cpu, u_int ipi)
{
	return;
}

void
ipi_selected(cpuset_t cpus, u_int ipi)
{
	return;
}

