/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 * Copyright (c) 2018 Joyent, Inc.
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

#include <sys/cdefs.h>
#include "opt_bhyve_snapshot.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/smp.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/smr.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#include <machine/psl.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#include <machine/smp.h>
#include <machine/specialreg.h>
#include <machine/vmparam.h>

#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <machine/vmm_instruction_emul.h>
#include <machine/vmm_snapshot.h>

#include <dev/vmm/vmm_ktr.h>
#include <dev/vmm/vmm_mem.h>

#include "vmm_lapic.h"
#include "vmm_host.h"
#include "vmm_ioport.h"
#include "vmm_stat.h"
#include "vatpic.h"
#include "vlapic.h"
#include "vlapic_priv.h"

#include "ept.h"
#include "vmx_cpufunc.h"
#include "vmx.h"
#include "vmx_msr.h"
#include "x86.h"
#include "vmx_controls.h"
#include "io/ppt.h"

#define	PINBASED_CTLS_ONE_SETTING					\
	(PINBASED_EXTINT_EXITING	|				\
	 PINBASED_NMI_EXITING		|				\
	 PINBASED_VIRTUAL_NMI)
#define	PINBASED_CTLS_ZERO_SETTING	0

#define PROCBASED_CTLS_WINDOW_SETTING					\
	(PROCBASED_INT_WINDOW_EXITING	|				\
	 PROCBASED_NMI_WINDOW_EXITING)

#define	PROCBASED_CTLS_ONE_SETTING					\
	(PROCBASED_SECONDARY_CONTROLS	|				\
	 PROCBASED_MWAIT_EXITING	|				\
	 PROCBASED_MONITOR_EXITING	|				\
	 PROCBASED_IO_EXITING		|				\
	 PROCBASED_MSR_BITMAPS		|				\
	 PROCBASED_CTLS_WINDOW_SETTING	|				\
	 PROCBASED_CR8_LOAD_EXITING	|				\
	 PROCBASED_CR8_STORE_EXITING)
#define	PROCBASED_CTLS_ZERO_SETTING	\
	(PROCBASED_CR3_LOAD_EXITING |	\
	PROCBASED_CR3_STORE_EXITING |	\
	PROCBASED_IO_BITMAPS)

#define	PROCBASED_CTLS2_ONE_SETTING	PROCBASED2_ENABLE_EPT
#define	PROCBASED_CTLS2_ZERO_SETTING	0

#define	VM_EXIT_CTLS_ONE_SETTING					\
	(VM_EXIT_SAVE_DEBUG_CONTROLS		|			\
	VM_EXIT_HOST_LMA			|			\
	VM_EXIT_SAVE_EFER			|			\
	VM_EXIT_LOAD_EFER			|			\
	VM_EXIT_ACKNOWLEDGE_INTERRUPT)

#define	VM_EXIT_CTLS_ZERO_SETTING	0

#define	VM_ENTRY_CTLS_ONE_SETTING					\
	(VM_ENTRY_LOAD_DEBUG_CONTROLS		|			\
	VM_ENTRY_LOAD_EFER)

#define	VM_ENTRY_CTLS_ZERO_SETTING					\
	(VM_ENTRY_INTO_SMM			|			\
	VM_ENTRY_DEACTIVATE_DUAL_MONITOR)

#define	HANDLED		1
#define	UNHANDLED	0

static MALLOC_DEFINE(M_VMX, "vmx", "vmx");
static MALLOC_DEFINE(M_VLAPIC, "vlapic", "vlapic");

bool vmx_have_msr_tsc_aux;

SYSCTL_DECL(_hw_vmm);
SYSCTL_NODE(_hw_vmm, OID_AUTO, vmx, CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    NULL);

int vmxon_enabled[MAXCPU];
static uint8_t *vmxon_region;

static uint32_t pinbased_ctls, procbased_ctls, procbased_ctls2;
static uint32_t exit_ctls, entry_ctls;

static uint64_t cr0_ones_mask, cr0_zeros_mask;
SYSCTL_ULONG(_hw_vmm_vmx, OID_AUTO, cr0_ones_mask, CTLFLAG_RD,
	     &cr0_ones_mask, 0, NULL);
SYSCTL_ULONG(_hw_vmm_vmx, OID_AUTO, cr0_zeros_mask, CTLFLAG_RD,
	     &cr0_zeros_mask, 0, NULL);

static uint64_t cr4_ones_mask, cr4_zeros_mask;
SYSCTL_ULONG(_hw_vmm_vmx, OID_AUTO, cr4_ones_mask, CTLFLAG_RD,
	     &cr4_ones_mask, 0, NULL);
SYSCTL_ULONG(_hw_vmm_vmx, OID_AUTO, cr4_zeros_mask, CTLFLAG_RD,
	     &cr4_zeros_mask, 0, NULL);

static int vmx_initialized;
SYSCTL_INT(_hw_vmm_vmx, OID_AUTO, initialized, CTLFLAG_RD,
	   &vmx_initialized, 0, "Intel VMX initialized");

/*
 * Optional capabilities
 */
static SYSCTL_NODE(_hw_vmm_vmx, OID_AUTO, cap,
    CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    NULL);

static int cap_halt_exit;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, halt_exit, CTLFLAG_RD, &cap_halt_exit, 0,
    "HLT triggers a VM-exit");

static int cap_pause_exit;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, pause_exit, CTLFLAG_RD, &cap_pause_exit,
    0, "PAUSE triggers a VM-exit");

static int cap_wbinvd_exit;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, wbinvd_exit, CTLFLAG_RD, &cap_wbinvd_exit,
    0, "WBINVD triggers a VM-exit");

static int cap_rdpid;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, rdpid, CTLFLAG_RD, &cap_rdpid, 0,
    "Guests are allowed to use RDPID");

static int cap_rdtscp;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, rdtscp, CTLFLAG_RD, &cap_rdtscp, 0,
    "Guests are allowed to use RDTSCP");

static int cap_unrestricted_guest;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, unrestricted_guest, CTLFLAG_RD,
    &cap_unrestricted_guest, 0, "Unrestricted guests");

static int cap_monitor_trap;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, monitor_trap, CTLFLAG_RD,
    &cap_monitor_trap, 0, "Monitor trap flag");

static int cap_invpcid;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, invpcid, CTLFLAG_RD, &cap_invpcid,
    0, "Guests are allowed to use INVPCID");

static int tpr_shadowing;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, tpr_shadowing,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &tpr_shadowing, 0, "TPR shadowing support");

static int virtual_interrupt_delivery;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, virtual_interrupt_delivery,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &virtual_interrupt_delivery, 0, "APICv virtual interrupt delivery support");

static int posted_interrupts;
SYSCTL_INT(_hw_vmm_vmx_cap, OID_AUTO, posted_interrupts,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &posted_interrupts, 0, "APICv posted interrupt support");

static int pirvec = -1;
SYSCTL_INT(_hw_vmm_vmx, OID_AUTO, posted_interrupt_vector, CTLFLAG_RD,
    &pirvec, 0, "APICv posted interrupt vector");

static struct unrhdr *vpid_unr;
static u_int vpid_alloc_failed;
SYSCTL_UINT(_hw_vmm_vmx, OID_AUTO, vpid_alloc_failed, CTLFLAG_RD,
	    &vpid_alloc_failed, 0, NULL);

int guest_l1d_flush;
SYSCTL_INT(_hw_vmm_vmx, OID_AUTO, l1d_flush, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &guest_l1d_flush, 0, NULL);
int guest_l1d_flush_sw;
SYSCTL_INT(_hw_vmm_vmx, OID_AUTO, l1d_flush_sw, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &guest_l1d_flush_sw, 0, NULL);

static struct msr_entry msr_load_list[1] __aligned(16);

/*
 * The definitions of SDT probes for VMX.
 */

