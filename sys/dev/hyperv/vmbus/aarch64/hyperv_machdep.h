/*- SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Microsoft Corp.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _HYPERV_MACHDEP_H_
#define _HYPERV_MACHDEP_H_

#include <sys/param.h>

typedef uint32_t u32;
typedef uint64_t u64;
struct hv_get_vp_registers_output {
	union {
		struct {
			u32 a;
			u32 b;
			u32 c;
			u32 d;
		} as32 __packed;
		struct {
			u64 low;
			u64 high;
		} as64 __packed;
	};
};

uint64_t hypercall_md(volatile void *hc_addr, u64 in_val,
    u64 in_paddr, u64 out_paddr);
void hv_get_vpreg_128(u32, struct hv_get_vp_registers_output *);
void arm_hv_set_vreg(u32 msr, u64 val);
#define WRMSR(msr, val) arm_hv_set_vreg(msr, val)
u64 arm_hv_get_vreg(u32 msr);
#define RDMSR(msr) arm_hv_get_vreg(msr)

#endif /* !_HYPERV_MACHDEP_H_ */
