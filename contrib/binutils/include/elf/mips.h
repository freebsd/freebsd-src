/* MIPS ELF support for BFD.
   Copyright (C) 1993, 1994, 1995, 1996 Free Software Foundation, Inc.

   By Ian Lance Taylor, Cygnus Support, <ian@cygnus.com>, from
   information in the System V Application Binary Interface, MIPS
   Processor Supplement.

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

/* This file holds definitions specific to the MIPS ELF ABI.  Note
   that most of this is not actually implemented by BFD.  */

#ifndef _ELF_MIPS_H
#define _ELF_MIPS_H

/* Processor specific flags for the ELF header e_flags field.  */

/* At least one .noreorder directive appears in the source.  */
#define EF_MIPS_NOREORDER	0x00000001

/* File contains position independent code.  */
#define EF_MIPS_PIC		0x00000002

/* Code in file uses the standard calling sequence for calling
   position independent code.  */
#define EF_MIPS_CPIC		0x00000004

/* Code in file uses new ABI (-n32 on Irix 6).  */
#define EF_MIPS_ABI2		0x00000020

/* Four bit MIPS architecture field.  */
#define EF_MIPS_ARCH		0xf0000000

/* -mips1 code.  */
#define E_MIPS_ARCH_1		0x00000000

/* -mips2 code.  */
#define E_MIPS_ARCH_2		0x10000000

/* -mips3 code.  */
#define E_MIPS_ARCH_3		0x20000000

/* -mips4 code.  */
#define E_MIPS_ARCH_4		0x30000000

/* Processor specific section indices.  These sections do not actually
   exist.  Symbols with a st_shndx field corresponding to one of these
   values have a special meaning.  */

/* Defined and allocated common symbol.  Value is virtual address.  If
   relocated, alignment must be preserved.  */
#define SHN_MIPS_ACOMMON	0xff00

/* Defined and allocated text symbol.  Value is virtual address.
   Occur in the dynamic symbol table of Alpha OSF/1 and Irix 5 executables.  */
#define SHN_MIPS_TEXT		0xff01

/* Defined and allocated data symbol.  Value is virtual address.
   Occur in the dynamic symbol table of Alpha OSF/1 and Irix 5 executables.  */
#define SHN_MIPS_DATA		0xff02

/* Small common symbol.  */
#define SHN_MIPS_SCOMMON	0xff03

/* Small undefined symbol.  */
#define SHN_MIPS_SUNDEFINED	0xff04

/* Processor specific section types.  */

/* Section contains the set of dynamic shared objects used when
   statically linking.  */
#define SHT_MIPS_LIBLIST	0x70000000

/* I'm not sure what this is, but it's used on Irix 5.  */
#define SHT_MIPS_MSYM		0x70000001

/* Section contains list of symbols whose definitions conflict with
   symbols defined in shared objects.  */
#define SHT_MIPS_CONFLICT	0x70000002

/* Section contains the global pointer table.  */
#define SHT_MIPS_GPTAB		0x70000003

/* Section contains microcode information.  The exact format is
   unspecified.  */
#define SHT_MIPS_UCODE		0x70000004

/* Section contains some sort of debugging information.  The exact
   format is unspecified.  It's probably ECOFF symbols.  */
#define SHT_MIPS_DEBUG		0x70000005

/* Section contains register usage information.  */
#define SHT_MIPS_REGINFO	0x70000006

/* Section contains interface information.  */
#define SHT_MIPS_IFACE		0x7000000b

/* Section contains description of contents of another section.  */
#define SHT_MIPS_CONTENT	0x7000000c

/* Section contains miscellaneous options.  */
#define SHT_MIPS_OPTIONS	0x7000000d

/* DWARF debugging section.  */
#define SHT_MIPS_DWARF		0x7000001e

/* I'm not sure what this is, but it appears on Irix 6.  */
#define SHT_MIPS_SYMBOL_LIB	0x70000020

/* Events section.  */
#define SHT_MIPS_EVENTS		0x70000021

