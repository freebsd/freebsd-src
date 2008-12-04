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
#include <sys/sysent.h>
#include <sys/imgact_elf.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/linker.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <machine/cpu.h>
#include <machine/elf.h>
#include <machine/md_var.h>

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
	.sv_flags	= SV_ABI_FREEBSD | SV_ILP32
};

static Elf32_Brandinfo freebsd_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_PPC,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf32_freebsd_sysvec,
	.interp_newpath	= NULL,
	.flags		= BI_CAN_EXEC_DYN,
};

SYSINIT(elf32, SI_SUB_EXEC, SI_ORDER_ANY,
    (sysinit_cfunc_t) elf32_insert_brand_entry,
    &freebsd_brand_info);

static Elf32_Brandinfo freebsd_brand_oinfo = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_PPC,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/usr/libexec/ld-elf.so.1",
	.sysvec		= &elf32_freebsd_sysvec,
	.interp_newpath	= NULL,
	.flags		= BI_CAN_EXEC_DYN,
};

SYSINIT(oelf32, SI_SUB_EXEC, SI_ORDER_ANY,
	(sysinit_cfunc_t) elf32_insert_brand_entry,
	&freebsd_brand_oinfo);


void
elf32_dump_thread(struct thread *td __unused, void *dst __unused,
    size_t *off __unused)
{
}


/* Process one elf relocation with addend. */
static int
elf_reloc_internal(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, int local, elf_lookup_fn lookup)
{
	Elf_Addr *where;
	Elf_Half *hwhere;
	Elf_Addr addr;
	Elf_Addr addend;
	Elf_Word rtype, symidx;
	const Elf_Rela *rela;

	switch (type) {
	case ELF_RELOC_REL:
		panic("PPC only supports RELA relocations");
		break;
	case ELF_RELOC_RELA:
		rela = (const Elf_Rela *)data;
		where = (Elf_Addr *) (relocbase + rela->r_offset);
		hwhere = (Elf_Half *) (relocbase + rela->r_offset);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
		break;
	default:
		panic("elf_reloc: unknown relocation mode %d\n", type);
	}

	switch (rtype) {

       	case R_PPC_NONE:
	       	break;

	case R_PPC_ADDR32: /* word32 S + A */
       		addr = lookup(lf, symidx, 1);
	       	if (addr == 0)
	       		return -1;
		addr += addend;
	       	*where = addr;
	       	break;

       	case R_PPC_ADDR16_LO: /* #lo(S) */
		addr = lookup(lf, symidx, 1);
		if (addr == 0)
			return -1;
		/*
		 * addend values are sometimes relative to sections
		 * (i.e. .rodata) in rela, where in reality they
		 * are relative to relocbase. Detect this condition.
		 */
		if (addr > relocbase && addr <= (relocbase + addend))
			addr = relocbase + addend;
		else
			addr += addend;
		*hwhere = addr & 0xffff;
		break;

	case R_PPC_ADDR16_HA: /* #ha(S) */
		addr = lookup(lf, symidx, 1);
		if (addr == 0)
			return -1;
		/*
		 * addend values are sometimes relative to sections
		 * (i.e. .rodata) in rela, where in reality they
		 * are relative to relocbase. Detect this condition.
		 */
		if (addr > relocbase && addr <= (relocbase + addend))
			addr = relocbase + addend;
		else
			addr += addend;
	       	*hwhere = ((addr >> 16) + ((addr & 0x8000) ? 1 : 0))
		    & 0xffff;
		break;

	case R_PPC_RELATIVE: /* word32 B + A */
       		*where = relocbase + addend;
	       	break;

	default:
       		printf("kldload: unexpected relocation type %d\n",
	       	    (int) rtype);
		return -1;
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
	/* Only sync the cache for non-kernel modules */
	if (lf->id != 1)
		__syncicache(lf->address, lf->size);
	return (0);
}

int
elf_cpu_unload_file(linker_file_t lf __unused)
{

	return (0);
}
