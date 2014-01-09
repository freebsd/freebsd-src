/*-
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

#ifndef _VMX_H_
#define	_VMX_H_

#include "vmcs.h"

struct pmap;

#define	GUEST_MSR_MAX_ENTRIES	64		/* arbitrary */

struct vmxctx {
	register_t	guest_rdi;		/* Guest state */
	register_t	guest_rsi;
	register_t	guest_rdx;
	register_t	guest_rcx;
	register_t	guest_r8;
	register_t	guest_r9;
	register_t	guest_rax;
	register_t	guest_rbx;
	register_t	guest_rbp;
	register_t	guest_r10;
	register_t	guest_r11;
	register_t	guest_r12;
	register_t	guest_r13;
	register_t	guest_r14;
	register_t	guest_r15;
	register_t	guest_cr2;

	register_t	host_r15;		/* Host state */
	register_t	host_r14;
	register_t	host_r13;
	register_t	host_r12;
	register_t	host_rbp;
	register_t	host_rsp;
	register_t	host_rbx;
	register_t	host_rip;
	/*
	 * XXX todo debug registers and fpu state
	 */
	
	int		inst_fail_status;

	long		eptgen[MAXCPU];		/* cached pmap->pm_eptgen */

	/*
	 * The 'eptp' and the 'pmap' do not change during the lifetime of
	 * the VM so it is safe to keep a copy in each vcpu's vmxctx.
	 */
	vm_paddr_t	eptp;
	struct pmap	*pmap;
};

struct vmxcap {
	int	set;
	uint32_t proc_ctls;
	uint32_t proc_ctls2;
};

struct vmxstate {
	int	lastcpu;	/* host cpu that this 'vcpu' last ran on */
	uint16_t vpid;
};

struct apic_page {
	uint32_t reg[PAGE_SIZE / 4];
};
CTASSERT(sizeof(struct apic_page) == PAGE_SIZE);

/* virtual machine softc */
struct vmx {
	struct vmcs	vmcs[VM_MAXCPU];	/* one vmcs per virtual cpu */
	struct apic_page apic_page[VM_MAXCPU];	/* one apic page per vcpu */
	char		msr_bitmap[PAGE_SIZE];
	struct msr_entry guest_msrs[VM_MAXCPU][GUEST_MSR_MAX_ENTRIES];
	struct vmxctx	ctx[VM_MAXCPU];
	struct vmxcap	cap[VM_MAXCPU];
	struct vmxstate	state[VM_MAXCPU];
	uint64_t	eptp;
	struct vm	*vm;
};
CTASSERT((offsetof(struct vmx, vmcs) & PAGE_MASK) == 0);
CTASSERT((offsetof(struct vmx, msr_bitmap) & PAGE_MASK) == 0);
CTASSERT((offsetof(struct vmx, guest_msrs) & 15) == 0);

#define	VMX_GUEST_VMEXIT	0
#define	VMX_VMRESUME_ERROR	1
#define	VMX_VMLAUNCH_ERROR	2
#define	VMX_INVEPT_ERROR	3
int	vmx_enter_guest(struct vmxctx *ctx, int launched);
void	vmx_exit_guest(void);

u_long	vmx_fix_cr0(u_long cr0);
u_long	vmx_fix_cr4(u_long cr4);

#endif
