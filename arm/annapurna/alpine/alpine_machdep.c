/*-
 * Copyright (c) 2013 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2015 Semihalf
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/devmap.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/frame.h> /* For trapframe_t, used in <machine/machdep.h> */
#include <machine/machdep.h>
#include <machine/platform.h>
#include <machine/fdt.h>

#include <dev/fdt/fdt_common.h>

#include "opt_ddb.h"
#include "opt_platform.h"

struct mtx al_dbg_lock;

#define	DEVMAP_MAX_VA_ADDRESS		0xF0000000
bus_addr_t al_devmap_pa;
bus_addr_t al_devmap_size;

#define	AL_NB_SERVICE_OFFSET		0x70000
#define	AL_NB_CCU_OFFSET			0x90000
#define	AL_CCU_SNOOP_CONTROL_IOFAB_0_OFFSET	0x4000
#define	AL_CCU_SNOOP_CONTROL_IOFAB_1_OFFSET	0x5000
#define	AL_CCU_SPECULATION_CONTROL_OFFSET	0x4

#define	AL_NB_ACF_MISC_OFFSET			0xD0
#define	AL_NB_ACF_MISC_READ_BYPASS 		(1 << 30)

int alpine_get_devmap_base(bus_addr_t *pa, bus_addr_t *size);

vm_offset_t
platform_lastaddr(void)
{

	return (DEVMAP_MAX_VA_ADDRESS);
}

void
platform_probe_and_attach(void)
{

}

void
platform_gpio_init(void)
{

}

void
platform_late_init(void)
{
	bus_addr_t reg_baddr;
	uint32_t val;

	if (!mtx_initialized(&al_dbg_lock))
		mtx_init(&al_dbg_lock, "ALDBG", "ALDBG", MTX_SPIN);

	/* configure system fabric */
	if (bus_space_map(fdtbus_bs_tag, al_devmap_pa, al_devmap_size, 0,
	    &reg_baddr))
		panic("Couldn't map Register Space area");

	/* do not allow reads to bypass writes to different addresses */
	val = bus_space_read_4(fdtbus_bs_tag, reg_baddr,
	    AL_NB_SERVICE_OFFSET + AL_NB_ACF_MISC_OFFSET);
	val &= ~AL_NB_ACF_MISC_READ_BYPASS;
	bus_space_write_4(fdtbus_bs_tag, reg_baddr,
	    AL_NB_SERVICE_OFFSET + AL_NB_ACF_MISC_OFFSET, val);

	/* enable cache snoop */
	bus_space_write_4(fdtbus_bs_tag, reg_baddr,
	    AL_NB_CCU_OFFSET + AL_CCU_SNOOP_CONTROL_IOFAB_0_OFFSET, 1);
	bus_space_write_4(fdtbus_bs_tag, reg_baddr,
	    AL_NB_CCU_OFFSET + AL_CCU_SNOOP_CONTROL_IOFAB_1_OFFSET, 1);

	/* disable speculative fetches from masters */
	bus_space_write_4(fdtbus_bs_tag, reg_baddr,
	    AL_NB_CCU_OFFSET + AL_CCU_SPECULATION_CONTROL_OFFSET, 7);

	bus_space_unmap(fdtbus_bs_tag, reg_baddr, al_devmap_size);
}

/*
 * Construct devmap table with DT-derived config data.
 */
int
platform_devmap_init(void)
{
	alpine_get_devmap_base(&al_devmap_pa, &al_devmap_size);
	devmap_add_entry(al_devmap_pa, al_devmap_size);
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
