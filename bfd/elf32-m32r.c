/* M32R-specific support for 32-bit ELF.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/m32r.h"

static bfd_reloc_status_type m32r_elf_10_pcrel_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type m32r_elf_do_10_pcrel_reloc
  PARAMS ((bfd *, reloc_howto_type *, asection *,
	   bfd_byte *, bfd_vma, asection *, bfd_vma, bfd_vma));
static bfd_reloc_status_type m32r_elf_hi16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static void m32r_elf_relocate_hi16
  PARAMS ((bfd *, int, Elf_Internal_Rela *, Elf_Internal_Rela *,
	   bfd_byte *, bfd_vma));
bfd_reloc_status_type m32r_elf_lo16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
bfd_reloc_status_type m32r_elf_generic_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type m32r_elf_sda16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void m32r_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static void m32r_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
bfd_boolean _bfd_m32r_elf_section_from_bfd_section
  PARAMS ((bfd *, asection *, int *));
void _bfd_m32r_elf_symbol_processing
  PARAMS ((bfd *, asymbol *));
static bfd_boolean m32r_elf_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));
static bfd_boolean m32r_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
#if 0 /* not yet */
static bfd_boolean m32r_elf_relax_delete_bytes
  PARAMS ((bfd *, asection *, bfd_vma, int));
#endif
static bfd_reloc_status_type m32r_elf_final_sda_base
  PARAMS ((bfd *, struct bfd_link_info *, const char **, bfd_vma *));
static bfd_boolean m32r_elf_object_p
  PARAMS ((bfd *));
static void m32r_elf_final_write_processing
  PARAMS ((bfd *, bfd_boolean));
static bfd_boolean m32r_elf_set_private_flags
  PARAMS ((bfd *, flagword));
static bfd_boolean m32r_elf_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static bfd_boolean m32r_elf_print_private_bfd_data
  PARAMS ((bfd *, PTR));
static bfd_boolean m32r_elf_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_boolean m32r_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

static bfd_boolean m32r_elf_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static bfd_boolean m32r_elf_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));

asection * m32r_elf_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));

static bfd_boolean m32r_elf_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));

static bfd_boolean m32r_elf_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));

static bfd_boolean m32r_elf_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
           Elf_Internal_Sym *));

static bfd_boolean allocate_dynrelocs
  PARAMS ((struct elf_link_hash_entry *, PTR));
static bfd_boolean readonly_dynrelocs
  PARAMS ((struct elf_link_hash_entry *, PTR));
static enum elf_reloc_type_class m32r_elf_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));
static bfd_boolean m32r_elf_fake_sections
  PARAMS ((bfd *, Elf_Internal_Shdr *, asection *));

#define NOP_INSN		0x7000
#define MAKE_PARALLEL(insn)	((insn) | 0x8000)

/* Use REL instead of RELA to save space.
   This only saves space in libraries and object files, but perhaps
   relocs will be put in ROM?  All in all though, REL relocs are a pain
   to work with.  */
/* #define USE_REL	1

#ifndef USE_REL
#define USE_REL	0
#endif */
/* Use RELA. But use REL to link old objects for backwords compatibility. */

/* Functions for the M32R ELF linker.  */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/libc.so.1"

/* The nop opcode we use.  */

#define M32R_NOP 0x7000f000

#define PLT_EMPTY   0x10101010  /* RIE  -> RIE */

/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 20
#define PLT_HEADER_SIZE 20

/* The first one entries in a procedure linkage table are reserved,
   and the initial contents are unimportant (we zero them out).
   Subsequent entries look like this. */

#define PLT0_ENTRY_WORD0  0xd6c00000    /* seth r6, #high(.got+4)          */
#define PLT0_ENTRY_WORD1  0x86e60000    /* or3  r6, r6, #low(.got)+4)      */
#define PLT0_ENTRY_WORD2  0x24e626c6    /* ld   r4, @r6+    -> ld r6, @r6  */
#define PLT0_ENTRY_WORD3  0x1fc6f000    /* jmp  r6          || pnop        */
#define PLT0_ENTRY_WORD4  PLT_EMPTY     /* RIE             -> RIE          */

#define PLT0_PIC_ENTRY_WORD0  0xa4cc0004 /* ld   r4, @(4,r12)              */
#define PLT0_PIC_ENTRY_WORD1  0xa6cc0008 /* ld   r6, @(8,r12)              */
#define PLT0_PIC_ENTRY_WORD2  0x1fc6f000 /* jmp  r6         || nop         */
#define PLT0_PIC_ENTRY_WORD3  PLT_EMPTY  /* RIE             -> RIE         */
#define PLT0_PIC_ENTRY_WORD4  PLT_EMPTY  /* RIE             -> RIE         */

#define PLT_ENTRY_WORD0  0xe6000000 /* ld24 r6, .name_in_GOT                */
#define PLT_ENTRY_WORD1  0x06acf000 /* add  r6, r12          || nop         */
#define PLT_ENTRY_WORD0b 0xd6c00000 /* seth r6, #high(.name_in_GOT)         */
#define PLT_ENTRY_WORD1b 0x86e60000 /* or3  r6, r6, #low(.name_in_GOT)      */
#define PLT_ENTRY_WORD2  0x26c61fc6 /* ld  r6, @r6           -> jmp r6      */
#define PLT_ENTRY_WORD3  0xe5000000 /* ld24 r5, $offset                     */
#define PLT_ENTRY_WORD4  0xff000000 /* bra  .plt0.                          */