/* A section of type SHT_MIPS_LIBLIST contains an array of the
   following structure.  The sh_link field is the section index of the
   string table.  The sh_info field is the number of entries in the
   section.  */
typedef struct
{
  /* String table index for name of shared object.  */
  unsigned long l_name;
  /* Time stamp.  */
  unsigned long l_time_stamp;
  /* Checksum of symbol names and common sizes.  */
  unsigned long l_checksum;
  /* String table index for version.  */
  unsigned long l_version;
  /* Flags.  */
  unsigned long l_flags;
} Elf32_Lib;

/* The l_flags field of an Elf32_Lib structure may contain the
   following flags.  */

/* Require an exact match at runtime.  */
#define LL_EXACT_MATCH		0x00000001

/* Ignore version incompatibilities at runtime.  */
#define LL_IGNORE_INT_VER	0x00000002

/* A section of type SHT_MIPS_CONFLICT is an array of indices into the
   .dynsym section.  Each element has the following type.  */
typedef unsigned long Elf32_Conflict;

/* A section of type SHT_MIPS_GPTAB contains information about how
   much GP space would be required for different -G arguments.  This
   information is only used so that the linker can provide informative
   suggestions as to the best -G value to use.  The sh_info field is
   the index of the section for which this information applies.  The
   contents of the section are an array of the following union.  The
   first element uses the gt_header field.  The remaining elements use
   the gt_entry field.  */
typedef union
{
  struct
    {
      /* -G value actually used for this object file.  */
      unsigned long gt_current_g_value;
      /* Unused.  */
      unsigned long gt_unused;
    } gt_header;
  struct
    {
      /* If this -G argument has been used...  */
      unsigned long gt_g_value;
      /* ...this many GP section bytes would be required.  */
      unsigned long gt_bytes;
    } gt_entry;
} Elf32_gptab;

/* The external version of Elf32_gptab.  */

typedef union
{
  struct
    {
      unsigned char gt_current_g_value[4];
      unsigned char gt_unused[4];
    } gt_header;
  struct
    {
      unsigned char gt_g_value[4];
      unsigned char gt_bytes[4];
    } gt_entry;
} Elf32_External_gptab;

/* A section of type SHT_MIPS_REGINFO contains the following
   structure.  */
typedef struct
{
  /* Mask of general purpose registers used.  */
  unsigned long ri_gprmask;
  /* Mask of co-processor registers used.  */
  unsigned long ri_cprmask[4];
  /* GP register value for this object file.  */
  long ri_gp_value;
} Elf32_RegInfo;

/* The external version of the Elf_RegInfo structure.  */
typedef struct
{
  unsigned char ri_gprmask[4];
  unsigned char ri_cprmask[4][4];
  unsigned char ri_gp_value[4];
} Elf32_External_RegInfo;

/* MIPS ELF .reginfo swapping routines.  */
extern void bfd_mips_elf32_swap_reginfo_in
  PARAMS ((bfd *, const Elf32_External_RegInfo *, Elf32_RegInfo *));
extern void bfd_mips_elf32_swap_reginfo_out
  PARAMS ((bfd *, const Elf32_RegInfo *, Elf32_External_RegInfo *));

/* Processor specific section flags.  */

/* This section must be in the global data area.  */
#define SHF_MIPS_GPREL		0x10000000

/* This section should be merged.  */
#define SHF_MIPS_MERGE		0x20000000

/* This section contains 32 bit addresses.  */
#define SHF_MIPS_ADDR32		0x40000000

/* This section contains 64 bit addresses.  */
#define SHF_MIPS_ADDR64		0x80000000

/* This section may not be stripped.  */
#define SHF_MIPS_NOSTRIP	0x08000000

/* This section is local to threads.  */
#define SHF_MIPS_LOCAL		0x04000000

/* Linker should generate implicit weak names for this section.  */
#define SHF_MIPS_NAMES		0x02000000

/* Processor specific program header types.  */

/* Register usage information.  Identifies one .reginfo section.  */
#define PT_MIPS_REGINFO		0x70000000

/* Runtime procedure table.  */
#define PT_MIPS_RTPROC		0x70000001

