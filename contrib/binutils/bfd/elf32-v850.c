/* V850-specific support for 32-bit ELF
   Copyright (C) 1996, 1997, 1998, 1999 Free Software Foundation, Inc.

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

/* XXX FIXME: This code is littered with 32bit int, 16bit short, 8bit char
   dependencies.  As is the gas & simulator code or the v850.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/v850.h"

/* sign-extend a 24-bit number */
#define SEXT24(x)	((((x) & 0xffffff) ^ (~ 0x7fffff)) + 0x800000)

static reloc_howto_type *v850_elf_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void v850_elf_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rel *));
static void v850_elf_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rela *));
static bfd_reloc_status_type v850_elf_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static boolean v850_elf_is_local_label_name
  PARAMS ((bfd *, const char *));
static boolean v850_elf_relocate_section
  PARAMS((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	  Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static bfd_reloc_status_type v850_elf_perform_relocation
  PARAMS ((bfd *, int, bfd_vma, bfd_byte *));
static boolean v850_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *, const Elf_Internal_Rela *));
static void remember_hi16s_reloc
  PARAMS ((bfd *, bfd_vma, bfd_byte *));
static bfd_byte * find_remembered_hi16s_reloc
  PARAMS ((bfd_vma, boolean *));
static bfd_reloc_status_type v850_elf_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, bfd *, asection *, bfd_byte *, bfd_vma,
	   bfd_vma, bfd_vma, struct bfd_link_info *, asection *, int));
static boolean v850_elf_object_p
  PARAMS ((bfd *));
static boolean v850_elf_fake_sections
  PARAMS ((bfd *, Elf32_Internal_Shdr *, asection *));
static void v850_elf_final_write_processing
  PARAMS ((bfd *, boolean));
static boolean v850_elf_set_private_flags
  PARAMS ((bfd *, flagword));
static boolean v850_elf_copy_private_bfd_data
  PARAMS ((bfd *, bfd *));
static boolean v850_elf_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static boolean v850_elf_print_private_bfd_data
  PARAMS ((bfd *, PTR));
static boolean v850_elf_section_from_bfd_section
  PARAMS ((bfd *, Elf32_Internal_Shdr *, asection *, int *));
static void v850_elf_symbol_processing
  PARAMS ((bfd *, asymbol *));
static boolean v850_elf_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));
static boolean v850_elf_link_output_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const char *,
	   Elf_Internal_Sym *, asection *));
static boolean v850_elf_section_from_shdr
  PARAMS ((bfd *, Elf_Internal_Shdr *, char *));

/* Note: It is REQUIRED that the 'type' value of each entry in this array
   match the index of the entry in the array.  */
