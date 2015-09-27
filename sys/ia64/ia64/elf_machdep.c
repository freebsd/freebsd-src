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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/linker.h>
#include <sys/sysent.h>
#include <sys/imgact_elf.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <machine/elf.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/unwind.h>

Elf_Addr link_elf_get_gp(linker_file_t);

extern Elf_Addr fptr_storage[];

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
	.sv_sigcode	= NULL,
	.sv_szsigcode	= NULL,
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
	.sv_stackprot	= VM_PROT_READ|VM_PROT_WRITE,
	.sv_copyout_strings = exec_copyout_strings,
	.sv_setregs	= exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_FREEBSD | SV_LP64,
	.sv_set_syscall_retval = cpu_set_syscall_retval,
	.sv_fetch_syscall_args = cpu_fetch_syscall_args,
	.sv_syscallnames = syscallnames,
	.sv_schedtail	= NULL,
};

static Elf64_Brandinfo freebsd_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_IA_64,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf64_freebsd_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &elf64_freebsd_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};
SYSINIT(elf64, SI_SUB_EXEC, SI_ORDER_FIRST,
    (sysinit_cfunc_t)elf64_insert_brand_entry, &freebsd_brand_info);

static Elf64_Brandinfo freebsd_brand_oinfo = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_IA_64,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/usr/libexec/ld-elf.so.1",
	.sysvec		= &elf64_freebsd_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &elf64_freebsd_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};
SYSINIT(oelf64, SI_SUB_EXEC, SI_ORDER_ANY,
    (sysinit_cfunc_t)elf64_insert_brand_entry, &freebsd_brand_oinfo);


void
elf64_dump_thread(struct thread *td, void *dst, size_t *off __unused)
{

	/* Flush the dirty registers onto the backingstore. */
	if (dst == NULL)
		ia64_flush_dirty(td, &td->td_frame->tf_special);
}


static int
lookup_fdesc(linker_file_t lf, Elf_Size symidx, elf_lookup_fn lookup,
    Elf_Addr *addr1)
{
	linker_file_t top;
	Elf_Addr addr;
	const char *symname;
	int i, error;
	static int eot = 0;

	error = lookup(lf, symidx, 0, &addr);
	if (error != 0) {
		top = lf;
		symname = elf_get_symname(top, symidx);
		for (i = 0; i < top->ndeps; i++) {
			lf = top->deps[i];
			addr = (Elf_Addr)linker_file_lookup_symbol(lf,
			    symname, 0);
			if (addr != 0)
				break;
		}
		if (addr == 0)
			return (EINVAL);
	}

	if (eot)
		return (EINVAL);

	/*
	 * Lookup and/or construct OPD
	 */
	for (i = 0; i < 8192; i += 2) {
		if (fptr_storage[i] == addr) {
			*addr1 = (Elf_Addr)(fptr_storage + i);
			return (0);
		}

		if (fptr_storage[i] == 0) {
			fptr_storage[i] = addr;
			fptr_storage[i+1] = link_elf_get_gp(lf);
			*addr1 = (Elf_Addr)(fptr_storage + i);
			return (0);
		}
	}

	printf("%s: fptr table full\n", __func__);
	eot = 1;

	return (EINVAL);
}

/* Process one elf relocation with addend. */
static int
elf_reloc_internal(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, int local, elf_lookup_fn lookup)
{
	Elf_Addr *where;
	Elf_Addr addend, addr;
	Elf_Size rtype, symidx;
	const Elf_Rel *rel;
	const Elf_Rela *rela;
	int error;

	switch (type) {
	case ELF_RELOC_REL:
		rel = (const Elf_Rel *)data;
		where = (Elf_Addr *)(relocbase + rel->r_offset);
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);
		switch (rtype) {
		case R_IA_64_DIR64LSB:
		case R_IA_64_FPTR64LSB:
		case R_IA_64_REL64LSB:
			addend = *where;
			break;
		default:
			addend = 0;
			break;
		}
		break;
	case ELF_RELOC_RELA:
		rela = (const Elf_Rela *)data;
		where = (Elf_Addr *)(relocbase + rela->r_offset);
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
		addend = rela->r_addend;
		break;
	default:
		panic("%s: invalid ELF relocation (0x%x)\n", __func__, type);
	}

	if (local) {
		if (rtype == R_IA_64_REL64LSB)
			*where = elf_relocaddr(lf, relocbase + addend);
		return (0);
	}

	switch (rtype) {
	case R_IA_64_NONE:
		break;
	case R_IA_64_DIR64LSB:	/* word64 LSB	S + A */
		error = lookup(lf, symidx, 1, &addr);
		if (error != 0)
			return (-1);
		*where = addr + addend;
		break;
	case R_IA_64_FPTR64LSB:	/* word64 LSB	@fptr(S + A) */
		if (addend != 0) {
			printf("%s: addend ignored for OPD relocation\n",
			    __func__);
		}
		error = lookup_fdesc(lf, symidx, lookup, &addr);
		if (error != 0)
			return (-1);
		*where = addr;
		break;
	case R_IA_64_REL64LSB:	/* word64 LSB	BD + A */
		break;
	case R_IA_64_IPLTLSB:
		error = lookup_fdesc(lf, symidx, lookup, &addr);
		if (error != 0)
			return (-1);
		where[0] = *((Elf_Addr*)addr) + addend;
		where[1] = *((Elf_Addr*)addr + 1);
		break;
	default:
		printf("%s: unknown relocation (0x%x)\n", __func__,
		    (int)rtype);
		return -1;
	}

	return (0);
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
	Elf_Ehdr *hdr;
	Elf_Phdr *ph, *phlim;
	Elf_Addr reloc, vaddr;

	hdr = (Elf_Ehdr *)(lf->address);
	if (!IS_ELF(*hdr)) {
		printf("Missing or corrupted ELF header at %p\n", hdr);
		return (EFTYPE);
	}

	/*
	 * Iterate over the segments and register the unwind table if
	 * we come across it.
	 */
	ph = (Elf_Phdr *)(lf->address + hdr->e_phoff);
	phlim = ph + hdr->e_phnum;
	reloc = ~0ULL;
	while (ph < phlim) {
		if (ph->p_type == PT_LOAD && reloc == ~0ULL)
			reloc = (Elf_Addr)lf->address - ph->p_vaddr;

		if (ph->p_type == PT_IA_64_UNWIND) {
			vaddr = ph->p_vaddr + reloc;
			unw_table_add((vm_offset_t)lf->address, vaddr,
			    vaddr + ph->p_memsz);
		}
		++ph;
	}

	/*
	 * Make the I-cache coherent, but don't worry obout the kernel
	 * itself because the loader needs to do that.
	 */
	if (lf->id != 1)
		ia64_sync_icache((uintptr_t)lf->address, lf->size);

	return (0);
}

int
elf_cpu_unload_file(linker_file_t lf)
{

	unw_table_remove((vm_offset_t)lf->address);
	return (0);
}
