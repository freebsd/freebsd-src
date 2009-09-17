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

#include <machine/bus.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>
#include <arm/mv/mvwin.h>

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
 * 0xf110_0000 - 0xf11f_ffff	: PCI-Express I/O space (1MB)
 * 0xf120_0000 - 0xf12f_ffff	: unused (1MB)
 * 0xf130_0000 - 0xf52f_ffff	: PCI-Express memory space (64MB)
 * 0xf930_0000 - 0xfffe_ffff	: unused (~172MB)
 * 0xffff_0000 - 0xffff_0fff	: 'high' vectors page (4KB)
 * 0xffff_1000 - 0xffff_1fff	: ARM_TP_ADDRESS/RAS page (4KB)
 * 0xffff_2000 - 0xffff_ffff	: unused (~55KB)
 */

/* Static device mappings. */
const struct pmap_devmap pmap_devmap[] = {
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
	{ 0, 0, 0, 0, 0, }
};

const struct gpio_config mv_gpio_config[] = {
	{ -1, -1, -1 }
};

void
platform_mpp_init(void)
{

	/*
	 * MPP Configuration for DB-78100-BP
	 *
	 * MPP[0]:  GE1_TXCLK
	 * MPP[1]:  GE1_TXCTL
	 * MPP[2]:  GE1_RXCTL
	 * MPP[3]:  GE1_RXCLK
	 * MPP[4]:  GE1_TXD[0]
	 * MPP[5]:  GE1_TXD[1]
	 * MPP[6]:  GE1_TXD[2]
	 * MPP[7]:  GE1_TXD[3]
	 * MPP[8]:  GE1_RXD[0]
	 * MPP[9]:  GE1_RXD[1]
	 * MPP[10]: GE1_RXD[2]
	 * MPP[11]: GE1_RXD[3]
	 * MPP[13]: SYSRST_OUTn
	 * MPP[14]: SATA1_ACT
	 * MPP[15]: SATA0_ACT
	 * MPP[16]: UA2_TXD
	 * MPP[17]: UA2_RXD
	 * MPP[18]: <UNKNOWN>
	 * MPP[19]: <UNKNOWN>
	 * MPP[20]: <UNKNOWN>
	 * MPP[21]: <UNKNOWN>
	 * MPP[22]: UA3_TXD
	 * MPP[23]: UA3_RXD
	 * MPP[48]: <UNKNOWN>
	 * MPP[49]: <UNKNOWN>
	 *
	 * Others:  GPIO
	 *
	 * <UNKNOWN> entries are not documented, not on the schematics etc.
	 */
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL0, 0x22222222);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL1, 0x33302222);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL2, 0x44333344);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL3, 0x00000000);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL4, 0x00000000);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL5, 0x00000000);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL6, 0x0000FFFF);
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