static reloc_howto_type v850_elf_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_V850_NONE,			/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,		/* special_function */
	 "R_V850_NONE",			/* name */
	 false,				/* partial_inplace */
	 0,				/* src_mask */
	 0,				/* dst_mask */
	 false),			/* pcrel_offset */

  /* A PC relative 9 bit branch.  */
  HOWTO (R_V850_9_PCREL,		/* type */
	 2,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 26,				/* bitsize */
	 true,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_9_PCREL",		/* name */
	 false,				/* partial_inplace */
	 0x00ffffff,			/* src_mask */
	 0x00ffffff,			/* dst_mask */
	 true),				/* pcrel_offset */

  /* A PC relative 22 bit branch.  */
  HOWTO (R_V850_22_PCREL,		/* type */
	 2,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 22,				/* bitsize */
	 true,				/* pc_relative */
	 7,				/* bitpos */
	 complain_overflow_signed,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_22_PCREL",		/* name */
	 false,				/* partial_inplace */
	 0x07ffff80,			/* src_mask */
	 0x07ffff80,			/* dst_mask */
	 true),				/* pcrel_offset */

  /* High 16 bits of symbol value.  */
  HOWTO (R_V850_HI16_S,			/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_HI16_S",		/* name */
	 false,				/* partial_inplace */
	 0xffff,			/* src_mask */
	 0xffff,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* High 16 bits of symbol value.  */
  HOWTO (R_V850_HI16,			/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_HI16",			/* name */
	 false,				/* partial_inplace */
	 0xffff,			/* src_mask */
	 0xffff,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_V850_LO16,			/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_LO16",			/* name */
	 false,				/* partial_inplace */
	 0xffff,			/* src_mask */
	 0xffff,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* Simple 32bit reloc.  */
  HOWTO (R_V850_32,			/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_32",			/* name */
	 false,				/* partial_inplace */
	 0xffffffff,			/* src_mask */
	 0xffffffff,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* Simple 16bit reloc.  */
  HOWTO (R_V850_16,			/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,		/* special_function */
	 "R_V850_16",			/* name */
	 false,				/* partial_inplace */
	 0xffff,			/* src_mask */
	 0xffff,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* Simple 8bit reloc.	 */
  HOWTO (R_V850_8,			/* type */
	 0,				/* rightshift */
	 0,				/* size (0 = byte, 1 = short, 2 = long) */
	 8,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,		/* special_function */
	 "R_V850_8",			/* name */
	 false,				/* partial_inplace */
	 0xff,				/* src_mask */
	 0xff,				/* dst_mask */
	 false),			/* pcrel_offset */

  /* 16 bit offset from the short data area pointer.  */
  HOWTO (R_V850_SDA_16_16_OFFSET,	/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_SDA_16_16_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0xffff,			/* src_mask */
	 0xffff,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* 15 bit offset from the short data area pointer.  */
  HOWTO (R_V850_SDA_15_16_OFFSET,	/* type */
	 1,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 1,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_SDA_15_16_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0xfffe,			/* src_mask */
	 0xfffe,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* 16 bit offset from the zero data area pointer.  */
  HOWTO (R_V850_ZDA_16_16_OFFSET,	/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_ZDA_16_16_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0xffff,			/* src_mask */
	 0xffff,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* 15 bit offset from the zero data area pointer.  */
  HOWTO (R_V850_ZDA_15_16_OFFSET,	/* type */
	 1,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 1,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_ZDA_15_16_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0xfffe,			/* src_mask */
	 0xfffe,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* 6 bit offset from the tiny data area pointer.  */
  HOWTO (R_V850_TDA_6_8_OFFSET,		/* type */
	 2,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 8,				/* bitsize */
	 false,				/* pc_relative */
	 1,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_TDA_6_8_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0x7e,				/* src_mask */
	 0x7e,				/* dst_mask */
	 false),			/* pcrel_offset */

  /* 8 bit offset from the tiny data area pointer.  */
  HOWTO (R_V850_TDA_7_8_OFFSET,		/* type */
	 1,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 8,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_TDA_7_8_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0x7f,				/* src_mask */
	 0x7f,				/* dst_mask */
	 false),			/* pcrel_offset */

  /* 7 bit offset from the tiny data area pointer.  */
  HOWTO (R_V850_TDA_7_7_OFFSET,		/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 7,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_TDA_7_7_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0x7f,				/* src_mask */
	 0x7f,				/* dst_mask */
	 false),			/* pcrel_offset */

  /* 16 bit offset from the tiny data area pointer!  */
  HOWTO (R_V850_TDA_16_16_OFFSET,	/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_TDA_16_16_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0xffff,			/* src_mask */
	 0xfff,				/* dst_mask */
	 false),			/* pcrel_offset */

  /* 5 bit offset from the tiny data area pointer.  */
  HOWTO (R_V850_TDA_4_5_OFFSET,		/* type */
	 1,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 5,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_TDA_4_5_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0x0f,				/* src_mask */
	 0x0f,				/* dst_mask */
	 false),			/* pcrel_offset */

  /* 4 bit offset from the tiny data area pointer.  */
  HOWTO (R_V850_TDA_4_4_OFFSET,		/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 4,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_TDA_4_4_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0x0f,				/* src_mask */
	 0x0f,				/* dst_mask */
	 false),			/* pcrel_offset */

  /* 16 bit offset from the short data area pointer.  */
  HOWTO (R_V850_SDA_16_16_SPLIT_OFFSET,	/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_SDA_16_16_SPLIT_OFFSET",/* name */
	 false,				/* partial_inplace */
	 0xfffe0020,			/* src_mask */
	 0xfffe0020,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* 16 bit offset from the zero data area pointer.  */
  HOWTO (R_V850_ZDA_16_16_SPLIT_OFFSET,	/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_ZDA_16_16_SPLIT_OFFSET",/* name */
	 false,				/* partial_inplace */
	 0xfffe0020,			/* src_mask */
	 0xfffe0020,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* 6 bit offset from the call table base pointer.  */
  HOWTO (R_V850_CALLT_6_7_OFFSET,	/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 7,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_CALLT_6_7_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0x3f,				/* src_mask */
	 0x3f,				/* dst_mask */
	 false),			/* pcrel_offset */

  /* 16 bit offset from the call table base pointer.  */
  HOWTO (R_V850_CALLT_16_16_OFFSET,	/* type */
	 0,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 v850_elf_reloc,		/* special_function */
	 "R_V850_CALLT_16_16_OFFSET",	/* name */
	 false,				/* partial_inplace */
	 0xffff,			/* src_mask */
	 0xffff,			/* dst_mask */
	 false),			/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_V850_GNU_VTINHERIT, /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         false,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_V850_GNU_VTINHERIT", /* name */
         false,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         false),                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_V850_GNU_VTENTRY,     /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         false,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_V850_GNU_VTENTRY",   /* name */
         false,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         false),                /* pcrel_offset */

};

/* Map BFD reloc types to V850 ELF reloc types.  */

struct v850_elf_reloc_map
{
  /* BFD_RELOC_V850_CALLT_16_16_OFFSET is 258, which will not fix in an
     unsigned char.  */
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct v850_elf_reloc_map v850_elf_reloc_map[] =
{
  { BFD_RELOC_NONE,		R_V850_NONE },
  { BFD_RELOC_V850_9_PCREL,	R_V850_9_PCREL },
  { BFD_RELOC_V850_22_PCREL,	R_V850_22_PCREL },
  { BFD_RELOC_HI16_S,		R_V850_HI16_S },
  { BFD_RELOC_HI16,		R_V850_HI16 },
  { BFD_RELOC_LO16,		R_V850_LO16 },
  { BFD_RELOC_32,		R_V850_32 },
  { BFD_RELOC_16,		R_V850_16 },
  { BFD_RELOC_8,		R_V850_8 },
  { BFD_RELOC_V850_SDA_16_16_OFFSET, R_V850_SDA_16_16_OFFSET },
  { BFD_RELOC_V850_SDA_15_16_OFFSET, R_V850_SDA_15_16_OFFSET },
  { BFD_RELOC_V850_ZDA_16_16_OFFSET, R_V850_ZDA_16_16_OFFSET },
  { BFD_RELOC_V850_ZDA_15_16_OFFSET, R_V850_ZDA_15_16_OFFSET },
  { BFD_RELOC_V850_TDA_6_8_OFFSET,   R_V850_TDA_6_8_OFFSET   },
  { BFD_RELOC_V850_TDA_7_8_OFFSET,   R_V850_TDA_7_8_OFFSET   },
  { BFD_RELOC_V850_TDA_7_7_OFFSET,   R_V850_TDA_7_7_OFFSET   },
  { BFD_RELOC_V850_TDA_16_16_OFFSET, R_V850_TDA_16_16_OFFSET },
  { BFD_RELOC_V850_TDA_4_5_OFFSET,         R_V850_TDA_4_5_OFFSET         },
  { BFD_RELOC_V850_TDA_4_4_OFFSET,         R_V850_TDA_4_4_OFFSET         },
  { BFD_RELOC_V850_SDA_16_16_SPLIT_OFFSET, R_V850_SDA_16_16_SPLIT_OFFSET },
  { BFD_RELOC_V850_ZDA_16_16_SPLIT_OFFSET, R_V850_ZDA_16_16_SPLIT_OFFSET },
  { BFD_RELOC_V850_CALLT_6_7_OFFSET,       R_V850_CALLT_6_7_OFFSET       },
  { BFD_RELOC_V850_CALLT_16_16_OFFSET,     R_V850_CALLT_16_16_OFFSET     },
  { BFD_RELOC_VTABLE_INHERIT,               R_V850_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY,                 R_V850_GNU_VTENTRY },

};

/* Map a bfd relocation into the appropriate howto structure */
static reloc_howto_type *
v850_elf_reloc_type_lookup (abfd, code)
     bfd *                     abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type  code;
{
  unsigned int i;

  for (i = 0;
       i < sizeof (v850_elf_reloc_map) / sizeof (struct v850_elf_reloc_map);
       i++)
    {
      if (v850_elf_reloc_map[i].bfd_reloc_val == code)
	{
	  BFD_ASSERT (v850_elf_howto_table[v850_elf_reloc_map[i].elf_reloc_val].type == v850_elf_reloc_map[i].elf_reloc_val);

	  return & v850_elf_howto_table[v850_elf_reloc_map[i].elf_reloc_val];
	}
    }

  return NULL;
}

/* Set the howto pointer for an V850 ELF reloc.  */
static void
v850_elf_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *                 abfd ATTRIBUTE_UNUSED;
     arelent *             cache_ptr;
     Elf32_Internal_Rel *  dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_V850_max);
  cache_ptr->howto = &v850_elf_howto_table[r_type];
}

/* Set the howto pointer for a V850 ELF reloc (type RELA).  */
static void
v850_elf_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd *                 abfd ATTRIBUTE_UNUSED;
     arelent *             cache_ptr;
     Elf32_Internal_Rela   *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_V850_max);
  cache_ptr->howto = &v850_elf_howto_table[r_type];
}

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static boolean
v850_elf_check_relocs (abfd, info, sec, relocs)
     bfd *                      abfd;
     struct bfd_link_info *     info;
     asection *                 sec;
     const Elf_Internal_Rela *  relocs;
{
  boolean ret = true;
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sreloc;
  enum v850_reloc_type r_type;
  int other = 0;
  const char *common = (const char *)0;

  if (info->relocateable)
    return true;

#ifdef DEBUG
  fprintf (stderr, "v850_elf_check_relocs called for section %s in %s\n",
	   bfd_get_section_name (abfd, sec),
	   bfd_get_filename (abfd));
#endif

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sreloc = NULL;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      r_type = (enum v850_reloc_type) ELF32_R_TYPE (rel->r_info);
      switch (r_type)
	{
	default:
	case R_V850_NONE:
	case R_V850_9_PCREL:
	case R_V850_22_PCREL:
	case R_V850_HI16_S:
	case R_V850_HI16:
	case R_V850_LO16:
	case R_V850_32:
	case R_V850_16:
	case R_V850_8:
	case R_V850_CALLT_6_7_OFFSET:
	case R_V850_CALLT_16_16_OFFSET:
	  break;

        /* This relocation describes the C++ object vtable hierarchy.
           Reconstruct it for later use during GC.  */
        case R_V850_GNU_VTINHERIT:
          if (!_bfd_elf32_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return false;
          break;

        /* This relocation describes which C++ vtable entries are actually
           used.  Record for later use during GC.  */
        case R_V850_GNU_VTENTRY:
          if (!_bfd_elf32_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return false;
          break;

	case R_V850_SDA_16_16_SPLIT_OFFSET:
	case R_V850_SDA_16_16_OFFSET:
	case R_V850_SDA_15_16_OFFSET:
	  other = V850_OTHER_SDA;
	  common = ".scommon";
	  goto small_data_common;

	case R_V850_ZDA_16_16_SPLIT_OFFSET:
	case R_V850_ZDA_16_16_OFFSET:
	case R_V850_ZDA_15_16_OFFSET:
	  other = V850_OTHER_ZDA;
	  common = ".zcommon";
	  goto small_data_common;

	case R_V850_TDA_4_5_OFFSET:
	case R_V850_TDA_4_4_OFFSET:
	case R_V850_TDA_6_8_OFFSET:
	case R_V850_TDA_7_8_OFFSET:
	case R_V850_TDA_7_7_OFFSET:
	case R_V850_TDA_16_16_OFFSET:
	  other = V850_OTHER_TDA;
	  common = ".tcommon";
	  /* fall through */

#define V850_OTHER_MASK (V850_OTHER_TDA | V850_OTHER_SDA | V850_OTHER_ZDA)

	small_data_common:
	  if (h)
	    {
	      h->other |= other;	/* flag which type of relocation was used */
	      if ((h->other & V850_OTHER_MASK) != (other & V850_OTHER_MASK)
		  && (h->other & V850_OTHER_ERROR) == 0)
		{
		  const char * msg;
		  static char  buff[200]; /* XXX */

		  switch (h->other & V850_OTHER_MASK)
		    {
		    default:
		      msg = _("Variable `%s' cannot occupy in multiple small data regions");
		      break;
		    case V850_OTHER_SDA | V850_OTHER_ZDA | V850_OTHER_TDA:
		      msg = _("Variable `%s' can only be in one of the small, zero, and tiny data regions");
		      break;
		    case V850_OTHER_SDA | V850_OTHER_ZDA:
		      msg = _("Variable `%s' cannot be in both small and zero data regions simultaneously");
		      break;
		    case V850_OTHER_SDA | V850_OTHER_TDA:
		      msg = _("Variable `%s' cannot be in both small and tiny data regions simultaneously");
		      break;
		    case V850_OTHER_ZDA | V850_OTHER_TDA:
		      msg = _("Variable `%s' cannot be in both zero and tiny data regions simultaneously");
		      break;
		    }

		  sprintf (buff, msg, h->root.root.string);
		  info->callbacks->warning (info, buff, h->root.root.string,
					    abfd, h->root.u.def.section, 0);

		  bfd_set_error (bfd_error_bad_value);
		  h->other |= V850_OTHER_ERROR;
		  ret = false;
		}
	    }

	  if (h && h->root.type == bfd_link_hash_common
	      && h->root.u.c.p
	      && !strcmp (bfd_get_section_name (abfd, h->root.u.c.p->section), "COMMON"))
	    {
	      asection *section = h->root.u.c.p->section = bfd_make_section_old_way (abfd, common);
	      section->flags |= SEC_IS_COMMON;
	    }

#ifdef DEBUG
	  fprintf (stderr, "v850_elf_check_relocs, found %s relocation for %s%s\n",
		   v850_elf_howto_table[ (int)r_type ].name,
		   (h && h->root.root.string) ? h->root.root.string : "<unknown>",
		   (h->root.type == bfd_link_hash_common) ? ", symbol is common" : "");
#endif
	  break;
	}
    }

  return ret;
}

/*
 * In the old version, when an entry was checked out from the table,
 * it was deleted.  This produced an error if the entry was needed
 * more than once, as the second attempted retry failed.
 *
 * In the current version, the entry is not deleted, instead we set
 * the field 'found' to true.  If a second lookup matches the same
 * entry, then we know that the hi16s reloc has already been updated
 * and does not need to be updated a second time.
 *
 * TODO - TOFIX: If it is possible that we need to restore 2 different
 * addresses from the same table entry, where the first generates an
 * overflow, whilst the second do not, then this code will fail.
 */

typedef struct hi16s_location
{
  bfd_vma       addend;
  bfd_byte *    address;
  unsigned long counter;
  boolean       found;
  struct hi16s_location * next;
}
hi16s_location;

static hi16s_location *  previous_hi16s;
static hi16s_location *  free_hi16s;
static unsigned long     hi16s_counter;

static void
remember_hi16s_reloc (abfd, addend, address)
     bfd *      abfd;
     bfd_vma    addend;
     bfd_byte * address;
{
  hi16s_location * entry = NULL;

  /* Find a free structure.  */
  if (free_hi16s == NULL)
    free_hi16s = (hi16s_location *) bfd_zalloc (abfd, sizeof (* free_hi16s));

  entry      = free_hi16s;
  free_hi16s = free_hi16s->next;

  entry->addend  = addend;
  entry->address = address;
  entry->counter = hi16s_counter ++;
  entry->found   = false;
  entry->next    = previous_hi16s;
  previous_hi16s = entry;

  /* Cope with wrap around of our counter.  */
  if (hi16s_counter == 0)
    {
      /* XXX - Assume that all counter entries differ only in their low 16 bits.  */
      for (entry = previous_hi16s; entry != NULL; entry = entry->next)
	entry->counter &= 0xffff;

      hi16s_counter = 0x10000;
    }

  return;
}

static bfd_byte *
find_remembered_hi16s_reloc (addend, already_found)
     bfd_vma   addend;
     boolean * already_found;
{
  hi16s_location * match = NULL;
  hi16s_location * entry;
  hi16s_location * previous = NULL;
  hi16s_location * prev;
  bfd_byte *       addr;

  /* Search the table.  Record the most recent entry that matches.  */
  for (entry = previous_hi16s; entry; entry = entry->next)
    {
      if (entry->addend == addend
	  && (match == NULL || match->counter < entry->counter))
	{
	  previous = prev;
	  match    = entry;
	}

      prev = entry;
    }

  if (match == NULL)
    return NULL;

  /* Extract the address.  */
  addr = match->address;

  /* Remeber if this entry has already been used before.  */
  if (already_found)
    * already_found = match->found;

  /* Note that this entry has now been used.  */
  match->found = true;

  return addr;
}

/* FIXME:  The code here probably ought to be removed and the code in reloc.c
   allowed to do its  stuff instead.  At least for most of the relocs, anwyay.  */
static bfd_reloc_status_type
v850_elf_perform_relocation (abfd, r_type, addend, address)
     bfd *      abfd;
     int        r_type;
     bfd_vma    addend;
     bfd_byte * address;
{
  unsigned long insn;
  bfd_signed_vma saddend = (bfd_signed_vma) addend;

  switch (r_type)
    {
    default:
      /* fprintf (stderr, "reloc type %d not SUPPORTED\n", r_type ); */
      return bfd_reloc_notsupported;

    case R_V850_32:
      bfd_put_32 (abfd, addend, address);
      return bfd_reloc_ok;

    case R_V850_22_PCREL:
      if (saddend > 0x1fffff || saddend < -0x200000)
	return bfd_reloc_overflow;

      if ((addend % 2) != 0)
	return bfd_reloc_dangerous;

      insn  = bfd_get_32 (abfd, address);
      insn &= ~0xfffe003f;
      insn |= (((addend & 0xfffe) << 16) | ((addend & 0x3f0000) >> 16));
      bfd_put_32 (abfd, insn, address);
      return bfd_reloc_ok;

    case R_V850_9_PCREL:
      if (saddend > 0xff || saddend < -0x100)
	return bfd_reloc_overflow;

      if ((addend % 2) != 0)
	return bfd_reloc_dangerous;

      insn  = bfd_get_16 (abfd, address);
      insn &= ~ 0xf870;
      insn |= ((addend & 0x1f0) << 7) | ((addend & 0x0e) << 3);
      break;

    case R_V850_HI16:
      addend += (bfd_get_16 (abfd, address) << 16);
      addend = (addend >> 16);
      insn = addend;
      break;

    case R_V850_HI16_S:
      /* Remember where this relocation took place.  */
      remember_hi16s_reloc (abfd, addend, address);

      addend += (bfd_get_16 (abfd, address) << 16);
      addend = (addend >> 16) + ((addend & 0x8000) != 0);

      /* This relocation cannot overflow.  */
      if (addend > 0x7fff)
	addend = 0;

      insn = addend;
      break;

    case R_V850_LO16:
      /* Calculate the sum of the value stored in the instruction and the
	 addend and check for overflow from the low 16 bits into the high
	 16 bits.  The assembler has already done some of this:  If the
	 value stored in the instruction has its 15th bit set, (counting
	 from zero) then the assembler will have added 1 to the value
	 stored in the associated HI16S reloc.  So for example, these
	 relocations:

	     movhi hi( fred ), r0, r1
	     movea lo( fred ), r1, r1

	 will store 0 in the value fields for the MOVHI and MOVEA instructions
	 and addend will be the address of fred, but for these instructions:

	     movhi hi( fred + 0x123456), r0, r1
	     movea lo( fred + 0x123456), r1, r1

	 the value stored in the MOVHI instruction will be 0x12 and the value
	 stored in the MOVEA instruction will be 0x3456.  If however the
	 instructions were:

	     movhi hi( fred + 0x10ffff), r0, r1
	     movea lo( fred + 0x10ffff), r1, r1

	 then the value stored in the MOVHI instruction would be 0x11 (not
	 0x10) and the value stored in the MOVEA instruction would be 0xffff.
	 Thus (assuming for the moment that the addend is 0), at run time the
	 MOVHI instruction loads 0x110000 into r1, then the MOVEA instruction
	 adds 0xffffffff (sign extension!) producing 0x10ffff.  Similarly if
	 the instructions were:

	     movhi hi( fred - 1), r0, r1
	     movea lo( fred - 1), r1, r1

	 then 0 is stored in the MOVHI instruction and -1 is stored in the
	 MOVEA instruction.

	 Overflow can occur if the addition of the value stored in the
	 instruction plus the addend sets the 15th bit when before it was clear.
	 This is because the 15th bit will be sign extended into the high part,
	 thus reducing its value by one, but since the 15th bit was originally
	 clear, the assembler will not have added 1 to the previous HI16S reloc
	 to compensate for this effect.  For example:

	    movhi hi( fred + 0x123456), r0, r1
	    movea lo( fred + 0x123456), r1, r1

	 The value stored in HI16S reloc is 0x12, the value stored in the LO16
	 reloc is 0x3456.  If we assume that the address of fred is 0x00007000
	 then the relocations become:

	   HI16S: 0x0012 + (0x00007000 >> 16)    = 0x12
	   LO16:  0x3456 + (0x00007000 & 0xffff) = 0xa456

	 but when the instructions are executed, the MOVEA instruction's value
	 is signed extended, so the sum becomes:

	      0x00120000
	    + 0xffffa456
	    ------------
	      0x0011a456    but 'fred + 0x123456' = 0x0012a456

	 Note that if the 15th bit was set in the value stored in the LO16
	 reloc, then we do not have to do anything:

	    movhi hi( fred + 0x10ffff), r0, r1
	    movea lo( fred + 0x10ffff), r1, r1

	    HI16S:  0x0011 + (0x00007000 >> 16)    = 0x11
	    LO16:   0xffff + (0x00007000 & 0xffff) = 0x6fff

	      0x00110000
	    + 0x00006fff
	    ------------
	      0x00116fff  = fred + 0x10ffff = 0x7000 + 0x10ffff

	 Overflow can also occur if the computation carries into the 16th bit
	 and it also results in the 15th bit having the same value as the 15th
	 bit of the original value.   What happens is that the HI16S reloc
	 will have already examined the 15th bit of the original value and
	 added 1 to the high part if the bit is set.  This compensates for the
	 sign extension of 15th bit of the result of the computation.  But now
	 there is a carry into the 16th bit, and this has not been allowed for.

	 So, for example if fred is at address 0xf000:

	   movhi hi( fred + 0xffff), r0, r1    [bit 15 of the offset is set]
	   movea lo( fred + 0xffff), r1, r1

	   HI16S: 0x0001 + (0x0000f000 >> 16)    = 0x0001
	   LO16:  0xffff + (0x0000f000 & 0xffff) = 0xefff   (carry into bit 16 is lost)

	     0x00010000
	   + 0xffffefff
	   ------------
	     0x0000efff   but 'fred + 0xffff' = 0x0001efff

	 Similarly, if the 15th bit remains clear, but overflow occurs into
	 the 16th bit then (assuming the address of fred is 0xf000):

	   movhi hi( fred + 0x7000), r0, r1    [bit 15 of the offset is clear]
	   movea lo( fred + 0x7000), r1, r1

	   HI16S: 0x0000 + (0x0000f000 >> 16)    = 0x0000
	   LO16:  0x7000 + (0x0000f000 & 0xffff) = 0x6fff  (carry into bit 16 is lost)

	     0x00000000
	   + 0x00006fff
	   ------------
	     0x00006fff   but 'fred + 0x7000' = 0x00016fff

	 Note - there is no need to change anything if a carry occurs, and the
	 15th bit changes its value from being set to being clear, as the HI16S
	 reloc will have already added in 1 to the high part for us:

	   movhi hi( fred + 0xffff), r0, r1     [bit 15 of the offset is set]
	   movea lo( fred + 0xffff), r1, r1

	   HI16S: 0x0001 + (0x00007000 >> 16)
	   LO16:  0xffff + (0x00007000 & 0xffff) = 0x6fff  (carry into bit 16 is lost)

	     0x00010000
	   + 0x00006fff   (bit 15 not set, so the top half is zero)
	   ------------
	     0x00016fff   which is right (assuming that fred is at 0x7000)

	 but if the 15th bit goes from being clear to being set, then we must
	 once again handle overflow:

	   movhi hi( fred + 0x7000), r0, r1     [bit 15 of the offset is clear]
	   movea lo( fred + 0x7000), r1, r1

	   HI16S: 0x0000 + (0x0000ffff >> 16)
	   LO16:  0x7000 + (0x0000ffff & 0xffff) = 0x6fff  (carry into bit 16)

	     0x00000000
	   + 0x00006fff   (bit 15 not set, so the top half is zero)
	   ------------
	     0x00006fff   which is wrong (assuming that fred is at 0xffff)
	 */

      {
	long result;

	insn   = bfd_get_16 (abfd, address);
	result = insn + addend;

#define BIT15_SET(x) ((x) & 0x8000)
#define OVERFLOWS(a,i) ((((a) & 0xffff) + (i)) > 0xffff)

	if ((BIT15_SET (result) && ! BIT15_SET (addend))
	    || (OVERFLOWS (addend, insn)
		&& ((! BIT15_SET (insn)) || (BIT15_SET (addend)))))
	  {
	    boolean already_updated;
	    bfd_byte * hi16s_address = find_remembered_hi16s_reloc
	      (addend, & already_updated);

	    /* Amend the matching HI16_S relocation.  */
	    if (hi16s_address != NULL)
	      {
		if (! already_updated)
		  {
		    insn = bfd_get_16 (abfd, hi16s_address);
		    insn += 1;
		    bfd_put_16 (abfd, insn, hi16s_address);
		  }
	      }
	    else
	      {
		fprintf (stderr, _("FAILED to find previous HI16 reloc\n"));
		return bfd_reloc_overflow;
	      }
	  }

	/* Do not complain if value has top bit set, as this has been anticipated.  */
	insn = result & 0xffff;
	break;
      }

    case R_V850_8:
      addend += (char) bfd_get_8 (abfd, address);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7f || saddend < -0x80)
	return bfd_reloc_overflow;

      bfd_put_8 (abfd, addend, address);
      return bfd_reloc_ok;

    case R_V850_CALLT_16_16_OFFSET:
      addend += bfd_get_16 (abfd, address);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0xffff || saddend < 0)
	return bfd_reloc_overflow;

      insn = addend;
      break;

    case R_V850_16:

      /* drop through */
    case R_V850_SDA_16_16_OFFSET:
    case R_V850_ZDA_16_16_OFFSET:
    case R_V850_TDA_16_16_OFFSET:
      addend += bfd_get_16 (abfd, address);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7fff || saddend < -0x8000)
	return bfd_reloc_overflow;

      insn = addend;
      break;

    case R_V850_SDA_15_16_OFFSET:
    case R_V850_ZDA_15_16_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += (insn & 0xfffe);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7ffe || saddend < -0x8000)
	return bfd_reloc_overflow;

      if (addend & 1)
        return bfd_reloc_dangerous;

      insn = (addend & ~1) | (insn & 1);
      break;

    case R_V850_TDA_6_8_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += ((insn & 0x7e) << 1);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0xfc || saddend < 0)
	return bfd_reloc_overflow;

      if (addend & 3)
	return bfd_reloc_dangerous;

      insn &= 0xff81;
      insn |= (addend >> 1);
      break;

    case R_V850_TDA_7_8_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += ((insn & 0x7f) << 1);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0xfe || saddend < 0)
	return bfd_reloc_overflow;

      if (addend & 1)
	return bfd_reloc_dangerous;

      insn &= 0xff80;
      insn |= (addend >> 1);
      break;

    case R_V850_TDA_7_7_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += insn & 0x7f;

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7f || saddend < 0)
	return bfd_reloc_overflow;

      insn &= 0xff80;
      insn |= addend;
      break;

    case R_V850_TDA_4_5_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += ((insn & 0xf) << 1);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x1e || saddend < 0)
	return bfd_reloc_overflow;

      if (addend & 1)
	return bfd_reloc_dangerous;

      insn &= 0xfff0;
      insn |= (addend >> 1);
      break;

    case R_V850_TDA_4_4_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += insn & 0xf;

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0xf || saddend < 0)
	return bfd_reloc_overflow;

      insn &= 0xfff0;
      insn |= addend;
      break;

    case R_V850_ZDA_16_16_SPLIT_OFFSET:
    case R_V850_SDA_16_16_SPLIT_OFFSET:
      insn = bfd_get_32 (abfd, address);
      addend += ((insn & 0xfffe0000) >> 16) + ((insn & 0x20) >> 5);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7fff || saddend < -0x8000)
	return bfd_reloc_overflow;

      insn &= 0x0001ffdf;
      insn |= (addend & 1) << 5;
      insn |= (addend & ~1) << 16;

      bfd_put_32 (abfd, insn, address);
      return bfd_reloc_ok;

    case R_V850_CALLT_6_7_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += ((insn & 0x3f) << 1);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7e || saddend < 0)
	return bfd_reloc_overflow;

      if (addend & 1)
	return bfd_reloc_dangerous;

      insn &= 0xff80;
      insn |= (addend >> 1);
      break;

    case R_V850_GNU_VTINHERIT:
    case R_V850_GNU_VTENTRY:
      return bfd_reloc_ok;

    }

  bfd_put_16 (abfd, insn, address);
  return bfd_reloc_ok;
}