SDT_PROBE_DEFINE3(vmm, vmx, exit, entry,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE4(vmm, vmx, exit, taskswitch,
    "struct vmx *", "int", "struct vm_exit *", "struct vm_task_switch *");

SDT_PROBE_DEFINE4(vmm, vmx, exit, craccess,
    "struct vmx *", "int", "struct vm_exit *", "uint64_t");

SDT_PROBE_DEFINE4(vmm, vmx, exit, rdmsr,
    "struct vmx *", "int", "struct vm_exit *", "uint32_t");

SDT_PROBE_DEFINE5(vmm, vmx, exit, wrmsr,
    "struct vmx *", "int", "struct vm_exit *", "uint32_t", "uint64_t");

SDT_PROBE_DEFINE3(vmm, vmx, exit, halt,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE3(vmm, vmx, exit, mtrap,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE3(vmm, vmx, exit, pause,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE3(vmm, vmx, exit, intrwindow,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE4(vmm, vmx, exit, interrupt,
    "struct vmx *", "int", "struct vm_exit *", "uint32_t");

SDT_PROBE_DEFINE3(vmm, vmx, exit, nmiwindow,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE3(vmm, vmx, exit, inout,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE3(vmm, vmx, exit, cpuid,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE5(vmm, vmx, exit, exception,
    "struct vmx *", "int", "struct vm_exit *", "uint32_t", "int");

SDT_PROBE_DEFINE5(vmm, vmx, exit, nestedfault,
    "struct vmx *", "int", "struct vm_exit *", "uint64_t", "uint64_t");

SDT_PROBE_DEFINE4(vmm, vmx, exit, mmiofault,
    "struct vmx *", "int", "struct vm_exit *", "uint64_t");

SDT_PROBE_DEFINE3(vmm, vmx, exit, eoi,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE3(vmm, vmx, exit, apicaccess,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE4(vmm, vmx, exit, apicwrite,
    "struct vmx *", "int", "struct vm_exit *", "struct vlapic *");

SDT_PROBE_DEFINE3(vmm, vmx, exit, xsetbv,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE3(vmm, vmx, exit, monitor,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE3(vmm, vmx, exit, mwait,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE3(vmm, vmx, exit, vminsn,
    "struct vmx *", "int", "struct vm_exit *");

SDT_PROBE_DEFINE4(vmm, vmx, exit, unknown,
    "struct vmx *", "int", "struct vm_exit *", "uint32_t");

SDT_PROBE_DEFINE4(vmm, vmx, exit, return,
    "struct vmx *", "int", "struct vm_exit *", "int");

/*
 * Use the last page below 4GB as the APIC access address. This address is
 * occupied by the boot firmware so it is guaranteed that it will not conflict
 * with a page in system memory.
 */
#define	APIC_ACCESS_ADDRESS	0xFFFFF000

static int vmx_getdesc(void *vcpui, int reg, struct seg_desc *desc);
static int vmx_getreg(void *vcpui, int reg, uint64_t *retval);
static int vmxctx_setreg(struct vmxctx *vmxctx, int reg, uint64_t val);
static void vmx_inject_pir(struct vlapic *vlapic);
#ifdef BHYVE_SNAPSHOT
static int vmx_restore_tsc(void *vcpui, uint64_t now);
#endif

static inline bool
host_has_rdpid(void)
{
	return ((cpu_stdext_feature2 & CPUID_STDEXT2_RDPID) != 0);
}

static inline bool
host_has_rdtscp(void)
{
	return ((amd_feature & AMDID_RDTSCP) != 0);
}

#ifdef KTR
static const char *
exit_reason_to_str(int reason)
{
	static char reasonbuf[32];

	switch (reason) {
	case EXIT_REASON_EXCEPTION:
		return "exception";
	case EXIT_REASON_EXT_INTR:
		return "extint";
	case EXIT_REASON_TRIPLE_FAULT:
		return "triplefault";
	case EXIT_REASON_INIT:
		return "init";
	case EXIT_REASON_SIPI:
		return "sipi";
	case EXIT_REASON_IO_SMI:
		return "iosmi";
	case EXIT_REASON_SMI:
		return "smi";
	case EXIT_REASON_INTR_WINDOW:
		return "intrwindow";
	case EXIT_REASON_NMI_WINDOW:
		return "nmiwindow";
	case EXIT_REASON_TASK_SWITCH:
		return "taskswitch";
	case EXIT_REASON_CPUID:
		return "cpuid";
	case EXIT_REASON_GETSEC:
		return "getsec";
	case EXIT_REASON_HLT:
		return "hlt";
	case EXIT_REASON_INVD:
		return "invd";
	case EXIT_REASON_INVLPG:
		return "invlpg";
	case EXIT_REASON_RDPMC:
		return "rdpmc";
	case EXIT_REASON_RDTSC:
		return "rdtsc";
	case EXIT_REASON_RSM:
		return "rsm";
	case EXIT_REASON_VMCALL:
		return "vmcall";
	case EXIT_REASON_VMCLEAR:
		return "vmclear";
	case EXIT_REASON_VMLAUNCH:
		return "vmlaunch";
	case EXIT_REASON_VMPTRLD:
		return "vmptrld";
	case EXIT_REASON_VMPTRST:
		return "vmptrst";
	case EXIT_REASON_VMREAD:
		return "vmread";
	case EXIT_REASON_VMRESUME:
		return "vmresume";
	case EXIT_REASON_VMWRITE:
		return "vmwrite";
	case EXIT_REASON_VMXOFF:
		return "vmxoff";
	case EXIT_REASON_VMXON:
		return "vmxon";
	case EXIT_REASON_CR_ACCESS:
		return "craccess";
	case EXIT_REASON_DR_ACCESS:
		return "draccess";
	case EXIT_REASON_INOUT:
		return "inout";
	case EXIT_REASON_RDMSR:
		return "rdmsr";
	case EXIT_REASON_WRMSR:
		return "wrmsr";
	case EXIT_REASON_INVAL_VMCS:
		return "invalvmcs";
	case EXIT_REASON_INVAL_MSR:
		return "invalmsr";
	case EXIT_REASON_MWAIT:
		return "mwait";
	case EXIT_REASON_MTF:
		return "mtf";
	case EXIT_REASON_MONITOR:
		return "monitor";
	case EXIT_REASON_PAUSE:
		return "pause";
	case EXIT_REASON_MCE_DURING_ENTRY:
		return "mce-during-entry";
	case EXIT_REASON_TPR:
		return "tpr";
	case EXIT_REASON_APIC_ACCESS:
		return "apic-access";
	case EXIT_REASON_GDTR_IDTR:
		return "gdtridtr";
	case EXIT_REASON_LDTR_TR:
		return "ldtrtr";
	case EXIT_REASON_EPT_FAULT:
		return "eptfault";
	case EXIT_REASON_EPT_MISCONFIG:
		return "eptmisconfig";
	case EXIT_REASON_INVEPT:
		return "invept";
	case EXIT_REASON_RDTSCP:
		return "rdtscp";
	case EXIT_REASON_VMX_PREEMPT:
		return "vmxpreempt";
	case EXIT_REASON_INVVPID:
		return "invvpid";
	case EXIT_REASON_WBINVD:
		return "wbinvd";
	case EXIT_REASON_XSETBV:
		return "xsetbv";
	case EXIT_REASON_APIC_WRITE:
		return "apic-write";
	default:
		snprintf(reasonbuf, sizeof(reasonbuf), "%d", reason);
		return (reasonbuf);
	}
}
#endif	/* KTR */

static int
vmx_allow_x2apic_msrs(struct vmx *vmx)
{
	int i, error;

	error = 0;

	/*
	 * Allow readonly access to the following x2APIC MSRs from the guest.
	 */
	error += guest_msr_ro(vmx, MSR_APIC_ID);
	error += guest_msr_ro(vmx, MSR_APIC_VERSION);
	error += guest_msr_ro(vmx, MSR_APIC_LDR);
	error += guest_msr_ro(vmx, MSR_APIC_SVR);

	for (i = 0; i < 8; i++)
		error += guest_msr_ro(vmx, MSR_APIC_ISR0 + i);

	for (i = 0; i < 8; i++)
		error += guest_msr_ro(vmx, MSR_APIC_TMR0 + i);

	for (i = 0; i < 8; i++)
		error += guest_msr_ro(vmx, MSR_APIC_IRR0 + i);

	error += guest_msr_ro(vmx, MSR_APIC_ESR);
	error += guest_msr_ro(vmx, MSR_APIC_LVT_TIMER);
	error += guest_msr_ro(vmx, MSR_APIC_LVT_THERMAL);
	error += guest_msr_ro(vmx, MSR_APIC_LVT_PCINT);
	error += guest_msr_ro(vmx, MSR_APIC_LVT_LINT0);
	error += guest_msr_ro(vmx, MSR_APIC_LVT_LINT1);
	error += guest_msr_ro(vmx, MSR_APIC_LVT_ERROR);
	error += guest_msr_ro(vmx, MSR_APIC_ICR_TIMER);
	error += guest_msr_ro(vmx, MSR_APIC_DCR_TIMER);
	error += guest_msr_ro(vmx, MSR_APIC_ICR);

	/*
	 * Allow TPR, EOI and SELF_IPI MSRs to be read and written by the guest.
	 *
	 * These registers get special treatment described in the section
	 * "Virtualizing MSR-Based APIC Accesses".
	 */
	error += guest_msr_rw(vmx, MSR_APIC_TPR);
	error += guest_msr_rw(vmx, MSR_APIC_EOI);
	error += guest_msr_rw(vmx, MSR_APIC_SELF_IPI);

	return (error);
}

u_long
vmx_fix_cr0(u_long cr0)
{

	return ((cr0 | cr0_ones_mask) & ~cr0_zeros_mask);
}

u_long
vmx_fix_cr4(u_long cr4)
{

	return ((cr4 | cr4_ones_mask) & ~cr4_zeros_mask);
}

static void
vpid_free(int vpid)
{
	if (vpid < 0 || vpid > 0xffff)
		panic("vpid_free: invalid vpid %d", vpid);

	/*
	 * VPIDs [0,vm_maxcpu] are special and are not allocated from
	 * the unit number allocator.
	 */

	if (vpid > vm_maxcpu)
		free_unr(vpid_unr, vpid);
}

static uint16_t
vpid_alloc(int vcpuid)
{
	int x;

	/*
	 * If the "enable vpid" execution control is not enabled then the
	 * VPID is required to be 0 for all vcpus.
	 */
	if ((procbased_ctls2 & PROCBASED2_ENABLE_VPID) == 0)
		return (0);

	/*
	 * Try to allocate a unique VPID for each from the unit number
	 * allocator.
	 */
	x = alloc_unr(vpid_unr);

	if (x == -1) {
		atomic_add_int(&vpid_alloc_failed, 1);

		/*
		 * If the unit number allocator does not have enough unique
		 * VPIDs then we need to allocate from the [1,vm_maxcpu] range.
		 *
		 * These VPIDs are not be unique across VMs but this does not
		 * affect correctness because the combined mappings are also
		 * tagged with the EP4TA which is unique for each VM.
		 *
		 * It is still sub-optimal because the invvpid will invalidate
		 * combined mappings for a particular VPID across all EP4TAs.
		 */
		return (vcpuid + 1);
	}

	return (x);
}

static void
vpid_init(void)
{
	/*
	 * VPID 0 is required when the "enable VPID" execution control is
	 * disabled.
	 *
	 * VPIDs [1,vm_maxcpu] are used as the "overflow namespace" when the
	 * unit number allocator does not have sufficient unique VPIDs to
	 * satisfy the allocation.
	 *
	 * The remaining VPIDs are managed by the unit number allocator.
	 */
	vpid_unr = new_unrhdr(vm_maxcpu + 1, 0xffff, NULL);
}

static void
vmx_disable(void *arg __unused)
{
	struct invvpid_desc invvpid_desc = { 0 };
	struct invept_desc invept_desc = { 0 };

	if (vmxon_enabled[curcpu]) {
		/*
		 * See sections 25.3.3.3 and 25.3.3.4 in Intel Vol 3b.
		 *
		 * VMXON or VMXOFF are not required to invalidate any TLB
		 * caching structures. This prevents potential retention of
		 * cached information in the TLB between distinct VMX episodes.
		 */
		invvpid(INVVPID_TYPE_ALL_CONTEXTS, invvpid_desc);
		invept(INVEPT_TYPE_ALL_CONTEXTS, invept_desc);
		vmxoff();
	}
	load_cr4(rcr4() & ~CR4_VMXE);
}

static int
vmx_modcleanup(void)
{

	if (pirvec >= 0)
		lapic_ipi_free(pirvec);

	if (vpid_unr != NULL) {
		delete_unrhdr(vpid_unr);
		vpid_unr = NULL;
	}

	if (nmi_flush_l1d_sw == 1)
		nmi_flush_l1d_sw = 0;

	smp_rendezvous(NULL, vmx_disable, NULL, NULL);

	if (vmxon_region != NULL)
		kmem_free(vmxon_region, (mp_maxid + 1) * PAGE_SIZE);

	return (0);
}

static void
vmx_enable(void *arg __unused)
{
	int error;
	uint64_t feature_control;

	feature_control = rdmsr(MSR_IA32_FEATURE_CONTROL);
	if ((feature_control & IA32_FEATURE_CONTROL_LOCK) == 0 ||
	    (feature_control & IA32_FEATURE_CONTROL_VMX_EN) == 0) {
		wrmsr(MSR_IA32_FEATURE_CONTROL,
		    feature_control | IA32_FEATURE_CONTROL_VMX_EN |
		    IA32_FEATURE_CONTROL_LOCK);
	}

	load_cr4(rcr4() | CR4_VMXE);

	*(uint32_t *)&vmxon_region[curcpu * PAGE_SIZE] = vmx_revision();
	error = vmxon(&vmxon_region[curcpu * PAGE_SIZE]);
	if (error == 0)
		vmxon_enabled[curcpu] = 1;
}

static void
vmx_modsuspend(void)
{

	if (vmxon_enabled[curcpu])
		vmx_disable(NULL);
}

static void
vmx_modresume(void)
{

	if (vmxon_enabled[curcpu])
		vmx_enable(NULL);
}

static int
vmx_modinit(int ipinum)
{
	int error;
	uint64_t basic, fixed0, fixed1, feature_control;
	uint32_t tmp, procbased2_vid_bits;

	/* CPUID.1:ECX[bit 5] must be 1 for processor to support VMX */
	if (!(cpu_feature2 & CPUID2_VMX)) {
		printf("vmx_modinit: processor does not support VMX "
		    "operation\n");
		return (ENXIO);
	}

	/*
	 * Verify that MSR_IA32_FEATURE_CONTROL lock and VMXON enable bits
	 * are set (bits 0 and 2 respectively).
	 */
	feature_control = rdmsr(MSR_IA32_FEATURE_CONTROL);
	if ((feature_control & IA32_FEATURE_CONTROL_LOCK) == 1 &&
	    (feature_control & IA32_FEATURE_CONTROL_VMX_EN) == 0) {
		printf("vmx_modinit: VMX operation disabled by BIOS\n");
		return (ENXIO);
	}

	/*
	 * Verify capabilities MSR_VMX_BASIC:
	 * - bit 54 indicates support for INS/OUTS decoding
	 */
	basic = rdmsr(MSR_VMX_BASIC);
	if ((basic & (1UL << 54)) == 0) {
		printf("vmx_modinit: processor does not support desired basic "
		    "capabilities\n");
		return (EINVAL);
	}

	/* Check support for primary processor-based VM-execution controls */
	error = vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS,
			       MSR_VMX_TRUE_PROCBASED_CTLS,
			       PROCBASED_CTLS_ONE_SETTING,
			       PROCBASED_CTLS_ZERO_SETTING, &procbased_ctls);
	if (error) {
		printf("vmx_modinit: processor does not support desired "
		    "primary processor-based controls\n");
		return (error);
	}

	/* Clear the processor-based ctl bits that are set on demand */
	procbased_ctls &= ~PROCBASED_CTLS_WINDOW_SETTING;

	/* Check support for secondary processor-based VM-execution controls */
	error = vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2,
			       MSR_VMX_PROCBASED_CTLS2,
			       PROCBASED_CTLS2_ONE_SETTING,
			       PROCBASED_CTLS2_ZERO_SETTING, &procbased_ctls2);
	if (error) {
		printf("vmx_modinit: processor does not support desired "
		    "secondary processor-based controls\n");
		return (error);
	}

	/* Check support for VPID */
	error = vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2, MSR_VMX_PROCBASED_CTLS2,
			       PROCBASED2_ENABLE_VPID, 0, &tmp);
	if (error == 0)
		procbased_ctls2 |= PROCBASED2_ENABLE_VPID;

	/* Check support for pin-based VM-execution controls */
	error = vmx_set_ctlreg(MSR_VMX_PINBASED_CTLS,
			       MSR_VMX_TRUE_PINBASED_CTLS,
			       PINBASED_CTLS_ONE_SETTING,
			       PINBASED_CTLS_ZERO_SETTING, &pinbased_ctls);
	if (error) {
		printf("vmx_modinit: processor does not support desired "
		    "pin-based controls\n");
		return (error);
	}

	/* Check support for VM-exit controls */
	error = vmx_set_ctlreg(MSR_VMX_EXIT_CTLS, MSR_VMX_TRUE_EXIT_CTLS,
			       VM_EXIT_CTLS_ONE_SETTING,
			       VM_EXIT_CTLS_ZERO_SETTING,
			       &exit_ctls);
	if (error) {
		printf("vmx_modinit: processor does not support desired "
		    "exit controls\n");
		return (error);
	}

	/* Check support for VM-entry controls */
	error = vmx_set_ctlreg(MSR_VMX_ENTRY_CTLS, MSR_VMX_TRUE_ENTRY_CTLS,
	    VM_ENTRY_CTLS_ONE_SETTING, VM_ENTRY_CTLS_ZERO_SETTING,
	    &entry_ctls);
	if (error) {
		printf("vmx_modinit: processor does not support desired "
		    "entry controls\n");
		return (error);
	}

	/*
	 * Check support for optional features by testing them
	 * as individual bits
	 */
	cap_halt_exit = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS,
					MSR_VMX_TRUE_PROCBASED_CTLS,
					PROCBASED_HLT_EXITING, 0,
					&tmp) == 0);

	cap_monitor_trap = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS,
					MSR_VMX_PROCBASED_CTLS,
					PROCBASED_MTF, 0,
					&tmp) == 0);

	cap_pause_exit = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS,
					 MSR_VMX_TRUE_PROCBASED_CTLS,
					 PROCBASED_PAUSE_EXITING, 0,
					 &tmp) == 0);

	cap_wbinvd_exit = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2,
					MSR_VMX_PROCBASED_CTLS2,
					PROCBASED2_WBINVD_EXITING,
					0,
					&tmp) == 0);

	/*
	 * Check support for RDPID and/or RDTSCP.
	 *
	 * Support a pass-through-based implementation of these via the
	 * "enable RDTSCP" VM-execution control and the "RDTSC exiting"
	 * VM-execution control.
	 *
	 * The "enable RDTSCP" VM-execution control applies to both RDPID
	 * and RDTSCP (see SDM volume 3, section 25.3, "Changes to
	 * Instruction Behavior in VMX Non-root operation"); this is why
	 * only this VM-execution control needs to be enabled in order to
	 * enable passing through whichever of RDPID and/or RDTSCP are
	 * supported by the host.
	 *
	 * The "RDTSC exiting" VM-execution control applies to both RDTSC
	 * and RDTSCP (again, per SDM volume 3, section 25.3), and is
	 * already set up for RDTSC and RDTSCP pass-through by the current
	 * implementation of RDTSC.
	 *
	 * Although RDPID and RDTSCP are optional capabilities, since there
	 * does not currently seem to be a use case for enabling/disabling
	 * these via libvmmapi, choose not to support this and, instead,
	 * just statically always enable or always disable this support
	 * across all vCPUs on all VMs. (Note that there may be some
	 * complications to providing this functionality, e.g., the MSR
	 * bitmap is currently per-VM rather than per-vCPU while the
	 * capability API wants to be able to control capabilities on a
	 * per-vCPU basis).
	 */
	error = vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2,
			       MSR_VMX_PROCBASED_CTLS2,
			       PROCBASED2_ENABLE_RDTSCP, 0, &tmp);
	cap_rdpid = error == 0 && host_has_rdpid();
	cap_rdtscp = error == 0 && host_has_rdtscp();
	if (cap_rdpid || cap_rdtscp) {
		procbased_ctls2 |= PROCBASED2_ENABLE_RDTSCP;
		vmx_have_msr_tsc_aux = true;
	}

	cap_unrestricted_guest = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2,
					MSR_VMX_PROCBASED_CTLS2,
					PROCBASED2_UNRESTRICTED_GUEST, 0,
				        &tmp) == 0);

	cap_invpcid = (vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2,
	    MSR_VMX_PROCBASED_CTLS2, PROCBASED2_ENABLE_INVPCID, 0,
	    &tmp) == 0);

	/*
	 * Check support for TPR shadow.
	 */
	error = vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS,
	    MSR_VMX_TRUE_PROCBASED_CTLS, PROCBASED_USE_TPR_SHADOW, 0,
	    &tmp);
	if (error == 0) {
		tpr_shadowing = 1;
#ifndef BURN_BRIDGES
		TUNABLE_INT_FETCH("hw.vmm.vmx.use_tpr_shadowing",
		    &tpr_shadowing);
#endif
		TUNABLE_INT_FETCH("hw.vmm.vmx.cap.tpr_shadowing",
		    &tpr_shadowing);
	}

	if (tpr_shadowing) {
		procbased_ctls |= PROCBASED_USE_TPR_SHADOW;
		procbased_ctls &= ~PROCBASED_CR8_LOAD_EXITING;
		procbased_ctls &= ~PROCBASED_CR8_STORE_EXITING;
	}

	/*
	 * Check support for virtual interrupt delivery.
	 */
	procbased2_vid_bits = (PROCBASED2_VIRTUALIZE_APIC_ACCESSES |
	    PROCBASED2_VIRTUALIZE_X2APIC_MODE |
	    PROCBASED2_APIC_REGISTER_VIRTUALIZATION |
	    PROCBASED2_VIRTUAL_INTERRUPT_DELIVERY);

	error = vmx_set_ctlreg(MSR_VMX_PROCBASED_CTLS2, MSR_VMX_PROCBASED_CTLS2,
	    procbased2_vid_bits, 0, &tmp);
	if (error == 0 && tpr_shadowing) {
		virtual_interrupt_delivery = 1;
#ifndef BURN_BRIDGES
		TUNABLE_INT_FETCH("hw.vmm.vmx.use_apic_vid",
		    &virtual_interrupt_delivery);
#endif
		TUNABLE_INT_FETCH("hw.vmm.vmx.cap.virtual_interrupt_delivery",
		    &virtual_interrupt_delivery);
	}

	if (virtual_interrupt_delivery) {
		procbased_ctls |= PROCBASED_USE_TPR_SHADOW;
		procbased_ctls2 |= procbased2_vid_bits;
		procbased_ctls2 &= ~PROCBASED2_VIRTUALIZE_X2APIC_MODE;

		/*
		 * Check for Posted Interrupts only if Virtual Interrupt
		 * Delivery is enabled.
		 */
		error = vmx_set_ctlreg(MSR_VMX_PINBASED_CTLS,
		    MSR_VMX_TRUE_PINBASED_CTLS, PINBASED_POSTED_INTERRUPT, 0,
		    &tmp);
		if (error == 0) {
			pirvec = lapic_ipi_alloc(pti ? &IDTVEC(justreturn1_pti) :
			    &IDTVEC(justreturn));
			if (pirvec < 0) {
				if (bootverbose) {
					printf("vmx_modinit: unable to "
					    "allocate posted interrupt "
					    "vector\n");
				}
			} else {
				posted_interrupts = 1;
#ifndef BURN_BRIDGES
				TUNABLE_INT_FETCH("hw.vmm.vmx.use_apic_pir",
				    &posted_interrupts);
#endif
				TUNABLE_INT_FETCH("hw.vmm.vmx.cap.posted_interrupts",
				    &posted_interrupts);
			}
		}
	}

	if (posted_interrupts)
		    pinbased_ctls |= PINBASED_POSTED_INTERRUPT;

	/* Initialize EPT */
	error = ept_init(ipinum);
	if (error) {
		printf("vmx_modinit: ept initialization failed (%d)\n", error);
		return (error);
	}

	guest_l1d_flush = (cpu_ia32_arch_caps &
	    IA32_ARCH_CAP_SKIP_L1DFL_VMENTRY) == 0;
#ifndef BURN_BRIDGES
	TUNABLE_INT_FETCH("hw.vmm.l1d_flush", &guest_l1d_flush);
#endif
	TUNABLE_INT_FETCH("hw.vmm.vmx.l1d_flush", &guest_l1d_flush);

	/*
	 * L1D cache flush is enabled.  Use IA32_FLUSH_CMD MSR when
	 * available.  Otherwise fall back to the software flush
	 * method which loads enough data from the kernel text to
	 * flush existing L1D content, both on VMX entry and on NMI
	 * return.
	 */
	if (guest_l1d_flush) {
		if ((cpu_stdext_feature3 & CPUID_STDEXT3_L1D_FLUSH) == 0) {
			guest_l1d_flush_sw = 1;
#ifndef BURN_BRIDGES
			TUNABLE_INT_FETCH("hw.vmm.l1d_flush_sw",
			    &guest_l1d_flush_sw);
#endif
			TUNABLE_INT_FETCH("hw.vmm.vmx.l1d_flush_sw",
			    &guest_l1d_flush_sw);
		}
		if (guest_l1d_flush_sw) {
			if (nmi_flush_l1d_sw <= 1)
				nmi_flush_l1d_sw = 1;
		} else {
			msr_load_list[0].index = MSR_IA32_FLUSH_CMD;
			msr_load_list[0].val = IA32_FLUSH_CMD_L1D;
		}
	}

	/*
	 * Stash the cr0 and cr4 bits that must be fixed to 0 or 1
	 */
	fixed0 = rdmsr(MSR_VMX_CR0_FIXED0);
	fixed1 = rdmsr(MSR_VMX_CR0_FIXED1);
	cr0_ones_mask = fixed0 & fixed1;
	cr0_zeros_mask = ~fixed0 & ~fixed1;

	/*
	 * CR0_PE and CR0_PG can be set to zero in VMX non-root operation
	 * if unrestricted guest execution is allowed.
	 */
	if (cap_unrestricted_guest)
		cr0_ones_mask &= ~(CR0_PG | CR0_PE);

	/*
	 * Do not allow the guest to set CR0_NW or CR0_CD.
	 */
	cr0_zeros_mask |= (CR0_NW | CR0_CD);

	fixed0 = rdmsr(MSR_VMX_CR4_FIXED0);
	fixed1 = rdmsr(MSR_VMX_CR4_FIXED1);
	cr4_ones_mask = fixed0 & fixed1;
	cr4_zeros_mask = ~fixed0 & ~fixed1;

	vpid_init();

	vmx_msr_init();

	/* enable VMX operation */
	vmxon_region = kmem_malloc((mp_maxid + 1) * PAGE_SIZE,
	    M_WAITOK | M_ZERO);
	smp_rendezvous(NULL, vmx_enable, NULL, NULL);

	vmx_initialized = 1;

	return (0);
}

static void
vmx_trigger_hostintr(int vector)
{
	uintptr_t func;
	struct gate_descriptor *gd;

	gd = &idt[vector];

	KASSERT(vector >= 32 && vector <= 255, ("vmx_trigger_hostintr: "
	    "invalid vector %d", vector));
	KASSERT(gd->gd_p == 1, ("gate descriptor for vector %d not present",
	    vector));
	KASSERT(gd->gd_type == SDT_SYSIGT, ("gate descriptor for vector %d "
	    "has invalid type %d", vector, gd->gd_type));
	KASSERT(gd->gd_dpl == SEL_KPL, ("gate descriptor for vector %d "
	    "has invalid dpl %d", vector, gd->gd_dpl));
	KASSERT(gd->gd_selector == GSEL(GCODE_SEL, SEL_KPL), ("gate descriptor "
	    "for vector %d has invalid selector %d", vector, gd->gd_selector));
	KASSERT(gd->gd_ist == 0, ("gate descriptor for vector %d has invalid "
	    "IST %d", vector, gd->gd_ist));

	func = ((long)gd->gd_hioffset << 16 | gd->gd_looffset);
	vmx_call_isr(func);
}

static int
vmx_setup_cr_shadow(int which, struct vmcs *vmcs, uint32_t initial)
{
	int error, mask_ident, shadow_ident;
	uint64_t mask_value;

	if (which != 0 && which != 4)
		panic("vmx_setup_cr_shadow: unknown cr%d", which);

	if (which == 0) {
		mask_ident = VMCS_CR0_MASK;
		mask_value = cr0_ones_mask | cr0_zeros_mask;
		shadow_ident = VMCS_CR0_SHADOW;
	} else {
		mask_ident = VMCS_CR4_MASK;
		mask_value = cr4_ones_mask | cr4_zeros_mask;
		shadow_ident = VMCS_CR4_SHADOW;
	}

	error = vmcs_setreg(vmcs, 0, VMCS_IDENT(mask_ident), mask_value);
	if (error)
		return (error);

	error = vmcs_setreg(vmcs, 0, VMCS_IDENT(shadow_ident), initial);
	if (error)
		return (error);

	return (0);
}
#define	vmx_setup_cr0_shadow(vmcs,init)	vmx_setup_cr_shadow(0, (vmcs), (init))
#define	vmx_setup_cr4_shadow(vmcs,init)	vmx_setup_cr_shadow(4, (vmcs), (init))

static void *
vmx_init(struct vm *vm, pmap_t pmap)
{
	int error __diagused;
	struct vmx *vmx;

	vmx = malloc(sizeof(struct vmx), M_VMX, M_WAITOK | M_ZERO);
	vmx->vm = vm;

	vmx->eptp = eptp(vtophys((vm_offset_t)pmap->pm_pmltop));

	/*
	 * Clean up EPTP-tagged guest physical and combined mappings
	 *
	 * VMX transitions are not required to invalidate any guest physical
	 * mappings. So, it may be possible for stale guest physical mappings
	 * to be present in the processor TLBs.
	 *
	 * Combined mappings for this EP4TA are also invalidated for all VPIDs.
	 */
	ept_invalidate_mappings(vmx->eptp);

	vmx->msr_bitmap = malloc_aligned(PAGE_SIZE, PAGE_SIZE, M_VMX,
	    M_WAITOK | M_ZERO);
	msr_bitmap_initialize(vmx->msr_bitmap);

	/*
	 * It is safe to allow direct access to MSR_GSBASE and MSR_FSBASE.
	 * The guest FSBASE and GSBASE are saved and restored during
	 * vm-exit and vm-entry respectively. The host FSBASE and GSBASE are
	 * always restored from the vmcs host state area on vm-exit.
	 *
	 * The SYSENTER_CS/ESP/EIP MSRs are identical to FS/GSBASE in
	 * how they are saved/restored so can be directly accessed by the
	 * guest.
	 *
	 * MSR_EFER is saved and restored in the guest VMCS area on a
	 * VM exit and entry respectively. It is also restored from the
	 * host VMCS area on a VM exit.
	 *
	 * The TSC MSR is exposed read-only. Writes are disallowed as
	 * that will impact the host TSC.  If the guest does a write
	 * the "use TSC offsetting" execution control is enabled and the
	 * difference between the host TSC and the guest TSC is written
	 * into the TSC offset in the VMCS.
	 *
	 * Guest TSC_AUX support is enabled if any of guest RDPID and/or
	 * guest RDTSCP support are enabled (since, as per Table 2-2 in SDM
	 * volume 4, TSC_AUX is supported if any of RDPID and/or RDTSCP are
	 * supported). If guest TSC_AUX support is enabled, TSC_AUX is
	 * exposed read-only so that the VMM can do one fewer MSR read per
	 * exit than if this register were exposed read-write; the guest
	 * restore value can be updated during guest writes (expected to be
	 * rare) instead of during all exits (common).
	 */
	if (guest_msr_rw(vmx, MSR_GSBASE) ||
	    guest_msr_rw(vmx, MSR_FSBASE) ||
	    guest_msr_rw(vmx, MSR_SYSENTER_CS_MSR) ||
	    guest_msr_rw(vmx, MSR_SYSENTER_ESP_MSR) ||
	    guest_msr_rw(vmx, MSR_SYSENTER_EIP_MSR) ||
	    guest_msr_rw(vmx, MSR_EFER) ||
	    guest_msr_ro(vmx, MSR_TSC) ||
	    ((cap_rdpid || cap_rdtscp) && guest_msr_ro(vmx, MSR_TSC_AUX)))
		panic("vmx_init: error setting guest msr access");

	if (virtual_interrupt_delivery) {
		error = vm_map_mmio(vm, DEFAULT_APIC_BASE, PAGE_SIZE,
		    APIC_ACCESS_ADDRESS);
		/* XXX this should really return an error to the caller */
		KASSERT(error == 0, ("vm_map_mmio(apicbase) error %d", error));
	}

	vmx->pmap = pmap;
	return (vmx);
}

