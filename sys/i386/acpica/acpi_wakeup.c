/*-
 * Copyright (c) 2001 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2001 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
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
__FBSDID("$FreeBSD: src/sys/i386/acpica/acpi_wakeup.c,v 1.46.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/intr_machdep.h>
#include <machine/segments.h>

#include <contrib/dev/acpica/acpi.h>
#include <dev/acpica/acpivar.h>

#include "acpi_wakecode.h"

/* Make sure the code is less than one page and leave room for the stack. */
CTASSERT(sizeof(wakecode) < PAGE_SIZE - 1024);

#ifndef _SYS_CDEFS_H_
#error this file needs sys/cdefs.h as a prerequisite
#endif

extern uint32_t	acpi_resume_beep;
extern uint32_t	acpi_reset_video;
extern void	initializecpu(void);

static struct region_descriptor	saved_idt, saved_gdt, *p_gdt;
static uint16_t		saved_ldt;

static uint32_t		r_eax, r_ebx, r_ecx, r_edx, r_ebp, r_esi, r_edi,
			r_efl, r_cr0, r_cr2, r_cr3, r_cr4, ret_addr;

static uint16_t		r_cs, r_ds, r_es, r_fs, r_gs, r_ss, r_tr;
static uint32_t		r_esp;

static void		acpi_printcpu(void);
static void		acpi_realmodeinst(void *arg, bus_dma_segment_t *segs,
					  int nsegs, int error);
static void		acpi_alloc_wakeup_handler(void);

/* XXX shut gcc up */
extern int		acpi_savecpu(void);
extern int		acpi_restorecpu(void);

#ifdef __GNUCLIKE_ASM
__asm__("				\n\
	.text				\n\
	.p2align 2, 0x90		\n\
	.type acpi_restorecpu, @function\n\
acpi_restorecpu:			\n\
	.align 4			\n\
	movl	r_eax,%eax		\n\
	movl	r_ebx,%ebx		\n\
	movl	r_ecx,%ecx		\n\
	movl	r_edx,%edx		\n\
	movl	r_ebp,%ebp		\n\
	movl	r_esi,%esi		\n\
	movl	r_edi,%edi		\n\
	movl	r_esp,%esp		\n\
					\n\
	pushl	r_efl			\n\
	popfl				\n\
					\n\
	movl	ret_addr,%eax		\n\
	movl	%eax,(%esp)		\n\
	xorl	%eax,%eax		\n\
	ret				\n\
					\n\
	.text				\n\
	.p2align 2, 0x90		\n\
	.type acpi_savecpu, @function	\n\
acpi_savecpu:				\n\
	movw	%cs,r_cs		\n\
	movw	%ds,r_ds		\n\
	movw	%es,r_es		\n\
	movw	%fs,r_fs		\n\
	movw	%gs,r_gs		\n\
	movw	%ss,r_ss		\n\
					\n\
	movl	%eax,r_eax		\n\
	movl	%ebx,r_ebx		\n\
	movl	%ecx,r_ecx		\n\
	movl	%edx,r_edx		\n\
	movl	%ebp,r_ebp		\n\
	movl	%esi,r_esi		\n\
	movl	%edi,r_edi		\n\
					\n\
	movl	%cr0,%eax		\n\
	movl	%eax,r_cr0		\n\
	movl	%cr2,%eax		\n\
	movl	%eax,r_cr2		\n\
	movl	%cr3,%eax		\n\
	movl	%eax,r_cr3		\n\
	movl	%cr4,%eax		\n\
	movl	%eax,r_cr4		\n\
					\n\
	pushfl				\n\
	popl	r_efl			\n\
					\n\
	movl	%esp,r_esp		\n\
					\n\
	sgdt	saved_gdt		\n\
	sidt	saved_idt		\n\
	sldt	saved_ldt		\n\
	str	r_tr			\n\
					\n\
	movl	(%esp),%eax		\n\
	movl	%eax,ret_addr		\n\
	movl	$1,%eax			\n\
	ret				\n\
");
#endif /* __GNUCLIKE_ASM */