/* Insert the addend into the instruction.  */
static bfd_reloc_status_type
v850_elf_reloc (abfd, reloc, symbol, data, isection, obfd, err)
     bfd *       abfd ATTRIBUTE_UNUSED;
     arelent *   reloc;
     asymbol *   symbol;
     PTR         data ATTRIBUTE_UNUSED;
     asection *  isection;
     bfd *       obfd;
     char **     err ATTRIBUTE_UNUSED;
{
  long relocation;

  /* If there is an output BFD,
     and the symbol is not a section name (which is only defined at final link time),
     and either we are not putting the addend into the instruction
         or the addend is zero, so there is nothing to add into the instruction
     then just fixup the address and return.  */
  if (obfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc->howto->partial_inplace
	  || reloc->addend == 0))
    {
      reloc->address += isection->output_offset;
      return bfd_reloc_ok;
    }
#if 0
  else if (obfd != NULL)
    {
      return bfd_reloc_continue;
    }
#endif

  /* Catch relocs involving undefined symbols.  */
  if (bfd_is_und_section (symbol->section)
      && (symbol->flags & BSF_WEAK) == 0
      && obfd == NULL)
    return bfd_reloc_undefined;

  /* We handle final linking of some relocs ourselves.  */

  /* Is the address of the relocation really within the section?  */
  if (reloc->address > isection->_cooked_size)
    return bfd_reloc_outofrange;

  /* Work out which section the relocation is targetted at and the
     initial relocation command value.  */

  /* Get symbol value.  (Common symbols are special.)  */
  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  /* Convert input-section-relative symbol value to absolute + addend.  */
  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc->addend;

