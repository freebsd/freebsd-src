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

#ifndef _MACHINE_ELF_H_
#define	_MACHINE_ELF_H_ 1

/*
 * ELF definitions for the IA-64 architecture.
 */

#ifndef __ELF_WORD_SIZE
#define	__ELF_WORD_SIZE	64
#endif

#include <sys/elf64.h>	/* Definitions common to all 64 bit architectures. */
#include <sys/elf32.h>	/* Definitions common to all 32 bit architectures. */

#include <sys/elf_generic.h>

#define	ELF_ARCH	EM_IA_64

#define	ELF_MACHINE_OK(x) ((x) == EM_IA_64)

/*
 * Auxiliary vector entries for passing information to the interpreter.
 *
 * The i386 supplement to the SVR4 ABI specification names this "auxv_t",
 * but POSIX lays claim to all symbols ending with "_t".
 */

typedef struct {	/* Auxiliary vector entry on initial stack */
	int	a_type;			/* Entry type. */
	union {
		int	a_val;		/* Integer value. */
	} a_un;
} Elf32_Auxinfo;

typedef struct {	/* Auxiliary vector entry on initial stack */
	int	a_type;			/* Entry type. */
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
#define	AT_FLAGS	8	/* Flags (unused for i386). */
#define	AT_ENTRY	9	/* Where interpreter should transfer control. */
#define	AT_NOTELF	10	/* Program is not ELF ?? */
#define	AT_UID		11	/* Real uid. */
#define	AT_EUID		12	/* Effective uid. */
#define	AT_GID		13	/* Real gid. */
#define	AT_EGID		14	/* Effective gid. */
#define	AT_EXECPATH	15	/* Path to the executable. */

#define	AT_COUNT	16	/* Count of defined aux entry types. */

/*
 * Values for e_flags.
 */
#define	EF_IA_64_MASKOS		0x00ff000f
#define	EF_IA_64_ABI64		0x00000010
#define	EF_IA_64_REDUCEDFP	0x00000020
#define	EF_IA_64_CONS_GP	0x00000040
#define	EF_IA_64_NOFUNCDESC_CONS_GP 0x00000080
#define	EF_IA_64_ABSOLUTE	0x00000100
#define	EF_IA_64_ARCH		0xff000000

/*
 * Segment types.
 */
#define	PT_IA_64_ARCHEXT	0x70000000
#define	PT_IA_64_UNWIND		0x70000001

/*
 * Segment attributes.
 */
#define	PF_IA_64_NORECOV	0x80000000

/*
 * Section types.
 */
#define	SHT_IA_64_EXT		0x70000000
#define	SHT_IA_64_UNWIND	0x70000001
#define	SHT_IA_64_LOPSREG	0x78000000
#define	SHT_IA_64_HIPSREG	0x7fffffff

/*
 * Section attribute flags.
 */
#define	SHF_IA_64_SHORT		0x10000000
#define	SHF_IA_64_NORECOV	0x20000000

/* Define "machine" characteristics */
#if __ELF_WORD_SIZE == 32
#define	ELF_TARG_CLASS	ELFCLASS32
#else
#define	ELF_TARG_CLASS	ELFCLASS64
#endif
#define	ELF_TARG_DATA	ELFDATA2LSB
#define	ELF_TARG_MACH	EM_IA_64
#define	ELF_TARG_VER	1

/* Processor specific dynmamic section tags. */

#define	DT_IA_64_PLT_RESERVE	0x70000000

#endif /* !_MACHINE_ELF_H_ */
