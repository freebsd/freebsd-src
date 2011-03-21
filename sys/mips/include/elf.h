/*	$OpenBSD: elf_abi.h,v 1.1 1998/01/28 11:14:41 pefo Exp $ */

/*-
 * Copyright (c) 1996 Per Fogelstrom
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	JNPR: elf.h,v 1.4 2006/12/02 09:53:40 katta
 * $FreeBSD$
 *
 */

#ifndef _MACHINE_ELF_H_
#define	_MACHINE_ELF_H_

/* Information taken from MIPS ABI supplemental */

#ifndef __ELF_WORD_SIZE
#if defined(__mips_n64)
#define	__ELF_WORD_SIZE 64	/* Used by <sys/elf_generic.h> */
#else
#define	__ELF_WORD_SIZE 32	/* Used by <sys/elf_generic.h> */
#endif
#endif
#include <sys/elf32.h>	/* Definitions common to all 32 bit architectures. */
#include <sys/elf64.h>	/* Definitions common to all 64 bit architectures. */
#include <sys/elf_generic.h>

#define	ELF_ARCH	EM_MIPS
#define	ELF_MACHINE_OK(x) ((x) == EM_MIPS || (x) == EM_MIPS_RS4_BE)

/* Architecture dependent Segment types - p_type */
#define	PT_MIPS_REGINFO		0x70000000 /* Register usage information */

/* Architecture dependent d_tag field for Elf32_Dyn.  */
#define	DT_MIPS_RLD_VERSION	0x70000001 /* Runtime Linker Interface ID */
#define	DT_MIPS_TIME_STAMP	0x70000002 /* Timestamp */
#define	DT_MIPS_ICHECKSUM	0x70000003 /* Cksum of ext str and com sizes */
#define	DT_MIPS_IVERSION	0x70000004 /* Version string (string tbl index) */
#define	DT_MIPS_FLAGS		0x70000005 /* Flags */
#define	DT_MIPS_BASE_ADDRESS	0x70000006 /* Segment base address */
#define	DT_MIPS_CONFLICT	0x70000008 /* Adr of .conflict section */
#define	DT_MIPS_LIBLIST		0x70000009 /* Address of .liblist section */
#define	DT_MIPS_LOCAL_GOTNO	0x7000000a /* Number of local .GOT entries */
#define	DT_MIPS_CONFLICTNO	0x7000000b /* Number of .conflict entries */
#define	DT_MIPS_LIBLISTNO	0x70000010 /* Number of .liblist entries */
#define	DT_MIPS_SYMTABNO	0x70000011 /* Number of .dynsym entries */
#define	DT_MIPS_UNREFEXTNO	0x70000012 /* First external DYNSYM */
#define	DT_MIPS_GOTSYM		0x70000013 /* First GOT entry in .dynsym */
#define	DT_MIPS_HIPAGENO	0x70000014 /* Number of GOT page table entries */
#define	DT_MIPS_RLD_MAP		0x70000016 /* Address of debug map pointer */

#define	DT_PROCNUM (DT_MIPS_RLD_MAP - DT_LOPROC + 1)

/*
 * Legal values for e_flags field of Elf32_Ehdr.
 */
#define	EF_MIPS_NOREORDER	1		/* .noreorder was used */
#define	EF_MIPS_PIC		2		/* Contains PIC code */
#define	EF_MIPS_CPIC		4		/* Uses PIC calling sequence */
#define	EF_MIPS_ARCH		0xf0000000	/* MIPS architecture level */

/*
 * Mips special sections.
 */
#define	SHN_MIPS_ACOMMON	0xff00		/* Allocated common symbols */
#define	SHN_MIPS_SCOMMON	0xff03		/* Small common symbols */
#define	SHN_MIPS_SUNDEFINED	0xff04		/* Small undefined symbols */

/*
 * Legal values for sh_type field of Elf32_Shdr.
 */
#define	SHT_MIPS_LIBLIST	0x70000000 /* Shared objects used in link */
#define	SHT_MIPS_CONFLICT	0x70000002 /* Conflicting symbols */
#define	SHT_MIPS_GPTAB		0x70000003 /* Global data area sizes */
#define	SHT_MIPS_UCODE		0x70000004 /* Reserved for SGI/MIPS compilers */
#define	SHT_MIPS_DEBUG		0x70000005 /* MIPS ECOFF debugging information */
#define	SHT_MIPS_REGINFO	0x70000006 /* Register usage information */

/*
 * Legal values for sh_flags field of Elf32_Shdr.
 */
#define	SHF_MIPS_GPREL		0x10000000 /* Must be part of global data area */

/*
 * Entries found in sections of type SHT_MIPS_GPTAB.
 */
typedef union {
	struct {
		Elf32_Word gt_current_g_value;	/* -G val used in compilation */
		Elf32_Word gt_unused;	/* Not used */
	} gt_header;			/* First entry in section */
	struct {
		Elf32_Word gt_g_value;	/* If this val were used for -G */
		Elf32_Word gt_bytes;	/* This many bytes would be used */
	} gt_entry;			/* Subsequent entries in section */
} Elf32_gptab;
typedef union {
	struct {
		Elf64_Word gt_current_g_value;	/* -G val used in compilation */
		Elf64_Word gt_unused;	/* Not used */
	} gt_header;			/* First entry in section */
	struct {
		Elf64_Word gt_g_value;	/* If this val were used for -G */
		Elf64_Word gt_bytes;	/* This many bytes would be used */
	} gt_entry;			/* Subsequent entries in section */
} Elf64_gptab;

