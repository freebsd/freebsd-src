/* FRV-specific support for 32-bit ELF.
   Copyright 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "elf/frv.h"
#include "elf/dwarf2.h"
#include "hashtab.h"

/* Forward declarations.  */
static bfd_reloc_status_type elf32_frv_relocate_lo16
  PARAMS ((bfd *,  Elf_Internal_Rela *, bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_hi16
  PARAMS ((bfd *,  Elf_Internal_Rela *, bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_label24
  PARAMS ((bfd *, asection *, Elf_Internal_Rela *, bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_gprel12
  PARAMS ((struct bfd_link_info *, bfd *, asection *, Elf_Internal_Rela *,
	   bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_gprelu12
  PARAMS ((struct bfd_link_info *, bfd *, asection *, Elf_Internal_Rela *,
	   bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_gprello
  PARAMS ((struct bfd_link_info *, bfd *, asection *, Elf_Internal_Rela *,
	   bfd_byte *, bfd_vma));
static bfd_reloc_status_type elf32_frv_relocate_gprelhi
  PARAMS ((struct bfd_link_info *, bfd *, asection *, Elf_Internal_Rela *,
	   bfd_byte *, bfd_vma));
static reloc_howto_type *frv_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void frv_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static bfd_boolean elf32_frv_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static bfd_boolean elf32_frv_add_symbol_hook
  PARAMS (( bfd *, struct bfd_link_info *, Elf_Internal_Sym *,
	    const char **, flagword *, asection **, bfd_vma *));
static bfd_reloc_status_type frv_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, bfd_vma));
static bfd_boolean elf32_frv_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *, const
	   Elf_Internal_Rela *));
static asection * elf32_frv_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static bfd_boolean elf32_frv_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static int elf32_frv_machine
  PARAMS ((bfd *));
static bfd_boolean elf32_frv_object_p
  PARAMS ((bfd *));
static bfd_boolean frv_elf_set_private_flags
  PARAMS ((bfd *, flagword));
static bfd_boolean frv_elf_copy_private_bfd_data
  PARAMS ((bfd *, bfd *));
static bfd_boolean frv_elf_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static bfd_boolean frv_elf_print_private_bfd_data
  PARAMS ((bfd *, PTR));

static reloc_howto_type elf32_frv_howto_table [] =
{
  /* This reloc does nothing.  */
  HOWTO (R_FRV_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_FRV_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit pc-relative relocation.  */
  HOWTO (R_FRV_LABEL16,	        /* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_LABEL16",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  /* A 24-bit pc-relative relocation.  */
  HOWTO (R_FRV_LABEL24,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_LABEL24",	/* name */
	 FALSE,			/* partial_inplace */
	 0x7e03ffff,		/* src_mask */
	 0x7e03ffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_FRV_LO16,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_LO16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_HI16,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_HI16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_GPREL12,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GPREL12",       /* name */
	 FALSE,			/* partial_inplace */
	 0xfff,		        /* src_mask */
	 0xfff,		        /* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_GPRELU12,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GPRELU12",      /* name */
	 FALSE,			/* partial_inplace */
	 0xfff,		        /* src_mask */
	 0x3f03f,	        /* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_GPREL32,         /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GPREL32",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_GPRELHI,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GPRELHI",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		        /* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_FRV_GPRELLO,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GPRELLO",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		        /* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* A 12-bit signed operand with the GOT offset for the address of
     the symbol.  */
  HOWTO (R_FRV_GOT12,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GOT12",		/* name */
	 FALSE,			/* partial_inplace */
	 0xfff,		        /* src_mask */
	 0xfff,		        /* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* The upper 16 bits of the GOT offset for the address of the
     symbol.  */
  HOWTO (R_FRV_GOTHI,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GOTHI",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		        /* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* The lower 16 bits of the GOT offset for the address of the
     symbol.  */
  HOWTO (R_FRV_GOTLO,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GOTLO",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* The 32-bit address of the canonical descriptor of a function.  */
  HOWTO (R_FRV_FUNCDESC,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_FUNCDESC",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 12-bit signed operand with the GOT offset for the address of
     canonical descriptor of a function.  */
  HOWTO (R_FRV_FUNCDESC_GOT12,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_FUNCDESC_GOT12", /* name */
	 FALSE,			/* partial_inplace */
	 0xfff,		        /* src_mask */
	 0xfff,		        /* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* The upper 16 bits of the GOT offset for the address of the
     canonical descriptor of a function.  */
  HOWTO (R_FRV_FUNCDESC_GOTHI,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_FUNCDESC_GOTHI", /* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* The lower 16 bits of the GOT offset for the address of the
     canonical descriptor of a function.  */
  HOWTO (R_FRV_FUNCDESC_GOTLO,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_FUNCDESC_GOTLO", /* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* The 32-bit address of the canonical descriptor of a function.  */
  HOWTO (R_FRV_FUNCDESC_VALUE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_FUNCDESC_VALUE", /* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 12-bit signed operand with the GOT offset for the address of
     canonical descriptor of a function.  */
  HOWTO (R_FRV_FUNCDESC_GOTOFF12, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_FUNCDESC_GOTOFF12", /* name */
	 FALSE,			/* partial_inplace */
	 0xfff,		        /* src_mask */
	 0xfff,		        /* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* The upper 16 bits of the GOT offset for the address of the
     canonical descriptor of a function.  */
  HOWTO (R_FRV_FUNCDESC_GOTOFFHI, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_FUNCDESC_GOTOFFHI", /* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* The lower 16 bits of the GOT offset for the address of the
     canonical descriptor of a function.  */
  HOWTO (R_FRV_FUNCDESC_GOTOFFLO, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_FUNCDESC_GOTOFFLO", /* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* A 12-bit signed operand with the GOT offset for the address of
     the symbol.  */
  HOWTO (R_FRV_GOTOFF12,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GOTOFF12",	/* name */
	 FALSE,			/* partial_inplace */
	 0xfff,		        /* src_mask */
	 0xfff,		        /* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* The upper 16 bits of the GOT offset for the address of the
     symbol.  */
  HOWTO (R_FRV_GOTOFFHI,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GOTOFFHI",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  /* The lower 16 bits of the GOT offset for the address of the
     symbol.  */
  HOWTO (R_FRV_GOTOFFLO,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_GOTOFFLO",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

};

/* GNU extension to record C++ vtable hierarchy.  */
static reloc_howto_type elf32_frv_vtinherit_howto =
  HOWTO (R_FRV_GNU_VTINHERIT,   /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_FRV_GNU_VTINHERIT", /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE);                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage.  */
static reloc_howto_type elf32_frv_vtentry_howto =
  HOWTO (R_FRV_GNU_VTENTRY,     /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_FRV_GNU_VTENTRY",   /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE);                /* pcrel_offset */

/* The following 3 relocations are REL.  The only difference to the
   entries in the table above are that partial_inplace is TRUE.  */
static reloc_howto_type elf32_frv_rel_32_howto =
  HOWTO (R_FRV_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE);		/* pcrel_offset */

static reloc_howto_type elf32_frv_rel_funcdesc_howto =
  HOWTO (R_FRV_FUNCDESC,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_FUNCDESC",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE);		/* pcrel_offset */

static reloc_howto_type elf32_frv_rel_funcdesc_value_howto =
  HOWTO (R_FRV_FUNCDESC_VALUE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_FRV_FUNCDESC_VALUE", /* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE);		/* pcrel_offset */


/* Map BFD reloc types to FRV ELF reloc types.  */
#if 0
struct frv_reloc_map
{
  unsigned int bfd_reloc_val;
  unsigned int frv_reloc_val;
};

static const struct frv_reloc_map frv_reloc_map [] =
{
  { BFD_RELOC_NONE,           R_FRV_NONE },
  { BFD_RELOC_32,             R_FRV_32 },
  { BFD_RELOC_FRV_LABEL16,    R_FRV_LABEL16 },
  { BFD_RELOC_FRV_LABEL24,    R_FRV_LABEL24 },
  { BFD_RELOC_FRV_LO16,       R_FRV_LO16 },
  { BFD_RELOC_FRV_HI16,       R_FRV_HI16 },
  { BFD_RELOC_FRV_GPREL12,    R_FRV_GPREL12 },
  { BFD_RELOC_FRV_GPRELU12,   R_FRV_GPRELU12 },
  { BFD_RELOC_FRV_GPREL32,    R_FRV_GPREL32 },
  { BFD_RELOC_FRV_GPRELHI,    R_FRV_GPRELHI },
  { BFD_RELOC_FRV_GPRELLO,    R_FRV_GPRELLO },
  { BFD_RELOC_FRV_GOT12,      R_FRV_GOT12 },
  { BFD_RELOC_FRV_GOTHI,      R_FRV_GOTHI },
  { BFD_RELOC_FRV_GOTLO,      R_FRV_GOTLO },
  { BFD_RELOC_FRV_FUNCDESC,   R_FRV_FUNCDESC },
  { BFD_RELOC_FRV_FUNCDESC_GOT12, R_FRV_FUNCDESC_GOT12 },
  { BFD_RELOC_FRV_FUNCDESC_GOTHI, R_FRV_FUNCDESC_GOTHI },
  { BFD_RELOC_FRV_FUNCDESC_GOTLO, R_FRV_FUNCDESC_GOTLO },
  { BFD_RELOC_FRV_FUNCDESC_VALUE, R_FRV_FUNCDESC_VALUE },
  { BFD_RELOC_FRV_FUNCDESC_GOTOFF12, R_FRV_FUNCDESC_GOTOFF12 },
  { BFD_RELOC_FRV_FUNCDESC_GOTOFFHI, R_FRV_FUNCDESC_GOTOFFHI },
  { BFD_RELOC_FRV_FUNCDESC_GOTOFFLO, R_FRV_FUNCDESC_GOTOFFLO },
  { BFD_RELOC_FRV_GOTOFF12,   R_FRV_GOTOFF12 },
  { BFD_RELOC_FRV_GOTOFFHI,   R_FRV_GOTOFFHI },
  { BFD_RELOC_FRV_GOTOFFLO,   R_FRV_GOTOFFLO },
  { BFD_RELOC_VTABLE_INHERIT, R_FRV_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY,   R_FRV_GNU_VTENTRY },
};
#endif

extern const bfd_target bfd_elf32_frvfdpic_vec;
#define IS_FDPIC(bfd) ((bfd)->xvec == &bfd_elf32_frvfdpic_vec)

/* An extension of the elf hash table data structure, containing some
   additional FRV-specific data.  */
struct frvfdpic_elf_link_hash_table
{
  struct elf_link_hash_table elf;

  /* A pointer to the .got section.  */
  asection *sgot;
  /* A pointer to the .rel.got section.  */
  asection *sgotrel;
  /* A pointer to the .rofixup section.  */
  asection *sgotfixup;
  /* A pointer to the .plt section.  */
  asection *splt;
  /* A pointer to the .rel.plt section.  */
  asection *spltrel;
  /* GOT base offset.  */
  bfd_vma got0;
  /* Location of the first non-lazy PLT entry, i.e., the number of
     bytes taken by lazy PLT entries.  */
  bfd_vma plt0;
  /* A hash table holding information about which symbols were
     referenced with which PIC-related relocations.  */
  struct htab *relocs_info;
};

/* Get the FRV ELF linker hash table from a link_info structure.  */

#define frvfdpic_hash_table(info) \
  ((struct frvfdpic_elf_link_hash_table *) ((info)->hash))

#define frvfdpic_got_section(info) \
  (frvfdpic_hash_table (info)->sgot)
#define frvfdpic_gotrel_section(info) \
  (frvfdpic_hash_table (info)->sgotrel)
#define frvfdpic_gotfixup_section(info) \
  (frvfdpic_hash_table (info)->sgotfixup)
#define frvfdpic_plt_section(info) \
  (frvfdpic_hash_table (info)->splt)
#define frvfdpic_pltrel_section(info) \
  (frvfdpic_hash_table (info)->spltrel)
#define frvfdpic_relocs_info(info) \
  (frvfdpic_hash_table (info)->relocs_info)
#define frvfdpic_got_initial_offset(info) \
  (frvfdpic_hash_table (info)->got0)
#define frvfdpic_plt_initial_offset(info) \
  (frvfdpic_hash_table (info)->plt0)

/* Create an FRV ELF linker hash table.  */

static struct bfd_link_hash_table *
frvfdpic_elf_link_hash_table_create (bfd *abfd)
{
  struct frvfdpic_elf_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct frvfdpic_elf_link_hash_table);

  ret = bfd_zalloc (abfd, amt);
  if (ret == NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->elf, abfd,
				       _bfd_elf_link_hash_newfunc))
    {
      free (ret);
      return NULL;
    }

  return &ret->elf.root;
}

/* Decide whether a reference to a symbol can be resolved locally or
   not.  If the symbol is protected, we want the local address, but
   its function descriptor must be assigned by the dynamic linker.  */
#define FRVFDPIC_SYM_LOCAL(INFO, H) \
  (_bfd_elf_symbol_refs_local_p ((H), (INFO), 1) \
   || ! elf_hash_table (INFO)->dynamic_sections_created \
   || (/* The condition below is an ugly hack to get .scommon data to
	  be regarded as local.  For some reason the
	  ELF_LINK_HASH_DEF_REGULAR bit is not set on such common
	  symbols, and the SEC_IS_COMMON bit is not set any longer
	  when we need to perform this test.  Hopefully this
	  approximation is good enough.  */ \
       ((H)->root.type == bfd_link_hash_defined \
	|| (H)->root.type == bfd_link_hash_defweak) \
       && (H)->root.u.def.section->output_section \
       && ((H)->root.u.def.section->flags & SEC_LINKER_CREATED)))
#define FRVFDPIC_FUNCDESC_LOCAL(INFO, H) \
  ((H)->dynindx == -1 || ! elf_hash_table (INFO)->dynamic_sections_created)

/* This structure collects information on what kind of GOT, PLT or
   function descriptors are required by relocations that reference a
   certain symbol.  */
struct frvfdpic_relocs_info
{
  /* The index of the symbol, as stored in the relocation r_info, if
     we have a local symbol; -1 otherwise.  */
  long symndx;
  union
  {
    /* The input bfd in which the symbol is defined, if it's a local
       symbol.  */
    bfd *abfd;
    /* If symndx == -1, the hash table entry corresponding to a global
       symbol (even if it turns out to bind locally, in which case it
       should ideally be replaced with section's symndx + addend).  */
    struct elf_link_hash_entry *h;
  } d;
  /* The addend of the relocation that references the symbol.  */
  bfd_vma addend;

  /* The fields above are used to identify an entry.  The fields below
     contain information on how an entry is used and, later on, which
     locations it was assigned.  */
  /* The following 3 fields record whether the symbol+addend above was
     ever referenced with a GOT relocation.  The 12 suffix indicates a
     GOT12 relocation; los is used for GOTLO relocations that are not
     matched by a GOTHI relocation; hilo is used for GOTLO/GOTHI
     pairs.  */
  unsigned got12:1;
  unsigned gotlos:1;
  unsigned gothilo:1;
  /* Whether a FUNCDESC relocation references symbol+addend.  */
  unsigned fd:1;
  /* Whether a FUNCDESC_GOT relocation references symbol+addend.  */
  unsigned fdgot12:1;
  unsigned fdgotlos:1;
  unsigned fdgothilo:1;
  /* Whether a FUNCDESC_GOTOFF relocation references symbol+addend.  */
  unsigned fdgoff12:1;
  unsigned fdgofflos:1;
  unsigned fdgoffhilo:1;
  /* Whether symbol+addend is referenced with GOTOFF12, GOTOFFLO or
     GOTOFFHI relocations.  The addend doesn't really matter, since we
     envision that this will only be used to check whether the symbol
     is mapped to the same segment as the got.  */
  unsigned gotoff:1;
  /* Whether symbol+addend is referenced by a LABEL24 relocation.  */
  unsigned call:1;
  /* Whether symbol+addend is referenced by a 32 or FUNCDESC_VALUE
     relocation.  */
  unsigned sym:1;
  /* Whether we need a PLT entry for a symbol.  Should be implied by
     something like:
     (call && symndx == -1 && ! FRVFDPIC_SYM_LOCAL (info, d.h))  */
  unsigned plt:1;
  /* Whether a function descriptor should be created in this link unit
     for symbol+addend.  Should be implied by something like:
     (plt || fdgotoff12 || fdgotofflos || fdgotofflohi
      || ((fd || fdgot12 || fdgotlos || fdgothilo)
          && (symndx != -1 || FRVFDPIC_FUNCDESC_LOCAL (info, d.h))))  */
  unsigned privfd:1;
  /* Whether a lazy PLT entry is needed for this symbol+addend.
     Should be implied by something like:
     (privfd && symndx == -1 && ! FRVFDPIC_SYM_LOCAL (info, d.h)
      && ! (info->flags & DF_BIND_NOW))  */
  unsigned lazyplt:1;
  /* Whether we've already emitted GOT relocations and PLT entries as
     needed for this symbol.  */
  unsigned done:1;

  /* The number of R_FRV_32, R_FRV_FUNCDESC and R_FRV_FUNCDESC_VALUE
     relocations referencing the symbol.  */
  unsigned relocs32, relocsfd, relocsfdv;

  /* The number of .rofixups entries and dynamic relocations allocated
     for this symbol, minus any that might have already been used.  */
  unsigned fixups, dynrelocs;

  /* The offsets of the GOT entries assigned to symbol+addend, to the
     function descriptor's address, and to a function descriptor,
     respectively.  Should be zero if unassigned.  The offsets are
     counted from the value that will be assigned to the PIC register,
     not from the beginning of the .got section.  */
  bfd_signed_vma got_entry, fdgot_entry, fd_entry;
  /* The offsets of the PLT entries assigned to symbol+addend,
     non-lazy and lazy, respectively.  If unassigned, should be
     (bfd_vma)-1.  */
  bfd_vma plt_entry, lzplt_entry;
};

/* Compute a hash with the key fields of an frvfdpic_relocs_info entry.  */
static hashval_t
frvfdpic_relocs_info_hash (const void *entry_)
{
  const struct frvfdpic_relocs_info *entry = entry_;

  return (entry->symndx == -1
	  ? entry->d.h->root.root.hash
	  : entry->symndx + entry->d.abfd->id * 257) + entry->addend;
}

/* Test whether the key fields of two frvfdpic_relocs_info entries are
   identical.  */
static int
frvfdpic_relocs_info_eq (const void *entry1, const void *entry2)
{
  const struct frvfdpic_relocs_info *e1 = entry1;
  const struct frvfdpic_relocs_info *e2 = entry2;

  return e1->symndx == e2->symndx && e1->addend == e2->addend
    && (e1->symndx == -1 ? e1->d.h == e2->d.h : e1->d.abfd == e2->d.abfd);
}

/* Find or create an entry in a hash table HT that matches the key
   fields of the given ENTRY.  If it's not found, memory for a new
   entry is allocated in ABFD's obstack.  */
static struct frvfdpic_relocs_info *
frvfdpic_relocs_info_find (struct htab *ht,
			   bfd *abfd,
			   const struct frvfdpic_relocs_info *entry,
			   enum insert_option insert)
{
  struct frvfdpic_relocs_info **loc =
    (struct frvfdpic_relocs_info **) htab_find_slot (ht, entry, insert);

  if (! loc)
    return NULL;

  if (*loc)
    return *loc;

  *loc = bfd_zalloc (abfd, sizeof (**loc));

  if (! *loc)
    return *loc;

  (*loc)->symndx = entry->symndx;
  (*loc)->d = entry->d;
  (*loc)->addend = entry->addend;
  (*loc)->plt_entry = (bfd_vma)-1;
  (*loc)->lzplt_entry = (bfd_vma)-1;

  return *loc;
}

/* Obtain the address of the entry in HT associated with H's symbol +
   addend, creating a new entry if none existed.  ABFD is only used
   for memory allocation purposes.  */
inline static struct frvfdpic_relocs_info *
frvfdpic_relocs_info_for_global (struct htab *ht,
				 bfd *abfd,
				 struct elf_link_hash_entry *h,
				 bfd_vma addend,
				 enum insert_option insert)
{
  struct frvfdpic_relocs_info entry;

  entry.symndx = -1;
  entry.d.h = h;
  entry.addend = addend;

  return frvfdpic_relocs_info_find (ht, abfd, &entry, insert);
}

/* Obtain the address of the entry in HT associated with the SYMNDXth
   local symbol of the input bfd ABFD, plus the addend, creating a new
   entry if none existed.  */  
inline static struct frvfdpic_relocs_info *
frvfdpic_relocs_info_for_local (struct htab *ht,
				bfd *abfd,
				long symndx,
				bfd_vma addend,
				enum insert_option insert)
{
  struct frvfdpic_relocs_info entry;

  entry.symndx = symndx;
  entry.d.abfd = abfd;
  entry.addend = addend;

  return frvfdpic_relocs_info_find (ht, abfd, &entry, insert);
}

/* Merge fields set by check_relocs() of two entries that end up being
   mapped to the same (presumably global) symbol.  */

inline static void
frvfdpic_pic_merge_early_relocs_info (struct frvfdpic_relocs_info *e2,
				      struct frvfdpic_relocs_info const *e1)
{
  e2->got12 |= e1->got12;
  e2->gotlos |= e1->gotlos;
  e2->gothilo |= e1->gothilo;
  e2->fd |= e1->fd;
  e2->fdgot12 |= e1->fdgot12;
  e2->fdgotlos |= e1->fdgotlos;
  e2->fdgothilo |= e1->fdgothilo;
  e2->fdgoff12 |= e1->fdgoff12;
  e2->fdgofflos |= e1->fdgofflos;
  e2->fdgoffhilo |= e1->fdgoffhilo;
  e2->gotoff |= e1->gotoff;
  e2->call |= e1->call;
  e2->sym |= e1->sym;

#if 0
  /* These are set in _frvfdpic_count_got_plt_entries() or later, and this
     function is only called in _frvfdpic_resolve_final_relocs_info(), that
     runs just before it, so we don't have to worry about the fields
     below.  */

  e2->plt |= e1->plt;
  e2->privfd |= e1->privfd;
  e2->lazyplt |= e1->lazyplt;
  e2->done |= e1->done;

  e2->relocs32 += e1->relocs32;
  e2->relocsfd += e1->relocsfd;
  e2->relocsfdv += e1->relocsfdv;
  e2->fixups += e1->fixups;
  e2->dynrelocs += e1->dynrelocs;

  if (abs (e1->got_entry) < abs (e2->got_entry))
    e2->got_entry = e1->got_entry;
  if (abs (e1->fdgot_entry) < abs (e2->fdgot_entry))
    e2->fdgot_entry = e1->fdgot_entry;
  if (abs (e1->fd_entry) < abs (e2->fd_entry))
    e2->fd_entry = e1->fd_entry;

  if (e1->plt_entry < e2->plt_entry)
    e2->plt_entry = e1->plt_entry;
  if (e1->lzplt_entry < e2->lzplt_entry)
    e2->lzplt_entry = e1->lzplt_entry;
#endif
}

/* Every block of 65535 lazy PLT entries shares a single call to the
   resolver, inserted in the 32768th lazy PLT entry (i.e., entry #
   32767, counting from 0).  All other lazy PLT entries branch to it
   in a single instruction.  */

#define FRVFDPIC_LZPLT_BLOCK_SIZE ((bfd_vma) 8 * 65535 + 4)
#define FRVFDPIC_LZPLT_RESOLV_LOC (8 * 32767)

/* Add a dynamic relocation to the SRELOC section.  */

inline static bfd_vma
_frvfdpic_add_dyn_reloc (bfd *output_bfd, asection *sreloc, bfd_vma offset,
			 int reloc_type, long dynindx, bfd_vma addend,
			 struct frvfdpic_relocs_info *entry)
{
  Elf_Internal_Rela outrel;
  bfd_vma reloc_offset;

  outrel.r_offset = offset;
  outrel.r_info = ELF32_R_INFO (dynindx, reloc_type);
  outrel.r_addend = addend;

  reloc_offset = sreloc->reloc_count * sizeof (Elf32_External_Rel);
  BFD_ASSERT (reloc_offset < sreloc->_raw_size);
  bfd_elf32_swap_reloc_out (output_bfd, &outrel,
			    sreloc->contents + reloc_offset);
  sreloc->reloc_count++;

  BFD_ASSERT (entry->dynrelocs > 0);
  entry->dynrelocs--;

  return reloc_offset;
}

/* Add a fixup to the ROFIXUP section.  */

static bfd_vma
_frvfdpic_add_rofixup (bfd *output_bfd, asection *rofixup, bfd_vma offset,
		       struct frvfdpic_relocs_info *entry)
{
  bfd_vma fixup_offset;

  if (rofixup->flags & SEC_EXCLUDE)
    return -1;

  fixup_offset = rofixup->reloc_count * 4;
  if (rofixup->contents)
    {
      BFD_ASSERT (fixup_offset < rofixup->_raw_size);
      bfd_put_32 (output_bfd, offset, rofixup->contents + fixup_offset);
    }
  rofixup->reloc_count++;

  if (entry)
    {
      BFD_ASSERT (entry->fixups > 0);
      entry->fixups--;
    }

  return fixup_offset;
}

/* Find the segment number in which OSEC, and output section, is
   located.  */

static unsigned
_frvfdpic_osec_to_segment (bfd *output_bfd, asection *osec)
{
  struct elf_segment_map *m;
  Elf_Internal_Phdr *p;

  /* Find the segment that contains the output_section.  */
  for (m = elf_tdata (output_bfd)->segment_map,
	 p = elf_tdata (output_bfd)->phdr;
       m != NULL;
       m = m->next, p++)
    {
      int i;

      for (i = m->count - 1; i >= 0; i--)
	if (m->sections[i] == osec)
	  break;

      if (i >= 0)
	break;
    }

  return p - elf_tdata (output_bfd)->phdr;
}

inline static bfd_boolean
_frvfdpic_osec_readonly_p (bfd *output_bfd, asection *osec)
{
  unsigned seg = _frvfdpic_osec_to_segment (output_bfd, osec);

  return ! (elf_tdata (output_bfd)->phdr[seg].p_flags & PF_W);
}

/* Generate relocations for GOT entries, function descriptors, and
   code for PLT and lazy PLT entries.  */

inline static bfd_boolean
_frvfdpic_emit_got_relocs_plt_entries (struct frvfdpic_relocs_info *entry,
				       bfd *output_bfd,
				       struct bfd_link_info *info,
				       asection *sec,
				       Elf_Internal_Sym *sym,
				       bfd_vma addend)
				  
{
  bfd_vma fd_lazy_rel_offset = (bfd_vma)-1;
  int dynindx = -1;

  if (entry->done)
    return TRUE;
  entry->done = 1;

  if (entry->got_entry || entry->fdgot_entry || entry->fd_entry)
    {
      /* If the symbol is dynamic, consider it for dynamic
	 relocations, otherwise decay to section + offset.  */
      if (entry->symndx == -1 && entry->d.h->dynindx != -1)
	dynindx = entry->d.h->dynindx;
      else
	{
	  if (sec->output_section
	      && ! bfd_is_abs_section (sec->output_section)
	      && ! bfd_is_und_section (sec->output_section))
	    dynindx = elf_section_data (sec->output_section)->dynindx;
	  else
	    dynindx = 0;
	}
    }

  /* Generate relocation for GOT entry pointing to the symbol.  */
  if (entry->got_entry)
    {
      int idx = dynindx;
      bfd_vma ad = addend;

      /* If the symbol is dynamic but binds locally, use
	 section+offset.  */
      if (sec && (entry->symndx != -1
		  || FRVFDPIC_SYM_LOCAL (info, entry->d.h)))
	{
	  if (entry->symndx == -1)
	    ad += entry->d.h->root.u.def.value;
	  else
	    ad += sym->st_value;
	  ad += sec->output_offset;
	  if (sec->output_section && elf_section_data (sec->output_section))
	    idx = elf_section_data (sec->output_section)->dynindx;
	  else
	    idx = 0;
	}

      /* If we're linking an executable at a fixed address, we can
	 omit the dynamic relocation as long as the symbol is local to
	 this module.  */
      if (info->executable && !info->pie
	  && (entry->symndx != -1
	      || FRVFDPIC_SYM_LOCAL (info, entry->d.h)))
	{
	  if (sec)
	    ad += sec->output_section->vma;
	  if (entry->symndx != -1
	      || entry->d.h->root.type != bfd_link_hash_undefweak)
	    _frvfdpic_add_rofixup (output_bfd,
				   frvfdpic_gotfixup_section (info),
				   frvfdpic_got_section (info)->output_section
				   ->vma
				   + frvfdpic_got_section (info)->output_offset
				   + frvfdpic_got_initial_offset (info)
				   + entry->got_entry, entry);
	}
      else
	_frvfdpic_add_dyn_reloc (output_bfd, frvfdpic_gotrel_section (info),
				 _bfd_elf_section_offset
				 (output_bfd, info,
				  frvfdpic_got_section (info),
				  frvfdpic_got_initial_offset (info)
				  + entry->got_entry)
				 + frvfdpic_got_section (info)
				 ->output_section->vma
				 + frvfdpic_got_section (info)->output_offset,
				 R_FRV_32, idx, ad, entry);
	
      bfd_put_32 (output_bfd, ad,
		  frvfdpic_got_section (info)->contents
		  + frvfdpic_got_initial_offset (info)
		  + entry->got_entry);
    }

  /* Generate relocation for GOT entry pointing to a canonical
     function descriptor.  */
  if (entry->fdgot_entry)
    {
      int reloc, idx;
      bfd_vma ad = 0;
      
      if (! (entry->symndx == -1
	     && entry->d.h->root.type == bfd_link_hash_undefweak
	     && FRVFDPIC_SYM_LOCAL (info, entry->d.h)))
	{
	  /* If the symbol is dynamic and there may be dynamic symbol
	     resolution because we are, or are linked with, a shared
	     library, emit a FUNCDESC relocation such that the dynamic
	     linker will allocate the function descriptor.  If the
	     symbol needs a non-local function descriptor but binds
	     locally (e.g., its visibility is protected, emit a
	     dynamic relocation decayed to section+offset.  */
	  if (entry->symndx == -1
	      && ! FRVFDPIC_FUNCDESC_LOCAL (info, entry->d.h)
	      && FRVFDPIC_SYM_LOCAL (info, entry->d.h)
	      && !(info->executable && !info->pie))
	    {
	      reloc = R_FRV_FUNCDESC;
	      idx = elf_section_data (entry->d.h->root.u.def.section
				      ->output_section)->dynindx;
	      ad = entry->d.h->root.u.def.section->output_offset
		+ entry->d.h->root.u.def.value;
	    }
	  else if (entry->symndx == -1
		   && ! FRVFDPIC_FUNCDESC_LOCAL (info, entry->d.h))
	    {
	      reloc = R_FRV_FUNCDESC;
	      idx = dynindx;
	      ad = addend;
	      if (ad)
		return FALSE;
	    }
	  else
	    {
	      /* Otherwise, we know we have a private function descriptor,
		 so reference it directly.  */
	      if (elf_hash_table (info)->dynamic_sections_created)
		BFD_ASSERT (entry->privfd);
	      reloc = R_FRV_32;
	      idx = elf_section_data (frvfdpic_got_section (info)
				      ->output_section)->dynindx;
	      ad = frvfdpic_got_section (info)->output_offset
		+ frvfdpic_got_initial_offset (info) + entry->fd_entry;
	    }

	  /* If there is room for dynamic symbol resolution, emit the
	     dynamic relocation.  However, if we're linking an
	     executable at a fixed location, we won't have emitted a
	     dynamic symbol entry for the got section, so idx will be
	     zero, which means we can and should compute the address
	     of the private descriptor ourselves.  */
	  if (info->executable && !info->pie
	      && (entry->symndx != -1
		  || FRVFDPIC_FUNCDESC_LOCAL (info, entry->d.h)))
	    {
	      ad += frvfdpic_got_section (info)->output_section->vma;
	      _frvfdpic_add_rofixup (output_bfd,
				     frvfdpic_gotfixup_section (info),
				     frvfdpic_got_section (info)
				     ->output_section->vma
				     + frvfdpic_got_section (info)
				     ->output_offset
				     + frvfdpic_got_initial_offset (info)
				     + entry->fdgot_entry, entry);
	    }
	  else
	    _frvfdpic_add_dyn_reloc (output_bfd,
				     frvfdpic_gotrel_section (info),
				     _bfd_elf_section_offset
				     (output_bfd, info,
				      frvfdpic_got_section (info),
				      frvfdpic_got_initial_offset (info)
				      + entry->fdgot_entry)
				     + frvfdpic_got_section (info)
				     ->output_section->vma
				     + frvfdpic_got_section (info)
				     ->output_offset,
				     reloc, idx, ad, entry);
	}

      bfd_put_32 (output_bfd, ad,
		  frvfdpic_got_section (info)->contents
		  + frvfdpic_got_initial_offset (info)
		  + entry->fdgot_entry);
    }

  /* Generate relocation to fill in a private function descriptor in
     the GOT.  */
  if (entry->fd_entry)
    {
      int idx = dynindx;
      bfd_vma ad = addend;
      bfd_vma ofst;
      long lowword, highword;

      /* If the symbol is dynamic but binds locally, use
	 section+offset.  */
      if (sec && (entry->symndx != -1
		  || FRVFDPIC_SYM_LOCAL (info, entry->d.h)))
	{
	  if (entry->symndx == -1)
	    ad += entry->d.h->root.u.def.value;
	  else
	    ad += sym->st_value;
	  ad += sec->output_offset;
	  if (sec->output_section && elf_section_data (sec->output_section))
	    idx = elf_section_data (sec->output_section)->dynindx;
	  else
	    idx = 0;
	}

      /* If we're linking an executable at a fixed address, we can
	 omit the dynamic relocation as long as the symbol is local to
	 this module.  */
      if (info->executable && !info->pie
	  && (entry->symndx != -1 || FRVFDPIC_SYM_LOCAL (info, entry->d.h)))
	{
	  if (sec)
	    ad += sec->output_section->vma;
	  ofst = 0;
	  if (entry->symndx != -1
	      || entry->d.h->root.type != bfd_link_hash_undefweak)
	    {
	      _frvfdpic_add_rofixup (output_bfd,
				     frvfdpic_gotfixup_section (info),
				     frvfdpic_got_section (info)
				     ->output_section->vma
				     + frvfdpic_got_section (info)
				     ->output_offset
				     + frvfdpic_got_initial_offset (info)
				     + entry->fd_entry, entry);
	      _frvfdpic_add_rofixup (output_bfd,
				     frvfdpic_gotfixup_section (info),
				     frvfdpic_got_section (info)
				     ->output_section->vma
				     + frvfdpic_got_section (info)
				     ->output_offset
				     + frvfdpic_got_initial_offset (info)
				     + entry->fd_entry + 4, entry);
	    }
	}
      else
	{
	  ofst =
	    _frvfdpic_add_dyn_reloc (output_bfd,
				     entry->lazyplt
				     ? frvfdpic_pltrel_section (info)
				     : frvfdpic_gotrel_section (info),
				     _bfd_elf_section_offset
				     (output_bfd, info,
				      frvfdpic_got_section (info),
				      frvfdpic_got_initial_offset (info)
				      + entry->fd_entry)
				     + frvfdpic_got_section (info)
				     ->output_section->vma
				     + frvfdpic_got_section (info)
				     ->output_offset,
				     R_FRV_FUNCDESC_VALUE, idx, ad, entry);
	}

      /* If we've omitted the dynamic relocation, just emit the fixed
	 addresses of the symbol and of the local GOT base offset.  */
      if (info->executable && !info->pie && sec && sec->output_section)
	{
	  lowword = ad;
	  highword = frvfdpic_got_section (info)->output_section->vma
	    + frvfdpic_got_section (info)->output_offset
	    + frvfdpic_got_initial_offset (info);
	}
      else if (entry->lazyplt)
	{
	  if (ad)
	    return FALSE;
	  
	  fd_lazy_rel_offset = ofst;

	  /* A function descriptor used for lazy or local resolving is
	     initialized such that its high word contains the output
	     section index in which the PLT entries are located, and
	     the low word contains the address of the lazy PLT entry
	     entry point, that must be within the memory region
	     assigned to that section.  */
	  lowword = entry->lzplt_entry + 4
	    + frvfdpic_plt_section (info)->output_offset
	    + frvfdpic_plt_section (info)->output_section->vma;
	  highword = _frvfdpic_osec_to_segment 
	    (output_bfd, frvfdpic_plt_section (info)->output_section);
	}
      else
	{
	  /* A function descriptor for a local function gets the index
	     of the section.  For a non-local function, it's
	     disregarded.  */
	  lowword = ad;
	  if (entry->symndx == -1 && entry->d.h->dynindx != -1
	      && entry->d.h->dynindx == idx)
	    highword = 0;
	  else
	    highword = _frvfdpic_osec_to_segment
	      (output_bfd, sec->output_section);
	}

      bfd_put_32 (output_bfd, lowword,
		  frvfdpic_got_section (info)->contents
		  + frvfdpic_got_initial_offset (info)
		  + entry->fd_entry);
      bfd_put_32 (output_bfd, highword,
		  frvfdpic_got_section (info)->contents
		  + frvfdpic_got_initial_offset (info)
		  + entry->fd_entry + 4);
    }

  /* Generate code for the PLT entry.  */
  if (entry->plt_entry != (bfd_vma) -1)
    {
      bfd_byte *plt_code = frvfdpic_plt_section (info)->contents
	+ entry->plt_entry;

      BFD_ASSERT (entry->fd_entry);

      /* Figure out what kind of PLT entry we need, depending on the
	 location of the function descriptor within the GOT.  */
      if (entry->fd_entry >= -(1 << (12 - 1))
	  && entry->fd_entry < (1 << (12 - 1)))
	{
	  /* lddi @(gr15, fd_entry), gr14 */
	  bfd_put_32 (output_bfd,
		      0x9cccf000 | (entry->fd_entry & ((1 << 12) - 1)),
		      plt_code);
	  plt_code += 4;
	}
      else
	{
	  if (entry->fd_entry >= -(1 << (16 - 1))
	      && entry->fd_entry < (1 << (16 - 1)))
	    {
	      /* setlos lo(fd_entry), gr14 */
	      bfd_put_32 (output_bfd,
			  0x9cfc0000
			  | (entry->fd_entry & (((bfd_vma)1 << 16) - 1)),
			  plt_code);
	      plt_code += 4;
	    }
	  else
	    {
	      /* sethi.p hi(fd_entry), gr14
		 setlo lo(fd_entry), gr14 */
	      bfd_put_32 (output_bfd,
			  0x1cf80000
			  | ((entry->fd_entry >> 16)
			     & (((bfd_vma)1 << 16) - 1)),
			  plt_code);
	      bfd_put_32 (output_bfd,
			  0x9cf40000
			  | (entry->fd_entry & (((bfd_vma)1 << 16) - 1)),
			  plt_code);
	      plt_code += 8;
	    }
	  /* ldd @(gr14,gr15),gr14 */
	  bfd_put_32 (output_bfd, 0x9c08e14f, plt_code);
	  plt_code += 4;
	}
      /* jmpl @(gr14,gr0) */
      bfd_put_32 (output_bfd, 0x8030e000, plt_code);
    }

  /* Generate code for the lazy PLT entry.  */
  if (entry->lzplt_entry != (bfd_vma) -1)
    {
      bfd_byte *lzplt_code = frvfdpic_plt_section (info)->contents
	+ entry->lzplt_entry;
      bfd_vma resolverStub_addr;

      bfd_put_32 (output_bfd, fd_lazy_rel_offset, lzplt_code);
      lzplt_code += 4;

      resolverStub_addr = entry->lzplt_entry / FRVFDPIC_LZPLT_BLOCK_SIZE
	* FRVFDPIC_LZPLT_BLOCK_SIZE + FRVFDPIC_LZPLT_RESOLV_LOC;
      if (resolverStub_addr >= frvfdpic_plt_initial_offset (info))
	resolverStub_addr = frvfdpic_plt_initial_offset (info) - 12;

      if (entry->lzplt_entry == resolverStub_addr)
	{
	  /* This is a lazy PLT entry that includes a resolver call.  */
	  /* ldd @(gr15,gr0), gr4
	     jmpl @(gr4,gr0)  */
	  bfd_put_32 (output_bfd, 0x8808f140, lzplt_code);
	  bfd_put_32 (output_bfd, 0x80304000, lzplt_code + 4);
	}
      else
	{
	  /* bra  resolverStub */
	  bfd_put_32 (output_bfd,
		      0xc01a0000
		      | (((resolverStub_addr - entry->lzplt_entry)
			  / 4) & (((bfd_vma)1 << 16) - 1)),
		      lzplt_code);
	}
    }

  return TRUE;
}

/* Handle an FRV small data reloc.  */

static bfd_reloc_status_type
elf32_frv_relocate_gprel12 (info, input_bfd, input_section, relocation,
			    contents, value)
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *relocation;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  bfd_vma gp;
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (info->hash, "_gp", FALSE, FALSE, TRUE);

  gp = (h->u.def.value
	+ h->u.def.section->output_section->vma
	+ h->u.def.section->output_offset);

  value -= input_section->output_section->vma;
  value -= (gp - input_section->output_section->vma);

  insn = bfd_get_32 (input_bfd, contents + relocation->r_offset);

  value += relocation->r_addend;

  if ((long) value > 0x7ff || (long) value < -0x800)
    return bfd_reloc_overflow;

  bfd_put_32 (input_bfd,
	      (insn & 0xfffff000) | (value & 0xfff),
	      contents + relocation->r_offset);

  return bfd_reloc_ok;
}

/* Handle an FRV small data reloc. for the u12 field.  */

static bfd_reloc_status_type
elf32_frv_relocate_gprelu12 (info, input_bfd, input_section, relocation,
			     contents, value)
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *relocation;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  bfd_vma gp;
  struct bfd_link_hash_entry *h;
  bfd_vma mask;

  h = bfd_link_hash_lookup (info->hash, "_gp", FALSE, FALSE, TRUE);

  gp = (h->u.def.value
	+ h->u.def.section->output_section->vma
	+ h->u.def.section->output_offset);

  value -= input_section->output_section->vma;
  value -= (gp - input_section->output_section->vma);

  insn = bfd_get_32 (input_bfd, contents + relocation->r_offset);

  value += relocation->r_addend;

  if ((long) value > 0x7ff || (long) value < -0x800)
    return bfd_reloc_overflow;

  /* The high 6 bits go into bits 17-12. The low 6 bits go into bits 5-0.  */
  mask = 0x3f03f;
  insn = (insn & ~mask) | ((value & 0xfc0) << 12) | (value & 0x3f);

  bfd_put_32 (input_bfd, insn, contents + relocation->r_offset);

  return bfd_reloc_ok;
}

/* Handle an FRV ELF HI16 reloc.  */

static bfd_reloc_status_type
elf32_frv_relocate_hi16 (input_bfd, relhi, contents, value)
     bfd *input_bfd;
     Elf_Internal_Rela *relhi;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;

  insn = bfd_get_32 (input_bfd, contents + relhi->r_offset);

  value += relhi->r_addend;
  value = ((value >> 16) & 0xffff);

  insn = (insn & 0xffff0000) | value;

  if ((long) value > 0xffff || (long) value < -0x10000)
    return bfd_reloc_overflow;

  bfd_put_32 (input_bfd, insn, contents + relhi->r_offset);
  return bfd_reloc_ok;

}
static bfd_reloc_status_type
elf32_frv_relocate_lo16 (input_bfd, rello, contents, value)
     bfd *input_bfd;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;

  insn = bfd_get_32 (input_bfd, contents + rello->r_offset);

  value += rello->r_addend;
  value = value & 0xffff;

  insn = (insn & 0xffff0000) | value;

  if ((long) value > 0xffff || (long) value < -0x10000)
    return bfd_reloc_overflow;

  bfd_put_32 (input_bfd, insn, contents + rello->r_offset);
  return bfd_reloc_ok;
}

/* Perform the relocation for the CALL label24 instruction.  */

static bfd_reloc_status_type
elf32_frv_relocate_label24 (input_bfd, input_section, rello, contents, value)
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  bfd_vma label6;
  bfd_vma label18;

  /* The format for the call instruction is:

    0 000000 0001111 000000000000000000
      label6 opcode  label18

    The branch calculation is: pc + (4*label24)
    where label24 is the concatenation of label6 and label18.  */

  /* Grab the instruction.  */
  insn = bfd_get_32 (input_bfd, contents + rello->r_offset);

  value -= input_section->output_section->vma + input_section->output_offset;
  value -= rello->r_offset;
  value += rello->r_addend;

  value = value >> 2;

  label6  = value & 0xfc0000;
  label6  = label6 << 7;

  label18 = value & 0x3ffff;

  insn = insn & 0x803c0000;
  insn = insn | label6;
  insn = insn | label18;

  bfd_put_32 (input_bfd, insn, contents + rello->r_offset);

  return bfd_reloc_ok;
}

static bfd_reloc_status_type
elf32_frv_relocate_gprelhi (info, input_bfd, input_section, relocation,
			    contents, value)
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *relocation;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  bfd_vma gp;
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (info->hash, "_gp", FALSE, FALSE, TRUE);

  gp = (h->u.def.value
        + h->u.def.section->output_section->vma
        + h->u.def.section->output_offset);

  value -= input_section->output_section->vma;
  value -= (gp - input_section->output_section->vma);
  value += relocation->r_addend;
  value = ((value >> 16) & 0xffff);

  if ((long) value > 0xffff || (long) value < -0x10000)
    return bfd_reloc_overflow;

  insn = bfd_get_32 (input_bfd, contents + relocation->r_offset);
  insn = (insn & 0xffff0000) | value;

  bfd_put_32 (input_bfd, insn, contents + relocation->r_offset);
  return bfd_reloc_ok;
}

static bfd_reloc_status_type
elf32_frv_relocate_gprello (info, input_bfd, input_section, relocation,
			    contents, value)
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *relocation;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  bfd_vma gp;
  struct bfd_link_hash_entry *h;

  h = bfd_link_hash_lookup (info->hash, "_gp", FALSE, FALSE, TRUE);

  gp = (h->u.def.value
        + h->u.def.section->output_section->vma
        + h->u.def.section->output_offset);

  value -= input_section->output_section->vma;
  value -= (gp - input_section->output_section->vma);
  value += relocation->r_addend;
  value = value & 0xffff;

  if ((long) value > 0xffff || (long) value < -0x10000)
    return bfd_reloc_overflow;

  insn = bfd_get_32 (input_bfd, contents + relocation->r_offset);
  insn = (insn & 0xffff0000) | value;

  bfd_put_32 (input_bfd, insn, contents + relocation->r_offset);

 return bfd_reloc_ok;
}

static reloc_howto_type *
frv_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    default:
      break;

    case BFD_RELOC_NONE:
      return &elf32_frv_howto_table[ (int) R_FRV_NONE];

    case BFD_RELOC_32:
      if (elf_elfheader (abfd)->e_type == ET_EXEC
	  || elf_elfheader (abfd)->e_type == ET_DYN)
	return &elf32_frv_rel_32_howto;
      /* Fall through.  */
    case BFD_RELOC_CTOR:
      return &elf32_frv_howto_table[ (int) R_FRV_32];

    case BFD_RELOC_FRV_LABEL16:
      return &elf32_frv_howto_table[ (int) R_FRV_LABEL16];

    case BFD_RELOC_FRV_LABEL24:
      return &elf32_frv_howto_table[ (int) R_FRV_LABEL24];

    case BFD_RELOC_FRV_LO16:
      return &elf32_frv_howto_table[ (int) R_FRV_LO16];

    case BFD_RELOC_FRV_HI16:
      return &elf32_frv_howto_table[ (int) R_FRV_HI16];

    case BFD_RELOC_FRV_GPREL12:
      return &elf32_frv_howto_table[ (int) R_FRV_GPREL12];

    case BFD_RELOC_FRV_GPRELU12:
      return &elf32_frv_howto_table[ (int) R_FRV_GPRELU12];

    case BFD_RELOC_FRV_GPREL32:
      return &elf32_frv_howto_table[ (int) R_FRV_GPREL32];

    case BFD_RELOC_FRV_GPRELHI:
      return &elf32_frv_howto_table[ (int) R_FRV_GPRELHI];

    case BFD_RELOC_FRV_GPRELLO:
      return &elf32_frv_howto_table[ (int) R_FRV_GPRELLO];

    case BFD_RELOC_FRV_GOT12:
      return &elf32_frv_howto_table[ (int) R_FRV_GOT12];

    case BFD_RELOC_FRV_GOTHI:
      return &elf32_frv_howto_table[ (int) R_FRV_GOTHI];

    case BFD_RELOC_FRV_GOTLO:
      return &elf32_frv_howto_table[ (int) R_FRV_GOTLO];

    case BFD_RELOC_FRV_FUNCDESC:
      if (elf_elfheader (abfd)->e_type == ET_EXEC
	  || elf_elfheader (abfd)->e_type == ET_DYN)
	return &elf32_frv_rel_funcdesc_howto;
      return &elf32_frv_howto_table[ (int) R_FRV_FUNCDESC];

    case BFD_RELOC_FRV_FUNCDESC_GOT12:
      return &elf32_frv_howto_table[ (int) R_FRV_FUNCDESC_GOT12];

    case BFD_RELOC_FRV_FUNCDESC_GOTHI:
      return &elf32_frv_howto_table[ (int) R_FRV_FUNCDESC_GOTHI];

    case BFD_RELOC_FRV_FUNCDESC_GOTLO:
      return &elf32_frv_howto_table[ (int) R_FRV_FUNCDESC_GOTLO];

    case BFD_RELOC_FRV_FUNCDESC_VALUE:
      if (elf_elfheader (abfd)->e_type == ET_EXEC
	  || elf_elfheader (abfd)->e_type == ET_DYN)
	return &elf32_frv_rel_funcdesc_value_howto;
      return &elf32_frv_howto_table[ (int) R_FRV_FUNCDESC_VALUE];

    case BFD_RELOC_FRV_FUNCDESC_GOTOFF12:
      return &elf32_frv_howto_table[ (int) R_FRV_FUNCDESC_GOTOFF12];

    case BFD_RELOC_FRV_FUNCDESC_GOTOFFHI:
      return &elf32_frv_howto_table[ (int) R_FRV_FUNCDESC_GOTOFFHI];

    case BFD_RELOC_FRV_FUNCDESC_GOTOFFLO:
      return &elf32_frv_howto_table[ (int) R_FRV_FUNCDESC_GOTOFFLO];

    case BFD_RELOC_FRV_GOTOFF12:
      return &elf32_frv_howto_table[ (int) R_FRV_GOTOFF12];

    case BFD_RELOC_FRV_GOTOFFHI:
      return &elf32_frv_howto_table[ (int) R_FRV_GOTOFFHI];

    case BFD_RELOC_FRV_GOTOFFLO:
      return &elf32_frv_howto_table[ (int) R_FRV_GOTOFFLO];

    case BFD_RELOC_VTABLE_INHERIT:
      return &elf32_frv_vtinherit_howto;

    case BFD_RELOC_VTABLE_ENTRY:
      return &elf32_frv_vtentry_howto;
    }

  return NULL;
}

/* Set the howto pointer for an FRV ELF reloc.  */

static void
frv_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  switch (r_type)
    {
    case R_FRV_GNU_VTINHERIT:
      cache_ptr->howto = &elf32_frv_vtinherit_howto;
      break;

    case R_FRV_GNU_VTENTRY:
      cache_ptr->howto = &elf32_frv_vtentry_howto;
      break;

    default:
      cache_ptr->howto = & elf32_frv_howto_table [r_type];
      break;
    }
}

/* Set the howto pointer for an FRV ELF REL reloc.  */
static void
frvfdpic_info_to_howto_rel (bfd *abfd ATTRIBUTE_UNUSED,
			    arelent *cache_ptr, Elf_Internal_Rela *dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  switch (r_type)
    {
    case R_FRV_32:
      cache_ptr->howto = &elf32_frv_rel_32_howto;
      break;

    case R_FRV_FUNCDESC:
      cache_ptr->howto = &elf32_frv_rel_funcdesc_howto;
      break;

    case R_FRV_FUNCDESC_VALUE:
      cache_ptr->howto = &elf32_frv_rel_funcdesc_value_howto;
      break;

    default:
      cache_ptr->howto = NULL;
      break;
    }
}

/* Perform a single relocation.  By default we use the standard BFD
   routines, but a few relocs, we have to do them ourselves.  */

static bfd_reloc_status_type
frv_final_link_relocate (howto, input_bfd, input_section, contents, rel,
			 relocation)
     reloc_howto_type *howto;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *rel;
     bfd_vma relocation;
{
  return _bfd_final_link_relocate (howto, input_bfd, input_section,
				   contents, rel->r_offset, relocation,
				   rel->r_addend);
}


/* Relocate an FRV ELF section.

   The RELOCATE_SECTION function is called by the new ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjusting the section contents as
   necessary, and (if using Rela relocs and generating a relocatable
   output file) adjusting the reloc addend as necessary.

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
elf32_frv_relocate_section (output_bfd, info, input_bfd, input_section,
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
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;
  unsigned isec_segment, got_segment, plt_segment, gprel_segment,
    check_segment[2];
  int silence_segment_error = !(info->shared || info->pie);

  if (info->relocatable)
    return TRUE;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  isec_segment = _frvfdpic_osec_to_segment (output_bfd,
					    input_section->output_section);
  if (IS_FDPIC (output_bfd) && frvfdpic_got_section (info))
    got_segment = _frvfdpic_osec_to_segment (output_bfd,
					     frvfdpic_got_section (info)
					     ->output_section);
  else
    got_segment = -1;
  if (IS_FDPIC (output_bfd) && frvfdpic_gotfixup_section (info))
    gprel_segment = _frvfdpic_osec_to_segment (output_bfd,
					       frvfdpic_gotfixup_section (info)
					       ->output_section);
  else
    gprel_segment = -1;
  if (IS_FDPIC (output_bfd) && elf_hash_table (info)->dynamic_sections_created)
    plt_segment = _frvfdpic_osec_to_segment (output_bfd,
					     frvfdpic_plt_section (info)
					     ->output_section);
  else
    plt_segment = -1;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;
      const char * name = NULL;
      int r_type;
      asection *osec;
      struct frvfdpic_relocs_info *picrel;
      bfd_vma orig_addend = rel->r_addend;

      r_type = ELF32_R_TYPE (rel->r_info);

      if (   r_type == R_FRV_GNU_VTINHERIT
	  || r_type == R_FRV_GNU_VTENTRY)
	continue;

      /* This is a final link.  */
      r_symndx = ELF32_R_SYM (rel->r_info);
      howto  = elf32_frv_howto_table + ELF32_R_TYPE (rel->r_info);
      h      = NULL;
      sym    = NULL;
      sec    = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  osec = sec = local_sections [r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);

	  name = bfd_elf_string_from_elf_section
	    (input_bfd, symtab_hdr->sh_link, sym->st_name);
	  name = (name == NULL) ? bfd_section_name (input_bfd, sec) : name;
	}
      else
	{
	  h = sym_hashes [r_symndx - symtab_hdr->sh_info];

	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  name = h->root.root.string;

	  if ((h->root.type == bfd_link_hash_defined
	       || h->root.type == bfd_link_hash_defweak)
	      && ! FRVFDPIC_SYM_LOCAL (info, h))
	    {
	      sec = NULL;
	      relocation = 0;
	    }
	  else
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      relocation = (h->root.u.def.value
			    + sec->output_section->vma
			    + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    {
	      relocation = 0;
	    }
	  else if (info->unresolved_syms_in_objects == RM_IGNORE
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    relocation = 0;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset,
		      (info->unresolved_syms_in_objects == RM_GENERATE_ERROR
		       || ELF_ST_VISIBILITY (h->other)))))
		return FALSE;
	      relocation = 0;
	    }
	  osec = sec;
	}

      switch (r_type)
	{
	case R_FRV_LABEL24:
	case R_FRV_32:
	  if (! IS_FDPIC (output_bfd))
	    goto non_fdpic;

	case R_FRV_GOT12:
	case R_FRV_GOTHI:
	case R_FRV_GOTLO:
	case R_FRV_FUNCDESC_GOT12:
	case R_FRV_FUNCDESC_GOTHI:
	case R_FRV_FUNCDESC_GOTLO:
	case R_FRV_GOTOFF12:
	case R_FRV_GOTOFFHI:
	case R_FRV_GOTOFFLO:
	case R_FRV_FUNCDESC_GOTOFF12:
	case R_FRV_FUNCDESC_GOTOFFHI:
	case R_FRV_FUNCDESC_GOTOFFLO:
	case R_FRV_FUNCDESC:
	case R_FRV_FUNCDESC_VALUE:
	  if (h != NULL)
	    picrel = frvfdpic_relocs_info_for_global (frvfdpic_relocs_info
						      (info), input_bfd, h,
						      orig_addend, INSERT);
	  else
	    /* In order to find the entry we created before, we must
	       use the original addend, not the one that may have been
	       modified by _bfd_elf_rela_local_sym().  */
	    picrel = frvfdpic_relocs_info_for_local (frvfdpic_relocs_info
						     (info), input_bfd, r_symndx,
						     orig_addend, INSERT);
	  if (! picrel)
	    return FALSE;

	  if (!_frvfdpic_emit_got_relocs_plt_entries (picrel, output_bfd, info,
						      osec, sym,
						      rel->r_addend))
	    {
	      (*_bfd_error_handler)
		(_("%s: relocation at `%s+0x%x' references symbol `%s' with nonzero addend"),
		 bfd_archive_filename (input_bfd), input_section->name,
		 rel->r_offset, name);
	      return FALSE;

	    }

	  break;

	default:
	non_fdpic:
	  picrel = NULL;
	  if (h && ! FRVFDPIC_SYM_LOCAL (info, h))
	    {
	      info->callbacks->warning
		(info, _("relocation references symbol not defined in the module"),
		 name, input_bfd, input_section, rel->r_offset);
	      return FALSE;
	    }
	  break;
	}

      switch (r_type)
	{
	case R_FRV_LABEL24:
	  check_segment[0] = isec_segment;
	  if (! IS_FDPIC (output_bfd))
	    check_segment[1] = isec_segment;
	  else if (picrel->plt)
	    {
	      relocation = frvfdpic_plt_section (info)->output_section->vma
		+ frvfdpic_plt_section (info)->output_offset
		+ picrel->plt_entry;
	      check_segment[1] = plt_segment;
	    }
	  /* We don't want to warn on calls to undefined weak symbols,
	     as calls to them must be protected by non-NULL tests
	     anyway, and unprotected calls would invoke undefined
	     behavior.  */
	  else if (picrel->symndx == -1
		   && picrel->d.h->root.type == bfd_link_hash_undefweak)
	    check_segment[1] = check_segment[0];
	  else
	    check_segment[1] = sec
	      ? _frvfdpic_osec_to_segment (output_bfd, sec->output_section)
	      : (unsigned)-1;
	  break;

	case R_FRV_GOT12:
	case R_FRV_GOTHI:
	case R_FRV_GOTLO:
	  relocation = picrel->got_entry;
	  check_segment[0] = check_segment[1] = got_segment;
	  break;
	  
	case R_FRV_FUNCDESC_GOT12:
	case R_FRV_FUNCDESC_GOTHI:
	case R_FRV_FUNCDESC_GOTLO:
	  relocation = picrel->fdgot_entry;
	  check_segment[0] = check_segment[1] = got_segment;
	  break;
	  
	case R_FRV_GOTOFFHI:
	case R_FRV_GOTOFF12:
	case R_FRV_GOTOFFLO:
	  relocation -= frvfdpic_got_section (info)->output_section->vma
	    + frvfdpic_got_section (info)->output_offset
	    + frvfdpic_got_initial_offset (info);
	  check_segment[0] = got_segment;
	  check_segment[1] = sec
	    ? _frvfdpic_osec_to_segment (output_bfd, sec->output_section)
	    : (unsigned)-1;
	  break;

	case R_FRV_FUNCDESC_GOTOFF12:
	case R_FRV_FUNCDESC_GOTOFFHI:
	case R_FRV_FUNCDESC_GOTOFFLO:
	  relocation = picrel->fd_entry;
	  check_segment[0] = check_segment[1] = got_segment;
	  break;

	case R_FRV_FUNCDESC:
	  {
	    int dynindx;
	    bfd_vma addend = rel->r_addend;

	    if (! (h && h->root.type == bfd_link_hash_undefweak
		   && FRVFDPIC_SYM_LOCAL (info, h)))
	      {
		/* If the symbol is dynamic and there may be dynamic
		   symbol resolution because we are or are linked with a
		   shared library, emit a FUNCDESC relocation such that
		   the dynamic linker will allocate the function
		   descriptor.  If the symbol needs a non-local function
		   descriptor but binds locally (e.g., its visibility is
		   protected, emit a dynamic relocation decayed to
		   section+offset.  */
		if (h && ! FRVFDPIC_FUNCDESC_LOCAL (info, h)
		    && FRVFDPIC_SYM_LOCAL (info, h)
		    && !(info->executable && !info->pie))
		  {
		    dynindx = elf_section_data (h->root.u.def.section
						->output_section)->dynindx;
		    addend += h->root.u.def.section->output_offset
		      + h->root.u.def.value;
		  }
		else if (h && ! FRVFDPIC_FUNCDESC_LOCAL (info, h))
		  {
		    if (addend)
		      {
			info->callbacks->warning
			  (info, _("R_FRV_FUNCDESC references dynamic symbol with nonzero addend"),
			   name, input_bfd, input_section, rel->r_offset);
			return FALSE;
		      }
		    dynindx = h->dynindx;
		  }
		else
		  {
		    /* Otherwise, we know we have a private function
		       descriptor, so reference it directly.  */
		    BFD_ASSERT (picrel->privfd);
		    r_type = R_FRV_32;
		    dynindx = elf_section_data (frvfdpic_got_section (info)
						->output_section)->dynindx;
		    addend = frvfdpic_got_section (info)->output_offset
		      + frvfdpic_got_initial_offset (info)
		      + picrel->fd_entry;
		  }

		/* If there is room for dynamic symbol resolution, emit
		   the dynamic relocation.  However, if we're linking an
		   executable at a fixed location, we won't have emitted a
		   dynamic symbol entry for the got section, so idx will
		   be zero, which means we can and should compute the
		   address of the private descriptor ourselves.  */
		if (info->executable && !info->pie
		    && (!h || FRVFDPIC_FUNCDESC_LOCAL (info, h)))
		  {
		    addend += frvfdpic_got_section (info)->output_section->vma;
		    if ((bfd_get_section_flags (output_bfd,
						input_section->output_section)
			 & (SEC_ALLOC | SEC_LOAD)) == (SEC_ALLOC | SEC_LOAD))
		      {
			if (_frvfdpic_osec_readonly_p (output_bfd,
						       input_section
						       ->output_section))
			  {
			    info->callbacks->warning
			      (info,
			       _("cannot emit fixups in read-only section"),
			       name, input_bfd, input_section, rel->r_offset);
			    return FALSE;
			  }
			_frvfdpic_add_rofixup (output_bfd,
					       frvfdpic_gotfixup_section
					       (info),
					       _bfd_elf_section_offset
					       (output_bfd, info,
						input_section, rel->r_offset)
					       + input_section
					       ->output_section->vma
					       + input_section->output_offset,
					       picrel);
		      }
		  }
		else if ((bfd_get_section_flags (output_bfd,
						 input_section->output_section)
			  & (SEC_ALLOC | SEC_LOAD)) == (SEC_ALLOC | SEC_LOAD))
		  {
		    if (_frvfdpic_osec_readonly_p (output_bfd,
						   input_section
						   ->output_section))
		      {
			info->callbacks->warning
			  (info,
			   _("cannot emit dynamic relocations in read-only section"),
			   name, input_bfd, input_section, rel->r_offset);
			return FALSE;
		      }
		    _frvfdpic_add_dyn_reloc (output_bfd,
					     frvfdpic_gotrel_section (info),
					     _bfd_elf_section_offset
					     (output_bfd, info,
					      input_section, rel->r_offset)
					     + input_section
					     ->output_section->vma
					     + input_section->output_offset,
					     r_type, dynindx, addend, picrel);
		  }
		else
		  addend += frvfdpic_got_section (info)->output_section->vma;
	      }

	    /* We want the addend in-place because dynamic
	       relocations are REL.  Setting relocation to it should
	       arrange for it to be installed.  */
	    relocation = addend - rel->r_addend;
	  }
	  check_segment[0] = check_segment[1] = got_segment;
	  break;

	case R_FRV_32:
	  if (! IS_FDPIC (output_bfd))
	    {
	      check_segment[0] = check_segment[1] = -1;
	      break;
	    }
	  /* Fall through.  */
	case R_FRV_FUNCDESC_VALUE:
	  {
	    int dynindx;
	    bfd_vma addend = rel->r_addend;

	    /* If the symbol is dynamic but binds locally, use
	       section+offset.  */
	    if (h && ! FRVFDPIC_SYM_LOCAL (info, h))
	      {
		if (addend && r_type == R_FRV_FUNCDESC_VALUE)
		  {
		    info->callbacks->warning
		      (info, _("R_FRV_FUNCDESC_VALUE references dynamic symbol with nonzero addend"),
		       name, input_bfd, input_section, rel->r_offset);
		    return FALSE;
		  }
		dynindx = h->dynindx;
	      }
	    else
	      {
		if (h)
		  addend += h->root.u.def.value;
		else
		  addend += sym->st_value;
		if (osec)
		  addend += osec->output_offset;
		if (osec && osec->output_section
		    && ! bfd_is_abs_section (osec->output_section)
		    && ! bfd_is_und_section (osec->output_section))
		  dynindx = elf_section_data (osec->output_section)->dynindx;
		else
		  dynindx = 0;
	      }

	    /* If we're linking an executable at a fixed address, we
	       can omit the dynamic relocation as long as the symbol
	       is defined in the current link unit (which is implied
	       by its output section not being NULL).  */
	    if (info->executable && !info->pie
		&& (!h || FRVFDPIC_SYM_LOCAL (info, h)))
	      {
		if (osec)
		  addend += osec->output_section->vma;
		if (IS_FDPIC (input_bfd)
		    && (bfd_get_section_flags (output_bfd,
					       input_section->output_section)
			& (SEC_ALLOC | SEC_LOAD)) == (SEC_ALLOC | SEC_LOAD))
		  {
		    if (_frvfdpic_osec_readonly_p (output_bfd,
						   input_section
						   ->output_section))
		      {
			info->callbacks->warning
			  (info,
			   _("cannot emit fixups in read-only section"),
			   name, input_bfd, input_section, rel->r_offset);
			return FALSE;
		      }
		    if (!h || h->root.type != bfd_link_hash_undefweak)
		      {
			_frvfdpic_add_rofixup (output_bfd,
					       frvfdpic_gotfixup_section
					       (info),
					       _bfd_elf_section_offset
					       (output_bfd, info,
						input_section, rel->r_offset)
					       + input_section
					       ->output_section->vma
					       + input_section->output_offset,
					       picrel);
			if (r_type == R_FRV_FUNCDESC_VALUE)
			  _frvfdpic_add_rofixup
			    (output_bfd,
			     frvfdpic_gotfixup_section (info),
			     _bfd_elf_section_offset
			     (output_bfd, info,
			      input_section, rel->r_offset)
			     + input_section->output_section->vma
			     + input_section->output_offset + 4, picrel);
		      }
		  }
	      }
	    else
	      {
		if ((bfd_get_section_flags (output_bfd,
					    input_section->output_section)
		     & (SEC_ALLOC | SEC_LOAD)) == (SEC_ALLOC | SEC_LOAD))
		  {
		    if (_frvfdpic_osec_readonly_p (output_bfd,
						   input_section
						   ->output_section))
		      {
			info->callbacks->warning
			  (info,
			   _("cannot emit dynamic relocations in read-only section"),
			   name, input_bfd, input_section, rel->r_offset);
			return FALSE;
		      }
		    _frvfdpic_add_dyn_reloc (output_bfd,
					     frvfdpic_gotrel_section (info),
					     _bfd_elf_section_offset
					     (output_bfd, info,
					      input_section, rel->r_offset)
					     + input_section
					     ->output_section->vma
					     + input_section->output_offset,
					     r_type, dynindx, addend, picrel);
		  }
		else if (osec)
		  addend += osec->output_section->vma;
		/* We want the addend in-place because dynamic
		   relocations are REL.  Setting relocation to it
		   should arrange for it to be installed.  */
		relocation = addend - rel->r_addend;
	      }

	    if (r_type == R_FRV_FUNCDESC_VALUE)
	      {
		/* If we've omitted the dynamic relocation, just emit
		   the fixed addresses of the symbol and of the local
		   GOT base offset.  */
		if (info->executable && !info->pie
		    && (!h || FRVFDPIC_SYM_LOCAL (info, h)))
		  bfd_put_32 (output_bfd,
			      frvfdpic_got_section (info)->output_section->vma
			      + frvfdpic_got_section (info)->output_offset
			      + frvfdpic_got_initial_offset (info),
			      contents + rel->r_offset + 4);
		else
		  /* A function descriptor used for lazy or local
		     resolving is initialized such that its high word
		     contains the output section index in which the
		     PLT entries are located, and the low word
		     contains the offset of the lazy PLT entry entry
		     point into that section.  */
		  bfd_put_32 (output_bfd,
			      h && ! FRVFDPIC_SYM_LOCAL (info, h)
			      ? 0
			      : _frvfdpic_osec_to_segment (output_bfd,
							   sec
							   ->output_section),
			      contents + rel->r_offset + 4);
	      }
	  }
	  check_segment[0] = check_segment[1] = got_segment;
	  break;

	case R_FRV_GPREL12:
	case R_FRV_GPRELU12:
	case R_FRV_GPREL32:
	case R_FRV_GPRELHI:
	case R_FRV_GPRELLO:
	  check_segment[0] = gprel_segment;
	  check_segment[1] = sec
	    ? _frvfdpic_osec_to_segment (output_bfd, sec->output_section)
	    : (unsigned)-1;
	  break;

	default:
	  check_segment[0] = isec_segment;
	  check_segment[1] = sec
	    ? _frvfdpic_osec_to_segment (output_bfd, sec->output_section)
	    : (unsigned)-1;
	  break;
	}

      if (check_segment[0] != check_segment[1] && IS_FDPIC (output_bfd))
	{
#if 1 /* If you take this out, remove the #error from fdpic-static-6.d
	 in the ld testsuite.  */
	  /* This helps catch problems in GCC while we can't do more
	     than static linking.  The idea is to test whether the
	     input file basename is crt0.o only once.  */
	  if (silence_segment_error == 1)
	    silence_segment_error =
	      (strlen (input_bfd->filename) == 6
	       && strcmp (input_bfd->filename, "crt0.o") == 0)
	      || (strlen (input_bfd->filename) > 6
		  && strcmp (input_bfd->filename
			     + strlen (input_bfd->filename) - 7,
			     "/crt0.o") == 0)
	      ? -1 : 0;
#endif
	  if (!silence_segment_error
	      /* We don't want duplicate errors for undefined
		 symbols.  */
	      && !(picrel && picrel->symndx == -1
		   && picrel->d.h->root.type == bfd_link_hash_undefined))
	    info->callbacks->warning
	      (info,
	       (info->shared || info->pie)
	       ? _("relocations between different segments are not supported")
	       : _("warning: relocation references a different segment"),
	       name, input_bfd, input_section, rel->r_offset);
	  if (!silence_segment_error && (info->shared || info->pie))
	    return FALSE;
	  elf_elfheader (output_bfd)->e_flags |= EF_FRV_PIC;
	}

      switch (r_type)
	{
	case R_FRV_GOTOFFHI:
	  /* We need the addend to be applied before we shift the
	     value right.  */
	  relocation += rel->r_addend;
	  /* Fall through.  */
	case R_FRV_GOTHI:
	case R_FRV_FUNCDESC_GOTHI:
	case R_FRV_FUNCDESC_GOTOFFHI:
	  relocation >>= 16;
	  /* Fall through.  */

	case R_FRV_GOTLO:
	case R_FRV_FUNCDESC_GOTLO:
	case R_FRV_GOTOFFLO:
	case R_FRV_FUNCDESC_GOTOFFLO:
	  relocation &= 0xffff;
	  break;

	default:
	  break;
	}

      switch (r_type)
	{
	case R_FRV_LABEL24:
	  if (! IS_FDPIC (output_bfd) || ! picrel->plt)
	    break;
	  /* Fall through.  */
	  
	  /* When referencing a GOT entry, a function descriptor or a
	     PLT, we don't want the addend to apply to the reference,
	     but rather to the referenced symbol.  The actual entry
	     will have already been created taking the addend into
	     account, so cancel it out here.  */
	case R_FRV_GOT12:
	case R_FRV_GOTHI:
	case R_FRV_GOTLO:
	case R_FRV_FUNCDESC_GOT12:
	case R_FRV_FUNCDESC_GOTHI:
	case R_FRV_FUNCDESC_GOTLO:
	case R_FRV_FUNCDESC_GOTOFF12:
	case R_FRV_FUNCDESC_GOTOFFHI:
	case R_FRV_FUNCDESC_GOTOFFLO:
	  /* Note that we only want GOTOFFHI, not GOTOFFLO or GOTOFF12
	     here, since we do want to apply the addend to the others.
	     Note that we've applied the addend to GOTOFFHI before we
	     shifted it right.  */ 
	case R_FRV_GOTOFFHI:
	  relocation -= rel->r_addend;
	  break;

	default:
	  break;
	}

     if (r_type == R_FRV_HI16)
       r = elf32_frv_relocate_hi16 (input_bfd, rel, contents, relocation);

     else if (r_type == R_FRV_LO16)
       r = elf32_frv_relocate_lo16 (input_bfd, rel, contents, relocation);

     else if (r_type == R_FRV_LABEL24)
       r = elf32_frv_relocate_label24 (input_bfd, input_section, rel,
				       contents, relocation);

     else if (r_type == R_FRV_GPREL12)
       r = elf32_frv_relocate_gprel12 (info, input_bfd, input_section, rel,
				       contents, relocation);

     else if (r_type == R_FRV_GPRELU12)
       r = elf32_frv_relocate_gprelu12 (info, input_bfd, input_section, rel,
					contents, relocation);

     else if (r_type == R_FRV_GPRELLO)
       r = elf32_frv_relocate_gprello (info, input_bfd, input_section, rel,
				       contents, relocation);

     else if (r_type == R_FRV_GPRELHI)
       r = elf32_frv_relocate_gprelhi (info, input_bfd, input_section, rel,
				       contents, relocation);

     else
       r = frv_final_link_relocate (howto, input_bfd, input_section, contents,
				    rel, relocation);

      if (r != bfd_reloc_ok)
	{
	  const char * msg = (const char *) NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		(info, name, howto->name, (bfd_vma) 0,
		 input_bfd, input_section, rel->r_offset);
	      break;

	    case bfd_reloc_undefined:
	      r = info->callbacks->undefined_symbol
		(info, name, input_bfd, input_section, rel->r_offset, TRUE);
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      break;

	    case bfd_reloc_notsupported:
	      msg = _("internal error: unsupported relocation error");
	      break;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous relocation");
	      break;

	    default:
	      msg = _("internal error: unknown error");
	      break;
	    }

	  if (msg)
	    r = info->callbacks->warning
	      (info, msg, name, input_bfd, input_section, rel->r_offset);

	  if (! r)
	    return FALSE;
	}
    }

  return TRUE;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
elf32_frv_gc_mark_hook (sec, info, rel, h, sym)
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
	case R_FRV_GNU_VTINHERIT:
	case R_FRV_GNU_VTENTRY:
	  break;

	default:
	  switch (h->root.type)
	    {
	    default:
	      break;

	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      return h->root.u.def.section;

	    case bfd_link_hash_common:
	      return h->root.u.c.p->section;
	    }
	}
    }
  else
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}

/* Update the got entry reference counts for the section being removed.  */

static bfd_boolean
elf32_frv_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  return TRUE;
}


/* Hook called by the linker routine which adds symbols from an object
   file.  We use it to put .comm items in .scomm, and not .comm.  */

static bfd_boolean
elf32_frv_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     Elf_Internal_Sym *sym;
     const char **namep ATTRIBUTE_UNUSED;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp;
     bfd_vma *valp;
{
  if (sym->st_shndx == SHN_COMMON
      && !info->relocatable
      && (int)sym->st_size <= (int)bfd_get_gp_size (abfd))
    {
      /* Common symbols less than or equal to -G nn bytes are
	 automatically put into .sbss.  */

      asection *scomm = bfd_get_section_by_name (abfd, ".scommon");

      if (scomm == NULL)
	{
	  scomm = bfd_make_section (abfd, ".scommon");
	  if (scomm == NULL
	      || !bfd_set_section_flags (abfd, scomm, (SEC_ALLOC
						       | SEC_IS_COMMON
						       | SEC_LINKER_CREATED)))
	    return FALSE;
	}

      *secp = scomm;
      *valp = sym->st_size;
    }

  return TRUE;
}

/* Create  a .got section, as well as its additional info field.  This
   is almost entirely copied from
   elflink.c:_bfd_elf_create_got_section().  */

static bfd_boolean
_frv_create_got_section (bfd *abfd, struct bfd_link_info *info)
{
  flagword flags;
  asection *s;
  struct elf_link_hash_entry *h;
  struct bfd_link_hash_entry *bh;
  const struct elf_backend_data *bed = get_elf_backend_data (abfd);
  int ptralign;
  int offset;

  /* This function may be called more than once.  */
  s = bfd_get_section_by_name (abfd, ".got");
  if (s != NULL && (s->flags & SEC_LINKER_CREATED) != 0)
    return TRUE;

  /* Machine specific: although pointers are 32-bits wide, we want the
     GOT to be aligned to a 64-bit boundary, such that function
     descriptors in it can be accessed with 64-bit loads and
     stores.  */
  ptralign = 3;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  s = bfd_make_section (abfd, ".got");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, flags)
      || !bfd_set_section_alignment (abfd, s, ptralign))
    return FALSE;

  if (bed->want_got_plt)
    {
      s = bfd_make_section (abfd, ".got.plt");
      if (s == NULL
	  || !bfd_set_section_flags (abfd, s, flags)
	  || !bfd_set_section_alignment (abfd, s, ptralign))
	return FALSE;
    }

  if (bed->want_got_sym)
    {
      /* Define the symbol _GLOBAL_OFFSET_TABLE_ at the start of the .got
	 (or .got.plt) section.  We don't do this in the linker script
	 because we don't want to define the symbol if we are not creating
	 a global offset table.  */
      bh = NULL;
      if (!(_bfd_generic_link_add_one_symbol
	    (info, abfd, "_GLOBAL_OFFSET_TABLE_", BSF_GLOBAL, s,
	     bed->got_symbol_offset, (const char *) NULL, FALSE,
	     bed->collect, &bh)))
	return FALSE;
      h = (struct elf_link_hash_entry *) bh;
      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
      h->type = STT_OBJECT;

      /* Machine-specific: we want the symbol for executables as
	 well.  */
      if (! bfd_elf_link_record_dynamic_symbol (info, h))
	return FALSE;

      elf_hash_table (info)->hgot = h;
    }

  /* The first bit of the global offset table is the header.  */
  s->_raw_size += bed->got_header_size + bed->got_symbol_offset;

  /* This is the machine-specific part.  Create and initialize section
     data for the got.  */
  if (IS_FDPIC (abfd))
    {
      frvfdpic_got_section (info) = s;
      frvfdpic_relocs_info (info) = htab_try_create (1, frvfdpic_relocs_info_hash,
						     frvfdpic_relocs_info_eq,
						     (htab_del) NULL);
      if (! frvfdpic_relocs_info (info))
	return FALSE;

      s = bfd_make_section (abfd, ".rel.got");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, (flags | SEC_READONLY))
	  || ! bfd_set_section_alignment (abfd, s, 2))
	return FALSE;

      frvfdpic_gotrel_section (info) = s;

      /* Machine-specific.  */
      s = bfd_make_section (abfd, ".rofixup");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, (flags | SEC_READONLY))
	  || ! bfd_set_section_alignment (abfd, s, 2))
	return FALSE;

      frvfdpic_gotfixup_section (info) = s;
      offset = -2048;
      flags = BSF_GLOBAL;
    }
  else
    {
      offset = 2048;
      flags = BSF_GLOBAL | BSF_WEAK;
    }

  /* Define _gp in .rofixup, for FDPIC, or .got otherwise.  If it
     turns out that we're linking with a different linker script, the
     linker script will override it.  */
  bh = NULL;
  if (!(_bfd_generic_link_add_one_symbol
	(info, abfd, "_gp", flags, s, offset, (const char *) NULL, FALSE,
	 bed->collect, &bh)))
    return FALSE;
  h = (struct elf_link_hash_entry *) bh;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  /* Machine-specific: we want the symbol for executables as well.  */
  if (IS_FDPIC (abfd) && ! bfd_elf_link_record_dynamic_symbol (info, h))
    return FALSE;

  return TRUE;
}

