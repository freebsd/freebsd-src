/* MIPS ELF support for BFD.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.

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

/* Four bit MIPS architecture field.  */
#define EF_MIPS_ARCH		0xf0000000

/* -mips1 code.  */
#define E_MIPS_ARCH_1		0x00000000

/* -mips2 code.  */
#define E_MIPS_ARCH_2		0x10000000

/* -mips3 code.  */
#define E_MIPS_ARCH_3		0x20000000

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

/* Section contains miscellaneous options (used on Irix).  */
#define SHT_MIPS_OPTIONS	0x7000000d

/* DWARF debugging section (used on Irix 6).  */
#define SHT_MIPS_DWARF		0x7000001e

/* Events section.  This appears on Irix 6.  I don't know what it
   means.  */
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

#endif /* _ELF_MIPS_H */