#if 0 /* Since this reloc is going to be processed later on, we should
	 not make it pc-relative here.  To test this, try assembling and
	 linking this program:

	 	.text
		.globl _start
		nop
	_start:         
        	jr foo

	        .section ".foo","ax"
		nop
	foo:
        	nop
      */
  if (reloc->howto->pc_relative == true)
    {
      /* Here the variable relocation holds the final address of the
	 symbol we are relocating against, plus any addend.  */
      relocation -= isection->output_section->vma + isection->output_offset;

      /* Deal with pcrel_offset */
      relocation -= reloc->address;
    }
#endif

  reloc->addend = relocation;
  return bfd_reloc_ok;
}

static boolean
v850_elf_is_local_label_name (abfd, name)
     bfd *         abfd ATTRIBUTE_UNUSED;
     const char *  name;
{
  return (   (name[0] == '.' && (name[1] == 'L' || name[1] == '.'))
	  || (name[0] == '_' &&  name[1] == '.' && name[2] == 'L' && name[3] == '_'));
}

/* Perform a relocation as part of a final link.  */
static bfd_reloc_status_type
v850_elf_final_link_relocate (howto, input_bfd, output_bfd,
				    input_section, contents, offset, value,
				    addend, info, sym_sec, is_local)
     reloc_howto_type *      howto;
     bfd *                   input_bfd;
     bfd *                   output_bfd ATTRIBUTE_UNUSED;
     asection *              input_section;
     bfd_byte *              contents;
     bfd_vma                 offset;
     bfd_vma                 value;
     bfd_vma                 addend;
     struct bfd_link_info *  info;
     asection *              sym_sec;
     int                     is_local ATTRIBUTE_UNUSED;
{
  unsigned long  r_type   = howto->type;
  bfd_byte *     hit_data = contents + offset;

  /* Adjust the value according to the relocation.  */
  switch (r_type)
    {
    case R_V850_9_PCREL:
      value -= (input_section->output_section->vma
		+ input_section->output_offset);
      value -= offset;
      break;

    case R_V850_22_PCREL:
      value -= (input_section->output_section->vma
		+ input_section->output_offset
		+ offset);

      /* If the sign extension will corrupt the value then we have overflowed.  */
      if (((value & 0xff000000) != 0x0) && ((value & 0xff000000) != 0xff000000))
	return bfd_reloc_overflow;

      value = SEXT24 (value);  /* Only the bottom 24 bits of the PC are valid */
      break;

    case R_V850_HI16_S:
    case R_V850_HI16:
    case R_V850_LO16:
    case R_V850_16:
    case R_V850_32:
    case R_V850_8:
      break;

    case R_V850_ZDA_15_16_OFFSET:
    case R_V850_ZDA_16_16_OFFSET:
    case R_V850_ZDA_16_16_SPLIT_OFFSET:
      if (sym_sec == NULL)
	return bfd_reloc_undefined;

      value -= sym_sec->output_section->vma;
      break;

    case R_V850_SDA_15_16_OFFSET:
    case R_V850_SDA_16_16_OFFSET:
    case R_V850_SDA_16_16_SPLIT_OFFSET:
      {
	unsigned long                gp;
	struct bfd_link_hash_entry * h;

	if (sym_sec == NULL)
	  return bfd_reloc_undefined;

	/* Get the value of __gp.  */
	h = bfd_link_hash_lookup (info->hash, "__gp", false, false, true);
	if (h == (struct bfd_link_hash_entry *) NULL
	    || h->type != bfd_link_hash_defined)
	  return bfd_reloc_other;

	gp = (h->u.def.value
	      + h->u.def.section->output_section->vma
	      + h->u.def.section->output_offset);

	value -= sym_sec->output_section->vma;
	value -= (gp - sym_sec->output_section->vma);
      }
    break;

    case R_V850_TDA_4_4_OFFSET:
    case R_V850_TDA_4_5_OFFSET:
    case R_V850_TDA_16_16_OFFSET:
    case R_V850_TDA_7_7_OFFSET:
    case R_V850_TDA_7_8_OFFSET:
    case R_V850_TDA_6_8_OFFSET:
      {
	unsigned long                ep;
	struct bfd_link_hash_entry * h;

	/* Get the value of __ep.  */
	h = bfd_link_hash_lookup (info->hash, "__ep", false, false, true);
	if (h == (struct bfd_link_hash_entry *) NULL
	    || h->type != bfd_link_hash_defined)
	  return bfd_reloc_continue;  /* Actually this indicates that __ep could not be found.  */

	ep = (h->u.def.value
	      + h->u.def.section->output_section->vma
	      + h->u.def.section->output_offset);

	value -= ep;
      }
    break;

    case R_V850_CALLT_6_7_OFFSET:
      {
	unsigned long                ctbp;
	struct bfd_link_hash_entry * h;

	/* Get the value of __ctbp.  */
	h = bfd_link_hash_lookup (info->hash, "__ctbp", false, false, true);
	if (h == (struct bfd_link_hash_entry *) NULL
	    || h->type != bfd_link_hash_defined)
	  return (bfd_reloc_dangerous + 1);  /* Actually this indicates that __ctbp could not be found.  */

	ctbp = (h->u.def.value
	      + h->u.def.section->output_section->vma
	      + h->u.def.section->output_offset);
	value -= ctbp;
      }
    break;

    case R_V850_CALLT_16_16_OFFSET:
      {
	unsigned long                ctbp;
	struct bfd_link_hash_entry * h;

	if (sym_sec == NULL)
	  return bfd_reloc_undefined;

	/* Get the value of __ctbp.  */
	h = bfd_link_hash_lookup (info->hash, "__ctbp", false, false, true);
	if (h == (struct bfd_link_hash_entry *) NULL
	    || h->type != bfd_link_hash_defined)
	  return (bfd_reloc_dangerous + 1);

	ctbp = (h->u.def.value
	      + h->u.def.section->output_section->vma
	      + h->u.def.section->output_offset);

	value -= sym_sec->output_section->vma;
	value -= (ctbp - sym_sec->output_section->vma);
      }
    break;

    case R_V850_NONE:
    case R_V850_GNU_VTINHERIT:
    case R_V850_GNU_VTENTRY:
      return bfd_reloc_ok;

    default:
      return bfd_reloc_notsupported;
    }

  /* Perform the relocation.  */
  return v850_elf_perform_relocation (input_bfd, r_type, value + addend, hit_data);
}

