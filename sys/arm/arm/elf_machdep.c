/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 1996-1998 John D. Polstra.
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
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/linker.h>
#include <sys/reg.h>
#include <sys/sysent.h>
#include <sys/imgact_elf.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/stack.h>
#ifdef VFP
#include <machine/vfp.h>
#endif

#include "opt_ddb.h"            /* for OPT_DDB */
#include "opt_global.h"         /* for OPT_KDTRACE_HOOKS */
#include "opt_stack.h"          /* for OPT_STACK */

static bool elf32_arm_abi_supported(const struct image_params *,
    const int32_t *, const uint32_t *);

u_long elf_hwcap;
u_long elf_hwcap2;

struct sysentvec elf32_freebsd_sysvec = {
	.sv_size	= SYS_MAXSYSCALL,
	.sv_table	= sysent,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= sendsig,
	.sv_sigcode	= sigcode,
	.sv_szsigcode	= &szsigcode,
	.sv_name	= "FreeBSD ELF32",
	.sv_coredump	= __elfN(coredump),
	.sv_elf_core_osabi = ELFOSABI_FREEBSD,
	.sv_elf_core_abi_vendor = FREEBSD_ABI_VENDOR,
	.sv_elf_core_prepare_notes = __elfN(prepare_notes),
	.sv_minsigstksz	= MINSIGSTKSZ,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= USRSTACK,
	.sv_psstrings	= PS_STRINGS,
	.sv_psstringssz	= sizeof(struct ps_strings),
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_auxargs = __elfN(freebsd_copyout_auxargs),
	.sv_copyout_strings = exec_copyout_strings,
	.sv_setregs	= exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	=
			  SV_ASLR | SV_SHP | SV_TIMEKEEP | SV_RNG_SEED_VER |
			  SV_ABI_FREEBSD | SV_ILP32 | SV_SIGSYS,
	.sv_set_syscall_retval = cpu_set_syscall_retval,
	.sv_fetch_syscall_args = cpu_fetch_syscall_args,
	.sv_syscallnames = syscallnames,
	.sv_shared_page_base = SHAREDPAGE,
	.sv_shared_page_len = PAGE_SIZE,
	.sv_schedtail	= NULL,
	.sv_thread_detach = NULL,
	.sv_trap	= NULL,
	.sv_hwcap	= &elf_hwcap,
	.sv_hwcap2	= &elf_hwcap2,
	.sv_hwcap3	= NULL,
	.sv_hwcap4	= NULL,
	.sv_onexec_old	= exec_onexec_old,
	.sv_onexit	= exit_onexit,
	.sv_regset_begin = SET_BEGIN(__elfN(regset)),
	.sv_regset_end  = SET_LIMIT(__elfN(regset)),
};
INIT_SYSENTVEC(elf32_sysvec, &elf32_freebsd_sysvec);

static Elf32_Brandinfo freebsd_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_ARM,
	.compat_3_brand	= "FreeBSD",
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf32_freebsd_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &elf32_freebsd_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE,
	.header_supported= elf32_arm_abi_supported,
};

SYSINIT(elf32, SI_SUB_EXEC, SI_ORDER_FIRST,
	(sysinit_cfunc_t) elf32_insert_brand_entry,
	&freebsd_brand_info);

static bool
elf32_arm_abi_supported(const struct image_params *imgp,
    const int32_t *osrel __unused, const uint32_t *fctl0 __unused)
{
	const Elf_Ehdr *hdr = (const Elf_Ehdr *)imgp->image_header;

	/*
	 * When configured for EABI, FreeBSD supports EABI vesions 4 and 5.
	 */
	if (EF_ARM_EABI_VERSION(hdr->e_flags) < EF_ARM_EABI_FREEBSD_MIN) {
		if (bootverbose)
			uprintf("Attempting to execute non EABI binary (rev %d) image %s",
			    EF_ARM_EABI_VERSION(hdr->e_flags), imgp->args->fname);
		return (false);
	}
	return (true);
}

void
elf32_dump_thread(struct thread *td __unused, void *dst __unused,
    size_t *off __unused)
{
}

bool
elf_is_ifunc_reloc(Elf_Size r_info __unused)
{

	return (false);
}

/*
 * It is possible for the compiler to emit relocations for unaligned data.
 * We handle this situation with these inlines.
 */
#define	RELOC_ALIGNED_P(x) \
	(((uintptr_t)(x) & (sizeof(void *) - 1)) == 0)

