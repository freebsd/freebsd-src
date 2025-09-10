/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 * Copyright (c) 2024 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) under Innovate
 * UK project 105694, "Digital Security by Design (DSbD) Technology Platform
 * Prototype".
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

#include <machine/riscvreg.h>
#include <machine/cpu.h>
#include <machine/sbi.h>
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

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "mem.h"
#include "vmexit.h"
#include "riscv.h"

#define	BHYVE_VERSION	((uint64_t)__FreeBSD_version)
#define	SBI_VERS_MAJOR	2
#define	SBI_VERS_MINOR	0

static cpuset_t running_hartmask = CPUSET_T_INITIALIZER(0);

void
vmexit_set_bsp(int hart_id)
{

	CPU_SET_ATOMIC(hart_id, &running_hartmask);
}

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

	/* NOT REACHED. */

	return (0);
}

static int
vmexit_debug(struct vmctx *ctx __unused, struct vcpu *vcpu __unused,
    struct vm_run *vmrun __unused)
{

	/*
	 * XXX-MJ sleep for a short period to avoid chewing up the CPU in the
	 * window between activation of the vCPU thread and the
	 * SBI_HSM_HART_START request.
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

static int
vmm_sbi_probe_extension(int ext_id)
{

	switch (ext_id) {
	case SBI_EXT_ID_HSM:
	case SBI_EXT_ID_TIME:
	case SBI_EXT_ID_IPI:
	case SBI_EXT_ID_RFNC:
	case SBI_EXT_ID_SRST:
	case SBI_CONSOLE_PUTCHAR:
	case SBI_CONSOLE_GETCHAR:
		break;
	default:
		return (0);
	}

	return (1);
}

static int
vmexit_ecall_hsm(struct vmctx *ctx __unused, struct vcpu *vcpu __unused,
    struct vm_exit *vme)
{
	struct vcpu *newvcpu;
	uint64_t hart_id;
	int func_id;
	int error;

	hart_id = vme->u.ecall.args[0];
	func_id = vme->u.ecall.args[6];

	if (HART_TO_CPU(hart_id) >= (uint64_t)guest_ncpus)
		return (SBI_ERR_INVALID_PARAM);

	newvcpu = fbsdrun_vcpu(HART_TO_CPU(hart_id));
	assert(newvcpu != NULL);

	switch (func_id) {
	case SBI_HSM_HART_START:
		if (CPU_ISSET(hart_id, &running_hartmask))
			break;

		/* Set hart ID. */
		error = vm_set_register(newvcpu, VM_REG_GUEST_A0, hart_id);
		assert(error == 0);

		/* Set PC. */
		error = vm_set_register(newvcpu, VM_REG_GUEST_SEPC,
		    vme->u.ecall.args[1]);
		assert(error == 0);

		/* Pass private data. */
		error = vm_set_register(newvcpu, VM_REG_GUEST_A1,
		    vme->u.ecall.args[2]);
		assert(error == 0);

		vm_resume_cpu(newvcpu);
		CPU_SET_ATOMIC(hart_id, &running_hartmask);
		break;
	case SBI_HSM_HART_STOP:
		if (!CPU_ISSET(hart_id, &running_hartmask))
			break;
		CPU_CLR_ATOMIC(hart_id, &running_hartmask);
		vm_suspend_cpu(newvcpu);
		break;
	case SBI_HSM_HART_STATUS:
		/* TODO. */
		break;
	default:
		return (SBI_ERR_NOT_SUPPORTED);
	}

	return (SBI_SUCCESS);
}

static int
vmexit_ecall_base(struct vmctx *ctx __unused, struct vcpu *vcpu,
    struct vm_exit *vme)
{
	int sbi_function_id;
	uint32_t val;
	int ext_id;
	int error;

	sbi_function_id = vme->u.ecall.args[6];

