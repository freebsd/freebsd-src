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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/cons.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>

#include <machine/atomic.h>
#include <machine/pmap.h>
#include <machine/rpb.h>
#include <machine/clock.h>
#include <machine/prom.h>
#include <machine/smp.h>

/* Set to 1 once we're ready to let the APs out of the pen. */
static volatile int aps_ready = 0;

static struct mtx ap_boot_mtx;

u_int boot_cpu_id;

static void	release_aps(void *dummy);
extern void	smp_init_secondary_glue(void);
static int	smp_send_secondary_command(const char *command, int cpuid);
static int	smp_start_secondary(int cpuid);

/*
 * Communicate with a console running on a secondary processor.
 * Return 1 on failure.
 */
static int
smp_send_secondary_command(const char *command, int cpuid)
{
	u_int64_t mask = 1L << cpuid;
	struct pcs *cpu = LOCATE_PCS(hwrpb, cpuid);
	int i, len;

	/*
	 * Sanity check.
	 */
	len = strlen(command);
	if (len > sizeof(cpu->pcs_buffer.rxbuf)) {
		printf("smp_send_secondary_command: command '%s' too long\n",
		       command);
		return 0;
	}

	/*
	 * Wait for the rx bit to clear.
	 */
	for (i = 0; i < 100000; i++) {
		if (!(hwrpb->rpb_rxrdy & mask))
			break;
		DELAY(10);
	}
	if (hwrpb->rpb_rxrdy & mask)
		return 0;

	/*
	 * Write the command into the processor's buffer.
	 */
	bcopy(command, cpu->pcs_buffer.rxbuf, len);
	cpu->pcs_buffer.rxlen = len;

	/*
	 * Set the bit in the rxrdy mask and let the secondary try to
	 * handle the command.
	 */
	atomic_set_64(&hwrpb->rpb_rxrdy, mask);

	/*
	 * Wait for the rx bit to clear.
	 */
	for (i = 0; i < 100000; i++) {
		if (!(hwrpb->rpb_rxrdy & mask))
			break;
		DELAY(10);
	}
	if (hwrpb->rpb_rxrdy & mask)
		return 0;

	return 1;
}

void
smp_init_secondary(void)
{
	struct pcs *cpu;

	/* spin until all the AP's are ready */
	while (!aps_ready)
		/*spin*/ ;
 
	/*
	 * Record the pcpu pointer in the per-cpu system value.
	 */
	alpha_pal_wrval((u_int64_t) pcpup);

	/* Clear userland thread pointer. */
	alpha_pal_wrunique(0);

	/*
	 * Point interrupt/exception vectors to our own.
	 */
	alpha_pal_wrent(XentInt, ALPHA_KENTRY_INT);
	alpha_pal_wrent(XentArith, ALPHA_KENTRY_ARITH);
	alpha_pal_wrent(XentMM, ALPHA_KENTRY_MM);
	alpha_pal_wrent(XentIF, ALPHA_KENTRY_IF);
	alpha_pal_wrent(XentUna, ALPHA_KENTRY_UNA);
	alpha_pal_wrent(XentSys, ALPHA_KENTRY_SYS);


	/* lower the ipl and take any pending machine check */
	mc_expected = 1;
	alpha_mb(); alpha_mb();
	alpha_pal_wrmces(7);
	(void)alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH);
	mc_expected = 0;

	/*
	 * Set flags in our per-CPU slot in the HWRPB.
	 */
	cpu = LOCATE_PCS(hwrpb, PCPU_GET(cpuid));
	cpu->pcs_flags &= ~PCS_BIP;
	cpu->pcs_flags |= PCS_RC;
	alpha_mb();

	/*
	 * XXX: doesn't idleproc already have a pcb from when it was
	 * kthread_create'd?
	 *
	 * cache idleproc's physical address.
	 */
	curthread->td_md.md_pcbpaddr = (struct pcb *)PCPU_GET(idlepcbphys);
	/*
	 * and make idleproc's trapframe pointer point to its
	 * stack pointer for sanity.
	 */
	curthread->td_frame =
	    (struct trapframe *)PCPU_PTR(idlepcb)->apcb_ksp;

	mtx_lock_spin(&ap_boot_mtx);

	smp_cpus++;

	CTR1(KTR_SMP, "SMP: AP CPU #%d Launched", PCPU_GET(cpuid));

	/* Build our map of 'other' CPUs. */
	PCPU_SET(other_cpus, all_cpus & ~PCPU_GET(cpumask));

	printf("SMP: AP CPU #%d Launched!\n", PCPU_GET(cpuid));

	if (smp_cpus == mp_ncpus) {
		smp_started = 1;
		smp_active = 1;
	}

	mtx_unlock_spin(&ap_boot_mtx);

	while (smp_started == 0)
		; /* nothing */

	binuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	/* ok, now grab sched_lock and enter the scheduler */
	mtx_lock_spin(&sched_lock);
	cpu_throw();	/* doesn't return */

	panic("scheduler returned us to %s", __func__);
}