static void *
vmx_vcpu_init(void *vmi, struct vcpu *vcpu1, int vcpuid)
{
	struct vmx *vmx = vmi;
	struct vmcs *vmcs;
	struct vmx_vcpu *vcpu;
	uint32_t exc_bitmap;
	uint16_t vpid;
	int error;

	vpid = vpid_alloc(vcpuid);

	vcpu = malloc(sizeof(*vcpu), M_VMX, M_WAITOK | M_ZERO);
	vcpu->vmx = vmx;
	vcpu->vcpu = vcpu1;
	vcpu->vcpuid = vcpuid;
	vcpu->vmcs = malloc_aligned(sizeof(*vmcs), PAGE_SIZE, M_VMX,
	    M_WAITOK | M_ZERO);
	vcpu->apic_page = malloc_aligned(PAGE_SIZE, PAGE_SIZE, M_VMX,
	    M_WAITOK | M_ZERO);
	vcpu->pir_desc = malloc_aligned(sizeof(*vcpu->pir_desc), 64, M_VMX,
	    M_WAITOK | M_ZERO);

	vmcs = vcpu->vmcs;
	vmcs->identifier = vmx_revision();
	error = vmclear(vmcs);
	if (error != 0) {
		panic("vmx_init: vmclear error %d on vcpu %d\n",
		    error, vcpuid);
	}

	vmx_msr_guest_init(vmx, vcpu);

	error = vmcs_init(vmcs);
	KASSERT(error == 0, ("vmcs_init error %d", error));

	VMPTRLD(vmcs);
	error = 0;
	error += vmwrite(VMCS_HOST_RSP, (u_long)&vcpu->ctx);
	error += vmwrite(VMCS_EPTP, vmx->eptp);
	error += vmwrite(VMCS_PIN_BASED_CTLS, pinbased_ctls);
	error += vmwrite(VMCS_PRI_PROC_BASED_CTLS, procbased_ctls);
	if (vcpu_trap_wbinvd(vcpu->vcpu)) {
		KASSERT(cap_wbinvd_exit, ("WBINVD trap not available"));
		procbased_ctls2 |= PROCBASED2_WBINVD_EXITING;
	}
	error += vmwrite(VMCS_SEC_PROC_BASED_CTLS, procbased_ctls2);
	error += vmwrite(VMCS_EXIT_CTLS, exit_ctls);
	error += vmwrite(VMCS_ENTRY_CTLS, entry_ctls);
	error += vmwrite(VMCS_MSR_BITMAP, vtophys(vmx->msr_bitmap));
	error += vmwrite(VMCS_VPID, vpid);

	if (guest_l1d_flush && !guest_l1d_flush_sw) {
		vmcs_write(VMCS_ENTRY_MSR_LOAD, pmap_kextract(
			(vm_offset_t)&msr_load_list[0]));
		vmcs_write(VMCS_ENTRY_MSR_LOAD_COUNT,
		    nitems(msr_load_list));
		vmcs_write(VMCS_EXIT_MSR_STORE, 0);
		vmcs_write(VMCS_EXIT_MSR_STORE_COUNT, 0);
	}

	/* exception bitmap */
	if (vcpu_trace_exceptions(vcpu->vcpu))
		exc_bitmap = 0xffffffff;
	else
		exc_bitmap = 1 << IDT_MC;
	error += vmwrite(VMCS_EXCEPTION_BITMAP, exc_bitmap);

	vcpu->ctx.guest_dr6 = DBREG_DR6_RESERVED1;
	error += vmwrite(VMCS_GUEST_DR7, DBREG_DR7_RESERVED1);

	if (tpr_shadowing) {
		error += vmwrite(VMCS_VIRTUAL_APIC, vtophys(vcpu->apic_page));
	}

	if (virtual_interrupt_delivery) {
		error += vmwrite(VMCS_APIC_ACCESS, APIC_ACCESS_ADDRESS);
		error += vmwrite(VMCS_EOI_EXIT0, 0);
		error += vmwrite(VMCS_EOI_EXIT1, 0);
		error += vmwrite(VMCS_EOI_EXIT2, 0);
		error += vmwrite(VMCS_EOI_EXIT3, 0);
	}
	if (posted_interrupts) {
		error += vmwrite(VMCS_PIR_VECTOR, pirvec);
		error += vmwrite(VMCS_PIR_DESC, vtophys(vcpu->pir_desc));
	}
	VMCLEAR(vmcs);
	KASSERT(error == 0, ("vmx_init: error customizing the vmcs"));

	vcpu->cap.set = 0;
	vcpu->cap.set |= cap_rdpid != 0 ? 1 << VM_CAP_RDPID : 0;
	vcpu->cap.set |= cap_rdtscp != 0 ? 1 << VM_CAP_RDTSCP : 0;
	vcpu->cap.proc_ctls = procbased_ctls;
	vcpu->cap.proc_ctls2 = procbased_ctls2;
	vcpu->cap.exc_bitmap = exc_bitmap;

	vcpu->state.nextrip = ~0;
	vcpu->state.lastcpu = NOCPU;
	vcpu->state.vpid = vpid;

	/*
	 * Set up the CR0/4 shadows, and init the read shadow
	 * to the power-on register value from the Intel Sys Arch.
	 *  CR0 - 0x60000010
	 *  CR4 - 0
	 */
	error = vmx_setup_cr0_shadow(vmcs, 0x60000010);
	if (error != 0)
		panic("vmx_setup_cr0_shadow %d", error);

	error = vmx_setup_cr4_shadow(vmcs, 0);
	if (error != 0)
		panic("vmx_setup_cr4_shadow %d", error);

	vcpu->ctx.pmap = vmx->pmap;

	return (vcpu);
}

static int
vmx_handle_cpuid(struct vmx_vcpu *vcpu, struct vmxctx *vmxctx)
{
	int handled;

	handled = x86_emulate_cpuid(vcpu->vcpu, (uint64_t *)&vmxctx->guest_rax,
	    (uint64_t *)&vmxctx->guest_rbx, (uint64_t *)&vmxctx->guest_rcx,
	    (uint64_t *)&vmxctx->guest_rdx);
	return (handled);
}

static __inline void
vmx_run_trace(struct vmx_vcpu *vcpu)
{
	VMX_CTR1(vcpu, "Resume execution at %#lx", vmcs_guest_rip());
}

static __inline void
vmx_exit_trace(struct vmx_vcpu *vcpu, uint64_t rip, uint32_t exit_reason,
    int handled)
{
	VMX_CTR3(vcpu, "%s %s vmexit at 0x%0lx",
		 handled ? "handled" : "unhandled",
		 exit_reason_to_str(exit_reason), rip);
}

static __inline void
vmx_astpending_trace(struct vmx_vcpu *vcpu, uint64_t rip)
{
	VMX_CTR1(vcpu, "astpending vmexit at 0x%0lx", rip);
}

static VMM_STAT_INTEL(VCPU_INVVPID_SAVED, "Number of vpid invalidations saved");
static VMM_STAT_INTEL(VCPU_INVVPID_DONE, "Number of vpid invalidations done");

/*
 * Invalidate guest mappings identified by its vpid from the TLB.
 */
static __inline void
vmx_invvpid(struct vmx *vmx, struct vmx_vcpu *vcpu, pmap_t pmap, int running)
{
	struct vmxstate *vmxstate;
	struct invvpid_desc invvpid_desc;

	vmxstate = &vcpu->state;
	if (vmxstate->vpid == 0)
		return;

	if (!running) {
		/*
		 * Set the 'lastcpu' to an invalid host cpu.
		 *
		 * This will invalidate TLB entries tagged with the vcpu's
		 * vpid the next time it runs via vmx_set_pcpu_defaults().
		 */
		vmxstate->lastcpu = NOCPU;
		return;
	}

	KASSERT(curthread->td_critnest > 0, ("%s: vcpu %d running outside "
	    "critical section", __func__, vcpu->vcpuid));

	/*
	 * Invalidate all mappings tagged with 'vpid'
	 *
	 * We do this because this vcpu was executing on a different host
	 * cpu when it last ran. We do not track whether it invalidated
	 * mappings associated with its 'vpid' during that run. So we must
	 * assume that the mappings associated with 'vpid' on 'curcpu' are
	 * stale and invalidate them.
	 *
	 * Note that we incur this penalty only when the scheduler chooses to
	 * move the thread associated with this vcpu between host cpus.
	 *
	 * Note also that this will invalidate mappings tagged with 'vpid'
	 * for "all" EP4TAs.
	 */
	if (atomic_load_long(&pmap->pm_eptgen) == vmx->eptgen[curcpu]) {
		invvpid_desc._res1 = 0;
		invvpid_desc._res2 = 0;
		invvpid_desc.vpid = vmxstate->vpid;
		invvpid_desc.linear_addr = 0;
		invvpid(INVVPID_TYPE_SINGLE_CONTEXT, invvpid_desc);
		vmm_stat_incr(vcpu->vcpu, VCPU_INVVPID_DONE, 1);
	} else {
		/*
		 * The invvpid can be skipped if an invept is going to
		 * be performed before entering the guest. The invept
		 * will invalidate combined mappings tagged with
		 * 'vmx->eptp' for all vpids.
		 */
		vmm_stat_incr(vcpu->vcpu, VCPU_INVVPID_SAVED, 1);
	}
}

static void
vmx_set_pcpu_defaults(struct vmx *vmx, struct vmx_vcpu *vcpu, pmap_t pmap)
{
	struct vmxstate *vmxstate;

	vmxstate = &vcpu->state;
	if (vmxstate->lastcpu == curcpu)
		return;

	vmxstate->lastcpu = curcpu;

	vmm_stat_incr(vcpu->vcpu, VCPU_MIGRATIONS, 1);

	vmcs_write(VMCS_HOST_TR_BASE, vmm_get_host_trbase());
	vmcs_write(VMCS_HOST_GDTR_BASE, vmm_get_host_gdtrbase());
	vmcs_write(VMCS_HOST_GS_BASE, vmm_get_host_gsbase());
	vmx_invvpid(vmx, vcpu, pmap, 1);
}

/*
 * We depend on 'procbased_ctls' to have the Interrupt Window Exiting bit set.
 */
CTASSERT((PROCBASED_CTLS_ONE_SETTING & PROCBASED_INT_WINDOW_EXITING) != 0);

static void __inline
vmx_set_int_window_exiting(struct vmx_vcpu *vcpu)
{

	if ((vcpu->cap.proc_ctls & PROCBASED_INT_WINDOW_EXITING) == 0) {
		vcpu->cap.proc_ctls |= PROCBASED_INT_WINDOW_EXITING;
		vmcs_write(VMCS_PRI_PROC_BASED_CTLS, vcpu->cap.proc_ctls);
		VMX_CTR0(vcpu, "Enabling interrupt window exiting");
	}
}

static void __inline
vmx_clear_int_window_exiting(struct vmx_vcpu *vcpu)
{

	KASSERT((vcpu->cap.proc_ctls & PROCBASED_INT_WINDOW_EXITING) != 0,
	    ("intr_window_exiting not set: %#x", vcpu->cap.proc_ctls));
	vcpu->cap.proc_ctls &= ~PROCBASED_INT_WINDOW_EXITING;
	vmcs_write(VMCS_PRI_PROC_BASED_CTLS, vcpu->cap.proc_ctls);
	VMX_CTR0(vcpu, "Disabling interrupt window exiting");
}

static void __inline
vmx_set_nmi_window_exiting(struct vmx_vcpu *vcpu)
{

	if ((vcpu->cap.proc_ctls & PROCBASED_NMI_WINDOW_EXITING) == 0) {
		vcpu->cap.proc_ctls |= PROCBASED_NMI_WINDOW_EXITING;
		vmcs_write(VMCS_PRI_PROC_BASED_CTLS, vcpu->cap.proc_ctls);
		VMX_CTR0(vcpu, "Enabling NMI window exiting");
	}
}

static void __inline
vmx_clear_nmi_window_exiting(struct vmx_vcpu *vcpu)
{

	KASSERT((vcpu->cap.proc_ctls & PROCBASED_NMI_WINDOW_EXITING) != 0,
	    ("nmi_window_exiting not set %#x", vcpu->cap.proc_ctls));
	vcpu->cap.proc_ctls &= ~PROCBASED_NMI_WINDOW_EXITING;
	vmcs_write(VMCS_PRI_PROC_BASED_CTLS, vcpu->cap.proc_ctls);
	VMX_CTR0(vcpu, "Disabling NMI window exiting");
}

int
vmx_set_tsc_offset(struct vmx_vcpu *vcpu, uint64_t offset)
{
	int error;

	if ((vcpu->cap.proc_ctls & PROCBASED_TSC_OFFSET) == 0) {
		vcpu->cap.proc_ctls |= PROCBASED_TSC_OFFSET;
		vmcs_write(VMCS_PRI_PROC_BASED_CTLS, vcpu->cap.proc_ctls);
		VMX_CTR0(vcpu, "Enabling TSC offsetting");
	}

	error = vmwrite(VMCS_TSC_OFFSET, offset);
#ifdef BHYVE_SNAPSHOT
	if (error == 0)
		vm_set_tsc_offset(vcpu->vcpu, offset);
#endif
	return (error);
}

#define	NMI_BLOCKING	(VMCS_INTERRUPTIBILITY_NMI_BLOCKING |		\
			 VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING)
#define	HWINTR_BLOCKING	(VMCS_INTERRUPTIBILITY_STI_BLOCKING |		\
			 VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING)

static void
vmx_inject_nmi(struct vmx_vcpu *vcpu)
{
	uint32_t gi __diagused, info;

	gi = vmcs_read(VMCS_GUEST_INTERRUPTIBILITY);
	KASSERT((gi & NMI_BLOCKING) == 0, ("vmx_inject_nmi: invalid guest "
	    "interruptibility-state %#x", gi));

	info = vmcs_read(VMCS_ENTRY_INTR_INFO);
	KASSERT((info & VMCS_INTR_VALID) == 0, ("vmx_inject_nmi: invalid "
	    "VM-entry interruption information %#x", info));

	/*
	 * Inject the virtual NMI. The vector must be the NMI IDT entry
	 * or the VMCS entry check will fail.
	 */
	info = IDT_NMI | VMCS_INTR_T_NMI | VMCS_INTR_VALID;
	vmcs_write(VMCS_ENTRY_INTR_INFO, info);

	VMX_CTR0(vcpu, "Injecting vNMI");

	/* Clear the request */
	vm_nmi_clear(vcpu->vcpu);
}

static void
vmx_inject_interrupts(struct vmx_vcpu *vcpu, struct vlapic *vlapic,
    uint64_t guestrip)
{
	int vector, need_nmi_exiting, extint_pending;
	uint64_t rflags, entryinfo;
	uint32_t gi, info;

	if (vcpu->cap.set & (1 << VM_CAP_MASK_HWINTR)) {
		return;
	}

	if (vcpu->state.nextrip != guestrip) {
		gi = vmcs_read(VMCS_GUEST_INTERRUPTIBILITY);
		if (gi & HWINTR_BLOCKING) {
			VMX_CTR2(vcpu, "Guest interrupt blocking "
			    "cleared due to rip change: %#lx/%#lx",
			    vcpu->state.nextrip, guestrip);
			gi &= ~HWINTR_BLOCKING;
			vmcs_write(VMCS_GUEST_INTERRUPTIBILITY, gi);
		}
	}

	if (vm_entry_intinfo(vcpu->vcpu, &entryinfo)) {
		KASSERT((entryinfo & VMCS_INTR_VALID) != 0, ("%s: entry "
		    "intinfo is not valid: %#lx", __func__, entryinfo));

		info = vmcs_read(VMCS_ENTRY_INTR_INFO);
		KASSERT((info & VMCS_INTR_VALID) == 0, ("%s: cannot inject "
		     "pending exception: %#lx/%#x", __func__, entryinfo, info));

		info = entryinfo;
		vector = info & 0xff;
		if (vector == IDT_BP || vector == IDT_OF) {
			/*
			 * VT-x requires #BP and #OF to be injected as software
			 * exceptions.
			 */
			info &= ~VMCS_INTR_T_MASK;
			info |= VMCS_INTR_T_SWEXCEPTION;
		}

		if (info & VMCS_INTR_DEL_ERRCODE)
			vmcs_write(VMCS_ENTRY_EXCEPTION_ERROR, entryinfo >> 32);

		vmcs_write(VMCS_ENTRY_INTR_INFO, info);
	}

	if (vm_nmi_pending(vcpu->vcpu)) {
		/*
		 * If there are no conditions blocking NMI injection then
		 * inject it directly here otherwise enable "NMI window
		 * exiting" to inject it as soon as we can.
		 *
		 * We also check for STI_BLOCKING because some implementations
		 * don't allow NMI injection in this case. If we are running
		 * on a processor that doesn't have this restriction it will
		 * immediately exit and the NMI will be injected in the
		 * "NMI window exiting" handler.
		 */
		need_nmi_exiting = 1;
		gi = vmcs_read(VMCS_GUEST_INTERRUPTIBILITY);
		if ((gi & (HWINTR_BLOCKING | NMI_BLOCKING)) == 0) {
			info = vmcs_read(VMCS_ENTRY_INTR_INFO);
			if ((info & VMCS_INTR_VALID) == 0) {
				vmx_inject_nmi(vcpu);
				need_nmi_exiting = 0;
			} else {
				VMX_CTR1(vcpu, "Cannot inject NMI "
				    "due to VM-entry intr info %#x", info);
			}
		} else {
			VMX_CTR1(vcpu, "Cannot inject NMI due to "
			    "Guest Interruptibility-state %#x", gi);
		}

		if (need_nmi_exiting)
			vmx_set_nmi_window_exiting(vcpu);
	}

	extint_pending = vm_extint_pending(vcpu->vcpu);

	if (!extint_pending && virtual_interrupt_delivery) {
		vmx_inject_pir(vlapic);
		return;
	}

	/*
	 * If interrupt-window exiting is already in effect then don't bother
	 * checking for pending interrupts. This is just an optimization and
	 * not needed for correctness.
	 */
	if ((vcpu->cap.proc_ctls & PROCBASED_INT_WINDOW_EXITING) != 0) {
		VMX_CTR0(vcpu, "Skip interrupt injection due to "
		    "pending int_window_exiting");
		return;
	}

	if (!extint_pending) {
		/* Ask the local apic for a vector to inject */
		if (!vlapic_pending_intr(vlapic, &vector))
			return;

		/*
		 * From the Intel SDM, Volume 3, Section "Maskable
		 * Hardware Interrupts":
		 * - maskable interrupt vectors [16,255] can be delivered
		 *   through the local APIC.
		*/
		KASSERT(vector >= 16 && vector <= 255,
		    ("invalid vector %d from local APIC", vector));
	} else {
		/* Ask the legacy pic for a vector to inject */
		vatpic_pending_intr(vcpu->vmx->vm, &vector);

		/*
		 * From the Intel SDM, Volume 3, Section "Maskable
		 * Hardware Interrupts":
		 * - maskable interrupt vectors [0,255] can be delivered
		 *   through the INTR pin.
		 */
		KASSERT(vector >= 0 && vector <= 255,
		    ("invalid vector %d from INTR", vector));
	}

	/* Check RFLAGS.IF and the interruptibility state of the guest */
	rflags = vmcs_read(VMCS_GUEST_RFLAGS);
	if ((rflags & PSL_I) == 0) {
		VMX_CTR2(vcpu, "Cannot inject vector %d due to "
		    "rflags %#lx", vector, rflags);
		goto cantinject;
	}

	gi = vmcs_read(VMCS_GUEST_INTERRUPTIBILITY);
	if (gi & HWINTR_BLOCKING) {
		VMX_CTR2(vcpu, "Cannot inject vector %d due to "
		    "Guest Interruptibility-state %#x", vector, gi);
		goto cantinject;
	}

	info = vmcs_read(VMCS_ENTRY_INTR_INFO);
	if (info & VMCS_INTR_VALID) {
		/*
		 * This is expected and could happen for multiple reasons:
		 * - A vectoring VM-entry was aborted due to astpending
		 * - A VM-exit happened during event injection.
		 * - An exception was injected above.
		 * - An NMI was injected above or after "NMI window exiting"
		 */
		VMX_CTR2(vcpu, "Cannot inject vector %d due to "
		    "VM-entry intr info %#x", vector, info);
		goto cantinject;
	}

	/* Inject the interrupt */
	info = VMCS_INTR_T_HWINTR | VMCS_INTR_VALID;
	info |= vector;
	vmcs_write(VMCS_ENTRY_INTR_INFO, info);

	if (!extint_pending) {
		/* Update the Local APIC ISR */
		vlapic_intr_accepted(vlapic, vector);
	} else {
		vm_extint_clear(vcpu->vcpu);
		vatpic_intr_accepted(vcpu->vmx->vm, vector);

		/*
		 * After we accepted the current ExtINT the PIC may
		 * have posted another one.  If that is the case, set
		 * the Interrupt Window Exiting execution control so
		 * we can inject that one too.
		 *
		 * Also, interrupt window exiting allows us to inject any
		 * pending APIC vector that was preempted by the ExtINT
		 * as soon as possible. This applies both for the software
		 * emulated vlapic and the hardware assisted virtual APIC.
		 */
		vmx_set_int_window_exiting(vcpu);
	}

	VMX_CTR1(vcpu, "Injecting hwintr at vector %d", vector);

	return;

cantinject:
	/*
	 * Set the Interrupt Window Exiting execution control so we can inject
	 * the interrupt as soon as blocking condition goes away.
	 */
	vmx_set_int_window_exiting(vcpu);
}