static reloc_howto_type m32r_elf_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_M32R_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_M32R_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 m32r_elf_generic_reloc,/* special_function */
	 "R_M32R_16",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_M32R_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 m32r_elf_generic_reloc,/* special_function */
	 "R_M32R_32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 24 bit address.  */
  HOWTO (R_M32R_24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 m32r_elf_generic_reloc,/* special_function */
	 "R_M32R_24",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An PC Relative 10-bit relocation, shifted by 2.
     This reloc is complicated because relocations are relative to pc & -4.
     i.e. branches in the right insn slot use the address of the left insn
     slot for pc.  */
  /* ??? It's not clear whether this should have partial_inplace set or not.
     Branch relaxing in the assembler can store the addend in the insn,
     and if bfd_install_relocation gets called the addend may get added
     again.  */
  HOWTO (R_M32R_10_PCREL,	/* type */
	 2,	                /* rightshift */
	 1,	                /* size (0 = byte, 1 = short, 2 = long) */
	 10,	                /* bitsize */
	 TRUE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 m32r_elf_10_pcrel_reloc, /* special_function */
	 "R_M32R_10_PCREL",	/* name */
	 FALSE,	                /* partial_inplace */
	 0xff,		        /* src_mask */
	 0xff,   		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A relative 18 bit relocation, right shifted by 2.  */
  HOWTO (R_M32R_18_PCREL,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_18_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A relative 26 bit relocation, right shifted by 2.  */
  /* ??? It's not clear whether this should have partial_inplace set or not.
     Branch relaxing in the assembler can store the addend in the insn,
     and if bfd_install_relocation gets called the addend may get added
     again.  */
  HOWTO (R_M32R_26_PCREL,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_26_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* High 16 bits of address when lower 16 is or'd in.  */
  HOWTO (R_M32R_HI16_ULO,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 m32r_elf_hi16_reloc,	/* special_function */
	 "R_M32R_HI16_ULO",	/* name */
	 TRUE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* High 16 bits of address when lower 16 is added in.  */
  HOWTO (R_M32R_HI16_SLO,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 m32r_elf_hi16_reloc,	/* special_function */
	 "R_M32R_HI16_SLO",	/* name */
	 TRUE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Lower 16 bits of address.  */
  HOWTO (R_M32R_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 m32r_elf_lo16_reloc,	/* special_function */
	 "R_M32R_LO16",		/* name */
	 TRUE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Small data area 16 bits offset.  */
  HOWTO (R_M32R_SDA16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 m32r_elf_sda16_reloc,	/* special_function */
	 "R_M32R_SDA16",	/* name */
	 TRUE,			/* partial_inplace */  /* FIXME: correct? */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_M32R_GNU_VTINHERIT, /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_M32R_GNU_VTINHERIT", /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_M32R_GNU_VTENTRY,     /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_M32R_GNU_VTENTRY",   /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

  EMPTY_HOWTO (13),
  EMPTY_HOWTO (14),
  EMPTY_HOWTO (15),
  EMPTY_HOWTO (16),
  EMPTY_HOWTO (17),
  EMPTY_HOWTO (18),
  EMPTY_HOWTO (19),
  EMPTY_HOWTO (20),
  EMPTY_HOWTO (21),
  EMPTY_HOWTO (22),
  EMPTY_HOWTO (23),
  EMPTY_HOWTO (24),
  EMPTY_HOWTO (25),
  EMPTY_HOWTO (26),
  EMPTY_HOWTO (27),
  EMPTY_HOWTO (28),
  EMPTY_HOWTO (29),
  EMPTY_HOWTO (30),
  EMPTY_HOWTO (31),
  EMPTY_HOWTO (32),

  /* A 16 bit absolute relocation.  */
  HOWTO (R_M32R_16_RELA,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_16_RELA",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_M32R_32_RELA,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,/* special_function */
	 "R_M32R_32_RELA",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 24 bit address.  */
  HOWTO (R_M32R_24_RELA,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,/* special_function */
	 "R_M32R_24_RELA",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_M32R_10_PCREL_RELA,	/* type */
	 2,	                /* rightshift */
	 1,	                /* size (0 = byte, 1 = short, 2 = long) */
	 10,	                /* bitsize */
	 TRUE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 m32r_elf_10_pcrel_reloc, /* special_function */
	 "R_M32R_10_PCREL_RELA",/* name */
	 FALSE,	                /* partial_inplace */
	 0xff,		        /* src_mask */
	 0xff,   		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A relative 18 bit relocation, right shifted by 2.  */
  HOWTO (R_M32R_18_PCREL_RELA,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_18_PCREL_RELA",/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A relative 26 bit relocation, right shifted by 2.  */
  HOWTO (R_M32R_26_PCREL_RELA,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_26_PCREL_RELA",/* name */
	 FALSE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* High 16 bits of address when lower 16 is or'd in.  */
  HOWTO (R_M32R_HI16_ULO_RELA,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_HI16_ULO_RELA",/* name */
	 FALSE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* High 16 bits of address when lower 16 is added in.  */
  HOWTO (R_M32R_HI16_SLO_RELA,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_HI16_SLO_RELA",/* name */
	 FALSE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Lower 16 bits of address.  */
  HOWTO (R_M32R_LO16_RELA,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_LO16_RELA",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Small data area 16 bits offset.  */
  HOWTO (R_M32R_SDA16_RELA,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_SDA16_RELA",	/* name */
	 TRUE,			/* partial_inplace */  /* FIXME: correct? */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_M32R_RELA_GNU_VTINHERIT, /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_M32R_RELA_GNU_VTINHERIT", /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_M32R_RELA_GNU_VTENTRY,     /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_M32R_RELA_GNU_VTENTRY",   /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

  EMPTY_HOWTO (45),
  EMPTY_HOWTO (46),
  EMPTY_HOWTO (47),

  /* Like R_M32R_24, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_M32R_GOT24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_M32R_GOT24",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_M32R_PCREL, but referring to the procedure linkage table
     entry for the symbol.  */
  HOWTO (R_M32R_26_PLTREL,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_26_PLTREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* This is used only by the dynamic linker.  The symbol should exist
     both in the object being run and in some shared library.  The
     dynamic linker copies the data addressed by the symbol from the
     shared library into the object, because the object being
     run has to have the data at some particular address.  */
  HOWTO (R_M32R_COPY,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_M32R_COPY",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_M32R_24, but used when setting global offset table
     entries.  */
  HOWTO (R_M32R_GLOB_DAT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_M32R_GLOB_DAT",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Marks a procedure linkage table entry for a symbol.  */
  HOWTO (R_M32R_JMP_SLOT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_M32R_JMP_SLOT",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Used only by the dynamic linker.  When the object is run, this
     longword is set to the load address of the object, plus the
     addend.  */
  HOWTO (R_M32R_RELATIVE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_M32R_RELATIVE",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_M32R_GOTOFF,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_M32R_GOTOFF",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An PC Relative 24-bit relocation used when setting PIC offset
     table register. */
  HOWTO (R_M32R_GOTPC24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_M32R_GOTPC24",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffff,		/* src_mask */
	 0xffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Like R_M32R_HI16_ULO, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_M32R_GOT16_HI_ULO,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_GOT16_HI_ULO",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_M32R_HI16_SLO, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_M32R_GOT16_HI_SLO,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_GOT16_HI_SLO",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_M32R_LO16, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_M32R_GOT16_LO,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_GOT16_LO",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An PC Relative relocation used when setting PIC offset table register.
     Like R_M32R_HI16_ULO, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_M32R_GOTPC_HI_ULO,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_GOTPC_HI_ULO",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* An PC Relative relocation used when setting PIC offset table register.
     Like R_M32R_HI16_SLO, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_M32R_GOTPC_HI_SLO,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_GOTPC_HI_SLO",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* An PC Relative relocation used when setting PIC offset table register.
     Like R_M32R_LO16, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_M32R_GOTPC_LO,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_M32R_GOTPC_LO",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */
};

/* Handle the R_M32R_10_PCREL reloc.  */

static bfd_reloc_status_type
m32r_elf_10_pcrel_reloc (abfd, reloc_entry, symbol, data,
			 input_section, output_bfd, error_message)
     bfd * abfd;
     arelent * reloc_entry;
     asymbol * symbol;
     PTR data;
     asection * input_section;
     bfd * output_bfd;
     char ** error_message ATTRIBUTE_UNUSED;
{
  /* This part is from bfd_elf_generic_reloc.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != NULL)
    {
      /* FIXME: See bfd_perform_relocation.  Is this right?  */
      return bfd_reloc_continue;
    }

  return m32r_elf_do_10_pcrel_reloc (abfd, reloc_entry->howto,
				     input_section,
				     data, reloc_entry->address,
				     symbol->section,
				     (symbol->value
				      + symbol->section->output_section->vma
				      + symbol->section->output_offset),
				     reloc_entry->addend);
}

/* Utility to actually perform an R_M32R_10_PCREL reloc.  */

static bfd_reloc_status_type
m32r_elf_do_10_pcrel_reloc (abfd, howto, input_section, data, offset,
			    symbol_section, symbol_value, addend)
     bfd *abfd;
     reloc_howto_type *howto;
     asection *input_section;
     bfd_byte *data;
     bfd_vma offset;
     asection *symbol_section ATTRIBUTE_UNUSED;
     bfd_vma symbol_value;
     bfd_vma addend;
{
  bfd_signed_vma relocation;
  unsigned long x;
  bfd_reloc_status_type status;

  /* Sanity check the address (offset in section).  */
  if (offset > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  relocation = symbol_value + addend;
  /* Make it pc relative.  */
  relocation -=	(input_section->output_section->vma
		 + input_section->output_offset);
  /* These jumps mask off the lower two bits of the current address
     before doing pcrel calculations.  */
  relocation -= (offset & -(bfd_vma) 4);

  if (relocation < -0x200 || relocation > 0x1ff)
    status = bfd_reloc_overflow;
  else
    status = bfd_reloc_ok;

  x = bfd_get_16 (abfd, data + offset);
  relocation >>= howto->rightshift;
  relocation <<= howto->bitpos;
  x = (x & ~howto->dst_mask) | (((x & howto->src_mask) + relocation) & howto->dst_mask);
  bfd_put_16 (abfd, (bfd_vma) x, data + offset);

  return status;
}

/* Handle the R_M32R_HI16_[SU]LO relocs.
   HI16_SLO is for the add3 and load/store with displacement instructions.
   HI16_ULO is for the or3 instruction.
   For R_M32R_HI16_SLO, the lower 16 bits are sign extended when added to
   the high 16 bytes so if the lower 16 bits are negative (bit 15 == 1) then
   we must add one to the high 16 bytes (which will get subtracted off when
   the low 16 bits are added).
   These relocs have to be done in combination with an R_M32R_LO16 reloc
   because there is a carry from the LO16 to the HI16.  Here we just save
   the information we need; we do the actual relocation when we see the LO16.
   This code is copied from the elf32-mips.c.  We also support an arbitrary
   number of HI16 relocs to be associated with a single LO16 reloc.  The
   assembler sorts the relocs to ensure each HI16 immediately precedes its
   LO16.  However if there are multiple copies, the assembler may not find
   the real LO16 so it picks the first one it finds.  */

struct m32r_hi16
{
  struct m32r_hi16 *next;
  bfd_byte *addr;
  bfd_vma addend;
};

/* FIXME: This should not be a static variable.  */

static struct m32r_hi16 *m32r_hi16_list;

static bfd_reloc_status_type
m32r_elf_hi16_reloc (abfd, reloc_entry, symbol, data,
		     input_section, output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_reloc_status_type ret;
  bfd_vma relocation;
  struct m32r_hi16 *n;

  /* This part is from bfd_elf_generic_reloc.
     If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* Sanity check the address (offset in section).  */
  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  ret = bfd_reloc_ok;
  if (bfd_is_und_section (symbol->section)
      && output_bfd == (bfd *) NULL)
    ret = bfd_reloc_undefined;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  /* Save the information, and let LO16 do the actual relocation.  */
  n = (struct m32r_hi16 *) bfd_malloc ((bfd_size_type) sizeof *n);
  if (n == NULL)
    return bfd_reloc_outofrange;
  n->addr = (bfd_byte *) data + reloc_entry->address;
  n->addend = relocation;
  n->next = m32r_hi16_list;
  m32r_hi16_list = n;

  if (output_bfd != (bfd *) NULL)
    reloc_entry->address += input_section->output_offset;

  return ret;
}

/* Handle an M32R ELF HI16 reloc.  */

static void
m32r_elf_relocate_hi16 (input_bfd, type, relhi, rello, contents, addend)
     bfd *input_bfd;
     int type;
     Elf_Internal_Rela *relhi;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma addend;
{
  unsigned long insn;
  bfd_vma addlo;

  insn = bfd_get_32 (input_bfd, contents + relhi->r_offset);

  addlo = bfd_get_32 (input_bfd, contents + rello->r_offset);
  if (type == R_M32R_HI16_SLO)
    addlo = ((addlo & 0xffff) ^ 0x8000) - 0x8000;
  else
    addlo &= 0xffff;

  addend += ((insn & 0xffff) << 16) + addlo;

  /* Reaccount for sign extension of low part.  */
  if (type == R_M32R_HI16_SLO
      && (addend & 0x8000) != 0)
    addend += 0x10000;

  bfd_put_32 (input_bfd,
	      (insn & 0xffff0000) | ((addend >> 16) & 0xffff),
	      contents + relhi->r_offset);
}

/* Do an R_M32R_LO16 relocation.  This is a straightforward 16 bit
   inplace relocation; this function exists in order to do the
   R_M32R_HI16_[SU]LO relocation described above.  */

bfd_reloc_status_type
m32r_elf_lo16_reloc (input_bfd, reloc_entry, symbol, data,
		     input_section, output_bfd, error_message)
     bfd *input_bfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  /* This part is from bfd_elf_generic_reloc.
     If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (m32r_hi16_list != NULL)
    {
      struct m32r_hi16 *l;

      l = m32r_hi16_list;
      while (l != NULL)
	{
	  unsigned long insn;
	  unsigned long val;
	  unsigned long vallo;
	  struct m32r_hi16 *next;

	  /* Do the HI16 relocation.  Note that we actually don't need
	     to know anything about the LO16 itself, except where to
	     find the low 16 bits of the addend needed by the LO16.  */
	  insn = bfd_get_32 (input_bfd, l->addr);
	  vallo = ((bfd_get_32 (input_bfd, (bfd_byte *) data + reloc_entry->address)
		   & 0xffff) ^ 0x8000) - 0x8000;
	  val = ((insn & 0xffff) << 16) + vallo;
	  val += l->addend;

	  /* Reaccount for sign extension of low part.  */
	  if ((val & 0x8000) != 0)
	    val += 0x10000;

	  insn = (insn &~ (bfd_vma) 0xffff) | ((val >> 16) & 0xffff);
	  bfd_put_32 (input_bfd, (bfd_vma) insn, l->addr);

	  next = l->next;
	  free (l);
	  l = next;
	}

      m32r_hi16_list = NULL;
    }

  /* Now do the LO16 reloc in the usual way.
     ??? It would be nice to call bfd_elf_generic_reloc here,
     but we have partial_inplace set.  bfd_elf_generic_reloc will
     pass the handling back to bfd_install_relocation which will install
     a section relative addend which is wrong.  */
  return m32r_elf_generic_reloc (input_bfd, reloc_entry, symbol, data,
				input_section, output_bfd, error_message);
}

/* Do generic partial_inplace relocation.
   This is a local replacement for bfd_elf_generic_reloc.  */

bfd_reloc_status_type
m32r_elf_generic_reloc (input_bfd, reloc_entry, symbol, data,
		     input_section, output_bfd, error_message)
     bfd *input_bfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_reloc_status_type ret;
  bfd_vma relocation;
  bfd_byte *inplace_address;

  /* This part is from bfd_elf_generic_reloc.
     If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* Now do the reloc in the usual way.
     ??? It would be nice to call bfd_elf_generic_reloc here,
     but we have partial_inplace set.  bfd_elf_generic_reloc will
     pass the handling back to bfd_install_relocation which will install
     a section relative addend which is wrong.  */

  /* Sanity check the address (offset in section).  */
  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  ret = bfd_reloc_ok;
  if (bfd_is_und_section (symbol->section)
      && output_bfd == (bfd *) NULL)
    ret = bfd_reloc_undefined;

  if (bfd_is_com_section (symbol->section)
      || output_bfd != (bfd *) NULL)
    relocation = 0;
  else
    relocation = symbol->value;

  /* Only do this for a final link.  */
  if (output_bfd == (bfd *) NULL)
    {
      relocation += symbol->section->output_section->vma;
      relocation += symbol->section->output_offset;
    }

  relocation += reloc_entry->addend;
  inplace_address = (bfd_byte *) data + reloc_entry->address;

#define DOIT(x) 					\
  x = ( (x & ~reloc_entry->howto->dst_mask) | 		\
  (((x & reloc_entry->howto->src_mask) +  relocation) &	\
  reloc_entry->howto->dst_mask))

  switch (reloc_entry->howto->size)
    {
    case 1:
      {
	short x = bfd_get_16 (input_bfd, inplace_address);
	DOIT (x);
      	bfd_put_16 (input_bfd, (bfd_vma) x, inplace_address);
      }
      break;
    case 2:
      {
	unsigned long x = bfd_get_32 (input_bfd, inplace_address);
	DOIT (x);
      	bfd_put_32 (input_bfd, (bfd_vma)x , inplace_address);
      }
      break;
    default:
      BFD_ASSERT (0);
    }

  if (output_bfd != (bfd *) NULL)
    reloc_entry->address += input_section->output_offset;

  return ret;
}

/* Handle the R_M32R_SDA16 reloc.
   This reloc is used to compute the address of objects in the small data area
   and to perform loads and stores from that area.
   The lower 16 bits are sign extended and added to the register specified
   in the instruction, which is assumed to point to _SDA_BASE_.  */

static bfd_reloc_status_type
m32r_elf_sda16_reloc (abfd, reloc_entry, symbol, data,
		      input_section, output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* This part is from bfd_elf_generic_reloc.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != NULL)
    {
      /* FIXME: See bfd_perform_relocation.  Is this right?  */
      return bfd_reloc_continue;
    }

  /* FIXME: not sure what to do here yet.  But then again, the linker
     may never call us.  */
  abort ();
}

/* Map BFD reloc types to M32R ELF reloc types.  */

struct m32r_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct m32r_reloc_map m32r_reloc_map_old[] =
{
  { BFD_RELOC_NONE, R_M32R_NONE },
  { BFD_RELOC_16, R_M32R_16 },
  { BFD_RELOC_32, R_M32R_32 },
  { BFD_RELOC_M32R_24, R_M32R_24 },
  { BFD_RELOC_M32R_10_PCREL, R_M32R_10_PCREL },
  { BFD_RELOC_M32R_18_PCREL, R_M32R_18_PCREL },
  { BFD_RELOC_M32R_26_PCREL, R_M32R_26_PCREL },
  { BFD_RELOC_M32R_HI16_ULO, R_M32R_HI16_ULO },
  { BFD_RELOC_M32R_HI16_SLO, R_M32R_HI16_SLO },
  { BFD_RELOC_M32R_LO16, R_M32R_LO16 },
  { BFD_RELOC_M32R_SDA16, R_M32R_SDA16 },
  { BFD_RELOC_VTABLE_INHERIT, R_M32R_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, R_M32R_GNU_VTENTRY },
};

static const struct m32r_reloc_map m32r_reloc_map[] =
{
  { BFD_RELOC_NONE, R_M32R_NONE },
  { BFD_RELOC_16, R_M32R_16_RELA },
  { BFD_RELOC_32, R_M32R_32_RELA },
  { BFD_RELOC_M32R_24, R_M32R_24_RELA },
  { BFD_RELOC_M32R_10_PCREL, R_M32R_10_PCREL_RELA },
  { BFD_RELOC_M32R_18_PCREL, R_M32R_18_PCREL_RELA },
  { BFD_RELOC_M32R_26_PCREL, R_M32R_26_PCREL_RELA },
  { BFD_RELOC_M32R_HI16_ULO, R_M32R_HI16_ULO_RELA },
  { BFD_RELOC_M32R_HI16_SLO, R_M32R_HI16_SLO_RELA },
  { BFD_RELOC_M32R_LO16, R_M32R_LO16_RELA },
  { BFD_RELOC_M32R_SDA16, R_M32R_SDA16_RELA },
  { BFD_RELOC_VTABLE_INHERIT, R_M32R_RELA_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, R_M32R_RELA_GNU_VTENTRY },

  { BFD_RELOC_M32R_GOT24, R_M32R_GOT24 },
  { BFD_RELOC_M32R_26_PLTREL, R_M32R_26_PLTREL },
  { BFD_RELOC_M32R_COPY, R_M32R_COPY },
  { BFD_RELOC_M32R_GLOB_DAT, R_M32R_GLOB_DAT },
  { BFD_RELOC_M32R_JMP_SLOT, R_M32R_JMP_SLOT },
  { BFD_RELOC_M32R_RELATIVE, R_M32R_RELATIVE },
  { BFD_RELOC_M32R_GOTOFF, R_M32R_GOTOFF },
  { BFD_RELOC_M32R_GOTPC24, R_M32R_GOTPC24 },
  { BFD_RELOC_M32R_GOT16_HI_ULO, R_M32R_GOT16_HI_ULO },
  { BFD_RELOC_M32R_GOT16_HI_SLO, R_M32R_GOT16_HI_SLO },
  { BFD_RELOC_M32R_GOT16_LO, R_M32R_GOT16_LO },
  { BFD_RELOC_M32R_GOTPC_HI_ULO, R_M32R_GOTPC_HI_ULO },
  { BFD_RELOC_M32R_GOTPC_HI_SLO, R_M32R_GOTPC_HI_SLO },
  { BFD_RELOC_M32R_GOTPC_LO, R_M32R_GOTPC_LO },
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

#ifdef USE_M32R_OLD_RELOC
  for (i = 0;
       i < sizeof (m32r_reloc_map_old) / sizeof (struct m32r_reloc_map);
       i++)
    {
      if (m32r_reloc_map_old[i].bfd_reloc_val == code)
	return &m32r_elf_howto_table[m32r_reloc_map_old[i].elf_reloc_val];
    }
#else /* ! USE_M32R_OLD_RELOC */

  for (i = 0;
       i < sizeof (m32r_reloc_map) / sizeof (struct m32r_reloc_map);
       i++)
    {
      if (m32r_reloc_map[i].bfd_reloc_val == code)
	return &m32r_elf_howto_table[m32r_reloc_map[i].elf_reloc_val];
    }
#endif

  return NULL;
}

/* Set the howto pointer for an M32R ELF reloc.  */

static void
m32r_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (ELF32_R_TYPE(dst->r_info) <= (unsigned int) R_M32R_GNU_VTENTRY)
  cache_ptr->howto = &m32r_elf_howto_table[r_type];
}

static void
m32r_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  BFD_ASSERT ((ELF32_R_TYPE(dst->r_info) == (unsigned int) R_M32R_NONE)
              || ((ELF32_R_TYPE(dst->r_info) > (unsigned int) R_M32R_GNU_VTENTRY)
                  && (ELF32_R_TYPE(dst->r_info) < (unsigned int) R_M32R_max)));
  cache_ptr->howto = &m32r_elf_howto_table[ELF32_R_TYPE(dst->r_info)];
}


/* Given a BFD section, try to locate the corresponding ELF section
   index.  */

bfd_boolean
_bfd_m32r_elf_section_from_bfd_section (abfd, sec, retval)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     int *retval;
{
  if (strcmp (bfd_get_section_name (abfd, sec), ".scommon") == 0)
    {
      *retval = SHN_M32R_SCOMMON;
      return TRUE;
    }
  return FALSE;
}

/* M32R ELF uses two common sections.  One is the usual one, and the other
   is for small objects.  All the small objects are kept together, and then
   referenced via one register, which yields faster assembler code.  It is
   up to the compiler to emit an instruction to load the register with
   _SDA_BASE.  This is what we use for the small common section.  This
   approach is copied from elf32-mips.c.  */
static asection m32r_elf_scom_section;
static asymbol m32r_elf_scom_symbol;
static asymbol *m32r_elf_scom_symbol_ptr;

/* Handle the special M32R section numbers that a symbol may use.  */

void
_bfd_m32r_elf_symbol_processing (abfd, asym)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol *asym;
{
  elf_symbol_type *elfsym;

  elfsym = (elf_symbol_type *) asym;

  switch (elfsym->internal_elf_sym.st_shndx)
    {
    case SHN_M32R_SCOMMON:
      if (m32r_elf_scom_section.name == NULL)
	{
	  /* Initialize the small common section.  */
	  m32r_elf_scom_section.name = ".scommon";
	  m32r_elf_scom_section.flags = SEC_IS_COMMON;
	  m32r_elf_scom_section.output_section = &m32r_elf_scom_section;
	  m32r_elf_scom_section.symbol = &m32r_elf_scom_symbol;
	  m32r_elf_scom_section.symbol_ptr_ptr = &m32r_elf_scom_symbol_ptr;
	  m32r_elf_scom_symbol.name = ".scommon";
	  m32r_elf_scom_symbol.flags = BSF_SECTION_SYM;
	  m32r_elf_scom_symbol.section = &m32r_elf_scom_section;
	  m32r_elf_scom_symbol_ptr = &m32r_elf_scom_symbol;
	}
      asym->section = &m32r_elf_scom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;
    }
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We must handle the special M32R section numbers here.
   We also keep watching for whether we need to create the sdata special
   linker sections.  */

static bfd_boolean
m32r_elf_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     Elf_Internal_Sym *sym;
     const char **namep;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp;
     bfd_vma *valp;
{
  if (! info->relocatable
      && (*namep)[0] == '_' && (*namep)[1] == 'S'
      && strcmp (*namep, "_SDA_BASE_") == 0
      && is_elf_hash_table (info->hash))
    {
      /* This is simpler than using _bfd_elf_create_linker_section
	 (our needs are simpler than ppc's needs).  Also
	 _bfd_elf_create_linker_section currently has a bug where if a .sdata
	 section already exists a new one is created that follows it which
	 screws of _SDA_BASE_ address calcs because output_offset != 0.  */
      struct elf_link_hash_entry *h;
      struct bfd_link_hash_entry *bh;
      asection *s = bfd_get_section_by_name (abfd, ".sdata");

      /* The following code was cobbled from elf32-ppc.c and elflink.c.  */

      if (s == NULL)
	{
	  flagword flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
			    | SEC_IN_MEMORY | SEC_LINKER_CREATED);

	  s = bfd_make_section_anyway (abfd, ".sdata");
	  if (s == NULL)
	    return FALSE;
	  bfd_set_section_flags (abfd, s, flags);
	  bfd_set_section_alignment (abfd, s, 2);
	}

      bh = bfd_link_hash_lookup (info->hash, "_SDA_BASE_",
				 FALSE, FALSE, FALSE);

      if ((bh == NULL || bh->type == bfd_link_hash_undefined)
	  && !(_bfd_generic_link_add_one_symbol (info,
						 abfd,
						 "_SDA_BASE_",
						 BSF_GLOBAL,
						 s,
						 (bfd_vma) 32768,
						 (const char *) NULL,
						 FALSE,
						 get_elf_backend_data (abfd)->collect,
						 &bh)))
	return FALSE;
      h = (struct elf_link_hash_entry *) bh;
      h->type = STT_OBJECT;
    }

  switch (sym->st_shndx)
    {
    case SHN_M32R_SCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".scommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;
    }

  return TRUE;
}

/* We have to figure out the SDA_BASE value, so that we can adjust the
   symbol value correctly.  We look up the symbol _SDA_BASE_ in the output
   BFD.  If we can't find it, we're stuck.  We cache it in the ELF
   target data.  We don't need to adjust the symbol value for an
   external symbol if we are producing relocatable output.  */

static bfd_reloc_status_type
m32r_elf_final_sda_base (output_bfd, info, error_message, psb)
     bfd *output_bfd;
     struct bfd_link_info *info;
     const char **error_message;
     bfd_vma *psb;
{
  if (elf_gp (output_bfd) == 0)
    {
      struct bfd_link_hash_entry *h;

      h = bfd_link_hash_lookup (info->hash, "_SDA_BASE_", FALSE, FALSE, TRUE);
      if (h != (struct bfd_link_hash_entry *) NULL
	  && h->type == bfd_link_hash_defined)
	elf_gp (output_bfd) = (h->u.def.value
			       + h->u.def.section->output_section->vma
			       + h->u.def.section->output_offset);
      else
	{
	  /* Only get the error once.  */
	  *psb = elf_gp (output_bfd) = 4;
	  *error_message =
	    (const char *) _("SDA relocation when _SDA_BASE_ not defined");
	  return bfd_reloc_dangerous;
	}
    }
  *psb = elf_gp (output_bfd);
  return bfd_reloc_ok;
}

/* Return size of a PLT entry.  */
#define elf_m32r_sizeof_plt(info) PLT_ENTRY_SIZE

/* The m32r linker needs to keep track of the number of relocs that it
   decides to copy in check_relocs for each symbol.  This is so that
   it can discard PC relative relocs if it doesn't need them when
   linking with -Bsymbolic.  We store the information in a field
   extending the regular ELF linker hash table.  */

/* This structure keeps track of the number of PC relative relocs we
   have copied for a given symbol.  */

struct elf_m32r_pcrel_relocs_copied
{
  /* Next section.  */
  struct elf_m32r_pcrel_relocs_copied *next;
  /* A section in dynobj.  */
  asection *section;
  /* Number of relocs copied in this section.  */
  bfd_size_type count;
};

/* The sh linker needs to keep track of the number of relocs that it
   decides to copy as dynamic relocs in check_relocs for each symbol.
   This is so that it can later discard them if they are found to be
   unnecessary.  We store the information in a field extending the
   regular ELF linker hash table.  */

struct elf_m32r_dyn_relocs
{
  struct elf_m32r_dyn_relocs *next;

  /* The input section of the reloc.  */
  asection *sec;

  /* Total number of relocs copied for the input section.  */
  bfd_size_type count;

  /* Number of pc-relative relocs copied for the input section.  */
  bfd_size_type pc_count;
};


/* m32r ELF linker hash entry.  */

struct elf_m32r_link_hash_entry
{
  struct elf_link_hash_entry root;

  /* Track dynamic relocs copied for this symbol.  */
  struct elf_m32r_dyn_relocs *dyn_relocs;

//  bfd_signed_vma gotplt_refcount;

  /* Number of PC relative relocs copied for this symbol.  */
  /* struct elf_m32r_pcrel_relocs_copied *pcrel_relocs_copied;  FIXME */
};

/* m32r ELF linker hash table.  */

struct elf_m32r_link_hash_table
{
  struct elf_link_hash_table root;

  /* Short-cuts to get to dynamic linker sections.  */
  asection *sgot;
  asection *sgotplt;
  asection *srelgot;
  asection *splt;
  asection *srelplt;
  asection *sdynbss;
  asection *srelbss;

  /* Small local sym to section mapping cache.  */
  struct sym_sec_cache sym_sec;
};

/* Traverse an m32r ELF linker hash table.  */

#define m32r_elf_link_hash_traverse(table, func, info)			\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (bfd_boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the m32r ELF linker hash table from a link_info structure.  */


#define m32r_elf_hash_table(p) \
  ((struct elf_m32r_link_hash_table *) ((p)->hash))

/* Create an entry in an m32r ELF linker hash table.  */
static struct bfd_hash_entry *
m32r_elf_link_hash_newfunc (struct bfd_hash_entry *, struct bfd_hash_table *,
                            const char * );

static struct bfd_hash_entry *
m32r_elf_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf_m32r_link_hash_entry *ret =
    (struct elf_m32r_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct elf_m32r_link_hash_entry *) NULL)
    ret = ((struct elf_m32r_link_hash_entry *)
           bfd_hash_allocate (table,
                              sizeof (struct elf_m32r_link_hash_entry)));
  if (ret == (struct elf_m32r_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf_m32r_link_hash_entry *)
         _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
                                     table, string));
  if (ret != (struct elf_m32r_link_hash_entry *) NULL)
    {
      struct elf_m32r_link_hash_entry *eh;

      eh = (struct elf_m32r_link_hash_entry *) ret;
      eh->dyn_relocs = NULL;
//      eh->gotplt_refcount = 0;
      /* eh->pcrel_relocs_copied = NULL; FIXME */
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create an m32r ELF linker hash table.  */
static struct bfd_link_hash_table *m32r_elf_link_hash_table_create (bfd *);

static struct bfd_link_hash_table *
m32r_elf_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf_m32r_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct elf_m32r_link_hash_table);

  ret = (struct elf_m32r_link_hash_table *) bfd_malloc (amt);
  if (ret == (struct elf_m32r_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
                                       m32r_elf_link_hash_newfunc))
    {
      free (ret);
      return NULL;
    }

  ret->sgot = NULL;
  ret->sgotplt = NULL;
  ret->srelgot = NULL;
  ret->splt = NULL;
  ret->srelplt = NULL;
  ret->sdynbss = NULL;
  ret->srelbss = NULL;
  ret->sym_sec.abfd = NULL;

  return &ret->root.root;
}

/* Create .got, .gotplt, and .rela.got sections in DYNOBJ, and set up
   shortcuts to them in our hash table.  */
static bfd_boolean create_got_section (bfd *, struct bfd_link_info *);

static bfd_boolean
create_got_section (dynobj, info)
     bfd *dynobj;
     struct bfd_link_info *info;
{
  struct elf_m32r_link_hash_table *htab;

  if (! _bfd_elf_create_got_section (dynobj, info))
    return FALSE;

  htab = m32r_elf_hash_table (info);
  htab->sgot = bfd_get_section_by_name (dynobj, ".got");
  htab->sgotplt = bfd_get_section_by_name (dynobj, ".got.plt");
  if (! htab->sgot || ! htab->sgotplt)
    abort ();

  htab->srelgot = bfd_make_section (dynobj, ".rela.got");
  if (htab->srelgot == NULL
      || ! bfd_set_section_flags (dynobj, htab->srelgot,
                                  (SEC_ALLOC
                                   | SEC_LOAD
                                   | SEC_HAS_CONTENTS
                                   | SEC_IN_MEMORY
                                   | SEC_LINKER_CREATED
                                   | SEC_READONLY))
      || ! bfd_set_section_alignment (dynobj, htab->srelgot, 2))
    return FALSE;

  return TRUE;
}

/* Create dynamic sections when linking against a dynamic object.  */

static bfd_boolean
m32r_elf_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  struct elf_m32r_link_hash_table *htab;
  flagword flags, pltflags;
  register asection *s;
  const struct elf_backend_data *bed = get_elf_backend_data (abfd);
  int ptralign = 2; /* 32bit */

  htab = m32r_elf_hash_table (info);

  /* We need to create .plt, .rel[a].plt, .got, .got.plt, .dynbss, and
     .rel[a].bss sections.  */

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
           | SEC_LINKER_CREATED);

  pltflags = flags;
  pltflags |= SEC_CODE;
  if (bed->plt_not_loaded)
    pltflags &= ~ (SEC_LOAD | SEC_HAS_CONTENTS);
  if (bed->plt_readonly)
    pltflags |= SEC_READONLY;

  s = bfd_make_section (abfd, ".plt");
  htab->splt = s;
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, pltflags)
      || ! bfd_set_section_alignment (abfd, s, bed->plt_alignment))
    return FALSE;

  if (bed->want_plt_sym)
    {
      /* Define the symbol _PROCEDURE_LINKAGE_TABLE_ at the start of the
         .plt section.  */
      struct bfd_link_hash_entry *bh = NULL;
      struct elf_link_hash_entry *h;
      if (! (_bfd_generic_link_add_one_symbol
             (info, abfd, "_PROCEDURE_LINKAGE_TABLE_", BSF_GLOBAL, s,
              (bfd_vma) 0, (const char *) NULL, FALSE,
              get_elf_backend_data (abfd)->collect, &bh)))
        return FALSE;
      h = (struct elf_link_hash_entry *) bh;
      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
      h->type = STT_OBJECT;

      if (info->shared
          && ! bfd_elf_link_record_dynamic_symbol (info, h))
        return FALSE;
    }

  s = bfd_make_section (abfd,
                        bed->default_use_rela_p ? ".rela.plt" : ".rel.plt");
  htab->srelplt = s;
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, ptralign))
    return FALSE;

  if (htab->sgot == NULL
      && ! create_got_section (abfd, info))
    return FALSE;

  {
    const char *secname;
    char *relname;
    flagword secflags;
    asection *sec;

    for (sec = abfd->sections; sec; sec = sec->next)
      {
        secflags = bfd_get_section_flags (abfd, sec);
        if ((secflags & (SEC_DATA | SEC_LINKER_CREATED))
            || ((secflags & SEC_HAS_CONTENTS) != SEC_HAS_CONTENTS))
          continue;
        secname = bfd_get_section_name (abfd, sec);
        relname = (char *) bfd_malloc ((bfd_size_type) strlen (secname) + 6);
        strcpy (relname, ".rela");
        strcat (relname, secname);
        if (bfd_get_section_by_name (abfd, secname))
          continue;
        s = bfd_make_section (abfd, relname);
        if (s == NULL
            || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
            || ! bfd_set_section_alignment (abfd, s, ptralign))
          return FALSE;
      }
  }

  if (bed->want_dynbss)
    {
      /* The .dynbss section is a place to put symbols which are defined
         by dynamic objects, are referenced by regular objects, and are
         not functions.  We must allocate space for them in the process
         image and use a R_*_COPY reloc to tell the dynamic linker to
         initialize them at run time.  The linker script puts the .dynbss
         section into the .bss section of the final image.  */
      s = bfd_make_section (abfd, ".dynbss");
      htab->sdynbss = s;
      if (s == NULL
          || ! bfd_set_section_flags (abfd, s, SEC_ALLOC))
        return FALSE;
      /* The .rel[a].bss section holds copy relocs.  This section is not
         normally needed.  We need to create it here, though, so that the
         linker will map it to an output section.  We can't just create it
         only if we need it, because we will not know whether we need it
         until we have seen all the input files, and the first time the
         main linker code calls BFD after examining all the input files
         (size_dynamic_sections) the input sections have already been
         mapped to the output sections.  If the section turns out not to
         be needed, we can discard it later.  We will never need this
         section when generating a shared object, since they do not use
         copy relocs.  */
      if (! info->shared)
        {
          s = bfd_make_section (abfd,
                                (bed->default_use_rela_p
                                 ? ".rela.bss" : ".rel.bss"));
          htab->srelbss = s;
          if (s == NULL
              || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
              || ! bfd_set_section_alignment (abfd, s, ptralign))
            return FALSE;
        }
    }

  return TRUE;
}

/* Copy the extra info we tack onto an elf_link_hash_entry.  */
static void m32r_elf_copy_indirect_symbol (const struct elf_backend_data *,
                                           struct elf_link_hash_entry *,
                                           struct elf_link_hash_entry *);

static void
m32r_elf_copy_indirect_symbol (const struct elf_backend_data *bed,
                               struct elf_link_hash_entry *dir,
                               struct elf_link_hash_entry *ind)
{
  struct elf_m32r_link_hash_entry *edir, *eind;

  edir = (struct elf_m32r_link_hash_entry *) dir;
  eind = (struct elf_m32r_link_hash_entry *) ind;

  if (eind->dyn_relocs != NULL)
    {
      if (edir->dyn_relocs != NULL)
        {
          struct elf_m32r_dyn_relocs **pp;
          struct elf_m32r_dyn_relocs *p;

          if (ind->root.type == bfd_link_hash_indirect)
            abort ();

          /* Add reloc counts against the weak sym to the strong sym
             list.  Merge any entries against the same section.  */
          for (pp = &eind->dyn_relocs; (p = *pp) != NULL; )
            {
              struct elf_m32r_dyn_relocs *q;

              for (q = edir->dyn_relocs; q != NULL; q = q->next)
                if (q->sec == p->sec)
                  {
                    q->pc_count += p->pc_count;
                    q->count += p->count;
                    *pp = p->next;
                    break;
                  }
              if (q == NULL)
                pp = &p->next;
            }
          *pp = edir->dyn_relocs;
        }

      edir->dyn_relocs = eind->dyn_relocs;
      eind->dyn_relocs = NULL;
    }

//  if (ind->root.type == bfd_link_hash_indirect
//      && dir->got.refcount <= 0)
//    {
//      edir->tls_type = eind->tls_type;
//      eind->tls_type = GOT_UNKNOWN;
//    }
  _bfd_elf_link_hash_copy_indirect (bed, dir, ind);
}


/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static bfd_boolean
m32r_elf_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  struct elf_m32r_link_hash_table *htab;
  struct elf_m32r_link_hash_entry *eh;
  struct elf_m32r_dyn_relocs *p;
  bfd *dynobj;
  asection *s;
  unsigned int power_of_two;

#ifdef DEBUG_PIC
printf("m32r_elf_adjust_dynamic_symbol()\n");
#endif

  dynobj = elf_hash_table (info)->dynobj;

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (dynobj != NULL
              && ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT)
                  || h->weakdef != NULL
                  || ((h->elf_link_hash_flags
                       & ELF_LINK_HASH_DEF_DYNAMIC) != 0
                      && (h->elf_link_hash_flags
                          & ELF_LINK_HASH_REF_REGULAR) != 0
                      && (h->elf_link_hash_flags
                          & ELF_LINK_HASH_DEF_REGULAR) == 0)));


  /* If this is a function, put it in the procedure linkage table.  We
     will fill in the contents of the procedure linkage table later,
     when we know the address of the .got section.  */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      if (! info->shared
          && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
          && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) == 0
	  && h->root.type != bfd_link_hash_undefweak
	  && h->root.type != bfd_link_hash_undefined)
        {
          /* This case can occur if we saw a PLT reloc in an input
             file, but the symbol was never referred to by a dynamic
             object.  In such a case, we don't actually need to build
             a procedure linkage table, and we can just do a PCREL
             reloc instead.  */
          h->plt.offset = (bfd_vma) -1;
          h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
        }

      return TRUE;
    }
  else
    h->plt.offset = (bfd_vma) -1;

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
                  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
      return TRUE;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  */

  /* If we are creating a shared library, we must presume that the
     only references to the symbol are via the global offset table.
     For such cases we need not do anything here; the relocations will
     be handled correctly by relocate_section.  */
  if (info->shared)
    return TRUE;

  /* If there are no references to this symbol that do not use the
     GOT, we don't need to generate a copy reloc.  */
  if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0)
    return TRUE;

  /* If -z nocopyreloc was given, we won't generate them either.  */
  if (info->nocopyreloc)
    {
      h->elf_link_hash_flags &= ~ELF_LINK_NON_GOT_REF;
      return TRUE;
    }

  eh = (struct elf_m32r_link_hash_entry *) h;
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      s = p->sec->output_section;
      if (s != NULL && (s->flags & (SEC_READONLY | SEC_HAS_CONTENTS)) != 0)
        break;
    }

  /* If we didn't find any dynamic relocs in sections which needs the
     copy reloc, then we'll be keeping the dynamic relocs and avoiding
     the copy reloc.  */
  if (p == NULL)
    {
      h->elf_link_hash_flags &= ~ELF_LINK_NON_GOT_REF;
      return TRUE;
    }

  /* We must allocate the symbol in our .dynbss section, which will
     become part of the .bss section of the executable.  There will be
     an entry for this symbol in the .dynsym section.  The dynamic
     object will contain position independent code, so all references
     from the dynamic object to this symbol will go through the global
     offset table.  The dynamic linker will use the .dynsym entry to
     determine the address it must put in the global offset table, so
     both the dynamic object and the regular object will refer to the
     same memory location for the variable.  */

  htab = m32r_elf_hash_table (info);
  s = htab->sdynbss;
  BFD_ASSERT (s != NULL);

  /* We must generate a R_M32R_COPY reloc to tell the dynamic linker
     to copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rela.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      srel = htab->srelbss;
      BFD_ASSERT (srel != NULL);
      srel->_raw_size += sizeof (Elf32_External_Rela);
      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_COPY;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 3)
    power_of_two = 3;

  /* Apply the required alignment.  */
  s->_raw_size = BFD_ALIGN (s->_raw_size,
                            (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (dynobj, s))
    {
      if (! bfd_set_section_alignment (dynobj, s, power_of_two))
        return FALSE;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return TRUE;
}

/* Allocate space in .plt, .got and associated reloc sections for
   dynamic relocs.  */

static bfd_boolean
allocate_dynrelocs (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct bfd_link_info *info;
  struct elf_m32r_link_hash_table *htab;
  struct elf_m32r_link_hash_entry *eh;
  struct elf_m32r_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_indirect)
    return TRUE;

  if (h->root.type == bfd_link_hash_warning)
    /* When warning symbols are created, they **replace** the "real"
       entry in the hash table, thus we never get to see the real
       symbol in a hash traversal.  So look at it now.  */
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  info = (struct bfd_link_info *) inf;
  htab = m32r_elf_hash_table (info);

  eh = (struct elf_m32r_link_hash_entry *) h;
//  if ((h->got.refcount > 0
//      || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL))
//      && eh->gotplt_refcount > 0)
//    {
//      /* The symbol has been forced local, or we have some direct got refs,
//         so treat all the gotplt refs as got refs. */
//      h->got.refcount += eh->gotplt_refcount;
//      if (h->plt.refcount >= eh->gotplt_refcount)
//        h->plt.refcount -= eh->gotplt_refcount;
//    }

  if (htab->root.dynamic_sections_created
      && h->plt.refcount > 0)
    {
      /* Make sure this symbol is output as a dynamic symbol.
         Undefined weak syms won't yet be marked as dynamic.  */
      if (h->dynindx == -1
          && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
        {
          if (! bfd_elf_link_record_dynamic_symbol (info, h))
            return FALSE;
        }

      if (WILL_CALL_FINISH_DYNAMIC_SYMBOL (1, info->shared, h))
        {
          asection *s = htab->splt;

          /* If this is the first .plt entry, make room for the special
             first entry.  */
          if (s->_raw_size == 0)
            s->_raw_size += PLT_ENTRY_SIZE;

          h->plt.offset = s->_raw_size;

          /* If this symbol is not defined in a regular file, and we are
             not generating a shared library, then set the symbol to this
             location in the .plt.  This is required to make function
             pointers compare as equal between the normal executable and
             the shared library.  */
          if (! info->shared
              && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
            {
              h->root.u.def.section = s;
              h->root.u.def.value = h->plt.offset;
            }

          /* Make room for this entry.  */
          s->_raw_size += PLT_ENTRY_SIZE;

          /* We also need to make an entry in the .got.plt section, which
             will be placed in the .got section by the linker script.  */
          htab->sgotplt->_raw_size += 4;

          /* We also need to make an entry in the .rel.plt section.  */
          htab->srelplt->_raw_size += sizeof (Elf32_External_Rela);
        }
      else
        {
          h->plt.offset = (bfd_vma) -1;
          h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
        }
    }
  else
    {
      h->plt.offset = (bfd_vma) -1;
      h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
    }

  if (h->got.refcount > 0)
    {
      asection *s;
      bfd_boolean dyn;

      /* Make sure this symbol is output as a dynamic symbol.
         Undefined weak syms won't yet be marked as dynamic.  */
      if (h->dynindx == -1
          && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
        {
          if (! bfd_elf_link_record_dynamic_symbol (info, h))
            return FALSE;
        }

      s = htab->sgot;

      h->got.offset = s->_raw_size;
      s->_raw_size += 4;
      dyn = htab->root.dynamic_sections_created;
      if (WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info->shared, h))
        htab->srelgot->_raw_size += sizeof (Elf32_External_Rela);
    }
  else
    h->got.offset = (bfd_vma) -1;

  if (eh->dyn_relocs == NULL)
    return TRUE;

  /* In the shared -Bsymbolic case, discard space allocated for
     dynamic pc-relative relocs against symbols which turn out to be
     defined in regular objects.  For the normal shared case, discard
     space for pc-relative relocs that have become local due to symbol
     visibility changes.  */

  if (info->shared)
    {
      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
          && ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0
              || info->symbolic))
        {
          struct elf_m32r_dyn_relocs **pp;
          for (pp = &eh->dyn_relocs; (p = *pp) != NULL; )
            {
              p->count -= p->pc_count;
              p->pc_count = 0;
              if (p->count == 0)
                *pp = p->next;
              else
                pp = &p->next;
            }
        }
    }
  else
    {
      /* For the non-shared case, discard space for relocs against
         symbols which turn out to need copy relocs or are not
         dynamic.  */

      if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0
          && (((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
               && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
              || (htab->root.dynamic_sections_created
                  && (h->root.type == bfd_link_hash_undefweak
                      || h->root.type == bfd_link_hash_undefined))))
        {
          /* Make sure this symbol is output as a dynamic symbol.
             Undefined weak syms won't yet be marked as dynamic.  */
          if (h->dynindx == -1
              && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
            {
              if (! bfd_elf_link_record_dynamic_symbol (info, h))
                return FALSE;
            }

          /* If that succeeded, we know we'll be keeping all the
             relocs.  */
          if (h->dynindx != -1)
            goto keep;
        }

      eh->dyn_relocs = NULL;

    keep: ;
    }

  /* Finally, allocate space.  */
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      asection *sreloc = elf_section_data (p->sec)->sreloc;
      sreloc->_raw_size += p->count * sizeof (Elf32_External_Rela);
    }

  return TRUE;
}
/* Find any dynamic relocs that apply to read-only sections.  */

static bfd_boolean
readonly_dynrelocs (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct elf_m32r_link_hash_entry *eh;
  struct elf_m32r_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  eh = (struct elf_m32r_link_hash_entry *) h;
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      asection *s = p->sec->output_section;

      if (s != NULL && (s->flags & SEC_READONLY) != 0)
        {
          struct bfd_link_info *info = (struct bfd_link_info *) inf;

          info->flags |= DF_TEXTREL;

          /* Not an error, just cut short the traversal.  */
          return FALSE;
        }
    }
  return TRUE;
}

