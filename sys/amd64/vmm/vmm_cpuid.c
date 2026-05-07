/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Copyright 2014 Pluribus Networks Inc.
 * Copyright 2018 Joyent, Inc.
 * Copyright 2025 Oxide Computer Company
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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <dev/vmm/vmm_ktr.h>
#include <dev/vmm/vmm_vm.h>

#include "vmm_host.h"
#include "vmm_util.h"
#include "vlapic.h"
#include "x86.h"

static MALLOC_DEFINE(M_CPUID, "cpuid", "cpuid");

/*
 * CPUID Emulation
 *
 * All CPUID instruction exits are handled by the in-kernel emulation.
 *
 * ----------------
 * Legacy Emulation
 * ----------------
 *
 * Originally, the kernel vmm portion of bhyve relied on fixed logic to filter
 * and/or generate CPUID results based on what was reported by the host CPU, as
 * well as attributes of the VM (such as CPU topology, and enabled features).
 * This is largely adequate to expose CPU capabilities to the guest in manner
 * which allows it to operate properly.
 *
 * ------------------------------
 * Userspace-Controlled Emulation
 * ------------------------------
 *
 * In certain situations, more control over the CPUID emulation results present
 * to the guest is desired.  Live migration between physical hosts is one such
 * example, where the underlying CPUs, or at least their microcode, may differ
 * between the source and destination.  In such cases, where changes to the
 * CPUID results cannot be tolerated, the userspace portion of the VMM can be in
 * complete control over the leaves which are presented to the guest.  It may
 * still consult the "legacy" CPUID data for guidance about which CPU features
 * are safe to expose (due to hypervisor limitations, etc).  This leaf
 * information is configured on a per-vCPU basis.
 *
 * The emulation entries provided by userspace are expected to be in sorted
 * order, running from lowest function and index to highest.
 *
 * For example:
 * (func: 00h idx: 00h) ->
 *     (flags: 0, eax: highest std leaf, ebx-edx: vendor id)
 * (func: 0Dh idx: 00h) ->
 *     (flags: VCE_FLAG_MATCH_INDEX, eax - edx: XCR0/XSAVE info)
 * (func: 0Dh idx: 01h) ->
 *     (flags: VCE_FLAG_MATCH_INDEX, eax - edx: XSAVE/XSAVEOPT details)
 *     ...
 * (func: 0Dh idx: 07H) ->
 *     (flags: VCE_FLAG_MATCH_INDEX, eax - edx: AVX-512 details)
 * (func: 8000000h idx: 0h) ->
 *     (flags: 0, eax: highest extd leaf ...)
 *     ...
 */


#define	CPUID_TYPE_MASK	0xf0000000
#define	CPUID_TYPE_STD	0x00000000
#define	CPUID_TYPE_EXTD	0x80000000

static const struct vcpu_cpuid_entry cpuid_empty_entry = { 0 };

/*
 * Given the CPUID configuration for a vCPU, locate the entry which matches the
 * provided function/index tuple.  The entries list is walked in order, and the
 * first valid match based on the function/index and flags will be emitted.
 *
 * If no match is found, but Intel-style fallback is configured, then the
 * highest standard leaf encountered will be emitted.
 */
static const struct vcpu_cpuid_entry *
cpuid_find_entry(const vcpu_cpuid_config_t *cfg, uint32_t func, uint32_t idx)
{
	const struct vcpu_cpuid_entry *last_std = &cpuid_empty_entry;
	const bool intel_fallback =
	    (cfg->vcc_flags & VCC_FLAG_INTEL_FALLBACK) != 0;
	bool matched_leaf = false;

	KASSERT((cfg->vcc_flags & VCC_FLAG_LEGACY_HANDLING) == 0,
		("legacy CPUID handling enabled"));

	for (u_int i = 0; i < cfg->vcc_nent; i++) {
		const struct vcpu_cpuid_entry *ent = &cfg->vcc_entries[i];
		const bool ent_is_std =
		    (ent->vce_function & CPUID_TYPE_MASK) == CPUID_TYPE_STD;
		const bool ent_must_match_idx =
		    (ent->vce_flags & VCE_FLAG_MATCH_INDEX) != 0;

		if (ent_is_std) {
			/*
			 * Keep track of the last "standard" leaf for
			 * Intel-style fallback behavior.
			 *
			 * This does currently not account for the sub-leaf
			 * index matching behavior for fallback described in the
			 * SDM.  It is not clear if any consumers rely on such
			 * matching when encountering fallback.
			 */
			last_std = ent;
		}
		if (ent->vce_function == func) {
			if (ent->vce_index == idx || !ent_must_match_idx) {
				return (ent);
			}
			/*
			 * Make note of when the top-level leaf matches, even
			 * when the index does not.
			 */
			matched_leaf = true;
		} else if (ent->vce_function > func) {
			if ((ent->vce_function & CPUID_TYPE_MASK) ==
			    (func & CPUID_TYPE_MASK)) {
				/*
				 * We are beyond a valid leaf to match, but have
				 * not exceeded the maximum leaf for this "type"
				 * (standard, extended, hvm, etc), so return an
				 * empty entry.
				 */
				return (&cpuid_empty_entry);
			} else {
				/*
				 * Otherwise, we can stop now, having gone
				 * beyond the last entry which could match the
				 * target function in a sorted list.
				 */
				break;
			}
		}
	}

	if (matched_leaf || !intel_fallback) {
		return (&cpuid_empty_entry);
	} else {
		return (last_std);
	}
}

