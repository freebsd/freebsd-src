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

#include <sys/param.h>
#include <sys/pcpu.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <machine/clock.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/vmm.h>

#include <dev/vmm/vmm_ktr.h>
#include <dev/vmm/vmm_vm.h>

#include "vmm_host.h"
#include "vmm_util.h"
#include "x86.h"

bool
vm_cpuid_capability(struct vcpu *vcpu, enum vm_cpuid_capability cap)
{
	bool rv;

	KASSERT(cap > 0 && cap < VCC_LAST, ("%s: invalid vm_cpu_capability %d",
	    __func__, cap));

	/*
	 * Simply passthrough the capabilities of the host cpu for now.
	 */
	rv = false;
	switch (cap) {
	case VCC_NO_EXECUTE:
		if (amd_feature & AMDID_NX)
			rv = true;
		break;
	case VCC_FFXSR:
		if (amd_feature & AMDID_FFXSR)
			rv = true;
		break;
	case VCC_TCE:
		if (amd_feature2 & AMDID2_TCE)
			rv = true;
		break;
	default:
		panic("%s: unknown vm_cpu_capability %d", __func__, cap);
	}
	return (rv);
}

int
vm_rdmtrr(struct vm_mtrr *mtrr, u_int num, uint64_t *val)
{
	switch (num) {
	case MSR_MTRRcap:
		*val = MTRR_CAP_WC | MTRR_CAP_FIXED | VMM_MTRR_VAR_MAX;
		break;
	case MSR_MTRRdefType:
		*val = mtrr->def_type;
		break;
	case MSR_MTRR4kBase ... MSR_MTRR4kBase + 7:
		*val = mtrr->fixed4k[num - MSR_MTRR4kBase];
		break;
	case MSR_MTRR16kBase ... MSR_MTRR16kBase + 1:
		*val = mtrr->fixed16k[num - MSR_MTRR16kBase];
		break;
	case MSR_MTRR64kBase:
		*val = mtrr->fixed64k;
		break;
	case MSR_MTRRVarBase ... MSR_MTRRVarBase + (VMM_MTRR_VAR_MAX * 2) - 1: {
		u_int offset = num - MSR_MTRRVarBase;
		if (offset % 2 == 0) {
			*val = mtrr->var[offset / 2].base;
		} else {
			*val = mtrr->var[offset / 2].mask;
		}
		break;
	}
	default:
		return (-1);
	}

	return (0);
}

int
vm_wrmtrr(struct vm_mtrr *mtrr, u_int num, uint64_t val)
{
	switch (num) {
	case MSR_MTRRcap:
		/* MTRRCAP is read only */
		return (-1);
	case MSR_MTRRdefType:
		if (val & ~VMM_MTRR_DEF_MASK) {
			/* generate #GP on writes to reserved fields */
			return (-1);
		}
		mtrr->def_type = val;
		break;
	case MSR_MTRR4kBase ... MSR_MTRR4kBase + 7:
		mtrr->fixed4k[num - MSR_MTRR4kBase] = val;
		break;
	case MSR_MTRR16kBase ... MSR_MTRR16kBase + 1:
		mtrr->fixed16k[num - MSR_MTRR16kBase] = val;
		break;
	case MSR_MTRR64kBase:
		mtrr->fixed64k = val;
		break;
	case MSR_MTRRVarBase ... MSR_MTRRVarBase + (VMM_MTRR_VAR_MAX * 2) - 1: {
		u_int offset = num - MSR_MTRRVarBase;
		if (offset % 2 == 0) {
			if (val & ~VMM_MTRR_PHYSBASE_MASK) {
				/* generate #GP on writes to reserved fields */
				return (-1);
			}
			mtrr->var[offset / 2].base = val;
		} else {
			if (val & ~VMM_MTRR_PHYSMASK_MASK) {
				/* generate #GP on writes to reserved fields */
				return (-1);
			}
			mtrr->var[offset / 2].mask = val;
		}
		break;
	}
	default:
		return (-1);
	}

	return (0);
}
