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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <x86/apicreg.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <machine/vmm.h>
#include <vmmapi.h>

#include "fbsdrun.h"
#include "xmsr.h"

/*
 * Trampoline for hypervisor direct 64-bit jump.
 *
 *   0  - signature for guest->host verification
 *   8  - kernel virtual address of trampoline
 *  16  - instruction virtual address
 *  24  - stack pointer virtual address
 *  32  - CR3, physical address of kernel page table
 *  40  - 24-byte area for null/code/data GDT entries
 */
#define MP_V64T_SIG	0xcafebabecafebabeULL
struct mp_v64tramp {
	uint64_t	mt_sig;
	uint64_t	mt_virt;
	uint64_t	mt_eip;
	uint64_t	mt_rsp;
	uint64_t	mt_cr3;
	uint64_t	mt_gdtr[3];
};

/*
 * CPU 0 is considered to be the BSP and is set to the RUNNING state.
 * All other CPUs are set up in the INIT state.
 */
#define BSP  0
enum cpu_bstate {
	CPU_S_INIT,
	CPU_S_SIPI,
	CPU_S_RUNNING
} static cpu_b[VM_MAXCPU] = { [BSP] = CPU_S_RUNNING };

static void spinup_ap(struct vmctx *, int, int, uint64_t *);
static void spinup_ap_direct64(struct vmctx *, int, uintptr_t, uint64_t *);

int
emulate_wrmsr(struct vmctx *ctx, int vcpu, uint32_t code, uint64_t val)
{
	int dest;
	int mode;
	int thiscpu;
	int vec;
	int error, retval;
	uint64_t rip;

	retval = vcpu;
	thiscpu = 1 << vcpu;

	/*
	 * The only MSR value handled is the x2apic CR register
	 */
	if (code != 0x830) {
		printf("Unknown WRMSR code %x, val %lx, cpu %d\n",
		       code, val, vcpu);
		exit(1);
	}

	/*
	 * The value written to the MSR will generate an IPI to
	 * a set of CPUs. If this is a SIPI, create the initial
	 * state for the CPU and switch to it. Otherwise, inject
	 * an interrupt for the destination CPU(s), and request
	 * a switch to the next available one by returning -1
	 */
	dest = val >> 32;
	vec = val & APIC_VECTOR_MASK;
	mode = val & APIC_DELMODE_MASK;

	switch (mode) {
	case APIC_DELMODE_INIT:		
		/*
		 * Ignore legacy de-assert INITs in x2apic mode
		 */
		if ((val & APIC_LEVEL_MASK) == APIC_LEVEL_DEASSERT) {
			break;
		}

		assert(dest != 0);
		assert(dest < guest_ncpus);
		assert(cpu_b[dest] == CPU_S_INIT);

		/*
		 * Move CPU to wait-for-SIPI state
		 */
		error = vcpu_reset(ctx, dest);
		assert(error == 0);

		cpu_b[dest] = CPU_S_SIPI;
		break;

	case APIC_DELMODE_STARTUP:
		assert(dest != 0);
		assert(dest < guest_ncpus);
		/*
		 * Ignore SIPIs in any state other than wait-for-SIPI
		 */
		if (cpu_b[dest] != CPU_S_SIPI) {
			break;
		}

		/*
		 * Bring up the AP and signal the main loop that it is
		 * available and to switch to it.
		 */
		spinup_ap(ctx, dest, vec, &rip);
		cpu_b[dest] = CPU_S_RUNNING;
		fbsdrun_addcpu(ctx, dest, rip);
		retval = dest;
		break;

	default:
		printf("APIC delivery mode %lx not supported!\n",
		       val & APIC_DELMODE_MASK);
		exit(1);
	}

	return (retval);
}

/*
 * There are 2 startup modes possible here:
 *  - if the CPU supports 'unrestricted guest' mode, the spinup can
 *    set up the processor state in power-on 16-bit mode, with the CS:IP
 *    init'd to the specified low-mem 4K page.
 *  - if the guest has requested a 64-bit trampoline in the low-mem 4K
 *    page by placing in the specified signature, set up the register
 *    state using register state in the signature. Note that this
 *    requires accessing guest physical memory to read the signature
 *    while 'unrestricted mode' does not.
 */
static void
spinup_ap(struct vmctx *ctx, int newcpu, int vector, uint64_t *rip)
{
	int error;
	uint16_t cs;
	uint64_t desc_base;
	uint32_t desc_limit, desc_access;

	if (fbsdrun_vmexit_on_hlt()) {
		error = vm_set_capability(ctx, newcpu, VM_CAP_HALT_EXIT, 1);
		assert(error == 0);
	}

	if (fbsdrun_vmexit_on_pause()) {
		error = vm_set_capability(ctx, newcpu, VM_CAP_PAUSE_EXIT, 1);
		assert(error == 0);
	}

	error = vm_set_capability(ctx, newcpu, VM_CAP_UNRESTRICTED_GUEST, 1);
	if (error) {
		/*
		 * If the guest does not support real-mode execution then
		 * we will bring up the AP directly in 64-bit mode.
		 */
		spinup_ap_direct64(ctx, newcpu, vector << PAGE_SHIFT, rip);
	} else {
		/*
		 * Update the %cs and %rip of the guest so that it starts
		 * executing real mode code at at 'vector << 12'.
		 */
		*rip = 0;
		error = vm_set_register(ctx, newcpu, VM_REG_GUEST_RIP, *rip);
		assert(error == 0);

		error = vm_get_desc(ctx, newcpu, VM_REG_GUEST_CS, &desc_base,
				    &desc_limit, &desc_access);
		assert(error == 0);

		desc_base = vector << PAGE_SHIFT;
		error = vm_set_desc(ctx, newcpu, VM_REG_GUEST_CS,
				    desc_base, desc_limit, desc_access);
		assert(error == 0);

		cs = (vector << PAGE_SHIFT) >> 4;
		error = vm_set_register(ctx, newcpu, VM_REG_GUEST_CS, cs);
		assert(error == 0);
	}
}

static void
spinup_ap_direct64(struct vmctx *ctx, int newcpu, uintptr_t gaddr,
	uint64_t *rip)
{
	struct mp_v64tramp *mvt;
	char *errstr;
	int error;
	uint64_t gdtbase;

	mvt = paddr_guest2host(gaddr);

	assert(mvt->mt_sig == MP_V64T_SIG);

	/*
	 * Set up the 3-entry GDT using memory supplied in the
	 * guest's trampoline structure.
	 */
	vm_setup_freebsd_gdt(mvt->mt_gdtr);

#define  CHECK_ERROR(msg) \
	if (error != 0) { \
		errstr = msg; \
		goto err_exit; \
	}

        /* entry point */
	*rip = mvt->mt_eip;

	/* Get the guest virtual address of the GDT */
        gdtbase = mvt->mt_virt + __offsetof(struct mp_v64tramp, mt_gdtr);

	error = vm_setup_freebsd_registers(ctx, newcpu, mvt->mt_eip,
					   mvt->mt_cr3, gdtbase, mvt->mt_rsp);
	CHECK_ERROR("vm_setup_freebsd_registers");

	return;
err_exit:
	printf("spinup_ap_direct64: machine state error: %s", errstr);
	exit(1);
}
