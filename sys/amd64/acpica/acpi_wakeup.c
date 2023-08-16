/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2001-2012 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2003 Peter Wemm
 * Copyright (c) 2008-2012 Jung-uk Kim <jkim@FreeBSD.org>
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
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <sys/cons.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <x86/mca.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>
#include <x86/ucode.h>

#include <x86/apicreg.h>
#include <x86/apicvar.h>
#ifdef SMP
#include <machine/smp.h>
#include <machine/vmparam.h>
#endif

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>

#include "acpi_wakecode.h"
#include "acpi_wakedata.h"

/* Make sure the code is less than a page and leave room for the stack. */
CTASSERT(sizeof(wakecode) < PAGE_SIZE - 1024);

extern int		acpi_resume_beep;
extern int		acpi_reset_video;
extern int		acpi_susp_bounce;

#ifdef SMP
extern struct susppcb	**susppcbs;
static cpuset_t		suspcpus;
#else
static struct susppcb	**susppcbs;
#endif

static void		acpi_stop_beep(void *);

#ifdef SMP
static int		acpi_wakeup_ap(struct acpi_softc *, int);
static void		acpi_wakeup_cpus(struct acpi_softc *);
#endif

#define	ACPI_WAKEPT_PAGES	7

#define	WAKECODE_FIXUP(offset, type, val)	do {	\
	type	*addr;					\
	addr = (type *)(sc->acpi_wakeaddr + (offset));	\
	*addr = val;					\
} while (0)

static void
acpi_stop_beep(void *arg)
{

	if (acpi_resume_beep != 0)
		timer_spkr_release();
}

#ifdef SMP
static int
acpi_wakeup_ap(struct acpi_softc *sc, int cpu)
{
	struct pcb *pcb;
	int		vector = (sc->acpi_wakephys >> 12) & 0xff;
	int		apic_id = cpu_apic_ids[cpu];
	int		ms;

	pcb = &susppcbs[cpu]->sp_pcb;
	WAKECODE_FIXUP(wakeup_pcb, struct pcb *, pcb);
	WAKECODE_FIXUP(wakeup_gdt, uint16_t, pcb->pcb_gdt.rd_limit);
	WAKECODE_FIXUP(wakeup_gdt + 2, uint64_t, pcb->pcb_gdt.rd_base);

	ipi_startup(apic_id, vector);

	/* Wait up to 5 seconds for it to resume. */
	for (ms = 0; ms < 5000; ms++) {
		if (!CPU_ISSET(cpu, &suspended_cpus))
			return (1);	/* return SUCCESS */
		DELAY(1000);
	}
	return (0);		/* return FAILURE */
}

#define	WARMBOOT_TARGET		0
#define	WARMBOOT_OFF		(KERNBASE + 0x0467)
#define	WARMBOOT_SEG		(KERNBASE + 0x0469)

#define	CMOS_REG		(0x70)
#define	CMOS_DATA		(0x71)
#define	BIOS_RESET		(0x0f)
#define	BIOS_WARM		(0x0a)

static void
acpi_wakeup_cpus(struct acpi_softc *sc)
{
	uint32_t	mpbioswarmvec;
	int		cpu;
	u_char		mpbiosreason;

	if (!efi_boot) {
		/* save the current value of the warm-start vector */
		mpbioswarmvec = *((uint32_t *)WARMBOOT_OFF);
		outb(CMOS_REG, BIOS_RESET);
		mpbiosreason = inb(CMOS_DATA);

		/* setup a vector to our boot code */
		*((volatile u_short *)WARMBOOT_OFF) = WARMBOOT_TARGET;
		*((volatile u_short *)WARMBOOT_SEG) = sc->acpi_wakephys >> 4;
		outb(CMOS_REG, BIOS_RESET);
		outb(CMOS_DATA, BIOS_WARM);	/* 'warm-start' */
	}

	/* Wake up each AP. */
	for (cpu = 1; cpu < mp_ncpus; cpu++) {
		if (!CPU_ISSET(cpu, &suspcpus))
			continue;
		if (acpi_wakeup_ap(sc, cpu) == 0) {
			/* restore the warmstart vector */
			*(uint32_t *)WARMBOOT_OFF = mpbioswarmvec;
			panic("acpi_wakeup: failed to resume AP #%d (PHY #%d)",
			    cpu, cpu_apic_ids[cpu]);
		}
	}

	if (!efi_boot) {
		/* restore the warmstart vector */
		*(uint32_t *)WARMBOOT_OFF = mpbioswarmvec;

		outb(CMOS_REG, BIOS_RESET);
		outb(CMOS_DATA, mpbiosreason);
	}
}
#endif

