/*-
 * Copyright (c) 2015 Semihalf.
 * Copyright (c) 2015 Stormshield.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/fdt.h>

#include <arm/mv/mvwin.h>
#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

int armada38x_open_bootrom_win(void);
int armada38x_scu_enable(void);
int armada38x_win_set_iosync_barrier(void);

uint32_t
get_tclk(void)
{
	uint32_t sar;

	/*
	 * On Armada38x TCLK can be configured to 250 MHz or 200 MHz.
	 * Current setting is read from Sample At Reset register.
	 */
	sar = (uint32_t)get_sar_value();
	sar = (sar & TCLK_MASK) >> TCLK_SHIFT;
	if (sar == 0)
		return (TCLK_250MHZ);
	else
		return (TCLK_200MHZ);
}

int
armada38x_win_set_iosync_barrier(void)
{
	bus_space_handle_t vaddr_iowind;
	int rv;

	rv = bus_space_map(fdtbus_bs_tag, (bus_addr_t)MV_MBUS_BRIDGE_BASE,
	    MV_CPU_SUBSYS_REGS_LEN, 0, &vaddr_iowind);
	if (rv != 0)
		return (rv);

	/* Set Sync Barrier flags for all Mbus internal units */
	bus_space_write_4(fdtbus_bs_tag, vaddr_iowind, MV_SYNC_BARRIER_CTRL,
	    MV_SYNC_BARRIER_CTRL_ALL);

	bus_space_barrier(fdtbus_bs_tag, vaddr_iowind, 0,
	    MV_CPU_SUBSYS_REGS_LEN, BUS_SPACE_BARRIER_WRITE);
	bus_space_unmap(fdtbus_bs_tag, vaddr_iowind, MV_CPU_SUBSYS_REGS_LEN);

	return (rv);
}

int
armada38x_open_bootrom_win(void)
{
	bus_space_handle_t vaddr_iowind;
	uint32_t val;
	int rv;

	rv = bus_space_map(fdtbus_bs_tag, (bus_addr_t)MV_MBUS_BRIDGE_BASE,
	    MV_CPU_SUBSYS_REGS_LEN, 0, &vaddr_iowind);
	if (rv != 0)
		return (rv);

	val = (MV_BOOTROM_WIN_SIZE & IO_WIN_SIZE_MASK) << IO_WIN_SIZE_SHIFT;
	val |= (MBUS_BOOTROM_ATTR & IO_WIN_ATTR_MASK) << IO_WIN_ATTR_SHIFT;
	val |= (MBUS_BOOTROM_TGT_ID & IO_WIN_TGT_MASK) << IO_WIN_TGT_SHIFT;
	/* Enable window and Sync Barrier */
	val |= (0x1 & IO_WIN_SYNC_MASK) << IO_WIN_SYNC_SHIFT;
	val |= (0x1 & IO_WIN_ENA_MASK) << IO_WIN_ENA_SHIFT;

	/* Configure IO Window Control Register */
	bus_space_write_4(fdtbus_bs_tag, vaddr_iowind, IO_WIN_9_CTRL_OFFSET,
	    val);
	/* Configure IO Window Base Register */
	bus_space_write_4(fdtbus_bs_tag, vaddr_iowind, IO_WIN_9_BASE_OFFSET,
	    MV_BOOTROM_MEM_ADDR);

	bus_space_barrier(fdtbus_bs_tag, vaddr_iowind, 0, MV_CPU_SUBSYS_REGS_LEN,
	    BUS_SPACE_BARRIER_WRITE);
	bus_space_unmap(fdtbus_bs_tag, vaddr_iowind, MV_CPU_SUBSYS_REGS_LEN);

	return (rv);
}

int
armada38x_scu_enable(void)
{
	bus_space_handle_t vaddr_scu;
	int rv;
	uint32_t val;

	rv = bus_space_map(fdtbus_bs_tag, (bus_addr_t)MV_SCU_BASE,
	    MV_SCU_REGS_LEN, 0, &vaddr_scu);
	if (rv != 0)
		return (rv);

	/* Enable SCU */
	val = bus_space_read_4(fdtbus_bs_tag, vaddr_scu, MV_SCU_REG_CTRL);
	if (!(val & MV_SCU_ENABLE))
		bus_space_write_4(fdtbus_bs_tag, vaddr_scu, 0,
		    val | MV_SCU_ENABLE);

	bus_space_unmap(fdtbus_bs_tag, vaddr_scu, MV_SCU_REGS_LEN);
	return (0);
}
