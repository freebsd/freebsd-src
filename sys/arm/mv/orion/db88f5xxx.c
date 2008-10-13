/*-
 * Copyright (C) 2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

/*
 * Virtual address space layout:
 * -----------------------------
 * 0x0000_0000 - 0xbfff_ffff	: user process
 *
 * 0xc040_0000 - virtual_avail	: kernel reserved (text, data, page tables
 *				: structures, ARM stacks etc.)
 * virtual_avail - 0xefff_ffff	: KVA (virtual_avail is typically < 0xc0a0_0000)
 * 0xf000_0000 - 0xf0ff_ffff	: no-cache allocation area (16MB)
 * 0xf100_0000 - 0xf10f_ffff	: SoC integrated devices registers range (1MB)
 * 0xf110_0000 - 0xfffe_ffff	: PCI, PCIE (MEM+IO) outbound windows (~238MB)
 * 0xffff_0000 - 0xffff_0fff	: 'high' vectors page (4KB)
 * 0xffff_1000 - 0xffff_1fff	: ARM_TP_ADDRESS/RAS page (4KB)
 * 0xffff_2000 - 0xffff_ffff	: unused (~55KB)
 */

const struct pmap_devmap *pmap_devmap_bootstrap_table;
vm_offset_t pmap_bootstrap_lastaddr;

/* Static device mappings. */
static const struct pmap_devmap pmap_devmap[] = {
	/*
	 * Map the on-board devices VA == PA so that we can access them
	 * with the MMU on or off.
	 */
	{ /* SoC integrated peripherals registers range */
		MV_BASE,
		MV_PHYS_BASE,
		MV_SIZE,
		VM_PROT_READ | VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	{ /* PCIE I/O */
		MV_PCIE_IO_BASE,
		MV_PCIE_IO_PHYS_BASE,
		MV_PCIE_IO_SIZE,
		VM_PROT_READ | VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	{ /* PCIE Memory */
		MV_PCIE_MEM_BASE,
		MV_PCIE_MEM_PHYS_BASE,
		MV_PCIE_MEM_SIZE,
		VM_PROT_READ | VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	{ /* PCI I/O */
		MV_PCI_IO_BASE,
		MV_PCI_IO_PHYS_BASE,
		MV_PCI_IO_SIZE,
		VM_PROT_READ | VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	{ /* PCI Memory */
		MV_PCI_MEM_BASE,
		MV_PCI_MEM_PHYS_BASE,
		MV_PCI_MEM_SIZE,
		VM_PROT_READ | VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	{ /* 7-seg LED */
		MV_DEV_CS0_BASE,
		MV_DEV_CS0_PHYS_BASE,
		MV_DEV_CS0_SIZE,
		VM_PROT_READ | VM_PROT_WRITE,
		PTE_NOCACHE,
	},
	{ 0, 0, 0, 0, 0, }
};

#if 0
int platform_pci_get_irq(u_int bus, u_int slot, u_int func, u_int pin)
{
	int irq;

	switch (slot) {
	case 7:
		irq = GPIO2IRQ(12);	/* GPIO 0 for DB-88F5182  */
		break;			/* GPIO 12 for DB-88F5281 */
	case 8:
	case 9:
		irq = GPIO2IRQ(13);	/* GPIO 1 for DB-88F5182  */
		break;			/* GPIO 13 for DB-88F5281 */
	default:
		irq = -1;
		break;
	};

	/*
	 * XXX This isn't the right place to setup GPIO, but it makes sure
	 * that PCI works on 5XXX targets where U-Boot doesn't set up the GPIO
	 * correctly to handle PCI IRQs (e.g., on 5182). This code will go
	 * away once we set up GPIO in a generic way in a proper place (TBD).
	 */
	if (irq >= 0)
		mv_gpio_configure(IRQ2GPIO(irq), MV_GPIO_POLARITY |
		    MV_GPIO_LEVEL, ~0u);

	return(irq);
}
#endif

int
platform_pmap_init(void)
{

	pmap_bootstrap_lastaddr = MV_BASE - ARM_NOCACHE_KVA_SIZE;
	pmap_devmap_bootstrap_table = &pmap_devmap[0];

	return (0);
}

static void
platform_identify(void *dummy)
{

	soc_identify();

	/*
	 * XXX Board identification e.g. read out from FPGA or similar should
	 * go here
	 */
}
SYSINIT(platform_identify, SI_SUB_CPU, SI_ORDER_SECOND, platform_identify, NULL);

/*
 * TODO routine setting GPIO/MPP pins 
 */
