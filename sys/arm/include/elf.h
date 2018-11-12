/*-
 * Copyright (c) 2001 David E. O'Brien
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
 * EABI ELF definitions for the StrongARM architecture.
 * See "ARM ELF", document no. `SWS ESPC 0003 A-08' for details.
 */

#include <sys/elf32.h>	/* Definitions common to all 32 bit architectures. */

#define	__ELF_WORD_SIZE	32	/* Used by <sys/elf_generic.h> */
#include <sys/elf_generic.h>

typedef struct {        /* Auxiliary vector entry on initial stack */
	int     a_type;                 /* Entry type. */
	union {
		long    a_val;          /* Integer value. */
		void    *a_ptr;         /* Address. */
		void    (*a_fcn)(void); /* Function pointer (not used). */
	} a_un;
} Elf32_Auxinfo;

__ElfType(Auxinfo);

#define	ELF_ARCH	EM_ARM

#define	ELF_MACHINE_OK(x) ((x) == EM_ARM)

/* Unwind info section type */
#define	PT_ARM_EXIDX (PT_LOPROC + 1)

/*
 * Relocation types.
 */

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
#define	AT_NOTELF	10	/* Program is not ELF ?? */
#define	AT_UID		11	/* Real uid. */
#define	AT_EUID		12	/* Effective uid. */
#define	AT_GID		13	/* Real gid. */
#define	AT_EGID		14	/* Effective gid. */
#define	AT_EXECPATH	15	/* Path to the executable. */
#define	AT_CANARY	16	/* Canary for SSP */
#define	AT_CANARYLEN	17	/* Length of the canary. */
#define	AT_OSRELDATE	18	/* OSRELDATE. */
#define	AT_NCPUS	19	/* Number of CPUs. */
#define	AT_PAGESIZES	20	/* Pagesizes. */
#define	AT_PAGESIZESLEN	21	/* Number of pagesizes. */
#define	AT_TIMEKEEP	22	/* Pointer to timehands. */
#define	AT_STACKPROT	23	/* Initial stack protection. */
#define	AT_EHDRFLAGS	24	/* e_flags field from elf hdr */

#define	AT_COUNT	25	/* Count of defined aux entry types. */

#define	R_ARM_COUNT	33	/* Count of defined relocation types. */


/* Define "machine" characteristics */
#define	ELF_TARG_CLASS	ELFCLASS32
#ifdef __ARMEB__
#define	ELF_TARG_DATA	ELFDATA2MSB
#else
#define	ELF_TARG_DATA	ELFDATA2LSB
#endif
#define	ELF_TARG_MACH	EM_ARM
#define	ELF_TARG_VER	1

/* Defines specific for arm headers */
#define	EF_ARM_EABI_VERSION(x) (((x) & EF_ARM_EABIMASK) >> 24)
#define	EF_ARM_EABI_VERSION_UNKNOWN 0
#define	EF_ARM_EABI_FREEBSD_MIN 4

/*
 * Magic number for the elf trampoline, chosen wisely to be an immediate
 * value.
 */
#define	MAGIC_TRAMP_NUMBER	0x5c000003

#define	ET_DYN_LOAD_ADDR	0x12000

#endif /* !_MACHINE_ELF_H_ */
