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
 *
 *      $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/segments.h>

#include <i386/isa/intr_machdep.h>

#include "acpi.h"

#include <dev/acpica/acpica_support.h>

#include <dev/acpica/acpivar.h>

#include "acpi_wakecode.h"

extern void initializecpu(void);

static struct region_descriptor	r_idt, r_gdt, *p_gdt;
static u_int16_t	r_ldt;

static u_int32_t	r_eax, r_ebx, r_ecx, r_edx, r_ebp, r_esi, r_edi,
			r_efl, r_cr0, r_cr2, r_cr3, r_cr4, ret_addr;

static u_int16_t	r_cs, r_ds, r_es, r_fs, r_gs, r_ss, r_tr;
static u_int32_t	r_esp = 0;

static void		acpi_printcpu(void);
static void		acpi_realmodeinst(void *arg, bus_dma_segment_t *segs,
					  int nsegs, int error);
static void		acpi_alloc_wakeup_handler(void);

/* XXX shut gcc up */
extern int		acpi_savecpu(void);
extern int		acpi_restorecpu(void);

__asm__("
	.text
	.p2align 2, 0x90
	.type acpi_restorecpu, @function
acpi_restorecpu:
	.align 4
	movl	r_eax,%eax
	movl	r_ebx,%ebx
	movl	r_ecx,%ecx
	movl	r_edx,%edx
	movl	r_ebp,%ebp
	movl	r_esi,%esi
	movl	r_edi,%edi
	movl	r_esp,%esp

	pushl	r_efl
	popfl

	pushl	ret_addr
	xorl	%eax,%eax
	ret

	.text
	.p2align 2, 0x90
	.type acpi_savecpu, @function
acpi_savecpu:
	movw	%cs,r_cs
	movw	%ds,r_ds
	movw	%es,r_es
	movw	%fs,r_fs
	movw	%gs,r_gs
	movw	%ss,r_ss

	movl	%eax,r_eax
	movl	%ebx,r_ebx
	movl	%ecx,r_ecx
	movl	%edx,r_edx
	movl	%ebp,r_ebp
	movl	%esi,r_esi
	movl	%edi,r_edi

	movl	%cr0,%eax
	movl	%eax,r_cr0
	movl	%cr2,%eax
	movl	%eax,r_cr2
	movl	%cr3,%eax
	movl	%eax,r_cr3
	movl	%cr4,%eax
	movl	%eax,r_cr4

	pushfl
	popl	r_efl

	movl	%esp,r_esp

	sgdt	r_gdt
	sidt	r_idt
	sldt	r_ldt
	str	r_tr

	movl	(%esp),%eax
	movl	%eax,ret_addr
	movl	$1,%eax
	ret
");

static void
acpi_printcpu(void)
{

	printf("======== acpi_printcpu() debug dump ========\n");
	printf("gdt[%04x:%08x] idt[%04x:%08x] ldt[%04x] tr[%04x] efl[%08x]\n",
		r_gdt.rd_limit, r_gdt.rd_base, r_idt.rd_limit, r_idt.rd_base,
		r_ldt, r_tr, r_efl);
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
	void	**addr;						\
	addr = (void **)(sc->acpi_wakeaddr + offset);		\
	(type *)*addr = val;					\
} while (0)

#define WAKECODE_BCOPY(offset, type, val) do	{		\
	void	**addr;						\
	addr = (void **)(sc->acpi_wakeaddr + offset);		\
	bcopy(&(val), addr, sizeof(type));			\
} while (0)