/* Processor specific dynamic array tags.  */

/* 32 bit version number for runtime linker interface.  */
#define DT_MIPS_RLD_VERSION	0x70000001

/* Time stamp.  */
#define DT_MIPS_TIME_STAMP	0x70000002

/* Checksum of external strings and common sizes.  */
#define DT_MIPS_ICHECKSUM	0x70000003

/* Index of version string in string table.  */
#define DT_MIPS_IVERSION	0x70000004

/* 32 bits of flags.  */
#define DT_MIPS_FLAGS		0x70000005

/* Base address of the segment.  */
#define DT_MIPS_BASE_ADDRESS	0x70000006

/* Address of .conflict section.  */
#define DT_MIPS_CONFLICT	0x70000008

/* Address of .liblist section.  */
#define DT_MIPS_LIBLIST		0x70000009

/* Number of local global offset table entries.  */
#define DT_MIPS_LOCAL_GOTNO	0x7000000a

/* Number of entries in the .conflict section.  */
#define DT_MIPS_CONFLICTNO	0x7000000b

/* Number of entries in the .liblist section.  */
#define DT_MIPS_LIBLISTNO	0x70000010

/* Number of entries in the .dynsym section.  */
#define DT_MIPS_SYMTABNO	0x70000011

/* Index of first external dynamic symbol not referenced locally.  */
#define DT_MIPS_UNREFEXTNO	0x70000012

/* Index of first dynamic symbol in global offset table.  */
#define DT_MIPS_GOTSYM		0x70000013

/* Number of page table entries in global offset table.  */
#define DT_MIPS_HIPAGENO	0x70000014

/* Address of run time loader map, used for debugging.  */
#define DT_MIPS_RLD_MAP		0x70000016

/* Flags which may appear in a DT_MIPS_FLAGS entry.  */

/* No flags.  */
#define RHF_NONE		0x00000000

/* Uses shortcut pointers.  */
#define RHF_QUICKSTART		0x00000001

/* Hash size is not a power of two.  */
#define RHF_NOTPOT		0x00000002

/* Ignore LD_LIBRARY_PATH.  */
#define RHS_NO_LIBRARY_REPLACEMENT \
				0x00000004

/* Special values for the st_other field in the symbol table.  These
   are used in an Irix 5 dynamic symbol table.  */

#define STO_DEFAULT		0x00
#define STO_INTERNAL		0x01
#define STO_HIDDEN		0x02
#define STO_PROTECTED		0x03

/* This value is used for a mips16 .text symbol.  */
#define STO_MIPS16		0xf0

/* The 64-bit MIPS ELF ABI uses an unusual reloc format.  Each
   relocation entry specifies up to three actual relocations, all at
   the same address.  The first relocation which required a symbol
   uses the symbol in the r_sym field.  The second relocation which
   requires a symbol uses the symbol in the r_ssym field.  If all
   three relocations require a symbol, the third one uses a zero
   value.  */

/* An entry in a 64 bit SHT_REL section.  */

typedef struct
{
  /* Address of relocation.  */
  unsigned char r_offset[8];
  /* Symbol index.  */
  unsigned char r_sym[4];
  /* Special symbol.  */
  unsigned char r_ssym[1];
  /* Third relocation.  */
  unsigned char r_type3[1];
  /* Second relocation.  */
  unsigned char r_type2[1];
  /* First relocation.  */
  unsigned char r_type[1];
} Elf64_Mips_External_Rel;

typedef struct
{
  /* Address of relocation.  */
  bfd_vma r_offset;
  /* Symbol index.  */
  unsigned long r_sym;
  /* Special symbol.  */
  unsigned char r_ssym;
  /* Third relocation.  */
  unsigned char r_type3;
  /* Second relocation.  */
  unsigned char r_type2;
  /* First relocation.  */
  unsigned char r_type;
} Elf64_Mips_Internal_Rel;

/* An entry in a 64 bit SHT_RELA section.  */