/* Set the sizes of the dynamic sections.  */

static bfd_boolean
m32r_elf_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  struct elf_m32r_link_hash_table *htab;
  bfd *dynobj;
  asection *s;
  bfd_boolean relocs;
  bfd *ibfd;

#ifdef DEBUG_PIC
printf("m32r_elf_size_dynamic_sections()\n");
#endif

  htab = m32r_elf_hash_table (info);
  dynobj = htab->root.dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (htab->root.dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (! info->shared)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}
    }

  /* Set up .got offsets for local syms, and space for local dynamic
     relocs.  */
  for (ibfd = info->input_bfds; ibfd != NULL; ibfd = ibfd->link_next)
    {
      bfd_signed_vma *local_got;
      bfd_signed_vma *end_local_got;
      bfd_size_type locsymcount;
      Elf_Internal_Shdr *symtab_hdr;
      asection *srel;

      if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour)
        continue;

      for (s = ibfd->sections; s != NULL; s = s->next)
        {
          struct elf_m32r_dyn_relocs *p;

          for (p = ((struct elf_m32r_dyn_relocs *)
                    elf_section_data (s)->local_dynrel);
               p != NULL;
               p = p->next)
            {
              if (! bfd_is_abs_section (p->sec)
                  && bfd_is_abs_section (p->sec->output_section))
                {
                  /* Input section has been discarded, either because
                     it is a copy of a linkonce section or due to
                     linker script /DISCARD/, so we'll be discarding
                     the relocs too.  */
                }
              else if (p->count != 0)
                {
                  srel = elf_section_data (p->sec)->sreloc;
                  srel->_raw_size += p->count * sizeof (Elf32_External_Rela);
                  if ((p->sec->output_section->flags & SEC_READONLY) != 0)
                    info->flags |= DF_TEXTREL;
                }
            }
        }

      local_got = elf_local_got_refcounts (ibfd);
      if (!local_got)
        continue;

      symtab_hdr = &elf_tdata (ibfd)->symtab_hdr;
      locsymcount = symtab_hdr->sh_info;
      end_local_got = local_got + locsymcount;
      s = htab->sgot;
      srel = htab->srelgot;
      for (; local_got < end_local_got; ++local_got)
        {
          if (*local_got > 0)
            {
              *local_got = s->_raw_size;
              s->_raw_size += 4;
              if (info->shared)
                srel->_raw_size += sizeof (Elf32_External_Rela);
            }
          else
            *local_got = (bfd_vma) -1;
        }
    }

  /* Allocate global sym .plt and .got entries, and space for global
     sym dynamic relocs.  */
  elf_link_hash_traverse (&htab->root, allocate_dynrelocs, (PTR) info);

  /* We now have determined the sizes of the various dynamic sections.
     Allocate memory for them.  */
  relocs = FALSE;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      if ((s->flags & SEC_LINKER_CREATED) == 0)
        continue;

      if (s == htab->splt
          || s == htab->sgot
          || s == htab->sgotplt)
        {
          /* Strip this section if we don't need it; see the
             comment below.  */
        }
      else if (strncmp (bfd_get_section_name (dynobj, s), ".rela", 5) == 0)
        {
          if (s->_raw_size != 0 && s != htab->srelplt)
            relocs = TRUE;

          /* We use the reloc_count field as a counter if we need
             to copy relocs into the output file.  */
          s->reloc_count = 0;
        }
      else
        {
          /* It's not one of our sections, so don't allocate space.  */
          continue;
        }

      if (s->_raw_size == 0)
        {
          /* If we don't need this section, strip it from the
             output file.  This is mostly to handle .rela.bss and
             .rela.plt.  We must create both sections in
             create_dynamic_sections, because they must be created
             before the linker maps input sections to output
             sections.  The linker does that before
             adjust_dynamic_symbol is called, and it is that
             function which decides whether anything needs to go
             into these sections.  */
          _bfd_strip_section_from_output (info, s);
          continue;
        }

      /* Allocate memory for the section contents.  We use bfd_zalloc
         here in case unused entries are not reclaimed before the
         section's contents are written out.  This should not happen,
         but this way if it does, we get a R_M32R_NONE reloc instead
         of garbage.  */
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->_raw_size);
      if (s->contents == NULL)
        return FALSE;
    }

  if (htab->root.dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in m32r_elf_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  _bfd_elf_add_dynamic_entry (info, TAG, VAL)

      if (! info->shared)
	{
	  if (! add_dynamic_entry (DT_DEBUG, 0))
	    return FALSE;
	}

      if (htab->splt->_raw_size != 0)
        {
          if (! add_dynamic_entry (DT_PLTGOT, 0)
              || ! add_dynamic_entry (DT_PLTRELSZ, 0)
              || ! add_dynamic_entry (DT_PLTREL, DT_RELA)
              || ! add_dynamic_entry (DT_JMPREL, 0))
            return FALSE;
        }

      if (relocs)
        {
          if (! add_dynamic_entry (DT_RELA, 0)
              || ! add_dynamic_entry (DT_RELASZ, 0)
              || ! add_dynamic_entry (DT_RELAENT,
                                      sizeof (Elf32_External_Rela)))
            return FALSE;

          /* If any dynamic relocs apply to a read-only section,
             then we need a DT_TEXTREL entry.  */
          if ((info->flags & DF_TEXTREL) == 0)
            elf_link_hash_traverse (&htab->root, readonly_dynrelocs,
                                    (PTR) info);

          if ((info->flags & DF_TEXTREL) != 0)
            {
              if (! add_dynamic_entry (DT_TEXTREL, 0))
                return FALSE;
            }
        }
    }
