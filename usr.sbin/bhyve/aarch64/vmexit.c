/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/cpuset.h>

#include <dev/psci/psci.h>
#include <dev/psci/smccc.h>

#include <machine/armreg.h>
#include <machine/cpu.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <machine/vmm_instruction_emul.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <vmmapi.h>

#include "bhyve_machdep.h"
#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "gdb.h"
#include "mem.h"
#include "vmexit.h"

cpuset_t running_cpumask;

static int
vmexit_inst_emul(struct vmctx *ctx __unused, struct vcpu *vcpu,
    struct vm_run *vmrun)
{
	struct vm_exit *vme;
	struct vie *vie;
	int err;

	vme = vmrun->vm_exit;
	vie = &vme->u.inst_emul.vie;

	err = emulate_mem(vcpu, vme->u.inst_emul.gpa, vie,
	    &vme->u.inst_emul.paging);
	if (err) {
		if (err == ESRCH) {
			EPRINTLN("Unhandled memory access to 0x%lx\n",
			    vme->u.inst_emul.gpa);
		}
		goto fail;
	}

	return (VMEXIT_CONTINUE);

fail:
	fprintf(stderr, "Failed to emulate instruction ");
	FPRINTLN(stderr, "at 0x%lx", vme->pc);
	return (VMEXIT_ABORT);
}

static int
vmexit_reg_emul(struct vmctx *ctx __unused, struct vcpu *vcpu __unused,
    struct vm_run *vmrun)
{
	struct vm_exit *vme;
	struct vre *vre;

	vme = vmrun->vm_exit;
	vre = &vme->u.reg_emul.vre;

	EPRINTLN("Unhandled register access: pc %#lx syndrome %#x reg %d\n",
	    vme->pc, vre->inst_syndrome, vre->reg);
	return (VMEXIT_ABORT);
}

static int
vmexit_suspend(struct vmctx *ctx, struct vcpu *vcpu, struct vm_run *vmrun)
{
	struct vm_exit *vme;
	enum vm_suspend_how how;
	int vcpuid = vcpu_id(vcpu);

	vme = vmrun->vm_exit;
	how = vme->u.suspended.how;

	fbsdrun_deletecpu(vcpuid);

	switch (how) {
	case VM_SUSPEND_RESET:
		exit(0);
	case VM_SUSPEND_POWEROFF:
		if (get_config_bool_default("destroy_on_poweroff", false))
			vm_destroy(ctx);
		exit(1);
	case VM_SUSPEND_HALT:
		exit(2);
	case VM_SUSPEND_DESTROY:
		exit(4);
	default:
		fprintf(stderr, "vmexit_suspend: invalid reason %d\n", how);
		exit(100);
	}
	return (0);	/* NOTREACHED */
}

static int
vmexit_debug(struct vmctx *ctx __unused, struct vcpu *vcpu,
    struct vm_run *vmrun __unused)
{
	gdb_cpu_suspend(vcpu);
	/*
	 * XXX-MJ sleep for a short period to avoid chewing up the CPU in the
	 * window between activation of the vCPU thread and the STARTUP IPI.
	 */
	usleep(1000);
	return (VMEXIT_CONTINUE);
}

static int
vmexit_bogus(struct vmctx *ctx __unused, struct vcpu *vcpu __unused,
    struct vm_run *vmrun __unused)
{
	return (VMEXIT_CONTINUE);
}

static uint64_t
smccc_affinity_info(uint64_t target_affinity, uint32_t lowest_affinity_level)
{
	uint64_t mask = 0;

	switch (lowest_affinity_level) {
	case 0:
		mask |= CPU_AFF0_MASK;
		/* FALLTHROUGH */
	case 1:
		mask |= CPU_AFF1_MASK;
		/* FALLTHROUGH */
	case 2:
		mask |= CPU_AFF2_MASK;
		/* FALLTHROUGH */
	case 3:
		mask |= CPU_AFF3_MASK;
		break;
	default:
		return (PSCI_RETVAL_INVALID_PARAMS);
	}

	for (int vcpu = 0; vcpu < guest_ncpus; vcpu++) {
		if ((cpu_to_mpidr[vcpu] & mask) == (target_affinity & mask) &&
		    CPU_ISSET(vcpu, &running_cpumask)) {
			/* Return ON if any CPUs are on */
			return (PSCI_AFFINITY_INFO_ON);
		}
	}

	/* No CPUs in the affinity mask are on, return OFF */
	return (PSCI_AFFINITY_INFO_OFF);
}