/*
 * Updates a previously-populated set of CPUID return values to account for the
 * runtime state of the executing vCPU, i.e., the values in its control
 * registers and MSRs that influence the values returned by the CPUID
 * instruction.
 *
 * This function does not account for "static" properties of the vCPU or VM,
 * such as the enablement of VM-wide features and capabilities (like x2APIC or
 * INVPCID support) or settings that vary only with the vCPU's ID (like the
 * values returned from its topology leaves).
 *
 * This function assumes that it is called from within VMRUN(), which guarantees
 * that the guest's FPU state is loaded. This is required to obtain the correct
 * values for leaves whose values depend on the guest values of %xcr0 and the
 * IA32_XSS MSR.
 */
static void
cpuid_apply_runtime_reg_state(struct vcpu *vcpu, uint32_t func,
    uint32_t index, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint64_t cr4;
	int error;
	unsigned int regs[4];

	switch (func) {
	case CPUID_0000_0001:
		/*
		 * If CPUID2_XSAVE is being advertised and the
		 * guest has set CR4_XSAVE, set CPUID2_OSXSAVE.
		 */
		*ecx &= ~CPUID2_OSXSAVE;
		if ((*ecx & CPUID2_XSAVE) != 0) {
			error = vm_get_register(vcpu,
			    VM_REG_GUEST_CR4, &cr4);
			if (error)
				panic("cpuid_apply_runtime_reg_state: error %d "
				    "fetching %%cr4", error);
			if ((cr4 & CR4_XSAVE) != 0) {
				*ecx |= CPUID2_OSXSAVE;
			}
		}

		/*
		 * AMD APM vol. 3 rev. 3.36 section E.3.2 notes that this bit is
		 * set only if the "APIC exists and is enabled." Vol. 3 of the
		 * June 2024 Intel SDM notes in section 11.4.3 that "[t]he CPUID
		 * feature flag for the APIC ... is also set to 0" when the APIC
		 * enable bit is cleared.
		 */
		if (vlapic_hw_disabled(vm_lapic(vcpu))) {
			*edx &= ~CPUID_APIC;
		}
		break;

	case CPUID_0000_000D:
		/*
		 * Leaf D reports XSAVE area sizes that vary with the current
		 * value of %xcr0. Since this function is called with %xcr0
		 * still set to its guest value, the easiest way to get the
		 * correct output is to execute CPUID on the host and copy out
		 * the relevant values.
		 */
		cpuid_count(func, index, regs);
		switch (index) {
		case 0:
			/*
			 * %eax, %ecx, and %edx return information about the
			 * complete set of features the processor supports, not
			 * just the ones that are enabled. The caller is
			 * presumed to have set these already, so just update
			 * %ebx.
			 */
			*ebx = regs[1];
			break;
		case 1:
			/*
			 * Subleaf 1 reports the XSAVE area size required for
			 * features enabled in %xcr0 and the IA32_XSS MSR via
			 * %ebx. As with subleaf 0, the caller is presumed to
			 * have set the other three output register values
			 * already.
			 *
			 * AMD APM vol. 3 rev. 3.36 and the June 2024 edition of
			 * volume 2 of the Intel SDM specify slightly different
			 * behavior here: the SDM says that the value returned
			 * in %ebx depends in part on whether %eax advertises
			 * XSAVEC and IA32_XSS support, but the APM does not. To
			 * handle these cases:
			 *
			 * 1. If the guest isn't a VMX guest, just copy the
			 *    current reported save area size.
			 * 2. If both the XSAVEC and XSAVES bits are clear in
			 *    %eax, return a save area size of 0 in %ebx to
			 *    match the SDM description.
			 * 3. Otherwise, copy the host's reported save area
			 *    size.
			 *
			 * Note that, because XSAVES saves a superset of the
			 * state saved by XSAVEC, it's OK to report the host's
			 * save area size even if the host and guest report
			 * different feature bits in %eax:
			 *
			 * - If the host supports XSAVES and the guest doesn't,
			 *   the reported save area size will be too large, but
			 *   the guest can still use XSAVEC safely.
			 * - If the VM's explicit CPUID values advertise XSAVES
			 *   support, but the host doesn't support XSAVES, the
			 *   host's reported save area size will still be large
			 *   enough for the xcr0-controlled state saved by
			 *   XSAVEC. The area will be undersized for XSAVES,
			 *   but this is OK because the guest can't execute
			 *   XSAVES anyway (it will #UD).
			 */
			if (!vmm_is_intel()) {
				*ebx = regs[1];
			} else {
				if ((*eax & (CPUID_EXTSTATE_XSAVEC |
				    CPUID_EXTSTATE_XSAVES)) == 0) {
					*ebx = 0;
				} else {
					*ebx = regs[1];
				}
			}
			break;
		default:
			/*
			 * Other subleaves of leaf D report the relative sizes
			 * and offsets of the state required for specific
			 * features in the relevant offset masks. These don't
			 * depend on the current enabled features (only the
			 * supported ones), so no enabled-feature specialization
			 * is required.
			 */
			break;
		}
		break;
	}
}

