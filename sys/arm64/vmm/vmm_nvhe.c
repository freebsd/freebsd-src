/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Andrew Turner
 * Copyright (c) 2024 Arm Ltd
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#define	VMM_STATIC	static
#define	VMM_HYP_FUNC(func)	vmm_nvhe_ ## func

#define	guest_or_nonvhe(guest)	(true)
#define	EL1_REG(reg)		MRS_REG_ALT_NAME(reg ## _EL1)
#define	EL0_REG(reg)		MRS_REG_ALT_NAME(reg ## _EL0)

#include "vmm_hyp.c"

uint64_t vmm_hyp_enter(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
    uint64_t, uint64_t, uint64_t);

/*
 * Handlers for EL2 addres space. Only needed by non-VHE code as in VHE the
 * kernel is in EL2 so pmap will manage the address space.
 */
static int
vmm_dc_civac(uint64_t start, uint64_t len)
{
	size_t line_size, end;
	uint64_t ctr;

	ctr = READ_SPECIALREG(ctr_el0);
	line_size = sizeof(int) << CTR_DLINE_SIZE(ctr);
	end = start + len;
	dsb(ishst);
	/* Clean and Invalidate the D-cache */
	for (; start < end; start += line_size)
		__asm __volatile("dc	civac, %0" :: "r" (start) : "memory");
	dsb(ish);
	return (0);
}

static int
vmm_el2_tlbi(uint64_t type, uint64_t start, uint64_t len)
{
	uint64_t end, r;

	dsb(ishst);
	switch (type) {
	default:
	case HYP_EL2_TLBI_ALL:
		__asm __volatile("tlbi	alle2" ::: "memory");
		break;
	case HYP_EL2_TLBI_VA:
		end = TLBI_VA(start + len);
		start = TLBI_VA(start);
		for (r = start; r < end; r += TLBI_VA_L3_INCR) {
			__asm __volatile("tlbi	vae2is, %0" :: "r"(r));
		}
		break;
	}
	dsb(ish);

	return (0);
}

uint64_t
vmm_hyp_enter(uint64_t handle, uint64_t x1, uint64_t x2, uint64_t x3,
    uint64_t x4, uint64_t x5, uint64_t x6, uint64_t x7)
{
	switch (handle) {
	case HYP_ENTER_GUEST:
		return (VMM_HYP_FUNC(enter_guest)((struct hyp *)x1,
		    (struct hypctx *)x2));
	case HYP_READ_REGISTER:
		return (VMM_HYP_FUNC(read_reg)(x1));
	case HYP_CLEAN_S2_TLBI:
		VMM_HYP_FUNC(clean_s2_tlbi());
		return (0);
	case HYP_DC_CIVAC:
		return (vmm_dc_civac(x1, x2));
	case HYP_EL2_TLBI:
		return (vmm_el2_tlbi(x1, x2, x3));
	case HYP_S2_TLBI_RANGE:
		VMM_HYP_FUNC(s2_tlbi_range)(x1, x2, x3, x4);
		return (0);
	case HYP_S2_TLBI_ALL:
		VMM_HYP_FUNC(s2_tlbi_all)(x1);
		return (0);
	case HYP_CLEANUP:	/* Handled in vmm_hyp_exception.S */
	default:
		break;
	}

	return (0);
}