/* Make sure the got and plt sections exist, and that our pointers in
   the link hash table point to them.  */

static bfd_boolean
elf32_frvfdpic_create_dynamic_sections (bfd *abfd, struct bfd_link_info *info)
{
  /* This is mostly copied from
     elflink.c:_bfd_elf_create_dynamic_sections().  */
  flagword flags, pltflags;
  asection *s;
  const struct elf_backend_data *bed = get_elf_backend_data (abfd);

  /* We need to create .plt, .rel[a].plt, .got, .got.plt, .dynbss, and
     .rel[a].bss sections.  */

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  pltflags = flags;
  pltflags |= SEC_CODE;
  if (bed->plt_not_loaded)
    pltflags &= ~ (SEC_CODE | SEC_LOAD | SEC_HAS_CONTENTS);
  if (bed->plt_readonly)
    pltflags |= SEC_READONLY;

  s = bfd_make_section (abfd, ".plt");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, pltflags)
      || ! bfd_set_section_alignment (abfd, s, bed->plt_alignment))
    return FALSE;
  /* FRV-specific: remember it.  */
  frvfdpic_plt_section (info) = s;

  if (bed->want_plt_sym)
    {
      /* Define the symbol _PROCEDURE_LINKAGE_TABLE_ at the start of the
	 .plt section.  */
      struct elf_link_hash_entry *h;
      struct bfd_link_hash_entry *bh = NULL;

      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, "_PROCEDURE_LINKAGE_TABLE_", BSF_GLOBAL, s, 0, NULL,
	      FALSE, get_elf_backend_data (abfd)->collect, &bh)))
	return FALSE;
      h = (struct elf_link_hash_entry *) bh;
      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
      h->type = STT_OBJECT;

      if (! info->executable
	  && ! bfd_elf_link_record_dynamic_symbol (info, h))
	return FALSE;
    }

  /* FRV-specific: we want rel relocations for the plt.  */
  s = bfd_make_section (abfd, ".rel.plt");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, bed->s->log_file_align))
    return FALSE;
  /* FRV-specific: remember it.  */
  frvfdpic_pltrel_section (info) = s;

  /* FRV-specific: we want to create the GOT in the FRV way.  */
  if (! _frv_create_got_section (abfd, info))
    return FALSE;

  /* FRV-specific: make sure we created everything we wanted.  */
  BFD_ASSERT (frvfdpic_got_section (info) && frvfdpic_gotrel_section (info)
	      && frvfdpic_gotfixup_section (info)
	      && frvfdpic_plt_section (info)
	      && frvfdpic_pltrel_section (info));

  if (bed->want_dynbss)
    {
      /* The .dynbss section is a place to put symbols which are defined
	 by dynamic objects, are referenced by regular objects, and are
	 not functions.  We must allocate space for them in the process
	 image and use a R_*_COPY reloc to tell the dynamic linker to
	 initialize them at run time.  The linker script puts the .dynbss
	 section into the .bss section of the final image.  */
      s = bfd_make_section (abfd, ".dynbss");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, SEC_ALLOC | SEC_LINKER_CREATED))
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
	  if (s == NULL
	      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
	      || ! bfd_set_section_alignment (abfd, s, bed->s->log_file_align))
	    return FALSE;
	}
    }

  return TRUE;
}

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/lib/ld.so.1"

