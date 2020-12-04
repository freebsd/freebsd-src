/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/fdt.h>
#include <machine/smp.h>
#include <machine/platformvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_cpu.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/psci/psci.h>

#include <arm/rockchip/rk32xx_mp.h>

#define	IMEM_PHYSBASE			0xFF700000
#define	IMEM_SIZE			0x00018000

#define	PMU_PHYSBASE			0xFF730000
#define	PMU_SIZE			0x00010000
#define	PMU_PWRDN_CON			0x08

static int running_cpus;
static uint32_t psci_mask, pmu_mask;
void
rk32xx_mp_setmaxid(platform_t plat)
{
	int ncpu;

	/* If we've already set the global vars don't bother to do it again. */
	if (mp_ncpus != 0)
		return;

	/* Read current CP15 Cache Size ID Register */
	ncpu = cp15_l2ctlr_get();
	ncpu = CPUV7_L2CTLR_NPROC(ncpu);

	mp_ncpus = ncpu;
	mp_maxid = ncpu - 1;
}

static void
rk32xx_mp_start_pmu(uint32_t mask)
{
	bus_space_handle_t imem;
	bus_space_handle_t pmu;
	uint32_t val;
	int i, rv;

	rv = bus_space_map(fdtbus_bs_tag, IMEM_PHYSBASE, IMEM_SIZE, 0, &imem);
	if (rv != 0)
		panic("Couldn't map the IMEM\n");
	rv = bus_space_map(fdtbus_bs_tag, PMU_PHYSBASE, PMU_SIZE, 0, &pmu);
	if (rv != 0)
		panic("Couldn't map the PMU\n");

	/* Power off all secondary cores first */
	val = bus_space_read_4(fdtbus_bs_tag, pmu, PMU_PWRDN_CON);
	for (i = 1; i < mp_ncpus; i++)
		val |= 1 << i;
	bus_space_write_4(fdtbus_bs_tag, pmu, PMU_PWRDN_CON, val);
	DELAY(5000);

	/* Power up all secondary cores */
	val = bus_space_read_4(fdtbus_bs_tag, pmu, PMU_PWRDN_CON);
	for (i = 1; i < mp_ncpus; i++)
		val &= ~(1 << i);
	bus_space_write_4(fdtbus_bs_tag, pmu, PMU_PWRDN_CON, val);
	DELAY(5000);

	/* Copy mpentry address then magic to sram */
	val = pmap_kextract((vm_offset_t)mpentry);
	bus_space_write_4(fdtbus_bs_tag, imem, 8, val);
	dsb();
	bus_space_write_4(fdtbus_bs_tag, imem, 4, 0xDEADBEAF);
	dsb();

	sev();

	bus_space_unmap(fdtbus_bs_tag, imem, IMEM_SIZE);
	bus_space_unmap(fdtbus_bs_tag, pmu, PMU_SIZE);
}

static boolean_t
rk32xx_start_ap(u_int id, phandle_t node, u_int addr_cells, pcell_t *reg)
{
	int rv;
	char method[16];
	uint32_t mask;

	if (!ofw_bus_node_status_okay(node))
		return(false);

	/* Skip boot CPU. */
	if (id == 0)
		return (true);

	if (running_cpus >= mp_ncpus)
		return (false);
	running_cpus++;

	mask = 1 << (*reg & 0x0f);

#ifdef INVARIANTS
	if ((mask & pmu_mask) || (mask & psci_mask))
		printf("CPU: Duplicated register value: 0x%X for CPU(%d)\n",
		    *reg, id);
#endif
	rv = OF_getprop(node, "enable-method", method, sizeof(method));
	if (rv > 0 && strcmp(method, "psci") == 0) {
		psci_mask |= mask;
		rv = psci_cpu_on(*reg, pmap_kextract((vm_offset_t)mpentry), id);
		if (rv != PSCI_RETVAL_SUCCESS) {
			printf("Failed to start CPU(%d)\n", id);
			return (false);
		}
		return (true);
	}

	pmu_mask |= mask;
	return (true);
}

void
rk32xx_mp_start_ap(platform_t plat)
{

	ofw_cpu_early_foreach(rk32xx_start_ap, true);
	if (pmu_mask != 0 && psci_mask != 0) {
		printf("Inconsistent CPUs startup methods detected.\n");
		printf("Only PSCI enabled cores will be started.\n");
		return;
	}
	if (pmu_mask != 0)
		rk32xx_mp_start_pmu(pmu_mask);
}