static void
acpi_printcpu(void)
{
	printf("======== acpi_printcpu() debug dump ========\n");
	printf("gdt[%04x:%08x] idt[%04x:%08x] ldt[%04x] tr[%04x] efl[%08x]\n",
		saved_gdt.rd_limit, saved_gdt.rd_base,
		saved_idt.rd_limit, saved_idt.rd_base,
		saved_ldt, r_tr, r_efl);
	printf("eax[%08x] ebx[%08x] ecx[%08x] edx[%08x]\n",
		r_eax, r_ebx, r_ecx, r_edx);
	printf("esi[%08x] edi[%08x] ebp[%08x] esp[%08x]\n",
		r_esi, r_edi, r_ebp, r_esp);
	printf("cr0[%08x] cr2[%08x] cr3[%08x] cr4[%08x]\n",
		r_cr0, r_cr2, r_cr3, r_cr4);
	printf("cs[%04x] ds[%04x] es[%04x] fs[%04x] gs[%04x] ss[%04x]\n",
		r_cs, r_ds, r_es, r_fs, r_gs, r_ss);
}

#define WAKECODE_FIXUP(offset, type, val) do	{		\
	type	*addr;						\
	addr = (type *)(sc->acpi_wakeaddr + offset);		\
	*addr = val;						\
} while (0)

#define WAKECODE_BCOPY(offset, type, val) do	{		\
	void	*addr;						\
	addr = (void *)(sc->acpi_wakeaddr + offset);		\
	bcopy(&(val), addr, sizeof(type));			\
} while (0)

/* Turn off bits 1&2 of the PIT, stopping the beep. */
static void
acpi_stop_beep(void *arg)
{
	outb(0x61, inb(0x61) & ~0x3);
}

int
acpi_sleep_machdep(struct acpi_softc *sc, int state)
{
	ACPI_STATUS		status;
	struct pmap		*pm;
	int			ret;
	uint32_t		cr3;
	u_long			ef;

	ret = 0;
	if (sc->acpi_wakeaddr == 0)
		return (0);

	AcpiSetFirmwareWakingVector(sc->acpi_wakephys);

	ef = read_eflags();

	/*
	 * Temporarily switch to the kernel pmap because it provides an
	 * identity mapping (setup at boot) for the low physical memory
	 * region containing the wakeup code.
	 */
	pm = kernel_pmap;
	cr3 = rcr3();
#ifdef PAE
	load_cr3(vtophys(pm->pm_pdpt));
#else
	load_cr3(vtophys(pm->pm_pdir));
#endif

	ret_addr = 0;
	ACPI_DISABLE_IRQS();
	if (acpi_savecpu()) {
		/* Execute Sleep */
		intr_suspend();

		p_gdt = (struct region_descriptor *)
				(sc->acpi_wakeaddr + physical_gdt);
		p_gdt->rd_limit = saved_gdt.rd_limit;
		p_gdt->rd_base = vtophys(saved_gdt.rd_base);

		WAKECODE_FIXUP(physical_esp, uint32_t, vtophys(r_esp));
		WAKECODE_FIXUP(previous_cr0, uint32_t, r_cr0);
		WAKECODE_FIXUP(previous_cr2, uint32_t, r_cr2);
		WAKECODE_FIXUP(previous_cr3, uint32_t, r_cr3);
		WAKECODE_FIXUP(previous_cr4, uint32_t, r_cr4);

		WAKECODE_FIXUP(resume_beep, uint32_t, acpi_resume_beep);
		WAKECODE_FIXUP(reset_video, uint32_t, acpi_reset_video);

		WAKECODE_FIXUP(previous_tr,  uint16_t, r_tr);
		WAKECODE_BCOPY(previous_gdt, struct region_descriptor, saved_gdt);
		WAKECODE_FIXUP(previous_ldt, uint16_t, saved_ldt);
		WAKECODE_BCOPY(previous_idt, struct region_descriptor, saved_idt);

		WAKECODE_FIXUP(where_to_recover, void *, acpi_restorecpu);

		WAKECODE_FIXUP(previous_ds,  uint16_t, r_ds);
		WAKECODE_FIXUP(previous_es,  uint16_t, r_es);
		WAKECODE_FIXUP(previous_fs,  uint16_t, r_fs);
		WAKECODE_FIXUP(previous_gs,  uint16_t, r_gs);
		WAKECODE_FIXUP(previous_ss,  uint16_t, r_ss);

		if (bootverbose)
			acpi_printcpu();

		/* Call ACPICA to enter the desired sleep state */
		if (state == ACPI_STATE_S4 && sc->acpi_s4bios)
			status = AcpiEnterSleepStateS4bios();
		else
			status = AcpiEnterSleepState(state);

		if (status != AE_OK) {
			device_printf(sc->acpi_dev,
				"AcpiEnterSleepState failed - %s\n",
				AcpiFormatException(status));
			ret = -1;
			goto out;
		}

		for (;;) ;
	} else {
		/* Execute Wakeup */
		intr_resume();

		if (bootverbose) {
			acpi_savecpu();
			acpi_printcpu();
		}
	}

out:
	load_cr3(cr3);
	write_eflags(ef);

	/* If we beeped, turn it off after a delay. */
	if (acpi_resume_beep)
		timeout(acpi_stop_beep, NULL, 3 * hz);

	return (ret);
}

