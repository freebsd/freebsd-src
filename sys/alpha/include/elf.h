/*-
 * Copyright (c) 1996-1997 John D. Polstra.
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
 * $FreeBSD: src/sys/alpha/include/elf.h,v 1.9 1999/12/29 04:27:57 peter Exp $
 */

#ifndef _MACHINE_ELF_H_
#define _MACHINE_ELF_H_ 1

/*
 * ELF definitions for the alpha architecture.
 */

#include <sys/elf64.h>	/* Definitions common to all 64 bit architectures. */

#define __ELF_WORD_SIZE	64	/* Used by <sys/elf_generic.h> */
#include <sys/elf_generic.h>

#define ELF_ARCH	EM_ALPHA

#define ELF_MACHINE_OK(x)	((x) == EM_ALPHA)

/*
 * Auxiliary vector entries for passing information to the interpreter.
 *
 * The i386 supplement to the SVR4 ABI specification names this "auxv_t",
 * but POSIX lays claim to all symbols ending with "_t".
 */

typedef struct {	/* Auxiliary vector entry on initial stack */
	long	a_type;			/* Entry type. */
	union {
		long	a_val;		/* Integer value. */
		void	*a_ptr;		/* Address. */
		void	(*a_fcn)(void);	/* Function pointer (not used). */
	} a_un;
} Elf64_Auxinfo;

__ElfType(Auxinfo);

/* Values for a_type. */
#define AT_NULL		0	/* Terminates the vector. */
#define AT_IGNORE	1	/* Ignored entry. */
#define AT_EXECFD	2	/* File descriptor of program to load. */
#define AT_PHDR		3	/* Program header of program already loaded. */
#define AT_PHENT	4	/* Size of each program header entry. */
#define AT_PHNUM	5	/* Number of program header entries. */
#define AT_PAGESZ	6	/* Page size in bytes. */
#define AT_BASE		7	/* Interpreter's base address. */
#define AT_FLAGS	8	/* Flags (unused for i386). */
#define AT_ENTRY	9	/* Where interpreter should transfer control. */

/*
 * The following non-standard values are used for passing information
 * from John Polstra's testbed program to the dynamic linker.  These
 * are expected to go away soon.
 *
 * Unfortunately, these overlap the Linux non-standard values, so they
 * must not be used in the same context.
 */
#define AT_BRK		10	/* Starting point for sbrk and brk. */
#define AT_DEBUG	11	/* Debugging level. */

/*
 * The following non-standard values are used in Linux ELF binaries.
 */
#define AT_NOTELF	10	/* Program is not ELF ?? */
#define AT_UID		11	/* Real uid. */
#define AT_EUID		12	/* Effective uid. */
#define AT_GID		13	/* Real gid. */
#define AT_EGID		14	/* Effective gid. */

#define AT_COUNT	15	/* Count of defined aux entry types. */

/*
 * Relocation types.
 */

#define	R_ALPHA_NONE		0	/* No reloc */
#define	R_ALPHA_REFLONG		1	/* Direct 32 bit */
#define	R_ALPHA_REFQUAD		2	/* Direct 64 bit */
#define	R_ALPHA_GPREL32		3	/* GP relative 32 bit */
#define	R_ALPHA_LITERAL		4	/* GP relative 16 bit w/optimization */
#define	R_ALPHA_LITUSE		5	/* Optimization hint for LITERAL */
#define	R_ALPHA_GPDISP		6	/* Add displacement to GP */
#define	R_ALPHA_BRADDR		7	/* PC+4 relative 23 bit shifted */
#define	R_ALPHA_HINT		8	/* PC+4 relative 16 bit shifted */
#define	R_ALPHA_SREL16		9	/* PC relative 16 bit */
#define	R_ALPHA_SREL32		10	/* PC relative 32 bit */
#define	R_ALPHA_SREL64		11	/* PC relative 64 bit */
#define	R_ALPHA_OP_PUSH		12	/* OP stack push */
#define	R_ALPHA_OP_STORE	13	/* OP stack pop and store */
#define	R_ALPHA_OP_PSUB		14	/* OP stack subtract */
#define	R_ALPHA_OP_PRSHIFT	15	/* OP stack right shift */
#define	R_ALPHA_GPVALUE		16
#define	R_ALPHA_GPRELHIGH	17
#define	R_ALPHA_GPRELLOW	18
#define	R_ALPHA_IMMED_GP_16	19
#define	R_ALPHA_IMMED_GP_HI32	20
#define	R_ALPHA_IMMED_SCN_HI32	21
#define	R_ALPHA_IMMED_BR_HI32	22
#define	R_ALPHA_IMMED_LO32	23
#define	R_ALPHA_COPY		24	/* Copy symbol at runtime */
#define	R_ALPHA_GLOB_DAT	25	/* Create GOT entry */
#define	R_ALPHA_JMP_SLOT	26	/* Create PLT entry */
#define	R_ALPHA_RELATIVE	27	/* Adjust by program base */

#define R_ALPHA_COUNT		28

/* Define "machine" characteristics */
#define ELF_TARG_CLASS	ELFCLASS64
#define ELF_TARG_DATA	ELFDATA2LSB
#define ELF_TARG_MACH	EM_ALPHA
#define ELF_TARG_VER	1

#ifdef _KERNEL

/*
 * On the Alpha we load the dynamic linker where a userland call
 * to mmap(0, ...) would put it.  The rationale behind this
 * calculation is that it leaves room for the heap to grow to
 * its maximum allowed size.
 */
#define ELF_RTLD_ADDR(vmspace) \
    (round_page((vm_offset_t)(vmspace)->vm_daddr + MAXDSIZ))

#endif
#endif /* !_MACHINE_ELF_H_ */
