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
#define __ELF_WORD_SIZE 64
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

/*
 * The following non-standard values are used for passing information
 * from John Polstra's testbed program to the dynamic linker.  These
 * are expected to go away soon.
 *
 * Unfortunately, these overlap the Linux non-standard values, so they
 * must not be used in the same context.
 */
#define	AT_BRK		10	/* Starting point for sbrk and brk. */
#define	AT_DEBUG	11	/* Debugging level. */

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

/*
 * Relocation types.
 */

/*	Name			Value	   Field	Calculation */
#define	R_IA64_NONE		0	/* None */
#define	R_IA64_IMM14		0x21	/* immediate14	S + A */
#define	R_IA64_IMM22		0x22	/* immediate22	S + A */
#define	R_IA64_IMM64		0x23	/* immediate64	S + A */
#define	R_IA64_DIR32MSB		0x24	/* word32 MSB	S + A */
#define	R_IA64_DIR32LSB		0x25	/* word32 LSB	S + A */
#define	R_IA64_DIR64MSB		0x26	/* word64 MSB	S + A */
#define	R_IA64_DIR64LSB		0x27	/* word64 LSB	S + A */
#define	R_IA64_GPREL22		0x2a	/* immediate22	@gprel(S + A) */
#define	R_IA64_GPREL64I		0x2b	/* immediate64	@gprel(S + A) */
#define	R_IA64_GPREL64MSB	0x2e	/* word64 MSB	@gprel(S + A) */
#define	R_IA64_GPREL64LSB	0x2f	/* word64 LSB	@gprel(S + A) */
#define	R_IA64_LTOFF22		0x32	/* immediate22	@ltoff(S + A) */
#define	R_IA64_LTOFF64I		0x33	/* immediate64	@ltoff(S + A) */
#define	R_IA64_PLTOFF22		0x3a	/* immediate22	@pltoff(S + A) */
#define	R_IA64_PLTOFF64I	0x3b	/* immediate64	@pltoff(S + A) */
#define	R_IA64_PLTOFF64MSB	0x3e	/* word64 MSB	@pltoff(S + A) */
#define	R_IA64_PLTOFF64LSB	0x3f	/* word64 LSB	@pltoff(S + A) */
#define	R_IA64_FPTR64I		0x43	/* immediate64	@fptr(S + A) */
#define	R_IA64_FPTR32MSB	0x44	/* word32 MSB	@fptr(S + A) */
#define	R_IA64_FPTR32LSB	0x45	/* word32 LSB	@fptr(S + A) */
#define	R_IA64_FPTR64MSB	0x46	/* word64 MSB	@fptr(S + A) */
#define	R_IA64_FPTR64LSB	0x47	/* word64 LSB	@fptr(S + A) */
#define	R_IA64_PCREL21B		0x49	/* immediate21 form1 S + A - P */
#define	R_IA64_PCREL21M		0x4a	/* immediate21 form2 S + A - P */
#define	R_IA64_PCREL21F		0x4b	/* immediate21 form3 S + A - P */
#define	R_IA64_PCREL32MSB	0x4c	/* word32 MSB	S + A - P */
#define	R_IA64_PCREL32LSB	0x4d	/* word32 LSB	S + A - P */
#define	R_IA64_PCREL64MSB	0x4e	/* word64 MSB	S + A - P */
#define	R_IA64_PCREL64LSB	0x4f	/* word64 LSB	S + A - P */
#define	R_IA64_LTOFF_FPTR22	0x52	/* immediate22	@ltoff(@fptr(S + A)) */
#define	R_IA64_LTOFF_FPTR64I	0x53	/* immediate64	@ltoff(@fptr(S + A)) */
#define	R_IA64_LTOFF_FPTR32MSB	0x54	/* word32 MSB	@ltoff(@fptr(S + A)) */
#define	R_IA64_LTOFF_FPTR32LSB	0x55	/* word32 LSB	@ltoff(@fptr(S + A)) */
#define	R_IA64_LTOFF_FPTR64MSB	0x56	/* word64 MSB	@ltoff(@fptr(S + A)) */
#define	R_IA64_LTOFF_FPTR64LSB	0x57	/* word64 LSB	@ltoff(@fptr(S + A)) */
#define	R_IA64_SEGREL32MSB	0x5c	/* word32 MSB	@segrel(S + A) */
#define	R_IA64_SEGREL32LSB	0x5d	/* word32 LSB	@segrel(S + A) */
#define	R_IA64_SEGREL64MSB	0x5e	/* word64 MSB	@segrel(S + A) */
#define	R_IA64_SEGREL64LSB	0x5f	/* word64 LSB	@segrel(S + A) */
#define	R_IA64_SECREL32MSB	0x64	/* word32 MSB	@secrel(S + A) */
#define	R_IA64_SECREL32LSB	0x65	/* word32 LSB	@secrel(S + A) */
#define	R_IA64_SECREL64MSB	0x66	/* word64 MSB	@secrel(S + A) */
#define	R_IA64_SECREL64LSB	0x67	/* word64 LSB	@secrel(S + A) */
#define	R_IA64_REL32MSB		0x6c	/* word32 MSB	BD + A */
#define	R_IA64_REL32LSB		0x6d	/* word32 LSB	BD + A */
#define	R_IA64_REL64MSB		0x6e	/* word64 MSB	BD + A */
#define	R_IA64_REL64LSB		0x6f	/* word64 LSB	BD + A */
#define	R_IA64_LTV32MSB		0x74	/* word32 MSB	S + A */
#define	R_IA64_LTV32LSB		0x75	/* word32 LSB	S + A */
#define	R_IA64_LTV64MSB		0x76	/* word64 MSB	S + A */
#define	R_IA64_LTV64LSB		0x77	/* word64 LSB	S + A */
#define	R_IA64_IPLTMSB		0x80	/* function descriptor MSB special */
#define	R_IA64_IPLTLSB		0x81	/* function descriptor LSB speciaal */
#define	R_IA64_SUB		0x85	/* immediate64	A - S */
#define	R_IA64_LTOFF22X		0x86	/* immediate22	special */
#define	R_IA64_LDXMOV		0x87	/* immediate22	special */

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

#define DT_IA64_PLT_RESERVE     0x70000000

#ifdef _KERNEL

/*
 * On the ia64 we load the dynamic linker where a userland call
 * to mmap(0, ...) would put it.  The rationale behind this
 * calculation is that it leaves room for the heap to grow to
 * its maximum allowed size.
 */
#define	ELF_RTLD_ADDR(vmspace) \
    (round_page((vm_offset_t)(vmspace)->vm_daddr + maxdsiz))

#endif /* _KERNEL */
#endif /* !_MACHINE_ELF_H_ */
