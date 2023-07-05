/*-
 * Copyright (c) 2014, 2015 The FreeBSD Foundation.
 * Copyright (c) 2014 Andrew Turner.
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/linker.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/sysent.h>
#include <sys/imgact_elf.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <machine/elf.h>
#include <machine/md_var.h>

#include "linker_if.h"

u_long __read_frequently elf_hwcap;
u_long __read_frequently elf_hwcap2;

struct arm64_addr_mask elf64_addr_mask;

static struct sysentvec elf64_freebsd_sysvec = {
	.sv_size	= SYS_MAXSYSCALL,
	.sv_table	= sysent,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= sendsig,
	.sv_sigcode	= sigcode,
	.sv_szsigcode	= &szsigcode,
	.sv_name	= "FreeBSD ELF64",
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
	.sv_stackprot	= VM_PROT_READ | VM_PROT_WRITE,
	.sv_copyout_auxargs = __elfN(freebsd_copyout_auxargs),
	.sv_copyout_strings = exec_copyout_strings,
	.sv_setregs	= exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_SHP | SV_TIMEKEEP | SV_ABI_FREEBSD | SV_LP64 |
	    SV_ASLR | SV_RNG_SEED_VER,
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
	.sv_onexec_old	= exec_onexec_old,
	.sv_onexit	= exit_onexit,
	.sv_regset_begin = SET_BEGIN(__elfN(regset)),
	.sv_regset_end	= SET_LIMIT(__elfN(regset)),
};
INIT_SYSENTVEC(elf64_sysvec, &elf64_freebsd_sysvec);

static Elf64_Brandinfo freebsd_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_AARCH64,
	.compat_3_brand	= "FreeBSD",
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf64_freebsd_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &elf64_freebsd_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

SYSINIT(elf64, SI_SUB_EXEC, SI_ORDER_FIRST,
    (sysinit_cfunc_t)elf64_insert_brand_entry, &freebsd_brand_info);

static bool
get_arm64_addr_mask(struct regset *rs, struct thread *td, void *buf,
    size_t *sizep)
{
	if (buf != NULL) {
		KASSERT(*sizep == sizeof(elf64_addr_mask),
		    ("%s: invalid size", __func__));
		memcpy(buf, &elf64_addr_mask, sizeof(elf64_addr_mask));
	}
	*sizep = sizeof(elf64_addr_mask);

	return (true);
}

static struct regset regset_arm64_addr_mask = {
	.note = NT_ARM_ADDR_MASK,
	.size = sizeof(struct arm64_addr_mask),
	.get = get_arm64_addr_mask,
};
ELF_REGSET(regset_arm64_addr_mask);

void
elf64_dump_thread(struct thread *td __unused, void *dst __unused,
    size_t *off __unused)
{
}

bool
elf_is_ifunc_reloc(Elf_Size r_info __unused)
{

	return (ELF_R_TYPE(r_info) == R_AARCH64_IRELATIVE);
}

static int
reloc_instr_imm(Elf32_Addr *where, Elf_Addr val, u_int msb, u_int lsb)
{

	/* Check bounds: upper bits must be all ones or all zeros. */
	if ((uint64_t)((int64_t)val >> (msb + 1)) + 1 > 1)
		return (-1);
	val >>= lsb;
	val &= (1 << (msb - lsb + 1)) - 1;
	*where |= (Elf32_Addr)val;
	return (0);
}

/*
 * Process a relocation.  Support for some static relocations is required
 * in order for the -zifunc-noplt optimization to work.
 */
static int
elf_reloc_internal(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, int flags, elf_lookup_fn lookup)
{
#define	ARM64_ELF_RELOC_LOCAL		(1 << 0)
#define	ARM64_ELF_RELOC_LATE_IFUNC	(1 << 1)
	Elf_Addr *where, addr, addend, val;
	Elf_Word rtype, symidx;
	const Elf_Rel *rel;
	const Elf_Rela *rela;
	int error;

	switch (type) {
	case ELF_RELOC_REL:
		rel = (const Elf_Rel *)data;
		where = (Elf_Addr *) (relocbase + rel->r_offset);
		addend = *where;
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

	if ((flags & ARM64_ELF_RELOC_LATE_IFUNC) != 0) {
		KASSERT(type == ELF_RELOC_RELA,
		    ("Only RELA ifunc relocations are supported"));
		if (rtype != R_AARCH64_IRELATIVE)
			return (0);
	}

	if ((flags & ARM64_ELF_RELOC_LOCAL) != 0) {
		if (rtype == R_AARCH64_RELATIVE)
			*where = elf_relocaddr(lf, relocbase + addend);
		return (0);
	}

	error = 0;
	switch (rtype) {
	case R_AARCH64_NONE:
	case R_AARCH64_RELATIVE:
		break;
	case R_AARCH64_TSTBR14:
		error = lookup(lf, symidx, 1, &addr);
		if (error != 0)
			return (-1);
		error = reloc_instr_imm((Elf32_Addr *)where,
		    addr + addend - (Elf_Addr)where, 15, 2);
		break;
	case R_AARCH64_CONDBR19:
		error = lookup(lf, symidx, 1, &addr);
		if (error != 0)
			return (-1);
		error = reloc_instr_imm((Elf32_Addr *)where,
		    addr + addend - (Elf_Addr)where, 20, 2);
		break;
	case R_AARCH64_JUMP26:
	case R_AARCH64_CALL26:
		error = lookup(lf, symidx, 1, &addr);
		if (error != 0)
			return (-1);
		error = reloc_instr_imm((Elf32_Addr *)where,
		    addr + addend - (Elf_Addr)where, 27, 2);
		break;
	case R_AARCH64_ABS64:
	case R_AARCH64_GLOB_DAT:
	case R_AARCH64_JUMP_SLOT:
		error = lookup(lf, symidx, 1, &addr);
		if (error != 0)
			return (-1);
		*where = addr + addend;
		break;
	case R_AARCH64_IRELATIVE:
		addr = relocbase + addend;
		val = ((Elf64_Addr (*)(void))addr)();
		if (*where != val)
			*where = val;
		break;
	default:
		printf("kldload: unexpected relocation type %d, "
		    "symbol index %d\n", rtype, symidx);
		return (-1);
	}
	return (error);
}

int
elf_reloc_local(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, elf_lookup_fn lookup)
{

	return (elf_reloc_internal(lf, relocbase, data, type,
	    ARM64_ELF_RELOC_LOCAL, lookup));
}

/* Process one elf relocation with addend. */
int
elf_reloc(linker_file_t lf, Elf_Addr relocbase, const void *data, int type,
    elf_lookup_fn lookup)
{

	return (elf_reloc_internal(lf, relocbase, data, type, 0, lookup));
}

int
elf_reloc_late(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, elf_lookup_fn lookup)
{

	return (elf_reloc_internal(lf, relocbase, data, type,
	    ARM64_ELF_RELOC_LATE_IFUNC, lookup));
}

int
elf_cpu_load_file(linker_file_t lf)
{

	if (lf->id != 1)
		cpu_icache_sync_range((vm_offset_t)lf->address, lf->size);
	return (0);
}

int
elf_cpu_unload_file(linker_file_t lf __unused)
{

	return (0);
}

int
elf_cpu_parse_dynamic(caddr_t loadbase __unused, Elf_Dyn *dynamic __unused)
{

	return (0);
}
