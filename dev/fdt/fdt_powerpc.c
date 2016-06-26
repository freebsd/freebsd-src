/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/intr_machdep.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "ofw_bus_if.h"
#include "fdt_common.h"

static void
fdt_fixup_busfreq(phandle_t root, uint32_t div)
{
	phandle_t sb, cpus, child;
	pcell_t freq;

	/*
	 * Do a strict check so as to skip non-SOC nodes, which also claim
	 * simple-bus compatibility such as eLBC etc.
	 */
	if ((sb = fdt_find_compatible(root, "simple-bus", 1)) == 0)
		return;

	/*
	 * This fixup uses /cpus/ bus-frequency prop value to set simple-bus
	 * bus-frequency property.
	 */
	if ((cpus = OF_finddevice("/cpus")) == -1)
		return;

	if ((child = OF_child(cpus)) == 0)
		return;

	if (OF_getprop(child, "bus-frequency", (void *)&freq,
	    sizeof(freq)) <= 0)
		return;

	if (div == 0)
		return;

	freq /= div;

	OF_setprop(sb, "bus-frequency", (void *)&freq, sizeof(freq));
}

static void
fdt_fixup_busfreq_mpc85xx(phandle_t root)
{

	fdt_fixup_busfreq(root, 1);
}

static void
fdt_fixup_busfreq_dpaa(phandle_t root)
{

	fdt_fixup_busfreq(root, 2);
}

static void
fdt_fixup_fman(phandle_t root)
{
	phandle_t node;
	pcell_t freq;

	if ((node = fdt_find_compatible(root, "simple-bus", 1)) == 0)
		return;

	if (OF_getprop(node, "bus-frequency", (void *)&freq,
	    sizeof(freq)) <= 0)
		return;

	/*
	 * Set clock-frequency for FMan nodes (only on QorIQ DPAA targets).
	 * That frequency is equal to /soc node bus-frequency.
	 */
	for (node = OF_child(node); node != 0; node = OF_peer(node)) {
		if (fdt_is_compatible(node, "fsl,fman") == 0)
			continue;

		if (OF_setprop(node, "clock-frequency", (void *)&freq,
		    sizeof(freq)) == -1) {
			/*
			 * XXX Shall we take some actions if no clock-frequency
			 * property was found?
			 */
		}
	}
}

struct fdt_fixup_entry fdt_fixup_table[] = {
	{ "fsl,MPC8572DS", &fdt_fixup_busfreq_mpc85xx },
	{ "MPC8555CDS", &fdt_fixup_busfreq_mpc85xx },
	{ "fsl,P2020", &fdt_fixup_busfreq_mpc85xx },
	{ "fsl,P2041RDB", &fdt_fixup_busfreq_dpaa },
	{ "fsl,P2041RDB", &fdt_fixup_fman },
	{ "fsl,P3041DS", &fdt_fixup_busfreq_dpaa },
	{ "fsl,P3041DS", &fdt_fixup_fman },
	{ "fsl,P5020DS", &fdt_fixup_busfreq_dpaa },
	{ "fsl,P5020DS", &fdt_fixup_fman },
	{ "varisys,CYRUS", &fdt_fixup_busfreq_dpaa },
	{ "varisys,CYRUS", &fdt_fixup_fman },
	{ NULL, NULL }
};