static __inline Elf_Addr
load_ptr(Elf_Addr *where)
{
	Elf_Addr res;

	if (RELOC_ALIGNED_P(where))
		return *where;
	memcpy(&res, where, sizeof(res));
	return (res);
}

static __inline void
store_ptr(Elf_Addr *where, Elf_Addr val)
{
	if (RELOC_ALIGNED_P(where))
		*where = val;
	else
		memcpy(where, &val, sizeof(val));
}
#undef RELOC_ALIGNED_P

/* Process one elf relocation with addend. */
static int
elf_reloc_internal(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, int local, elf_lookup_fn lookup)
{
	Elf_Addr *where;
	Elf_Addr addr;
	Elf_Addr addend;
	Elf_Word rtype, symidx;
	const Elf_Rel *rel;
	const Elf_Rela *rela;
	int error;

	switch (type) {
	case ELF_RELOC_REL:
		rel = (const Elf_Rel *)data;
		where = (Elf_Addr *) (relocbase + rel->r_offset);
		addend = load_ptr(where);
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);
		break;
	case ELF_RELOC_RELA:
		rela = (const Elf_Rela *)data;
		where = (Elf_Addr *) (relocbase + rela->r_offset);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
		break;
	default:
		panic("unknown reloc type %d\n", type);
	}

	if (local) {
		if (rtype == R_ARM_RELATIVE) {	/* A + B */
			addr = elf_relocaddr(lf, relocbase + addend);
			if (load_ptr(where) != addr)
				store_ptr(where, addr);
		}
		return (0);
	}

	switch (rtype) {
		case R_ARM_NONE:	/* none */
			break;

		case R_ARM_ABS32:
			error = lookup(lf, symidx, 1, &addr);
			if (error != 0)
				return (-1);
			store_ptr(where, addr + load_ptr(where));
			break;

		case R_ARM_COPY:	/* none */
			/*
			 * There shouldn't be copy relocations in kernel
			 * objects.
			 */
			printf("kldload: unexpected R_COPY relocation, "
			    "symbol index %d\n", symidx);
			return (-1);
			break;

		case R_ARM_JUMP_SLOT:
			error = lookup(lf, symidx, 1, &addr);
			if (error == 0) {
				store_ptr(where, addr);
				return (0);
			}
			return (-1);
		case R_ARM_RELATIVE:
			break;

		default:
			printf("kldload: unexpected relocation type %d, "
			    "symbol index %d\n", rtype, symidx);
			return (-1);
	}
	return(0);
}

int
elf_reloc(linker_file_t lf, Elf_Addr relocbase, const void *data, int type,
    elf_lookup_fn lookup)
{

	return (elf_reloc_internal(lf, relocbase, data, type, 0, lookup));
}

int
elf_reloc_local(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, elf_lookup_fn lookup)
{

	return (elf_reloc_internal(lf, relocbase, data, type, 1, lookup));
}

int
elf_cpu_load_file(linker_file_t lf)
{

	/*
	 * The pmap code does not do an icache sync upon establishing executable
	 * mappings in the kernel pmap.  It's an optimization based on the fact
	 * that kernel memory allocations always have EXECUTABLE protection even
	 * when the memory isn't going to hold executable code.  The only time
	 * kernel memory holding instructions does need a sync is after loading
	 * a kernel module, and that's when this function gets called.
	 *
	 * This syncs data and instruction caches after loading a module.  We
	 * don't worry about the kernel itself (lf->id is 1) as locore.S did
	 * that on entry.  Even if data cache maintenance was done by IO code,
	 * the relocation fixup process creates dirty cache entries that we must
	 * write back before doing icache sync. The instruction cache sync also
	 * invalidates the branch predictor cache on platforms that have one.
	 */
	if (lf->id == 1)
		return (0);
	dcache_wb_pou((vm_offset_t)lf->address, (vm_size_t)lf->size);
	icache_inv_all();

#if defined(DDB) || defined(KDTRACE_HOOKS) || defined(STACK)
	/*
	 * Inform the stack(9) code of the new module, so it can acquire its
	 * per-module unwind data.
	 */
	unwind_module_loaded(lf);
#endif

	return (0);
}

int
elf_cpu_parse_dynamic(caddr_t loadbase __unused, Elf_Dyn *dynamic __unused)
{

	return (0);
}

int
elf_cpu_unload_file(linker_file_t lf)
{

#if defined(DDB) || defined(KDTRACE_HOOKS) || defined(STACK)
	/* Inform the stack(9) code that this module is gone. */
	unwind_module_unloaded(lf);
#endif
	return (0);
}
