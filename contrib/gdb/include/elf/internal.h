/* ELF support for BFD.
   Copyright (C) 1991, 1992, 1993, 1994 Free Software Foundation, Inc.

   Written by Fred Fish @ Cygnus Support, from information published
   in "UNIX System V Release 4, Programmers Guide: ANSI C and
   Programming Support Tools".

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


/* This file is part of ELF support for BFD, and contains the portions
   that describe how ELF is represented internally in the BFD library.
   I.E. it describes the in-memory representation of ELF.  It requires
   the elf-common.h file which contains the portions that are common to
   both the internal and external representations. */
   

/* NOTE that these structures are not kept in the same order as they appear
   in the object file.  In some cases they've been reordered for more optimal
   packing under various circumstances.  */

#ifndef _ELF_INTERNAL_H
#define _ELF_INTERNAL_H

/* ELF Header */

#define EI_NIDENT	16		/* Size of e_ident[] */

typedef struct elf_internal_ehdr {
  unsigned char		e_ident[EI_NIDENT];	/* ELF "magic number" */
  bfd_vma		e_entry;		/* Entry point virtual address */
  bfd_signed_vma	e_phoff;		/* Program header table file offset */
  bfd_signed_vma	e_shoff;		/* Section header table file offset */
  unsigned long		e_version;		/* Identifies object file version */
  unsigned long		e_flags;		/* Processor-specific flags */
  unsigned short	e_type;			/* Identifies object file type */
  unsigned short	e_machine;		/* Specifies required architecture */
  unsigned short	e_ehsize;		/* ELF header size in bytes */
  unsigned short	e_phentsize;		/* Program header table entry size */
  unsigned short	e_phnum;		/* Program header table entry count */
  unsigned short	e_shentsize;		/* Section header table entry size */
  unsigned short	e_shnum;		/* Section header table entry count */
  unsigned short	e_shstrndx;		/* Section header string table index */
} Elf_Internal_Ehdr;

#define elf32_internal_ehdr elf_internal_ehdr
#define Elf32_Internal_Ehdr Elf_Internal_Ehdr
#define elf64_internal_ehdr elf_internal_ehdr
#define Elf64_Internal_Ehdr Elf_Internal_Ehdr

/* Program header */

struct elf_internal_phdr {
  unsigned long	p_type;			/* Identifies program segment type */
  unsigned long	p_flags;		/* Segment flags */
  bfd_vma	p_offset;		/* Segment file offset */
  bfd_vma	p_vaddr;		/* Segment virtual address */
  bfd_vma	p_paddr;		/* Segment physical address */
  bfd_vma	p_filesz;		/* Segment size in file */
  bfd_vma	p_memsz;		/* Segment size in memory */
  bfd_vma	p_align;		/* Segment alignment, file & memory */
};

typedef struct elf_internal_phdr Elf_Internal_Phdr;
#define elf32_internal_phdr elf_internal_phdr
#define Elf32_Internal_Phdr Elf_Internal_Phdr
#define elf64_internal_phdr elf_internal_phdr
#define Elf64_Internal_Phdr Elf_Internal_Phdr

/* Section header */

typedef struct elf_internal_shdr {
  unsigned int	sh_name;		/* Section name, index in string tbl */
  unsigned int	sh_type;		/* Type of section */
  bfd_vma	sh_flags;		/* Miscellaneous section attributes */
  bfd_vma	sh_addr;		/* Section virtual addr at execution */
  bfd_size_type	sh_size;		/* Size of section in bytes */
  bfd_size_type	sh_entsize;		/* Entry size if section holds table */
  unsigned long	sh_link;		/* Index of another section */
  unsigned long	sh_info;		/* Additional section information */
  file_ptr	sh_offset;		/* Section file offset */
  unsigned int	sh_addralign;		/* Section alignment */

  /* The internal rep also has some cached info associated with it. */
  asection *	bfd_section;		/* Associated BFD section.  */
  PTR		contents;		/* Section contents.  */
} Elf_Internal_Shdr;