/*
 * Entry found in sections of type SHT_MIPS_REGINFO.
 */
typedef struct {
	Elf32_Word	ri_gprmask;	/* General registers used */
	Elf32_Word	ri_cprmask[4];	/* Coprocessor registers used */
	Elf32_Sword	ri_gp_value;	/* $gp register value */
} Elf32_RegInfo;
typedef struct {
	Elf64_Word	ri_gprmask;	/* General registers used */
	Elf64_Word	ri_cprmask[4];	/* Coprocessor registers used */
	Elf64_Sword	ri_gp_value;	/* $gp register value */
} Elf64_RegInfo;


/*
 * Mips relocations.
 */

#define	R_MIPS_NONE	0	/* No reloc */
#define	R_MIPS_16	1	/* Direct 16 bit */
#define	R_MIPS_32	2	/* Direct 32 bit */
#define	R_MIPS_REL32	3	/* PC relative 32 bit */
#define	R_MIPS_26	4	/* Direct 26 bit shifted */
#define	R_MIPS_HI16	5	/* High 16 bit */
#define	R_MIPS_LO16	6	/* Low 16 bit */
#define	R_MIPS_GPREL16	7	/* GP relative 16 bit */
#define	R_MIPS_LITERAL	8	/* 16 bit literal entry */
#define	R_MIPS_GOT16	9	/* 16 bit GOT entry */
#define	R_MIPS_PC16	10	/* PC relative 16 bit */
#define	R_MIPS_CALL16	11	/* 16 bit GOT entry for function */
#define	R_MIPS_GPREL32	12	/* GP relative 32 bit */
#define	R_MIPS_GOTHI16	21	/* GOT HI 16 bit */
#define	R_MIPS_GOTLO16	22	/* GOT LO 16 bit */
#define	R_MIPS_CALLHI16 30	/* upper 16 bit GOT entry for function */
#define	R_MIPS_CALLLO16 31	/* lower 16 bit GOT entry for function */

/*
 * These are from the 64-bit Irix ELF ABI
 */
#define	R_MIPS_SHIFT5	16
#define	R_MIPS_SHIFT6	17
#define	R_MIPS_64	18
#define	R_MIPS_GOT_DISP	19
#define	R_MIPS_GOT_PAGE	20
#define	R_MIPS_GOT_OFST	21
#define	R_MIPS_GOT_HI16	22
#define	R_MIPS_GOT_LO16	23
#define	R_MIPS_SUB	24
#define	R_MIPS_INSERT_A	25
#define	R_MIPS_INSERT_B	26
#define	R_MIPS_DELETE	27
#define	R_MIPS_HIGHER	28
#define	R_MIPS_HIGHEST	29
#define	R_MIPS_SCN_DISP	32
#define	R_MIPS_REL16	33
#define	R_MIPS_ADD_IMMEDIATE 34
#define	R_MIPS_PJUMP	35
#define	R_MIPS_ERLGOT	36

#define	R_MIPS_max	37
#define	R_TYPE(name)		__CONCAT(R_MIPS_,name)

/* Define "machine" characteristics */
#if __ELF_WORD_SIZE == 32
#define	ELF_TARG_CLASS	ELFCLASS32
#else
#define	ELF_TARG_CLASS	ELFCLASS64
#endif
#ifdef __MIPSEB__
#define	ELF_TARG_DATA	ELFDATA2MSB
#else
#define ELF_TARG_DATA	ELFDATA2LSB
#endif
#define	ELF_TARG_MACH	EM_MIPS
#define	ELF_TARG_VER	1

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
		void	*a_ptr;		/* Address. */
		void	(*a_fcn)(void); /* Function pointer (not used). */
	} a_un;
} Elf32_Auxinfo;

typedef struct {	/* Auxiliary vector entry on initial stack */
	long	a_type;			/* Entry type. */
	union {
		long	a_val;		/* Integer value. */
		void	*a_ptr;		/* Address. */
		void	(*a_fcn)(void); /* Function pointer (not used). */
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
#define	AT_CANARY	16	/* Canary for SSP */
#define	AT_CANARYLEN	17	/* Length of the canary. */
#define	AT_OSRELDATE	18	/* OSRELDATE. */
#define	AT_NCPUS	19	/* Number of CPUs. */
#define	AT_PAGESIZES	20	/* Pagesizes. */
#define	AT_PAGESIZESLEN	21	/* Number of pagesizes. */
#define	AT_STACKPROT	23	/* Initial stack protection. */

#define	AT_COUNT	24	/* Count of defined aux entry types. */

#define	ET_DYN_LOAD_ADDR 0x0120000

/*
 * Constant to mark start of symtab/strtab saved by trampoline
 */
#define	SYMTAB_MAGIC	0x64656267

#endif /* !_MACHINE_ELF_H_ */