#define DEFAULT_STACK_SIZE 0x20000

/* This structure is used to collect the number of entries present in
   each addressable range of the got.  */
struct _frvfdpic_dynamic_got_info
{
  /* Several bits of information about the current link.  */
  struct bfd_link_info *info;
  /* Total size needed for GOT entries within the 12-, 16- or 32-bit
     ranges.  */
  bfd_vma got12, gotlos, gothilo;
  /* Total size needed for function descriptor entries within the 12-,
     16- or 32-bit ranges.  */
  bfd_vma fd12, fdlos, fdhilo;
  /* Total size needed function descriptor entries referenced in PLT
     entries, that would be profitable to place in offsets close to
     the PIC register.  */
  bfd_vma fdplt;
  /* Total size needed by lazy PLT entries.  */
  bfd_vma lzplt;
  /* Number of relocations carried over from input object files.  */
  unsigned long relocs;
  /* Number of fixups introduced by relocations in input object files.  */
  unsigned long fixups;
};

/* Compute the total GOT size required by each symbol in each range.
   Symbols may require up to 4 words in the GOT: an entry pointing to
   the symbol, an entry pointing to its function descriptor, and a
   private function descriptors taking two words.  */

static int
_frvfdpic_count_got_plt_entries (void **entryp, void *dinfo_)
{
  struct frvfdpic_relocs_info *entry = *entryp;
  struct _frvfdpic_dynamic_got_info *dinfo = dinfo_;
  unsigned relocs = 0, fixups = 0;

  /* Allocate space for a GOT entry pointing to the symbol.  */
  if (entry->got12)
    dinfo->got12 += 4;
  else if (entry->gotlos)
    dinfo->gotlos += 4;
  else if (entry->gothilo)
    dinfo->gothilo += 4;
  else
    entry->relocs32--;
  entry->relocs32++;

  /* Allocate space for a GOT entry pointing to the function
     descriptor.  */
  if (entry->fdgot12)
    dinfo->got12 += 4;
  else if (entry->fdgotlos)
    dinfo->gotlos += 4;
  else if (entry->fdgothilo)
    dinfo->gothilo += 4;
  else
    entry->relocsfd--;
  entry->relocsfd++;

  /* Decide whether we need a PLT entry, a function descriptor in the
     GOT, and a lazy PLT entry for this symbol.  */
  entry->plt = entry->call
    && entry->symndx == -1 && ! FRVFDPIC_SYM_LOCAL (dinfo->info, entry->d.h)
    && elf_hash_table (dinfo->info)->dynamic_sections_created;
  entry->privfd = entry->plt
    || entry->fdgoff12 || entry->fdgofflos || entry->fdgoffhilo
    || ((entry->fd || entry->fdgot12 || entry->fdgotlos || entry->fdgothilo)
	&& (entry->symndx != -1
	    || FRVFDPIC_FUNCDESC_LOCAL (dinfo->info, entry->d.h)));
  entry->lazyplt = entry->privfd
    && entry->symndx == -1 && ! FRVFDPIC_SYM_LOCAL (dinfo->info, entry->d.h)
    && ! (dinfo->info->flags & DF_BIND_NOW)
    && elf_hash_table (dinfo->info)->dynamic_sections_created;

  /* Allocate space for a function descriptor.  */
  if (entry->fdgoff12)
    dinfo->fd12 += 8;
  else if (entry->fdgofflos)
    dinfo->fdlos += 8;
  else if (entry->privfd && entry->plt)
    dinfo->fdplt += 8;
  else if (entry->privfd)
    dinfo->fdhilo += 8;
  else
    entry->relocsfdv--;
  entry->relocsfdv++;

  if (entry->lazyplt)
    dinfo->lzplt += 8;

  if (!dinfo->info->executable || dinfo->info->pie)
    relocs = entry->relocs32 + entry->relocsfd + entry->relocsfdv;
  else
    {
      if (entry->symndx != -1 || FRVFDPIC_SYM_LOCAL (dinfo->info, entry->d.h))
	{
	  if (entry->symndx != -1
	      || entry->d.h->root.type != bfd_link_hash_undefweak)
	    fixups += entry->relocs32 + 2 * entry->relocsfdv;
	}
      else
	relocs += entry->relocs32 + entry->relocsfdv;

      if (entry->symndx != -1
	  || FRVFDPIC_FUNCDESC_LOCAL (dinfo->info, entry->d.h))
	{
	  if (entry->symndx != -1
	      || entry->d.h->root.type != bfd_link_hash_undefweak)
	    fixups += entry->relocsfd;
	}
      else
	relocs += entry->relocsfd;
    }

  entry->dynrelocs += relocs;
  entry->fixups += fixups;
  dinfo->relocs += relocs;
  dinfo->fixups += fixups;

  return 1;
}

