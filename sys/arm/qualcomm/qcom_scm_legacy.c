/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/machdep.h>
#include <machine/smp.h>

#include <arm/qualcomm/qcom_scm_defs.h>
#include <arm/qualcomm/qcom_scm_legacy_defs.h>
#include <arm/qualcomm/qcom_scm_legacy.h>

#include <dev/psci/smccc.h>

/*
 * Set the cold boot address for (later) a mask of CPUs.
 *
 * Don't set it for CPU0, that CPU is the boot CPU and is already alive.
 *
 * For now it sets it on CPU1..3.
 *
 * This works on the IPQ4019 as tested; the retval is 0x0.
 */
uint32_t
qcom_scm_legacy_mp_set_cold_boot_address(vm_offset_t mp_entry_func)
{
	struct arm_smccc_res res;
	int ret;
	int context_id;

	uint32_t scm_arg0 = QCOM_SCM_LEGACY_ATOMIC_ID(QCOM_SCM_SVC_BOOT,
	    QCOM_SCM_BOOT_SET_ADDR, 2);

	uint32_t scm_arg1 = QCOM_SCM_FLAG_COLDBOOT_CPU1
	    | QCOM_SCM_FLAG_COLDBOOT_CPU2
	    | QCOM_SCM_FLAG_COLDBOOT_CPU3;
	uint32_t scm_arg2 = pmap_kextract((vm_offset_t)mp_entry_func);

	ret = arm_smccc_smc(scm_arg0, (uint32_t) &context_id, scm_arg1,
	    scm_arg2, 0, 0, 0, 0, &res);

	if (ret == 0 && res.a0 == 0)
		return (0);
	printf("%s: called; error; ret=0x%08x; retval[0]=0x%08x\n",
	    __func__, ret, res.a0);

	return (0);
}