static int
smp_start_secondary(int cpuid)
{
	struct pcs *cpu = LOCATE_PCS(hwrpb, cpuid);
	struct pcs *bootcpu = LOCATE_PCS(hwrpb, boot_cpu_id);
	struct alpha_pcb *pcb = (struct alpha_pcb *) cpu->pcs_hwpcb;
	struct pcpu *pcpu;
	int i;
	size_t sz;

	if ((cpu->pcs_flags & PCS_PV) == 0) {
		printf("smp_start_secondary: cpu %d PALcode invalid\n", cpuid);
		return 0;
	}

	if (bootverbose)
		printf("smp_start_secondary: starting cpu %d\n", cpuid);

	sz = round_page((UAREA_PAGES + KSTACK_PAGES) * PAGE_SIZE);
	pcpu = malloc(sz, M_TEMP, M_NOWAIT);
	if (!pcpu) {
		printf("smp_start_secondary: can't allocate memory\n");
		return 0;
	}
	
	pcpu_init(pcpu, cpuid, sz);

	/*
	 * Copy the idle pcb and setup the address to start executing.
	 * Use the pcb unique value to point the secondary at its pcpu
	 * structure.
	 */
	*pcb = pcpu->pc_idlepcb;
	pcb->apcb_unique = (u_int64_t)pcpu;
	hwrpb->rpb_restart = (u_int64_t) smp_init_secondary_glue;
	hwrpb->rpb_restart_val = (u_int64_t) smp_init_secondary_glue;
	hwrpb->rpb_checksum = hwrpb_checksum();

	/*
	 * Tell the cpu to start with the same PALcode as us.
	 */
	bcopy(&bootcpu->pcs_pal_rev, &cpu->pcs_pal_rev,
	      sizeof cpu->pcs_pal_rev);

	/*
	 * Set flags in cpu structure and push out write buffers to
	 * make sure the secondary sees it.
	 */
	cpu->pcs_flags |= PCS_CV|PCS_RC;
	cpu->pcs_flags &= ~PCS_BIP;
	alpha_mb();

	/*
	 * Fire it up and hope for the best.
	 */
	if (!smp_send_secondary_command("START\r\n", cpuid)) {
		printf("smp_start_secondary: can't send START command\n");
		pcpu_destroy(pcpu);
		free(pcpu, M_TEMP);
		return 0;
	}

	/*
	 * Wait for the secondary to set the BIP flag in its structure.
	 */
	for (i = 0; i < 100000; i++) {
		if (cpu->pcs_flags & PCS_BIP)
			break;
		DELAY(10);
	}
	if (!(cpu->pcs_flags & PCS_BIP)) {
		printf("smp_start_secondary: secondary did not respond\n");
		pcpu_destroy(pcpu);
		free(pcpu, M_TEMP);
		return 0;
	}

	/*
	 * It worked (I think).
	 */
	if (bootverbose)
		printf("smp_start_secondary: cpu %d started\n", cpuid);
	return 1;
}

/* Other stuff */

int
cpu_mp_probe(void)
{
	struct pcs *pcsp;
	int i, cpus;

	/* XXX: Need to check for valid platforms here. */

	boot_cpu_id = PCPU_GET(cpuid);
	KASSERT(boot_cpu_id == hwrpb->rpb_primary_cpu_id,
	    ("cpu_mp_probe() called on non-primary CPU"));
	all_cpus = 1 << boot_cpu_id;

	mp_ncpus = 1;
	mp_maxid = 0;

	/* Make sure we have at least one secondary CPU. */
	cpus = 0;
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		if (i == PCPU_GET(cpuid))
			continue;
		pcsp = (struct pcs *)((char *)hwrpb + hwrpb->rpb_pcs_off +
		    (i * hwrpb->rpb_pcs_size));
		if ((pcsp->pcs_flags & PCS_PP) == 0) {
			continue;
		}
		if ((pcsp->pcs_flags & PCS_PA) == 0) {
			/*
			 * The TurboLaser PCS_PA bit doesn't seem to be set
			 * correctly.
			 */
			if (hwrpb->rpb_type != ST_DEC_21000) 
				continue;
		}
		if ((pcsp->pcs_flags & PCS_PV) == 0) {
			continue;
		}
		if (i > MAXCPU) {
			continue;
		}
		mp_maxid = i;
		cpus++;
	}
	return (cpus);
}