/* This structure is used to assign offsets to got entries, function
   descriptors, plt entries and lazy plt entries.  */

struct _frvfdpic_dynamic_got_plt_info
{
  /* Summary information collected with _frvfdpic_count_got_plt_entries.  */
  struct _frvfdpic_dynamic_got_info g;

  /* For each addressable range, we record a MAX (positive) and MIN
     (negative) value.  CUR is used to assign got entries, and it's
     incremented from an initial positive value to MAX, then from MIN
     to FDCUR (unless FDCUR wraps around first).  FDCUR is used to
     assign function descriptors, and it's decreased from an initial
     non-positive value to MIN, then from MAX down to CUR (unless CUR
     wraps around first).  All of MIN, MAX, CUR and FDCUR always point
     to even words.  ODD, if non-zero, indicates an odd word to be
     used for the next got entry, otherwise CUR is used and
     incremented by a pair of words, wrapping around when it reaches
     MAX.  FDCUR is decremented (and wrapped) before the next function
     descriptor is chosen.  FDPLT indicates the number of remaining
     slots that can be used for function descriptors used only by PLT
     entries.  */
  struct _frvfdpic_dynamic_got_alloc_data
  {
    bfd_signed_vma max, cur, odd, fdcur, min;
    bfd_vma fdplt;
  } got12, gotlos, gothilo;
};

