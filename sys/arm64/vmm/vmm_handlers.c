/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd
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

#include <sys/types.h>
#include <sys/systm.h>

#include <machine/ifunc.h>

#include "arm64.h"
#include "vmm_handlers.h"

uint64_t
vmm_read_reg(uint64_t reg)
{
	return (vmm_call_hyp(HYP_READ_REGISTER, reg));
}

uint64_t
vmm_enter_guest(struct hyp *hyp, struct hypctx *hypctx)
{
	return (vmm_call_hyp(HYP_ENTER_GUEST, hyp->el2_addr, hypctx->el2_addr));
}

void
vmm_clean_s2_tlbi(void)
{
	vmm_call_hyp(HYP_CLEAN_S2_TLBI);
}

void
vmm_s2_tlbi_range(uint64_t vttbr, vm_offset_t sva, vm_offset_t eva,
    bool final_only)
{
	vmm_call_hyp(HYP_S2_TLBI_RANGE, vttbr, sva, eva, final_only);
}

void
vmm_s2_tlbi_all(uint64_t vttbr)
{
	vmm_call_hyp(HYP_S2_TLBI_ALL, vttbr);
}