#define elf32_internal_shdr elf_internal_shdr
#define Elf32_Internal_Shdr Elf_Internal_Shdr
#define elf64_internal_shdr elf_internal_shdr
#define Elf64_Internal_Shdr Elf_Internal_Shdr

/* Symbol table entry */

struct elf_internal_sym {
  bfd_vma	st_value;		/* Value of the symbol */
  bfd_vma	st_size;		/* Associated symbol size */
  unsigned long	st_name;		/* Symbol name, index in string tbl */
  unsigned char	st_info;		/* Type and binding attributes */
  unsigned char	st_other;		/* No defined meaning, 0 */
  unsigned short st_shndx;		/* Associated section index */
};

typedef struct elf_internal_sym Elf_Internal_Sym;

#define elf32_internal_sym elf_internal_sym
#define elf64_internal_sym elf_internal_sym
#define Elf32_Internal_Sym Elf_Internal_Sym
#define Elf64_Internal_Sym Elf_Internal_Sym

/* Note segments */

typedef struct elf_internal_note {
  unsigned long	namesz;			/* Size of entry's owner string */
  unsigned long	descsz;			/* Size of the note descriptor */
  unsigned long	type;			/* Interpretation of the descriptor */
  char		name[1];		/* Start of the name+desc data */
} Elf_Internal_Note;
#define Elf32_Internal_Note	Elf_Internal_Note
#define elf32_internal_note	elf_internal_note

/* Relocation Entries */

typedef struct elf_internal_rel {
  bfd_vma	r_offset;	/* Location at which to apply the action */
  /* This needs to support 64-bit values in elf64.  */
  bfd_vma	r_info;		/* index and type of relocation */
} Elf_Internal_Rel;

#define elf32_internal_rel elf_internal_rel
#define Elf32_Internal_Rel Elf_Internal_Rel
#define elf64_internal_rel elf_internal_rel
#define Elf64_Internal_Rel Elf_Internal_Rel

typedef struct elf_internal_rela {
  bfd_vma	r_offset;	/* Location at which to apply the action */
  bfd_vma	r_info;		/* Index and Type of relocation */
  bfd_signed_vma r_addend;	/* Constant addend used to compute value */
} Elf_Internal_Rela;

#define elf32_internal_rela elf_internal_rela
#define elf64_internal_rela elf_internal_rela
#define Elf32_Internal_Rela Elf_Internal_Rela
#define Elf64_Internal_Rela Elf_Internal_Rela

/* dynamic section structure */

typedef struct elf_internal_dyn {
  /* This needs to support 64-bit values in elf64.  */
  bfd_vma d_tag;		/* entry tag value */
  union {
    /* This needs to support 64-bit values in elf64.  */
    bfd_vma	d_val;
    bfd_vma	d_ptr;
  } d_un;
} Elf_Internal_Dyn;

#define elf32_internal_dyn elf_internal_dyn
#define elf64_internal_dyn elf_internal_dyn
#define Elf32_Internal_Dyn Elf_Internal_Dyn
#define Elf64_Internal_Dyn Elf_Internal_Dyn

/* This structure is used to describe how sections should be assigned
   to program segments.  */

struct elf_segment_map
{
  /* Next program segment.  */
  struct elf_segment_map *next;
  /* Program segment type.  */
  unsigned long p_type;
  /* Program segment flags.  */
  unsigned long p_flags;
  /* Program segment physical address.  */
  bfd_vma p_paddr;
  /* Whether the p_flags field is valid; if not, the flags are based
     on the section flags.  */
  unsigned int p_flags_valid : 1;
  /* Whether the p_paddr field is valid; if not, the physical address
     is based on the section lma values.  */
  unsigned int p_paddr_valid : 1;
  /* Whether this segment includes the file header.  */
  unsigned int includes_filehdr : 1;
  /* Whether this segment includes the program headers.  */
  unsigned int includes_phdrs : 1;
  /* Number of sections (may be 0).  */
  unsigned int count;
  /* Sections.  Actual number of elements is in count field.  */
  asection *sections[1];
};

#endif /* _ELF_INTERNAL_H */