/* Determine the positive and negative ranges to be used by each
   offset range in the GOT.  FDCUR and CUR, that must be aligned to a
   double-word boundary, are the minimum (negative) and maximum
   (positive) GOT offsets already used by previous ranges, except for
   an ODD entry that may have been left behind.  GOT and FD indicate
   the size of GOT entries and function descriptors that must be
   placed within the range from -WRAP to WRAP.  If there's room left,
   up to FDPLT bytes should be reserved for additional function
   descriptors.  */

inline static bfd_signed_vma
_frvfdpic_compute_got_alloc_data (struct _frvfdpic_dynamic_got_alloc_data *gad,
				  bfd_signed_vma fdcur,
				  bfd_signed_vma odd,
				  bfd_signed_vma cur,
				  bfd_vma got,
				  bfd_vma fd,
				  bfd_vma fdplt,
				  bfd_vma wrap)
{
  bfd_signed_vma wrapmin = -wrap;

  /* Start at the given initial points.  */
  gad->fdcur = fdcur;
  gad->cur = cur;

  /* If we had an incoming odd word and we have any got entries that
     are going to use it, consume it, otherwise leave gad->odd at
     zero.  We might force gad->odd to zero and return the incoming
     odd such that it is used by the next range, but then GOT entries
     might appear to be out of order and we wouldn't be able to
     shorten the GOT by one word if it turns out to end with an
     unpaired GOT entry.  */
  if (odd && got)
    {
      gad->odd = odd;
      got -= 4;
      odd = 0;
    }
  else
    gad->odd = 0;

  /* If we're left with an unpaired GOT entry, compute its location
     such that we can return it.  Otherwise, if got doesn't require an
     odd number of words here, either odd was already zero in the
     block above, or it was set to zero because got was non-zero, or
     got was already zero.  In the latter case, we want the value of
     odd to carry over to the return statement, so we don't want to
     reset odd unless the condition below is true.  */
  if (got & 4)
    {
      odd = cur + got;
      got += 4;
    }
  
  /* Compute the tentative boundaries of this range.  */
  gad->max = cur + got;
  gad->min = fdcur - fd;
  gad->fdplt = 0;

  /* If function descriptors took too much space, wrap some of them
     around.  */
  if (gad->min < wrapmin)
    {
      gad->max += wrapmin - gad->min;
      gad->min = wrapmin;
    }
  /* If there is space left and we have function descriptors
     referenced in PLT entries that could take advantage of shorter
     offsets, place them here.  */
  else if (fdplt && gad->min > wrapmin)
    {
      bfd_vma fds;
      if ((bfd_vma) (gad->min - wrapmin) < fdplt)
	fds = gad->min - wrapmin;
      else
	fds = fdplt;

      fdplt -= fds;
      gad->min -= fds;
      gad->fdplt += fds;
    }

  /* If GOT entries took too much space, wrap some of them around.
     This may well cause gad->min to become lower than wrapmin.  This
     will cause a relocation overflow later on, so we don't have to
     report it here . */
  if ((bfd_vma) gad->max > wrap)
    {
      gad->min -= gad->max - wrap;
      gad->max = wrap;
    }
  /* If there is more space left, try to place some more function
     descriptors for PLT entries.  */
  else if (fdplt && (bfd_vma) gad->max < wrap)
    {
      bfd_vma fds;
      if ((bfd_vma) (wrap - gad->max) < fdplt)
	fds = wrap - gad->max;
      else
	fds = fdplt;

      fdplt -= fds;
      gad->max += fds;
      gad->fdplt += fds;
    }

  /* If odd was initially computed as an offset past the wrap point,
     wrap it around.  */
  if (odd > gad->max)
    odd = gad->min + odd - gad->max;

  /* _frvfdpic_get_got_entry() below will always wrap gad->cur if needed
     before returning, so do it here too.  This guarantees that,
     should cur and fdcur meet at the wrap point, they'll both be
     equal to min.  */
  if (gad->cur == gad->max)
    gad->cur = gad->min;

  return odd;
}