/*
 * Emulates the CPUID instruction on the specified vCPU and returns its outputs
 * in the rax/rbx/rcx/rdx variables.
 *
 * This function assumes it is called from within VMRUN(), which guarantees that
 * certain guest state (e.g. FPU state) remains loaded.
 */
void
vcpu_emulate_cpuid(struct vcpu *vcpu, uint64_t *rax, uint64_t *rbx,
    uint64_t *rcx, uint64_t *rdx)
{
	const vcpu_cpuid_config_t *cfg = vm_cpuid_config(vcpu);
	uint32_t func, index;

	KASSERT(rax != NULL, ("rax == NULL"));
	KASSERT(rbx != NULL, ("rbx == NULL"));
	KASSERT(rcx != NULL, ("rcx == NULL"));
	KASSERT(rdx != NULL, ("rdx == NULL"));

	uint32_t regs[4] = { *rax, 0, *rcx, 0 };
	func = (uint32_t)*rax;
	index = (uint32_t)*rcx;

	/* Fall back to legacy handling if specified */
	if ((cfg->vcc_flags & VCC_FLAG_LEGACY_HANDLING) != 0) {
		legacy_emulate_cpuid(vcpu, &regs[0], &regs[1], &regs[2],
		    &regs[3]);
	} else {
		const struct vcpu_cpuid_entry *ent = cpuid_find_entry(cfg, func,
		    index);
		KASSERT(ent != NULL, ("ent == NULL"));

		/*
		 * The function and index in the found entry may differ from
		 * what the guest requested (if the entry was chosen via the
		 * "highest leaf" fallback described above). Use the values
		 * from the entry to ensure that the correct vCPU state fixups
		 * get applied below.
		 *
		 * The found entry may also be an all-zero empty entry (if the
		 * requested leaf is invalid but is less than the maximum valid
		 * leaf). It's OK to fall through in this case because leaf 0
		 * never has any CPU state-based fixups to apply.
		 */
		func = ent->vce_function;
		index = ent->vce_index;
		regs[0] = ent->vce_eax;
		regs[1] = ent->vce_ebx;
		regs[2] = ent->vce_ecx;
		regs[3] = ent->vce_edx;
	}

	/* Fix up any returned values that vary with guest register state. */
	cpuid_apply_runtime_reg_state(vcpu, func, index, &regs[0],
	    &regs[1], &regs[2], &regs[3]);

	/* CPUID clears the upper 32-bits of the long-mode registers. */
	*rax = regs[0];
	*rbx = regs[1];
	*rcx = regs[2];
	*rdx = regs[3];
}

/*
 * Get the current CPUID emulation configuration for this vCPU.
 *
 * Only the existing flags will be emitted if the vCPU is configured for legacy
 * operation via the VCC_FLAG_LEGACY_HANDLING flag.  If in userspace-controlled
 * mode, then we will attempt to copy the existing entries into vcc_entries,
 * its side specified by vcc_nent.
 *
 * Regardless of whether vcc_entries is adequately sized (or even present),
 * vcc_nent will be set to the number of existing entries.
 */
