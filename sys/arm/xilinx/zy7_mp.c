/*-
 * Copyright (c) 2013 Thomas Skibo.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_platform.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/smp.h>
#include <machine/fdt.h>
#include <machine/intr.h>
#include <machine/platformvar.h>

#include <arm/xilinx/zy7_machdep.h>
#include <arm/xilinx/zy7_reg.h>
#include <arm/xilinx/zy7_slcr.h>

#define	ZYNQ7_CPU1_ENTRY		0xfffffff0

#define	SCU_CONTROL_REG			0xf8f00000
#define	   SCU_CONTROL_ENABLE		1
#define	SCU_CONFIG_REG			0xf8f00004
#define	   SCU_CONFIG_N_CPUS_MASK	3

#define SLCR_PSS_IDCODE			0xf8000530

void
zynq7_mp_setmaxid(platform_t plat)
{
	bus_space_handle_t slcr_handle;
	int device_id;
	bus_space_handle_t scu_handle;

	if (mp_ncpus != 0)
		return;

	/* Map in SLCR PSS_IDCODE register. */
	if (bus_space_map(fdtbus_bs_tag, SLCR_PSS_IDCODE, 4, 0,
	    &slcr_handle) != 0)
		panic("%s: Could not map SLCR IDCODE reg.\n", __func__);

	device_id = bus_space_read_4(fdtbus_bs_tag, slcr_handle, 0) &
	    ZY7_SLCR_PSS_IDCODE_DEVICE_MASK;

	bus_space_unmap(fdtbus_bs_tag, slcr_handle, 4);

	/*
	 * Zynq XC7z0xxS single core chips indicate incorrect number of CPUs in
	 * SCU configuration register.
	 */
	if (device_id == ZY7_SLCR_PSS_IDCODE_DEVICE_7Z007S ||
	    device_id == ZY7_SLCR_PSS_IDCODE_DEVICE_7Z012S ||
	    device_id == ZY7_SLCR_PSS_IDCODE_DEVICE_7Z014S) {
		mp_maxid = 0;
		mp_ncpus = 1;
		return;
	}

	/* Map in SCU config register. */
	if (bus_space_map(fdtbus_bs_tag, SCU_CONFIG_REG, 4, 0,
	    &scu_handle) != 0)
		panic("zynq7_mp_setmaxid: Could not map SCU config reg.\n");

	mp_maxid = bus_space_read_4(fdtbus_bs_tag, scu_handle, 0) &
	    SCU_CONFIG_N_CPUS_MASK;
	mp_ncpus = mp_maxid + 1;

	bus_space_unmap(fdtbus_bs_tag, scu_handle, 4);
}

void
zynq7_mp_start_ap(platform_t plat)
{
	bus_space_handle_t scu_handle;
	bus_space_handle_t ocm_handle;
	uint32_t scu_ctrl;

	/* Map in SCU control register. */
	if (bus_space_map(fdtbus_bs_tag, SCU_CONTROL_REG, 4,
	    0, &scu_handle) != 0)
		panic("%s: Could not map SCU control reg.\n", __func__);

	/* Set SCU enable bit. */
	scu_ctrl = bus_space_read_4(fdtbus_bs_tag, scu_handle, 0);
	scu_ctrl |= SCU_CONTROL_ENABLE;
	bus_space_write_4(fdtbus_bs_tag, scu_handle, 0, scu_ctrl);

	bus_space_unmap(fdtbus_bs_tag, scu_handle, 4);

	/* Map in magic location to give entry address to CPU1. */
	if (bus_space_map(fdtbus_bs_tag, ZYNQ7_CPU1_ENTRY, 4,
	    0, &ocm_handle) != 0)
		panic("%s: Could not map OCM\n", __func__);

	/* Write start address for CPU1. */
	bus_space_write_4(fdtbus_bs_tag, ocm_handle, 0,
	    pmap_kextract((vm_offset_t)mpentry));

	bus_space_unmap(fdtbus_bs_tag, ocm_handle, 4);

	/*
	 * The SCU is enabled above but I think the second CPU doesn't
	 * turn on filtering until after the wake-up below. I think that's why
	 * things don't work if I don't put these cache ops here.  Also, the
	 * magic location, 0xfffffff0, isn't in the SCU's filtering range so it
	 * needs a write-back too.
	 */
	dcache_wbinv_poc_all();

	/* Wake up CPU1. */
	dsb();
	sev();
}