/* Relocate an V850 ELF section.  */
static boolean
v850_elf_relocate_section (output_bfd, info, input_bfd, input_section,
			   contents, relocs, local_syms, local_sections)
     bfd *                  output_bfd;
     struct bfd_link_info * info;
     bfd *                  input_bfd;
     asection *             input_section;
     bfd_byte *             contents;
     Elf_Internal_Rela *    relocs;
     Elf_Internal_Sym *     local_syms;
     asection **            local_sections;
{
  Elf_Internal_Shdr *           symtab_hdr;
  struct elf_link_hash_entry ** sym_hashes;
  Elf_Internal_Rela *           rel;
  Elf_Internal_Rela *           relend;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  if (sym_hashes == NULL)
    {
      info->callbacks->warning
	(info, "no hash table available", NULL, input_bfd, input_section, 0);

      return false;
    }

  /* Reset the list of remembered HI16S relocs to empty.  */
  free_hi16s     = previous_hi16s;
  previous_hi16s = NULL;
  hi16s_counter  = 0;

  rel    = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int                          r_type;
      reloc_howto_type *           howto;
      unsigned long                r_symndx;
      Elf_Internal_Sym *           sym;
      asection *                   sec;
      struct elf_link_hash_entry * h;
      bfd_vma                      relocation;
      bfd_reloc_status_type        r;

      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type   = ELF32_R_TYPE (rel->r_info);

      if (r_type == R_V850_GNU_VTENTRY
          || r_type == R_V850_GNU_VTINHERIT)
        continue;

      howto = v850_elf_howto_table + r_type;

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
             anything, unless the reloc is against a section symbol,
             in which case we have to adjust according to where the
             section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sec = local_sections[r_symndx];
		  rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }

	  continue;
	}

      /* This is a final link.  */
      h = NULL;
      sym = NULL;
      sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);