int
acpi_sleep_machdep(struct acpi_softc *sc, int state)
{
	ACPI_STATUS	status;
	struct pcb	*pcb;
	struct pcpu *pc;
	int i;

	if (sc->acpi_wakeaddr == 0ul)
		return (-1);	/* couldn't alloc wake memory */

#ifdef SMP
	suspcpus = all_cpus;
	CPU_CLR(PCPU_GET(cpuid), &suspcpus);
#endif

	if (acpi_resume_beep != 0)
		timer_spkr_acquire();

	AcpiSetFirmwareWakingVector(sc->acpi_wakephys, 0);

	intr_suspend();

	pcb = &susppcbs[0]->sp_pcb;
	if (savectx(pcb)) {
		fpususpend(susppcbs[0]->sp_fpususpend);
#ifdef SMP
		if (!CPU_EMPTY(&suspcpus) && suspend_cpus(suspcpus) == 0) {
			device_printf(sc->acpi_dev, "Failed to suspend APs\n");
			return (0);	/* couldn't sleep */
		}
#endif
		hw_ibrs_ibpb_active = 0;
		hw_ssb_active = 0;
		cpu_stdext_feature3 = 0;
		CPU_FOREACH(i) {
			pc = pcpu_find(i);
			pc->pc_ibpb_set = 0;
		}

		WAKECODE_FIXUP(resume_beep, uint8_t, (acpi_resume_beep != 0));
		WAKECODE_FIXUP(reset_video, uint8_t, (acpi_reset_video != 0));

		WAKECODE_FIXUP(wakeup_efer, uint64_t, rdmsr(MSR_EFER) &
		    ~(EFER_LMA));
		WAKECODE_FIXUP(wakeup_pcb, struct pcb *, pcb);
		WAKECODE_FIXUP(wakeup_gdt, uint16_t, pcb->pcb_gdt.rd_limit);
		WAKECODE_FIXUP(wakeup_gdt + 2, uint64_t, pcb->pcb_gdt.rd_base);

		/* Call ACPICA to enter the desired sleep state */
		if (state == ACPI_STATE_S4 && sc->acpi_s4bios)
			status = AcpiEnterSleepStateS4bios();
		else
			status = AcpiEnterSleepState(state);
		if (ACPI_FAILURE(status)) {
			device_printf(sc->acpi_dev,
			    "AcpiEnterSleepState failed - %s\n",
			    AcpiFormatException(status));
			return (0);	/* couldn't sleep */
		}

		if (acpi_susp_bounce)
			resumectx(pcb);

		for (;;)
			ia32_pause();
	} else {
		/*
		 * Re-initialize console hardware as soon as possible.
		 * No console output (e.g. printf) is allowed before
		 * this point.
		 */
		cnresume();
		fpuresume(susppcbs[0]->sp_fpususpend);
	}

	return (1);	/* wakeup successfully */
}

