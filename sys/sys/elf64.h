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
 *      $Id: elf64.h,v 1.2 1998/06/14 13:24:09 dfr Exp $
 */

#ifndef _SYS_ELF64_H_
#define _SYS_ELF64_H_ 1

/*
 * ELF definitions common to all 64-bit architectures.
 */

typedef u_int64_t	Elf64_Addr;
typedef u_int16_t	Elf64_Half;
typedef u_int64_t	Elf64_Off;
typedef int32_t		Elf64_Sword;
typedef u_int32_t	Elf64_Word;
typedef u_int64_t	Elf64_Size;

/*
 * ELF header.
 */

#define EI_NIDENT	16		/* Size of e_ident array. */

typedef struct {
	unsigned char	e_ident[EI_NIDENT];	/* File identification. */
	Elf64_Half	e_type;		/* File type. */
	Elf64_Half	e_machine;	/* Machine architecture. */
	Elf64_Word	e_version;	/* ELF format version. */
	Elf64_Addr	e_entry;	/* Entry point. */
	Elf64_Off	e_phoff;	/* Program header file offset. */
	Elf64_Off	e_shoff;	/* Section header file offset. */
	Elf64_Word	e_flags;	/* Architecture-specific flags. */
	Elf64_Half	e_ehsize;	/* Size of ELF header in bytes. */
	Elf64_Half	e_phentsize;	/* Size of program header entry. */
	Elf64_Half	e_phnum;	/* Number of program header entries. */
	Elf64_Half	e_shentsize;	/* Size of section header entry. */
	Elf64_Half	e_shnum;	/* Number of section header entries. */
	Elf64_Half	e_shstrndx;	/* Section name strings section. */
} Elf64_Ehdr;

/* Indexes into the e_ident array. */
#define EI_MAG0		0	/* Magic number, byte 0. */
#define EI_MAG1		1	/* Magic number, byte 1. */
#define EI_MAG2		2	/* Magic number, byte 2. */
#define EI_MAG3		3	/* Magic number, byte 3. */
#define EI_CLASS	4	/* Class of machine. */
#define EI_DATA		5	/* Data format. */
#define EI_VERSION	6	/* ELF format version. */
#define EI_PAD		7	/* Start of padding (per SVR4 ABI). */
#define EI_BRAND	8	/* Start of architecture identification. */

/* Values for the magic number bytes. */
#define ELFMAG0		0x7f
#define ELFMAG1		'E'
#define ELFMAG2		'L'
#define ELFMAG3		'F'

/* Values for e_ident[EI_VERSION] and e_version. */
#define EV_NONE		0
#define EV_CURRENT	1

/* Values for e_ident[EI_CLASS]. */
#define ELFCLASSNONE	0	/* Unknown class. */
#define ELFCLASS32	1	/* 32-bit architecture. */
#define ELFCLASS64	2	/* 64-bit architecture. */

/* Values for e_ident[EI_DATA]. */
#define ELFDATANONE	0	/* Unknown data format. */
#define ELFDATA2LSB	1	/* 2's complement little-endian. */
#define ELFDATA2MSB	2	/* 2's complement big-endian. */