/*
 * If the Virtual NMIs execution control is '1' then the logical processor
 * tracks virtual-NMI blocking in the Guest Interruptibility-state field of
 * the VMCS. An IRET instruction in VMX non-root operation will remove any
 * virtual-NMI blocking.
 *
 * This unblocking occurs even if the IRET causes a fault. In this case the
 * hypervisor needs to restore virtual-NMI blocking before resuming the guest.
 */
static void
vmx_restore_nmi_blocking(struct vmx_vcpu *vcpu)
{
	uint32_t gi;

	VMX_CTR0(vcpu, "Restore Virtual-NMI blocking");
	gi = vmcs_read(VMCS_GUEST_INTERRUPTIBILITY);
	gi |= VMCS_INTERRUPTIBILITY_NMI_BLOCKING;
	vmcs_write(VMCS_GUEST_INTERRUPTIBILITY, gi);
}

static void
vmx_clear_nmi_blocking(struct vmx_vcpu *vcpu)
{
	uint32_t gi;

	VMX_CTR0(vcpu, "Clear Virtual-NMI blocking");
	gi = vmcs_read(VMCS_GUEST_INTERRUPTIBILITY);
	gi &= ~VMCS_INTERRUPTIBILITY_NMI_BLOCKING;
	vmcs_write(VMCS_GUEST_INTERRUPTIBILITY, gi);
}

static void
vmx_assert_nmi_blocking(struct vmx_vcpu *vcpu)
{
	uint32_t gi __diagused;

	gi = vmcs_read(VMCS_GUEST_INTERRUPTIBILITY);
	KASSERT(gi & VMCS_INTERRUPTIBILITY_NMI_BLOCKING,
	    ("NMI blocking is not in effect %#x", gi));
}

static int
vmx_emulate_xsetbv(struct vmx *vmx, struct vmx_vcpu *vcpu,
    struct vm_exit *vmexit)
{
	struct vmxctx *vmxctx;
	uint64_t xcrval;
	const struct xsave_limits *limits;

	vmxctx = &vcpu->ctx;
	limits = vmm_get_xsave_limits();

	/*
	 * Note that the processor raises a GP# fault on its own if
	 * xsetbv is executed for CPL != 0, so we do not have to
	 * emulate that fault here.
	 */

	/* Only xcr0 is supported. */
	if (vmxctx->guest_rcx != 0) {
		vm_inject_gp(vcpu->vcpu);
		return (HANDLED);
	}

	/* We only handle xcr0 if both the host and guest have XSAVE enabled. */
	if (!limits->xsave_enabled || !(vmcs_read(VMCS_GUEST_CR4) & CR4_XSAVE)) {
		vm_inject_ud(vcpu->vcpu);
		return (HANDLED);
	}

	xcrval = vmxctx->guest_rdx << 32 | (vmxctx->guest_rax & 0xffffffff);
	if ((xcrval & ~limits->xcr0_allowed) != 0) {
		vm_inject_gp(vcpu->vcpu);
		return (HANDLED);
	}

	if (!(xcrval & XFEATURE_ENABLED_X87)) {
		vm_inject_gp(vcpu->vcpu);
		return (HANDLED);
	}

	/* AVX (YMM_Hi128) requires SSE. */
	if (xcrval & XFEATURE_ENABLED_AVX &&
	    (xcrval & XFEATURE_AVX) != XFEATURE_AVX) {
		vm_inject_gp(vcpu->vcpu);
		return (HANDLED);
	}

	/*
	 * AVX512 requires base AVX (YMM_Hi128) as well as OpMask,
	 * ZMM_Hi256, and Hi16_ZMM.
	 */
	if (xcrval & XFEATURE_AVX512 &&
	    (xcrval & (XFEATURE_AVX512 | XFEATURE_AVX)) !=
	    (XFEATURE_AVX512 | XFEATURE_AVX)) {
		vm_inject_gp(vcpu->vcpu);
		return (HANDLED);
	}

	/*
	 * Intel MPX requires both bound register state flags to be
	 * set.
	 */
	if (((xcrval & XFEATURE_ENABLED_BNDREGS) != 0) !=
	    ((xcrval & XFEATURE_ENABLED_BNDCSR) != 0)) {
		vm_inject_gp(vcpu->vcpu);
		return (HANDLED);
	}

	/*
	 * This runs "inside" vmrun() with the guest's FPU state, so
	 * modifying xcr0 directly modifies the guest's xcr0, not the
	 * host's.
	 */
	load_xcr(0, xcrval);
	return (HANDLED);
}

static uint64_t
vmx_get_guest_reg(struct vmx_vcpu *vcpu, int ident)
{
	const struct vmxctx *vmxctx;

	vmxctx = &vcpu->ctx;

	switch (ident) {
	case 0:
		return (vmxctx->guest_rax);
	case 1:
		return (vmxctx->guest_rcx);
	case 2:
		return (vmxctx->guest_rdx);
	case 3:
		return (vmxctx->guest_rbx);
	case 4:
		return (vmcs_read(VMCS_GUEST_RSP));
	case 5:
		return (vmxctx->guest_rbp);
	case 6:
		return (vmxctx->guest_rsi);
	case 7:
		return (vmxctx->guest_rdi);
	case 8:
		return (vmxctx->guest_r8);
	case 9:
		return (vmxctx->guest_r9);
	case 10:
		return (vmxctx->guest_r10);
	case 11:
		return (vmxctx->guest_r11);
	case 12:
		return (vmxctx->guest_r12);
	case 13:
		return (vmxctx->guest_r13);
	case 14:
		return (vmxctx->guest_r14);
	case 15:
		return (vmxctx->guest_r15);
	default:
		panic("invalid vmx register %d", ident);
	}
}

static void
vmx_set_guest_reg(struct vmx_vcpu *vcpu, int ident, uint64_t regval)
{
	struct vmxctx *vmxctx;

	vmxctx = &vcpu->ctx;

	switch (ident) {
	case 0:
		vmxctx->guest_rax = regval;
		break;
	case 1:
		vmxctx->guest_rcx = regval;
		break;
	case 2:
		vmxctx->guest_rdx = regval;
		break;
	case 3:
		vmxctx->guest_rbx = regval;
		break;
	case 4:
		vmcs_write(VMCS_GUEST_RSP, regval);
		break;
	case 5:
		vmxctx->guest_rbp = regval;
		break;
	case 6:
		vmxctx->guest_rsi = regval;
		break;
	case 7:
		vmxctx->guest_rdi = regval;
		break;
	case 8:
		vmxctx->guest_r8 = regval;
		break;
	case 9:
		vmxctx->guest_r9 = regval;
		break;
	case 10:
		vmxctx->guest_r10 = regval;
		break;
	case 11:
		vmxctx->guest_r11 = regval;
		break;
	case 12:
		vmxctx->guest_r12 = regval;
		break;
	case 13:
		vmxctx->guest_r13 = regval;
		break;
	case 14:
		vmxctx->guest_r14 = regval;
		break;
	case 15:
		vmxctx->guest_r15 = regval;
		break;
	default:
		panic("invalid vmx register %d", ident);
	}
}

static int
vmx_emulate_cr0_access(struct vmx_vcpu *vcpu, uint64_t exitqual)
{
	uint64_t crval, regval;

	/* We only handle mov to %cr0 at this time */
	if ((exitqual & 0xf0) != 0x00)
		return (UNHANDLED);

	regval = vmx_get_guest_reg(vcpu, (exitqual >> 8) & 0xf);

	vmcs_write(VMCS_CR0_SHADOW, regval);

	crval = regval | cr0_ones_mask;
	crval &= ~cr0_zeros_mask;
	vmcs_write(VMCS_GUEST_CR0, crval);

	if (regval & CR0_PG) {
		uint64_t efer, entry_ctls;

		/*
		 * If CR0.PG is 1 and EFER.LME is 1 then EFER.LMA and
		 * the "IA-32e mode guest" bit in VM-entry control must be
		 * equal.
		 */
		efer = vmcs_read(VMCS_GUEST_IA32_EFER);
		if (efer & EFER_LME) {
			efer |= EFER_LMA;
			vmcs_write(VMCS_GUEST_IA32_EFER, efer);
			entry_ctls = vmcs_read(VMCS_ENTRY_CTLS);
			entry_ctls |= VM_ENTRY_GUEST_LMA;
			vmcs_write(VMCS_ENTRY_CTLS, entry_ctls);
		}
	}

	return (HANDLED);
}

static int
vmx_emulate_cr4_access(struct vmx_vcpu *vcpu, uint64_t exitqual)
{
	uint64_t crval, regval;

	/* We only handle mov to %cr4 at this time */
	if ((exitqual & 0xf0) != 0x00)
		return (UNHANDLED);

	regval = vmx_get_guest_reg(vcpu, (exitqual >> 8) & 0xf);

	vmcs_write(VMCS_CR4_SHADOW, regval);

	crval = regval | cr4_ones_mask;
	crval &= ~cr4_zeros_mask;
	vmcs_write(VMCS_GUEST_CR4, crval);

	return (HANDLED);
}

static int
vmx_emulate_cr8_access(struct vmx *vmx, struct vmx_vcpu *vcpu,
    uint64_t exitqual)
{
	struct vlapic *vlapic;
	uint64_t cr8;
	int regnum;

	/* We only handle mov %cr8 to/from a register at this time. */
	if ((exitqual & 0xe0) != 0x00) {
		return (UNHANDLED);
	}

	vlapic = vm_lapic(vcpu->vcpu);
	regnum = (exitqual >> 8) & 0xf;
	if (exitqual & 0x10) {
		cr8 = vlapic_get_cr8(vlapic);
		vmx_set_guest_reg(vcpu, regnum, cr8);
	} else {
		cr8 = vmx_get_guest_reg(vcpu, regnum);
		vlapic_set_cr8(vlapic, cr8);
	}

	return (HANDLED);
}

/*
 * From section "Guest Register State" in the Intel SDM: CPL = SS.DPL
 */
static int
vmx_cpl(void)
{
	uint32_t ssar;

	ssar = vmcs_read(VMCS_GUEST_SS_ACCESS_RIGHTS);
	return ((ssar >> 5) & 0x3);
}

static enum vm_cpu_mode
vmx_cpu_mode(void)
{
	uint32_t csar;

	if (vmcs_read(VMCS_GUEST_IA32_EFER) & EFER_LMA) {
		csar = vmcs_read(VMCS_GUEST_CS_ACCESS_RIGHTS);
		if (csar & 0x2000)
			return (CPU_MODE_64BIT);	/* CS.L = 1 */
		else
			return (CPU_MODE_COMPATIBILITY);
	} else if (vmcs_read(VMCS_GUEST_CR0) & CR0_PE) {
		return (CPU_MODE_PROTECTED);
	} else {
		return (CPU_MODE_REAL);
	}
}

static enum vm_paging_mode
vmx_paging_mode(void)
{
	uint64_t cr4;

	if (!(vmcs_read(VMCS_GUEST_CR0) & CR0_PG))
		return (PAGING_MODE_FLAT);
	cr4 = vmcs_read(VMCS_GUEST_CR4);
	if (!(cr4 & CR4_PAE))
		return (PAGING_MODE_32);
	if (vmcs_read(VMCS_GUEST_IA32_EFER) & EFER_LME) {
		if (!(cr4 & CR4_LA57))
			return (PAGING_MODE_64);
		return (PAGING_MODE_64_LA57);
	} else
		return (PAGING_MODE_PAE);
}

static uint64_t
inout_str_index(struct vmx_vcpu *vcpu, int in)
{
	uint64_t val;
	int error __diagused;
	enum vm_reg_name reg;

	reg = in ? VM_REG_GUEST_RDI : VM_REG_GUEST_RSI;
	error = vmx_getreg(vcpu, reg, &val);
	KASSERT(error == 0, ("%s: vmx_getreg error %d", __func__, error));
	return (val);
}

static uint64_t
inout_str_count(struct vmx_vcpu *vcpu, int rep)
{
	uint64_t val;
	int error __diagused;

	if (rep) {
		error = vmx_getreg(vcpu, VM_REG_GUEST_RCX, &val);
		KASSERT(!error, ("%s: vmx_getreg error %d", __func__, error));
	} else {
		val = 1;
	}
	return (val);
}

static int
inout_str_addrsize(uint32_t inst_info)
{
	uint32_t size;

	size = (inst_info >> 7) & 0x7;
	switch (size) {
	case 0:
		return (2);	/* 16 bit */
	case 1:
		return (4);	/* 32 bit */
	case 2:
		return (8);	/* 64 bit */
	default:
		panic("%s: invalid size encoding %d", __func__, size);
	}
}

static void
inout_str_seginfo(struct vmx_vcpu *vcpu, uint32_t inst_info, int in,
    struct vm_inout_str *vis)
{
	int error __diagused, s;

	if (in) {
		vis->seg_name = VM_REG_GUEST_ES;
	} else {
		s = (inst_info >> 15) & 0x7;
		vis->seg_name = vm_segment_name(s);
	}

	error = vmx_getdesc(vcpu, vis->seg_name, &vis->seg_desc);
	KASSERT(error == 0, ("%s: vmx_getdesc error %d", __func__, error));
}

static void
vmx_paging_info(struct vm_guest_paging *paging)
{
	paging->cr3 = vmcs_guest_cr3();
	paging->cpl = vmx_cpl();
	paging->cpu_mode = vmx_cpu_mode();
	paging->paging_mode = vmx_paging_mode();
}

static void
vmexit_inst_emul(struct vm_exit *vmexit, uint64_t gpa, uint64_t gla)
{
	struct vm_guest_paging *paging;
	uint32_t csar;

	paging = &vmexit->u.inst_emul.paging;

	vmexit->exitcode = VM_EXITCODE_INST_EMUL;
	vmexit->inst_length = 0;
	vmexit->u.inst_emul.gpa = gpa;
	vmexit->u.inst_emul.gla = gla;
	vmx_paging_info(paging);
	switch (paging->cpu_mode) {
	case CPU_MODE_REAL:
		vmexit->u.inst_emul.cs_base = vmcs_read(VMCS_GUEST_CS_BASE);
		vmexit->u.inst_emul.cs_d = 0;
		break;
	case CPU_MODE_PROTECTED:
	case CPU_MODE_COMPATIBILITY:
		vmexit->u.inst_emul.cs_base = vmcs_read(VMCS_GUEST_CS_BASE);
		csar = vmcs_read(VMCS_GUEST_CS_ACCESS_RIGHTS);
		vmexit->u.inst_emul.cs_d = SEG_DESC_DEF32(csar);
		break;
	default:
		vmexit->u.inst_emul.cs_base = 0;
		vmexit->u.inst_emul.cs_d = 0;
		break;
	}
	vie_init(&vmexit->u.inst_emul.vie, NULL, 0);
}

static int
ept_fault_type(uint64_t ept_qual)
{
	int fault_type;

	if (ept_qual & EPT_VIOLATION_DATA_WRITE)
		fault_type = VM_PROT_WRITE;
	else if (ept_qual & EPT_VIOLATION_INST_FETCH)
		fault_type = VM_PROT_EXECUTE;
	else
		fault_type= VM_PROT_READ;

	return (fault_type);
}

static bool
ept_emulation_fault(uint64_t ept_qual)
{
	int read, write;

	/* EPT fault on an instruction fetch doesn't make sense here */
	if (ept_qual & EPT_VIOLATION_INST_FETCH)
		return (false);

	/* EPT fault must be a read fault or a write fault */
	read = ept_qual & EPT_VIOLATION_DATA_READ ? 1 : 0;
	write = ept_qual & EPT_VIOLATION_DATA_WRITE ? 1 : 0;
	if ((read | write) == 0)
		return (false);

	/*
	 * The EPT violation must have been caused by accessing a
	 * guest-physical address that is a translation of a guest-linear
	 * address.
	 */
	if ((ept_qual & EPT_VIOLATION_GLA_VALID) == 0 ||
	    (ept_qual & EPT_VIOLATION_XLAT_VALID) == 0) {
		return (false);
	}

	return (true);
}

static __inline int
apic_access_virtualization(struct vmx_vcpu *vcpu)
{
	uint32_t proc_ctls2;

	proc_ctls2 = vcpu->cap.proc_ctls2;
	return ((proc_ctls2 & PROCBASED2_VIRTUALIZE_APIC_ACCESSES) ? 1 : 0);
}

