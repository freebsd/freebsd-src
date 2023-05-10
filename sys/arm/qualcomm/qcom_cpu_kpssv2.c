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

#include <arm/qualcomm/qcom_cpu_kpssv2_reg.h>
#include <arm/qualcomm/qcom_cpu_kpssv2.h>

#include "platform_if.h"

/*
 * Since DELAY() hangs this early, we need some way to
 * delay things to settle.
 */
static inline void
loop_delay(int usec)
{
	int lcount = usec * 100000;

	for (volatile int i = 0; i < lcount; i++)
		;
}

/*
 * This is the KPSSv2 (eg IPQ4018) regulator path for CPU
 * and shared L2 cache power-on.
 */
boolean_t
qcom_cpu_kpssv2_regulator_start(u_int id, phandle_t node)
{
	phandle_t acc_phandle, l2_phandle, saw_phandle;
	bus_space_tag_t acc_tag, saw_tag;
	bus_space_handle_t acc_handle, saw_handle;
	bus_size_t acc_sz, saw_sz;
	ssize_t sret;
	int ret;
	uint32_t reg_val;

	/*
	 * We don't need to power up CPU 0!  This will power it
	 * down first and ... then everything hangs.
	 */
	if (id == 0)
		return true;

	/*
	 * Walk the qcom,acc and next-level-cache entries to find their
	 * child phandles and thus regulators.
	 *
	 * The qcom,acc is a phandle to a node.
	 *
	 * The next-level-cache actually is a phandle through to a qcom,saw
	 * entry.
	 */
	sret = OF_getencprop(node, "qcom,acc", (void *) &acc_phandle,
	    sizeof(acc_phandle));
	if (sret != sizeof(acc_phandle))
		panic("***couldn't get phandle for qcom,acc");
	acc_phandle = OF_node_from_xref(acc_phandle);

	sret = OF_getencprop(node, "next-level-cache", (void *) &l2_phandle,
	    sizeof(l2_phandle));
	if (sret != sizeof(l2_phandle))
		panic("***couldn't get phandle for next-level-cache");
	l2_phandle = OF_node_from_xref(l2_phandle);

	sret = OF_getencprop(l2_phandle, "qcom,saw", (void *) &saw_phandle,
	    sizeof(saw_phandle));
	if (sret != sizeof(saw_phandle))
		panic("***couldn't get phandle for qcom,saw");
	l2_phandle = OF_node_from_xref(l2_phandle);

	/*
	 * Now that we have the phandles referencing the correct locations,
	 * do some KVA mappings so we can go access the registers.
	 */
	ret = OF_decode_addr(acc_phandle, 0, &acc_tag, &acc_handle, &acc_sz);
	if (ret != 0)
		panic("*** couldn't map qcom,acc space (%d)", ret);
	ret = OF_decode_addr(saw_phandle, 0, &saw_tag, &saw_handle, &saw_sz);
	if (ret != 0)
		panic("*** couldn't map next-level-cache -> "
		    "qcom,saw space (%d)", ret);

	/*
	 * Power sequencing to ensure the cores are off, then power them on
	 * and bring them out of reset.
	 */

	/*
	 * BHS: off
	 * LDO: bypassed, powered off
	 */
	reg_val = (64 << QCOM_APC_PWR_GATE_CTL_BHS_CNT_SHIFT)
	    | (0x3f << QCOM_APC_PWR_GATE_CTL_LDO_PWR_DWN_SHIFT)
	    | QCOM_APC_PWR_GATE_CTL_BHS_EN;
	bus_space_write_4(acc_tag, acc_handle, QCOM_APC_PWR_GATE_CTL, reg_val);
	mb();
	/* Settle time */
	loop_delay(1);

	/*
	 * Start up BHS segments.
	 */
	reg_val |= 0x3f << QCOM_APC_PWR_GATE_CTL_BHS_SEG_SHIFT;
	bus_space_write_4(acc_tag, acc_handle, QCOM_APC_PWR_GATE_CTL, reg_val);
	mb();
	/* Settle time */
	loop_delay(1);

	/*
	 * Switch on the LDO bypass; BHS will now supply power.
	 */
	reg_val |= 0x3f << QCOM_APC_PWR_GATE_CTL_LDO_BYP_SHIFT;
	bus_space_write_4(acc_tag, acc_handle, QCOM_APC_PWR_GATE_CTL, reg_val);

	/*
	 * Shared L2 regulator control.
	 */
	bus_space_write_4(saw_tag, saw_handle, QCOM_APCS_SAW2_2_VCTL, 0x10003);
	mb();
	/* Settle time */
	loop_delay(50);

	/*
	 * Put the core in reset.
	 */
	reg_val = QCOM_APCS_CPU_PWR_CTL_COREPOR_RST
	    | QCOM_APCS_CPU_PWR_CTL_CLAMP;
	bus_space_write_4(acc_tag, acc_handle, QCOM_APCS_CPU_PWR_CTL, reg_val);
	mb();
	loop_delay(2);

	/*
	 * Remove power-down clamp.
	 */
	reg_val &= ~QCOM_APCS_CPU_PWR_CTL_CLAMP;
	bus_space_write_4(acc_tag, acc_handle, QCOM_APCS_CPU_PWR_CTL, reg_val);
	mb();
	loop_delay(2);

	/*
	 * Clear core power reset.
	 */
	reg_val &= ~QCOM_APCS_CPU_PWR_CTL_COREPOR_RST;
	bus_space_write_4(acc_tag, acc_handle, QCOM_APCS_CPU_PWR_CTL, reg_val);
	mb();

	/*
	 * The power is ready, the core is out of reset, signal the core
	 * to power up.
	 */
	reg_val |= QCOM_APCS_CPU_PWR_CTL_CORE_PWRD_UP;
	bus_space_write_4(acc_tag, acc_handle, QCOM_APCS_CPU_PWR_CTL, reg_val);
	mb();

	/*
	 * Finished with these KVA mappings, so release them.
	 */
	bus_space_unmap(acc_tag, acc_handle, acc_sz);
	bus_space_unmap(saw_tag, saw_handle, saw_sz);

	return true;
}
