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
#include <machine/sal.h>

#define	LID_SAPIC_ID(x)		((int)((x) >> 24) & 0xff)
#define	LID_SAPIC_EID(x)	((int)((x) >> 16) & 0xff)

static MALLOC_DEFINE(M_SMP, "smp", "SMP structures");

static void ipi_send(u_int64_t, int);
static void cpu_mp_unleash(void *);

struct mp_cpu {
	TAILQ_ENTRY(mp_cpu) cpu_next;
	u_int64_t	cpu_lid;	/* Local processor ID */
	int32_t		cpu_no;		/* Sequential CPU number */
	u_int32_t	cpu_bsp:1;	/* 1=BSP; 0=AP */
	u_int32_t	cpu_awake:1;	/* 1=Awake; 0=sleeping */
	void		*cpu_stack;
};

int	mp_hardware = 0;
int	mp_ipi_vector[IPI_COUNT];
int	mp_ipi_test = 0;

/* Variables used by os_boot_rendez */
volatile void *ap_stack;
volatile int ap_delay;
volatile int ap_awake;

TAILQ_HEAD(, mp_cpu) ia64_cpus = TAILQ_HEAD_INITIALIZER(ia64_cpus);

void
ia64_ap_startup(void)
{
#if 0
	struct mp_cpu *cpu;
	u_int64_t lid = ia64_get_lid() & 0xffff0000L;
#endif

	ap_awake = 1;
	ap_delay = 0;
	while (1);

#if 0
	TAILQ_FOREACH(cpu, &ia64_cpus, cpu_next) {
		if (cpu->cpu_lid == lid)
			break;
	}

	KASSERT(cpu != NULL, ("foo!"));

	cpu->cpu_lid = ia64_get_lid();
	cpu->cpu_awake = 1;
#endif
}

int
cpu_mp_probe()
{
	/*
	 * We've already discovered any APs when they're present.
	 * Just return the result here.
	 */
	return (mp_hardware && mp_ncpus > 1);
}

void
cpu_mp_add(uint acpiid, uint apicid, uint apiceid)
{
	struct mp_cpu *cpu;
	u_int64_t bsp = ia64_get_lid() & 0xffff0000L;

	cpu = malloc(sizeof(*cpu), M_SMP, M_WAITOK|M_ZERO);
	if (cpu == NULL)
		return;

	TAILQ_INSERT_TAIL(&ia64_cpus, cpu, cpu_next);
	cpu->cpu_no = acpiid;
	cpu->cpu_lid = ((apicid & 0xff) << 8 | (apiceid & 0xff)) << 16;
	if (cpu->cpu_lid == bsp)
		cpu->cpu_bsp = 1;
	all_cpus |= (1 << acpiid);
	mp_ncpus++;
}

void
cpu_mp_announce()
{
	struct mp_cpu *cpu;

	TAILQ_FOREACH(cpu, &ia64_cpus, cpu_next) {
		printf("cpu%d: SAPIC Id=%x, SAPIC Eid=%x", cpu->cpu_no,
		    LID_SAPIC_ID(cpu->cpu_lid), LID_SAPIC_EID(cpu->cpu_lid));
		if (cpu->cpu_bsp)
			printf(" (BSP)\n");
		else
			printf("\n");
	}
}

void
cpu_mp_start()
{
	struct mp_cpu *cpu;

	TAILQ_FOREACH(cpu, &ia64_cpus, cpu_next) {
		if (!cpu->cpu_bsp) {
			cpu->cpu_stack = malloc(KSTACK_PAGES * PAGE_SIZE,
			    M_SMP, M_WAITOK);

			if (bootverbose)
				printf("SMP: waking up cpu%d\n", cpu->cpu_no);

			ap_stack = cpu->cpu_stack;
			ap_delay = 2000;
			ap_awake = 0;
			ipi_send(cpu->cpu_lid, IPI_AP_WAKEUP);

			do {
				DELAY(1000);
			} while (--ap_delay > 0);
			cpu->cpu_awake = (ap_awake) ? 1 : 0;

			if (bootverbose && !ap_awake)
				printf("SMP: WARNING: cpu%d did not wake up\n",
				    cpu->cpu_no);
		} else {
			cpu->cpu_lid = ia64_get_lid();
			cpu->cpu_awake = 1;
			ipi_self(IPI_TEST);
		}
	}
}

static void
cpu_mp_unleash(void *dummy)
{
	struct mp_cpu *cpu;
	int awake = 0;

	if (!mp_hardware)
		return;

	if (mp_ipi_test != 1)
		printf("SMP: sending of a test IPI to BSP failed\n");

	TAILQ_FOREACH(cpu, &ia64_cpus, cpu_next) {
		awake += cpu->cpu_awake;
	}

	if (awake != mp_ncpus)
		printf("SMP: %d CPU(s) didn't get woken\n", mp_ncpus - awake);
}

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(u_int64_t cpus, int ipi)
{
	struct mp_cpu *cpu;

	TAILQ_FOREACH(cpu, &ia64_cpus, cpu_next) {
		if (cpus & (1 << cpu->cpu_no))
			ipi_send(cpu->cpu_lid, ipi);
	}
}

/*
 * send an IPI to all CPUs, including myself.
 */
void
ipi_all(int ipi)
{
	struct mp_cpu *cpu;

	TAILQ_FOREACH(cpu, &ia64_cpus, cpu_next) {
		ipi_send(cpu->cpu_lid, ipi);
	}
}

/*
 * send an IPI to all CPUs EXCEPT myself.
 */
void
ipi_all_but_self(int ipi)
{
	struct mp_cpu *cpu;
	u_int64_t lid = ia64_get_lid();

	TAILQ_FOREACH(cpu, &ia64_cpus, cpu_next) {
		if (cpu->cpu_lid != lid)
			ipi_send(cpu->cpu_lid, ipi);
	}
}

/*
 * send an IPI to myself.
 */
void
ipi_self(int ipi)
{

	ipi_send(ia64_get_lid(), ipi);
}

/*
 * Send an IPI to the specified processor. The lid parameter holds the
 * cr.lid (CR64) contents of the target processor. Only the id and eid
 * fields are used here.
 */
static void
ipi_send(u_int64_t lid, int ipi)
{
	volatile u_int64_t *pipi;
	u_int64_t vector;

	pipi = ia64_memory_address(PAL_PIB_DEFAULT_ADDR |
	    ((lid >> 12) & 0xFFFF0L));
	vector = (u_int64_t)(mp_ipi_vector[ipi] & 0xff);
	*pipi = vector;
	ia64_mf_a();
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, cpu_mp_unleash, NULL);