typedef struct
{
  /* Address of relocation.  */
  unsigned char r_offset[8];
  /* Symbol index.  */
  unsigned char r_sym[4];
  /* Special symbol.  */
  unsigned char r_ssym[1];
  /* Third relocation.  */
  unsigned char r_type3[1];
  /* Second relocation.  */
  unsigned char r_type2[1];
  /* First relocation.  */
  unsigned char r_type[1];
  /* Addend.  */
  unsigned char r_addend[8];
} Elf64_Mips_External_Rela;

typedef struct
{
  /* Address of relocation.  */
  bfd_vma r_offset;
  /* Symbol index.  */
  unsigned long r_sym;
  /* Special symbol.  */
  unsigned char r_ssym;
  /* Third relocation.  */
  unsigned char r_type3;
  /* Second relocation.  */
  unsigned char r_type2;
  /* First relocation.  */
  unsigned char r_type;
  /* Addend.  */
  bfd_signed_vma r_addend;
} Elf64_Mips_Internal_Rela;

/* Values found in the r_ssym field of a relocation entry.  */

/* No relocation.  */
#define RSS_UNDEF	0

/* Value of GP.  */
#define RSS_GP		1

/* Value of GP in object being relocated.  */
#define RSS_GP0		2

/* Address of location being relocated.  */
#define RSS_LOC		3

/* A SHT_MIPS_OPTIONS section contains a series of options, each of
   which starts with this header.  */

typedef struct
{
  /* Type of option.  */
  unsigned char kind[1];
  /* Size of option descriptor, including header.  */
  unsigned char size[1];
  /* Section index of affected section, or 0 for global option.  */
  unsigned char section[2];
  /* Information specific to this kind of option.  */
  unsigned char info[4];
} Elf_External_Options;

typedef struct
{
  /* Type of option.  */
  unsigned char kind;
  /* Size of option descriptor, including header.  */
  unsigned char size;
  /* Section index of affected section, or 0 for global option.  */
  unsigned short section;
  /* Information specific to this kind of option.  */
  unsigned long info;
} Elf_Internal_Options;

/* MIPS ELF option header swapping routines.  */
extern void bfd_mips_elf_swap_options_in
  PARAMS ((bfd *, const Elf_External_Options *, Elf_Internal_Options *));
extern void bfd_mips_elf_swap_options_out
  PARAMS ((bfd *, const Elf_Internal_Options *, Elf_External_Options *));

/* Values which may appear in the kind field of an Elf_Options
   structure.  */

/* Undefined.  */
#define ODK_NULL	0

/* Register usage and GP value.  */
#define ODK_REGINFO	1

/* Exception processing information.  */
#define ODK_EXCEPTIONS	2

/* Section padding information.  */
#define ODK_PAD		3

/* In the 32 bit ABI, an ODK_REGINFO option is just a Elf32_Reginfo
   structure.  In the 64 bit ABI, it is the following structure.  The
   info field of the options header is not used.  */

typedef struct
{
  /* Mask of general purpose registers used.  */
  unsigned char ri_gprmask[4];
  /* Padding.  */
  unsigned char ri_pad[4];
  /* Mask of co-processor registers used.  */
  unsigned char ri_cprmask[4][4];
  /* GP register value for this object file.  */
  unsigned char ri_gp_value[8];
} Elf64_External_RegInfo;

typedef struct
{
  /* Mask of general purpose registers used.  */
  unsigned long ri_gprmask;
  /* Padding.  */
  unsigned long ri_pad;
  /* Mask of co-processor registers used.  */
  unsigned long ri_cprmask[4];
  /* GP register value for this object file.  */
  bfd_vma ri_gp_value;
} Elf64_Internal_RegInfo;

/* MIPS ELF reginfo swapping routines.  */
extern void bfd_mips_elf64_swap_reginfo_in
  PARAMS ((bfd *, const Elf64_External_RegInfo *, Elf64_Internal_RegInfo *));
extern void bfd_mips_elf64_swap_reginfo_out
  PARAMS ((bfd *, const Elf64_Internal_RegInfo *, Elf64_External_RegInfo *));

#endif /* _ELF_MIPS_H */
