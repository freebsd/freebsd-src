/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/reboot.h>
#include <sys/devmap.h>
#include <sys/smp.h>

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/machdep.h>
#include <machine/platformvar.h>
#include <machine/smp.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_cpu.h>

#include <arm/qualcomm/ipq4018_machdep.h>
#include <arm/qualcomm/qcom_scm_legacy.h>
#include <arm/qualcomm/qcom_cpu_kpssv2.h>

#include "platform_if.h"

void
ipq4018_mp_setmaxid(platform_t plat)
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

	printf("SMP: ncpu=%d\n", ncpu);
}

static bool
ipq4018_start_ap(u_int id, phandle_t node, u_int addr_cells, pcell_t *arg)
{

	/*
	 * For the IPQ401x we assume the enable method is
	 * "qcom,kpss-acc-v2".  If this path gets turned into
	 * something more generic for other 32 bit qualcomm
	 * SoCs then we'll likely want to turn this into a
	 * switch based on "enable-method".
	 */
	return qcom_cpu_kpssv2_regulator_start(id, node);
}

void
ipq4018_mp_start_ap(platform_t plat)
{
	int ret;

	/*
	 * First step - SCM call to set the cold boot address to mpentry, so
	 * CPUs hopefully start in the MP path.
	 */
	ret = qcom_scm_legacy_mp_set_cold_boot_address((vm_offset_t) mpentry);
	if (ret != 0)
		panic("%s: Couldn't set cold boot address via SCM "
		    "(error 0x%08x)", __func__, ret);

	/*
	 * Next step - loop over the CPU nodes and do the per-CPU setup
	 * required to power on the CPUs themselves.
	 */
	ofw_cpu_early_foreach(ipq4018_start_ap, true);

	/*
	 * The next set of IPIs to the CPUs will wake them up and enter
	 * mpentry.
	 */
}
