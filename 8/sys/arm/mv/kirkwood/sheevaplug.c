/*-
 * Copyright (C) 2008 MARVELL INTERNATIONAL LTD.
 * Copyright (C) 2009 Semihalf
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
 * 0x0000_0000 - 0x7FFF_FFFF	: User Process (2 GB)
 * 0x8000_0000 - 0xBBFF_FFFF	: Unused (960 MB)
 * 0xBC00_0000 - 0xBDFF_FFFF	: Device Bus: CS1 (32 MB)
 * 0xBE00_0000 - 0xBECF_FFFF	: Unused (13 MB)
 * 0xBED0_0000 - 0xBEDF_FFFF	: Device Bus: CS2 (1 MB)
 * 0xBEE0_0000 - 0xBEEF_FFFF	: Device Bus: CS0 (1 MB)
 * 0xBEF0_0000 - 0xBEFF_FFFF	: Device Bus: BOOT (1 MB)
 * 0xBF00_0000 - 0xBFFF_FFFF	: Unused (16 MB)
 * 0xC000_0000 - virtual_avail	: Kernel Reserved (text, data, page tables,
 * 				: stack etc.)
 * virtual-avail - 0xEFFF_FFFF	: KVA (virtual_avail is typically < 0xc0a0_0000)
 * 0xF000_0000 - 0xF0FF_FFFF	: No-Cache allocation area (16 MB)
 * 0xF100_0000 - 0xF10F_FFFF	: SoC Integrated devices registers range (1 MB)
 * 0xF110_0000 - 0xF11F_FFFF	: CESA SRAM (1 MB)
 * 0xF120_0000 - 0xFFFE_FFFF	: Unused (237 MB + 960 kB)
 * 0xFFFF_0000 - 0xFFFF_0FFF	: 'High' vectors page (4 kB)
 * 0xFFFF_1000 - 0xFFFF_1FFF	: ARM_TP_ADDRESS/RAS page (4 kB)
 * 0xFFFF_2000 - 0xFFFF_FFFF	: Unused (56 kB)
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
	{ /* CESA SRAM */
		MV_CESA_SRAM_BASE,
		MV_CESA_SRAM_PHYS_BASE,
		MV_CESA_SRAM_SIZE,
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
	 * MPP configuration for Sheeva Plug
	 *
	 * MPP[0]:  NF_IO[2]
	 * MPP[1]:  NF_IO[3]
	 * MPP[2]:  NF_IO[4]
	 * MPP[3]:  NF_IO[5]
	 * MPP[4]:  NF_IO[6]
	 * MPP[5]:  NF_IO[7]
	 * MPP[6]:  SYSRST_OUTn
	 * MPP[8]:  UA0_RTS
	 * MPP[9]:  UA0_CTS
	 * MPP[10]: UA0_TXD
	 * MPP[11]: UA0_RXD
	 * MPP[12]: SD_CLK
	 * MPP[13]: SD_CMD
	 * MPP[14]: SD_D[0]
	 * MPP[15]: SD_D[1]
	 * MPP[16]: SD_D[2]
	 * MPP[17]: SD_D[3]
	 * MPP[18]: NF_IO[0]
	 * MPP[19]: NF_IO[1]
	 * MPP[29]: TSMP[9]
	 *
	 * Others:  GPIO
	 */

	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL0, 0x01111111);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL1, 0x11113322);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL2, 0x00001111);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL3, 0x00100000);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL4, 0x00000000);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL5, 0x00000000);
	bus_space_write_4(obio_tag, MV_MPP_BASE, MPP_CONTROL6, 0x00000000);
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