int
acpi_wakeup_machdep(struct acpi_softc *sc, int state, int sleep_result,
    int intr_enabled)
{

	if (sleep_result == -1)
		return (sleep_result);

	if (!intr_enabled) {
		/* Wakeup MD procedures in interrupt disabled context */
		if (sleep_result == 1) {
			ucode_reload();
			pmap_init_pat();
			initializecpu();
			PCPU_SET(switchtime, 0);
			PCPU_SET(switchticks, ticks);
			lapic_xapic_mode();
#ifdef SMP
			if (!CPU_EMPTY(&suspcpus))
				acpi_wakeup_cpus(sc);
#endif
		}

#ifdef SMP
		if (!CPU_EMPTY(&suspcpus))
			resume_cpus(suspcpus);
#endif

		/*
		 * Re-read cpu_stdext_feature3, which was zeroed-out
		 * in acpi_sleep_machdep(), after the microcode was
		 * reloaded.  Then recalculate the active mitigation
		 * knobs that depend on the microcode and
		 * cpu_stdext_feature3.  Do it after LAPICs are woken,
		 * so that IPIs work.
		 */
		identify_cpu_ext_features();

		mca_resume();
		if (vmm_resume_p != NULL)
			vmm_resume_p();
		intr_resume(/*suspend_cancelled*/false);

		hw_ibrs_recalculate(true);
		amd64_syscall_ret_flush_l1d_recalc();
		hw_ssb_recalculate(true);
		x86_rngds_mitg_recalculate(true);

		AcpiSetFirmwareWakingVector(0, 0);
	} else {
		/* Wakeup MD procedures in interrupt enabled context */
		if (sleep_result == 1 && mem_range_softc.mr_op != NULL &&
		    mem_range_softc.mr_op->reinit != NULL)
			mem_range_softc.mr_op->reinit(&mem_range_softc);
	}

	return (sleep_result);
}

static void
acpi_alloc_wakeup_handler(void **wakeaddr,
    void *wakept_pages[ACPI_WAKEPT_PAGES])
{
	vm_page_t wakept_m[ACPI_WAKEPT_PAGES];
	int i;

	*wakeaddr = NULL;
	memset(wakept_pages, 0, ACPI_WAKEPT_PAGES * sizeof(*wakept_pages));
	memset(wakept_m, 0, ACPI_WAKEPT_PAGES * sizeof(*wakept_m));

	/*
	 * Specify the region for our wakeup code.  We want it in the
	 * low 1 MB region, excluding real mode IVT (0-0x3ff), BDA
	 * (0x400-0x4ff), EBDA (less than 128KB, below 0xa0000, must
	 * be excluded by SMAP and DSDT), and ROM area (0xa0000 and
	 * above).
	 */
	*wakeaddr = contigmalloc(PAGE_SIZE, M_DEVBUF,
	    M_NOWAIT, 0x500, 0xa0000, PAGE_SIZE, 0ul);
	if (*wakeaddr == NULL) {
		printf("%s: can't alloc wake memory\n", __func__);
		goto freepages;
	}

	for (i = 0; i < ACPI_WAKEPT_PAGES - (la57 ? 0 : 1); i++) {
		wakept_m[i] = pmap_page_alloc_below_4g(true);
		wakept_pages[i] = (void *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(
		    wakept_m[i]));
	}
	if (EVENTHANDLER_REGISTER(power_resume, acpi_stop_beep, NULL,
	    EVENTHANDLER_PRI_LAST) == NULL) {
		printf("%s: can't register event handler\n", __func__);
		goto freepages;
	}
	susppcbs = malloc(mp_ncpus * sizeof(*susppcbs), M_DEVBUF, M_WAITOK);
	for (i = 0; i < mp_ncpus; i++) {
		susppcbs[i] = malloc(sizeof(**susppcbs), M_DEVBUF, M_WAITOK);
		susppcbs[i]->sp_fpususpend = alloc_fpusave(M_WAITOK);
	}
	return;

freepages:
	if (*wakeaddr != NULL)
		contigfree(*wakeaddr, PAGE_SIZE, M_DEVBUF);
	for (i = 0; i < ACPI_WAKEPT_PAGES; i++) {
		if (wakept_m[i] != NULL)
			vm_page_free(wakept_m[i]);
	}
	*wakeaddr = NULL;
}

