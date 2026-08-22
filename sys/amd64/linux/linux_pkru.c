/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 */

/*
 * x86 memory protection keys (PKU) for the Linuxulator, serving both
 * the 64-bit and 32-bit Linux ABIs.
 *
 * The PKRU register is directly user-visible: Linux programs read and
 * write it with RDPKRU/WRPKRU, which execute natively.  The kernel's
 * part is key allocation bookkeeping (per address space: inherited on
 * fork, reset on exec, as with Linux mm->context.pkey_allocation_map),
 * tagging pages (pkey_mprotect), and applying the initial access
 * rights of pkey_alloc() to the calling thread's PKRU.
 */

#include <sys/systm.h>
#include <sys/imgact.h>
#include <sys/lock.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sx.h>

#include <machine/cpufunc.h>
#include <machine/fpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <x86/x86_var.h>

#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_mmap.h>

static bool
linux_pkey_supported(void)
{

	return ((cpu_stdext_feature2 & CPUID_STDEXT2_OSPKE) != 0);
}

/*
 * Update the calling thread's PKRU: new value is (PKRU & keep) | set.
 */
static void
linux_pkru_write(struct thread *td, uint32_t keep, uint32_t set)
{
	struct pcb *pcb;
	struct xstate_hdr *hdr;
	char *sa;
	uint32_t *pkru;

	MPASS(td == curthread);
	pcb = td->td_pcb;

	/*
	 * The critical section is held across the save area update to
	 * exclude preemption: a context switch could otherwise load the
	 * xsave area back into the CPU after fpugetregs(), and the
	 * stores below would then be lost to the next save.
	 */
	critical_enter();
	if ((pcb->pcb_flags & PCB_USERFPUINITDONE) != 0 &&
	    td == PCPU_GET(fpcurthread) && PCB_USER_FPU(pcb)) {
		wrpkru((rdpkru() & keep) | set);
		critical_exit();
		return;
	}

	/*
	 * The user FPU state is in the PCB save area, or is not yet
	 * initialized, in which case fpugetregs() installs the initial
	 * state there.
	 */
	(void)fpugetregs(td);
	sa = (char *)get_pcb_user_save_td(td);
	hdr = (struct xstate_hdr *)(sa + xsave_area_hdr_offset());
	pkru = (uint32_t *)(sa + xsave_area_offset(xsave_mask,
	    XFEATURE_ENABLED_PKRU, false, false));
	if ((hdr->xstate_bv & XFEATURE_ENABLED_PKRU) == 0) {
		hdr->xstate_bv |= XFEATURE_ENABLED_PKRU;
		*pkru = 0;
	}
	*pkru = (*pkru & keep) | set;
	critical_exit();
}

/*
 * Set the calling thread's PKRU access rights for the given key.
 */
static void
linux_pkru_set_perm(struct thread *td, u_int keyidx, uint32_t rights)
{

	linux_pkru_write(td, ~(LINUX_PKEY_ACCESS_MASK << (keyidx * 2)),
	    rights << (keyidx * 2));
}

/*
 * Called from the Linux sysvecs' exec_setregs.  Linux initializes
 * PKRU at exec to deny access to all keys but key 0
 * (arch/x86/mm/pkeys.c init_pkru_value), so memory tagged with a not
 * yet allocated key is inaccessible; FreeBSD's initial PKRU is 0.
 * This initializes the user FPU state slightly earlier than the lazy
 * first-use path; the state would be initialized moments later in
 * rtld/libc startup regardless.
 */
void
linux_pkru_exec_init(struct thread *td)
{

	if (!linux_pkey_supported())
		return;
	linux_pkru_write(td, 0, LINUX_PKRU_INIT);
}

/*
 * Protection keys are a property of the address space: inherit the
 * allocation map on fork, as Linux does.  When a FreeBSD process is
 * switching to the Linux ABI there is no parent emuldata; start from
 * the initial map.  The unlocked read is atomic on the aligned word;
 * a pkey_alloc() racing the fork in another thread yields a valid
 * serialization either way.
 */
void
linux_pemuldata_init_md(struct thread *td, struct linux_pemuldata *pem)
{
	struct linux_pemuldata *ppem;

	ppem = pem_find(td->td_proc);
	if (ppem != NULL)
		pem->pem_md.md_pkey_allocation_map =
		    ppem->pem_md.md_pkey_allocation_map;
	else
		pem->pem_md.md_pkey_allocation_map = LINUX_PKEY_INITIAL_MAP;
}

void
linux_pemuldata_exec_md(struct linux_pemuldata *pem)
{

	pem->pem_md.md_pkey_allocation_map = LINUX_PKEY_INITIAL_MAP;
}

int
linux_pkey_alloc_machdep(struct thread *td, uint64_t init_val)
{
	struct linux_pemuldata *pem;
	uint32_t free_keys;
	int key;

	if (!linux_pkey_supported())
		return (ENOSPC);

	pem = pem_find(td->td_proc);
	LINUX_PEM_XLOCK(pem);
	free_keys = ~pem->pem_md.md_pkey_allocation_map &
	    ((1u << LINUX_PKEY_MAX) - 1) & ~LINUX_PKEY_INITIAL_MAP;
	if (free_keys == 0) {
		LINUX_PEM_XUNLOCK(pem);
		return (ENOSPC);
	}
	key = ffs(free_keys) - 1;
	pem->pem_md.md_pkey_allocation_map |= 1u << key;
	LINUX_PEM_XUNLOCK(pem);

	linux_pkru_set_perm(td, key, init_val);
	td->td_retval[0] = key;
	return (0);
}

int
linux_pkey_free_machdep(struct thread *td, int pkey)
{
	struct linux_pemuldata *pem;

	if (!linux_pkey_supported())
		return (EINVAL);

	pem = pem_find(td->td_proc);
	LINUX_PEM_XLOCK(pem);
	if ((pem->pem_md.md_pkey_allocation_map & (1u << pkey)) == 0) {
		LINUX_PEM_XUNLOCK(pem);
		return (EINVAL);
	}
	pem->pem_md.md_pkey_allocation_map &= ~(1u << pkey);
	LINUX_PEM_XUNLOCK(pem);

	/*
	 * As on Linux, freeing a key neither untags pages nor updates
	 * PKRU; that is the application's responsibility.
	 */
	return (0);
}

int
linux_pkey_mprotect_machdep(struct thread *td, uintptr_t addr, size_t len,
    int prot, int pkey)
{
	struct linux_pemuldata *pem;
	int error;

	if (!linux_pkey_supported())
		return (EINVAL);

	pem = pem_find(td->td_proc);
	LINUX_PEM_SLOCK(pem);
	if ((pem->pem_md.md_pkey_allocation_map & (1u << pkey)) == 0) {
		LINUX_PEM_SUNLOCK(pem);
		return (EINVAL);
	}
	LINUX_PEM_SUNLOCK(pem);

	error = linux_mprotect_common(td, addr, len, prot);
	if (error != 0 || len == 0)
		return (error);

	/*
	 * Tag the range; a pkey of 0 untags it.  The tag is not
	 * persistent: it dies with the mapping, matching Linux VMA
	 * semantics.
	 */
	return (amd64_pkru_update(td, addr, len, pkey, 0, pkey == 0));
}