static __inline int
x2apic_virtualization(struct vmx_vcpu *vcpu)
{
	uint32_t proc_ctls2;

	proc_ctls2 = vcpu->cap.proc_ctls2;
	return ((proc_ctls2 & PROCBASED2_VIRTUALIZE_X2APIC_MODE) ? 1 : 0);
}

static int
vmx_handle_apic_write(struct vmx_vcpu *vcpu, struct vlapic *vlapic,
    uint64_t qual)
{
	int error, handled, offset;
	uint32_t *apic_regs, vector;
	bool retu;

	handled = HANDLED;
	offset = APIC_WRITE_OFFSET(qual);

	if (!apic_access_virtualization(vcpu)) {
		/*
		 * In general there should not be any APIC write VM-exits
		 * unless APIC-access virtualization is enabled.
		 *
		 * However self-IPI virtualization can legitimately trigger
		 * an APIC-write VM-exit so treat it specially.
		 */
		if (x2apic_virtualization(vcpu) &&
		    offset == APIC_OFFSET_SELF_IPI) {
			apic_regs = (uint32_t *)(vlapic->apic_page);
			vector = apic_regs[APIC_OFFSET_SELF_IPI / 4];
			vlapic_self_ipi_handler(vlapic, vector);
			return (HANDLED);
		} else
			return (UNHANDLED);
	}

	switch (offset) {
	case APIC_OFFSET_ID:
		vlapic_id_write_handler(vlapic);
		break;
	case APIC_OFFSET_LDR:
		vlapic_ldr_write_handler(vlapic);
		break;
	case APIC_OFFSET_DFR:
		vlapic_dfr_write_handler(vlapic);
		break;
	case APIC_OFFSET_SVR:
		vlapic_svr_write_handler(vlapic);
		break;
	case APIC_OFFSET_ESR:
		vlapic_esr_write_handler(vlapic);
		break;
	case APIC_OFFSET_ICR_LOW:
		retu = false;
		error = vlapic_icrlo_write_handler(vlapic, &retu);
		if (error != 0 || retu)
			handled = UNHANDLED;
		break;
	case APIC_OFFSET_CMCI_LVT:
	case APIC_OFFSET_TIMER_LVT ... APIC_OFFSET_ERROR_LVT:
		vlapic_lvt_write_handler(vlapic, offset);
		break;
	case APIC_OFFSET_TIMER_ICR:
		vlapic_icrtmr_write_handler(vlapic);
		break;
	case APIC_OFFSET_TIMER_DCR:
		vlapic_dcr_write_handler(vlapic);
		break;
	default:
		handled = UNHANDLED;
		break;
	}
	return (handled);
}

static bool
apic_access_fault(struct vmx_vcpu *vcpu, uint64_t gpa)
{

	if (apic_access_virtualization(vcpu) &&
	    (gpa >= DEFAULT_APIC_BASE && gpa < DEFAULT_APIC_BASE + PAGE_SIZE))
		return (true);
	else
		return (false);
}

static int
vmx_handle_apic_access(struct vmx_vcpu *vcpu, struct vm_exit *vmexit)
{
	uint64_t qual;
	int access_type, offset, allowed;

	if (!apic_access_virtualization(vcpu))
		return (UNHANDLED);

	qual = vmexit->u.vmx.exit_qualification;
	access_type = APIC_ACCESS_TYPE(qual);
	offset = APIC_ACCESS_OFFSET(qual);

	allowed = 0;
	if (access_type == 0) {
		/*
		 * Read data access to the following registers is expected.
		 */
		switch (offset) {
		case APIC_OFFSET_APR:
		case APIC_OFFSET_PPR:
		case APIC_OFFSET_RRR:
		case APIC_OFFSET_CMCI_LVT:
		case APIC_OFFSET_TIMER_CCR:
			allowed = 1;
			break;
		default:
			break;
		}
	} else if (access_type == 1) {
		/*
		 * Write data access to the following registers is expected.
		 */
		switch (offset) {
		case APIC_OFFSET_VER:
		case APIC_OFFSET_APR:
		case APIC_OFFSET_PPR:
		case APIC_OFFSET_RRR:
		case APIC_OFFSET_ISR0 ... APIC_OFFSET_ISR7:
		case APIC_OFFSET_TMR0 ... APIC_OFFSET_TMR7:
		case APIC_OFFSET_IRR0 ... APIC_OFFSET_IRR7:
		case APIC_OFFSET_CMCI_LVT:
		case APIC_OFFSET_TIMER_CCR:
			allowed = 1;
			break;
		default:
			break;
		}
	}

	if (allowed) {
		vmexit_inst_emul(vmexit, DEFAULT_APIC_BASE + offset,
		    VIE_INVALID_GLA);
	}

	/*
	 * Regardless of whether the APIC-access is allowed this handler
	 * always returns UNHANDLED:
	 * - if the access is allowed then it is handled by emulating the
	 *   instruction that caused the VM-exit (outside the critical section)
	 * - if the access is not allowed then it will be converted to an
	 *   exitcode of VM_EXITCODE_VMX and will be dealt with in userland.
	 */
	return (UNHANDLED);
}

static enum task_switch_reason
vmx_task_switch_reason(uint64_t qual)
{
	int reason;

	reason = (qual >> 30) & 0x3;
	switch (reason) {
	case 0:
		return (TSR_CALL);
	case 1:
		return (TSR_IRET);
	case 2:
		return (TSR_JMP);
	case 3:
		return (TSR_IDT_GATE);
	default:
		panic("%s: invalid reason %d", __func__, reason);
	}
}

static int
emulate_wrmsr(struct vmx_vcpu *vcpu, u_int num, uint64_t val, bool *retu)
{
	int error;

	if (lapic_msr(num))
		error = lapic_wrmsr(vcpu->vcpu, num, val, retu);
	else
		error = vmx_wrmsr(vcpu, num, val, retu);

	return (error);
}

static int
emulate_rdmsr(struct vmx_vcpu *vcpu, u_int num, bool *retu)
{
	struct vmxctx *vmxctx;
	uint64_t result;
	uint32_t eax, edx;
	int error;

	if (lapic_msr(num))
		error = lapic_rdmsr(vcpu->vcpu, num, &result, retu);
	else
		error = vmx_rdmsr(vcpu, num, &result, retu);

	if (error == 0) {
		eax = result;
		vmxctx = &vcpu->ctx;
		error = vmxctx_setreg(vmxctx, VM_REG_GUEST_RAX, eax);
		KASSERT(error == 0, ("vmxctx_setreg(rax) error %d", error));

		edx = result >> 32;
		error = vmxctx_setreg(vmxctx, VM_REG_GUEST_RDX, edx);
		KASSERT(error == 0, ("vmxctx_setreg(rdx) error %d", error));
	}

	return (error);
}

static int
vmx_exit_process(struct vmx *vmx, struct vmx_vcpu *vcpu, struct vm_exit *vmexit)
{
	int error, errcode, errcode_valid, handled, in;
	struct vmxctx *vmxctx;
	struct vlapic *vlapic;
	struct vm_inout_str *vis;
	struct vm_task_switch *ts;
	uint32_t eax, ecx, edx, idtvec_info, idtvec_err, intr_info, inst_info;
	uint32_t intr_type, intr_vec, reason;
	uint64_t exitintinfo, qual, gpa;
#ifdef KDTRACE_HOOKS
	int vcpuid;
#endif
	bool retu;

	CTASSERT((PINBASED_CTLS_ONE_SETTING & PINBASED_VIRTUAL_NMI) != 0);
	CTASSERT((PINBASED_CTLS_ONE_SETTING & PINBASED_NMI_EXITING) != 0);

	handled = UNHANDLED;
	vmxctx = &vcpu->ctx;
#ifdef KDTRACE_HOOKS
	vcpuid = vcpu->vcpuid;
#endif

	qual = vmexit->u.vmx.exit_qualification;
	reason = vmexit->u.vmx.exit_reason;
	vmexit->exitcode = VM_EXITCODE_BOGUS;

	vmm_stat_incr(vcpu->vcpu, VMEXIT_COUNT, 1);
	SDT_PROBE3(vmm, vmx, exit, entry, vmx, vcpuid, vmexit);

	/*
	 * VM-entry failures during or after loading guest state.
	 *
	 * These VM-exits are uncommon but must be handled specially
	 * as most VM-exit fields are not populated as usual.
	 */
	if (__predict_false(reason == EXIT_REASON_MCE_DURING_ENTRY)) {
		VMX_CTR0(vcpu, "Handling MCE during VM-entry");
		__asm __volatile("int $18");
		return (1);
	}

	/*
	 * VM exits that can be triggered during event delivery need to
	 * be handled specially by re-injecting the event if the IDT
	 * vectoring information field's valid bit is set.
	 *
	 * See "Information for VM Exits During Event Delivery" in Intel SDM
	 * for details.
	 */
	idtvec_info = vmcs_idt_vectoring_info();
	if (idtvec_info & VMCS_IDT_VEC_VALID) {
		idtvec_info &= ~(1 << 12); /* clear undefined bit */
		exitintinfo = idtvec_info;
		if (idtvec_info & VMCS_IDT_VEC_ERRCODE_VALID) {
			idtvec_err = vmcs_idt_vectoring_err();
			exitintinfo |= (uint64_t)idtvec_err << 32;
		}
		error = vm_exit_intinfo(vcpu->vcpu, exitintinfo);
		KASSERT(error == 0, ("%s: vm_set_intinfo error %d",
		    __func__, error));

		/*
		 * If 'virtual NMIs' are being used and the VM-exit
		 * happened while injecting an NMI during the previous
		 * VM-entry, then clear "blocking by NMI" in the
		 * Guest Interruptibility-State so the NMI can be
		 * reinjected on the subsequent VM-entry.
		 *
		 * However, if the NMI was being delivered through a task
		 * gate, then the new task must start execution with NMIs
		 * blocked so don't clear NMI blocking in this case.
		 */
		intr_type = idtvec_info & VMCS_INTR_T_MASK;
		if (intr_type == VMCS_INTR_T_NMI) {
			if (reason != EXIT_REASON_TASK_SWITCH)
				vmx_clear_nmi_blocking(vcpu);
			else
				vmx_assert_nmi_blocking(vcpu);
		}

		/*
		 * Update VM-entry instruction length if the event being
		 * delivered was a software interrupt or software exception.
		 */
		if (intr_type == VMCS_INTR_T_SWINTR ||
		    intr_type == VMCS_INTR_T_PRIV_SWEXCEPTION ||
		    intr_type == VMCS_INTR_T_SWEXCEPTION) {
			vmcs_write(VMCS_ENTRY_INST_LENGTH, vmexit->inst_length);
		}
	}

	switch (reason) {
	case EXIT_REASON_TASK_SWITCH:
		ts = &vmexit->u.task_switch;
		ts->tsssel = qual & 0xffff;
		ts->reason = vmx_task_switch_reason(qual);
		ts->ext = 0;
		ts->errcode_valid = 0;
		vmx_paging_info(&ts->paging);
		/*
		 * If the task switch was due to a CALL, JMP, IRET, software
		 * interrupt (INT n) or software exception (INT3, INTO),
		 * then the saved %rip references the instruction that caused
		 * the task switch. The instruction length field in the VMCS
		 * is valid in this case.
		 *
		 * In all other cases (e.g., NMI, hardware exception) the
		 * saved %rip is one that would have been saved in the old TSS
		 * had the task switch completed normally so the instruction
		 * length field is not needed in this case and is explicitly
		 * set to 0.
		 */
		if (ts->reason == TSR_IDT_GATE) {
			KASSERT(idtvec_info & VMCS_IDT_VEC_VALID,
			    ("invalid idtvec_info %#x for IDT task switch",
			    idtvec_info));
			intr_type = idtvec_info & VMCS_INTR_T_MASK;
			if (intr_type != VMCS_INTR_T_SWINTR &&
			    intr_type != VMCS_INTR_T_SWEXCEPTION &&
			    intr_type != VMCS_INTR_T_PRIV_SWEXCEPTION) {
				/* Task switch triggered by external event */
				ts->ext = 1;
				vmexit->inst_length = 0;
				if (idtvec_info & VMCS_IDT_VEC_ERRCODE_VALID) {
					ts->errcode_valid = 1;
					ts->errcode = vmcs_idt_vectoring_err();
				}
			}
		}
		vmexit->exitcode = VM_EXITCODE_TASK_SWITCH;
		SDT_PROBE4(vmm, vmx, exit, taskswitch, vmx, vcpuid, vmexit, ts);
		VMX_CTR4(vcpu, "task switch reason %d, tss 0x%04x, "
		    "%s errcode 0x%016lx", ts->reason, ts->tsssel,
		    ts->ext ? "external" : "internal",
		    ((uint64_t)ts->errcode << 32) | ts->errcode_valid);
		break;
	case EXIT_REASON_CR_ACCESS:
		vmm_stat_incr(vcpu->vcpu, VMEXIT_CR_ACCESS, 1);
		SDT_PROBE4(vmm, vmx, exit, craccess, vmx, vcpuid, vmexit, qual);
		switch (qual & 0xf) {
		case 0:
			handled = vmx_emulate_cr0_access(vcpu, qual);
			break;
		case 4:
			handled = vmx_emulate_cr4_access(vcpu, qual);
			break;
		case 8:
			handled = vmx_emulate_cr8_access(vmx, vcpu, qual);
			break;
		}
		break;
	case EXIT_REASON_RDMSR:
		vmm_stat_incr(vcpu->vcpu, VMEXIT_RDMSR, 1);
		retu = false;
		ecx = vmxctx->guest_rcx;
		VMX_CTR1(vcpu, "rdmsr 0x%08x", ecx);
		SDT_PROBE4(vmm, vmx, exit, rdmsr, vmx, vcpuid, vmexit, ecx);
		error = emulate_rdmsr(vcpu, ecx, &retu);
		if (error) {
			vmexit->exitcode = VM_EXITCODE_RDMSR;
			vmexit->u.msr.code = ecx;
		} else if (!retu) {
			handled = HANDLED;
		} else {
			/* Return to userspace with a valid exitcode */
			KASSERT(vmexit->exitcode != VM_EXITCODE_BOGUS,
			    ("emulate_rdmsr retu with bogus exitcode"));
		}
		break;
	case EXIT_REASON_WRMSR:
		vmm_stat_incr(vcpu->vcpu, VMEXIT_WRMSR, 1);
		retu = false;
		eax = vmxctx->guest_rax;
		ecx = vmxctx->guest_rcx;
		edx = vmxctx->guest_rdx;
		VMX_CTR2(vcpu, "wrmsr 0x%08x value 0x%016lx",
		    ecx, (uint64_t)edx << 32 | eax);
		SDT_PROBE5(vmm, vmx, exit, wrmsr, vmx, vmexit, vcpuid, ecx,
		    (uint64_t)edx << 32 | eax);
		error = emulate_wrmsr(vcpu, ecx, (uint64_t)edx << 32 | eax,
		    &retu);
		if (error) {
			vmexit->exitcode = VM_EXITCODE_WRMSR;
			vmexit->u.msr.code = ecx;
			vmexit->u.msr.wval = (uint64_t)edx << 32 | eax;
		} else if (!retu) {
			handled = HANDLED;
		} else {
			/* Return to userspace with a valid exitcode */
			KASSERT(vmexit->exitcode != VM_EXITCODE_BOGUS,
			    ("emulate_wrmsr retu with bogus exitcode"));
		}
		break;
	case EXIT_REASON_HLT:
		vmm_stat_incr(vcpu->vcpu, VMEXIT_HLT, 1);
		SDT_PROBE3(vmm, vmx, exit, halt, vmx, vcpuid, vmexit);
		vmexit->exitcode = VM_EXITCODE_HLT;
		vmexit->u.hlt.rflags = vmcs_read(VMCS_GUEST_RFLAGS);
		if (virtual_interrupt_delivery)
			vmexit->u.hlt.intr_status =
			    vmcs_read(VMCS_GUEST_INTR_STATUS);
		else
			vmexit->u.hlt.intr_status = 0;
		break;
	case EXIT_REASON_MTF:
		vmm_stat_incr(vcpu->vcpu, VMEXIT_MTRAP, 1);
		SDT_PROBE3(vmm, vmx, exit, mtrap, vmx, vcpuid, vmexit);
		vmexit->exitcode = VM_EXITCODE_MTRAP;
		vmexit->inst_length = 0;
		break;
	case EXIT_REASON_PAUSE:
		vmm_stat_incr(vcpu->vcpu, VMEXIT_PAUSE, 1);
		SDT_PROBE3(vmm, vmx, exit, pause, vmx, vcpuid, vmexit);
		vmexit->exitcode = VM_EXITCODE_PAUSE;
		break;
	case EXIT_REASON_INTR_WINDOW:
		vmm_stat_incr(vcpu->vcpu, VMEXIT_INTR_WINDOW, 1);
		SDT_PROBE3(vmm, vmx, exit, intrwindow, vmx, vcpuid, vmexit);
		vmx_clear_int_window_exiting(vcpu);
		return (1);
	case EXIT_REASON_EXT_INTR:
		/*
		 * External interrupts serve only to cause VM exits and allow
		 * the host interrupt handler to run.
		 *
		 * If this external interrupt triggers a virtual interrupt
		 * to a VM, then that state will be recorded by the
		 * host interrupt handler in the VM's softc. We will inject
		 * this virtual interrupt during the subsequent VM enter.
		 */
		intr_info = vmcs_read(VMCS_EXIT_INTR_INFO);
		SDT_PROBE4(vmm, vmx, exit, interrupt,
		    vmx, vcpuid, vmexit, intr_info);

		/*
		 * XXX: Ignore this exit if VMCS_INTR_VALID is not set.
		 * This appears to be a bug in VMware Fusion?
		 */
		if (!(intr_info & VMCS_INTR_VALID))
			return (1);
		KASSERT((intr_info & VMCS_INTR_VALID) != 0 &&
		    (intr_info & VMCS_INTR_T_MASK) == VMCS_INTR_T_HWINTR,
		    ("VM exit interruption info invalid: %#x", intr_info));
		vmx_trigger_hostintr(intr_info & 0xff);

		/*
		 * This is special. We want to treat this as an 'handled'
		 * VM-exit but not increment the instruction pointer.
		 */
		vmm_stat_incr(vcpu->vcpu, VMEXIT_EXTINT, 1);
		return (1);
	case EXIT_REASON_NMI_WINDOW:
		SDT_PROBE3(vmm, vmx, exit, nmiwindow, vmx, vcpuid, vmexit);
		/* Exit to allow the pending virtual NMI to be injected */
		if (vm_nmi_pending(vcpu->vcpu))
			vmx_inject_nmi(vcpu);
		vmx_clear_nmi_window_exiting(vcpu);
		vmm_stat_incr(vcpu->vcpu, VMEXIT_NMI_WINDOW, 1);
		return (1);
	case EXIT_REASON_INOUT:
		vmm_stat_incr(vcpu->vcpu, VMEXIT_INOUT, 1);
		vmexit->exitcode = VM_EXITCODE_INOUT;
		vmexit->u.inout.bytes = (qual & 0x7) + 1;
		vmexit->u.inout.in = in = (qual & 0x8) ? 1 : 0;
		vmexit->u.inout.string = (qual & 0x10) ? 1 : 0;
		vmexit->u.inout.rep = (qual & 0x20) ? 1 : 0;
		vmexit->u.inout.port = (uint16_t)(qual >> 16);
		vmexit->u.inout.eax = (uint32_t)(vmxctx->guest_rax);
		if (vmexit->u.inout.string) {
			inst_info = vmcs_read(VMCS_EXIT_INSTRUCTION_INFO);
			vmexit->exitcode = VM_EXITCODE_INOUT_STR;
			vis = &vmexit->u.inout_str;
			vmx_paging_info(&vis->paging);
			vis->rflags = vmcs_read(VMCS_GUEST_RFLAGS);
			vis->cr0 = vmcs_read(VMCS_GUEST_CR0);
			vis->index = inout_str_index(vcpu, in);
			vis->count = inout_str_count(vcpu, vis->inout.rep);
			vis->addrsize = inout_str_addrsize(inst_info);
			inout_str_seginfo(vcpu, inst_info, in, vis);
		}
		SDT_PROBE3(vmm, vmx, exit, inout, vmx, vcpuid, vmexit);
		break;
	case EXIT_REASON_CPUID:
		vmm_stat_incr(vcpu->vcpu, VMEXIT_CPUID, 1);
		SDT_PROBE3(vmm, vmx, exit, cpuid, vmx, vcpuid, vmexit);
		handled = vmx_handle_cpuid(vcpu, vmxctx);
		break;
	case EXIT_REASON_EXCEPTION:
		vmm_stat_incr(vcpu->vcpu, VMEXIT_EXCEPTION, 1);
		intr_info = vmcs_read(VMCS_EXIT_INTR_INFO);
		KASSERT((intr_info & VMCS_INTR_VALID) != 0,
		    ("VM exit interruption info invalid: %#x", intr_info));

		intr_vec = intr_info & 0xff;
		intr_type = intr_info & VMCS_INTR_T_MASK;

		/*
		 * If Virtual NMIs control is 1 and the VM-exit is due to a
		 * fault encountered during the execution of IRET then we must
		 * restore the state of "virtual-NMI blocking" before resuming
		 * the guest.
		 *
		 * See "Resuming Guest Software after Handling an Exception".
		 * See "Information for VM Exits Due to Vectored Events".
		 */
		if ((idtvec_info & VMCS_IDT_VEC_VALID) == 0 &&
		    (intr_vec != IDT_DF) &&
		    (intr_info & EXIT_QUAL_NMIUDTI) != 0)
			vmx_restore_nmi_blocking(vcpu);

		/*
		 * The NMI has already been handled in vmx_exit_handle_nmi().
		 */
		if (intr_type == VMCS_INTR_T_NMI)
			return (1);

		/*
		 * Call the machine check handler by hand. Also don't reflect
		 * the machine check back into the guest.
		 */
		if (intr_vec == IDT_MC) {
			VMX_CTR0(vcpu, "Vectoring to MCE handler");
			__asm __volatile("int $18");
			return (1);
		}

		/*
		 * If the hypervisor has requested user exits for
		 * debug exceptions, bounce them out to userland.
		 */
		if (intr_type == VMCS_INTR_T_SWEXCEPTION && intr_vec == IDT_BP &&
		    (vcpu->cap.set & (1 << VM_CAP_BPT_EXIT))) {
			vmexit->exitcode = VM_EXITCODE_BPT;
			vmexit->u.bpt.inst_length = vmexit->inst_length;
			vmexit->inst_length = 0;
			break;
		}

		if (intr_vec == IDT_PF) {
			error = vmxctx_setreg(vmxctx, VM_REG_GUEST_CR2, qual);
			KASSERT(error == 0, ("%s: vmxctx_setreg(cr2) error %d",
			    __func__, error));
		}

		/*
		 * Software exceptions exhibit trap-like behavior. This in
		 * turn requires populating the VM-entry instruction length
		 * so that the %rip in the trap frame is past the INT3/INTO
		 * instruction.
		 */
		if (intr_type == VMCS_INTR_T_SWEXCEPTION)
			vmcs_write(VMCS_ENTRY_INST_LENGTH, vmexit->inst_length);

		/* Reflect all other exceptions back into the guest */
		errcode_valid = errcode = 0;
		if (intr_info & VMCS_INTR_DEL_ERRCODE) {
			errcode_valid = 1;
			errcode = vmcs_read(VMCS_EXIT_INTR_ERRCODE);
		}
		VMX_CTR2(vcpu, "Reflecting exception %d/%#x into "
		    "the guest", intr_vec, errcode);
		SDT_PROBE5(vmm, vmx, exit, exception,
		    vmx, vcpuid, vmexit, intr_vec, errcode);
		error = vm_inject_exception(vcpu->vcpu, intr_vec,
		    errcode_valid, errcode, 0);
		KASSERT(error == 0, ("%s: vm_inject_exception error %d",
		    __func__, error));
		return (1);

	case EXIT_REASON_EPT_FAULT:
		/*
		 * If 'gpa' lies within the address space allocated to
		 * memory then this must be a nested page fault otherwise
		 * this must be an instruction that accesses MMIO space.
		 */
		gpa = vmcs_gpa();
		if (vm_mem_allocated(vcpu->vcpu, gpa) ||
		    ppt_is_mmio(vmx->vm, gpa) || apic_access_fault(vcpu, gpa)) {
			vmexit->exitcode = VM_EXITCODE_PAGING;
			vmexit->inst_length = 0;
			vmexit->u.paging.gpa = gpa;
			vmexit->u.paging.fault_type = ept_fault_type(qual);
			vmm_stat_incr(vcpu->vcpu, VMEXIT_NESTED_FAULT, 1);
			SDT_PROBE5(vmm, vmx, exit, nestedfault,
			    vmx, vcpuid, vmexit, gpa, qual);
		} else if (ept_emulation_fault(qual)) {
			vmexit_inst_emul(vmexit, gpa, vmcs_gla());
			vmm_stat_incr(vcpu->vcpu, VMEXIT_INST_EMUL, 1);
			SDT_PROBE4(vmm, vmx, exit, mmiofault,
			    vmx, vcpuid, vmexit, gpa);
		}
		/*
		 * If Virtual NMIs control is 1 and the VM-exit is due to an
		 * EPT fault during the execution of IRET then we must restore
		 * the state of "virtual-NMI blocking" before resuming.
		 *
		 * See description of "NMI unblocking due to IRET" in
		 * "Exit Qualification for EPT Violations".
		 */
		if ((idtvec_info & VMCS_IDT_VEC_VALID) == 0 &&
		    (qual & EXIT_QUAL_NMIUDTI) != 0)
			vmx_restore_nmi_blocking(vcpu);
		break;
	case EXIT_REASON_VIRTUALIZED_EOI:
		vmexit->exitcode = VM_EXITCODE_IOAPIC_EOI;
		vmexit->u.ioapic_eoi.vector = qual & 0xFF;
		SDT_PROBE3(vmm, vmx, exit, eoi, vmx, vcpuid, vmexit);
		vmexit->inst_length = 0;	/* trap-like */
		break;
	case EXIT_REASON_APIC_ACCESS:
		SDT_PROBE3(vmm, vmx, exit, apicaccess, vmx, vcpuid, vmexit);
		handled = vmx_handle_apic_access(vcpu, vmexit);
		break;
	case EXIT_REASON_APIC_WRITE:
		/*
		 * APIC-write VM exit is trap-like so the %rip is already
		 * pointing to the next instruction.
		 */
		vmexit->inst_length = 0;
		vlapic = vm_lapic(vcpu->vcpu);
		SDT_PROBE4(vmm, vmx, exit, apicwrite,
		    vmx, vcpuid, vmexit, vlapic);
		handled = vmx_handle_apic_write(vcpu, vlapic, qual);
		break;
	case EXIT_REASON_XSETBV:
		SDT_PROBE3(vmm, vmx, exit, xsetbv, vmx, vcpuid, vmexit);
		handled = vmx_emulate_xsetbv(vmx, vcpu, vmexit);
		break;
	case EXIT_REASON_MONITOR:
		SDT_PROBE3(vmm, vmx, exit, monitor, vmx, vcpuid, vmexit);
		vmexit->exitcode = VM_EXITCODE_MONITOR;
		break;
	case EXIT_REASON_MWAIT:
		SDT_PROBE3(vmm, vmx, exit, mwait, vmx, vcpuid, vmexit);
		vmexit->exitcode = VM_EXITCODE_MWAIT;
		break;
	case EXIT_REASON_TPR:
		vlapic = vm_lapic(vcpu->vcpu);
		vlapic_sync_tpr(vlapic);
		vmexit->inst_length = 0;
		handled = HANDLED;
		break;
	case EXIT_REASON_VMCALL:
	case EXIT_REASON_VMCLEAR:
	case EXIT_REASON_VMLAUNCH:
	case EXIT_REASON_VMPTRLD:
	case EXIT_REASON_VMPTRST:
	case EXIT_REASON_VMREAD:
	case EXIT_REASON_VMRESUME:
	case EXIT_REASON_VMWRITE:
	case EXIT_REASON_VMXOFF:
	case EXIT_REASON_VMXON:
		SDT_PROBE3(vmm, vmx, exit, vminsn, vmx, vcpuid, vmexit);
		vmexit->exitcode = VM_EXITCODE_VMINSN;
		break;
	case EXIT_REASON_INVD:
	case EXIT_REASON_WBINVD:
		/* ignore exit */
		handled = HANDLED;
		break;
	default:
		SDT_PROBE4(vmm, vmx, exit, unknown,
		    vmx, vcpuid, vmexit, reason);
		vmm_stat_incr(vcpu->vcpu, VMEXIT_UNKNOWN, 1);
		break;
	}

	if (handled) {
		/*
		 * It is possible that control is returned to userland
		 * even though we were able to handle the VM exit in the
		 * kernel.
		 *
		 * In such a case we want to make sure that the userland
		 * restarts guest execution at the instruction *after*
		 * the one we just processed. Therefore we update the
		 * guest rip in the VMCS and in 'vmexit'.
		 */
		vmexit->rip += vmexit->inst_length;
		vmexit->inst_length = 0;
		vmcs_write(VMCS_GUEST_RIP, vmexit->rip);
	} else {
		if (vmexit->exitcode == VM_EXITCODE_BOGUS) {
			/*
			 * If this VM exit was not claimed by anybody then
			 * treat it as a generic VMX exit.
			 */
			vmexit->exitcode = VM_EXITCODE_VMX;
			vmexit->u.vmx.status = VM_SUCCESS;
			vmexit->u.vmx.inst_type = 0;
			vmexit->u.vmx.inst_error = 0;
		} else {
			/*
			 * The exitcode and collateral have been populated.
			 * The VM exit will be processed further in userland.
			 */
		}
	}

	SDT_PROBE4(vmm, vmx, exit, return,
	    vmx, vcpuid, vmexit, handled);
	return (handled);
}

