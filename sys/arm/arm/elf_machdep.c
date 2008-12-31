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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/arm/elf_machdep.c,v 1.7.6.1 2008/11/25 02:59:29 kensmith Exp $");

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
	SYS_MAXSYSCALL,
	sysent,
	0,
	0,
	NULL,
	0,
	NULL,
	NULL,
	__elfN(freebsd_fixup),
	sendsig,
	sigcode,
	&szsigcode,
	NULL,
	"FreeBSD ELF32",
	__elfN(coredump),
	NULL,
	MINSIGSTKSZ,
	PAGE_SIZE,
	VM_MIN_ADDRESS,
	VM_MAXUSER_ADDRESS,
	USRSTACK,
	PS_STRINGS,
	VM_PROT_ALL,
	exec_copyout_strings,
	exec_setregs,
	NULL
};

static Elf32_Brandinfo freebsd_brand_info = {
						ELFOSABI_FREEBSD,
						EM_ARM,
						"FreeBSD",
						NULL,
						"/libexec/ld-elf.so.1",
						&elf32_freebsd_sysvec,
						NULL,
						BI_CAN_EXEC_DYN,
					  };

SYSINIT(elf32, SI_SUB_EXEC, SI_ORDER_ANY,
	(sysinit_cfunc_t) elf32_insert_brand_entry,
	&freebsd_brand_info);

static Elf32_Brandinfo freebsd_brand_oinfo = {
						ELFOSABI_FREEBSD,
						EM_ARM,
						"FreeBSD",
						NULL,
						"/usr/libexec/ld-elf.so.1",
						&elf32_freebsd_sysvec,
						NULL,
						BI_CAN_EXEC_DYN,
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
	Elf_Addr addr;
	Elf_Addr addend;
	Elf_Word rtype, symidx;
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
		if (rtype == R_ARM_RELATIVE) {	/* A + B */
			addr = relocbase + addend;
			if (*where != addr)
				*where = addr;
		}
		return (0);
	}

	switch (rtype) {

		case R_ARM_NONE:	/* none */
			break;

		case R_ARM_ABS32:
			addr = lookup(lf, symidx, 1);
			if (addr == 0)
				return -1;
			if (*where != addr)
				*where = addr;

			break;

		case R_ARM_COPY:	/* none */
			/*
			 * There shouldn't be copy relocations in kernel
			 * objects.
			 */
			printf("kldload: unexpected R_COPY relocation\n");
			return -1;
			break;

		case R_ARM_JUMP_SLOT:
			addr = lookup(lf, symidx, 1);
			if (addr) {
				*where = addr;
				return (0);
			}
			return (-1);
		case R_ARM_RELATIVE:
			break;

		default:
			printf("kldload: unexpected relocation type %d\n",
			       rtype);
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
elf_cpu_load_file(linker_file_t lf __unused)
{

	cpu_idcache_wbinv_all();
	cpu_tlb_flushID();
	return (0);
}

int
elf_cpu_unload_file(linker_file_t lf __unused)
{

	return (0);
}