/* e_ident */
#define IS_ELF(ehdr)	((ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
			 (ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
			 (ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
			 (ehdr).e_ident[EI_MAG3] == ELFMAG3)

/* Values for e_type. */
#define ET_NONE		0	/* Unknown type. */
#define ET_REL		1	/* Relocatable. */
#define ET_EXEC		2	/* Executable. */
#define ET_DYN		3	/* Shared object. */
#define ET_CORE		4	/* Core file. */

/* Values for e_machine. */
#define EM_NONE		0	/* Unknown machine. */
#define EM_M64		1	/* AT&T WE64100. */
#define EM_SPARC	2	/* Sun SPARC. */
#define EM_386		3	/* Intel i386. */
#define EM_68K		4	/* Motorola 68000. */
#define EM_88K		5	/* Motorola 88000. */
#define EM_486		6	/* Intel i486. */
#define EM_860		7	/* Intel i860. */
#define EM_MIPS		8	/* MIPS R3000 Big-Endian only */

/* Extensions */
#define EM_MIPS_RS4_BE	10	/* MIPS R4000 Big-Endian */
#define EM_SPARC64	11	/* SPARC v9 64-bit unoffical */
#define EM_PARISC	15	/* HPPA */
#define EM_PPC		20	/* PowerPC */
#define EM_ALPHA	0x9026	/* Alpha */


/*
 * Section header.
 */

typedef struct {
	Elf64_Word	sh_name;	/* Section name (index into the
					   section header string table). */
	Elf64_Word	sh_type;	/* Section type. */
	Elf64_Size	sh_flags;	/* Section flags. */
	Elf64_Addr	sh_addr;	/* Address in memory image. */
	Elf64_Off	sh_offset;	/* Offset in file. */
	Elf64_Size	sh_size;	/* Size in bytes. */
	Elf64_Word	sh_link;	/* Index of a related section. */
	Elf64_Word	sh_info;	/* Depends on section type. */
	Elf64_Size	sh_addralign;	/* Alignment in bytes. */
	Elf64_Size	sh_entsize;	/* Size of each entry in section. */
} Elf64_Shdr;

/* Special section indexes. */
#define SHN_UNDEF	     0		/* Undefined, missing, irrelevant. */
#define SHN_LORESERVE	0xff00		/* First of reserved range. */
#define SHN_LOPROC	0xff00		/* First processor-specific. */
#define SHN_HIPROC	0xff1f		/* Last processor-specific. */
#define SHN_ABS		0xfff1		/* Absolute values. */
#define SHN_COMMON	0xfff2		/* Common data. */
#define SHN_HIRESERVE	0xffff		/* Last of reserved range. */

/* sh_type */
#define SHT_NULL	0		/* inactive */
#define SHT_PROGBITS	1		/* program defined information */
#define SHT_SYMTAB	2		/* symbol table section */
#define SHT_STRTAB	3		/* string table section */
#define SHT_RELA	4		/* relocation section with addends*/
#define SHT_HASH	5		/* symbol hash table section */
#define SHT_DYNAMIC	6		/* dynamic section */ 
#define SHT_NOTE	7		/* note section */
#define SHT_NOBITS	8		/* no space section */
#define SHT_REL		9		/* relation section without addends */
#define SHT_SHLIB	10		/* reserved - purpose unknown */
#define SHT_DYNSYM	11		/* dynamic symbol table section */ 
#define SHT_LOPROC	0x70000000	/* reserved range for processor */
#define SHT_HIPROC	0x7fffffff	/* specific section header types */
#define SHT_LOUSER	0x80000000	/* reserved range for application */
#define SHT_HIUSER	0xffffffff	/* specific indexes */


/*
 * Program header.
 */

typedef struct {
	Elf64_Word	p_type;		/* Entry type. */
	Elf64_Word	p_flags;	/* Access permission flags. */
	Elf64_Off	p_offset;	/* File offset of contents. */
	Elf64_Addr	p_vaddr;	/* Virtual address in memory image. */
	Elf64_Addr	p_paddr;	/* Physical address (not used). */
	Elf64_Size	p_filesz;	/* Size of contents in file. */
	Elf64_Size	p_memsz;	/* Size of contents in memory. */
	Elf64_Size	p_align;	/* Alignment in memory and file. */
} Elf64_Phdr;

/* Values for p_type. */
#define PT_NULL		0	/* Unused entry. */
#define PT_LOAD		1	/* Loadable segment. */
#define PT_DYNAMIC	2	/* Dynamic linking information segment. */
#define PT_INTERP	3	/* Pathname of interpreter. */
#define PT_NOTE		4	/* Auxiliary information. */
#define PT_SHLIB	5	/* Reserved (not used). */
#define PT_PHDR		6	/* Location of program header itself. */

#define PT_COUNT	7	/* Number of defined p_type values. */

#define PT_LOPROC	0x70000000	/* First processor-specific type. */
#define PT_HIPROC	0x7fffffff	/* Last processor-specific type. */

/* Values for p_flags. */
#define PF_X		0x1	/* Executable. */
#define PF_W		0x2	/* Writable. */
#define PF_R		0x4	/* Readable. */

/*
 * Dynamic structure.  The ".dynamic" section contains an array of them.
 */

typedef struct {
	Elf64_Size	d_tag;		/* Entry type. */
	union {
		Elf64_Size	d_val;	/* Integer value. */
		Elf64_Addr	d_ptr;	/* Address value. */
	} d_un;
} Elf64_Dyn;

/* Values for d_tag. */
#define DT_NULL		0	/* Terminating entry. */
#define DT_NEEDED	1	/* String table offset of a needed shared
				   library. */
#define DT_PLTRELSZ	2	/* Total size in bytes of PLT relocations. */
#define DT_PLTGOT	3	/* Processor-dependent address. */
#define DT_HASH		4	/* Address of symbol hash table. */
#define DT_STRTAB	5	/* Address of string table. */
#define DT_SYMTAB	6	/* Address of symbol table. */
#define DT_RELA		7	/* Address of Elf64_Rela relocations. */
#define DT_RELASZ	8	/* Total size of Elf64_Rela relocations. */
#define DT_RELAENT	9	/* Size of each Elf64_Rela relocation entry. */
#define DT_STRSZ	10	/* Size of string table. */
#define DT_SYMENT	11	/* Size of each symbol table entry. */
#define DT_INIT		12	/* Address of initialization function. */
#define DT_FINI		13	/* Address of finalization function. */
#define DT_SONAME	14	/* String table offset of shared object
				   name. */
#define DT_RPATH	15	/* String table offset of library path. */
#define DT_SYMBOLIC	16	/* Indicates "symbolic" linking. */
#define DT_REL		17	/* Address of Elf64_Rel relocations. */
#define DT_RELSZ	18	/* Total size of Elf64_Rel relocations. */
#define DT_RELENT	19	/* Size of each Elf64_Rel relocation. */
#define DT_PLTREL	20	/* Type of relocation used for PLT. */
#define DT_DEBUG	21	/* Reserved (not used). */
#define DT_TEXTREL	22	/* Indicates there may be relocations in
				   non-writable segments. */
#define DT_JMPREL	23	/* Address of PLT relocations. */

#define DT_COUNT	24	/* Number of defined d_tag values. */

/*
 * Relocation entries.
 */

/* Relocations that don't need an addend field. */
typedef struct {
	Elf64_Addr	r_offset;	/* Location to be relocated. */
	Elf64_Size	r_info;		/* Relocation type and symbol index. */
} Elf64_Rel;

/* Relocations that need an addend field. */
typedef struct {
	Elf64_Addr	r_offset;	/* Location to be relocated. */
	Elf64_Size	r_info;		/* Relocation type and symbol index. */
	Elf64_Off	r_addend;	/* Addend. */
} Elf64_Rela;

/* Macros for accessing the fields of r_info. */
#define ELF64_R_SYM(info)	((info) >> 8)
#define ELF64_R_TYPE(info)	((unsigned char)(info))

/* Macro for constructing r_info from field values. */
#define ELF64_R_INFO(sym, type)	(((sym) << 8) + (unsigned char)(type))

/*
 * Symbol table entries.
 */

typedef struct {
	Elf64_Word	st_name;	/* String table index of name. */
	unsigned char	st_info;	/* Type and binding information. */
	unsigned char	st_other;	/* Reserved (not used). */
	Elf64_Half	st_shndx;	/* Section index of symbol. */
	Elf64_Addr	st_value;	/* Symbol value. */
	Elf64_Size	st_size;	/* Size of associated object. */
} Elf64_Sym;

/* Macros for accessing the fields of st_info. */
#define ELF64_ST_BIND(info)		((info) >> 4)
#define ELF64_ST_TYPE(info)		((info) & 0xf)

/* Macro for constructing st_info from field values. */
#define ELF64_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))

/* Symbol Binding - ELF64_ST_BIND - st_info */
#define STB_LOCAL	0	/* Local symbol */
#define STB_GLOBAL	1	/* Global symbol */
#define STB_WEAK	2	/* like global - lower precedence */
#define STB_LOPROC	13	/* reserved range for processor */
#define STB_HIPROC	15	/*  specific symbol bindings */

/* Symbol type - ELF64_ST_TYPE - st_info */
#define STT_NOTYPE	0	/* Unspecified type. */
#define STT_OBJECT	1	/* Data object. */
#define STT_FUNC	2	/* Function. */
#define STT_SECTION	3	/* Section. */
#define STT_FILE	4	/* Source file. */
#define STT_LOPROC	13	/* reserved range for processor */
#define STT_HIPROC	15	/*  specific symbol types */

/* Special symbol table indexes. */
#define STN_UNDEF	0	/* Undefined symbol index. */

#endif /* !_SYS_ELF64_H_ */