/* Compute the location of the next GOT entry, given the allocation
   data for a range.  */

inline static bfd_signed_vma
_frvfdpic_get_got_entry (struct _frvfdpic_dynamic_got_alloc_data *gad)
{
  bfd_signed_vma ret;
  
  if (gad->odd)
    {
      /* If there was an odd word left behind, use it.  */
      ret = gad->odd;
      gad->odd = 0;
    }
  else
    {
      /* Otherwise, use the word pointed to by cur, reserve the next
	 as an odd word, and skip to the next pair of words, possibly
	 wrapping around.  */
      ret = gad->cur;
      gad->odd = gad->cur + 4;
      gad->cur += 8;
      if (gad->cur == gad->max)
	gad->cur = gad->min;
    }

  return ret;
}

/* Compute the location of the next function descriptor entry in the
   GOT, given the allocation data for a range.  */

inline static bfd_signed_vma
_frvfdpic_get_fd_entry (struct _frvfdpic_dynamic_got_alloc_data *gad)
{
  /* If we're at the bottom, wrap around, and only then allocate the
     next pair of words.  */
  if (gad->fdcur == gad->min)
    gad->fdcur = gad->max;
  return gad->fdcur -= 8;
}

/* Assign GOT offsets for every GOT entry and function descriptor.
   Doing everything in a single pass is tricky.  */

static int
_frvfdpic_assign_got_entries (void **entryp, void *info_)
{
  struct frvfdpic_relocs_info *entry = *entryp;
  struct _frvfdpic_dynamic_got_plt_info *dinfo = info_;

  if (entry->got12)
    entry->got_entry = _frvfdpic_get_got_entry (&dinfo->got12);
  else if (entry->gotlos)
    entry->got_entry = _frvfdpic_get_got_entry (&dinfo->gotlos);
  else if (entry->gothilo)
    entry->got_entry = _frvfdpic_get_got_entry (&dinfo->gothilo);

  if (entry->fdgot12)
    entry->fdgot_entry = _frvfdpic_get_got_entry (&dinfo->got12);
  else if (entry->fdgotlos)
    entry->fdgot_entry = _frvfdpic_get_got_entry (&dinfo->gotlos);
  else if (entry->fdgothilo)
    entry->fdgot_entry = _frvfdpic_get_got_entry (&dinfo->gothilo);

  if (entry->fdgoff12)
    entry->fd_entry = _frvfdpic_get_fd_entry (&dinfo->got12);
  else if (entry->plt && dinfo->got12.fdplt)
    {
      dinfo->got12.fdplt -= 8;
      entry->fd_entry = _frvfdpic_get_fd_entry (&dinfo->got12);
    }
  else if (entry->fdgofflos)
    entry->fd_entry = _frvfdpic_get_fd_entry (&dinfo->gotlos);
  else if (entry->plt && dinfo->gotlos.fdplt)
    {
      dinfo->gotlos.fdplt -= 8;
      entry->fd_entry = _frvfdpic_get_fd_entry (&dinfo->gotlos);
    }
  else if (entry->plt)
    {
      dinfo->gothilo.fdplt -= 8;
      entry->fd_entry = _frvfdpic_get_fd_entry (&dinfo->gothilo);
    }
  else if (entry->privfd)
    entry->fd_entry = _frvfdpic_get_fd_entry (&dinfo->gothilo);
  
  return 1;
}

/* Assign GOT offsets to private function descriptors used by PLT
   entries (or referenced by 32-bit offsets), as well as PLT entries
   and lazy PLT entries.  */

static int
_frvfdpic_assign_plt_entries (void **entryp, void *info_)
{
  struct frvfdpic_relocs_info *entry = *entryp;
  struct _frvfdpic_dynamic_got_plt_info *dinfo = info_;

  /* If this symbol requires a local function descriptor, allocate
     one.  */
  if (entry->privfd && entry->fd_entry == 0)
    {
      if (dinfo->got12.fdplt)
	{
	  entry->fd_entry = _frvfdpic_get_fd_entry (&dinfo->got12);
	  dinfo->got12.fdplt -= 8;
	}
      else if (dinfo->gotlos.fdplt)
	{
	  entry->fd_entry = _frvfdpic_get_fd_entry (&dinfo->gotlos);
	  dinfo->gotlos.fdplt -= 8;
	}
      else
	{
	  BFD_ASSERT (dinfo->gothilo.fdplt)
	  entry->fd_entry = _frvfdpic_get_fd_entry (&dinfo->gothilo);
	  dinfo->gothilo.fdplt -= 8;
	}
    }

  if (entry->plt)
    {
      int size;

      /* We use the section's raw size to mark the location of the
	 next PLT entry.  */
      entry->plt_entry = frvfdpic_plt_section (dinfo->g.info)->_raw_size;

      /* Figure out the length of this PLT entry based on the
	 addressing mode we need to reach the function descriptor.  */
      BFD_ASSERT (entry->fd_entry);
      if (entry->fd_entry >= -(1 << (12 - 1))
	  && entry->fd_entry < (1 << (12 - 1)))
	size = 8;
      else if (entry->fd_entry >= -(1 << (16 - 1))
	       && entry->fd_entry < (1 << (16 - 1)))
	size = 12;
      else
	size = 16;

      frvfdpic_plt_section (dinfo->g.info)->_raw_size += size;
    }

  if (entry->lazyplt)
    {
      entry->lzplt_entry = dinfo->g.lzplt;
      dinfo->g.lzplt += 8;
      /* If this entry is the one that gets the resolver stub, account
	 for the additional instruction.  */
      if (entry->lzplt_entry % FRVFDPIC_LZPLT_BLOCK_SIZE
	  == FRVFDPIC_LZPLT_RESOLV_LOC)
	dinfo->g.lzplt += 4;
    }
      
  return 1;
}  

/* Follow indirect and warning hash entries so that each got entry
   points to the final symbol definition.  P must point to a pointer
   to the hash table we're traversing.  Since this traversal may
   modify the hash table, we set this pointer to NULL to indicate
   we've made a potentially-destructive change to the hash table, so
   the traversal must be restarted.  */
static int
_frvfdpic_resolve_final_relocs_info (void **entryp, void *p)
{
  struct frvfdpic_relocs_info *entry = *entryp;
  htab_t *htab = p;

  if (entry->symndx == -1)
    {
      struct elf_link_hash_entry *h = entry->d.h;
      struct frvfdpic_relocs_info *oentry;

      while (h->root.type == bfd_link_hash_indirect
	     || h->root.type == bfd_link_hash_warning)
	h = (struct elf_link_hash_entry *)h->root.u.i.link;

      if (entry->d.h == h)
	return 1;

      oentry = frvfdpic_relocs_info_for_global (*htab, 0, h, entry->addend,
						NO_INSERT);

      if (oentry)
	{
	  /* Merge the two entries.  */
	  frvfdpic_pic_merge_early_relocs_info (oentry, entry);
	  htab_clear_slot (*htab, entryp);
	  return 1;
	}

      entry->d.h = h;

      /* If we can't find this entry with the new bfd hash, re-insert
	 it, and get the traversal restarted.  */
      if (! htab_find (*htab, entry))
	{
	  htab_clear_slot (*htab, entryp);
	  entryp = htab_find_slot (*htab, entry, INSERT);
	  if (! *entryp)
	    *entryp = entry;
	  /* Abort the traversal, since the whole table may have
	     moved, and leave it up to the parent to restart the
	     process.  */
	  *(htab_t *)p = NULL;
	  return 0;
	}
    }

  return 1;
}

/* Set the sizes of the dynamic sections.  */

static bfd_boolean
elf32_frvfdpic_size_dynamic_sections (bfd *output_bfd,
				      struct bfd_link_info *info)
{
  bfd *dynobj;
  asection *s;
  struct _frvfdpic_dynamic_got_plt_info gpinfo;
  bfd_signed_vma odd;
  bfd_vma limit;

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (info->executable)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (bfd_byte *) ELF_DYNAMIC_INTERPRETER;
	}
    }

  memset (&gpinfo, 0, sizeof (gpinfo));
  gpinfo.g.info = info;

  for (;;)
    {
      htab_t relocs = frvfdpic_relocs_info (info);

      htab_traverse (relocs, _frvfdpic_resolve_final_relocs_info, &relocs);

      if (relocs == frvfdpic_relocs_info (info))
	break;
    }

  htab_traverse (frvfdpic_relocs_info (info), _frvfdpic_count_got_plt_entries,
		 &gpinfo.g);

  odd = 12;
  /* Compute the total size taken by entries in the 12-bit and 16-bit
     ranges, to tell how many PLT function descriptors we can bring
     into the 12-bit range without causing the 16-bit range to
     overflow.  */
  limit = odd + gpinfo.g.got12 + gpinfo.g.gotlos
    + gpinfo.g.fd12 + gpinfo.g.fdlos;
  if (limit < (bfd_vma)1 << 16)
    limit = ((bfd_vma)1 << 16) - limit;
  else
    limit = 0;
  if (gpinfo.g.fdplt < limit)
    limit = gpinfo.g.fdplt;

  /* Determine the ranges of GOT offsets that we can use for each
     range of addressing modes.  */
  odd = _frvfdpic_compute_got_alloc_data (&gpinfo.got12,
					  0,
					  odd,
					  16,
					  gpinfo.g.got12,
					  gpinfo.g.fd12,
					  limit,
					  (bfd_vma)1 << (12-1));
  odd = _frvfdpic_compute_got_alloc_data (&gpinfo.gotlos,
					  gpinfo.got12.min,
					  odd,
					  gpinfo.got12.max,
					  gpinfo.g.gotlos,
					  gpinfo.g.fdlos,
					  gpinfo.g.fdplt - gpinfo.got12.fdplt,
					  (bfd_vma)1 << (16-1));
  odd = _frvfdpic_compute_got_alloc_data (&gpinfo.gothilo,
					  gpinfo.gotlos.min,
					  odd,
					  gpinfo.gotlos.max,
					  gpinfo.g.gothilo,
					  gpinfo.g.fdhilo,
					  gpinfo.g.fdplt - gpinfo.got12.fdplt
					  - gpinfo.gotlos.fdplt,
					  (bfd_vma)1 << (32-1));

  /* Now assign (most) GOT offsets.  */
  htab_traverse (frvfdpic_relocs_info (info), _frvfdpic_assign_got_entries,
		 &gpinfo);

  frvfdpic_got_section (info)->_raw_size = gpinfo.gothilo.max
    - gpinfo.gothilo.min
    /* If an odd word is the last word of the GOT, we don't need this
       word to be part of the GOT.  */
    - (odd + 4 == gpinfo.gothilo.max ? 4 : 0);
  if (frvfdpic_got_section (info)->_raw_size == 0)
    frvfdpic_got_section (info)->flags |= SEC_EXCLUDE;
  else if (frvfdpic_got_section (info)->_raw_size == 12
	   && ! elf_hash_table (info)->dynamic_sections_created)
    {
      frvfdpic_got_section (info)->flags |= SEC_EXCLUDE;
      frvfdpic_got_section (info)->_raw_size = 0;
    }
  else
    {
      frvfdpic_got_section (info)->contents =
	(bfd_byte *) bfd_zalloc (dynobj,
				 frvfdpic_got_section (info)->_raw_size);
      if (frvfdpic_got_section (info)->contents == NULL)
	return FALSE;
    }
  
  if (elf_hash_table (info)->dynamic_sections_created)
    /* Subtract the number of lzplt entries, since those will generate
       relocations in the pltrel section.  */
    frvfdpic_gotrel_section (info)->_raw_size =
      (gpinfo.g.relocs - gpinfo.g.lzplt / 8)
      * get_elf_backend_data (output_bfd)->s->sizeof_rel;
  else
    BFD_ASSERT (gpinfo.g.relocs == 0);
  if (frvfdpic_gotrel_section (info)->_raw_size == 0)
    frvfdpic_gotrel_section (info)->flags |= SEC_EXCLUDE;
  else
    {
      frvfdpic_gotrel_section (info)->contents =
	(bfd_byte *) bfd_zalloc (dynobj,
				 frvfdpic_gotrel_section (info)->_raw_size);
      if (frvfdpic_gotrel_section (info)->contents == NULL)
	return FALSE;
    }

  frvfdpic_gotfixup_section (info)->_raw_size = (gpinfo.g.fixups + 1) * 4;
  if (frvfdpic_gotfixup_section (info)->_raw_size == 0)
    frvfdpic_gotfixup_section (info)->flags |= SEC_EXCLUDE;
  else
    {
      frvfdpic_gotfixup_section (info)->contents =
	(bfd_byte *) bfd_zalloc (dynobj,
				 frvfdpic_gotfixup_section (info)->_raw_size);
      if (frvfdpic_gotfixup_section (info)->contents == NULL)
	return FALSE;
    }
  
  if (elf_hash_table (info)->dynamic_sections_created)
    {
      frvfdpic_pltrel_section (info)->_raw_size =
	gpinfo.g.lzplt / 8 * get_elf_backend_data (output_bfd)->s->sizeof_rel;
      if (frvfdpic_pltrel_section (info)->_raw_size == 0)
	frvfdpic_pltrel_section (info)->flags |= SEC_EXCLUDE;
      else
	{
	  frvfdpic_pltrel_section (info)->contents =
	    (bfd_byte *) bfd_zalloc (dynobj,
				     frvfdpic_pltrel_section (info)
				     ->_raw_size);
	  if (frvfdpic_pltrel_section (info)->contents == NULL)
	    return FALSE;
	}
    }
  
  /* Add 4 bytes for every block of at most 65535 lazy PLT entries,
     such that there's room for the additional instruction needed to
     call the resolver.  Since _frvfdpic_assign_got_entries didn't
     account for them, our block size is 4 bytes smaller than the real
     block size.  */
  if (elf_hash_table (info)->dynamic_sections_created)
    {
      frvfdpic_plt_section (info)->_raw_size = gpinfo.g.lzplt
	+ ((gpinfo.g.lzplt + (FRVFDPIC_LZPLT_BLOCK_SIZE - 4) - 8)
	   / (FRVFDPIC_LZPLT_BLOCK_SIZE - 4) * 4);
    }

  /* Reset it, such that _frvfdpic_assign_plt_entries() can use it to
     actually assign lazy PLT entries addresses.  */
  gpinfo.g.lzplt = 0;

  /* Save information that we're going to need to generate GOT and PLT
     entries.  */
  frvfdpic_got_initial_offset (info) = -gpinfo.gothilo.min;

  if (get_elf_backend_data (output_bfd)->want_got_sym)
    elf_hash_table (info)->hgot->root.u.def.value
      += frvfdpic_got_initial_offset (info);

  if (elf_hash_table (info)->dynamic_sections_created)
    frvfdpic_plt_initial_offset (info) =
      frvfdpic_plt_section (info)->_raw_size;

  htab_traverse (frvfdpic_relocs_info (info), _frvfdpic_assign_plt_entries,
		 &gpinfo);

  /* Allocate the PLT section contents only after
     _frvfdpic_assign_plt_entries has a chance to add the size of the
     non-lazy PLT entries.  */
  if (elf_hash_table (info)->dynamic_sections_created)
    {
      if (frvfdpic_plt_section (info)->_raw_size == 0)
	frvfdpic_plt_section (info)->flags |= SEC_EXCLUDE;
      else
	{
	  frvfdpic_plt_section (info)->contents =
	    (bfd_byte *) bfd_zalloc (dynobj,
				     frvfdpic_plt_section (info)->_raw_size);
	  if (frvfdpic_plt_section (info)->contents == NULL)
	    return FALSE;
	}
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      if (frvfdpic_got_section (info)->_raw_size)
	if (!_bfd_elf_add_dynamic_entry (info, DT_PLTGOT, 0))
	  return FALSE;

      if (frvfdpic_pltrel_section (info)->_raw_size)
	if (!_bfd_elf_add_dynamic_entry (info, DT_PLTRELSZ, 0)
	    || !_bfd_elf_add_dynamic_entry (info, DT_PLTREL, DT_REL)
	    || !_bfd_elf_add_dynamic_entry (info, DT_JMPREL, 0))
	  return FALSE;

      if (frvfdpic_gotrel_section (info)->_raw_size)
	if (!_bfd_elf_add_dynamic_entry (info, DT_REL, 0)
	    || !_bfd_elf_add_dynamic_entry (info, DT_RELSZ, 0)
	    || !_bfd_elf_add_dynamic_entry (info, DT_RELENT,
					    sizeof (Elf32_External_Rel)))
	  return FALSE;
    }

  return TRUE;
}

