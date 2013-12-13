/*-
 * Copyright (c) 2013 Ian Lepore <ian@freebsd.org>
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

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/armreg.h>
#include <machine/bus.h>
#include <machine/machdep.h>

#include <arm/freescale/imx/imx_machdep.h>
#include <arm/freescale/imx/imx_wdogreg.h>

#define	IMX_MAX_DEVMAP_ENTRIES	8

static struct pmap_devmap devmap_entries[IMX_MAX_DEVMAP_ENTRIES];
static u_int		  devmap_idx;
static vm_offset_t	  devmap_vaddr = ARM_VECTORS_HIGH;

void
imx_devmap_addentry(vm_paddr_t pa, vm_size_t sz) 
{
	struct pmap_devmap *m;

	/*
	 * The last table entry is the all-zeroes end-of-table marker.  If we're
	 * about to overwrite it the world is coming to an end.  This code runs
	 * too early for the panic to be printed unless a special early-debug
	 * console is in use, but there's nothing else we can do.
	 */
	if (devmap_idx == (IMX_MAX_DEVMAP_ENTRIES - 1))
		panic("IMX_MAX_DEVMAP_ENTRIES is too small!\n");

	/*
	 * Allocate virtual address space from the top of kva downwards.  If the
	 * range being mapped is aligned and sized to 1MB boundaries then also
	 * align the virtual address to the next-lower 1MB boundary so that we
	 * end up with a section mapping.
	 */
	if ((pa & 0x000fffff) == 0 && (sz & 0x000fffff) == 0) {
		devmap_vaddr = (devmap_vaddr - sz) & ~0x000fffff;
	} else {
		devmap_vaddr = (devmap_vaddr - sz) & ~0x00000fff;
	}
	m = &devmap_entries[devmap_idx++];
	m->pd_va    = devmap_vaddr;
	m->pd_pa    = pa;
	m->pd_size  = sz;
	m->pd_prot  = VM_PROT_READ | VM_PROT_WRITE;
	m->pd_cache = PTE_DEVICE;
}

vm_offset_t
initarm_lastaddr(void)
{

	/* XXX - Get rid of this stuff soon. */
	boothowto |= RB_VERBOSE|RB_MULTIPLE;
	bootverbose = 1;

	/*
	 * Normally initarm() calls platform_devmap_init() much later in the
	 * init process to set up static device mappings.  To calculate the
	 * highest available kva address we have to do that setup first.  It
	 * maps downwards from ARM_VECTORS_HIGH and the last usable kva address
	 * is the point right before the virtual address of the first static
	 * mapping.  So go set up the static mapping table now, then we can
	 * return the lowest static devmap vaddr as the end of usable kva.
	 */
	imx_devmap_init();

	pmap_devmap_bootstrap_table = devmap_entries;

	return (devmap_vaddr);
}

int
platform_devmap_init(void)
{

	/* On imx this work is done during initarm_lastaddr(). */
	return (0);
}

/*
 * Set initial values of GPIO output ports
 */
void
initarm_gpio_init(void)
{

}

void
initarm_late_init(void)
{
	struct pmap_devmap *m;

	/*
	 * We did the static devmap setup earlier, during initarm_lastaddr(),
	 * but now the console should be working and we can be verbose about
	 * what we did.
	 */
	if (bootverbose) {
		for (m = devmap_entries; m->pd_va != 0; ++m) {
			printf("Devmap: phys 0x%08x virt 0x%08x size %uK\n",
			    m->pd_pa, m->pd_va, m->pd_size / 1024);
		}
	}


}

struct arm32_dma_range *
bus_dma_get_range(void)
{

	return (NULL);
}

int
bus_dma_get_range_nb(void)
{

	return (0);
}

/*
 * This code which manipulates the watchdog hardware is here to implement
 * cpu_reset() because the watchdog is the only way for software to reset the
 * chip.  Why here and not in imx_wdog.c?  Because there's no requirement that
 * the watchdog driver be compiled in, but it's nice to be able to reboot even
 * if it's not.
 */
void
imx_wdog_cpu_reset(vm_offset_t wdcr_physaddr)
{
	const struct pmap_devmap *pd;
	volatile uint16_t * pcr;

	/*
	 * The deceptively simple write of WDOG_CR_WDE enables the watchdog,
	 * sets the timeout to its minimum value (half a second), and also
	 * clears the SRS bit which results in the SFTW (software-requested
	 * reset) bit being set in the watchdog status register after the reset.
	 * This is how software can distinguish a reset from a wdog timeout.
	 */
	if ((pd = pmap_devmap_find_pa(wdcr_physaddr, 2)) == NULL) {
		printf("cpu_reset() can't find its control register... locking up now.");
	} else {
		pcr = (uint16_t *)(pd->pd_va + (wdcr_physaddr - pd->pd_pa));
		*pcr = WDOG_CR_WDE;
	}
	for (;;)
		continue;
}

u_int
imx_soc_family(void)
{
	return (imx_soc_type() >> IMXSOC_FAMSHIFT);
}