#undef add_dynamic_entry

  return TRUE;
}
/* Relocate an M32R/D ELF section.
   There is some attempt to make this function usable for many architectures,
   both for RELA and REL type relocs, if only to serve as a learning tool.

   The RELOCATE_SECTION function is called by the new ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjust the section contents as
   necessary, and (if using Rela relocs and generating a
   relocatable output file) adjusting the reloc addend as
   necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocatable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.  */

static bfd_boolean
m32r_elf_relocate_section (output_bfd, info, input_bfd, input_section,
			   contents, relocs, local_syms, local_sections)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
{
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  struct elf_link_hash_entry **sym_hashes = elf_sym_hashes (input_bfd);
  Elf_Internal_Rela *rel, *relend;
  /* Assume success.  */
  bfd_boolean ret = TRUE;

  struct elf_m32r_link_hash_table *htab = m32r_elf_hash_table (info);
  bfd *dynobj;
  bfd_vma *local_got_offsets;
  asection *sgot, *splt, *sreloc;

  dynobj = htab->root.dynobj;
  local_got_offsets = elf_local_got_offsets (input_bfd);

  sgot = htab->sgot;
  splt = htab->splt;
  sreloc = NULL;

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      /* We can't modify r_addend here as elf_link_input_bfd has an assert to
         ensure it's zero (we use REL relocs, not RELA).  Therefore this
         should be assigning zero to `addend', but for clarity we use
         `r_addend'.  */
      bfd_vma addend = rel->r_addend;
      bfd_vma offset = rel->r_offset;
      Elf_Internal_Sym *sym;
      asection *sec;
      const char *sym_name;
      bfd_reloc_status_type r;
      const char *errmsg = NULL;
      bfd_boolean use_rel = FALSE;

      h = NULL;
      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_M32R_max)
	{
	  (*_bfd_error_handler) (_("%s: unknown relocation type %d"),
				 bfd_archive_filename (input_bfd),
				 (int) r_type);
	  bfd_set_error (bfd_error_bad_value);
	  ret = FALSE;
	  continue;
	}

      if (r_type == R_M32R_GNU_VTENTRY
          || r_type == R_M32R_GNU_VTINHERIT
          || r_type == R_M32R_NONE
          || r_type == R_M32R_RELA_GNU_VTENTRY
          || r_type == R_M32R_RELA_GNU_VTINHERIT)
        continue;

      if (r_type <= R_M32R_GNU_VTENTRY)
        use_rel = TRUE;

      howto = m32r_elf_howto_table + r_type;
      r_symndx = ELF32_R_SYM (rel->r_info);

      if (info->relocatable && (use_rel == TRUE))
	{
	  /* This is a relocatable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  sec = NULL;
	  if (r_symndx >= symtab_hdr->sh_info)
	    {
	      /* External symbol.  */
	      continue;
	    }

	  /* Local symbol.  */
	  sym = local_syms + r_symndx;
	  sym_name = "<local symbol>";
	  /* STT_SECTION: symbol is associated with a section.  */
	  if (ELF_ST_TYPE (sym->st_info) != STT_SECTION)
	    {
	      /* Symbol isn't associated with a section.  Nothing to do.  */
	      continue;
	    }

	  sec = local_sections[r_symndx];
	  addend += sec->output_offset + sym->st_value;

	  /* If partial_inplace, we need to store any additional addend
	     back in the section.  */
	  if (! howto->partial_inplace)
	    continue;
	  /* ??? Here is a nice place to call a special_function
	     like handler.  */
	  if (r_type != R_M32R_HI16_SLO && r_type != R_M32R_HI16_ULO)
	    r = _bfd_relocate_contents (howto, input_bfd,
					addend, contents + offset);
	  else
	    {
	      Elf_Internal_Rela *lorel;

	      /* We allow an arbitrary number of HI16 relocs before the
		 LO16 reloc.  This permits gcc to emit the HI and LO relocs
		 itself.  */
	      for (lorel = rel + 1;
		   (lorel < relend
		    && (ELF32_R_TYPE (lorel->r_info) == R_M32R_HI16_SLO
			|| ELF32_R_TYPE (lorel->r_info) == R_M32R_HI16_ULO));
		   lorel++)
		continue;
	      if (lorel < relend
		  && ELF32_R_TYPE (lorel->r_info) == R_M32R_LO16)
		{
		  m32r_elf_relocate_hi16 (input_bfd, r_type, rel, lorel,
					  contents, addend);
		  r = bfd_reloc_ok;
		}
	      else
		r = _bfd_relocate_contents (howto, input_bfd,
					    addend, contents + offset);
	    }
	}
      else
	{
	  bfd_vma relocation;

	  /* This is a final link.  */
	  sym = NULL;
	  sec = NULL;
          h = NULL;

	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      /* Local symbol.  */
	      sym = local_syms + r_symndx;
	      sec = local_sections[r_symndx];
	      sym_name = "<local symbol>";

              if (use_rel == FALSE)
                {
	          relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);
	          addend = rel->r_addend;

                  if (info->relocatable)
                    {
                      /* This is a relocatable link.  We don't have to change
                         anything, unless the reloc is against a section symbol,
                         in which case we have to adjust according to where the
                         section symbol winds up in the output section.  */
                      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
                        rel->r_addend += sec->output_offset + sym->st_value;

                      continue;
                    }
                }
              else
                {
	          relocation = (sec->output_section->vma
			        + sec->output_offset
			        + sym->st_value);
                }
	    }
	  else
	    {
	      /* External symbol.  */
              if (info->relocatable && (use_rel == FALSE))
                continue;

	      h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	      while (h->root.type == bfd_link_hash_indirect
		     || h->root.type == bfd_link_hash_warning)
		h = (struct elf_link_hash_entry *) h->root.u.i.link;
	      sym_name = h->root.root.string;

	      if (h->root.type == bfd_link_hash_defined
		  || h->root.type == bfd_link_hash_defweak)
		{
	          bfd_boolean dyn;
		  sec = h->root.u.def.section;

	          dyn = htab->root.dynamic_sections_created;
                  sec = h->root.u.def.section;
                  if (r_type == R_M32R_GOTPC24
                      || (r_type == R_M32R_GOTPC_HI_ULO
                          || r_type == R_M32R_GOTPC_HI_SLO
                          || r_type == R_M32R_GOTPC_LO)
                      || (r_type == R_M32R_26_PLTREL
                          && h->plt.offset != (bfd_vma) -1)
                      || ((r_type == R_M32R_GOT24
                           || r_type == R_M32R_GOT16_HI_ULO
                           || r_type == R_M32R_GOT16_HI_SLO
                           || r_type == R_M32R_GOT16_LO)
                          && WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn,
							      info->shared, h)
                          && (! info->shared
                              || (! info->symbolic && h->dynindx != -1)
                              || (h->elf_link_hash_flags
                                  & ELF_LINK_HASH_DEF_REGULAR) == 0))
                      || (info->shared
                          && ((! info->symbolic && h->dynindx != -1)
                              || (h->elf_link_hash_flags
                                  & ELF_LINK_HASH_DEF_REGULAR) == 0)
                          && (((r_type == R_M32R_16_RELA
                              || r_type == R_M32R_32_RELA
                              || r_type == R_M32R_24_RELA
                              || r_type == R_M32R_HI16_ULO_RELA
                              || r_type == R_M32R_HI16_SLO_RELA
                              || r_type == R_M32R_LO16_RELA)
			          && (h->elf_link_hash_flags
				      & ELF_LINK_FORCED_LOCAL) == 0)
                              || r_type == R_M32R_10_PCREL_RELA
                              || r_type == R_M32R_18_PCREL_RELA
                              || r_type == R_M32R_26_PCREL_RELA)
                          && ((input_section->flags & SEC_ALLOC) != 0
                              /* DWARF will emit R_M32R_16(24,32) relocations
                                 in its sections against symbols defined
                                 externally in shared libraries.  We can't do
                                 anything with them here.  */
                              || ((input_section->flags & SEC_DEBUGGING) != 0
                                  && (h->elf_link_hash_flags
                                      & ELF_LINK_HASH_DEF_DYNAMIC) != 0))))
                    {
                      /* In these cases, we don't need the relocation
                         value.  We check specially because in some
                         obscure cases sec->output_section will be NULL.  */
                      relocation = 0;
                    }
		  else if (sec->output_section == NULL)
                    {
                      (*_bfd_error_handler)
                        (_("%s: warning: unresolvable relocation against symbol `%s' from %s section"),
                         bfd_get_filename (input_bfd), h->root.root.string,
                         bfd_get_section_name (input_bfd, input_section));

		       relocation = 0;
                    }
		  else
		    relocation = (h->root.u.def.value
				  + sec->output_section->vma
				  + sec->output_offset);
		}
	      else if (h->root.type == bfd_link_hash_undefweak)
		relocation = 0;
              else if (info->unresolved_syms_in_objects == RM_IGNORE
                       && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
                relocation = 0;
	      else
		{
		  if (! ((*info->callbacks->undefined_symbol)
			 (info, h->root.root.string, input_bfd,
			  input_section, offset,
                          (info->unresolved_syms_in_objects == RM_GENERATE_ERROR
                           || ELF_ST_VISIBILITY (h->other)))))
		    return FALSE;
		  relocation = 0;
		}
	    }

	  /* Sanity check the address.  */
	  if (offset > input_section->_raw_size)
	    {
	      r = bfd_reloc_outofrange;
	      goto check_reloc;
	    }

	  switch ((int) r_type)
	    {
            case R_M32R_GOTPC24:
              /* .got(_GLOBAL_OFFSET_TABLE_) - pc relocation
                 ld24 rx,#_GLOBAL_OFFSET_TABLE_
               */
             relocation = sgot->output_section->vma;
             break;

            case R_M32R_GOTPC_HI_ULO:
            case R_M32R_GOTPC_HI_SLO:
            case R_M32R_GOTPC_LO:
              {
                /* .got(_GLOBAL_OFFSET_TABLE_) - pc relocation
                   bl .+4
                   seth rx,#high(_GLOBAL_OFFSET_TABLE_)
                   or3 rx,rx,#low(_GLOBAL_OFFSET_TABLE_ +4)
                   or
                   bl .+4
                   seth rx,#shigh(_GLOBAL_OFFSET_TABLE_)
                   add3 rx,rx,#low(_GLOBAL_OFFSET_TABLE_ +4)
                 */
                relocation = sgot->output_section->vma;
                relocation -= (input_section->output_section->vma
                               + input_section->output_offset
                               + rel->r_offset);
                if ((r_type == R_M32R_GOTPC_HI_SLO)
                     && ((relocation + rel->r_addend) & 0x8000))
                  rel->r_addend += 0x10000;

                break;
              }
            case R_M32R_GOT16_HI_ULO:
            case R_M32R_GOT16_HI_SLO:
            case R_M32R_GOT16_LO:
              /* Fall through.  */
            case R_M32R_GOT24:
              /* Relocation is to the entry for this symbol in the global
                 offset table.  */
              BFD_ASSERT (sgot != NULL);

              if (h != NULL)
                {
                  bfd_boolean dyn;
                  bfd_vma off;

                  off = h->got.offset;
                  BFD_ASSERT (off != (bfd_vma) -1);

                  dyn = htab->root.dynamic_sections_created;
                  if (! WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info->shared, h)
                      || (info->shared
                          && (info->symbolic
                              || h->dynindx == -1
                              || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL))
                          && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
                    {
                      /* This is actually a static link, or it is a
                         -Bsymbolic link and the symbol is defined
                         locally, or the symbol was forced to be local
                         because of a version file.  We must initialize
                         this entry in the global offset table.  Since the
                         offset must always be a multiple of 4, we use the
                         least significant bit to record whether we have
                         initialized it already.

                         When doing a dynamic link, we create a .rela.got
                         relocation entry to initialize the value.  This
                         is done in the finish_dynamic_symbol routine.  */
                      if ((off & 1) != 0)
                        off &= ~1;
                      else
                        {
                          bfd_put_32 (output_bfd, relocation,
                                      sgot->contents + off);
                          h->got.offset |= 1;
                        }
                    }

                  relocation = sgot->output_offset + off;
                }
              else
                {
                  bfd_vma off;
                  bfd_byte *loc;

                  BFD_ASSERT (local_got_offsets != NULL
                              && local_got_offsets[r_symndx] != (bfd_vma) -1);

                  off = local_got_offsets[r_symndx];

                  /* The offset must always be a multiple of 4.  We use
                     the least significant bit to record whether we have
                     already processed this entry.  */
                  if ((off & 1) != 0)
                    off &= ~1;
                  else
                    {
                      bfd_put_32 (output_bfd, relocation, sgot->contents + off);

                      if (info->shared)
                        {
                          asection *srelgot;
                          Elf_Internal_Rela outrel;

                          /* We need to generate a R_M32R_RELATIVE reloc
                             for the dynamic linker.  */
                          srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
                          BFD_ASSERT (srelgot != NULL);

                          outrel.r_offset = (sgot->output_section->vma
                                             + sgot->output_offset
                                             + off);
                          outrel.r_info = ELF32_R_INFO (0, R_M32R_RELATIVE);
                          outrel.r_addend = relocation;
                          loc = srelgot->contents;
                          loc += srelgot->reloc_count * sizeof(Elf32_External_Rela);
                          bfd_elf32_swap_reloca_out (output_bfd, &outrel,loc);
                          ++srelgot->reloc_count;
                        }

                      local_got_offsets[r_symndx] |= 1;
                    }

                  relocation = sgot->output_offset + off;
                }
              if ((r_type == R_M32R_GOT16_HI_SLO)
                  && ((relocation + rel->r_addend) & 0x8000))
                rel->r_addend += 0x10000;

              break;

            case R_M32R_26_PLTREL:
              /* Relocation is to the entry for this symbol in the
                 procedure linkage table.  */

              /* The native assembler will generate a 26_PLTREL reloc
                 for a local symbol if you assemble a call from one
                 section to another when using -K pic. */
              if (h == NULL)
                break;

              //if (ELF_ST_VISIBILITY (h->other) == STV_INTERNAL
              //    || ELF_ST_VISIBILITY (h->other) == STV_HIDDEN)
              //  break;
              if (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL)
                break;

              if (h->plt.offset == (bfd_vma) -1)
                {
                  /* We didn't make a PLT entry for this symbol.  This
                     happens when statically linking PIC code, or when
                     using -Bsymbolic.  */
                  break;
                }

              relocation = (splt->output_section->vma
                            + splt->output_offset
                            + h->plt.offset);
              break;

            case R_M32R_HI16_SLO_RELA:
              {
                 if ((relocation + rel->r_addend) & 0x8000)
                   {
                     rel->r_addend += 0x10000;
                   }
              }
              /* Fall through.  */
            case R_M32R_16_RELA:
            case R_M32R_24_RELA:
            case R_M32R_32_RELA:
            case R_M32R_18_PCREL_RELA:
            case R_M32R_26_PCREL_RELA:
            case R_M32R_HI16_ULO_RELA:
            case R_M32R_LO16_RELA:
            case R_M32R_SDA16_RELA:
              if (info->shared
                  && r_symndx != 0
                  && (input_section->flags & SEC_ALLOC) != 0
                  && ((r_type != R_M32R_18_PCREL_RELA
                       && r_type != R_M32R_26_PCREL_RELA)
                      || (h != NULL
                          && h->dynindx != -1
                          && (! info->symbolic
                              || (h->elf_link_hash_flags
                                  & ELF_LINK_HASH_DEF_REGULAR) == 0))))
                {
                  Elf_Internal_Rela outrel;
                  bfd_boolean skip, relocate;
                  bfd_byte *loc;

                  /* When generating a shared object, these relocations
                     are copied into the output file to be resolved at run
                     time.  */

                  if (sreloc == NULL)
                    {
                      const char *name;

                      name = (bfd_elf_string_from_elf_section
                              (input_bfd,
                               elf_elfheader (input_bfd)->e_shstrndx,
                               elf_section_data (input_section)->rel_hdr.sh_name));
                      if (name == NULL)
                        return FALSE;

                      BFD_ASSERT (strncmp (name, ".rela", 5) == 0
                                  && strcmp (bfd_get_section_name (input_bfd,
                                                                   input_section),
                                             name + 5) == 0);

                      sreloc = bfd_get_section_by_name (dynobj, name);
                      BFD_ASSERT (sreloc != NULL);
                    }

                  skip = FALSE;
                  relocate = FALSE;

                  outrel.r_offset = _bfd_elf_section_offset (output_bfd,
                                                             info,
                                                             input_section,
                                                             rel->r_offset);
                  if (outrel.r_offset == (bfd_vma) -1)
                    skip = TRUE;
                  else if (outrel.r_offset == (bfd_vma) -2)
                    skip = TRUE, relocate = TRUE;
                  outrel.r_offset += (input_section->output_section->vma
                                      + input_section->output_offset);

                  if (skip)
                    memset (&outrel, 0, sizeof outrel);
                  else if (r_type == R_M32R_18_PCREL_RELA
                           || r_type == R_M32R_26_PCREL_RELA)
                    {
                      BFD_ASSERT (h != NULL && h->dynindx != -1);
                      outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
                      outrel.r_addend = rel->r_addend;
                    }
                  else
                    {
                    /* h->dynindx may be -1 if this symbol was marked to
                       become local.  */
                    if (h == NULL
                        || ((info->symbolic || h->dynindx == -1)
                             && (h->elf_link_hash_flags
                                 & ELF_LINK_HASH_DEF_REGULAR) != 0))
                      {
                        relocate = TRUE;
                        outrel.r_info = ELF32_R_INFO (0, R_M32R_RELATIVE);
                        outrel.r_addend = relocation + rel->r_addend;
                      }
                    else
                      {
                        BFD_ASSERT (h->dynindx != -1);
                        outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
                        outrel.r_addend = relocation + rel->r_addend;
                      }
                    }

                  loc = sreloc->contents;
                  loc += sreloc->reloc_count * sizeof(Elf32_External_Rela);
                  bfd_elf32_swap_reloca_out (output_bfd, &outrel,loc);
                  ++sreloc->reloc_count;

                  /* If this reloc is against an external symbol, we do
                     not want to fiddle with the addend.  Otherwise, we
                     need to include the symbol value so that it becomes
                     an addend for the dynamic reloc.  */
                  if (! relocate)
                    continue;
                }
              break;

	    case (int) R_M32R_10_PCREL :
	      r = m32r_elf_do_10_pcrel_reloc (input_bfd, howto, input_section,
					      contents, offset,
					      sec, relocation, addend);
              goto check_reloc;

	    case (int) R_M32R_HI16_SLO :
	    case (int) R_M32R_HI16_ULO :
	      {
		Elf_Internal_Rela *lorel;

		/* We allow an arbitrary number of HI16 relocs before the
		   LO16 reloc.  This permits gcc to emit the HI and LO relocs
		   itself.  */
		for (lorel = rel + 1;
		     (lorel < relend
		      && (ELF32_R_TYPE (lorel->r_info) == R_M32R_HI16_SLO
			  || ELF32_R_TYPE (lorel->r_info) == R_M32R_HI16_ULO));
		     lorel++)
		  continue;
		if (lorel < relend
		    && ELF32_R_TYPE (lorel->r_info) == R_M32R_LO16)
		  {
		    m32r_elf_relocate_hi16 (input_bfd, r_type, rel, lorel,
					    contents, relocation + addend);
		    r = bfd_reloc_ok;
		  }
		else
		  r = _bfd_final_link_relocate (howto, input_bfd, input_section,
						contents, offset,
						relocation, addend);
	      }

              goto check_reloc;

	    case (int) R_M32R_SDA16 :
	      {
		const char *name;

		BFD_ASSERT (sec != NULL);
		name = bfd_get_section_name (abfd, sec);

		if (strcmp (name, ".sdata") == 0
		    || strcmp (name, ".sbss") == 0
		    || strcmp (name, ".scommon") == 0)
		  {
		    bfd_vma sda_base;
		    bfd *out_bfd = sec->output_section->owner;

		    r = m32r_elf_final_sda_base (out_bfd, info,
						 &errmsg,
						 &sda_base);
		    if (r != bfd_reloc_ok)
		      {
			ret = FALSE;
			goto check_reloc;
		      }

		    /* At this point `relocation' contains the object's
		       address.  */
		    relocation -= sda_base;
		    /* Now it contains the offset from _SDA_BASE_.  */
		  }
		else
		  {
		    (*_bfd_error_handler)
		      (_("%s: The target (%s) of an %s relocation is in the wrong section (%s)"),
		       bfd_archive_filename (input_bfd),
		       sym_name,
		       m32r_elf_howto_table[(int) r_type].name,
		       bfd_get_section_name (abfd, sec));
		    /*bfd_set_error (bfd_error_bad_value); ??? why? */
		    ret = FALSE;
		    continue;
		  }
	      }
              /* fall through */

	    default : /* OLD_M32R_RELOC */

	      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					    contents, offset,
					    relocation, addend);
	      goto check_reloc;
	    }

          r = _bfd_final_link_relocate (howto, input_bfd, input_section,
                                        contents, rel->r_offset,
                                        relocation, rel->r_addend);

	}

    check_reloc:

      if (r != bfd_reloc_ok)
	{
	  /* FIXME: This should be generic enough to go in a utility.  */
	  const char *name;

	  if (h != NULL)
	    name = h->root.root.string;
	  else
	    {
	      name = (bfd_elf_string_from_elf_section
		      (input_bfd, symtab_hdr->sh_link, sym->st_name));
	      if (name == NULL || *name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  if (errmsg != NULL)
	    goto common_error;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      if (! ((*info->callbacks->reloc_overflow)
		     (info, name, howto->name, (bfd_vma) 0,
		      input_bfd, input_section, offset)))
		return FALSE;
	      break;

	    case bfd_reloc_undefined:
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, name, input_bfd, input_section,
		      offset, TRUE)))
		return FALSE;
	      break;

	    case bfd_reloc_outofrange:
	      errmsg = _("internal error: out of range error");
	      goto common_error;

	    case bfd_reloc_notsupported:
	      errmsg = _("internal error: unsupported relocation error");
	      goto common_error;

	    case bfd_reloc_dangerous:
	      errmsg = _("internal error: dangerous error");
	      goto common_error;

	    default:
	      errmsg = _("internal error: unknown error");
	      /* fall through */

	    common_error:
	      if (!((*info->callbacks->warning)
		    (info, errmsg, name, input_bfd, input_section,
		     offset)))
		return FALSE;
	      break;
	    }
	}
    }

  return ret;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */
static bfd_boolean
m32r_elf_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  struct elf_m32r_link_hash_table *htab;
  bfd *dynobj;
  bfd_byte *loc;

#ifdef DEBUG_PIC
printf("m32r_elf_finish_dynamic_symbol()\n");
#endif

  htab = m32r_elf_hash_table (info);
  dynobj = htab->root.dynobj;

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *sgot;
      asection *srela;

      bfd_vma plt_index;
      bfd_vma got_offset;
      Elf_Internal_Rela rela;

      /* This symbol has an entry in the procedure linkage table.  Set
         it up.  */

      BFD_ASSERT (h->dynindx != -1);

      splt = htab->splt;
      sgot = htab->sgotplt;
      srela = htab->srelplt;
      BFD_ASSERT (splt != NULL && sgot != NULL && srela != NULL);

      /* Get the index in the procedure linkage table which
         corresponds to this symbol.  This is the index of this symbol
         in all the symbols for which we are making plt entries.  The
         first entry in the procedure linkage table is reserved.  */
      plt_index = h->plt.offset / PLT_ENTRY_SIZE - 1;

      /* Get the offset into the .got table of the entry that
        corresponds to this function.  Each .got entry is 4 bytes.
        The first three are reserved.  */
      got_offset = (plt_index + 3) * 4;

      /* Fill in the entry in the procedure linkage table.  */
      if (! info->shared)
        {
          bfd_put_32 (output_bfd,
              (PLT_ENTRY_WORD0b
               + (((sgot->output_section->vma
                    + sgot->output_offset
                    + got_offset) >> 16) & 0xffff)),
              splt->contents + h->plt.offset);
          bfd_put_32 (output_bfd,
              (PLT_ENTRY_WORD1b
               + ((sgot->output_section->vma
                   + sgot->output_offset
                   + got_offset) & 0xffff)),
              splt->contents + h->plt.offset + 4);
          bfd_put_32 (output_bfd, PLT_ENTRY_WORD2,
              splt->contents + h->plt.offset + 8);
          bfd_put_32 (output_bfd,
              (PLT_ENTRY_WORD3
               + plt_index * sizeof (Elf32_External_Rela)),
              splt->contents + h->plt.offset + 12);
          bfd_put_32 (output_bfd,
              (PLT_ENTRY_WORD4
               + (((unsigned int) ((- (h->plt.offset + 16)) >> 2)) & 0xffffff)),
              splt->contents + h->plt.offset + 16);
        }
      else
        {
          bfd_put_32 (output_bfd,
              PLT_ENTRY_WORD0 + got_offset,
              splt->contents + h->plt.offset);
          bfd_put_32 (output_bfd, PLT_ENTRY_WORD1,
              splt->contents + h->plt.offset + 4);
          bfd_put_32 (output_bfd, PLT_ENTRY_WORD2,
              splt->contents + h->plt.offset + 8);
          bfd_put_32 (output_bfd,
              (PLT_ENTRY_WORD3
               + plt_index * sizeof (Elf32_External_Rela)),
              splt->contents + h->plt.offset + 12);
          bfd_put_32 (output_bfd,
              (PLT_ENTRY_WORD4
               + (((unsigned int) ((- (h->plt.offset + 16)) >> 2)) & 0xffffff)),
              splt->contents + h->plt.offset + 16);
        }

      /* Fill in the entry in the global offset table.  */
      bfd_put_32 (output_bfd,
                  (splt->output_section->vma
                   + splt->output_offset
                   + h->plt.offset
                   + 12), /* same offset */
                  sgot->contents + got_offset);

      /* Fill in the entry in the .rela.plt section.  */
      rela.r_offset = (sgot->output_section->vma
                       + sgot->output_offset
                       + got_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_M32R_JMP_SLOT);
      rela.r_addend = 0;
      loc = srela->contents;
      loc += plt_index * sizeof(Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
        {
          /* Mark the symbol as undefined, rather than as defined in
             the .plt section.  Leave the value alone.  */
          sym->st_shndx = SHN_UNDEF;
        }
    }

  if (h->got.offset != (bfd_vma) -1)
    {
      asection *sgot;
      asection *srela;
      Elf_Internal_Rela rela;

      /* This symbol has an entry in the global offset table.  Set it
         up.  */

      sgot = htab->sgot;
      srela = htab->srelgot;
      BFD_ASSERT (sgot != NULL && srela != NULL);

      rela.r_offset = (sgot->output_section->vma
                       + sgot->output_offset
                       + (h->got.offset &~ 1));

      /* If this is a -Bsymbolic link, and the symbol is defined
         locally, we just want to emit a RELATIVE reloc.  Likewise if
         the symbol was forced to be local because of a version file.
         The entry in the global offset table will already have been
         initialized in the relocate_section function.  */
      if (info->shared
          && (info->symbolic
	      || h->dynindx == -1
	      || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL))
          && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
        {
          rela.r_info = ELF32_R_INFO (0, R_M32R_RELATIVE);
          rela.r_addend = (h->root.u.def.value
                           + h->root.u.def.section->output_section->vma
                           + h->root.u.def.section->output_offset);
        }
      else
        {
	  BFD_ASSERT((h->got.offset & 1) == 0);
          bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + h->got.offset);
          rela.r_info = ELF32_R_INFO (h->dynindx, R_M32R_GLOB_DAT);
          rela.r_addend = 0;
        }

      loc = srela->contents;
      loc += srela->reloc_count * sizeof(Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
      ++srela->reloc_count;
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
    {
      asection *s;
      Elf_Internal_Rela rela;

      /* This symbols needs a copy reloc.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1
                  && (h->root.type == bfd_link_hash_defined
                      || h->root.type == bfd_link_hash_defweak));

      s = bfd_get_section_by_name (h->root.u.def.section->owner,
                                   ".rela.bss");
      BFD_ASSERT (s != NULL);

      rela.r_offset = (h->root.u.def.value
                       + h->root.u.def.section->output_section->vma
                       + h->root.u.def.section->output_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_M32R_COPY);
      rela.r_addend = 0;
      loc = s->contents;
      loc += s->reloc_count * sizeof(Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
      ++s->reloc_count;
    }

  /* Mark some specially defined symbols as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return TRUE;
}


/* Finish up the dynamic sections.  */

static bfd_boolean
m32r_elf_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  struct elf_m32r_link_hash_table *htab;
  bfd *dynobj;
  asection *sdyn;
  asection *sgot;

#ifdef DEBUG_PIC
printf("m32r_elf_finish_dynamic_sections()\n");
#endif

  htab = m32r_elf_hash_table (info);
  dynobj = htab->root.dynobj;

  sgot = htab->sgotplt;
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (htab->root.dynamic_sections_created)
    {
      asection *splt;
      Elf32_External_Dyn *dyncon, *dynconend;

      BFD_ASSERT (sgot != NULL && sdyn != NULL);

      dyncon = (Elf32_External_Dyn *) sdyn->contents;
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->_raw_size);

      for (; dyncon < dynconend; dyncon++)
        {
          Elf_Internal_Dyn dyn;
          const char *name;
          asection *s;

          bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

          switch (dyn.d_tag)
            {
            default:
              break;

            case DT_PLTGOT:
              name = ".got";
              s = htab->sgot->output_section;
              goto get_vma;
            case DT_JMPREL:
              name = ".rela.plt";
              s = htab->srelplt->output_section;
            get_vma:
              BFD_ASSERT (s != NULL);
              dyn.d_un.d_ptr = s->vma;
              bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
              break;

            case DT_PLTRELSZ:
              s = htab->srelplt->output_section;
              BFD_ASSERT (s != NULL);
              if (s->_cooked_size != 0)
                dyn.d_un.d_val = s->_cooked_size;
              else
                dyn.d_un.d_val = s->_raw_size;
              bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
              break;

            case DT_RELASZ:
              /* My reading of the SVR4 ABI indicates that the
                 procedure linkage table relocs (DT_JMPREL) should be
                 included in the overall relocs (DT_RELA).  This is
                 what Solaris does.  However, UnixWare can not handle
                 that case.  Therefore, we override the DT_RELASZ entry
                 here to make it not include the JMPREL relocs.  Since
                 the linker script arranges for .rela.plt to follow all
                 other relocation sections, we don't have to worry
                 about changing the DT_RELA entry.  */
              if (htab->srelplt != NULL)
                {
                  s = htab->srelplt->output_section;
                  if (s->_cooked_size != 0)
                    dyn.d_un.d_val -= s->_cooked_size;
                  else
                    dyn.d_un.d_val -= s->_raw_size;
                }
              bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
              break;
            }
        }

      /* Fill in the first entry in the procedure linkage table.  */
      splt = htab->splt;
      if (splt && splt->_raw_size > 0)
        {
          if (info->shared)
            {
              bfd_put_32 (output_bfd, PLT0_PIC_ENTRY_WORD0, splt->contents);
              bfd_put_32 (output_bfd, PLT0_PIC_ENTRY_WORD1, splt->contents + 4);
              bfd_put_32 (output_bfd, PLT0_PIC_ENTRY_WORD2, splt->contents + 8);
              bfd_put_32 (output_bfd, PLT0_PIC_ENTRY_WORD3, splt->contents + 12);
              bfd_put_32 (output_bfd, PLT0_PIC_ENTRY_WORD4, splt->contents + 16);
            }
          else
            {
              unsigned long addr;
              /* addr = .got + 4 */
              addr = sgot->output_section->vma + sgot->output_offset + 4;
              bfd_put_32 (output_bfd,
			  PLT0_ENTRY_WORD0 | ((addr >> 16) & 0xffff),
			  splt->contents);
              bfd_put_32 (output_bfd,
			  PLT0_ENTRY_WORD1 | (addr & 0xffff),
			  splt->contents + 4);
              bfd_put_32 (output_bfd, PLT0_ENTRY_WORD2, splt->contents + 8);
              bfd_put_32 (output_bfd, PLT0_ENTRY_WORD3, splt->contents + 12);
              bfd_put_32 (output_bfd, PLT0_ENTRY_WORD4, splt->contents + 16);
            }

          elf_section_data (splt->output_section)->this_hdr.sh_entsize =
            PLT_ENTRY_SIZE;
        }
    }

  /* Fill in the first three entries in the global offset table.  */
  if (sgot && sgot->_raw_size > 0)
    {
      if (sdyn == NULL)
        bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents);
      else
        bfd_put_32 (output_bfd,
                    sdyn->output_section->vma + sdyn->output_offset,
                    sgot->contents);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + 4);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + 8);

      elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 4;
    }

  return TRUE;
}