void
cpu_mp_start(void)
{
	int i;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		struct pcs *pcsp;

		if (i == boot_cpu_id)
			continue;
		pcsp = (struct pcs *)((char *)hwrpb + hwrpb->rpb_pcs_off +
		    (i * hwrpb->rpb_pcs_size));
		if ((pcsp->pcs_flags & PCS_PP) == 0)
			continue;
		if ((pcsp->pcs_flags & PCS_PA) == 0) {
			if (hwrpb->rpb_type == ST_DEC_21000)  {
				printf("Ignoring PA bit for CPU %d.\n", i);
			} else {
				if (bootverbose)
					printf("CPU %d not available.\n", i);
				continue;
			}
		}
		if ((pcsp->pcs_flags & PCS_PV) == 0) {
			if (bootverbose)
				printf("CPU %d does not have valid PALcode.\n",
				    i);
			continue;
		}
		if (i > MAXCPU) {
			if (bootverbose) {
				printf("CPU %d not supported.", i);
				printf("  Only %d CPUs supported.\n", MAXCPU);
			}
			continue;
		}
		if (resource_disabled("cpu", i)) {
			printf("CPU %d disabled by loader.\n", i);
			continue;
		}
		all_cpus |= (1 << i);
		mp_ncpus++;
	}
	PCPU_SET(other_cpus, all_cpus & ~(1 << boot_cpu_id));

	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		if (i == boot_cpu_id)
			continue;
		if (all_cpus & (1 << i))
			smp_start_secondary(i);
	}
}

void
cpu_mp_announce(void)
{
}

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(u_int32_t cpus, u_int64_t ipi)
{
	struct pcpu *pcpu;

	CTR2(KTR_SMP, "ipi_selected: cpus: %x ipi: %lx", cpus, ipi);
	alpha_mb();
	while (cpus) {
		int cpuid = ffs(cpus) - 1;
		cpus &= ~(1 << cpuid);

		pcpu = pcpu_find(cpuid);
		if (pcpu) {
			atomic_set_64(&pcpu->pc_pending_ipis, ipi);
			alpha_mb();
			CTR1(KTR_SMP, "calling alpha_pal_wripir(%d)", cpuid);
			alpha_pal_wripir(cpuid);
		}
	}
}

/*
 * send an IPI INTerrupt containing 'vector' to all CPUs, including myself
 */
void
ipi_all(u_int64_t ipi)
{
	ipi_selected(all_cpus, ipi);
}

/*
 * send an IPI to all CPUs EXCEPT myself
 */
void
ipi_all_but_self(u_int64_t ipi)
{
	ipi_selected(PCPU_GET(other_cpus), ipi);
}

/*
 * send an IPI to myself
 */
void
ipi_self(u_int64_t ipi)
{
	ipi_selected(1 << PCPU_GET(cpuid), ipi);
}

/*
 * Handle an IPI sent to this processor.
 */
void
smp_handle_ipi(struct trapframe *frame)
{
	u_int64_t ipis = atomic_readandclear_64(PCPU_PTR(pending_ipis));
	u_int64_t ipi;
	int cpumask;

	cpumask = 1 << PCPU_GET(cpuid);

	CTR1(KTR_SMP, "smp_handle_ipi(), ipis=%lx", ipis);
	while (ipis) {
		/*
		 * Find the lowest set bit.
		 */
		ipi = ipis & ~(ipis - 1);
		ipis &= ~ipi;
		switch (ipi) {
		case IPI_INVLTLB:
			CTR0(KTR_SMP, "IPI_NVLTLB");
			ALPHA_TBIA();
			break;

		case IPI_RENDEZVOUS:
			CTR0(KTR_SMP, "IPI_RENDEZVOUS");
			smp_rendezvous_action();
			break;

		case IPI_AST:
			CTR0(KTR_SMP, "IPI_AST");
			break;

		case IPI_STOP:
			CTR0(KTR_SMP, "IPI_STOP");
			atomic_set_int(&stopped_cpus, cpumask);
			while ((started_cpus & cpumask) == 0)
				alpha_mb();
			atomic_clear_int(&started_cpus, cpumask);
			atomic_clear_int(&stopped_cpus, cpumask);
			break;
		}
	}

	/*
	 * Dump console messages to the console.  XXX - we need to handle
	 * requests to provide PALcode to secondaries and to start up new
	 * secondaries that are added to the system on the fly.
	 */
	if (PCPU_GET(cpuid) == boot_cpu_id) {
		u_int cpuid;
		u_int64_t txrdy;
#ifdef DIAGNOSTIC
		struct pcs *cpu;
		char buf[81];
#endif

		alpha_mb();
		while (hwrpb->rpb_txrdy != 0) {
			cpuid = ffs(hwrpb->rpb_txrdy) - 1;
#ifdef DIAGNOSTIC
			cpu = LOCATE_PCS(hwrpb, cpuid);
			bcopy(&cpu->pcs_buffer.txbuf, buf,
			    cpu->pcs_buffer.txlen);
			buf[cpu->pcs_buffer.txlen] = '\0';
			printf("SMP From CPU%d: %s\n", cpuid, buf);
#endif
			do {
				txrdy = hwrpb->rpb_txrdy;
			} while (atomic_cmpset_64(&hwrpb->rpb_txrdy, txrdy,
			    txrdy & ~(1 << cpuid)) == 0);
		}
	}
}

static void
release_aps(void *dummy __unused)
{
	if (bootverbose && mp_ncpus > 1)
		printf("%s: releasing secondary CPUs\n", __func__);
	atomic_store_rel_int(&aps_ready, 1);

	while (mp_ncpus > 1 && smp_started == 0)
		; /* nothing */
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, release_aps, NULL);