static bfd_boolean
elf32_frvfdpic_always_size_sections (bfd *output_bfd,
				     struct bfd_link_info *info)
{
  if (!info->relocatable)
    {
      struct elf_link_hash_entry *h;
      asection *sec;

      /* Force a PT_GNU_STACK segment to be created.  */
      if (! elf_tdata (output_bfd)->stack_flags)
	elf_tdata (output_bfd)->stack_flags = PF_R | PF_W | PF_X;

      /* Define __stacksize if it's not defined yet.  */
      h = elf_link_hash_lookup (elf_hash_table (info), "__stacksize",
				FALSE, FALSE, FALSE);
      if (! h || h->root.type != bfd_link_hash_defined
	  || h->type != STT_OBJECT
	  || !(h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	{
	  struct bfd_link_hash_entry *bh = NULL;

	  if (!(_bfd_generic_link_add_one_symbol
		(info, output_bfd, "__stacksize",
		 BSF_GLOBAL, bfd_abs_section_ptr, DEFAULT_STACK_SIZE,
		 (const char *) NULL, FALSE,
		 get_elf_backend_data (output_bfd)->collect, &bh)))
	    return FALSE;

	  h = (struct elf_link_hash_entry *) bh;
	  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	  h->type = STT_OBJECT;
	}

      /* Create a stack section, and set its alignment.  */
      sec = bfd_make_section (output_bfd, ".stack");

      if (sec == NULL
	  || ! bfd_set_section_alignment (output_bfd, sec, 3))
	return FALSE;
    }

  return TRUE;
}

static bfd_boolean
elf32_frvfdpic_modify_segment_map (bfd *output_bfd,
				   struct bfd_link_info *info)
{
  struct elf_segment_map *m;

  /* objcopy and strip preserve what's already there using
     elf32_frvfdpic_copy_private_bfd_data ().  */
  if (! info)
    return TRUE;

  for (m = elf_tdata (output_bfd)->segment_map; m != NULL; m = m->next)
    if (m->p_type == PT_GNU_STACK)
      break;

  if (m)
    {
      asection *sec = bfd_get_section_by_name (output_bfd, ".stack");
      struct elf_link_hash_entry *h;

      if (sec)
	{
	  /* Obtain the pointer to the __stacksize symbol.  */
	  h = elf_link_hash_lookup (elf_hash_table (info), "__stacksize",
				    FALSE, FALSE, FALSE);
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *)h->root.u.i.link;
	  BFD_ASSERT (h->root.type == bfd_link_hash_defined);

	  /* Set the section size from the symbol value.  We
	     intentionally ignore the symbol section.  */
	  if (h->root.type == bfd_link_hash_defined)
	    sec->_raw_size = h->root.u.def.value;
	  else
	    sec->_raw_size = DEFAULT_STACK_SIZE;

	  /* Add the stack section to the PT_GNU_STACK segment,
	     such that its size and alignment requirements make it
	     to the segment.  */
	  m->sections[m->count] = sec;
	  m->count++;
	}
    }

  return TRUE;
}

/* Fill in code and data in dynamic sections.  */

static bfd_boolean
elf32_frv_finish_dynamic_sections (bfd *output_bfd ATTRIBUTE_UNUSED,
				   struct bfd_link_info *info ATTRIBUTE_UNUSED)
{
  /* Nothing to be done for non-FDPIC.  */
  return TRUE;
}

static bfd_boolean
elf32_frvfdpic_finish_dynamic_sections (bfd *output_bfd,
					struct bfd_link_info *info)
{
  bfd *dynobj;
  asection *sdyn;

  dynobj = elf_hash_table (info)->dynobj;

  if (frvfdpic_got_section (info))
    {
      BFD_ASSERT (frvfdpic_gotrel_section (info)->_raw_size
		  == (frvfdpic_gotrel_section (info)->reloc_count
		      * sizeof (Elf32_External_Rel)));

      if (frvfdpic_gotfixup_section (info))
	{
	  struct elf_link_hash_entry *hgot = elf_hash_table (info)->hgot;
	  bfd_vma got_value = hgot->root.u.def.value
	    + hgot->root.u.def.section->output_section->vma
	    + hgot->root.u.def.section->output_offset;

	  _frvfdpic_add_rofixup (output_bfd, frvfdpic_gotfixup_section (info),
				 got_value, 0);

	  if (frvfdpic_gotfixup_section (info)->_raw_size
	      != (frvfdpic_gotfixup_section (info)->reloc_count * 4))
	    {
	      (*_bfd_error_handler)
		("LINKER BUG: .rofixup section size mismatch");
	      return FALSE;
	    }
	}
    }
  if (elf_hash_table (info)->dynamic_sections_created)
    {
      BFD_ASSERT (frvfdpic_pltrel_section (info)->_raw_size
		  == (frvfdpic_pltrel_section (info)->reloc_count
		      * sizeof (Elf32_External_Rel)));
    }

  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      Elf32_External_Dyn * dyncon;
      Elf32_External_Dyn * dynconend;

      BFD_ASSERT (sdyn != NULL);

      dyncon = (Elf32_External_Dyn *) sdyn->contents;
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->_raw_size);

      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;

	  bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;

	    case DT_PLTGOT:
	      dyn.d_un.d_ptr = frvfdpic_got_section (info)->output_section->vma
		+ frvfdpic_got_section (info)->output_offset
		+ frvfdpic_got_initial_offset (info);
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_JMPREL:
	      dyn.d_un.d_ptr = frvfdpic_pltrel_section (info)
		->output_section->vma
		+ frvfdpic_pltrel_section (info)->output_offset;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_PLTRELSZ:
	      if (frvfdpic_pltrel_section (info)->_cooked_size != 0)
		dyn.d_un.d_val = frvfdpic_pltrel_section (info)->_cooked_size;
	      else
		dyn.d_un.d_val = frvfdpic_pltrel_section (info)->_raw_size;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;
	    }
	}
    }

  return TRUE;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  */

static bfd_boolean
elf32_frvfdpic_adjust_dynamic_symbol
(struct bfd_link_info *info ATTRIBUTE_UNUSED,
 struct elf_link_hash_entry *h ATTRIBUTE_UNUSED)
{
  bfd * dynobj;

  dynobj = elf_hash_table (info)->dynobj;

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (dynobj != NULL
	      && (h->weakdef != NULL
		  || ((h->elf_link_hash_flags
		       & ELF_LINK_HASH_DEF_DYNAMIC) != 0
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_REF_REGULAR) != 0
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)));

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
		  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
    }

  return TRUE;
}

/* Perform any actions needed for dynamic symbols.  */

static bfd_boolean
elf32_frvfdpic_finish_dynamic_symbol
(bfd *output_bfd ATTRIBUTE_UNUSED,
 struct bfd_link_info *info ATTRIBUTE_UNUSED,
 struct elf_link_hash_entry *h ATTRIBUTE_UNUSED,
 Elf_Internal_Sym *sym ATTRIBUTE_UNUSED)
{
  return TRUE;
}

/* Decide whether to attempt to turn absptr or lsda encodings in
   shared libraries into pcrel within the given input section.  */

static bfd_boolean
frvfdpic_elf_use_relative_eh_frame
(bfd *input_bfd ATTRIBUTE_UNUSED,
 struct bfd_link_info *info ATTRIBUTE_UNUSED,
 asection *eh_frame_section ATTRIBUTE_UNUSED)
{
  /* We can't use PC-relative encodings in FDPIC binaries, in general.  */
  return FALSE;
}

/* Adjust the contents of an eh_frame_hdr section before they're output.  */

static bfd_byte
frvfdpic_elf_encode_eh_address (bfd *abfd,
				struct bfd_link_info *info,
				asection *osec, bfd_vma offset,
				asection *loc_sec, bfd_vma loc_offset,
				bfd_vma *encoded)
{
  struct elf_link_hash_entry *h;

  h = elf_hash_table (info)->hgot;
  BFD_ASSERT (h && h->root.type == bfd_link_hash_defined);

  if (! h || (_frvfdpic_osec_to_segment (abfd, osec)
	      == _frvfdpic_osec_to_segment (abfd, loc_sec->output_section)))
    return _bfd_elf_encode_eh_address (abfd, info, osec, offset,
				       loc_sec, loc_offset, encoded);

  BFD_ASSERT (_frvfdpic_osec_to_segment (abfd, osec)
	      == (_frvfdpic_osec_to_segment
		  (abfd, h->root.u.def.section->output_section)));

  *encoded = osec->vma + offset
    - (h->root.u.def.value
       + h->root.u.def.section->output_section->vma
       + h->root.u.def.section->output_offset);

  return DW_EH_PE_datarel | DW_EH_PE_sdata4;
}

/* Look through the relocs for a section during the first phase.

   Besides handling virtual table relocs for gc, we have to deal with
   all sorts of PIC-related relocations.  We describe below the
   general plan on how to handle such relocations, even though we only
   collect information at this point, storing them in hash tables for
   perusal of later passes.

   32 relocations are propagated to the linker output when creating
   position-independent output.  LO16 and HI16 relocations are not
   supposed to be encountered in this case.

   LABEL16 should always be resolvable by the linker, since it's only
   used by branches.

   LABEL24, on the other hand, is used by calls.  If it turns out that
   the target of a call is a dynamic symbol, a PLT entry must be
   created for it, which triggers the creation of a private function
   descriptor and, unless lazy binding is disabled, a lazy PLT entry.

   GPREL relocations require the referenced symbol to be in the same
   segment as _gp, but this can only be checked later.

   All GOT, GOTOFF and FUNCDESC relocations require a .got section to
   exist.  LABEL24 might as well, since it may require a PLT entry,
   that will require a got.

   Non-FUNCDESC GOT relocations require a GOT entry to be created
   regardless of whether the symbol is dynamic.  However, since a
   global symbol that turns out to not be exported may have the same
   address of a non-dynamic symbol, we don't assign GOT entries at
   this point, such that we can share them in this case.  A relocation
   for the GOT entry always has to be created, be it to offset a
   private symbol by the section load address, be it to get the symbol
   resolved dynamically.

   FUNCDESC GOT relocations require a GOT entry to be created, and
   handled as if a FUNCDESC relocation was applied to the GOT entry in
   an object file.

   FUNCDESC relocations referencing a symbol that turns out to NOT be
   dynamic cause a private function descriptor to be created.  The
   FUNCDESC relocation then decays to a 32 relocation that points at
   the private descriptor.  If the symbol is dynamic, the FUNCDESC
   relocation is propagated to the linker output, such that the
   dynamic linker creates the canonical descriptor, pointing to the
   dynamically-resolved definition of the function.

   Non-FUNCDESC GOTOFF relocations must always refer to non-dynamic
   symbols that are assigned to the same segment as the GOT, but we
   can only check this later, after we know the complete set of
   symbols defined and/or exported.

   FUNCDESC GOTOFF relocations require a function descriptor to be
   created and, unless lazy binding is disabled or the symbol is not
   dynamic, a lazy PLT entry.  Since we can't tell at this point
   whether a symbol is going to be dynamic, we have to decide later
   whether to create a lazy PLT entry or bind the descriptor directly
   to the private function.

   FUNCDESC_VALUE relocations are not supposed to be present in object
   files, but they may very well be simply propagated to the linker
   output, since they have no side effect.


   A function descriptor always requires a FUNCDESC_VALUE relocation.
   Whether it's in .plt.rel or not depends on whether lazy binding is
   enabled and on whether the referenced symbol is dynamic.

   The existence of a lazy PLT requires the resolverStub lazy PLT
   entry to be present.


   As for assignment of GOT, PLT and lazy PLT entries, and private
   descriptors, we might do them all sequentially, but we can do
   better than that.  For example, we can place GOT entries and
   private function descriptors referenced using 12-bit operands
   closer to the PIC register value, such that these relocations don't
   overflow.  Those that are only referenced with LO16 relocations
   could come next, but we may as well place PLT-required function
   descriptors in the 12-bit range to make them shorter.  Symbols
   referenced with LO16/HI16 may come next, but we may place
   additional function descriptors in the 16-bit range if we can
   reliably tell that we've already placed entries that are ever
   referenced with only LO16.  PLT entries are therefore generated as
   small as possible, while not introducing relocation overflows in
   GOT or FUNCDESC_GOTOFF relocations.  Lazy PLT entries could be
   generated before or after PLT entries, but not intermingled with
   them, such that we can have more lazy PLT entries in range for a
   branch to the resolverStub.  The resolverStub should be emitted at
   the most distant location from the first lazy PLT entry such that
   it's still in range for a branch, or closer, if there isn't a need
   for so many lazy PLT entries.  Additional lazy PLT entries may be
   emitted after the resolverStub, as long as branches are still in
   range.  If the branch goes out of range, longer lazy PLT entries
   are emitted.

   We could further optimize PLT and lazy PLT entries by giving them
   priority in assignment to closer-to-gr17 locations depending on the
   number of occurrences of references to them (assuming a function
   that's called more often is more important for performance, so its
   PLT entry should be faster), or taking hints from the compiler.
   Given infinite time and money... :-)  */

static bfd_boolean
elf32_frv_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  bfd *dynobj;
  struct frvfdpic_relocs_info *picrel;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof(Elf32_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  dynobj = elf_hash_table (info)->dynobj;
  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
        h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_FRV_GOT12:
	case R_FRV_GOTHI:
	case R_FRV_GOTLO:
	case R_FRV_FUNCDESC_GOT12:
	case R_FRV_FUNCDESC_GOTHI:
	case R_FRV_FUNCDESC_GOTLO:
	case R_FRV_GOTOFF12:
	case R_FRV_GOTOFFHI:
	case R_FRV_GOTOFFLO:
	case R_FRV_FUNCDESC_GOTOFF12:
	case R_FRV_FUNCDESC_GOTOFFHI:
	case R_FRV_FUNCDESC_GOTOFFLO:
	case R_FRV_FUNCDESC:
	case R_FRV_FUNCDESC_VALUE:
	  if (! IS_FDPIC (abfd))
	    goto bad_reloc;
	  /* Fall through.  */
	case R_FRV_GPREL12:
	case R_FRV_GPRELU12:
	case R_FRV_GPRELHI:
	case R_FRV_GPRELLO:
	case R_FRV_LABEL24:
	case R_FRV_32:
	  if (! dynobj)
	    {
	      elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (! _frv_create_got_section (abfd, info))
		return FALSE;
	    }
	  if (! IS_FDPIC (abfd))
	    {
	      picrel = NULL;
	      break;
	    }
	  if (h != NULL)
	    {
	      if (h->dynindx == -1)
		switch (ELF_ST_VISIBILITY (h->other))
		  {
		  case STV_INTERNAL:
		  case STV_HIDDEN:
		    break;
		  default:
		    bfd_elf_link_record_dynamic_symbol (info, h);
		    break;
		  }
	      picrel
		= frvfdpic_relocs_info_for_global (frvfdpic_relocs_info (info),
						   abfd, h,
						   rel->r_addend, INSERT);
	    }
	  else
	    picrel = frvfdpic_relocs_info_for_local (frvfdpic_relocs_info
						     (info), abfd, r_symndx,
						     rel->r_addend, INSERT);
	  if (! picrel)
	    return FALSE;
	  break;

	default:
	  picrel = NULL;
	  break;
	}
      
      switch (ELF32_R_TYPE (rel->r_info))
        {
	case R_FRV_LABEL24:
	  if (IS_FDPIC (abfd))
	    picrel->call = 1;
	  break;
		
	case R_FRV_FUNCDESC_VALUE:
	  picrel->relocsfdv++;
	  if (bfd_get_section_flags (abfd, sec) & SEC_ALLOC)
	    picrel->relocs32--;
	  /* Fall through.  */

	case R_FRV_32:
	  if (! IS_FDPIC (abfd))
	    break;

	  picrel->sym = 1;
	  if (bfd_get_section_flags (abfd, sec) & SEC_ALLOC)
	    picrel->relocs32++;
	  break;
	    
	case R_FRV_GOT12:
	  picrel->got12 = 1;
	  break;
	    
	case R_FRV_GOTHI:
	case R_FRV_GOTLO:
	  picrel->gothilo = 1;
	  break;

	case R_FRV_FUNCDESC_GOT12:
	  picrel->fdgot12 = 1;
	  break;
	    
	case R_FRV_FUNCDESC_GOTHI:
	case R_FRV_FUNCDESC_GOTLO:
	  picrel->fdgothilo = 1;
	  break;
	    
	case R_FRV_GOTOFF12:
	case R_FRV_GOTOFFHI:
	case R_FRV_GOTOFFLO:
	  picrel->gotoff = 1;
	  break;
	    
	case R_FRV_FUNCDESC_GOTOFF12:
	  picrel->fdgoff12 = 1;
	  break;
	    
	case R_FRV_FUNCDESC_GOTOFFHI:
	case R_FRV_FUNCDESC_GOTOFFLO:
	  picrel->fdgoffhilo = 1;
	  break;
	    
	case R_FRV_FUNCDESC:
	  picrel->fd = 1;
	  picrel->relocsfd++;
	  break;
	  
        /* This relocation describes the C++ object vtable hierarchy.
           Reconstruct it for later use during GC.  */
        case R_FRV_GNU_VTINHERIT:
          if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

        /* This relocation describes which C++ vtable entries are actually
           used.  Record for later use during GC.  */
        case R_FRV_GNU_VTENTRY:
          if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return FALSE;
          break;

	case R_FRV_LABEL16:
	case R_FRV_LO16:
	case R_FRV_HI16:
	case R_FRV_GPREL12:
	case R_FRV_GPRELU12:
	case R_FRV_GPREL32:
	case R_FRV_GPRELHI:
	case R_FRV_GPRELLO:
	  break;

	default:
	bad_reloc:
	  (*_bfd_error_handler)
	    (_("%s: unsupported relocation type %i"),
	     bfd_archive_filename (abfd), ELF32_R_TYPE (rel->r_info));
	  return FALSE;
        }
    }

  return TRUE;
}


/* Return the machine subcode from the ELF e_flags header.  */

static int
elf32_frv_machine (abfd)
     bfd *abfd;
{
  switch (elf_elfheader (abfd)->e_flags & EF_FRV_CPU_MASK)
    {
    default:		    break;
    case EF_FRV_CPU_FR550:  return bfd_mach_fr550;
    case EF_FRV_CPU_FR500:  return bfd_mach_fr500;
    case EF_FRV_CPU_FR400:  return bfd_mach_fr400;
    case EF_FRV_CPU_FR300:  return bfd_mach_fr300;
    case EF_FRV_CPU_SIMPLE: return bfd_mach_frvsimple;
    case EF_FRV_CPU_TOMCAT: return bfd_mach_frvtomcat;
    }

  return bfd_mach_frv;
}

/* Set the right machine number for a FRV ELF file.  */

static bfd_boolean
elf32_frv_object_p (abfd)
     bfd *abfd;
{
  bfd_default_set_arch_mach (abfd, bfd_arch_frv, elf32_frv_machine (abfd));
  return (((elf_elfheader (abfd)->e_flags & EF_FRV_FDPIC) != 0)
	  == (IS_FDPIC (abfd)));
}

/* Function to set the ELF flag bits.  */

static bfd_boolean
frv_elf_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

/* Copy backend specific data from one object module to another.  */

static bfd_boolean
frv_elf_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  BFD_ASSERT (!elf_flags_init (obfd)
	      || elf_elfheader (obfd)->e_flags == elf_elfheader (ibfd)->e_flags);

  elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;
  elf_flags_init (obfd) = TRUE;
  return TRUE;
}