static bus_dma_tag_t	acpi_waketag;
static bus_dmamap_t	acpi_wakemap;
static vm_offset_t	acpi_wakeaddr;

static void
acpi_alloc_wakeup_handler(void)
{
	void *wakeaddr;

	if (!cold)
		return;

	/*
	 * Specify the region for our wakeup code.  We want it in the low 1 MB
	 * region, excluding video memory and above (0xa0000).  We ask for
	 * it to be page-aligned, just to be safe.
	 */
	if (bus_dma_tag_create(/*parent*/ NULL,
	    /*alignment*/ PAGE_SIZE, /*no boundary*/ 0,
	    /*lowaddr*/ 0x9ffff, /*highaddr*/ BUS_SPACE_MAXADDR, NULL, NULL,
	    /*maxsize*/ PAGE_SIZE, /*segments*/ 1, /*maxsegsize*/ PAGE_SIZE,
	    0, busdma_lock_mutex, &Giant, &acpi_waketag) != 0) {
		printf("acpi_alloc_wakeup_handler: can't create wake tag\n");
		return;
	}
	if (bus_dmamem_alloc(acpi_waketag, &wakeaddr, BUS_DMA_NOWAIT,
	    &acpi_wakemap) != 0) {
		printf("acpi_alloc_wakeup_handler: can't alloc wake memory\n");
		return;
	}
	acpi_wakeaddr = (vm_offset_t)wakeaddr;
}

SYSINIT(acpiwakeup, SI_SUB_KMEM, SI_ORDER_ANY, acpi_alloc_wakeup_handler, 0);

static void
acpi_realmodeinst(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct acpi_softc *sc;
	uint32_t *addr;

	/* Overwrite the ljmp target with the real address */
	sc = arg;
	sc->acpi_wakephys = segs[0].ds_addr;
	addr = (uint32_t *)&wakecode[wakeup_sw32 + 2];
	*addr = sc->acpi_wakephys + wakeup_32;

	/* Copy the wake code into our low page and save its physical addr. */
	bcopy(wakecode, (void *)sc->acpi_wakeaddr, sizeof(wakecode));
	if (bootverbose) {
		device_printf(sc->acpi_dev, "wakeup code va %#x pa %#jx\n",
		    acpi_wakeaddr, (uintmax_t)sc->acpi_wakephys);
	}
}

void
acpi_install_wakeup_handler(struct acpi_softc *sc)
{
	if (acpi_wakeaddr == 0)
		return;

	sc->acpi_waketag = acpi_waketag;
	sc->acpi_wakeaddr = acpi_wakeaddr;
	sc->acpi_wakemap = acpi_wakemap;

	bus_dmamap_load(sc->acpi_waketag, sc->acpi_wakemap,
	    (void *)sc->acpi_wakeaddr, PAGE_SIZE, acpi_realmodeinst, sc, 0);
}
