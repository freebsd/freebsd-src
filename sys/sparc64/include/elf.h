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
 * $FreeBSD$
 */

#ifndef	_MACHINE_ELF_H_
#define _MACHINE_ELF_H_ 1

/*
 * ELF definitions for the sparc64 architecture.
 */

#include <sys/elf32.h>	/* Definitions common to all 32 bit architectures. */
#include <sys/elf64.h>	/* Definitions common to all 64 bit architectures. */

#ifndef __ELF_WORD_SIZE
#define __ELF_WORD_SIZE	64	/* Used by <sys/elf_generic.h> */
#endif
#include <sys/elf_generic.h>

#define	ELF_ARCH	EM_SPARCV9

#define	ELF_MACHINE_OK(x)	((x) == ELF_ARCH)

/*
 * Auxiliary vector entries for passing information to the interpreter.
 */

typedef struct {	/* Auxiliary vector entry on initial stack */
	int	a_type;			/* Entry type. */
	union {
		int	a_val;		/* Integer value. */
	} a_un;
} Elf32_Auxinfo;

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
#define	AT_NULL		0	/* Terminates the vector. */
#define	AT_IGNORE	1	/* Ignored entry. */
#define	AT_EXECFD	2	/* File descriptor of program to load. */
#define	AT_PHDR		3	/* Program header of program already loaded. */
#define	AT_PHENT	4	/* Size of each program header entry. */
#define	AT_PHNUM	5	/* Number of program header entries. */
#define	AT_PAGESZ	6	/* Page size in bytes. */
#define	AT_BASE		7	/* Interpreter's base address. */
#define	AT_FLAGS	8	/* Flags (unused). */
#define	AT_ENTRY	9	/* Where interpreter should transfer control. */

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
#define	AT_NOTELF	10	/* Program is not ELF ?? */
#define	AT_UID		11	/* Real uid. */
#define	AT_EUID		12	/* Effective uid. */
#define	AT_GID		13	/* Real gid. */
#define	AT_EGID		14	/* Effective gid. */

#define	AT_COUNT	15	/* Count of defined aux entry types. */

/*
 * Relocation types.
 */
#define R_SPARC_NONE		0
#define R_SPARC_8		1
#define R_SPARC_16		2
#define R_SPARC_32		3
#define R_SPARC_DISP8		4
#define R_SPARC_DISP16		5
#define R_SPARC_DISP32		6
#define R_SPARC_WDISP30		7
#define R_SPARC_WDISP22		8
#define R_SPARC_HI22		9
#define R_SPARC_22		10
#define R_SPARC_13		11
#define R_SPARC_LO10		12
#define R_SPARC_GOT10		13
#define R_SPARC_GOT13		14
#define R_SPARC_GOT22		15
#define R_SPARC_PC10		16
#define R_SPARC_PC22		17
#define R_SPARC_WPLT30		18
#define R_SPARC_COPY		19
#define R_SPARC_GLOB_DAT	20
#define R_SPARC_JMP_SLOT	21
#define R_SPARC_RELATIVE	22
#define R_SPARC_UA32		23
#define R_SPARC_PLT32		24
#define R_SPARC_HIPLT22		25
#define R_SPARC_LOPLT10		26
#define R_SPARC_PCPLT32		27
#define R_SPARC_PCPLT22		28
#define R_SPARC_PCPLT10		29
#define R_SPARC_10		30
#define R_SPARC_11		31
#define R_SPARC_64		32
#define R_SPARC_OLO10		33
#define R_SPARC_HH22		34
#define R_SPARC_HM10		35
#define R_SPARC_LM22		36
#define R_SPARC_PC_HH22		37
#define R_SPARC_PC_HM10		38
#define R_SPARC_PC_LM22		39
#define R_SPARC_WDISP16		40
#define R_SPARC_WDISP19		41
#define R_SPARC_GLOB_JMP	42
#define R_SPARC_7		43
#define R_SPARC_5		44
#define R_SPARC_6		45
#define	R_SPARC_DISP64		46
#define	R_SPARC_PLT64		47
#define	R_SPARC_HIX22		48
#define	R_SPARC_LOX10		49
#define	R_SPARC_H44		50
#define	R_SPARC_M44		51
#define	R_SPARC_L44		52
#define	R_SPARC_REGISTER	53
#define	R_SPARC_UA64		54
#define	R_SPARC_UA16		55

/* Define "machine" characteristics */
#if __ELF_WORD_SIZE == 32
#define ELF_TARG_CLASS  ELFCLASS32
#else
#define ELF_TARG_CLASS  ELFCLASS64
#endif
#define	ELF_TARG_DATA	ELFDATA2MSB
#define	ELF_TARG_MACH	ELF_ARCH
#define	ELF_TARG_VER	1

#endif /* !_MACHINE_ELF_H_ */