static __inline void
vmx_exit_inst_error(struct vmxctx *vmxctx, int rc, struct vm_exit *vmexit)
{

	KASSERT(vmxctx->inst_fail_status != VM_SUCCESS,
	    ("vmx_exit_inst_error: invalid inst_fail_status %d",
	    vmxctx->inst_fail_status));

	vmexit->inst_length = 0;
	vmexit->exitcode = VM_EXITCODE_VMX;
	vmexit->u.vmx.status = vmxctx->inst_fail_status;
	vmexit->u.vmx.inst_error = vmcs_instruction_error();
	vmexit->u.vmx.exit_reason = ~0;
	vmexit->u.vmx.exit_qualification = ~0;

	switch (rc) {
	case VMX_VMRESUME_ERROR:
	case VMX_VMLAUNCH_ERROR:
		vmexit->u.vmx.inst_type = rc;
		break;
	default:
		panic("vm_exit_inst_error: vmx_enter_guest returned %d", rc);
	}
}

/*
 * If the NMI-exiting VM execution control is set to '1' then an NMI in
 * non-root operation causes a VM-exit. NMI blocking is in effect so it is
 * sufficient to simply vector to the NMI handler via a software interrupt.
 * However, this must be done before maskable interrupts are enabled
 * otherwise the "iret" issued by an interrupt handler will incorrectly
 * clear NMI blocking.
 */
static __inline void
vmx_exit_handle_nmi(struct vmx_vcpu *vcpu, struct vm_exit *vmexit)
{
	uint32_t intr_info;

	KASSERT((read_rflags() & PSL_I) == 0, ("interrupts enabled"));

	if (vmexit->u.vmx.exit_reason != EXIT_REASON_EXCEPTION)
		return;

	intr_info = vmcs_read(VMCS_EXIT_INTR_INFO);
	KASSERT((intr_info & VMCS_INTR_VALID) != 0,
	    ("VM exit interruption info invalid: %#x", intr_info));

	if ((intr_info & VMCS_INTR_T_MASK) == VMCS_INTR_T_NMI) {
		KASSERT((intr_info & 0xff) == IDT_NMI, ("VM exit due "
		    "to NMI has invalid vector: %#x", intr_info));
		VMX_CTR0(vcpu, "Vectoring to NMI handler");
		__asm __volatile("int $2");
	}
}

static __inline void
vmx_dr_enter_guest(struct vmxctx *vmxctx)
{
	register_t rflags;

	/* Save host control debug registers. */
	vmxctx->host_dr7 = rdr7();
	vmxctx->host_debugctl = rdmsr(MSR_DEBUGCTLMSR);

	/*
	 * Disable debugging in DR7 and DEBUGCTL to avoid triggering
	 * exceptions in the host based on the guest DRx values.  The
	 * guest DR7 and DEBUGCTL are saved/restored in the VMCS.
	 */
	load_dr7(0);
	wrmsr(MSR_DEBUGCTLMSR, 0);

	/*
	 * Disable single stepping the kernel to avoid corrupting the
	 * guest DR6.  A debugger might still be able to corrupt the
	 * guest DR6 by setting a breakpoint after this point and then
	 * single stepping.
	 */
	rflags = read_rflags();
	vmxctx->host_tf = rflags & PSL_T;
	write_rflags(rflags & ~PSL_T);

	/* Save host debug registers. */
	vmxctx->host_dr0 = rdr0();
	vmxctx->host_dr1 = rdr1();
	vmxctx->host_dr2 = rdr2();
	vmxctx->host_dr3 = rdr3();
	vmxctx->host_dr6 = rdr6();

	/* Restore guest debug registers. */
	load_dr0(vmxctx->guest_dr0);
	load_dr1(vmxctx->guest_dr1);
	load_dr2(vmxctx->guest_dr2);
	load_dr3(vmxctx->guest_dr3);
	load_dr6(vmxctx->guest_dr6);
}

static __inline void
vmx_dr_leave_guest(struct vmxctx *vmxctx)
{

	/* Save guest debug registers. */
	vmxctx->guest_dr0 = rdr0();
	vmxctx->guest_dr1 = rdr1();
	vmxctx->guest_dr2 = rdr2();
	vmxctx->guest_dr3 = rdr3();
	vmxctx->guest_dr6 = rdr6();

	/*
	 * Restore host debug registers.  Restore DR7, DEBUGCTL, and
	 * PSL_T last.
	 */
	load_dr0(vmxctx->host_dr0);
	load_dr1(vmxctx->host_dr1);
	load_dr2(vmxctx->host_dr2);
	load_dr3(vmxctx->host_dr3);
	load_dr6(vmxctx->host_dr6);
	wrmsr(MSR_DEBUGCTLMSR, vmxctx->host_debugctl);
	load_dr7(vmxctx->host_dr7);
	write_rflags(read_rflags() | vmxctx->host_tf);
}

static __inline void
vmx_pmap_activate(struct vmx *vmx, pmap_t pmap)
{
	long eptgen;
	int cpu;

	cpu = curcpu;

	CPU_SET_ATOMIC(cpu, &pmap->pm_active);
	smr_enter(pmap->pm_eptsmr);
	eptgen = atomic_load_long(&pmap->pm_eptgen);
	if (eptgen != vmx->eptgen[cpu]) {
		vmx->eptgen[cpu] = eptgen;
		invept(INVEPT_TYPE_SINGLE_CONTEXT,
		    (struct invept_desc){ .eptp = vmx->eptp, ._res = 0 });
	}
}

static __inline void
vmx_pmap_deactivate(struct vmx *vmx, pmap_t pmap)
{
	smr_exit(pmap->pm_eptsmr);
	CPU_CLR_ATOMIC(curcpu, &pmap->pm_active);
}