#if 0 /* relaxing not supported yet */

/* This function handles relaxing for the m32r.
   Relaxing on the m32r is tricky because of instruction alignment
   requirements (4 byte instructions must be aligned on 4 byte boundaries).

   The following relaxing opportunities are handled:

   seth/add3/jl -> bl24 or bl8
   seth/add3 -> ld24

   It would be nice to handle bl24 -> bl8 but given:

   - 4 byte insns must be on 4 byte boundaries
   - branch instructions only branch to insns on 4 byte boundaries

   this isn't much of a win because the insn in the 2 "deleted" bytes
   must become a nop.  With some complexity some real relaxation could be
   done but the frequency just wouldn't make it worth it; it's better to
   try to do all the code compaction one can elsewhere.
   When the chip supports parallel 16 bit insns, things may change.
*/

static bfd_boolean
m32r_elf_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     bfd_boolean *again;
{
  Elf_Internal_Shdr *symtab_hdr;
  /* The Rela structures are used here because that's what
     _bfd_elf_link_read_relocs uses [for convenience - it sets the addend
     field to 0].  */
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents = NULL;
  Elf_Internal_Sym *isymbuf = NULL;

  /* Assume nothing changes.  */
  *again = FALSE;

  /* We don't have to do anything for a relocatable link, if
     this section does not have relocs, or if this is not a
     code section.  */
  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0
      || 0 /* FIXME: check SHF_M32R_CAN_RELAX */)
    return TRUE;

  /* If this is the first time we have been called for this section,
     initialize the cooked size.  */
  if (sec->_cooked_size == 0)
    sec->_cooked_size = sec->_raw_size;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs = (_bfd_elf_link_read_relocs
		     (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;

  /* Walk through them looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;

      /* If this isn't something that can be relaxed, then ignore
	 this reloc.  */
      if (ELF32_R_TYPE (irel->r_info) != (int) R_M32R_HI16_SLO)
	continue;

      /* Get the section contents if we haven't done so already.  */
      if (contents == NULL)
	{
	  /* Get cached copy if it exists.  */
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    contents = elf_section_data (sec)->this_hdr.contents;
	  else
	    {
	      /* Go get them off disk.  */
	      contents = (bfd_byte *) bfd_malloc (sec->_raw_size);
	      if (contents == NULL)
		goto error_return;

	      if (! bfd_get_section_contents (abfd, sec, contents,
					      (file_ptr) 0, sec->_raw_size))
		goto error_return;
	    }
	}

      /* Read this BFD's local symbols if we haven't done so already.  */
      if (isymbuf == NULL && symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  Elf_Internal_Sym *isym;
	  asection *sym_sec;

	  isym = isymbuf + ELF32_R_SYM (irel->r_info),
	  sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
	  symval = (isym->st_value
		    + sym_sec->output_section->vma
		    + sym_sec->output_offset);
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry *h;

	  /* An external symbol.  */
	  indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);
	  if (h->root.type != bfd_link_hash_defined
	      && h->root.type != bfd_link_hash_defweak)
	    {
	      /* This appears to be a reference to an undefined
                 symbol.  Just ignore it--it will be caught by the
                 regular reloc processing.  */
	      continue;
	    }

	  symval = (h->root.u.def.value
		    + h->root.u.def.section->output_section->vma
		    + h->root.u.def.section->output_offset);
	}

      /* For simplicity of coding, we are going to modify the section
	 contents, the section relocs, and the BFD symbol table.  We
	 must tell the rest of the code not to free up this
	 information.  It would be possible to instead create a table
	 of changes which have to be made, as is done in coff-mips.c;
	 that would be more work, but would require less memory when
	 the linker is run.  */

      /* Try to change a seth/add3/jl subroutine call to bl24 or bl8.
	 This sequence is generated by the compiler when compiling in
	 32 bit mode.  Also look for seth/add3 -> ld24.  */

      if (ELF32_R_TYPE (irel->r_info) == (int) R_M32R_HI16_SLO)
	{
	  Elf_Internal_Rela *nrel;
	  bfd_vma pc = (sec->output_section->vma + sec->output_offset
			+ irel->r_offset);
	  bfd_signed_vma pcrel_value = symval - pc;
	  unsigned int code,reg;
	  int addend,nop_p,bl8_p,to_delete;

	  /* The tests are ordered so that we get out as quickly as possible
	     if this isn't something we can relax, taking into account that
	     we are looking for two separate possibilities (jl/ld24).  */

	  /* Do nothing if no room in the section for this to be what we're
	     looking for.  */
	  if (irel->r_offset > sec->_cooked_size - 8)
	    continue;

	  /* Make sure the next relocation applies to the next
	     instruction and that it's the add3's reloc.  */
	  nrel = irel + 1;
	  if (nrel == irelend
	      || irel->r_offset + 4 != nrel->r_offset
	      || ELF32_R_TYPE (nrel->r_info) != (int) R_M32R_LO16)
	    continue;

	  /* See if the instructions are seth/add3.  */
	  /* FIXME: This is where macros from cgen can come in.  */
	  code = bfd_get_16 (abfd, contents + irel->r_offset + 0);
	  if ((code & 0xf0ff) != 0xd0c0)
	    continue; /* not seth rN,foo */
	  reg = (code & 0x0f00) >> 8;
	  code = bfd_get_16 (abfd, contents + irel->r_offset + 4);
	  if (code != (0x80a0 | reg | (reg << 8)))
	    continue; /* not add3 rN,rN,foo */

	  /* At this point we've confirmed we have seth/add3.  Now check
	     whether the next insn is a jl, in which case try to change this
	     to bl24 or bl8.  */

	  /* Ensure the branch target is in range.
	     The bl24 instruction has a 24 bit operand which is the target
	     address right shifted by 2, giving a signed range of 26 bits.
	     Note that 4 bytes are added to the high value because the target
	     will be at least 4 bytes closer if we can relax.  It'll actually
	     be 4 or 8 bytes closer, but we don't know which just yet and
	     the difference isn't significant enough to worry about.  */
#if !USE_REL /* put in for learning purposes */
	  pcrel_value += irel->r_addend;
#else
	  addend = bfd_get_signed_16 (abfd, contents + irel->r_offset + 2);
	  pcrel_value += addend;
#endif

	  if (pcrel_value >= -(1 << 25) && pcrel_value < (1 << 25) + 4
	      /* Do nothing if no room in the section for this to be what we're
		 looking for.  */
	      && (irel->r_offset <= sec->_cooked_size - 12)
	      /* Ensure the next insn is "jl rN".  */
	      && ((code = bfd_get_16 (abfd, contents + irel->r_offset + 8)),
		  code != (0x1ec0 | reg)))
	    {
	      /* We can relax to bl24/bl8.  */

	      /* See if there's a nop following the jl.
		 Also see if we can use a bl8 insn.  */
	      code = bfd_get_16 (abfd, contents + irel->r_offset + 10);
	      nop_p = (code & 0x7fff) == NOP_INSN;
	      bl8_p = pcrel_value >= -0x200 && pcrel_value < 0x200;

	      if (bl8_p)
		{
		  /* Change "seth rN,foo" to "bl8 foo || nop".
		     We OR in CODE just in case it's not a nop (technically,
		     CODE currently must be a nop, but for cleanness we
		     allow it to be anything).  */
#if !USE_REL /* put in for learning purposes */
		  code = 0x7e000000 | MAKE_PARALLEL (code);
#else
		  code = (0x7e000000 + (((addend >> 2) & 0xff) << 16)) | MAKE_PARALLEL (code);
#endif
		  to_delete = 8;
		}
	      else
		{
		  /* Change the seth rN,foo to a bl24 foo.  */
#if !USE_REL /* put in for learning purposes */
		  code = 0xfe000000;
#else
		  code = 0xfe000000 + ((addend >> 2) & 0xffffff);
#endif
		  to_delete = nop_p ? 8 : 4;
		}

	      bfd_put_32 (abfd, code, contents + irel->r_offset);

	      /* Set the new reloc type.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (nrel->r_info),
					   bl8_p ? R_M32R_10_PCREL : R_M32R_26_PCREL);

	      /* Delete the add3 reloc by making it a null reloc.  */
	      nrel->r_info = ELF32_R_INFO (ELF32_R_SYM (nrel->r_info),
					   R_M32R_NONE);
	    }
	  else if (addend >= 0
		   && symval + addend <= 0xffffff)
	    {
	      /* We can relax to ld24.  */

	      code = 0xe0000000 | (reg << 24) | (addend & 0xffffff);
	      bfd_put_32 (abfd, code, contents + irel->r_offset);
	      to_delete = 4;
	      /* Tell the following code a nop filler isn't needed.  */
	      nop_p = 1;
	    }
	  else
	    {
	      /* Can't do anything here.  */
	      continue;
	    }

	  /* Note that we've changed the relocs, section contents, etc.  */
	  elf_section_data (sec)->relocs = internal_relocs;
	  elf_section_data (sec)->this_hdr.contents = contents;
	  symtab_hdr->contents = (unsigned char *) isymbuf;

	  /* Delete TO_DELETE bytes of data.  */
	  if (!m32r_elf_relax_delete_bytes (abfd, sec,
					    irel->r_offset + 4, to_delete))
	    goto error_return;

	  /* Now that the following bytes have been moved into place, see if
	     we need to replace the jl with a nop.  This happens when we had
	     to use a bl24 insn and the insn following the jl isn't a nop.
	     Technically, this situation can't happen (since the insn can
	     never be executed) but to be clean we do this.  When the chip
	     supports parallel 16 bit insns things may change.
	     We don't need to do this in the case of relaxing to ld24,
	     and the above code sets nop_p so this isn't done.  */
	  if (! nop_p && to_delete == 4)
	    bfd_put_16 (abfd, NOP_INSN, contents + irel->r_offset + 4);

	  /* That will change things, so we should relax again.
	     Note that this is not required, and it may be slow.  */
	  *again = TRUE;

	  continue;
	}

      /* loop to try the next reloc */
    }

  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    {
      if (! link_info->keep_memory)
	free (isymbuf);
      else
	{
	  /* Cache the symbols for elf_link_input_bfd.  */
	  symtab_hdr->contents = (unsigned char *) isymbuf;
	}
    }

  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    {
      if (! link_info->keep_memory)
	free (contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = contents;
	}
    }

  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return TRUE;

 error_return:
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return FALSE;
}

