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
#include <sys/proc.h>
#include <sys/pcpu.h>
#include <sys/sched.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <machine/smp.h>

extern struct pcpu __pcpu[];

/* used to hold the AP's until we are ready to release them */
static struct mtx ap_boot_mtx;

/* # of Applications processors */
int mp_naps;

/* Set to 1 once we're ready to let the APs out of the pen. */
static volatile int aps_ready = 0;

/* Temporary variables for init_secondary()  */
void *dpcpu;


/* Determine if we running MP machine */
int
cpu_mp_probe(void)
{
	CPU_SETOF(0, &all_cpus);

	return (platform_mp_probe());
}

/* Start Application Processor via platform specific function */
static int
start_ap(int cpu)
{
	int cpus, ms;

	cpus = mp_naps;

	if (platform_mp_start_ap(cpu) != 0)
		return (-1);			/* could not start AP */

	for (ms = 0; ms < 5000; ++ms) {
		if (mp_naps > cpus)
			return (0);		/* success */
		else
			DELAY(1000);
	}

	return (-2);
}


/* Initialize and fire up non-boot processors */
void
cpu_mp_start(void)
{
	int error, i;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	for (i = 1; i < mp_maxid; i++) {
		error = start_ap(i);
		if (error) {
			printf("AP #%d failed to start\n", i);
			continue;
		}
		CPU_SET(i, &all_cpus);
	}


	return;
}

/* Introduce rest of cores to the world */
void
cpu_mp_announce(void)
{
	return;
}

void
init_secondary(int cpu)
{
	struct pcpu *pc;
	void *dpcpu;

	/* Per-cpu initialization */
	pc = &__pcpu[cpu];
	pcpu_init(pc, cpu, sizeof(struct pcpu));

	dpcpu = (void *)kmem_alloc(kernel_map, DPCPU_SIZE);
	dpcpu_init(dpcpu, cpu);

	/* Signal our startup to BSP */
	mp_naps++;

	/* Spin until the BSP releases the APs */
	while (!aps_ready)
		;

	/* Initialize curthread */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	PCPU_SET(curthread, PCPU_GET(idlethread));


	mtx_lock_spin(&ap_boot_mtx);

	printf("SMP: AP CPU #%d Launched!\n", cpu);

	smp_cpus++;

	if (smp_cpus == mp_ncpus) {
		/* enable IPI's, tlb shootdown, freezes etc */
		atomic_store_rel_int(&smp_started, 1);
		/*
		 * XXX do we really need it
		 * smp_active = 1;
		 */
	}

	mtx_unlock_spin(&ap_boot_mtx);

	while (smp_started == 0)
		;

	/* Start per-CPU event timers. */
	cpu_initclocks_ap();

	/* Enter the scheduler */
	sched_throw(NULL);

	panic("scheduler returned us to %s", __func__);
	/* NOTREACHED */
}

struct cpu_group *
cpu_topo(void)
{
	return (smp_topo_1level(CG_SHARE_L2, 4, 0));
}

void
cpu_mp_setmaxid(void)
{

	platform_mp_setmaxid();
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