#if 0
	  {
	    char * name;
	    name = bfd_elf_string_from_elf_section (input_bfd, symtab_hdr->sh_link, sym->st_name);
	    name = (name == NULL) ? "<none>" : name;
fprintf (stderr, "local: sec: %s, sym: %s (%d), value: %x + %x + %x addend %x\n",
	 sec->name, name, sym->st_name,
	 sec->output_section->vma, sec->output_offset, sym->st_value, rel->r_addend);
	  }
#endif
	}
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];

	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      relocation = (h->root.u.def.value
			    + sec->output_section->vma
			    + sec->output_offset);
#if 0
fprintf (stderr, "defined: sec: %s, name: %s, value: %x + %x + %x gives: %x\n",
	 sec->name, h->root.root.string, h->root.u.def.value, sec->output_section->vma, sec->output_offset, relocation);
#endif
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    {
#if 0
fprintf (stderr, "undefined: sec: %s, name: %s\n",
	 sec->name, h->root.root.string);
#endif
	      relocation = 0;
	    }
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset, true)))
		return false;
#if 0
fprintf (stderr, "unknown: name: %s\n", h->root.root.string);
#endif
	      relocation = 0;
	    }
	}

      /* FIXME: We should use the addend, but the COFF relocations
         don't.  */
      r = v850_elf_final_link_relocate (howto, input_bfd, output_bfd,
					input_section,
					contents, rel->r_offset,
					relocation, rel->r_addend,
					info, sec, h == NULL);

      if (r != bfd_reloc_ok)
	{
	  const char * name;
	  const char * msg = (const char *)0;

	  if (h != NULL)
	    name = h->root.root.string;
	  else
	    {
	      name = (bfd_elf_string_from_elf_section
		      (input_bfd, symtab_hdr->sh_link, sym->st_name));
	      if (name == NULL || *name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      if (! ((*info->callbacks->reloc_overflow)
		     (info, name, howto->name, (bfd_vma) 0,
		      input_bfd, input_section, rel->r_offset)))
		return false;
	      break;

	    case bfd_reloc_undefined:
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, name, input_bfd, input_section,
		      rel->r_offset, true)))
		return false;
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      goto common_error;

	    case bfd_reloc_notsupported:
	      msg = _("internal error: unsupported relocation error");
	      goto common_error;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous relocation");
	      goto common_error;

	    case bfd_reloc_other:
	      msg = _("could not locate special linker symbol __gp");
	      goto common_error;

	    case bfd_reloc_continue:
	      msg = _("could not locate special linker symbol __ep");
	      goto common_error;

	    case (bfd_reloc_dangerous + 1):
	      msg = _("could not locate special linker symbol __ctbp");
	      goto common_error;

	    default:
	      msg = _("internal error: unknown error");
	      /* fall through */

	    common_error:
	      if (!((*info->callbacks->warning)
		    (info, msg, name, input_bfd, input_section,
		     rel->r_offset)))
		return false;
	      break;
	    }
	}
    }

  return true;
}

static boolean
v850_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  /* No got and plt entries for v850-elf */
  return true;
}

