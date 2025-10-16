/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Andrew Turner
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/systm.h>

#include <machine/cpu.h>

#include <dev/psci/psci.h>
#include <dev/psci/smccc.h>

typedef void (cpu_quirk_install)(void);
struct cpu_quirks {
	cpu_quirk_install *quirk_install;
	u_int		midr_mask;
	u_int		midr_value;
#define	CPU_QUIRK_POST_DEVICE	(1 << 0)	/* After device attach */
						/* e.g. needs SMCCC */
	u_int		flags;
};

static cpu_quirk_install install_thunderx_bcast_tlbi_workaround;

static struct cpu_quirks cpu_quirks[] = {
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_CAVIUM, CPU_PART_THUNDERX, 0, 0),
		.quirk_install = install_thunderx_bcast_tlbi_workaround,
	},
	{
		.midr_mask = CPU_IMPL_MASK | CPU_PART_MASK,
		.midr_value =
		    CPU_ID_RAW(CPU_IMPL_CAVIUM, CPU_PART_THUNDERX_81XX, 0, 0),
		.quirk_install = install_thunderx_bcast_tlbi_workaround,
	},
};

/*
 * Workaround Cavium erratum 27456.
 *
 * Invalidate the local icache when changing address spaces.
 */
static void
install_thunderx_bcast_tlbi_workaround(void)
{
	u_int midr;

	midr = get_midr();
	if (CPU_PART(midr) == CPU_PART_THUNDERX_81XX)
		PCPU_SET(bcast_tlbi_workaround, 1);
	else if (CPU_PART(midr) == CPU_PART_THUNDERX) {
		if (CPU_VAR(midr) == 0) {
			/* ThunderX 1.x */
			PCPU_SET(bcast_tlbi_workaround, 1);
		} else if (CPU_VAR(midr) == 1 && CPU_REV(midr) <= 1) {
			/* ThunderX 2.0 - 2.1 */
			PCPU_SET(bcast_tlbi_workaround, 1);
		}
	}
}

static void
install_cpu_errata_flags(u_int mask, u_int flags)
{
	u_int midr;
	size_t i;

	midr = get_midr();

	for (i = 0; i < nitems(cpu_quirks); i++) {
		if ((midr & cpu_quirks[i].midr_mask) ==
		    cpu_quirks[i].midr_value &&
		    (cpu_quirks[i].flags & mask) == flags) {
			cpu_quirks[i].quirk_install();
		}
	}
}

/*
 * Install any CPU errata we need. On CPU 0 we only install the errata that
 * don't depend on device drivers as this is called early in the boot process.
 * On other CPUs the device drivers have already attached so install all
 * applicable errata.
 */
void
install_cpu_errata(void)
{
	/*
	 * Only install early CPU errata on CPU 0, device drivers may not
	 * have attached and some workarounds depend on them, e.g. to query
	 * SMCCC.
	 */
	if (PCPU_GET(cpuid) == 0) {
		install_cpu_errata_flags(CPU_QUIRK_POST_DEVICE, 0);
	} else {
		install_cpu_errata_flags(0, 0);
	}
}

/*
 * Install any errata workarounds that depend on device drivers, e.g. use
 * SMCCC to install a workaround.
 */
static void
install_cpu_errata_late(void *dummy __unused)
{
	MPASS(PCPU_GET(cpuid) == 0);
	install_cpu_errata_flags(CPU_QUIRK_POST_DEVICE, CPU_QUIRK_POST_DEVICE);
}
SYSINIT(install_cpu_errata_late, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE,
    install_cpu_errata_late, NULL);
