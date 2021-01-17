/*-
 * Copyright (c) 2003 Jake Burkholder.
 * Copyright 1996-1998 John D. Polstra.
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 1998 Peter Wemm <peter@freebsd.org>
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

#include <sys/types.h>
#include <machine/elf.h>

#include <stand.h>

#define FREEBSD_ELF
#include <sys/link_elf.h>

#include "bootstrap.h"

#define COPYOUT(s,d,l)	archsw.arch_copyout((vm_offset_t)(s), d, l)

/*
 * Apply a single intra-module relocation to the data. `relbase' is the
 * target relocation base for the section (i.e. it corresponds to where
 * r_offset == 0). `dataaddr' is the relocated address corresponding to
 * the start of the data, and `len' is the number of bytes.
 */
int
__elfN(reloc)(struct elf_file *ef, symaddr_fn *symaddr, const void *reldata,
    int reltype, Elf_Addr relbase, Elf_Addr dataaddr, void *data, size_t len)
{
#if (defined(__aarch64__) || defined(__amd64__) || defined(__i386__)) && \
    __ELF_WORD_SIZE == 64
	Elf64_Addr *where, val;
	Elf_Addr addend, addr;
	Elf_Size rtype, symidx;
	const Elf_Rel *rel;
	const Elf_Rela *rela;

	switch (reltype) {
	case ELF_RELOC_REL:
		rel = (const Elf_Rel *)reldata;
		where = (Elf_Addr *)((char *)data + relbase + rel->r_offset -
		    dataaddr);
		addend = 0;
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);
		addend = 0;
		break;
	case ELF_RELOC_RELA:
		rela = (const Elf_Rela *)reldata;
		where = (Elf_Addr *)((char *)data + relbase + rela->r_offset -
		    dataaddr);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
		break;
	default:
		return (EINVAL);
	}

	if ((char *)where < (char *)data || (char *)where >= (char *)data + len)
		return (0);

	if (reltype == ELF_RELOC_REL)
		addend = *where;

#if defined(__aarch64__)
#define	RELOC_RELATIVE		R_AARCH64_RELATIVE
#define	RELOC_IRELATIVE		R_AARCH64_IRELATIVE
#elif defined(__amd64__) || defined(__i386__)
/* XXX, definitions not available on i386. */
#define	R_X86_64_64		1
#define	R_X86_64_RELATIVE	8
#define	R_X86_64_IRELATIVE	37

#define	RELOC_RELATIVE		R_X86_64_RELATIVE
#define	RELOC_IRELATIVE		R_X86_64_IRELATIVE
#endif

	switch (rtype) {
	case RELOC_RELATIVE:
		addr = (Elf_Addr)addend + relbase;
		val = addr;
		memcpy(where, &val, sizeof(val));
		break;
	case RELOC_IRELATIVE:
		/* leave it to kernel */
		break;
#if defined(__amd64__) || defined(__i386__)
	case R_X86_64_64:		/* S + A */
		addr = symaddr(ef, symidx);
		if (addr == 0)
			return (ESRCH);
		val = addr + addend;
		*where = val;
		break;
#endif
	default:
		printf("\nunhandled relocation type %u\n", (u_int)rtype);
		return (EFTYPE);
	}

	return (0);
#elif defined(__i386__) && __ELF_WORD_SIZE == 32
	Elf_Addr addend, addr, *where, val;
	Elf_Size rtype, symidx;
	const Elf_Rel *rel;
	const Elf_Rela *rela;

	switch (reltype) {
	case ELF_RELOC_REL:
		rel = (const Elf_Rel *)reldata;
		where = (Elf_Addr *)((char *)data + relbase + rel->r_offset -
		    dataaddr);
		addend = 0;
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);
		addend = 0;
		break;
	case ELF_RELOC_RELA:
		rela = (const Elf_Rela *)reldata;
		where = (Elf_Addr *)((char *)data + relbase + rela->r_offset -
		    dataaddr);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
		break;
	default:
		return (EINVAL);
	}

	if ((char *)where < (char *)data || (char *)where >= (char *)data + len)
		return (0);

	if (reltype == ELF_RELOC_REL)
		addend = *where;

/* XXX, definitions not available on amd64. */
#define R_386_32	1	/* Add symbol value. */
#define R_386_GLOB_DAT	6	/* Set GOT entry to data address. */
#define R_386_RELATIVE	8	/* Add load address of shared object. */
#define	R_386_IRELATIVE	42

	switch (rtype) {
	case R_386_RELATIVE:
		addr = addend + relbase;
		*where = addr;
		break;
	case R_386_32:		/* S + A */
		addr = symaddr(ef, symidx);
		if (addr == 0)
			return (ESRCH);
		val = addr + addend;
		*where = val;
		break;
	case R_386_IRELATIVE:
		/* leave it to kernel */
		break;
	default:
		printf("\nunhandled relocation type %u\n", (u_int)rtype);
		return (EFTYPE);
	}

	return (0);
#elif defined(__powerpc__) || defined(__riscv)
	Elf_Size w;
	const Elf_Rela *rela;

	switch (reltype) {
	case ELF_RELOC_RELA:
		rela = reldata;
		if (relbase + rela->r_offset >= dataaddr &&
		    relbase + rela->r_offset < dataaddr + len) {
			switch (ELF_R_TYPE(rela->r_info)) {
#if defined(__powerpc__)
			case R_PPC_RELATIVE:
#elif defined(__riscv)
			case R_RISCV_RELATIVE:
#endif
				w = relbase + rela->r_addend;
				bcopy(&w, (u_char *)data + (relbase +
				      rela->r_offset - dataaddr), sizeof(w));
				break;
			default:
				printf("\nunhandled relocation type %u\n",
				       (u_int)ELF_R_TYPE(rela->r_info));
				return (EFTYPE);
			}
		}
		break;
	}

	return (0);
#else
	return (EOPNOTSUPP);
#endif
}
