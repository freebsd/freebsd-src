/*-
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software was developed by Andrew Turner under sponsorship from
 * the FreeBSD Foundation.
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

/*
 * This manages pointer authentication. As it needs to enable the use of
 * pointer authentication and change the keys we must built this with
 * pointer authentication disabled.
 */
#ifdef __ARM_FEATURE_PAC_DEFAULT
#error Must be built with pointer authentication disabled
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/proc.h>
#include <sys/reboot.h>

#include <machine/armreg.h>
#include <machine/cpu.h>
#include <machine/cpu_feat.h>
#include <machine/reg.h>
#include <machine/vmparam.h>

#define	SCTLR_PTRAUTH	(SCTLR_EnIA | SCTLR_EnIB | SCTLR_EnDA | SCTLR_EnDB)

static bool __read_mostly enable_ptrauth = false;

/* Functions called from assembly. */
void ptrauth_start(void);
struct thread *ptrauth_switch(struct thread *);
void ptrauth_exit_el0(struct thread *);
void ptrauth_enter_el0(struct thread *);

static bool
ptrauth_disable(void)
{
	const char *family, *maker, *product;

	family = kern_getenv("smbios.system.family");
	maker = kern_getenv("smbios.system.maker");
	product = kern_getenv("smbios.system.product");
	if (family == NULL || maker == NULL || product == NULL)
		return (false);

	/*
	 * The Dev Kit appears to be configured to trap upon access to PAC
	 * registers, but the kernel boots at EL1 and so we have no way to
	 * inspect or change this configuration.  As a workaround, simply
	 * disable PAC on this platform.
	 */
	if (strcmp(maker, "Microsoft Corporation") == 0 &&
	    strcmp(family, "Surface") == 0 &&
	    strcmp(product, "Windows Dev Kit 2023") == 0)
		return (true);

	return (false);
}

static cpu_feat_en
ptrauth_check(const struct cpu_feat *feat __unused, u_int midr __unused)
{
	uint64_t isar;
	int pac_enable;

	/*
	 * Allow the sysadmin to disable pointer authentication globally,
	 * e.g. on broken hardware.
	 */
	pac_enable = 1;
	TUNABLE_INT_FETCH("hw.pac.enable", &pac_enable);
	if (!pac_enable) {
		if (boothowto & RB_VERBOSE)
			printf("Pointer authentication is disabled\n");
		return (FEAT_ALWAYS_DISABLE);
	}

	if (ptrauth_disable())
		return (FEAT_ALWAYS_DISABLE);

	/*
	 * This assumes if there is pointer authentication on the boot CPU
	 * it will also be available on any non-boot CPUs. If this is ever
	 * not the case we will have to add a quirk.
	 */

	/*
	 * The QARMA5 or implementation defined algorithms are reported in
	 * ID_AA64ISAR1_EL1.
	 */
	if (get_kernel_reg(ID_AA64ISAR1_EL1, &isar)) {
		if (ID_AA64ISAR1_APA_VAL(isar) > 0 ||
		    ID_AA64ISAR1_API_VAL(isar) > 0) {
			return (FEAT_DEFAULT_ENABLE);
		}
	}

	/* The QARMA3 algorithm is reported in ID_AA64ISAR2_EL1. */
	if (get_kernel_reg(ID_AA64ISAR2_EL1, &isar)) {
		if (ID_AA64ISAR2_APA3_VAL(isar) > 0) {
			return (FEAT_DEFAULT_ENABLE);
		}
	}

	return (FEAT_ALWAYS_DISABLE);
}

static bool
ptrauth_enable(const struct cpu_feat *feat __unused,
    cpu_feat_errata errata_status __unused, u_int *errata_list __unused,
    u_int errata_count __unused)
{
	enable_ptrauth = true;
	elf64_addr_mask.code |= PAC_ADDR_MASK;
	elf64_addr_mask.data |= PAC_ADDR_MASK;
#ifdef COMPAT_FREEBSD14
	elf64_addr_mask_14.code |= PAC_ADDR_MASK_14;
	elf64_addr_mask_14.data |= PAC_ADDR_MASK_14;
#endif

	return (true);
}

static void
ptrauth_disabled(const struct cpu_feat *feat __unused)
{
	/*
	 * Pointer authentication may be disabled, mask out the ID fields we
	 * expose to userspace and the rest of the kernel so they don't try
	 * to use it.
	 */
	if (PCPU_GET(cpuid) == 0) {
		update_special_reg(ID_AA64ISAR1_EL1, ID_AA64ISAR1_API_MASK |
		    ID_AA64ISAR1_APA_MASK | ID_AA64ISAR1_GPA_MASK |
		    ID_AA64ISAR1_GPI_MASK, 0);
		update_special_reg(ID_AA64ISAR2_EL1, ID_AA64ISAR2_APA3_MASK, 0);
	}

}

CPU_FEAT(feat_pauth, "Pointer Authentication",
    ptrauth_check, NULL, ptrauth_enable, ptrauth_disabled,
    CPU_FEAT_EARLY_BOOT | CPU_FEAT_SYSTEM);

