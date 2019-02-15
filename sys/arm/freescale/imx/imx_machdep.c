/*-
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * Copyright (c) 2012, 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 * Portions of this software were developed by Oleksandr Rybalko
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/at91/kb920x_machdep.c, rev 45
 */

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/reboot.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/frame.h> /* For trapframe_t, used in <machine/machdep.h> */
#include <machine/machdep.h>
#include <machine/pmap.h>

#include <dev/fdt/fdt_common.h>

#define	IMX51_DEV_VIRT_BASE	0xe0000000
vm_offset_t
initarm_lastaddr(void)
{

	boothowto |= RB_VERBOSE|RB_MULTIPLE;
	bootverbose = 1;

	if (fdt_immr_addr(IMX51_DEV_VIRT_BASE) != 0)
		while (1);

	/* Platform-specific initialisation */
	return (fdt_immr_va - ARM_NOCACHE_KVA_SIZE);
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

}

#define FDT_DEVMAP_MAX	2
static struct pmap_devmap fdt_devmap[FDT_DEVMAP_MAX] = {
	{ 0, 0, 0, 0, 0, },
	{ 0, 0, 0, 0, 0, }
};

/*
 * Construct pmap_devmap[] with DT-derived config data.
 */
int
platform_devmap_init(void)
{

	/*
	 * Map segment where UART1 and UART2 located.
	 */
	fdt_devmap[0].pd_va = IMX51_DEV_VIRT_BASE + 0x03f00000;
	fdt_devmap[0].pd_pa = 0x73f00000;
	fdt_devmap[0].pd_size = 0x00100000;
	fdt_devmap[0].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[0].pd_cache = PTE_NOCACHE;

	pmap_devmap_bootstrap_table = &fdt_devmap[0];
	return (0);
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

void
cpu_reset(void)
{

	printf("Reset ...\n");
	/* Clear n_reset flag */
	*((volatile u_int16_t *)(IMX51_DEV_VIRT_BASE + 0x03f98000)) =
	    (u_int16_t)0;
	while (1);
}