static int
vmx_run(void *vcpui, register_t rip, pmap_t pmap, struct vm_eventinfo *evinfo)
{
	int rc, handled, launched;
	struct vmx *vmx;
	struct vmx_vcpu *vcpu;
	struct vmxctx *vmxctx;
	struct vmcs *vmcs;
	struct vm_exit *vmexit;
	struct vlapic *vlapic;
	uint32_t exit_reason;
	struct region_descriptor gdtr, idtr;
	uint16_t ldt_sel;

	vcpu = vcpui;
	vmx = vcpu->vmx;
	vmcs = vcpu->vmcs;
	vmxctx = &vcpu->ctx;
	vlapic = vm_lapic(vcpu->vcpu);
	vmexit = vm_exitinfo(vcpu->vcpu);
	launched = 0;

	KASSERT(vmxctx->pmap == pmap,
	    ("pmap %p different than ctx pmap %p", pmap, vmxctx->pmap));

	vmx_msr_guest_enter(vcpu);

	VMPTRLD(vmcs);

	/*
	 * XXX
	 * We do this every time because we may setup the virtual machine
	 * from a different process than the one that actually runs it.
	 *
	 * If the life of a virtual machine was spent entirely in the context
	 * of a single process we could do this once in vmx_init().
	 */
	vmcs_write(VMCS_HOST_CR3, rcr3());

	vmcs_write(VMCS_GUEST_RIP, rip);
	vmx_set_pcpu_defaults(vmx, vcpu, pmap);
	do {
		KASSERT(vmcs_guest_rip() == rip, ("%s: vmcs guest rip mismatch "
		    "%#lx/%#lx", __func__, vmcs_guest_rip(), rip));

		handled = UNHANDLED;
		/*
		 * Interrupts are disabled from this point on until the
		 * guest starts executing. This is done for the following
		 * reasons:
		 *
		 * If an AST is asserted on this thread after the check below,
		 * then the IPI_AST notification will not be lost, because it
		 * will cause a VM exit due to external interrupt as soon as
		 * the guest state is loaded.
		 *
		 * A posted interrupt after 'vmx_inject_interrupts()' will
		 * not be "lost" because it will be held pending in the host
		 * APIC because interrupts are disabled. The pending interrupt
		 * will be recognized as soon as the guest state is loaded.
		 *
		 * The same reasoning applies to the IPI generated by
		 * pmap_invalidate_ept().
		 */
		disable_intr();
		vmx_inject_interrupts(vcpu, vlapic, rip);

		/*
		 * Check for vcpu suspension after injecting events because
		 * vmx_inject_interrupts() can suspend the vcpu due to a
		 * triple fault.
		 */
		if (vcpu_suspended(evinfo)) {
			enable_intr();
			vm_exit_suspended(vcpu->vcpu, rip);
			break;
		}

		if (vcpu_rendezvous_pending(vcpu->vcpu, evinfo)) {
			enable_intr();
			vm_exit_rendezvous(vcpu->vcpu, rip);
			break;
		}

		if (vcpu_reqidle(evinfo)) {
			enable_intr();
			vm_exit_reqidle(vcpu->vcpu, rip);
			break;
		}

		if (vcpu_should_yield(vcpu->vcpu)) {
			enable_intr();
			vm_exit_astpending(vcpu->vcpu, rip);
			vmx_astpending_trace(vcpu, rip);
			handled = HANDLED;
			break;
		}

		if (vcpu_debugged(vcpu->vcpu)) {
			enable_intr();
			vm_exit_debug(vcpu->vcpu, rip);
			break;
		}

		/*
		 * If TPR Shadowing is enabled, the TPR Threshold
		 * must be updated right before entering the guest.
		 */
		if (tpr_shadowing && !virtual_interrupt_delivery) {
			if ((vcpu->cap.proc_ctls & PROCBASED_USE_TPR_SHADOW) != 0) {
				vmcs_write(VMCS_TPR_THRESHOLD, vlapic_get_cr8(vlapic));
			}
		}

		/*
		 * VM exits restore the base address but not the
		 * limits of GDTR and IDTR.  The VMCS only stores the
		 * base address, so VM exits set the limits to 0xffff.
		 * Save and restore the full GDTR and IDTR to restore
		 * the limits.
		 *
		 * The VMCS does not save the LDTR at all, and VM
		 * exits clear LDTR as if a NULL selector were loaded.
		 * The userspace hypervisor probably doesn't use a
		 * LDT, but save and restore it to be safe.
		 */
		sgdt(&gdtr);
		sidt(&idtr);
		ldt_sel = sldt();

		/*
		 * The TSC_AUX MSR must be saved/restored while interrupts
		 * are disabled so that it is not possible for the guest
		 * TSC_AUX MSR value to be overwritten by the resume
		 * portion of the IPI_SUSPEND codepath. This is why the
		 * transition of this MSR is handled separately from those
		 * handled by vmx_msr_guest_{enter,exit}(), which are ok to
		 * be transitioned with preemption disabled but interrupts
		 * enabled.
		 *
		 * These vmx_msr_guest_{enter,exit}_tsc_aux() calls can be
		 * anywhere in this loop so long as they happen with
		 * interrupts disabled. This location is chosen for
		 * simplicity.
		 */
		vmx_msr_guest_enter_tsc_aux(vmx, vcpu);

		vmx_dr_enter_guest(vmxctx);

		/*
		 * Mark the EPT as active on this host CPU and invalidate
		 * EPTP-tagged TLB entries if required.
		 */
		vmx_pmap_activate(vmx, pmap);

		vmx_run_trace(vcpu);
		rc = vmx_enter_guest(vmxctx, vmx, launched);

		vmx_pmap_deactivate(vmx, pmap);
		vmx_dr_leave_guest(vmxctx);
		vmx_msr_guest_exit_tsc_aux(vmx, vcpu);

		bare_lgdt(&gdtr);
		lidt(&idtr);
		lldt(ldt_sel);

		/* Collect some information for VM exit processing */
		vmexit->rip = rip = vmcs_guest_rip();
		vmexit->inst_length = vmexit_instruction_length();
		vmexit->u.vmx.exit_reason = exit_reason = vmcs_exit_reason();
		vmexit->u.vmx.exit_qualification = vmcs_exit_qualification();

		/* Update 'nextrip' */
		vcpu->state.nextrip = rip;

		if (rc == VMX_GUEST_VMEXIT) {
			vmx_exit_handle_nmi(vcpu, vmexit);
			enable_intr();
			handled = vmx_exit_process(vmx, vcpu, vmexit);
		} else {
			enable_intr();
			vmx_exit_inst_error(vmxctx, rc, vmexit);
		}
		launched = 1;
		vmx_exit_trace(vcpu, rip, exit_reason, handled);
		rip = vmexit->rip;
	} while (handled);

	/*
	 * If a VM exit has been handled then the exitcode must be BOGUS
	 * If a VM exit is not handled then the exitcode must not be BOGUS
	 */
	if ((handled && vmexit->exitcode != VM_EXITCODE_BOGUS) ||
	    (!handled && vmexit->exitcode == VM_EXITCODE_BOGUS)) {
		panic("Mismatch between handled (%d) and exitcode (%d)",
		      handled, vmexit->exitcode);
	}

	VMX_CTR1(vcpu, "returning from vmx_run: exitcode %d",
	    vmexit->exitcode);

	VMCLEAR(vmcs);
	vmx_msr_guest_exit(vcpu);

	return (0);
}

static void
vmx_vcpu_cleanup(void *vcpui)
{
	struct vmx_vcpu *vcpu = vcpui;

	vpid_free(vcpu->state.vpid);
	free(vcpu->pir_desc, M_VMX);
	free(vcpu->apic_page, M_VMX);
	free(vcpu->vmcs, M_VMX);
	free(vcpu, M_VMX);
}

static void
vmx_cleanup(void *vmi)
{
	struct vmx *vmx = vmi;

	if (virtual_interrupt_delivery)
		vm_unmap_mmio(vmx->vm, DEFAULT_APIC_BASE, PAGE_SIZE);

	free(vmx->msr_bitmap, M_VMX);
	free(vmx, M_VMX);

	return;
}

static register_t *
vmxctx_regptr(struct vmxctx *vmxctx, int reg)
{

	switch (reg) {
	case VM_REG_GUEST_RAX:
		return (&vmxctx->guest_rax);
	case VM_REG_GUEST_RBX:
		return (&vmxctx->guest_rbx);
	case VM_REG_GUEST_RCX:
		return (&vmxctx->guest_rcx);
	case VM_REG_GUEST_RDX:
		return (&vmxctx->guest_rdx);
	case VM_REG_GUEST_RSI:
		return (&vmxctx->guest_rsi);
	case VM_REG_GUEST_RDI:
		return (&vmxctx->guest_rdi);
	case VM_REG_GUEST_RBP:
		return (&vmxctx->guest_rbp);
	case VM_REG_GUEST_R8:
		return (&vmxctx->guest_r8);
	case VM_REG_GUEST_R9:
		return (&vmxctx->guest_r9);
	case VM_REG_GUEST_R10:
		return (&vmxctx->guest_r10);
	case VM_REG_GUEST_R11:
		return (&vmxctx->guest_r11);
	case VM_REG_GUEST_R12:
		return (&vmxctx->guest_r12);
	case VM_REG_GUEST_R13:
		return (&vmxctx->guest_r13);
	case VM_REG_GUEST_R14:
		return (&vmxctx->guest_r14);
	case VM_REG_GUEST_R15:
		return (&vmxctx->guest_r15);
	case VM_REG_GUEST_CR2:
		return (&vmxctx->guest_cr2);
	case VM_REG_GUEST_DR0:
		return (&vmxctx->guest_dr0);
	case VM_REG_GUEST_DR1:
		return (&vmxctx->guest_dr1);
	case VM_REG_GUEST_DR2:
		return (&vmxctx->guest_dr2);
	case VM_REG_GUEST_DR3:
		return (&vmxctx->guest_dr3);
	case VM_REG_GUEST_DR6:
		return (&vmxctx->guest_dr6);
	default:
		break;
	}
	return (NULL);
}

static int
vmxctx_getreg(struct vmxctx *vmxctx, int reg, uint64_t *retval)
{
	register_t *regp;

	if ((regp = vmxctx_regptr(vmxctx, reg)) != NULL) {
		*retval = *regp;
		return (0);
	} else
		return (EINVAL);
}

static int
vmxctx_setreg(struct vmxctx *vmxctx, int reg, uint64_t val)
{
	register_t *regp;

	if ((regp = vmxctx_regptr(vmxctx, reg)) != NULL) {
		*regp = val;
		return (0);
	} else
		return (EINVAL);
}

static int
vmx_get_intr_shadow(struct vmx_vcpu *vcpu, int running, uint64_t *retval)
{
	uint64_t gi;
	int error;

	error = vmcs_getreg(vcpu->vmcs, running,
	    VMCS_IDENT(VMCS_GUEST_INTERRUPTIBILITY), &gi);
	*retval = (gi & HWINTR_BLOCKING) ? 1 : 0;
	return (error);
}

static int
vmx_modify_intr_shadow(struct vmx_vcpu *vcpu, int running, uint64_t val)
{
	struct vmcs *vmcs;
	uint64_t gi;
	int error, ident;

	/*
	 * Forcing the vcpu into an interrupt shadow is not supported.
	 */
	if (val) {
		error = EINVAL;
		goto done;
	}

	vmcs = vcpu->vmcs;
	ident = VMCS_IDENT(VMCS_GUEST_INTERRUPTIBILITY);
	error = vmcs_getreg(vmcs, running, ident, &gi);
	if (error == 0) {
		gi &= ~HWINTR_BLOCKING;
		error = vmcs_setreg(vmcs, running, ident, gi);
	}
done:
	VMX_CTR2(vcpu, "Setting intr_shadow to %#lx %s", val,
	    error ? "failed" : "succeeded");
	return (error);
}

static int
vmx_shadow_reg(int reg)
{
	int shreg;

	shreg = -1;

	switch (reg) {
	case VM_REG_GUEST_CR0:
		shreg = VMCS_CR0_SHADOW;
		break;
	case VM_REG_GUEST_CR4:
		shreg = VMCS_CR4_SHADOW;
		break;
	default:
		break;
	}

	return (shreg);
}

static int
vmx_getreg(void *vcpui, int reg, uint64_t *retval)
{
	int running, hostcpu;
	struct vmx_vcpu *vcpu = vcpui;
	struct vmx *vmx = vcpu->vmx;

	running = vcpu_is_running(vcpu->vcpu, &hostcpu);
	if (running && hostcpu != curcpu)
		panic("vmx_getreg: %s%d is running", vm_name(vmx->vm),
		    vcpu->vcpuid);

	switch (reg) {
	case VM_REG_GUEST_INTR_SHADOW:
		return (vmx_get_intr_shadow(vcpu, running, retval));
	case VM_REG_GUEST_KGS_BASE:
		*retval = vcpu->guest_msrs[IDX_MSR_KGSBASE];
		return (0);
	case VM_REG_GUEST_TPR:
		*retval = vlapic_get_cr8(vm_lapic(vcpu->vcpu));
		return (0);
	}

	if (vmxctx_getreg(&vcpu->ctx, reg, retval) == 0)
		return (0);

	return (vmcs_getreg(vcpu->vmcs, running, reg, retval));
}

static int
vmx_setreg(void *vcpui, int reg, uint64_t val)
{
	int error, hostcpu, running, shadow;
	uint64_t ctls;
	pmap_t pmap;
	struct vmx_vcpu *vcpu = vcpui;
	struct vmx *vmx = vcpu->vmx;

	running = vcpu_is_running(vcpu->vcpu, &hostcpu);
	if (running && hostcpu != curcpu)
		panic("vmx_setreg: %s%d is running", vm_name(vmx->vm),
		    vcpu->vcpuid);

	if (reg == VM_REG_GUEST_INTR_SHADOW)
		return (vmx_modify_intr_shadow(vcpu, running, val));

	if (vmxctx_setreg(&vcpu->ctx, reg, val) == 0)
		return (0);

	/* Do not permit user write access to VMCS fields by offset. */
	if (reg < 0)
		return (EINVAL);

	error = vmcs_setreg(vcpu->vmcs, running, reg, val);

	if (error == 0) {
		/*
		 * If the "load EFER" VM-entry control is 1 then the
		 * value of EFER.LMA must be identical to "IA-32e mode guest"
		 * bit in the VM-entry control.
		 */
		if ((entry_ctls & VM_ENTRY_LOAD_EFER) != 0 &&
		    (reg == VM_REG_GUEST_EFER)) {
			vmcs_getreg(vcpu->vmcs, running,
				    VMCS_IDENT(VMCS_ENTRY_CTLS), &ctls);
			if (val & EFER_LMA)
				ctls |= VM_ENTRY_GUEST_LMA;
			else
				ctls &= ~VM_ENTRY_GUEST_LMA;
			vmcs_setreg(vcpu->vmcs, running,
				    VMCS_IDENT(VMCS_ENTRY_CTLS), ctls);
		}

		shadow = vmx_shadow_reg(reg);
		if (shadow > 0) {
			/*
			 * Store the unmodified value in the shadow
			 */
			error = vmcs_setreg(vcpu->vmcs, running,
				    VMCS_IDENT(shadow), val);
		}

		if (reg == VM_REG_GUEST_CR3) {
			/*
			 * Invalidate the guest vcpu's TLB mappings to emulate
			 * the behavior of updating %cr3.
			 *
			 * XXX the processor retains global mappings when %cr3
			 * is updated but vmx_invvpid() does not.
			 */
			pmap = vcpu->ctx.pmap;
			vmx_invvpid(vmx, vcpu, pmap, running);
		}
	}

	return (error);
}

static int
vmx_getdesc(void *vcpui, int reg, struct seg_desc *desc)
{
	int hostcpu, running;
	struct vmx_vcpu *vcpu = vcpui;
	struct vmx *vmx = vcpu->vmx;

	running = vcpu_is_running(vcpu->vcpu, &hostcpu);
	if (running && hostcpu != curcpu)
		panic("vmx_getdesc: %s%d is running", vm_name(vmx->vm),
		    vcpu->vcpuid);

	return (vmcs_getdesc(vcpu->vmcs, running, reg, desc));
}

static int
vmx_setdesc(void *vcpui, int reg, struct seg_desc *desc)
{
	int hostcpu, running;
	struct vmx_vcpu *vcpu = vcpui;
	struct vmx *vmx = vcpu->vmx;

	running = vcpu_is_running(vcpu->vcpu, &hostcpu);
	if (running && hostcpu != curcpu)
		panic("vmx_setdesc: %s%d is running", vm_name(vmx->vm),
		    vcpu->vcpuid);

	return (vmcs_setdesc(vcpu->vmcs, running, reg, desc));
}

static int
vmx_getcap(void *vcpui, int type, int *retval)
{
	struct vmx_vcpu *vcpu = vcpui;
	int vcap;
	int ret;

	ret = ENOENT;

	vcap = vcpu->cap.set;

	switch (type) {
	case VM_CAP_HALT_EXIT:
		if (cap_halt_exit)
			ret = 0;
		break;
	case VM_CAP_PAUSE_EXIT:
		if (cap_pause_exit)
			ret = 0;
		break;
	case VM_CAP_MTRAP_EXIT:
		if (cap_monitor_trap)
			ret = 0;
		break;
	case VM_CAP_RDPID:
		if (cap_rdpid)
			ret = 0;
		break;
	case VM_CAP_RDTSCP:
		if (cap_rdtscp)
			ret = 0;
		break;
	case VM_CAP_UNRESTRICTED_GUEST:
		if (cap_unrestricted_guest)
			ret = 0;
		break;
	case VM_CAP_ENABLE_INVPCID:
		if (cap_invpcid)
			ret = 0;
		break;
	case VM_CAP_BPT_EXIT:
	case VM_CAP_IPI_EXIT:
		ret = 0;
		break;
	default:
		break;
	}

	if (ret == 0)
		*retval = (vcap & (1 << type)) ? 1 : 0;

	return (ret);
}

static int
vmx_setcap(void *vcpui, int type, int val)
{
	struct vmx_vcpu *vcpu = vcpui;
	struct vmcs *vmcs = vcpu->vmcs;
	struct vlapic *vlapic;
	uint32_t baseval;
	uint32_t *pptr;
	int error;
	int flag;
	int reg;
	int retval;

	retval = ENOENT;
	pptr = NULL;

	switch (type) {
	case VM_CAP_HALT_EXIT:
		if (cap_halt_exit) {
			retval = 0;
			pptr = &vcpu->cap.proc_ctls;
			baseval = *pptr;
			flag = PROCBASED_HLT_EXITING;
			reg = VMCS_PRI_PROC_BASED_CTLS;
		}
		break;
	case VM_CAP_MTRAP_EXIT:
		if (cap_monitor_trap) {
			retval = 0;
			pptr = &vcpu->cap.proc_ctls;
			baseval = *pptr;
			flag = PROCBASED_MTF;
			reg = VMCS_PRI_PROC_BASED_CTLS;
		}
		break;
	case VM_CAP_PAUSE_EXIT:
		if (cap_pause_exit) {
			retval = 0;
			pptr = &vcpu->cap.proc_ctls;
			baseval = *pptr;
			flag = PROCBASED_PAUSE_EXITING;
			reg = VMCS_PRI_PROC_BASED_CTLS;
		}
		break;
	case VM_CAP_RDPID:
	case VM_CAP_RDTSCP:
		if (cap_rdpid || cap_rdtscp)
			/*
			 * Choose not to support enabling/disabling
			 * RDPID/RDTSCP via libvmmapi since, as per the
			 * discussion in vmx_modinit(), RDPID/RDTSCP are
			 * either always enabled or always disabled.
			 */
			error = EOPNOTSUPP;
		break;
	case VM_CAP_UNRESTRICTED_GUEST:
		if (cap_unrestricted_guest) {
			retval = 0;
			pptr = &vcpu->cap.proc_ctls2;
			baseval = *pptr;
			flag = PROCBASED2_UNRESTRICTED_GUEST;
			reg = VMCS_SEC_PROC_BASED_CTLS;
		}
		break;
	case VM_CAP_ENABLE_INVPCID:
		if (cap_invpcid) {
			retval = 0;
			pptr = &vcpu->cap.proc_ctls2;
			baseval = *pptr;
			flag = PROCBASED2_ENABLE_INVPCID;
			reg = VMCS_SEC_PROC_BASED_CTLS;
		}
		break;
	case VM_CAP_BPT_EXIT:
		retval = 0;

		/* Don't change the bitmap if we are tracing all exceptions. */
		if (vcpu->cap.exc_bitmap != 0xffffffff) {
			pptr = &vcpu->cap.exc_bitmap;
			baseval = *pptr;
			flag = (1 << IDT_BP);
			reg = VMCS_EXCEPTION_BITMAP;
		}
		break;
	case VM_CAP_IPI_EXIT:
		retval = 0;

		vlapic = vm_lapic(vcpu->vcpu);
		vlapic->ipi_exit = val;
		break;
	case VM_CAP_MASK_HWINTR:
		retval = 0;
		break;
	default:
		break;
	}

	if (retval)
		return (retval);

	if (pptr != NULL) {
		if (val) {
			baseval |= flag;
		} else {
			baseval &= ~flag;
		}
		VMPTRLD(vmcs);
		error = vmwrite(reg, baseval);
		VMCLEAR(vmcs);

		if (error)
			return (error);

		/*
		 * Update optional stored flags, and record
		 * setting
		 */
		*pptr = baseval;
	}

	if (val) {
		vcpu->cap.set |= (1 << type);
	} else {
		vcpu->cap.set &= ~(1 << type);
	}

	return (0);
}

static struct vmspace *
vmx_vmspace_alloc(vm_offset_t min, vm_offset_t max)
{
	return (ept_vmspace_alloc(min, max));
}

static void
vmx_vmspace_free(struct vmspace *vmspace)
{
	ept_vmspace_free(vmspace);
}

struct vlapic_vtx {
	struct vlapic	vlapic;
	struct pir_desc	*pir_desc;
	struct vmx_vcpu	*vcpu;
	u_int	pending_prio;
};

#define VPR_PRIO_BIT(vpr)	(1 << ((vpr) >> 4))

#define	VMX_CTR_PIR(vlapic, pir_desc, notify, vector, level, msg)	\
do {									\
	VLAPIC_CTR2(vlapic, msg " assert %s-triggered vector %d",	\
	    level ? "level" : "edge", vector);				\
	VLAPIC_CTR1(vlapic, msg " pir0 0x%016lx", pir_desc->pir[0]);	\
	VLAPIC_CTR1(vlapic, msg " pir1 0x%016lx", pir_desc->pir[1]);	\
	VLAPIC_CTR1(vlapic, msg " pir2 0x%016lx", pir_desc->pir[2]);	\
	VLAPIC_CTR1(vlapic, msg " pir3 0x%016lx", pir_desc->pir[3]);	\
	VLAPIC_CTR1(vlapic, msg " notify: %s", notify ? "yes" : "no");	\
} while (0)

/*
 * vlapic->ops handlers that utilize the APICv hardware assist described in
 * Chapter 29 of the Intel SDM.
 */