/* Copy the keys when forking a new process */
void
ptrauth_fork(struct thread *new_td, struct thread *orig_td)
{
	if (!enable_ptrauth)
		return;

	memcpy(&new_td->td_md.md_ptrauth_user, &orig_td->td_md.md_ptrauth_user,
	    sizeof(new_td->td_md.md_ptrauth_user));
}

/* Generate new userspace keys when executing a new process */
void
ptrauth_exec(struct thread *td)
{
	if (!enable_ptrauth)
		return;

	arc4rand(&td->td_md.md_ptrauth_user, sizeof(td->td_md.md_ptrauth_user),
	    0);
}

/*
 * Copy the user keys when creating a new userspace thread until it's clear
 * how the ABI expects the various keys to be assigned.
 */
void
ptrauth_copy_thread(struct thread *new_td, struct thread *orig_td)
{
	if (!enable_ptrauth)
		return;

	memcpy(&new_td->td_md.md_ptrauth_user, &orig_td->td_md.md_ptrauth_user,
	    sizeof(new_td->td_md.md_ptrauth_user));
}

/* Generate new kernel keys when executing a new kernel thread */
void
ptrauth_thread_alloc(struct thread *td)
{
	if (!enable_ptrauth)
		return;

	arc4rand(&td->td_md.md_ptrauth_kern, sizeof(td->td_md.md_ptrauth_kern),
	    0);
}

/*
 * Load the userspace keys. We can't use WRITE_SPECIALREG as we need
 * to set the architecture extension.
 */
#define	LOAD_KEY(space, name, reg)					\
__asm __volatile(							\
	"msr	"__XSTRING(MRS_REG_ALT_NAME(reg ## KeyLo_EL1))", %0	\n"	\
	"msr	"__XSTRING(MRS_REG_ALT_NAME(reg ## KeyHi_EL1))", %1	\n"	\
	:: "r"(td->td_md.md_ptrauth_##space.name.pa_key_lo),		\
	   "r"(td->td_md.md_ptrauth_##space.name.pa_key_hi))

void
ptrauth_thread0(struct thread *td)
{
	if (!enable_ptrauth)
		return;

	/* TODO: Generate a random number here */
	memset(&td->td_md.md_ptrauth_kern, 0,
	    sizeof(td->td_md.md_ptrauth_kern));
	LOAD_KEY(kern, apia, APIA);
	/*
	 * No isb as this is called before ptrauth_start so can rely on
	 * the instruction barrier there.
	 */
}

/*
 * Enable pointer authentication. After this point userspace and the kernel
 * can sign return addresses, etc. based on their keys
 *
 * This assumes either all or no CPUs have pointer authentication support,
 * and, if supported, all CPUs have the same algorithm.
 */
void
ptrauth_start(void)
{
	uint64_t sctlr;

	if (!enable_ptrauth)
		return;

	/* Enable pointer authentication */
	sctlr = READ_SPECIALREG(sctlr_el1);
	sctlr |= SCTLR_PTRAUTH;
	WRITE_SPECIALREG(sctlr_el1, sctlr);
	isb();
}

#ifdef SMP
void
ptrauth_mp_start(uint64_t cpu)
{
	struct ptrauth_key start_key;
	uint64_t sctlr;

	if (!enable_ptrauth)
		return;

	/*
	 * We need a key until we call sched_throw, however we don't have
	 * a thread until then. Create a key just for use within
	 * init_secondary and whatever it calls. As init_secondary never
	 * returns it is safe to do so from within it.
	 *
	 * As it's only used for a short length of time just use the cpu
	 * as the key.
	 */
	start_key.pa_key_lo = cpu;
	start_key.pa_key_hi = ~cpu;

	__asm __volatile(
	    ".arch_extension pauth		\n"
	    "msr	"__XSTRING(APIAKeyLo_EL1_REG)", %0	\n"
	    "msr	"__XSTRING(APIAKeyHi_EL1_REG)", %1	\n"
	    ".arch_extension nopauth		\n"
	    :: "r"(start_key.pa_key_lo), "r"(start_key.pa_key_hi));

	/* Enable pointer authentication */
	sctlr = READ_SPECIALREG(sctlr_el1);
	sctlr |= SCTLR_PTRAUTH;
	WRITE_SPECIALREG(sctlr_el1, sctlr);
	isb();
}
#endif

struct thread *
ptrauth_switch(struct thread *td)
{
	if (enable_ptrauth) {
		LOAD_KEY(kern, apia, APIA);
		isb();
	}

	return (td);
}

/* Called when we are exiting uerspace and entering the kernel */
void
ptrauth_exit_el0(struct thread *td)
{
	if (!enable_ptrauth)
		return;

	LOAD_KEY(kern, apia, APIA);
	isb();
}

/* Called when we are about to exit the kernel and enter userspace */
void
ptrauth_enter_el0(struct thread *td)
{
	if (!enable_ptrauth)
		return;

	LOAD_KEY(user, apia, APIA);
	LOAD_KEY(user, apib, APIB);
	LOAD_KEY(user, apda, APDA);
	LOAD_KEY(user, apdb, APDB);
	LOAD_KEY(user, apga, APGA);
	/*
	 * No isb as this is called from the exception handler so can rely
	 * on the eret instruction to be the needed context synchronizing event.
	 */
}