void
acpi_install_wakeup_handler(struct acpi_softc *sc)
{
	static void *wakeaddr;
	void *wakept_pages[ACPI_WAKEPT_PAGES];
	uint64_t *pt5, *pt4, *pt3, *pt2_0, *pt2_1, *pt2_2, *pt2_3;
	vm_paddr_t pt5pa, pt4pa, pt3pa, pt2_0pa, pt2_1pa, pt2_2pa, pt2_3pa;
	int i;

	if (wakeaddr != NULL)
		return;
	acpi_alloc_wakeup_handler(&wakeaddr, wakept_pages);
	if (wakeaddr == NULL)
		return;

	sc->acpi_wakeaddr = (vm_offset_t)wakeaddr;
	sc->acpi_wakephys = vtophys(wakeaddr);

	if (la57) {
		pt5 = wakept_pages[6];
		pt5pa = vtophys(pt5);
	}
	pt4 = wakept_pages[0];
	pt3 = wakept_pages[1];
	pt2_0 = wakept_pages[2];
	pt2_1 = wakept_pages[3];
	pt2_2 = wakept_pages[4];
	pt2_3 = wakept_pages[5];
	pt4pa = vtophys(pt4);
	pt3pa = vtophys(pt3);
	pt2_0pa = vtophys(pt2_0);
	pt2_1pa = vtophys(pt2_1);
	pt2_2pa = vtophys(pt2_2);
	pt2_3pa = vtophys(pt2_3);

	bcopy(wakecode, (void *)sc->acpi_wakeaddr, sizeof(wakecode));

	/* Patch GDT base address, ljmp targets. */
	WAKECODE_FIXUP((bootgdtdesc + 2), uint32_t,
	    sc->acpi_wakephys + bootgdt);
	WAKECODE_FIXUP((wakeup_sw32 + 2), uint32_t,
	    sc->acpi_wakephys + wakeup_32);
	WAKECODE_FIXUP((wakeup_sw64 + 1), uint32_t,
	    sc->acpi_wakephys + wakeup_64);
	WAKECODE_FIXUP(wakeup_pagetables, uint32_t, la57 ? (pt5pa | 0x1) :
	    pt4pa);

	/* Save pointers to some global data. */
	WAKECODE_FIXUP(wakeup_ret, void *, resumectx);
	/* Create 1:1 mapping for the low 4G */
	if (la57) {
		bcopy(kernel_pmap->pm_pmltop, pt5, PAGE_SIZE);
		pt5[0] = (uint64_t)pt4pa;
		pt5[0] |= PG_V | PG_RW | PG_U;
	} else {
		bcopy(kernel_pmap->pm_pmltop, pt4, PAGE_SIZE);
	}

	pt4[0] = (uint64_t)pt3pa;
	pt4[0] |= PG_V | PG_RW | PG_U;

	pt3[0] = (uint64_t)pt2_0pa;
	pt3[0] |= PG_V | PG_RW | PG_U;
	pt3[1] = (uint64_t)pt2_1pa;
	pt3[1] |= PG_V | PG_RW | PG_U;
	pt3[2] = (uint64_t)pt2_2pa;
	pt3[2] |= PG_V | PG_RW | PG_U;
	pt3[3] = (uint64_t)pt2_3pa;
	pt3[3] |= PG_V | PG_RW | PG_U;

	for (i = 0; i < NPDEPG; i++) {
		pt2_0[i] = (pd_entry_t)i * NBPDR;
		pt2_0[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}
	for (i = 0; i < NPDEPG; i++) {
		pt2_1[i] = (pd_entry_t)NBPDP + i * NBPDR;
		pt2_1[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}
	for (i = 0; i < NPDEPG; i++) {
		pt2_2[i] = (pd_entry_t)2 * NBPDP + i * NBPDR;
		pt2_2[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}
	for (i = 0; i < NPDEPG; i++) {
		pt2_3[i] = (pd_entry_t)3 * NBPDP + i * NBPDR;
		pt2_3[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}

	if (bootverbose)
		device_printf(sc->acpi_dev, "wakeup code va %#jx pa %#jx\n",
		    (uintmax_t)sc->acpi_wakeaddr, (uintmax_t)sc->acpi_wakephys);
}