int
acpi_sleep_machdep(struct acpi_softc *sc, int state)
{
	ACPI_STATUS		status;
	vm_offset_t		oldphys;
	struct pmap		*pm;
	vm_page_t		page;
	static vm_page_t	opage = NULL;
	int			ret = 0;
	int			pteobj_allocated = 0;
	u_long			ef;

	if (sc->acpi_wakeaddr == NULL) {
		return (0);
	}

	AcpiSetFirmwareWakingVector(sc->acpi_wakephys);

	ef = read_eflags();
	disable_intr();

	/* Create Identity Mapping */
	pm = vmspace_pmap(CURPROC->p_vmspace);
	if (pm->pm_pteobj == NULL) {
		pm->pm_pteobj = vm_object_allocate(OBJT_DEFAULT, PTDPTDI + 1);
		pteobj_allocated = 1;
	}

	oldphys = pmap_extract(pm, sc->acpi_wakephys);
	if (oldphys) {
		opage = PHYS_TO_VM_PAGE(oldphys);
	}
	page = PHYS_TO_VM_PAGE(sc->acpi_wakephys);
	pmap_enter(pm, sc->acpi_wakephys, page,
		   VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE, 1);

	ret_addr = 0;
	if (acpi_savecpu()) {
		/* Execute Sleep */
		p_gdt = (struct region_descriptor *)(sc->acpi_wakeaddr + physical_gdt);
		p_gdt->rd_limit = r_gdt.rd_limit;
		p_gdt->rd_base = vtophys(r_gdt.rd_base);

		WAKECODE_FIXUP(physical_esp, u_int32_t, vtophys(r_esp));
		WAKECODE_FIXUP(previous_cr0, u_int32_t, r_cr0);
		WAKECODE_FIXUP(previous_cr2, u_int32_t, r_cr2);
		WAKECODE_FIXUP(previous_cr3, u_int32_t, r_cr3);
		WAKECODE_FIXUP(previous_cr4, u_int32_t, r_cr4);

		WAKECODE_FIXUP(previous_tr,  u_int16_t, r_tr);
		WAKECODE_BCOPY(previous_gdt, struct region_descriptor, r_gdt);
		WAKECODE_FIXUP(previous_ldt, u_int16_t, r_ldt);
		WAKECODE_BCOPY(previous_idt, struct region_descriptor, r_idt);

		WAKECODE_FIXUP(where_to_recover, void, acpi_restorecpu);

		WAKECODE_FIXUP(previous_ds,  u_int16_t, r_ds);
		WAKECODE_FIXUP(previous_es,  u_int16_t, r_es);
		WAKECODE_FIXUP(previous_fs,  u_int16_t, r_fs);
		WAKECODE_FIXUP(previous_gs,  u_int16_t, r_gs);
		WAKECODE_FIXUP(previous_ss,  u_int16_t, r_ss);

		if (acpi_get_verbose(sc)) {
			acpi_printcpu();
		}

		if (state == ACPI_STATE_S4 && sc->acpi_s4bios) {
			status = AcpiEnterSleepStateS4Bios();
		} else {
			status = AcpiEnterSleepState(state);
		}

		if (status != AE_OK) {
			device_printf(sc->acpi_dev,
				"AcpiEnterSleepState failed - %s\n",
				AcpiFormatException(status));
			ret = -1;
			goto out;
		}
		wbinvd();

		for (;;) ;
	} else {
		/* Execute Wakeup */
#if 0
		initializecpu();
#endif
		icu_reinit();

		if (acpi_get_verbose(sc)) {
			acpi_savecpu();
			acpi_printcpu();
		}
	}

out:
	pmap_remove(pm, sc->acpi_wakephys, sc->acpi_wakephys + PAGE_SIZE);
	if (opage) {
		pmap_enter(pm, sc->acpi_wakephys, page,
			   VM_PROT_READ | VM_PROT_WRITE, 0);
	}

	if (pteobj_allocated) {
		vm_object_deallocate(pm->pm_pteobj);
		pm->pm_pteobj = NULL;
	}

	write_eflags(ef);

	return (ret);
}

static bus_dma_tag_t	acpi_waketag;
static bus_dmamap_t	acpi_wakemap;
static vm_offset_t	acpi_wakeaddr = 0;

static void
acpi_alloc_wakeup_handler(void)
{

	if (bus_dma_tag_create(/* parent */ NULL, /* alignment */ 2, 0,
			       /* lowaddr below 1MB */ 0x9ffff,
			       /* highaddr */ BUS_SPACE_MAXADDR, NULL, NULL,
				PAGE_SIZE, 1, PAGE_SIZE, 0, &acpi_waketag) != 0) {
		printf("acpi_alloc_wakeup_handler: unable to create wake tag\n");
		return;
	}

	if (bus_dmamem_alloc(acpi_waketag, (void **)&acpi_wakeaddr,
			     BUS_DMA_NOWAIT, &acpi_wakemap)) {
		printf("acpi_alloc_wakeup_handler: unable to allocate wake memory\n");
		return;
	}
}

SYSINIT(acpiwakeup, SI_SUB_KMEM, SI_ORDER_ANY, acpi_alloc_wakeup_handler, 0)

static void
acpi_realmodeinst(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct acpi_softc	*sc = arg;
	u_int32_t		*addr;

	addr = (u_int32_t *)&wakecode[wakeup_sw32 + 2];
	*addr = segs[0].ds_addr + wakeup_32;
	bcopy(wakecode, (void *)sc->acpi_wakeaddr, sizeof(wakecode));
	sc->acpi_wakephys = segs[0].ds_addr;
}

void
acpi_install_wakeup_handler(struct acpi_softc *sc)
{

	if (acpi_wakeaddr == 0) {
		return;
	}

	sc->acpi_waketag = acpi_waketag;
	sc->acpi_wakeaddr = acpi_wakeaddr;
	sc->acpi_wakemap = acpi_wakemap;

	bus_dmamap_load(sc->acpi_waketag, sc->acpi_wakemap,
			(void *)sc->acpi_wakeaddr, PAGE_SIZE,
			acpi_realmodeinst, sc, 0);
}

