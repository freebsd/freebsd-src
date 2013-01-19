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

#ifndef _VMX_MSR_H_
#define	_VMX_MSR_H_

#define	MSR_VMX_BASIC			0x480
#define	MSR_VMX_EPT_VPID_CAP		0x48C

#define	MSR_VMX_PROCBASED_CTLS		0x482
#define	MSR_VMX_TRUE_PROCBASED_CTLS	0x48E

#define	MSR_VMX_PINBASED_CTLS		0x481
#define	MSR_VMX_TRUE_PINBASED_CTLS	0x48D

#define	MSR_VMX_PROCBASED_CTLS2		0x48B

#define	MSR_VMX_EXIT_CTLS		0x483
#define	MSR_VMX_TRUE_EXIT_CTLS		0x48f

#define	MSR_VMX_ENTRY_CTLS		0x484
#define	MSR_VMX_TRUE_ENTRY_CTLS		0x490

#define	MSR_VMX_CR0_FIXED0		0x486
#define	MSR_VMX_CR0_FIXED1		0x487

#define	MSR_VMX_CR4_FIXED0		0x488
#define	MSR_VMX_CR4_FIXED1		0x489

uint32_t vmx_revision(void);

int vmx_set_ctlreg(int ctl_reg, int true_ctl_reg, uint32_t ones_mask,
		   uint32_t zeros_mask, uint32_t *retval);

/*
 * According to Section 21.10.4 "Software Access to Related Structures",
 * changes to data structures pointed to by the VMCS must be made only when
 * there is no logical processor with a current VMCS that points to the
 * data structure.
 *
 * This pretty much limits us to configuring the MSR bitmap before VMCS
 * initialization for SMP VMs. Unless of course we do it the hard way - which
 * would involve some form of synchronization between the vcpus to vmclear
 * all VMCSs' that point to the bitmap.
 */
#define	MSR_BITMAP_ACCESS_NONE	0x0
#define	MSR_BITMAP_ACCESS_READ	0x1
#define	MSR_BITMAP_ACCESS_WRITE	0x2
#define	MSR_BITMAP_ACCESS_RW	(MSR_BITMAP_ACCESS_READ|MSR_BITMAP_ACCESS_WRITE)
void	msr_bitmap_initialize(char *bitmap);
int	msr_bitmap_change_access(char *bitmap, u_int msr, int access);

#endif