int
vm_get_cpuid(struct vcpu *vcpu, vcpu_cpuid_config_t *res)
{
	const vcpu_cpuid_config_t *src = vm_cpuid_config(vcpu);
	if (src->vcc_nent > res->vcc_nent) {
		res->vcc_nent = src->vcc_nent;
		return (E2BIG);
	} else if (src->vcc_nent != 0) {
		bcopy(src->vcc_entries, res->vcc_entries,
		    src->vcc_nent * sizeof(struct vcpu_cpuid_entry));
	}
	res->vcc_flags = src->vcc_flags;
	res->vcc_nent = src->vcc_nent;
	return (0);
}

/*
 * Set the CPUID emulation configuration for this vCPU.
 *
 * If VCC_FLAG_LEGACY_HANDLING is set in vcc_flags, then vcc_nent is expected to
 * be set to 0, as configuring a list of entries would be useless when using the
 * legacy handling.
 *
 * Any existing entries which are configured are freed, and the newly provided
 * ones will be copied into their place.
 */
int
vm_set_cpuid(struct vcpu *vcpu, const vcpu_cpuid_config_t *src)
{
	if (src->vcc_nent > VMM_MAX_CPUID_ENTRIES) {
		return (EINVAL);
	}
	if ((src->vcc_flags & ~VCC_FLAGS_VALID) != 0) {
		return (EINVAL);
	}
	if ((src->vcc_flags & VCC_FLAG_LEGACY_HANDLING) != 0 &&
	    src->vcc_nent != 0) {
		/* No entries should be provided if using legacy handling */
		return (EINVAL);
	}
	for (u_int i = 0; i < src->vcc_nent; i++) {
		/* Ensure all entries carry valid flags */
		if ((src->vcc_entries[i].vce_flags & ~VCE_FLAGS_VALID) != 0) {
			return (EINVAL);
		}
	}

	vcpu_cpuid_config_t *cfg = vm_cpuid_config(vcpu);

	/* Free any existing entries first */
	vcpu_cpuid_cleanup(cfg);

	/* Copy supplied entries into freshly allocated space */
	if (src->vcc_nent != 0) {
		const size_t entries_sz =
		    src->vcc_nent * sizeof(struct vcpu_cpuid_entry);

		cfg->vcc_nent = src->vcc_nent;
		cfg->vcc_entries = malloc(entries_sz, M_CPUID, M_WAITOK);
		bcopy(src->vcc_entries, cfg->vcc_entries, entries_sz);
	}
	cfg->vcc_flags = src->vcc_flags;

	return (0);
}

void
vcpu_cpuid_init(vcpu_cpuid_config_t *cfg)
{
	/* Default to legacy-style handling */
	cfg->vcc_flags = VCC_FLAG_LEGACY_HANDLING;
	cfg->vcc_nent = 0;
	cfg->vcc_entries = NULL;
}

void
vcpu_cpuid_cleanup(vcpu_cpuid_config_t *cfg)
{
	if (cfg->vcc_nent != 0) {
		KASSERT(cfg->vcc_entries != NULL, ("cfg->vcc_entries == NULL"));

		free(cfg->vcc_entries, M_CPUID);

		cfg->vcc_nent = 0;
		cfg->vcc_entries = NULL;
	}
}

SYSCTL_DECL(_hw_vmm);
static SYSCTL_NODE(_hw_vmm, OID_AUTO, topology, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    NULL);

static uint64_t bhyve_xcpuids;
SYSCTL_ULONG(_hw_vmm, OID_AUTO, bhyve_xcpuids, CTLFLAG_RW, &bhyve_xcpuids, 0,
    "Number of times an unknown cpuid leaf was accessed");

static int cpuid_leaf_b = 1;
SYSCTL_INT(_hw_vmm_topology, OID_AUTO, cpuid_leaf_b, CTLFLAG_RDTUN,
    &cpuid_leaf_b, 0, NULL);

static const char bhyve_id[12] = "bhyve bhyve ";

/*
 * Compute ceil(log2(x)).  Returns -1 if x is zero.
 */
static __inline int
log2(u_int x)
{

	return (x == 0 ? -1 : order_base_2(x));
}

/*
 * The "legacy" bhyve cpuid emulation, which largly applies statically defined
 * masks to the data provided by the host CPU.
 */