/* Delete some bytes from a section while relaxing.  */

static bfd_boolean
m32r_elf_relax_delete_bytes (abfd, sec, addr, count)
     bfd *abfd;
     asection *sec;
     bfd_vma addr;
     int count;
{
  Elf_Internal_Shdr *symtab_hdr;
  int shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend;
  Elf_Internal_Rela *irelalign;
  bfd_vma toaddr;
  Elf_Internal_Sym *isym, *isymend;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;

  shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  /* The deletion must stop at the next ALIGN reloc for an aligment
     power larger than the number of bytes we are deleting.  */

  irelalign = NULL;
  toaddr = sec->_cooked_size;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count, toaddr - addr - count);
  sec->_cooked_size -= count;

  /* Adjust all the relocs.  */
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      /* Get the new reloc address.  */
      if ((irel->r_offset > addr
	   && irel->r_offset < toaddr))
	irel->r_offset -= count;
    }

  /* Adjust the local symbols defined in this section.  */
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isym = (Elf_Internal_Sym *) symtab_hdr->contents;
  for (isymend = isym + symtab_hdr->sh_info; isym < isymend; isym++)
    {
      if (isym->st_shndx == shndx
	  && isym->st_value > addr
	  && isym->st_value < toaddr)
	isym->st_value -= count;
    }

  /* Now adjust the global symbols defined in this section.  */
  symcount = (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
	      - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;

      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec
	  && sym_hash->root.u.def.value > addr
	  && sym_hash->root.u.def.value < toaddr)
	{
	  sym_hash->root.u.def.value -= count;
	}
    }

  return TRUE;
}

/* This is a version of bfd_generic_get_relocated_section_contents
   which uses m32r_elf_relocate_section.  */

static bfd_byte *
m32r_elf_get_relocated_section_contents (output_bfd, link_info, link_order,
					 data, relocatable, symbols)
     bfd *output_bfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     bfd_boolean relocatable;
     asymbol **symbols;
{
  Elf_Internal_Shdr *symtab_hdr;
  asection *input_section = link_order->u.indirect.section;
  bfd *input_bfd = input_section->owner;
  asection **sections = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Sym *isymbuf = NULL;
  bfd_size_type amt;

  /* We only need to handle the case of relaxing, or of having a
     particular set of section contents, specially.  */
  if (relocatable
      || elf_section_data (input_section)->this_hdr.contents == NULL)
    return bfd_generic_get_relocated_section_contents (output_bfd, link_info,
						       link_order, data,
						       relocatable,
						       symbols);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  memcpy (data, elf_section_data (input_section)->this_hdr.contents,
	  input_section->_raw_size);

  if ((input_section->flags & SEC_RELOC) != 0
      && input_section->reloc_count > 0)
    {
      Elf_Internal_Sym *isymp;
      asection **secpp;
      Elf32_External_Sym *esym, *esymend;

      internal_relocs = (_bfd_elf_link_read_relocs
			 (input_bfd, input_section, (PTR) NULL,
			  (Elf_Internal_Rela *) NULL, FALSE));
      if (internal_relocs == NULL)
	goto error_return;

      if (symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (input_bfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      amt = symtab_hdr->sh_info;
      amt *= sizeof (asection *);
      sections = (asection **) bfd_malloc (amt);
      if (sections == NULL && symtab_hdr->sh_info > 0)
	goto error_return;

      isymend = isymbuf + symtab_hdr->sh_info;
      for (isym = isymbuf, secpp = sections; isym < isymend; ++isym, ++secpp)
	{
	  asection *isec;

	  if (isym->st_shndx == SHN_UNDEF)
	    isec = bfd_und_section_ptr;
	  else if (isym->st_shndx == SHN_ABS)
	    isec = bfd_abs_section_ptr;
	  else if (isym->st_shndx == SHN_COMMON)
	    isec = bfd_com_section_ptr;
	  else if (isym->st_shndx == SHN_M32R_SCOMMON)
	    isec = &m32r_elf_scom_section;
	  else
	    isec = bfd_section_from_elf_index (input_bfd, isym->st_shndx);

	  *secpp = isec;
	}

      if (! m32r_elf_relocate_section (output_bfd, link_info, input_bfd,
				       input_section, data, internal_relocs,
				       isymbuf, sections))
	goto error_return;

      if (sections != NULL)
	free (sections);
      if (isymbuf != NULL
	  && symtab_hdr->contents != (unsigned char *) isymbuf)
	free (isymbuf);
      if (elf_section_data (input_section)->relocs != internal_relocs)
	free (internal_relocs);
    }

  return data;

 error_return:
  if (sections != NULL)
    free (sections);
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (internal_relocs != NULL
      && elf_section_data (input_section)->relocs != internal_relocs)
    free (internal_relocs);
  return NULL;
}

#endif /* #if 0 */

/* Set the right machine number.  */
static bfd_boolean
m32r_elf_object_p (abfd)
     bfd *abfd;
{
  switch (elf_elfheader (abfd)->e_flags & EF_M32R_ARCH)
    {
    default:
    case E_M32R_ARCH:   (void) bfd_default_set_arch_mach (abfd, bfd_arch_m32r, bfd_mach_m32r);  break;
    case E_M32RX_ARCH:  (void) bfd_default_set_arch_mach (abfd, bfd_arch_m32r, bfd_mach_m32rx); break;
    case E_M32R2_ARCH:  (void) bfd_default_set_arch_mach (abfd, bfd_arch_m32r, bfd_mach_m32r2); break;
    }
  return TRUE;
}

/* Store the machine number in the flags field.  */
static void
m32r_elf_final_write_processing (abfd, linker)
     bfd *abfd;
     bfd_boolean linker ATTRIBUTE_UNUSED;
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
    default:
    case bfd_mach_m32r:  val = E_M32R_ARCH; break;
    case bfd_mach_m32rx: val = E_M32RX_ARCH; break;
    case bfd_mach_m32r2: val = E_M32R2_ARCH; break;
    }

  elf_elfheader (abfd)->e_flags &=~ EF_M32R_ARCH;
  elf_elfheader (abfd)->e_flags |= val;
}

/* Function to keep M32R specific file flags.  */
static bfd_boolean
m32r_elf_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */
static bfd_boolean
m32r_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword out_flags;
  flagword in_flags;

  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  in_flags  = elf_elfheader (ibfd)->e_flags;
  out_flags = elf_elfheader (obfd)->e_flags;

  if (! elf_flags_init (obfd))
    {
      /* If the input is the default architecture then do not
	 bother setting the flags for the output architecture,
	 instead allow future merges to do this.  If no future
	 merges ever set these flags then they will retain their
	 unitialised values, which surprise surprise, correspond
	 to the default values.  */
      if (bfd_get_arch_info (ibfd)->the_default)
	return TRUE;

      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = in_flags;

      if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
	  && bfd_get_arch_info (obfd)->the_default)
	{
	  return bfd_set_arch_mach (obfd, bfd_get_arch (ibfd), bfd_get_mach (ibfd));
	}

      return TRUE;
    }

  /* Check flag compatibility.  */
  if (in_flags == out_flags)
    return TRUE;

  if ((in_flags & EF_M32R_ARCH) != (out_flags & EF_M32R_ARCH))
    {
      if (   ((in_flags  & EF_M32R_ARCH) != E_M32R_ARCH)
          || ((out_flags & EF_M32R_ARCH) == E_M32R_ARCH)
          || ((in_flags  & EF_M32R_ARCH) == E_M32R2_ARCH))
	{
	  (*_bfd_error_handler)
	    (_("%s: Instruction set mismatch with previous modules"),
	     bfd_archive_filename (ibfd));

	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}
    }

  return TRUE;
}

/* Display the flags field */
static bfd_boolean
m32r_elf_print_private_bfd_data (abfd, ptr)
     bfd *abfd;
     PTR ptr;
{
  FILE * file = (FILE *) ptr;

  BFD_ASSERT (abfd != NULL && ptr != NULL)

  _bfd_elf_print_private_bfd_data (abfd, ptr);

  fprintf (file, _("private flags = %lx"), elf_elfheader (abfd)->e_flags);

  switch (elf_elfheader (abfd)->e_flags & EF_M32R_ARCH)
    {
    default:
    case E_M32R_ARCH:  fprintf (file, _(": m32r instructions"));  break;
    case E_M32RX_ARCH: fprintf (file, _(": m32rx instructions")); break;
    case E_M32R2_ARCH: fprintf (file, _(": m32r2 instructions")); break;
    }

  fputc ('\n', file);

  return TRUE;
}

asection *
m32r_elf_gc_mark_hook (sec, info, rel, h, sym)
     asection *sec;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
      {
      case R_M32R_GNU_VTINHERIT:
      case R_M32R_GNU_VTENTRY:
      case R_M32R_RELA_GNU_VTINHERIT:
      case R_M32R_RELA_GNU_VTENTRY:
        break;

      default:
        switch (h->root.type)
          {
          case bfd_link_hash_defined:
          case bfd_link_hash_defweak:
            return h->root.u.def.section;

          case bfd_link_hash_common:
            return h->root.u.c.p->section;

	  default:
	    break;
          }
       }
     }
   else
     return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}

