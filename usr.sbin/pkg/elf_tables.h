/*-
 * Copyright (c) 2012 Olivier Houchard <cognet@FreeBSD.org>
 * Copyright (c) 2012 Baptiste Daroussin <bapt@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef ELF_TABLES_H_
#define ELF_TABLES_H_
struct _elf_corres {
	int elf_nb;
	const char *string;
};

static struct _elf_corres mach_corres[] = {
	{ EM_386, "x86" },
	{ EM_AMD64, "x86" },
	{ EM_ARM, "arm" },
	{ EM_MIPS, "mips" },
	{ EM_PPC, "powerpc" },
	{ EM_PPC64, "powerpc" },
	{ EM_SPARCV9, "sparc64" },
	{ EM_IA_64, "ia64" },
	{ -1, NULL },
};

static struct _elf_corres wordsize_corres[] = {
	{ ELFCLASS32, "32" },
	{ ELFCLASS64, "64" },
	{ -1, NULL},
};

static struct _elf_corres endian_corres[] = {
	{ ELFDATA2MSB, "eb" },
	{ ELFDATA2LSB, "el" },
	{ -1, NULL}
};

#define EF_MIPS_ABI	0x0000F000
#define E_MIPS_ABI_O32	0x00001000
#define E_MIPS_ABI_N32	0x00000020

#define EF_ARM_NEW_ABI	0x80
#define EF_ARM_OLD_ABI	0x100

#define EF_ARM_SOFT_FLOAT	0x200
#define EF_ARM_VFP_FLOAT	0x400

#define NT_VERSION	1
#define NT_ARCH	2

#endif /* ELF_TABLES_H_ */