static asection *
v850_elf_gc_mark_hook (abfd, info, rel, h, sym)
       bfd *abfd;
       struct bfd_link_info *info ATTRIBUTE_UNUSED;
       Elf_Internal_Rela *rel;
       struct elf_link_hash_entry *h;
       Elf_Internal_Sym *sym;
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
      {
      case R_V850_GNU_VTINHERIT:
      case R_V850_GNU_VTENTRY:
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
     {
       if (!(elf_bad_symtab (abfd)
           && ELF_ST_BIND (sym->st_info) != STB_LOCAL)
         && ! ((sym->st_shndx <= 0 || sym->st_shndx >= SHN_LORESERVE)
                && sym->st_shndx != SHN_COMMON))
          {
            return bfd_section_from_elf_index (abfd, sym->st_shndx);
          }
      }
  return NULL;
}
/* Set the right machine number.  */
static boolean
v850_elf_object_p (abfd)
     bfd *abfd;
{
  switch (elf_elfheader (abfd)->e_flags & EF_V850_ARCH)
    {
    default:
    case E_V850_ARCH:   (void) bfd_default_set_arch_mach (abfd, bfd_arch_v850, 0); break;
    case E_V850E_ARCH:  (void) bfd_default_set_arch_mach (abfd, bfd_arch_v850, bfd_mach_v850e); break;
    case E_V850EA_ARCH: (void) bfd_default_set_arch_mach (abfd, bfd_arch_v850, bfd_mach_v850ea); break;
    }
  return true;
}

/* Store the machine number in the flags field.  */
static void
v850_elf_final_write_processing (abfd, linker)
     bfd *   abfd;
     boolean linker ATTRIBUTE_UNUSED;
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
    default:
    case 0: val = E_V850_ARCH; break;
    case bfd_mach_v850e:  val = E_V850E_ARCH; break;
    case bfd_mach_v850ea: val = E_V850EA_ARCH;  break;
    }

  elf_elfheader (abfd)->e_flags &=~ EF_V850_ARCH;
  elf_elfheader (abfd)->e_flags |= val;
}

/* Function to keep V850 specific file flags.  */
static boolean
v850_elf_set_private_flags (abfd, flags)
     bfd *    abfd;
     flagword flags;
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = true;
  return true;
}

/* Copy backend specific data from one object module to another */
static boolean
v850_elf_copy_private_bfd_data (ibfd, obfd)
     bfd * ibfd;
     bfd * obfd;
{
  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  BFD_ASSERT (!elf_flags_init (obfd)
	      || (elf_elfheader (obfd)->e_flags
		  == elf_elfheader (ibfd)->e_flags));

  elf_gp (obfd) = elf_gp (ibfd);
  elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;
  elf_flags_init (obfd) = true;
  return true;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */
static boolean
v850_elf_merge_private_bfd_data (ibfd, obfd)
     bfd * ibfd;
     bfd * obfd;
{
  flagword out_flags;
  flagword in_flags;

  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  in_flags = elf_elfheader (ibfd)->e_flags;
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
	return true;

      elf_flags_init (obfd) = true;
      elf_elfheader (obfd)->e_flags = in_flags;

      if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
	  && bfd_get_arch_info (obfd)->the_default)
	{
	  return bfd_set_arch_mach (obfd, bfd_get_arch (ibfd), bfd_get_mach (ibfd));
	}

      return true;
    }

  /* Check flag compatibility.  */
  if (in_flags == out_flags)
    return true;

  if ((in_flags & EF_V850_ARCH) != (out_flags & EF_V850_ARCH)
      && (in_flags & EF_V850_ARCH) != E_V850_ARCH)
    _bfd_error_handler (_("%s: Architecture mismatch with previous modules"),
			bfd_get_filename (ibfd));

  return true;
}
/* Display the flags field */

static boolean
v850_elf_print_private_bfd_data (abfd, ptr)
     bfd *   abfd;
     PTR     ptr;
{
  FILE * file = (FILE *) ptr;

  BFD_ASSERT (abfd != NULL && ptr != NULL);

  _bfd_elf_print_private_bfd_data (abfd, ptr);

  /* xgettext:c-format */
  fprintf (file, _("private flags = %lx: "), elf_elfheader (abfd)->e_flags);

  switch (elf_elfheader (abfd)->e_flags & EF_V850_ARCH)
    {
    default:
    case E_V850_ARCH: fprintf (file, _("v850 architecture")); break;
    case E_V850E_ARCH:  fprintf (file, _("v850e architecture")); break;
    case E_V850EA_ARCH: fprintf (file, _("v850ea architecture")); break;
    }

  fputc ('\n', file);

  return true;
}

/* V850 ELF uses four common sections.  One is the usual one, and the
   others are for (small) objects in one of the special data areas:
   small, tiny and zero.  All the objects are kept together, and then
   referenced via the gp register, the ep register or the r0 register
   respectively, which yields smaller, faster assembler code.  This
   approach is copied from elf32-mips.c.  */

static asection  v850_elf_scom_section;
static asymbol   v850_elf_scom_symbol;
static asymbol * v850_elf_scom_symbol_ptr;
static asection  v850_elf_tcom_section;
static asymbol   v850_elf_tcom_symbol;
static asymbol * v850_elf_tcom_symbol_ptr;
static asection  v850_elf_zcom_section;
static asymbol   v850_elf_zcom_symbol;
static asymbol * v850_elf_zcom_symbol_ptr;

/* Given a BFD section, try to locate the corresponding ELF section
   index.  */

static boolean
v850_elf_section_from_bfd_section (abfd, hdr, sec, retval)
     bfd *                 abfd ATTRIBUTE_UNUSED;
     Elf32_Internal_Shdr * hdr ATTRIBUTE_UNUSED;
     asection *            sec;
     int *                 retval;
{
  if (strcmp (bfd_get_section_name (abfd, sec), ".scommon") == 0)
    *retval = SHN_V850_SCOMMON;
  else if (strcmp (bfd_get_section_name (abfd, sec), ".tcommon") == 0)
    *retval = SHN_V850_TCOMMON;
  else if (strcmp (bfd_get_section_name (abfd, sec), ".zcommon") == 0)
    *retval = SHN_V850_ZCOMMON;
  else
    return false;

  return true;
}

/* Handle the special V850 section numbers that a symbol may use.  */

