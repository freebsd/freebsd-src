/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
 *
 * $FreeBSD$
 */

#ifndef _X86_H_
#define	_X86_H_

#define CPUID_0000_0000 (0x0)
#define CPUID_0000_0001	(0x1)
#define CPUID_0000_0002 (0x2)
#define CPUID_0000_0003 (0x3)
#define CPUID_0000_0004 (0x4)
#define CPUID_0000_0006 (0x6)
#define CPUID_0000_0007 (0x7)
#define	CPUID_0000_000A	(0xA)
#define	CPUID_0000_000B	(0xB)
#define	CPUID_0000_000D	(0xD)
#define	CPUID_0000_000F	(0xF)
#define	CPUID_0000_0010	(0x10)
#define	CPUID_0000_0015	(0x15)
#define CPUID_8000_0000	(0x80000000)
#define CPUID_8000_0001	(0x80000001)
#define CPUID_8000_0002	(0x80000002)
#define CPUID_8000_0003	(0x80000003)
#define CPUID_8000_0004	(0x80000004)
#define CPUID_8000_0006	(0x80000006)
#define CPUID_8000_0007	(0x80000007)
#define CPUID_8000_0008	(0x80000008)
#define CPUID_8000_001D	(0x8000001D)
#define CPUID_8000_001E	(0x8000001E)

/*
 * CPUID instruction Fn0000_0001:
 */
#define CPUID_0000_0001_APICID_MASK			(0xff<<24)
#define CPUID_0000_0001_APICID_SHIFT			24

/*
 * CPUID instruction Fn0000_0001 ECX
 */
#define CPUID_0000_0001_FEAT0_VMX	(1<<5)

int x86_emulate_cpuid(struct vm *vm, int vcpu_id, uint64_t *rax, uint64_t *rbx,
    uint64_t *rcx, uint64_t *rdx);

enum vm_cpuid_capability {
	VCC_NONE,
	VCC_NO_EXECUTE,
	VCC_FFXSR,
	VCC_TCE,
	VCC_LAST
};

/*
 * Return 'true' if the capability 'cap' is enabled in this virtual cpu
 * and 'false' otherwise.
 */
bool vm_cpuid_capability(struct vm *vm, int vcpuid, enum vm_cpuid_capability);

#define VMM_MTRR_VAR_MAX 10
#define VMM_MTRR_DEF_MASK \
	(MTRR_DEF_ENABLE | MTRR_DEF_FIXED_ENABLE | MTRR_DEF_TYPE)
#define VMM_MTRR_PHYSBASE_MASK (MTRR_PHYSBASE_PHYSBASE | MTRR_PHYSBASE_TYPE)
#define VMM_MTRR_PHYSMASK_MASK (MTRR_PHYSMASK_PHYSMASK | MTRR_PHYSMASK_VALID)
struct vm_mtrr {
	uint64_t def_type;
	uint64_t fixed4k[8];
	uint64_t fixed16k[2];
	uint64_t fixed64k;
	struct {
		uint64_t base;
		uint64_t mask;
	} var[VMM_MTRR_VAR_MAX];
};

int vm_rdmtrr(struct vm_mtrr *mtrr, u_int num, uint64_t *val);
int vm_wrmtrr(struct vm_mtrr *mtrr, u_int num, uint64_t val);

#endif