static bfd_boolean
elf32_frvfdpic_copy_private_bfd_data (bfd *ibfd, bfd *obfd)
{
  unsigned i;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  if (! frv_elf_copy_private_bfd_data (ibfd, obfd))
    return FALSE;

  if (! elf_tdata (ibfd) || ! elf_tdata (ibfd)->phdr
      || ! elf_tdata (obfd) || ! elf_tdata (obfd)->phdr)
    return TRUE;

  /* Copy the stack size.  */
  for (i = 0; i < elf_elfheader (ibfd)->e_phnum; i++)
    if (elf_tdata (ibfd)->phdr[i].p_type == PT_GNU_STACK)
      {
	Elf_Internal_Phdr *iphdr = &elf_tdata (ibfd)->phdr[i];

	for (i = 0; i < elf_elfheader (obfd)->e_phnum; i++)
	  if (elf_tdata (obfd)->phdr[i].p_type == PT_GNU_STACK)
	    {
	      memcpy (&elf_tdata (obfd)->phdr[i], iphdr, sizeof (*iphdr));

	      /* Rewrite the phdrs, since we're only called after they
		 were first written.  */
	      if (bfd_seek (obfd, (bfd_signed_vma) get_elf_backend_data (obfd)
			    ->s->sizeof_ehdr, SEEK_SET) != 0
		  || get_elf_backend_data (obfd)->s
		  ->write_out_phdrs (obfd, elf_tdata (obfd)->phdr,
				     elf_elfheader (obfd)->e_phnum) != 0)
		return FALSE;
	      break;
	    }

	break;
      }

  return TRUE;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static bfd_boolean
frv_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword old_flags, old_partial;
  flagword new_flags, new_partial;
  bfd_boolean error = FALSE;
  char new_opt[80];
  char old_opt[80];

  new_opt[0] = old_opt[0] = '\0';
  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;

  if (new_flags & EF_FRV_FDPIC)
    new_flags &= ~EF_FRV_PIC;

#ifdef DEBUG
  (*_bfd_error_handler) ("old_flags = 0x%.8lx, new_flags = 0x%.8lx, init = %s, filename = %s",
			 old_flags, new_flags, elf_flags_init (obfd) ? "yes" : "no",
			 bfd_get_filename (ibfd));
#endif

  if (!elf_flags_init (obfd))			/* First call, no flags set.  */
    {
      elf_flags_init (obfd) = TRUE;
      old_flags = new_flags;
    }

  else if (new_flags == old_flags)		/* Compatible flags are ok.  */
    ;

  else						/* Possibly incompatible flags.  */
    {
      /* Warn if different # of gprs are used.  Note, 0 means nothing is
         said about the size of gprs.  */
      new_partial = (new_flags & EF_FRV_GPR_MASK);
      old_partial = (old_flags & EF_FRV_GPR_MASK);
      if (new_partial == old_partial)
	;

      else if (new_partial == 0)
	;

      else if (old_partial == 0)
	old_flags |= new_partial;

      else
	{
	  switch (new_partial)
	    {
	    default:		strcat (new_opt, " -mgpr-??"); break;
	    case EF_FRV_GPR_32: strcat (new_opt, " -mgpr-32"); break;
	    case EF_FRV_GPR_64: strcat (new_opt, " -mgpr-64"); break;
	    }

	  switch (old_partial)
	    {
	    default:		strcat (old_opt, " -mgpr-??"); break;
	    case EF_FRV_GPR_32: strcat (old_opt, " -mgpr-32"); break;
	    case EF_FRV_GPR_64: strcat (old_opt, " -mgpr-64"); break;
	    }
	}

      /* Warn if different # of fprs are used.  Note, 0 means nothing is
         said about the size of fprs.  */
      new_partial = (new_flags & EF_FRV_FPR_MASK);
      old_partial = (old_flags & EF_FRV_FPR_MASK);
      if (new_partial == old_partial)
	;

      else if (new_partial == 0)
	;

      else if (old_partial == 0)
	old_flags |= new_partial;

      else
	{
	  switch (new_partial)
	    {
	    default:		  strcat (new_opt, " -mfpr-?");      break;
	    case EF_FRV_FPR_32:   strcat (new_opt, " -mfpr-32");     break;
	    case EF_FRV_FPR_64:   strcat (new_opt, " -mfpr-64");     break;
	    case EF_FRV_FPR_NONE: strcat (new_opt, " -msoft-float"); break;
	    }

	  switch (old_partial)
	    {
	    default:		  strcat (old_opt, " -mfpr-?");      break;
	    case EF_FRV_FPR_32:   strcat (old_opt, " -mfpr-32");     break;
	    case EF_FRV_FPR_64:   strcat (old_opt, " -mfpr-64");     break;
	    case EF_FRV_FPR_NONE: strcat (old_opt, " -msoft-float"); break;
	    }
	}

      /* Warn if different dword support was used.  Note, 0 means nothing is
         said about the dword support.  */
      new_partial = (new_flags & EF_FRV_DWORD_MASK);
      old_partial = (old_flags & EF_FRV_DWORD_MASK);
      if (new_partial == old_partial)
	;

      else if (new_partial == 0)
	;

      else if (old_partial == 0)
	old_flags |= new_partial;

      else
	{
	  switch (new_partial)
	    {
	    default:		   strcat (new_opt, " -mdword-?");  break;
	    case EF_FRV_DWORD_YES: strcat (new_opt, " -mdword");    break;
	    case EF_FRV_DWORD_NO:  strcat (new_opt, " -mno-dword"); break;
	    }

	  switch (old_partial)
	    {
	    default:		   strcat (old_opt, " -mdword-?");  break;
	    case EF_FRV_DWORD_YES: strcat (old_opt, " -mdword");    break;
	    case EF_FRV_DWORD_NO:  strcat (old_opt, " -mno-dword"); break;
	    }
	}

      /* Or in flags that accumulate (ie, if one module uses it, mark that the
	 feature is used.  */
      old_flags |= new_flags & (EF_FRV_DOUBLE
				| EF_FRV_MEDIA
				| EF_FRV_MULADD
				| EF_FRV_NON_PIC_RELOCS);

      /* If any module was compiled without -G0, clear the G0 bit.  */
      old_flags = ((old_flags & ~ EF_FRV_G0)
		   | (old_flags & new_flags & EF_FRV_G0));

      /* If any module was compiled without -mnopack, clear the mnopack bit.  */
      old_flags = ((old_flags & ~ EF_FRV_NOPACK)
		   | (old_flags & new_flags & EF_FRV_NOPACK));

      /* We don't have to do anything if the pic flags are the same, or the new
         module(s) were compiled with -mlibrary-pic.  */
      new_partial = (new_flags & EF_FRV_PIC_FLAGS);
      old_partial = (old_flags & EF_FRV_PIC_FLAGS);
      if ((new_partial == old_partial) || ((new_partial & EF_FRV_LIBPIC) != 0))
	;

      /* If the old module(s) were compiled with -mlibrary-pic, copy in the pic
         flags if any from the new module.  */
      else if ((old_partial & EF_FRV_LIBPIC) != 0)
	old_flags = (old_flags & ~ EF_FRV_PIC_FLAGS) | new_partial;

      /* If we have mixtures of -fpic and -fPIC, or in both bits.  */
      else if (new_partial != 0 && old_partial != 0)
	old_flags |= new_partial;

      /* One module was compiled for pic and the other was not, see if we have
         had any relocations that are not pic-safe.  */
      else
	{
	  if ((old_flags & EF_FRV_NON_PIC_RELOCS) == 0)
	    old_flags |= new_partial;
	  else
	    {
	      old_flags &= ~ EF_FRV_PIC_FLAGS;
#ifndef FRV_NO_PIC_ERROR
	      error = TRUE;
	      (*_bfd_error_handler)
		(_("%s: compiled with %s and linked with modules that use non-pic relocations"),
		 bfd_get_filename (ibfd),
		 (new_flags & EF_FRV_BIGPIC) ? "-fPIC" : "-fpic");
#endif
	    }
	}

      /* Warn if different cpu is used (allow a specific cpu to override
	 the generic cpu).  */
      new_partial = (new_flags & EF_FRV_CPU_MASK);
      old_partial = (old_flags & EF_FRV_CPU_MASK);
      if (new_partial == old_partial)
	;

      else if (new_partial == EF_FRV_CPU_GENERIC)
	;

      else if (old_partial == EF_FRV_CPU_GENERIC)
	old_flags = (old_flags & ~EF_FRV_CPU_MASK) | new_partial;

      else
	{
	  switch (new_partial)
	    {
	    default:		     strcat (new_opt, " -mcpu=?");      break;
	    case EF_FRV_CPU_GENERIC: strcat (new_opt, " -mcpu=frv");    break;
	    case EF_FRV_CPU_SIMPLE:  strcat (new_opt, " -mcpu=simple"); break;
	    case EF_FRV_CPU_FR550:   strcat (new_opt, " -mcpu=fr550");  break;
	    case EF_FRV_CPU_FR500:   strcat (new_opt, " -mcpu=fr500");  break;
	    case EF_FRV_CPU_FR400:   strcat (new_opt, " -mcpu=fr400");  break;
	    case EF_FRV_CPU_FR300:   strcat (new_opt, " -mcpu=fr300");  break;
	    case EF_FRV_CPU_TOMCAT:  strcat (new_opt, " -mcpu=tomcat"); break;
	    }

	  switch (old_partial)
	    {
	    default:		     strcat (old_opt, " -mcpu=?");      break;
	    case EF_FRV_CPU_GENERIC: strcat (old_opt, " -mcpu=frv");    break;
	    case EF_FRV_CPU_SIMPLE:  strcat (old_opt, " -mcpu=simple"); break;
	    case EF_FRV_CPU_FR550:   strcat (old_opt, " -mcpu=fr550");  break;
	    case EF_FRV_CPU_FR500:   strcat (old_opt, " -mcpu=fr500");  break;
	    case EF_FRV_CPU_FR400:   strcat (old_opt, " -mcpu=fr400");  break;
	    case EF_FRV_CPU_FR300:   strcat (old_opt, " -mcpu=fr300");  break;
	    case EF_FRV_CPU_TOMCAT:  strcat (old_opt, " -mcpu=tomcat"); break;
	    }
	}

      /* Print out any mismatches from above.  */
      if (new_opt[0])
	{
	  error = TRUE;
	  (*_bfd_error_handler)
	    (_("%s: compiled with %s and linked with modules compiled with %s"),
	     bfd_get_filename (ibfd), new_opt, old_opt);
	}

      /* Warn about any other mismatches */
      new_partial = (new_flags & ~ EF_FRV_ALL_FLAGS);
      old_partial = (old_flags & ~ EF_FRV_ALL_FLAGS);
      if (new_partial != old_partial)
	{
	  old_flags |= new_partial;
	  error = TRUE;
	  (*_bfd_error_handler)
	    (_("%s: uses different unknown e_flags (0x%lx) fields than previous modules (0x%lx)"),
	     bfd_get_filename (ibfd), (long)new_partial, (long)old_partial);
	}
    }

  /* If the cpu is -mcpu=simple, then set the -mnopack bit.  */
  if ((old_flags & EF_FRV_CPU_MASK) == EF_FRV_CPU_SIMPLE)
    old_flags |= EF_FRV_NOPACK;

  /* Update the old flags now with changes made above.  */
  old_partial = elf_elfheader (obfd)->e_flags & EF_FRV_CPU_MASK;
  elf_elfheader (obfd)->e_flags = old_flags;
  if (old_partial != (old_flags & EF_FRV_CPU_MASK))
    bfd_default_set_arch_mach (obfd, bfd_arch_frv, elf32_frv_machine (obfd));

  if (((new_flags & EF_FRV_FDPIC) == 0)
      != (! IS_FDPIC (ibfd)))
    {
      error = TRUE;
      if (IS_FDPIC (obfd))
	(*_bfd_error_handler)
	  (_("%s: cannot link non-fdpic object file into fdpic executable"),
	   bfd_get_filename (ibfd));
      else
	(*_bfd_error_handler)
	  (_("%s: cannot link fdpic object file into non-fdpic executable"),
	   bfd_get_filename (ibfd));
    }

  if (error)
    bfd_set_error (bfd_error_bad_value);

  return !error;
}


bfd_boolean
frv_elf_print_private_bfd_data (abfd, ptr)
     bfd *abfd;
     PTR ptr;
{
  FILE *file = (FILE *) ptr;
  flagword flags;

  BFD_ASSERT (abfd != NULL && ptr != NULL);

  /* Print normal ELF private data.  */
  _bfd_elf_print_private_bfd_data (abfd, ptr);

  flags = elf_elfheader (abfd)->e_flags;
  fprintf (file, _("private flags = 0x%lx:"), (long)flags);

  switch (flags & EF_FRV_CPU_MASK)
    {
    default:							break;
    case EF_FRV_CPU_SIMPLE: fprintf (file, " -mcpu=simple");	break;
    case EF_FRV_CPU_FR550:  fprintf (file, " -mcpu=fr550");	break;
    case EF_FRV_CPU_FR500:  fprintf (file, " -mcpu=fr500");	break;
    case EF_FRV_CPU_FR400:  fprintf (file, " -mcpu=fr400");	break;
    case EF_FRV_CPU_FR300:  fprintf (file, " -mcpu=fr300");	break;
    case EF_FRV_CPU_TOMCAT: fprintf (file, " -mcpu=tomcat");	break;
    }

  switch (flags & EF_FRV_GPR_MASK)
    {
    default:							break;
    case EF_FRV_GPR_32: fprintf (file, " -mgpr-32");		break;
    case EF_FRV_GPR_64: fprintf (file, " -mgpr-64");		break;
    }

  switch (flags & EF_FRV_FPR_MASK)
    {
    default:							break;
    case EF_FRV_FPR_32:   fprintf (file, " -mfpr-32");		break;
    case EF_FRV_FPR_64:   fprintf (file, " -mfpr-64");		break;
    case EF_FRV_FPR_NONE: fprintf (file, " -msoft-float");	break;
    }

  switch (flags & EF_FRV_DWORD_MASK)
    {
    default:							break;
    case EF_FRV_DWORD_YES: fprintf (file, " -mdword");		break;
    case EF_FRV_DWORD_NO:  fprintf (file, " -mno-dword");	break;
    }

  if (flags & EF_FRV_DOUBLE)
    fprintf (file, " -mdouble");

  if (flags & EF_FRV_MEDIA)
    fprintf (file, " -mmedia");

  if (flags & EF_FRV_MULADD)
    fprintf (file, " -mmuladd");

  if (flags & EF_FRV_PIC)
    fprintf (file, " -fpic");

  if (flags & EF_FRV_BIGPIC)
    fprintf (file, " -fPIC");

  if (flags & EF_FRV_LIBPIC)
    fprintf (file, " -mlibrary-pic");

  if (flags & EF_FRV_FDPIC)
    fprintf (file, " -mfdpic");
  
  if (flags & EF_FRV_NON_PIC_RELOCS)
    fprintf (file, " non-pic relocations");

  if (flags & EF_FRV_G0)
    fprintf (file, " -G0");

  fputc ('\n', file);
  return TRUE;
}


#define ELF_ARCH		bfd_arch_frv
#define ELF_MACHINE_CODE	EM_CYGNUS_FRV
#define ELF_MAXPAGESIZE		0x1000

#define TARGET_BIG_SYM          bfd_elf32_frv_vec
#define TARGET_BIG_NAME		"elf32-frv"

#define elf_info_to_howto			frv_info_to_howto_rela
#define elf_backend_relocate_section		elf32_frv_relocate_section
#define elf_backend_gc_mark_hook		elf32_frv_gc_mark_hook
#define elf_backend_gc_sweep_hook		elf32_frv_gc_sweep_hook
#define elf_backend_check_relocs                elf32_frv_check_relocs
#define elf_backend_object_p			elf32_frv_object_p
#define elf_backend_add_symbol_hook             elf32_frv_add_symbol_hook

#define elf_backend_can_gc_sections		1
#define elf_backend_rela_normal			1

#define bfd_elf32_bfd_reloc_type_lookup		frv_reloc_type_lookup
#define bfd_elf32_bfd_set_private_flags		frv_elf_set_private_flags
#define bfd_elf32_bfd_copy_private_bfd_data	frv_elf_copy_private_bfd_data
#define bfd_elf32_bfd_merge_private_bfd_data	frv_elf_merge_private_bfd_data
#define bfd_elf32_bfd_print_private_bfd_data	frv_elf_print_private_bfd_data

#define elf_backend_want_got_sym	1
#define elf_backend_got_header_size	0
#define elf_backend_want_got_plt	0
#define elf_backend_plt_readonly	1
#define elf_backend_want_plt_sym	0
#define elf_backend_plt_header_size	0

#define elf_backend_finish_dynamic_sections \
		elf32_frv_finish_dynamic_sections

#include "elf32-target.h"

#undef ELF_MAXPAGESIZE
#define ELF_MAXPAGESIZE		0x4000

#undef TARGET_BIG_SYM
#define TARGET_BIG_SYM          bfd_elf32_frvfdpic_vec
#undef TARGET_BIG_NAME
#define TARGET_BIG_NAME		"elf32-frvfdpic"
#undef	elf32_bed
#define	elf32_bed		elf32_frvfdpic_bed

#undef elf_info_to_howto_rel
#define elf_info_to_howto_rel	frvfdpic_info_to_howto_rel

#undef bfd_elf32_bfd_link_hash_table_create
#define bfd_elf32_bfd_link_hash_table_create \
		frvfdpic_elf_link_hash_table_create
#undef elf_backend_always_size_sections
#define elf_backend_always_size_sections \
		elf32_frvfdpic_always_size_sections
#undef elf_backend_modify_segment_map
#define elf_backend_modify_segment_map \
		elf32_frvfdpic_modify_segment_map
#undef bfd_elf32_bfd_copy_private_bfd_data
#define bfd_elf32_bfd_copy_private_bfd_data \
		elf32_frvfdpic_copy_private_bfd_data

#undef elf_backend_create_dynamic_sections
#define elf_backend_create_dynamic_sections \
		elf32_frvfdpic_create_dynamic_sections
#undef elf_backend_adjust_dynamic_symbol
#define elf_backend_adjust_dynamic_symbol \
		elf32_frvfdpic_adjust_dynamic_symbol
#undef elf_backend_size_dynamic_sections
#define elf_backend_size_dynamic_sections \
		elf32_frvfdpic_size_dynamic_sections
#undef elf_backend_finish_dynamic_symbol
#define elf_backend_finish_dynamic_symbol \
		elf32_frvfdpic_finish_dynamic_symbol
#undef elf_backend_finish_dynamic_sections
#define elf_backend_finish_dynamic_sections \
		elf32_frvfdpic_finish_dynamic_sections

#undef elf_backend_can_make_relative_eh_frame
#define elf_backend_can_make_relative_eh_frame \
		frvfdpic_elf_use_relative_eh_frame
#undef elf_backend_can_make_lsda_relative_eh_frame
#define elf_backend_can_make_lsda_relative_eh_frame \
		frvfdpic_elf_use_relative_eh_frame
#undef elf_backend_encode_eh_address
#define elf_backend_encode_eh_address \
		frvfdpic_elf_encode_eh_address

#undef elf_backend_may_use_rel_p
#define elf_backend_may_use_rel_p       1
#undef elf_backend_may_use_rela_p
#define elf_backend_may_use_rela_p      1
/* We use REL for dynamic relocations only.  */
#undef elf_backend_default_use_rela_p
#define elf_backend_default_use_rela_p  1

#include "elf32-target.h"