	switch (sbi_function_id) {
	case SBI_BASE_GET_SPEC_VERSION:
		val = SBI_VERS_MAJOR << SBI_SPEC_VERS_MAJOR_OFFSET;
		val |= SBI_VERS_MINOR << SBI_SPEC_VERS_MINOR_OFFSET;
		break;
	case SBI_BASE_GET_IMPL_ID:
		val = SBI_IMPL_ID_BHYVE;
		break;
	case SBI_BASE_GET_IMPL_VERSION:
		val = BHYVE_VERSION;
		break;
	case SBI_BASE_PROBE_EXTENSION:
		ext_id = vme->u.ecall.args[0];
		val = vmm_sbi_probe_extension(ext_id);
		break;
	case SBI_BASE_GET_MVENDORID:
		val = MVENDORID_UNIMPL;
		break;
	case SBI_BASE_GET_MARCHID:
		val = MARCHID_UNIMPL;
		break;
	case SBI_BASE_GET_MIMPID:
		val = 0;
		break;
	default:
		return (SBI_ERR_NOT_SUPPORTED);
	}

	error = vm_set_register(vcpu, VM_REG_GUEST_A1, val);
	assert(error == 0);

	return (SBI_SUCCESS);
}

static int
vmexit_ecall_srst(struct vmctx *ctx, struct vm_exit *vme)
{
	enum vm_suspend_how how;
	int func_id;
	int type;

	func_id = vme->u.ecall.args[6];
	type = vme->u.ecall.args[0];

	switch (func_id) {
	case SBI_SRST_SYSTEM_RESET:
		switch (type) {
		case SBI_SRST_TYPE_SHUTDOWN:
		case SBI_SRST_TYPE_COLD_REBOOT:
		case SBI_SRST_TYPE_WARM_REBOOT:
			how = VM_SUSPEND_POWEROFF;
			vm_suspend(ctx, how);
			break;
		default:
			return (SBI_ERR_NOT_SUPPORTED);
		}
		break;
	default:
		return (SBI_ERR_NOT_SUPPORTED);
	}

	return (SBI_SUCCESS);
}

static int
vmexit_ecall(struct vmctx *ctx, struct vcpu *vcpu, struct vm_run *vmrun)
{
	int sbi_extension_id;
	struct vm_exit *vme;
	int error;
	int ret;

	vme = vmrun->vm_exit;

	sbi_extension_id = vme->u.ecall.args[7];
	switch (sbi_extension_id) {
	case SBI_EXT_ID_SRST:
		ret = vmexit_ecall_srst(ctx, vme);
		break;
	case SBI_EXT_ID_BASE:
		ret = vmexit_ecall_base(ctx, vcpu, vme);
		break;
	case SBI_EXT_ID_HSM:
		ret = vmexit_ecall_hsm(ctx, vcpu, vme);
		break;
	case SBI_CONSOLE_PUTCHAR:
	case SBI_CONSOLE_GETCHAR:
	default:
		/* Unknown SBI extension. */
		ret = SBI_ERR_NOT_SUPPORTED;
		break;
	}

	error = vm_set_register(vcpu, VM_REG_GUEST_A0, ret);
	assert(error == 0);

	return (VMEXIT_CONTINUE);
}


static int
vmexit_hyp(struct vmctx *ctx __unused, struct vcpu *vcpu __unused,
    struct vm_run *vmrun)
{
	struct vm_exit *vme;

	vme = vmrun->vm_exit;

	printf("unhandled exception: scause %#lx\n", vme->u.hyp.scause);

	return (VMEXIT_ABORT);
}

const vmexit_handler_t vmexit_handlers[VM_EXITCODE_MAX] = {
	[VM_EXITCODE_BOGUS]  = vmexit_bogus,
	[VM_EXITCODE_HYP] = vmexit_hyp,
	[VM_EXITCODE_INST_EMUL] = vmexit_inst_emul,
	[VM_EXITCODE_SUSPENDED] = vmexit_suspend,
	[VM_EXITCODE_DEBUG] = vmexit_debug,
	[VM_EXITCODE_ECALL] = vmexit_ecall,
};
