/*-
 * Copyright (c) 1995-1996 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: imgact_elf.h,v 1.1 1996/03/10 08:42:52 sos Exp $
 */

#ifndef _IMGACT_ELF_H_
#define _IMGACT_ELF_H_

typedef u_int32_t Elf32_Addr;
typedef u_int32_t Elf32_Off;
typedef int32_t   Elf32_Sword;
typedef u_int32_t Elf32_Word;
typedef u_int16_t Elf32_Half;
 
extern int elf_trace;

#define EI_NINDENT	16
typedef struct {
        unsigned char   e_ident[EI_NINDENT];    /* file id */
        Elf32_Half      e_type;                 /* type */
        Elf32_Half      e_machine;              /* machine type */
        Elf32_Word      e_version;              /* version number */
        Elf32_Addr      e_entry;                /* entry point */
        Elf32_Off       e_phoff;                /* program hdr offset */
        Elf32_Off       e_shoff;                /* section hdr offset */
        Elf32_Word      e_flags;                /* flags */
        Elf32_Half      e_ehsize;               /* sizeof ehdr */
        Elf32_Half      e_phentsize;            /* program header entry size */
        Elf32_Half      e_phnum;                /* number of program headers */
        Elf32_Half      e_shentsize;            /* section header entry size */
        Elf32_Half      e_shnum;                /* number of section headers */
        Elf32_Half      e_shstrndx;             /* string table index */
} Elf32_Ehdr;

/*
 * Values for e_indent entry in struct Elf32_Ehdr.
 */
#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_SPARE	8
#define EI_BRAND	8


#define ELFMAG0		'\177'
#define ELFMAG1		'E'
#define ELFMAG2		'L'
#define ELFMAG3		'F'
#define ELFCLASSNONE	0	/* invalid class */
#define ELFCLASS32	1	/* 32bit object class */
#define ELFCLASS64	2	/* 64bit object class */
#define ELFDATANONE	0	/* invalid data encoding */
#define ELFDATA2LSB	1	/* little endian */
#define ELFDATA2MSB	2	/* big endian */

/*
 * Values for e_version entry in struct Elf32_Ehdr.
 */
#define EV_NONE		0	/* invalid version */
#define EV_CURRENT	1	/* current version */

/*
 * Values for e_type entry in struct Elf32_Ehdr.
 */
#define ET_NONE   	0
#define ET_REL    	1
#define ET_EXEC   	2
#define ET_DYN    	3
#define ET_CORE   	4
#define ET_LOPROC 	5
#define ET_HIPROC 	6

/*
 * Values for e_machine entry in struct Elf32_Ehdr.
 */
#define EM_NONE  	0
#define EM_M32   	1
#define EM_SPARC 	2
#define EM_386   	3
#define EM_68K   	4
#define EM_88K   	5
#define EM_486   	6
#define EM_860   	7


typedef struct {
        Elf32_Word      p_type;         /* entry type */
        Elf32_Off       p_offset;       /* offset */
        Elf32_Addr      p_vaddr;        /* virtual address */
        Elf32_Addr      p_paddr;        /* physical address */
        Elf32_Word      p_filesz;       /* file size */
        Elf32_Word      p_memsz;        /* memory size */
        Elf32_Word      p_flags;        /* flags */
        Elf32_Word      p_align;        /* memory & file alignment */
} Elf32_Phdr;

/*
 * Values for p_type entry in struct Elf32_Phdr.
 */
#define PT_NULL    	0
#define PT_LOAD    	1
#define PT_DYNAMIC 	2
#define PT_INTERP  	3
#define PT_NOTE    	4
#define PT_SHLIB   	5
#define PT_PHDR    	6
#define PT_LOPROC  	0x70000000
#define PT_HIPROC  	0x7fffffff

/*
 * Values for p_flags entry in struct Elf32_Phdr.
 */
#define PF_X		0x1
#define PF_W		0x2
#define PF_R		0x4
#define PF_MASKPROC	0xf0000000

/*
 * Auxiliary vector entry on initial stack.
 */
typedef struct {
	Elf32_Sword	a_type;
	Elf32_Word	a_val;
} Elf32_Auxinfo;

#define AUXARGS_ENTRY(pos, id, val) {suword(pos++, id); suword(pos++, val);}

/* 
 * Values for a_type in struct Elf32_Auxinfo.
 */
#define AT_NULL		0	/* Terminates the vector */
#define AT_IGNORE	1	/* Ignored */
#define AT_EXECFD	2	/* File descriptor of program to load */
#define AT_PHDR		3	/* Program header of program already loaded */
#define AT_PHENT	4	/* Size of each program header entry */
#define AT_PHNUM	5	/* Number of program header entries */
#define AT_PAGESZ	6	/* Page size in bytes */
#define AT_BASE		7	/* Interpreter's base address */
#define AT_FLAGS	8	/* Flags (unused for i386) */
#define AT_ENTRY	9	/* Where interpreter should transfer control */

/*
 * The following non-standard values are used for passing information
 * to the (FreeBSD ELF) dynamic linker. Will probably go away soon....
 */
#define AT_BRK		10	/* Starting point for sbrk and brk */
#define AT_DEBUG	11	/* Debugging level */
#define AT_COUNT	15

/*
 * The following non-standard values are used in Linux ELF binaries.
 */
#define AT_NOTELF	10	/* Program is not ELF ?? */
#define AT_UID		11	/* Real uid */
#define AT_EUID		12	/* Effective uid */
#define AT_GID		13	/* Real gid */
#define AT_EGID		14	/* Effective gid */

/*
 * Structure used to pass infomation from the loader to the
 * stack fixup routine.
 */
typedef struct {
	Elf32_Sword	execfd;
	Elf32_Word	phdr;
	Elf32_Word	phent;
	Elf32_Word	phnum;
	Elf32_Word	pagesz;
	Elf32_Word	base;
	Elf32_Word	flags;
	Elf32_Word	entry;
	Elf32_Word	trace;
} Elf32_Auxargs;

typedef struct {
	char *brand;
	char *emul_path;
	char *interp_path;
        struct sysentvec *sysvec;
} Elf32_Brandinfo;

#define MAX_BRANDS      8

int elf_insert_brand_entry __P((Elf32_Brandinfo *entry));
int elf_remove_brand_entry __P((Elf32_Brandinfo *entry));

#endif /* _IMGACT_ELF_H_ */
