/*-
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
 *
 *	from: src/sys/i386/i386/elf_machdep.c,v 1.20 2004/08/11 02:35:05 marcel
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/linker.h>
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
#include <machine/cache.h>

#ifdef __mips_n64
struct sysentvec elf64_freebsd_sysvec = {
	.sv_size	= SYS_MAXSYSCALL,
	.sv_table	= sysent,
	.sv_mask	= 0,
	.sv_sigsize	= 0,
	.sv_sigtbl	= NULL,
	.sv_errsize	= 0,
	.sv_errtbl	= NULL,
	.sv_transtrap	= NULL,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= sendsig,
	.sv_sigcode	= sigcode,
	.sv_szsigcode	= &szsigcode,
	.sv_prepsyscall	= NULL,
	.sv_name	= "FreeBSD ELF64",
	.sv_coredump	= __elfN(coredump),
	.sv_imgact_try	= NULL,
	.sv_minsigstksz	= MINSIGSTKSZ,
	.sv_pagesize	= PAGE_SIZE,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= USRSTACK,
	.sv_psstrings	= PS_STRINGS,
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings = exec_copyout_strings,
	.sv_setregs	= exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_FREEBSD | SV_LP64,
	.sv_set_syscall_retval = cpu_set_syscall_retval,
	.sv_fetch_syscall_args = cpu_fetch_syscall_args,
	.sv_syscallnames = syscallnames,
	.sv_schedtail	= NULL,
	.sv_thread_detach = NULL,
	.sv_trap	= NULL,
};

static Elf64_Brandinfo freebsd_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_MIPS,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf64_freebsd_sysvec,
	.interp_newpath	= NULL,
	.flags		= 0
};

SYSINIT(elf64, SI_SUB_EXEC, SI_ORDER_ANY,
    (sysinit_cfunc_t) elf64_insert_brand_entry,
    &freebsd_brand_info);

void
elf64_dump_thread(struct thread *td __unused, void *dst __unused,
    size_t *off __unused)
{
}
#else
struct sysentvec elf32_freebsd_sysvec = {
	.sv_size	= SYS_MAXSYSCALL,
	.sv_table	= sysent,
	.sv_mask	= 0,
	.sv_sigsize	= 0,
	.sv_sigtbl	= NULL,
	.sv_errsize	= 0,
	.sv_errtbl	= NULL,
	.sv_transtrap	= NULL,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= sendsig,
	.sv_sigcode	= sigcode,
	.sv_szsigcode	= &szsigcode,
	.sv_prepsyscall	= NULL,
	.sv_name	= "FreeBSD ELF32",
	.sv_coredump	= __elfN(coredump),
	.sv_imgact_try	= NULL,
	.sv_minsigstksz	= MINSIGSTKSZ,
	.sv_pagesize	= PAGE_SIZE,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= USRSTACK,
	.sv_psstrings	= PS_STRINGS,
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings = exec_copyout_strings,
	.sv_setregs	= exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_FREEBSD | SV_ILP32,
	.sv_set_syscall_retval = cpu_set_syscall_retval,
	.sv_fetch_syscall_args = cpu_fetch_syscall_args,
	.sv_syscallnames = syscallnames,
	.sv_schedtail	= NULL,
	.sv_thread_detach = NULL,
	.sv_trap	= NULL,
};

static Elf32_Brandinfo freebsd_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_MIPS,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf32_freebsd_sysvec,
	.interp_newpath	= NULL,
	.flags		= 0
};

SYSINIT(elf32, SI_SUB_EXEC, SI_ORDER_FIRST,
    (sysinit_cfunc_t) elf32_insert_brand_entry,
    &freebsd_brand_info);

void
elf32_dump_thread(struct thread *td __unused, void *dst __unused,
    size_t *off __unused)
{
}
#endif

/* Process one elf relocation with addend. */
static int
elf_reloc_internal(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, int local, elf_lookup_fn lookup)
{
	Elf32_Addr *where = (Elf32_Addr *)NULL;
	Elf_Addr addr;
	Elf_Addr addend = (Elf_Addr)0;
	Elf_Word rtype = (Elf_Word)0, symidx;
	const Elf_Rel *rel = NULL;
	const Elf_Rela *rela = NULL;
	int error;

	/*
	 * Stash R_MIPS_HI16 info so we can use it when processing R_MIPS_LO16
	 */
	static Elf_Addr ahl;
	static Elf32_Addr *where_hi16;

	switch (type) {
	case ELF_RELOC_REL:
		rel = (const Elf_Rel *)data;
		where = (Elf32_Addr *) (relocbase + rel->r_offset);
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);
		switch (rtype) {
		case R_MIPS_64:
			addend = *(Elf64_Addr *)where;
			break;
		default:
			addend = *where;
			break;
		}

		break;
	case ELF_RELOC_RELA:
		rela = (const Elf_Rela *)data;
		where = (Elf32_Addr *) (relocbase + rela->r_offset);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
		break;
	default:
		panic("unknown reloc type %d\n", type);
	}

	switch (rtype) {
	case R_MIPS_NONE:	/* none */
		break;

	case R_MIPS_32:		/* S + A */
		error = lookup(lf, symidx, 1, &addr);
		if (error != 0)
			return (-1);
		addr += addend;
		if (*where != addr)
			*where = (Elf32_Addr)addr;
		break;

	case R_MIPS_26:		/* ((A << 2) | (P & 0xf0000000) + S) >> 2 */
		error = lookup(lf, symidx, 1, &addr);
		if (error != 0)
			return (-1);

		addend &= 0x03ffffff;
		/*
		 * Addendum for .rela R_MIPS_26 is not shifted right
		 */
		if (rela == NULL)
			addend <<= 2;

		addr += ((Elf_Addr)where & 0xf0000000) | addend;
		addr >>= 2;

		*where &= ~0x03ffffff;
		*where |= addr & 0x03ffffff;
		break;

	case R_MIPS_64:		/* S + A */
		error = lookup(lf, symidx, 1, &addr);
		if (error != 0)
			return (-1);
		addr += addend;
		if (*(Elf64_Addr*)where != addr)
			*(Elf64_Addr*)where = addr;
		break;

	case R_MIPS_HI16:	/* ((AHL + S) - ((short)(AHL + S)) >> 16 */
		if (rela != NULL) {
			error = lookup(lf, symidx, 1, &addr);
			if (error != 0)
				return (-1);
			addr += addend;
			*where &= 0xffff0000;
			*where |= ((((long long) addr + 0x8000LL) >> 16) & 0xffff);
		}
		else {
			ahl = addend << 16;
			where_hi16 = where;
		}
		break;

	case R_MIPS_LO16:	/* AHL + S */
		if (rela != NULL) {
			error = lookup(lf, symidx, 1, &addr);
			if (error != 0)
				return (-1);
			addr += addend;
			*where &= 0xffff0000;
			*where |= addr & 0xffff;
		}
		else {
			ahl += (int16_t)addend;
			error = lookup(lf, symidx, 1, &addr);
			if (error != 0)
				return (-1);

			addend &= 0xffff0000;
			addend |= (uint16_t)(ahl + addr);
			*where = addend;

			addend = *where_hi16;
			addend &= 0xffff0000;
			addend |= ((ahl + addr) - (int16_t)(ahl + addr)) >> 16;
			*where_hi16 = addend;
		}

		break;

	case R_MIPS_HIGHER:	/* %higher(A+S) */
		error = lookup(lf, symidx, 1, &addr);
		if (error != 0)
			return (-1);
		addr += addend;
		*where &= 0xffff0000;
		*where |= (((long long)addr + 0x80008000LL) >> 32) & 0xffff;
		break;

	case R_MIPS_HIGHEST:	/* %highest(A+S) */
		error = lookup(lf, symidx, 1, &addr);
		if (error != 0)
			return (-1);
		addr += addend;
		*where &= 0xffff0000;
		*where |= (((long long)addr + 0x800080008000LL) >> 48) & 0xffff;
		break;

	default:
		printf("kldload: unexpected relocation type %d\n",
			rtype);
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
elf_cpu_load_file(linker_file_t lf __unused)
{

	/*
	 * Sync the I and D caches to make sure our relocations are visible.
	 */
	mips_icache_sync_all();

	return (0);
}

int
elf_cpu_unload_file(linker_file_t lf __unused)
{

	return (0);
}
