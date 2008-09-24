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
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

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
	.sv_maxssiz	= NULL
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

SYSINIT(elf32, SI_SUB_EXEC, SI_ORDER_ANY,
    (sysinit_cfunc_t) elf32_insert_brand_entry,
    &freebsd_brand_info);

static Elf32_Brandinfo freebsd_brand_oinfo = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_MIPS,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/usr/libexec/ld-elf.so.1",
	.sysvec		= &elf32_freebsd_sysvec,
	.interp_newpath	= NULL,
	.flags		= 0
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
	Elf_Addr *where = (Elf_Addr *)NULL;;
	Elf_Addr addr;
	Elf_Addr addend = (Elf_Addr)0;
	Elf_Word rtype = (Elf_Word)0, symidx;
	const Elf_Rel *rel;
	const Elf_Rela *rela;

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

	if (local) {
#if 0 /* TBD  */
		if (rtype == R_386_RELATIVE) {	/* A + B */
			addr = relocbase + addend;
			if (*where != addr)
				*where = addr;
		}
		return (0);
#endif
	}

	switch (rtype) {

		case R_MIPS_NONE:	/* none */
			break;

		case R_MIPS_16:	    /* S + sign-extend(A) */
			/*
			 * There shouldn't be R_MIPS_16 relocs in kernel objects.
			 */
			printf("kldload: unexpected R_MIPS_16 relocation\n");
			return -1;
			break;

		case R_MIPS_32: /* S + A - P */
			addr = lookup(lf, symidx, 1);
			if (addr == 0)
				return -1;
			addr += addend;
			if (*where != addr)
				*where = addr;
			break;

		case R_MIPS_REL32:		/* A - EA + S */
			/*
			 * There shouldn't be R_MIPS_REL32 relocs in kernel objects?
			 */
			printf("kldload: unexpected R_MIPS_REL32 relocation\n");
			return -1;
			break;

		case R_MIPS_26:	     /* ((A << 2) | (P & 0xf0000000) + S) >> 2 */
			break;

		case R_MIPS_HI16:
			/* extern/local: ((AHL + S) - ((short)(AHL + S)) >> 16 */
			/* _gp_disp: ((AHL + GP - P) - (short)(AHL + GP - P)) >> 16 */
			break;

		case R_MIPS_LO16:
			/* extern/local: AHL + S */
			/* _gp_disp: AHL + GP - P + 4 */
			break;

		case R_MIPS_GPREL16:
			/* extern/local: ((AHL + S) - ((short)(AHL + S)) >> 16 */
			/* _gp_disp: ((AHL + GP - P) - (short)(AHL + GP - P)) >> 16 */
			break;

		case R_MIPS_LITERAL: /* sign-extend(A) + L */
			break;

		case R_MIPS_GOT16: /* external: G */
			/* local: tbd */
			break;

		case R_MIPS_PC16: /* sign-extend(A) + S - P */
			break;

		case R_MIPS_CALL16: /* G */
			break;

		case R_MIPS_GPREL32: /* A + S + GP0 - GP */
			break;

		case R_MIPS_GOTHI16: /* (G - (short)G) >> 16 + A */
			break;

		case R_MIPS_GOTLO16: /* G & 0xffff */
			break;

		case R_MIPS_CALLHI16: /* (G - (short)G) >> 16 + A */
			break;

		case R_MIPS_CALLLO16: /* G & 0xffff */
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

	return (0);
}

int
elf_cpu_unload_file(linker_file_t lf __unused)
{

	return (0);
}