static int
vmexit_smccc(struct vmctx *ctx, struct vcpu *vcpu, struct vm_run *vmrun)
{
	struct vcpu *newvcpu;
	struct vm_exit *vme;
	uint64_t mpidr, smccc_rv;
	enum vm_suspend_how how;
	int error, newcpu;

	/* Return the Unknown Function Identifier  by default */
	smccc_rv = SMCCC_RET_NOT_SUPPORTED;

	vme = vmrun->vm_exit;
	switch (vme->u.smccc_call.func_id) {
	case PSCI_FNID_VERSION:
		/* We implement PSCI 1.0 */
		smccc_rv = PSCI_VER(1, 0);
		break;
	case PSCI_FNID_CPU_SUSPEND:
		break;
	case PSCI_FNID_CPU_OFF:
		CPU_CLR_ATOMIC(vcpu_id(vcpu), &running_cpumask);
		vm_suspend_cpu(vcpu);
		break;
	case PSCI_FNID_CPU_ON:
		mpidr = vme->u.smccc_call.args[0];
		for (newcpu = 0; newcpu < guest_ncpus; newcpu++) {
			if (cpu_to_mpidr[newcpu] == mpidr)
				break;
		}

		if (newcpu == guest_ncpus) {
			smccc_rv = PSCI_RETVAL_INVALID_PARAMS;
			break;
		}

		if (CPU_TEST_SET_ATOMIC(newcpu, &running_cpumask)) {
			smccc_rv = PSCI_RETVAL_ALREADY_ON;
			break;
		}

		newvcpu = fbsdrun_vcpu(newcpu);
		assert(newvcpu != NULL);

		/* Set the context ID */
		error = vm_set_register(newvcpu, VM_REG_GUEST_X0,
		    vme->u.smccc_call.args[2]);
		assert(error == 0);

		/* Set the start program counter */
		error = vm_set_register(newvcpu, VM_REG_GUEST_PC,
		    vme->u.smccc_call.args[1]);
		assert(error == 0);

		vm_resume_cpu(newvcpu);

		smccc_rv = PSCI_RETVAL_SUCCESS;
		break;
	case PSCI_FNID_AFFINITY_INFO:
		smccc_rv = smccc_affinity_info(vme->u.smccc_call.args[0],
		    vme->u.smccc_call.args[1]);
		break;
	case PSCI_FNID_SYSTEM_OFF:
	case PSCI_FNID_SYSTEM_RESET:
		if (vme->u.smccc_call.func_id == PSCI_FNID_SYSTEM_OFF)
			how = VM_SUSPEND_POWEROFF;
		else
			how = VM_SUSPEND_RESET;
		error = vm_suspend(ctx, how);
		assert(error == 0 || errno == EALREADY);
		break;
	default:
		break;
	}

	error = vm_set_register(vcpu, VM_REG_GUEST_X0, smccc_rv);
	assert(error == 0);

	return (VMEXIT_CONTINUE);
}

static int
vmexit_hyp(struct vmctx *ctx __unused, struct vcpu *vcpu, struct vm_run *vmrun)
{
	/* Raise an unknown reason exception */
	if (vm_inject_exception(vcpu,
	    (EXCP_UNKNOWN << ESR_ELx_EC_SHIFT) | ESR_ELx_IL,
	    vmrun->vm_exit->u.hyp.far_el2) != 0)
		return (VMEXIT_ABORT);

	return (VMEXIT_CONTINUE);
}

static int
vmexit_brk(struct vmctx *ctx __unused, struct vcpu *vcpu, struct vm_run *vmrun)
{
	gdb_cpu_breakpoint(vcpu, vmrun->vm_exit);
	return (VMEXIT_CONTINUE);
}

static int
vmexit_ss(struct vmctx *ctx __unused, struct vcpu *vcpu, struct vm_run *vmrun)
{
	gdb_cpu_debug(vcpu, vmrun->vm_exit);
	return (VMEXIT_CONTINUE);
}

const vmexit_handler_t vmexit_handlers[VM_EXITCODE_MAX] = {
	[VM_EXITCODE_BOGUS]  = vmexit_bogus,
	[VM_EXITCODE_INST_EMUL] = vmexit_inst_emul,
	[VM_EXITCODE_REG_EMUL] = vmexit_reg_emul,
	[VM_EXITCODE_SUSPENDED] = vmexit_suspend,
	[VM_EXITCODE_DEBUG] = vmexit_debug,
	[VM_EXITCODE_SMCCC] = vmexit_smccc,
	[VM_EXITCODE_HYP] = vmexit_hyp,
	[VM_EXITCODE_BRK] = vmexit_brk,
	[VM_EXITCODE_SS] = vmexit_ss,
};