void
legacy_emulate_cpuid(struct vcpu *vcpu, uint32_t *eax, uint32_t *ebx,
    uint32_t *ecx, uint32_t *edx)
{
	struct vm *vm = vcpu_vm(vcpu);
	int vcpu_id = vcpu_vcpuid(vcpu);
	const struct xsave_limits *limits;
	int error, enable_invpcid, enable_rdpid, enable_rdtscp, level,
	    width, x2apic_id;
	unsigned int func, regs[4], logical_cpus, param;
	enum x2apic_state x2apic_state;
	uint16_t cores, maxcpus, sockets, threads;

	/*
	 * The function of CPUID is controlled through the provided value of
	 * %eax (and secondarily %ecx, for certain leaf data).
	 */
	func = (uint32_t)*eax;
	param = (uint32_t)*ecx;

	VCPU_CTR2(vm, vcpu_id, "cpuid %#x,%#x", func, param);

	/*
	 * Requests for invalid CPUID levels should map to the highest
	 * available level instead.
	 */
	if (cpu_exthigh != 0 && func >= 0x80000000) {
		if (func > cpu_exthigh)
			func = cpu_exthigh;
	} else if (func >= CPUID_VM_SIGNATURE + CPUID_VM_BHYVE_OFF) {
		if (func > CPUID_VM_HIGH + CPUID_VM_BHYVE_OFF)
			func = CPUID_VM_HIGH + CPUID_VM_BHYVE_OFF;
	} else if (func >= CPUID_VM_SIGNATURE) {
		if (func > CPUID_VM_HIGH)
			func = CPUID_VM_HIGH;
	} else if (func > cpu_high) {
		func = cpu_high;
	}

	/*
	 * In general the approach used for CPU topology is to
	 * advertise a flat topology where all CPUs are packages with
	 * no multi-core or SMT.
	 */
	switch (func) {
		/*
		 * Pass these through to the guest
		 */
		case CPUID_0000_0000:
		case CPUID_0000_0002:
		case CPUID_0000_0003:
		case CPUID_8000_0000:
		case CPUID_8000_0002:
		case CPUID_8000_0003:
		case CPUID_8000_0004:
		case CPUID_8000_0006:
			cpuid_count(func, param, regs);
			break;
		case CPUID_8000_0008:
			cpuid_count(func, param, regs);
			if (vmm_is_svm()) {
				/*
				 * As on Intel (0000_0007:0, EDX), mask out
				 * unsupported or unsafe AMD extended features
				 * (8000_0008 EBX).
				 */
				regs[1] &= AMDFEID_CLZERO | AMDFEID_XSAVEERPTR;

				vm_get_topology(vm, &sockets, &cores, &threads,
				    &maxcpus);
				/*
				 * Here, width is ApicIdCoreIdSize, present on
				 * at least Family 15h and newer.  It
				 * represents the "number of bits in the
				 * initial apicid that indicate thread id
				 * within a package."
				 *
				 * Our topo_probe_amd() uses it for
				 * pkg_id_shift and other OSes may rely on it.
				 */
				width = MIN(0xF, log2(threads * cores));
				logical_cpus = MIN(0xFF, threads * cores - 1);
				regs[2] = (width << AMDID_COREID_SIZE_SHIFT) |
				    logical_cpus;
			}
			break;

		case CPUID_8000_0001:
			cpuid_count(func, param, regs);

			/*
			 * Hide SVM from guest.
			 */
			regs[2] &= ~AMDID2_SVM;

			/*
			 * Don't advertise extended performance counter MSRs
			 * to the guest.
			 */
			regs[2] &= ~AMDID2_PCXC;
			regs[2] &= ~AMDID2_PNXC;
			regs[2] &= ~AMDID2_PTSCEL2I;

			/*
			 * Don't advertise Instruction Based Sampling feature.
			 */
			regs[2] &= ~AMDID2_IBS;

			/* NodeID MSR not available */
			regs[2] &= ~AMDID2_NODE_ID;

			/* Don't advertise the OS visible workaround feature */
			regs[2] &= ~AMDID2_OSVW;

			/* Hide mwaitx/monitorx capability from the guest */
			regs[2] &= ~AMDID2_MWAITX;

			/* Advertise RDTSCP if it is enabled. */
			error = vm_get_capability(vcpu,
			    VM_CAP_RDTSCP, &enable_rdtscp);
			if (error == 0 && enable_rdtscp)
				regs[3] |= AMDID_RDTSCP;
			else
				regs[3] &= ~AMDID_RDTSCP;
			break;

		case CPUID_8000_0007:
			cpuid_count(func, param, regs);
			/*
			 * AMD uses this leaf to advertise the processor's
			 * power monitoring and RAS capabilities. These
			 * features are hardware-specific and exposing
			 * them to a guest doesn't make a lot of sense.
			 *
			 * Intel uses this leaf only to advertise the
			 * "Invariant TSC" feature with all other bits
			 * being reserved (set to zero).
			 */
			regs[0] = 0;
			regs[1] = 0;
			regs[2] = 0;
			regs[3] = 0;

			/*
			 * "Invariant TSC" can be advertised to the guest if:
			 * - host TSC frequency is invariant
			 * - host TSCs are synchronized across physical cpus
			 *
			 * XXX This still falls short because the vcpu
			 * can observe the TSC moving backwards as it
			 * migrates across physical cpus. But at least
			 * it should discourage the guest from using the
			 * TSC to keep track of time.
			 */
			if (tsc_is_invariant && smp_tsc)
				regs[3] |= AMDPM_TSC_INVARIANT;
			break;

		case CPUID_8000_001D:
			/* AMD Cache topology, like 0000_0004 for Intel. */
			if (!vmm_is_svm())
				goto default_leaf;

			/*
			 * Similar to Intel, generate a fictitious cache
			 * topology for the guest with L3 shared by the
			 * package, and L1 and L2 local to a core.
			 */
			vm_get_topology(vm, &sockets, &cores, &threads,
			    &maxcpus);
			switch (param) {
			case 0:
				logical_cpus = threads;
				level = 1;
				func = 1;	/* data cache */
				break;
			case 1:
				logical_cpus = threads;
				level = 2;
				func = 3;	/* unified cache */
				break;
			case 2:
				logical_cpus = threads * cores;
				level = 3;
				func = 3;	/* unified cache */
				break;
			default:
				logical_cpus = sockets * threads * cores;
				level = 0;
				func = 0;
				break;
			}

			if (level == 0) {
				regs[0] = 0;
				regs[1] = 0;
			} else {
				logical_cpus = MIN(0xfff, logical_cpus - 1);
				regs[0] = (logical_cpus << 14) | (1 << 8) |
				    (level << 5) | func;
				regs[1] = func > 0 ? CACHE_LINE_SIZE - 1 : 0;
			}

			/*
			 * ecx: Number of cache ways for non-fully
			 * associative cache, minus 1.  Reported value
			 * of zero means there is one way.
			 */
			regs[2] = 0;

			regs[3] = 0;
			break;

		case CPUID_8000_001E:
			/*
			 * AMD Family 16h+ and Hygon Family 18h additional
			 * identifiers.
			 */
			if (!vmm_is_svm() || CPUID_TO_FAMILY(cpu_id) < 0x16)
				goto default_leaf;

			vm_get_topology(vm, &sockets, &cores, &threads,
			    &maxcpus);
			regs[0] = vcpu_id;
			threads = MIN(0xFF, threads - 1);
			regs[1] = (threads << 8) |
			    (vcpu_id >> log2(threads + 1));
			/*
			 * XXX Bhyve topology cannot yet represent >1 node per
			 * processor.
			 */
			regs[2] = 0;
			regs[3] = 0;
			break;

		case CPUID_8000_0022:
			/*
			 * AMD uses this leaf to advertise performance
			 * counters and PerfMonV2. vmm implements neither.
			 */
			regs[0] = 0;
			regs[1] = 0;
			regs[2] = 0;
			regs[3] = 0;
			break;

		case CPUID_0000_0001:
			do_cpuid(1, regs);

			error = vm_get_x2apic_state(vcpu, &x2apic_state);
			if (error) {
				panic("x86_emulate_cpuid: error %d "
				      "fetching x2apic state", error);
			}

			/*
			 * Override the APIC ID only in ebx
			 */
			regs[1] &= ~(CPUID_LOCAL_APIC_ID);
			regs[1] |= (vcpu_id << CPUID_0000_0001_APICID_SHIFT);

			/*
			 * Don't expose VMX, SpeedStep, TME or SMX capability.
			 * Advertise x2APIC capability and Hypervisor guest.
			 */
			regs[2] &= ~(CPUID2_VMX | CPUID2_EST | CPUID2_TM2);
			regs[2] &= ~(CPUID2_SMX);

			regs[2] |= CPUID2_HV;

			if (x2apic_state != X2APIC_DISABLED)
				regs[2] |= CPUID2_X2APIC;
			else
				regs[2] &= ~CPUID2_X2APIC;

			/*
			 * Only advertise CPUID2_XSAVE in the guest if
			 * the host is using XSAVE.
			 */
			if (!(regs[2] & CPUID2_OSXSAVE))
				regs[2] &= ~CPUID2_XSAVE;

			/*
			 * Hide monitor/mwait until we know how to deal with
			 * these instructions.
			 */
			regs[2] &= ~CPUID2_MON;

			/*
			 * Hide the performance and debug features.
			 */
			regs[2] &= ~CPUID2_PDCM;

			/*
			 * No TSC deadline support in the APIC yet
			 */
			regs[2] &= ~CPUID2_TSCDLT;

			/*
			 * Hide thermal monitoring
			 */
			regs[3] &= ~(CPUID_ACPI | CPUID_TM);

			/*
			 * Hide the debug store capability.
			 */
			regs[3] &= ~CPUID_DS;

			/*
			 * Advertise the Machine Check and MTRR capability.
			 *
			 * Some guest OSes (e.g. Windows) will not boot if
			 * these features are absent.
			 */
			regs[3] |= (CPUID_MCA | CPUID_MCE | CPUID_MTRR);

			vm_get_topology(vm, &sockets, &cores, &threads,
			    &maxcpus);
			logical_cpus = threads * cores;
			regs[1] &= ~CPUID_HTT_CORES;
			regs[1] |= (logical_cpus & 0xff) << 16;
			regs[3] |= CPUID_HTT;
			break;

		case CPUID_0000_0004:
			cpuid_count(func, param, regs);

			if (regs[0] || regs[1] || regs[2] || regs[3]) {
				vm_get_topology(vm, &sockets, &cores, &threads,
				    &maxcpus);
				regs[0] &= 0x3ff;
				regs[0] |= (cores - 1) << 26;
				/*
				 * Cache topology:
				 * - L1 and L2 are shared only by the logical
				 *   processors in a single core.
				 * - L3 and above are shared by all logical
				 *   processors in the package.
				 */
				logical_cpus = threads;
				level = (regs[0] >> 5) & 0x7;
				if (level >= 3)
					logical_cpus *= cores;
				regs[0] |= (logical_cpus - 1) << 14;
			}
			break;

		case CPUID_0000_0007:
			regs[0] = 0;
			regs[1] = 0;
			regs[2] = 0;
			regs[3] = 0;

			/* leaf 0 */
			if (param == 0) {
				cpuid_count(func, param, regs);

				/* Only leaf 0 is supported */
				regs[0] = 0;

				/*
				 * Expose known-safe features.
				 */
				regs[1] &= CPUID_STDEXT_FSGSBASE |
				    CPUID_STDEXT_BMI1 | CPUID_STDEXT_HLE |
				    CPUID_STDEXT_AVX2 | CPUID_STDEXT_SMEP |
				    CPUID_STDEXT_BMI2 |
				    CPUID_STDEXT_ERMS | CPUID_STDEXT_RTM |
				    CPUID_STDEXT_AVX512F |
				    CPUID_STDEXT_AVX512DQ |
				    CPUID_STDEXT_RDSEED |
				    CPUID_STDEXT_SMAP |
				    CPUID_STDEXT_AVX512PF |
				    CPUID_STDEXT_AVX512ER |
				    CPUID_STDEXT_AVX512CD | CPUID_STDEXT_SHA |
				    CPUID_STDEXT_AVX512BW |
				    CPUID_STDEXT_AVX512VL |
				    CPUID_STDEXT_AVX512IFMA;
				regs[2] &= CPUID_STDEXT2_VAES |
				    CPUID_STDEXT2_VPCLMULQDQ |
				    CPUID_STDEXT2_AVX512VBMI |
				    CPUID_STDEXT2_AVX512VBMI2 |
				    CPUID_STDEXT2_AVX512VNNI |
				    CPUID_STDEXT2_AVX512BITALG |
				    CPUID_STDEXT2_AVX512VPOPCNTDQ;
				regs[3] &= CPUID_STDEXT3_MD_CLEAR |
				    CPUID_STDEXT3_AVX5124VNNIW |
				    CPUID_STDEXT3_AVX5124FMAPS |
				    CPUID_STDEXT3_AVX512VP2INTERSECT;

				/* Advertise RDPID if it is enabled. */
				error = vm_get_capability(vcpu, VM_CAP_RDPID,
				    &enable_rdpid);
				if (error == 0 && enable_rdpid)
					regs[2] |= CPUID_STDEXT2_RDPID;

				/* Advertise INVPCID if it is enabled. */
				error = vm_get_capability(vcpu,
				    VM_CAP_ENABLE_INVPCID, &enable_invpcid);
				if (error == 0 && enable_invpcid)
					regs[1] |= CPUID_STDEXT_INVPCID;
			}
			break;

		case CPUID_0000_0006:
			regs[0] = CPUTPM1_ARAT;
			regs[1] = 0;
			regs[2] = 0;
			regs[3] = 0;
			break;

		case CPUID_0000_000A:
			/*
			 * Handle the access, but report 0 for
			 * all options
			 */
			regs[0] = 0;
			regs[1] = 0;
			regs[2] = 0;
			regs[3] = 0;
			break;

		case CPUID_0000_000B:
		case CPUID_0000_001F:
			/*
			 * Intel processor topology enumeration.  Leaf 1Fh
			 * is the V2 form of leaf 0Bh and uses the same
			 * encoding for the SMT and core levels.
			 */
			if (vmm_is_intel()) {
				vm_get_topology(vm, &sockets, &cores, &threads,
				    &maxcpus);
				if (param == 0) {
					logical_cpus = threads;
					width = log2(logical_cpus);
					level = CPUID_TYPE_SMT;
					x2apic_id = vcpu_id;
				}

				if (param == 1) {
					logical_cpus = threads * cores;
					width = log2(logical_cpus);
					level = CPUID_TYPE_CORE;
					x2apic_id = vcpu_id;
				}

				if (!cpuid_leaf_b || param >= 2) {
					width = 0;
					logical_cpus = 0;
					level = 0;
					x2apic_id = 0;
				}

				regs[0] = width & 0x1f;
				regs[1] = logical_cpus & 0xffff;
				regs[2] = (level << 8) | (param & 0xff);
				regs[3] = x2apic_id;
			} else {
				regs[0] = 0;
				regs[1] = 0;
				regs[2] = 0;
				regs[3] = 0;
			}
			break;

		case CPUID_0000_000D:
			limits = vmm_get_xsave_limits();
			if (!limits->xsave_enabled) {
				regs[0] = 0;
				regs[1] = 0;
				regs[2] = 0;
				regs[3] = 0;
				break;
			}

			cpuid_count(func, param, regs);
			switch (param) {
			case 0:
				/*
				 * Only permit the guest to use bits
				 * that are active in the host in
				 * %xcr0.  Also, claim that the
				 * maximum save area size is
				 * equivalent to the host's current
				 * save area size.  Since this runs
				 * "inside" of vmrun(), it runs with
				 * the guest's xcr0, so the current
				 * save area size is correct as-is.
				 */
				regs[0] &= limits->xcr0_allowed;
				regs[2] = limits->xsave_max_size;
				regs[3] &= (limits->xcr0_allowed >> 32);
				break;
			case 1:
				/* Only permit XSAVEOPT. */
				regs[0] &= CPUID_EXTSTATE_XSAVEOPT;
				regs[1] = 0;
				regs[2] = 0;
				regs[3] = 0;
				break;
			default:
				/*
				 * If the leaf is for a permitted feature,
				 * pass through as-is, otherwise return
				 * all zeroes.
				 */
				if (!(limits->xcr0_allowed & (1ul << param))) {
					regs[0] = 0;
					regs[1] = 0;
					regs[2] = 0;
					regs[3] = 0;
				}
				break;
			}
			break;

		case CPUID_0000_000F:
		case CPUID_0000_0010:
			/*
			 * Do not report any Resource Director Technology
			 * capabilities.  Exposing control of cache or memory
			 * controller resource partitioning to the guest is not
			 * at all sensible.
			 *
			 * This is already hidden at a high level by masking of
			 * leaf 0x7.  Even still, a guest may look here for
			 * detailed capability information.
			 */
			regs[0] = 0;
			regs[1] = 0;
			regs[2] = 0;
			regs[3] = 0;
			break;

		case CPUID_0000_0015:
			/*
			 * Don't report CPU TSC/Crystal ratio and clock
			 * values since guests may use these to derive the
			 * local APIC frequency..
			 */
			regs[0] = 0;
			regs[1] = 0;
			regs[2] = 0;
			regs[3] = 0;
			break;

		case CPUID_VM_SIGNATURE:
		case CPUID_VM_SIGNATURE + CPUID_VM_BHYVE_OFF:
			regs[0] = CPUID_VM_HIGH + (func - CPUID_VM_SIGNATURE);
			bcopy(bhyve_id, &regs[1], 4);
			bcopy(bhyve_id + 4, &regs[2], 4);
			bcopy(bhyve_id + 8, &regs[3], 4);
			break;

		case CPUID_BHYVE_FEATURES:
		case CPUID_BHYVE_FEATURES + CPUID_VM_BHYVE_OFF:
			regs[0] = CPUID_BHYVE_FEAT_EXT_DEST_ID;
			regs[1] = 0;
			regs[2] = 0;
			regs[3] = 0;
			break;

		default:
default_leaf:
			/*
			 * The leaf value has already been clamped so
			 * simply pass this through, keeping count of
			 * how many unhandled leaf values have been seen.
			 */
			atomic_add_long(&bhyve_xcpuids, 1);
			cpuid_count(func, param, regs);
			break;
	}

	*eax = regs[0];
	*ebx = regs[1];
	*ecx = regs[2];
	*edx = regs[3];
}