static void
v850_elf_symbol_processing (abfd, asym)
     bfd *     abfd;
     asymbol * asym;
{
  elf_symbol_type * elfsym = (elf_symbol_type *) asym;
  unsigned short index;

  index = elfsym->internal_elf_sym.st_shndx;

  /* If the section index is an "ordinary" index, then it may
     refer to a v850 specific section created by the assembler.
     Check the section's type and change the index it matches.

     FIXME: Should we alter the st_shndx field as well ?  */

  if (index < elf_elfheader(abfd)[0].e_shnum)
    switch (elf_elfsections(abfd)[index]->sh_type)
      {
      case SHT_V850_SCOMMON:
	index = SHN_V850_SCOMMON;
	break;

      case SHT_V850_TCOMMON:
	index = SHN_V850_TCOMMON;
	break;

      case SHT_V850_ZCOMMON:
	index = SHN_V850_ZCOMMON;
	break;

      default:
	break;
      }

  switch (index)
    {
    case SHN_V850_SCOMMON:
      if (v850_elf_scom_section.name == NULL)
	{
	  /* Initialize the small common section.  */
	  v850_elf_scom_section.name           = ".scommon";
	  v850_elf_scom_section.flags          = SEC_IS_COMMON | SEC_ALLOC | SEC_DATA;
	  v850_elf_scom_section.output_section = & v850_elf_scom_section;
	  v850_elf_scom_section.symbol         = & v850_elf_scom_symbol;
	  v850_elf_scom_section.symbol_ptr_ptr = & v850_elf_scom_symbol_ptr;
	  v850_elf_scom_symbol.name            = ".scommon";
	  v850_elf_scom_symbol.flags           = BSF_SECTION_SYM;
	  v850_elf_scom_symbol.section         = & v850_elf_scom_section;
	  v850_elf_scom_symbol_ptr             = & v850_elf_scom_symbol;
	}
      asym->section = & v850_elf_scom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;

    case SHN_V850_TCOMMON:
      if (v850_elf_tcom_section.name == NULL)
	{
	  /* Initialize the tcommon section.  */
	  v850_elf_tcom_section.name           = ".tcommon";
	  v850_elf_tcom_section.flags          = SEC_IS_COMMON;
	  v850_elf_tcom_section.output_section = & v850_elf_tcom_section;
	  v850_elf_tcom_section.symbol         = & v850_elf_tcom_symbol;
	  v850_elf_tcom_section.symbol_ptr_ptr = & v850_elf_tcom_symbol_ptr;
	  v850_elf_tcom_symbol.name            = ".tcommon";
	  v850_elf_tcom_symbol.flags           = BSF_SECTION_SYM;
	  v850_elf_tcom_symbol.section         = & v850_elf_tcom_section;
	  v850_elf_tcom_symbol_ptr             = & v850_elf_tcom_symbol;
	}
      asym->section = & v850_elf_tcom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;

    case SHN_V850_ZCOMMON:
      if (v850_elf_zcom_section.name == NULL)
	{
	  /* Initialize the zcommon section.  */
	  v850_elf_zcom_section.name           = ".zcommon";
	  v850_elf_zcom_section.flags          = SEC_IS_COMMON;
	  v850_elf_zcom_section.output_section = & v850_elf_zcom_section;
	  v850_elf_zcom_section.symbol         = & v850_elf_zcom_symbol;
	  v850_elf_zcom_section.symbol_ptr_ptr = & v850_elf_zcom_symbol_ptr;
	  v850_elf_zcom_symbol.name            = ".zcommon";
	  v850_elf_zcom_symbol.flags           = BSF_SECTION_SYM;
	  v850_elf_zcom_symbol.section         = & v850_elf_zcom_section;
	  v850_elf_zcom_symbol_ptr             = & v850_elf_zcom_symbol;
	}
      asym->section = & v850_elf_zcom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;
    }
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We must handle the special v850 section numbers here.  */

static boolean
v850_elf_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *                    abfd;
     struct bfd_link_info *   info ATTRIBUTE_UNUSED;
     const Elf_Internal_Sym * sym;
     const char **            namep ATTRIBUTE_UNUSED;
     flagword *               flagsp ATTRIBUTE_UNUSED;
     asection **              secp;
     bfd_vma *                valp;
{
  int index = sym->st_shndx;

  /* If the section index is an "ordinary" index, then it may
     refer to a v850 specific section created by the assembler.
     Check the section's type and change the index it matches.

     FIXME: Should we alter the st_shndx field as well ?  */

  if (index < elf_elfheader(abfd)[0].e_shnum)
    switch (elf_elfsections(abfd)[index]->sh_type)
      {
      case SHT_V850_SCOMMON:
	index = SHN_V850_SCOMMON;
	break;

      case SHT_V850_TCOMMON:
	index = SHN_V850_TCOMMON;
	break;

      case SHT_V850_ZCOMMON:
	index = SHN_V850_ZCOMMON;
	break;

      default:
	break;
      }

  switch (index)
    {
    case SHN_V850_SCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".scommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;

    case SHN_V850_TCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".tcommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;

    case SHN_V850_ZCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".zcommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;
    }

  return true;
}

/*ARGSIGNORED*/
static boolean
v850_elf_link_output_symbol_hook (abfd, info, name, sym, input_sec)
     bfd *                  abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info * info ATTRIBUTE_UNUSED;
     const char *           name ATTRIBUTE_UNUSED;
     Elf_Internal_Sym *     sym;
     asection *             input_sec;
{
  /* If we see a common symbol, which implies a relocatable link, then
     if a symbol was in a special common section in an input file, mark
     it as a special common in the output file.  */

  if (sym->st_shndx == SHN_COMMON)
    {
      if (strcmp (input_sec->name, ".scommon") == 0)
	sym->st_shndx = SHN_V850_SCOMMON;
      else if (strcmp (input_sec->name, ".tcommon") == 0)
	sym->st_shndx = SHN_V850_TCOMMON;
      else if (strcmp (input_sec->name, ".zcommon") == 0)
	sym->st_shndx = SHN_V850_ZCOMMON;
    }

  return true;
}

static boolean
v850_elf_section_from_shdr (abfd, hdr, name)
     bfd *               abfd;
     Elf_Internal_Shdr * hdr;
     char *              name;
{
  /* There ought to be a place to keep ELF backend specific flags, but
     at the moment there isn't one.  We just keep track of the
     sections by their name, instead.  */

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
    return false;

  switch (hdr->sh_type)
    {
    case SHT_V850_SCOMMON:
    case SHT_V850_TCOMMON:
    case SHT_V850_ZCOMMON:
      if (! bfd_set_section_flags (abfd, hdr->bfd_section,
				   (bfd_get_section_flags (abfd,
							   hdr->bfd_section)
				    | SEC_IS_COMMON)))
	return false;
    }

  return true;
}

/* Set the correct type for a V850 ELF section.  We do this by the
   section name, which is a hack, but ought to work.  */
static boolean
v850_elf_fake_sections (abfd, hdr, sec)
     bfd *                 abfd ATTRIBUTE_UNUSED;
     Elf32_Internal_Shdr * hdr;
     asection *            sec;
{
  register const char * name;

  name = bfd_get_section_name (abfd, sec);

  if (strcmp (name, ".scommon") == 0)
    {
      hdr->sh_type = SHT_V850_SCOMMON;
    }
  else if (strcmp (name, ".tcommon") == 0)
    {
      hdr->sh_type = SHT_V850_TCOMMON;
    }
  else if (strcmp (name, ".zcommon") == 0)
    hdr->sh_type = SHT_V850_ZCOMMON;

  return true;
}

#define TARGET_LITTLE_SYM			bfd_elf32_v850_vec
#define TARGET_LITTLE_NAME			"elf32-v850"
#define ELF_ARCH				bfd_arch_v850
#define ELF_MACHINE_CODE			EM_CYGNUS_V850
#define ELF_MAXPAGESIZE				0x1000

#define elf_info_to_howto			v850_elf_info_to_howto_rela
#define elf_info_to_howto_rel			v850_elf_info_to_howto_rel

#define elf_backend_check_relocs		v850_elf_check_relocs
#define elf_backend_relocate_section    	v850_elf_relocate_section
#define elf_backend_object_p			v850_elf_object_p
#define elf_backend_final_write_processing 	v850_elf_final_write_processing
#define elf_backend_section_from_bfd_section 	v850_elf_section_from_bfd_section
#define elf_backend_symbol_processing		v850_elf_symbol_processing
#define elf_backend_add_symbol_hook		v850_elf_add_symbol_hook
#define elf_backend_link_output_symbol_hook 	v850_elf_link_output_symbol_hook
#define elf_backend_section_from_shdr		v850_elf_section_from_shdr
#define elf_backend_fake_sections		v850_elf_fake_sections
#define elf_backend_gc_mark_hook                v850_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook               v850_elf_gc_sweep_hook

#define elf_backend_can_gc_sections 1

#define bfd_elf32_bfd_is_local_label_name	v850_elf_is_local_label_name
#define bfd_elf32_bfd_reloc_type_lookup		v850_elf_reloc_type_lookup
#define bfd_elf32_bfd_copy_private_bfd_data 	v850_elf_copy_private_bfd_data
#define bfd_elf32_bfd_merge_private_bfd_data 	v850_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags		v850_elf_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data	v850_elf_print_private_bfd_data

#define elf_symbol_leading_char			'_'

#include "elf32-target.h"