static bfd_boolean
m32r_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  /* Update the got entry reference counts for the section being removed.  */
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;
  unsigned long r_symndx;
  struct elf_link_hash_entry *h;

  elf_section_data (sec)->local_dynrel = NULL;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    switch (ELF32_R_TYPE (rel->r_info))
      {
      case R_M32R_GOT16_HI_ULO:
      case R_M32R_GOT16_HI_SLO:
      case R_M32R_GOT16_LO:
      case R_M32R_GOT24:
      case R_M32R_GOTPC_HI_ULO:
      case R_M32R_GOTPC_HI_SLO:
      case R_M32R_GOTPC_LO:
      case R_M32R_GOTPC24:
	r_symndx = ELF32_R_SYM (rel->r_info);
	if (r_symndx >= symtab_hdr->sh_info)
	  {
	    h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	    if (h->got.refcount > 0)
	      h->got.refcount--;
	  }
	else
	  {
	    if (local_got_refcounts && local_got_refcounts[r_symndx] > 0)
	      local_got_refcounts[r_symndx]--;
	  }
        break;

      case R_M32R_16_RELA:
      case R_M32R_24_RELA:
      case R_M32R_32_RELA:
      case R_M32R_HI16_ULO_RELA:
      case R_M32R_HI16_SLO_RELA:
      case R_M32R_LO16_RELA:
      case R_M32R_SDA16_RELA:
      case R_M32R_18_PCREL_RELA:
      case R_M32R_26_PCREL_RELA:
        r_symndx = ELF32_R_SYM (rel->r_info);
        if (r_symndx >= symtab_hdr->sh_info)
          {
            struct elf_m32r_link_hash_entry *eh;
            struct elf_m32r_dyn_relocs **pp;
            struct elf_m32r_dyn_relocs *p;

            h = sym_hashes[r_symndx - symtab_hdr->sh_info];

            if (!info->shared && h->plt.refcount > 0)
              h->plt.refcount -= 1;

            eh = (struct elf_m32r_link_hash_entry *) h;

            for (pp = &eh->dyn_relocs; (p = *pp) != NULL; pp = &p->next)
              if (p->sec == sec)
                {
                  if (ELF32_R_TYPE (rel->r_info) == R_M32R_26_PCREL_RELA
                      || ELF32_R_TYPE (rel->r_info) == R_M32R_26_PCREL_RELA)
                    p->pc_count -= 1;
                  p->count -= 1;
                  if (p->count == 0)
                    *pp = p->next;
                  break;
                }
          }
        break;

      case R_M32R_26_PLTREL:
	r_symndx = ELF32_R_SYM (rel->r_info);
	if (r_symndx >= symtab_hdr->sh_info)
	  {
	    h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	    if (h->plt.refcount > 0)
	      h->plt.refcount--;
	  }
	break;

      default:
	break;
      }

  return TRUE;
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
m32r_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  struct elf_m32r_link_hash_table *htab;
  bfd *dynobj;
  bfd_vma *local_got_offsets;
  asection *sgot, *srelgot, *sreloc;

  if (info->relocatable)
    return TRUE;

  sgot = srelgot = sreloc = NULL;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof (Elf32_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  htab = m32r_elf_hash_table (info);
  dynobj = htab->root.dynobj;
  local_got_offsets = elf_local_got_offsets (abfd);

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      int r_type;
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;

      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
        h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      /* Some relocs require a global offset table.  */
      if (htab->sgot == NULL)
        {
          switch (r_type)
            {
            case R_M32R_GOT16_HI_ULO:
            case R_M32R_GOT16_HI_SLO:
            case R_M32R_GOT16_LO:
            case R_M32R_GOTPC24:
            case R_M32R_GOTPC_HI_ULO:
            case R_M32R_GOTPC_HI_SLO:
            case R_M32R_GOTPC_LO:
            case R_M32R_GOT24:
              if (dynobj == NULL)
                htab->root.dynobj = dynobj = abfd;
              if (! create_got_section (dynobj, info))
                return FALSE;
              break;

            default:
              break;
          }
        }

      switch (r_type)
        {
	case R_M32R_GOT16_HI_ULO:
	case R_M32R_GOT16_HI_SLO:
	case R_M32R_GOT16_LO:
        case R_M32R_GOT24:

          if (h != NULL)
            h->got.refcount += 1;
          else
            {
              bfd_signed_vma *local_got_refcounts;

              /* This is a global offset table entry for a local
                 symbol.  */
              local_got_refcounts = elf_local_got_refcounts (abfd);
              if (local_got_refcounts == NULL)
                {
                  bfd_size_type size;

                  size = symtab_hdr->sh_info;
                  size *= sizeof (bfd_signed_vma);
                  local_got_refcounts = ((bfd_signed_vma *)
                                         bfd_zalloc (abfd, size));
                  if (local_got_refcounts == NULL)
                    return FALSE;
                  elf_local_got_refcounts (abfd) = local_got_refcounts;
                }
              local_got_refcounts[r_symndx] += 1;
            }
          break;

        case R_M32R_26_PLTREL:
          /* This symbol requires a procedure linkage table entry.  We
             actually build the entry in adjust_dynamic_symbol,
             because this might be a case of linking PIC code without
             linking in any dynamic objects, in which case we don't
             need to generate a procedure linkage table after all.  */

	  /* If this is a local symbol, we resolve it directly without
	     creating a procedure linkage table entry.  */
          if (h == NULL)
            continue;

          if (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL)
            break;

          h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	  h->plt.refcount += 1;
          break;

        case R_M32R_16_RELA:
        case R_M32R_24_RELA:
        case R_M32R_32_RELA:
        case R_M32R_HI16_ULO_RELA:
        case R_M32R_HI16_SLO_RELA:
        case R_M32R_LO16_RELA:
        case R_M32R_SDA16_RELA:
        case R_M32R_18_PCREL_RELA:
        case R_M32R_26_PCREL_RELA:

          if (h != NULL && !info->shared)
            {
              h->elf_link_hash_flags |= ELF_LINK_NON_GOT_REF;
              h->plt.refcount += 1;
            }

          /* If we are creating a shared library, and this is a reloc
             against a global symbol, or a non PC relative reloc
             against a local symbol, then we need to copy the reloc
             into the shared library.  However, if we are linking with
             -Bsymbolic, we do not need to copy a reloc against a
             global symbol which is defined in an object we are
             including in the link (i.e., DEF_REGULAR is set).  At
             this point we have not seen all the input files, so it is
             possible that DEF_REGULAR is not set now but will be set
             later (it is never cleared).  We account for that
             possibility below by storing information in the
             dyn_relocs field of the hash table entry. A similar
             situation occurs when creating shared libraries and symbol
             visibility changes render the symbol local.

             If on the other hand, we are creating an executable, we
             may need to keep relocations for symbols satisfied by a
             dynamic library if we manage to avoid copy relocs for the
             symbol.  */
          if ((info->shared
               && (sec->flags & SEC_ALLOC) != 0
	       && ((r_type != R_M32R_26_PCREL_RELA
                    && r_type != R_M32R_18_PCREL_RELA)
	           || (h != NULL
		       && (! info->symbolic
		           || h->root.type == bfd_link_hash_defweak
		           || (h->elf_link_hash_flags
		               & ELF_LINK_HASH_DEF_REGULAR) == 0))))
              || (!info->shared
                  && (sec->flags & SEC_ALLOC) != 0
                  && h != NULL
                  && (h->root.type == bfd_link_hash_defweak
                      || (h->elf_link_hash_flags
                          & ELF_LINK_HASH_DEF_REGULAR) == 0)))
            {
              struct elf_m32r_dyn_relocs *p;
              struct elf_m32r_dyn_relocs **head;

              if (dynobj == NULL)
                htab->root.dynobj = dynobj = abfd;

              /* When creating a shared object, we must copy these
                 relocs into the output file.  We create a reloc
                 section in dynobj and make room for the reloc.  */
              if (sreloc == NULL)
                {
                  const char *name;

                  name = (bfd_elf_string_from_elf_section
                          (abfd,
                           elf_elfheader (abfd)->e_shstrndx,
                           elf_section_data (sec)->rel_hdr.sh_name));
                  if (name == NULL)
                    return FALSE;

                  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
                              && strcmp (bfd_get_section_name (abfd, sec),
                                         name + 5) == 0);

                  sreloc = bfd_get_section_by_name (dynobj, name);
                  if (sreloc == NULL)
                    {
                      flagword flags;

                      sreloc = bfd_make_section (dynobj, name);
                      flags = (SEC_HAS_CONTENTS | SEC_READONLY
                               | SEC_IN_MEMORY | SEC_LINKER_CREATED);
                      if ((sec->flags & SEC_ALLOC) != 0)
                        flags |= SEC_ALLOC | SEC_LOAD;
                      if (sreloc == NULL
                          || ! bfd_set_section_flags (dynobj, sreloc, flags)
                          || ! bfd_set_section_alignment (dynobj, sreloc, 2))
                        return FALSE;
                    }
                  elf_section_data (sec)->sreloc = sreloc;
                }

              /* If this is a global symbol, we count the number of
                 relocations we need for this symbol.  */
              if (h != NULL)
                head = &((struct elf_m32r_link_hash_entry *) h)->dyn_relocs;
              else
                {
                  asection *s;

                  /* Track dynamic relocs needed for local syms too.  */
                  s = bfd_section_from_r_symndx (abfd, &htab->sym_sec,
                                                 sec, r_symndx);
                  if (s == NULL)
                    return FALSE;

                  head = ((struct elf_m32r_dyn_relocs **)
                          &elf_section_data (s)->local_dynrel);
                }

              p = *head;
              if (p == NULL || p->sec != sec)
                {
                  bfd_size_type amt = sizeof (*p);
                  p = ((struct elf_m32r_dyn_relocs *) bfd_alloc (dynobj, amt));
                  if (p == NULL)
                    return FALSE;
                  p->next = *head;
                  *head = p;
                  p->sec = sec;
                  p->count = 0;
                  p->pc_count = 0;
                }

              p->count += 1;
              if (ELF32_R_TYPE (rel->r_info) == R_M32R_26_PCREL_RELA
                  || ELF32_R_TYPE (rel->r_info) == R_M32R_18_PCREL_RELA)
                p->pc_count += 1;
            }
          break;

        /* This relocation describes the C++ object vtable hierarchy.
           Reconstruct it for later use during GC.  */
        case R_M32R_RELA_GNU_VTINHERIT:
        case R_M32R_GNU_VTINHERIT:
          if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

        /* This relocation describes which C++ vtable entries are actually
           used.  Record for later use during GC.  */
        case R_M32R_GNU_VTENTRY:
          if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;
        case R_M32R_RELA_GNU_VTENTRY:
          if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return FALSE;
          break;
        }
    }

  return TRUE;
}

static struct bfd_elf_special_section const m32r_elf_special_sections[]=
{
  { ".sdata",   6, -2, SHT_PROGBITS, SHF_ALLOC + SHF_WRITE },
  { ".sbss",    5, -2, SHT_NOBITS,   SHF_ALLOC + SHF_WRITE },
  { NULL,       0,  0, 0,            0 }
};

static bfd_boolean
m32r_elf_fake_sections (abfd, hdr, sec)
     bfd *abfd;
     Elf_Internal_Shdr *hdr ATTRIBUTE_UNUSED;
     asection *sec;
{
  register const char *name;

  name = bfd_get_section_name (abfd, sec);

  /* The generic elf_fake_sections will set up REL_HDR using the
     default kind of relocations.  But, we may actually need both
     kinds of relocations, so we set up the second header here.

     This is not necessary for the O32 ABI since that only uses Elf32_Rel
     relocations (cf. System V ABI, MIPS RISC Processor Supplement,
     3rd Edition, p. 4-17).  It breaks the IRIX 5/6 32-bit ld, since one
     of the resulting empty .rela.<section> sections starts with
     sh_offset == object size, and ld doesn't allow that.  While the check
     is arguably bogus for empty or SHT_NOBITS sections, it can easily be
     avoided by not emitting those useless sections in the first place.  */
  if ((sec->flags & SEC_RELOC) != 0)
    {
      struct bfd_elf_section_data *esd;
      bfd_size_type amt = sizeof (Elf_Internal_Shdr);

      esd = elf_section_data (sec);
      BFD_ASSERT (esd->rel_hdr2 == NULL);
      esd->rel_hdr2 = (Elf_Internal_Shdr *) bfd_zalloc (abfd, amt);
      if (!esd->rel_hdr2)
        return FALSE;
      _bfd_elf_init_reloc_shdr (abfd, esd->rel_hdr2, sec,
                                !sec->use_rela_p);
    }

  return TRUE;
}

static enum elf_reloc_type_class
m32r_elf_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  switch ((int) ELF32_R_TYPE (rela->r_info))
    {
    case R_M32R_RELATIVE:
      return reloc_class_relative;
    case R_M32R_JMP_SLOT:
      return reloc_class_plt;
    case R_M32R_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

#define ELF_ARCH		bfd_arch_m32r
#define ELF_MACHINE_CODE	EM_M32R
#define ELF_MACHINE_ALT1	EM_CYGNUS_M32R
#define ELF_MAXPAGESIZE		0x1 /* Explicitly requested by Mitsubishi.  */

#define TARGET_BIG_SYM          bfd_elf32_m32r_vec
#define TARGET_BIG_NAME		"elf32-m32r"
#define TARGET_LITTLE_SYM       bfd_elf32_m32rle_vec
#define TARGET_LITTLE_NAME      "elf32-m32rle"

#define elf_info_to_howto			m32r_info_to_howto
#define elf_info_to_howto_rel			m32r_info_to_howto_rel
#define elf_backend_section_from_bfd_section	_bfd_m32r_elf_section_from_bfd_section
#define elf_backend_symbol_processing		_bfd_m32r_elf_symbol_processing
#define elf_backend_add_symbol_hook		m32r_elf_add_symbol_hook
#define elf_backend_relocate_section		m32r_elf_relocate_section
#define elf_backend_gc_mark_hook                m32r_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook               m32r_elf_gc_sweep_hook
#define elf_backend_check_relocs                m32r_elf_check_relocs

#define elf_backend_create_dynamic_sections     m32r_elf_create_dynamic_sections
#define bfd_elf32_bfd_link_hash_table_create    m32r_elf_link_hash_table_create
#define elf_backend_size_dynamic_sections       m32r_elf_size_dynamic_sections
#define elf_backend_finish_dynamic_sections     m32r_elf_finish_dynamic_sections
#define elf_backend_adjust_dynamic_symbol       m32r_elf_adjust_dynamic_symbol
#define elf_backend_finish_dynamic_symbol       m32r_elf_finish_dynamic_symbol
#define elf_backend_reloc_type_class            m32r_elf_reloc_type_class
#define elf_backend_copy_indirect_symbol        m32r_elf_copy_indirect_symbol

#define elf_backend_can_gc_sections             1
/*#if !USE_REL
#define elf_backend_rela_normal			1
#endif*/
#define elf_backend_can_refcount 1
#define elf_backend_want_got_plt 1
#define elf_backend_plt_readonly 1
#define elf_backend_want_plt_sym 0
#define elf_backend_got_header_size 12

#define elf_backend_may_use_rel_p       1
#ifdef USE_M32R_OLD_RELOC
#define elf_backend_default_use_rela_p  0
#define elf_backend_may_use_rela_p      0
#else
#define elf_backend_default_use_rela_p  1
#define elf_backend_may_use_rela_p      1
#define elf_backend_fake_sections       m32r_elf_fake_sections
#endif

#if 0 /* not yet */
/* relax support */
#define bfd_elf32_bfd_relax_section		m32r_elf_relax_section
#define bfd_elf32_bfd_get_relocated_section_contents \
					m32r_elf_get_relocated_section_contents
#endif

#define elf_backend_object_p			m32r_elf_object_p
#define elf_backend_final_write_processing 	m32r_elf_final_write_processing
#define bfd_elf32_bfd_merge_private_bfd_data 	m32r_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags		m32r_elf_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data	m32r_elf_print_private_bfd_data
#define elf_backend_special_sections		m32r_elf_special_sections

#include "elf32-target.h"

#undef ELF_MAXPAGESIZE
#define ELF_MAXPAGESIZE         0x1000

#undef TARGET_BIG_SYM
#define TARGET_BIG_SYM          bfd_elf32_m32rlin_vec
#undef TARGET_BIG_NAME
#define TARGET_BIG_NAME         "elf32-m32r-linux"
#undef TARGET_LITTLE_SYM
#define TARGET_LITTLE_SYM       bfd_elf32_m32rlelin_vec
#undef TARGET_LITTLE_NAME
#define TARGET_LITTLE_NAME      "elf32-m32rle-linux"
#undef elf32_bed
#define elf32_bed               elf32_m32r_lin_bed

#include "elf32-target.h"