static int
vmx_set_intr_ready(struct vlapic *vlapic, int vector, bool level)
{
	struct vlapic_vtx *vlapic_vtx;
	struct pir_desc *pir_desc;
	uint64_t mask;
	int idx, notify = 0;

	vlapic_vtx = (struct vlapic_vtx *)vlapic;
	pir_desc = vlapic_vtx->pir_desc;

	/*
	 * Keep track of interrupt requests in the PIR descriptor. This is
	 * because the virtual APIC page pointed to by the VMCS cannot be
	 * modified if the vcpu is running.
	 */
	idx = vector / 64;
	mask = 1UL << (vector % 64);
	atomic_set_long(&pir_desc->pir[idx], mask);

	/*
	 * A notification is required whenever the 'pending' bit makes a
	 * transition from 0->1.
	 *
	 * Even if the 'pending' bit is already asserted, notification about
	 * the incoming interrupt may still be necessary.  For example, if a
	 * vCPU is HLTed with a high PPR, a low priority interrupt would cause
	 * the 0->1 'pending' transition with a notification, but the vCPU
	 * would ignore the interrupt for the time being.  The same vCPU would
	 * need to then be notified if a high-priority interrupt arrived which
	 * satisfied the PPR.
	 *
	 * The priorities of interrupts injected while 'pending' is asserted
	 * are tracked in a custom bitfield 'pending_prio'.  Should the
	 * to-be-injected interrupt exceed the priorities already present, the
	 * notification is sent.  The priorities recorded in 'pending_prio' are
	 * cleared whenever the 'pending' bit makes another 0->1 transition.
	 */
	if (atomic_cmpset_long(&pir_desc->pending, 0, 1) != 0) {
		notify = 1;
		vlapic_vtx->pending_prio = 0;
	} else {
		const u_int old_prio = vlapic_vtx->pending_prio;
		const u_int prio_bit = VPR_PRIO_BIT(vector & APIC_TPR_INT);

		if ((old_prio & prio_bit) == 0 && prio_bit > old_prio) {
			atomic_set_int(&vlapic_vtx->pending_prio, prio_bit);
			notify = 1;
		}
	}

	VMX_CTR_PIR(vlapic, pir_desc, notify, vector, level,
	    "vmx_set_intr_ready");
	return (notify);
}

static int
vmx_pending_intr(struct vlapic *vlapic, int *vecptr)
{
	struct vlapic_vtx *vlapic_vtx;
	struct pir_desc *pir_desc;
	struct LAPIC *lapic;
	uint64_t pending, pirval;
	uint8_t ppr, vpr, rvi;
	struct vm_exit *vmexit;
	int i;

	/*
	 * This function is only expected to be called from the 'HLT' exit
	 * handler which does not care about the vector that is pending.
	 */
	KASSERT(vecptr == NULL, ("vmx_pending_intr: vecptr must be NULL"));

	vlapic_vtx = (struct vlapic_vtx *)vlapic;
	pir_desc = vlapic_vtx->pir_desc;
	lapic = vlapic->apic_page;

	/*
	 * While a virtual interrupt may have already been
	 * processed the actual delivery maybe pending the
	 * interruptibility of the guest.  Recognize a pending
	 * interrupt by reevaluating virtual interrupts
	 * following Section 30.2.1 in the Intel SDM Volume 3.
	 */
	vmexit = vm_exitinfo(vlapic->vcpu);
	KASSERT(vmexit->exitcode == VM_EXITCODE_HLT,
	    ("vmx_pending_intr: exitcode not 'HLT'"));
	rvi = vmexit->u.hlt.intr_status & APIC_TPR_INT;
	ppr = lapic->ppr & APIC_TPR_INT;
	if (rvi > ppr)
		return (1);

	pending = atomic_load_acq_long(&pir_desc->pending);
	if (!pending)
		return (0);

	/*
	 * If there is an interrupt pending then it will be recognized only
	 * if its priority is greater than the processor priority.
	 *
	 * Special case: if the processor priority is zero then any pending
	 * interrupt will be recognized.
	 */
	if (ppr == 0)
		return (1);

	VLAPIC_CTR1(vlapic, "HLT with non-zero PPR %d", lapic->ppr);

	vpr = 0;
	for (i = 3; i >= 0; i--) {
		pirval = pir_desc->pir[i];
		if (pirval != 0) {
			vpr = (i * 64 + flsl(pirval) - 1) & APIC_TPR_INT;
			break;
		}
	}

	/*
	 * If the highest-priority pending interrupt falls short of the
	 * processor priority of this vCPU, ensure that 'pending_prio' does not
	 * have any stale bits which would preclude a higher-priority interrupt
	 * from incurring a notification later.
	 */
	if (vpr <= ppr) {
		const u_int prio_bit = VPR_PRIO_BIT(vpr);
		const u_int old = vlapic_vtx->pending_prio;

		if (old > prio_bit && (old & prio_bit) == 0) {
			vlapic_vtx->pending_prio = prio_bit;
		}
		return (0);
	}
	return (1);
}

static void
vmx_intr_accepted(struct vlapic *vlapic, int vector)
{

	panic("vmx_intr_accepted: not expected to be called");
}

static void
vmx_set_tmr(struct vlapic *vlapic, int vector, bool level)
{
	struct vlapic_vtx *vlapic_vtx;
	struct vmcs *vmcs;
	uint64_t mask, val;

	KASSERT(vector >= 0 && vector <= 255, ("invalid vector %d", vector));
	KASSERT(!vcpu_is_running(vlapic->vcpu, NULL),
	    ("vmx_set_tmr: vcpu cannot be running"));

	vlapic_vtx = (struct vlapic_vtx *)vlapic;
	vmcs = vlapic_vtx->vcpu->vmcs;
	mask = 1UL << (vector % 64);

	VMPTRLD(vmcs);
	val = vmcs_read(VMCS_EOI_EXIT(vector));
	if (level)
		val |= mask;
	else
		val &= ~mask;
	vmcs_write(VMCS_EOI_EXIT(vector), val);
	VMCLEAR(vmcs);
}

static void
vmx_enable_x2apic_mode_ts(struct vlapic *vlapic)
{
	struct vlapic_vtx *vlapic_vtx;
	struct vmx_vcpu *vcpu;
	struct vmcs *vmcs;
	uint32_t proc_ctls;

	vlapic_vtx = (struct vlapic_vtx *)vlapic;
	vcpu = vlapic_vtx->vcpu;
	vmcs = vcpu->vmcs;

	proc_ctls = vcpu->cap.proc_ctls;
	proc_ctls &= ~PROCBASED_USE_TPR_SHADOW;
	proc_ctls |= PROCBASED_CR8_LOAD_EXITING;
	proc_ctls |= PROCBASED_CR8_STORE_EXITING;
	vcpu->cap.proc_ctls = proc_ctls;

	VMPTRLD(vmcs);
	vmcs_write(VMCS_PRI_PROC_BASED_CTLS, proc_ctls);
	VMCLEAR(vmcs);
}

static void
vmx_enable_x2apic_mode_vid(struct vlapic *vlapic)
{
	struct vlapic_vtx *vlapic_vtx;
	struct vmx *vmx;
	struct vmx_vcpu *vcpu;
	struct vmcs *vmcs;
	uint32_t proc_ctls2;
	int error __diagused;

	vlapic_vtx = (struct vlapic_vtx *)vlapic;
	vcpu = vlapic_vtx->vcpu;
	vmx = vcpu->vmx;
	vmcs = vcpu->vmcs;

	proc_ctls2 = vcpu->cap.proc_ctls2;
	KASSERT((proc_ctls2 & PROCBASED2_VIRTUALIZE_APIC_ACCESSES) != 0,
	    ("%s: invalid proc_ctls2 %#x", __func__, proc_ctls2));

	proc_ctls2 &= ~PROCBASED2_VIRTUALIZE_APIC_ACCESSES;
	proc_ctls2 |= PROCBASED2_VIRTUALIZE_X2APIC_MODE;
	vcpu->cap.proc_ctls2 = proc_ctls2;

	VMPTRLD(vmcs);
	vmcs_write(VMCS_SEC_PROC_BASED_CTLS, proc_ctls2);
	VMCLEAR(vmcs);

	if (vlapic->vcpuid == 0) {
		/*
		 * The nested page table mappings are shared by all vcpus
		 * so unmap the APIC access page just once.
		 */
		error = vm_unmap_mmio(vmx->vm, DEFAULT_APIC_BASE, PAGE_SIZE);
		KASSERT(error == 0, ("%s: vm_unmap_mmio error %d",
		    __func__, error));

		/*
		 * The MSR bitmap is shared by all vcpus so modify it only
		 * once in the context of vcpu 0.
		 */
		error = vmx_allow_x2apic_msrs(vmx);
		KASSERT(error == 0, ("%s: vmx_allow_x2apic_msrs error %d",
		    __func__, error));
	}
}

static void
vmx_post_intr(struct vlapic *vlapic, int hostcpu)
{

	ipi_cpu(hostcpu, pirvec);
}

/*
 * Transfer the pending interrupts in the PIR descriptor to the IRR
 * in the virtual APIC page.
 */
static void
vmx_inject_pir(struct vlapic *vlapic)
{
	struct vlapic_vtx *vlapic_vtx;
	struct pir_desc *pir_desc;
	struct LAPIC *lapic;
	uint64_t val, pirval;
	int rvi, pirbase = -1;
	uint16_t intr_status_old, intr_status_new;

	vlapic_vtx = (struct vlapic_vtx *)vlapic;
	pir_desc = vlapic_vtx->pir_desc;
	if (atomic_cmpset_long(&pir_desc->pending, 1, 0) == 0) {
		VLAPIC_CTR0(vlapic, "vmx_inject_pir: "
		    "no posted interrupt pending");
		return;
	}

	pirval = 0;
	pirbase = -1;
	lapic = vlapic->apic_page;

	val = atomic_readandclear_long(&pir_desc->pir[0]);
	if (val != 0) {
		lapic->irr0 |= val;
		lapic->irr1 |= val >> 32;
		pirbase = 0;
		pirval = val;
	}

	val = atomic_readandclear_long(&pir_desc->pir[1]);
	if (val != 0) {
		lapic->irr2 |= val;
		lapic->irr3 |= val >> 32;
		pirbase = 64;
		pirval = val;
	}

	val = atomic_readandclear_long(&pir_desc->pir[2]);
	if (val != 0) {
		lapic->irr4 |= val;
		lapic->irr5 |= val >> 32;
		pirbase = 128;
		pirval = val;
	}

	val = atomic_readandclear_long(&pir_desc->pir[3]);
	if (val != 0) {
		lapic->irr6 |= val;
		lapic->irr7 |= val >> 32;
		pirbase = 192;
		pirval = val;
	}

	VLAPIC_CTR_IRR(vlapic, "vmx_inject_pir");

	/*
	 * Update RVI so the processor can evaluate pending virtual
	 * interrupts on VM-entry.
	 *
	 * It is possible for pirval to be 0 here, even though the
	 * pending bit has been set. The scenario is:
	 * CPU-Y is sending a posted interrupt to CPU-X, which
	 * is running a guest and processing posted interrupts in h/w.
	 * CPU-X will eventually exit and the state seen in s/w is
	 * the pending bit set, but no PIR bits set.
	 *
	 *      CPU-X                      CPU-Y
	 *   (vm running)                (host running)
	 *   rx posted interrupt
	 *   CLEAR pending bit
	 *				 SET PIR bit
	 *   READ/CLEAR PIR bits
	 *				 SET pending bit
	 *   (vm exit)
	 *   pending bit set, PIR 0
	 */
	if (pirval != 0) {
		rvi = pirbase + flsl(pirval) - 1;
		intr_status_old = vmcs_read(VMCS_GUEST_INTR_STATUS);
		intr_status_new = (intr_status_old & 0xFF00) | rvi;
		if (intr_status_new > intr_status_old) {
			vmcs_write(VMCS_GUEST_INTR_STATUS, intr_status_new);
			VLAPIC_CTR2(vlapic, "vmx_inject_pir: "
			    "guest_intr_status changed from 0x%04x to 0x%04x",
			    intr_status_old, intr_status_new);
		}
	}
}

static struct vlapic *
vmx_vlapic_init(void *vcpui)
{
	struct vmx *vmx;
	struct vmx_vcpu *vcpu;
	struct vlapic *vlapic;
	struct vlapic_vtx *vlapic_vtx;

	vcpu = vcpui;
	vmx = vcpu->vmx;

	vlapic = malloc(sizeof(struct vlapic_vtx), M_VLAPIC, M_WAITOK | M_ZERO);
	vlapic->vm = vmx->vm;
	vlapic->vcpu = vcpu->vcpu;
	vlapic->vcpuid = vcpu->vcpuid;
	vlapic->apic_page = (struct LAPIC *)vcpu->apic_page;

	vlapic_vtx = (struct vlapic_vtx *)vlapic;
	vlapic_vtx->pir_desc = vcpu->pir_desc;
	vlapic_vtx->vcpu = vcpu;

	if (tpr_shadowing) {
		vlapic->ops.enable_x2apic_mode = vmx_enable_x2apic_mode_ts;
	}

	if (virtual_interrupt_delivery) {
		vlapic->ops.set_intr_ready = vmx_set_intr_ready;
		vlapic->ops.pending_intr = vmx_pending_intr;
		vlapic->ops.intr_accepted = vmx_intr_accepted;
		vlapic->ops.set_tmr = vmx_set_tmr;
		vlapic->ops.enable_x2apic_mode = vmx_enable_x2apic_mode_vid;
	}

	if (posted_interrupts)
		vlapic->ops.post_intr = vmx_post_intr;

	vlapic_init(vlapic);

	return (vlapic);
}

static void
vmx_vlapic_cleanup(struct vlapic *vlapic)
{

	vlapic_cleanup(vlapic);
	free(vlapic, M_VLAPIC);
}

#ifdef BHYVE_SNAPSHOT
static int
vmx_vcpu_snapshot(void *vcpui, struct vm_snapshot_meta *meta)
{
	struct vmcs *vmcs;
	struct vmx *vmx;
	struct vmx_vcpu *vcpu;
	struct vmxctx *vmxctx;
	int err, run, hostcpu;

	err = 0;
	vcpu = vcpui;
	vmx = vcpu->vmx;
	vmcs = vcpu->vmcs;

	run = vcpu_is_running(vcpu->vcpu, &hostcpu);
	if (run && hostcpu != curcpu) {
		printf("%s: %s%d is running", __func__, vm_name(vmx->vm),
		    vcpu->vcpuid);
		return (EINVAL);
	}

	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_CR0, meta);
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_CR3, meta);
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_CR4, meta);
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_DR7, meta);
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_RSP, meta);
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_RIP, meta);
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_RFLAGS, meta);

	/* Guest segments */
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_ES, meta);
	err += vmcs_snapshot_desc(vmcs, run, VM_REG_GUEST_ES, meta);

	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_CS, meta);
	err += vmcs_snapshot_desc(vmcs, run, VM_REG_GUEST_CS, meta);

	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_SS, meta);
	err += vmcs_snapshot_desc(vmcs, run, VM_REG_GUEST_SS, meta);

	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_DS, meta);
	err += vmcs_snapshot_desc(vmcs, run, VM_REG_GUEST_DS, meta);

	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_FS, meta);
	err += vmcs_snapshot_desc(vmcs, run, VM_REG_GUEST_FS, meta);

	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_GS, meta);
	err += vmcs_snapshot_desc(vmcs, run, VM_REG_GUEST_GS, meta);

	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_TR, meta);
	err += vmcs_snapshot_desc(vmcs, run, VM_REG_GUEST_TR, meta);

	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_LDTR, meta);
	err += vmcs_snapshot_desc(vmcs, run, VM_REG_GUEST_LDTR, meta);

	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_EFER, meta);

	err += vmcs_snapshot_desc(vmcs, run, VM_REG_GUEST_IDTR, meta);
	err += vmcs_snapshot_desc(vmcs, run, VM_REG_GUEST_GDTR, meta);

	/* Guest page tables */
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_PDPTE0, meta);
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_PDPTE1, meta);
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_PDPTE2, meta);
	err += vmcs_snapshot_reg(vmcs, run, VM_REG_GUEST_PDPTE3, meta);

	/* Other guest state */
	err += vmcs_snapshot_any(vmcs, run, VMCS_GUEST_IA32_SYSENTER_CS, meta);
	err += vmcs_snapshot_any(vmcs, run, VMCS_GUEST_IA32_SYSENTER_ESP, meta);
	err += vmcs_snapshot_any(vmcs, run, VMCS_GUEST_IA32_SYSENTER_EIP, meta);
	err += vmcs_snapshot_any(vmcs, run, VMCS_GUEST_INTERRUPTIBILITY, meta);
	err += vmcs_snapshot_any(vmcs, run, VMCS_GUEST_ACTIVITY, meta);
	err += vmcs_snapshot_any(vmcs, run, VMCS_ENTRY_CTLS, meta);
	err += vmcs_snapshot_any(vmcs, run, VMCS_EXIT_CTLS, meta);
	if (err != 0)
		goto done;

	SNAPSHOT_BUF_OR_LEAVE(vcpu->guest_msrs,
	    sizeof(vcpu->guest_msrs), meta, err, done);

	SNAPSHOT_BUF_OR_LEAVE(vcpu->pir_desc,
	    sizeof(*vcpu->pir_desc), meta, err, done);

	SNAPSHOT_BUF_OR_LEAVE(&vcpu->mtrr,
	    sizeof(vcpu->mtrr), meta, err, done);

	vmxctx = &vcpu->ctx;
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_rdi, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_rsi, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_rdx, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_rcx, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_r8, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_r9, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_rax, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_rbx, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_rbp, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_r10, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_r11, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_r12, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_r13, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_r14, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_r15, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_cr2, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_dr0, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_dr1, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_dr2, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_dr3, meta, err, done);
	SNAPSHOT_VAR_OR_LEAVE(vmxctx->guest_dr6, meta, err, done);

done:
	return (err);
}

static int
vmx_restore_tsc(void *vcpui, uint64_t offset)
{
	struct vmx_vcpu *vcpu = vcpui;
	struct vmcs *vmcs;
	struct vmx *vmx;
	int error, running, hostcpu;

	vmx = vcpu->vmx;
	vmcs = vcpu->vmcs;

	running = vcpu_is_running(vcpu->vcpu, &hostcpu);
	if (running && hostcpu != curcpu) {
		printf("%s: %s%d is running", __func__, vm_name(vmx->vm),
		    vcpu->vcpuid);
		return (EINVAL);
	}

	if (!running)
		VMPTRLD(vmcs);

	error = vmx_set_tsc_offset(vcpu, offset);

	if (!running)
		VMCLEAR(vmcs);
	return (error);
}
#endif

const struct vmm_ops vmm_ops_intel = {
	.modinit	= vmx_modinit,
	.modcleanup	= vmx_modcleanup,
	.modsuspend	= vmx_modsuspend,
	.modresume	= vmx_modresume,
	.init		= vmx_init,
	.run		= vmx_run,
	.cleanup	= vmx_cleanup,
	.vcpu_init	= vmx_vcpu_init,
	.vcpu_cleanup	= vmx_vcpu_cleanup,
	.getreg		= vmx_getreg,
	.setreg		= vmx_setreg,
	.getdesc	= vmx_getdesc,
	.setdesc	= vmx_setdesc,
	.getcap		= vmx_getcap,
	.setcap		= vmx_setcap,
	.vmspace_alloc	= vmx_vmspace_alloc,
	.vmspace_free	= vmx_vmspace_free,
	.vlapic_init	= vmx_vlapic_init,
	.vlapic_cleanup	= vmx_vlapic_cleanup,
#ifdef BHYVE_SNAPSHOT
	.vcpu_snapshot	= vmx_vcpu_snapshot,
	.restore_tsc	= vmx_restore_tsc,
#endif
};
