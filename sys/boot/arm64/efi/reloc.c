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

/*
 * A simple relocator for ARM64 EFI binaries.
 */
EFI_STATUS
_reloc(unsigned long ImageBase, Elf64_Dyn *dynamic)
{
	unsigned long relent, relcnt;
	unsigned long *newaddr;
	Elf64_Rela *rel;
	Elf64_Dyn *dynp;

	/*
	 * Find the relocation address, its size and the relocation entry.
	 */
	relent = 0;
	relcnt = 0;
	for (dynp = dynamic; dynp->d_tag != DT_NULL; dynp++) {
		switch (dynp->d_tag) {
		case DT_RELA:
			rel = (Elf64_Rela *)((caddr_t)dynp->d_un.d_ptr +
			    ImageBase);
			break;
		case DT_RELACOUNT:
			relcnt = dynp->d_un.d_val;
			break;
		case DT_RELAENT:
			relent = dynp->d_un.d_val;
			break;
		default:
			break;
		}
	}

	/*
	 * Perform the actual relocation.
	 */
	for (; relcnt != 0; relcnt--) {
		switch (ELF64_R_TYPE(rel->r_info)) {
		case R_AARCH64_RELATIVE:
			newaddr = (unsigned long *)(ImageBase + rel->r_offset);
			*newaddr = ImageBase + rel->r_addend;
			break;
		default:
			/* XXX: do we need other relocations ? */
			break;
		}
		rel = (Elf64_Rela *)((caddr_t)rel + relent);
	}

	return (EFI_SUCCESS);
}

