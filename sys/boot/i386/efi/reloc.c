/*-
 * Copyright (c) 2008-2010 Rui Paulo <rpaulo@FreeBSD.org>
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
#include <elf.h>
#include <efi.h>
#include <bootstrap.h>

#ifdef __i386__
#define ElfW_Rel	Elf32_Rel
#define	ElfW_Dyn	Elf32_Dyn
#define	ELFW_R_TYPE	ELF32_R_TYPE
#elif __amd64__
#define ElfW_Rel	Elf64_Rel
#define	ElfW_Dyn	Elf64_Dyn
#define	ELFW_R_TYPE	ELF64_R_TYPE
#endif

/*
 * A simple relocator for IA32/AMD64 EFI binaries.
 */
EFI_STATUS
_reloc(unsigned long ImageBase, ElfW_Dyn *dynamic, EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE *system_table)
{
	unsigned long relsz, relent;
	unsigned long *newaddr;
	ElfW_Rel *rel;
	ElfW_Dyn *dynp;

	/*
	 * Find the relocation address, its size and the relocation entry.
	 */
	relsz = 0;
	relent = 0;
	for (dynp = dynamic; dynp->d_tag != DT_NULL; dynp++) {
		switch (dynp->d_tag) {
		case DT_RELA:
		case DT_REL:
			rel = (ElfW_Rel *) ((unsigned long) dynp->d_un.d_ptr +
			    ImageBase);
			break;
		case DT_RELSZ:
		case DT_RELASZ:
			relsz = dynp->d_un.d_val;
			break;
		case DT_RELENT:
		case DT_RELAENT:
			relent = dynp->d_un.d_val;
			break;
		default:
			break;
		}
	}

	/*
	 * Perform the actual relocation.
	 * XXX: We are reusing code for the amd64 version of this, but
	 * we must make sure the relocation types are the same.
	 */
	CTASSERT(R_386_NONE == R_X86_64_NONE);
	CTASSERT(R_386_RELATIVE == R_X86_64_RELATIVE);
	for (; relsz > 0; relsz -= relent) {
		switch (ELFW_R_TYPE(rel->r_info)) {
		case R_386_NONE:
			/* No relocation needs be performed. */
			break;
		case R_386_RELATIVE:
			/* Address relative to the base address. */
			newaddr = (unsigned long *)(ImageBase + rel->r_offset);
			*newaddr += ImageBase;
			break;
		default:
			/* XXX: do we need other relocations ? */
			break;
		}
		rel = (ElfW_Rel *) ((caddr_t) rel + relent);
	}

	return (EFI_SUCCESS);
}
