/* CRIS-specific support for 32-bit ELF.
   Copyright 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.
   Contributed by Axis Communications AB.
   Written by Hans-Peter Nilsson, based on elf32-fr30.c
   PIC and shlib bits based primarily on elf32-m68k.c and elf32-i386.c.

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
#include "elf/cris.h"

/* Forward declarations.  */
static reloc_howto_type * cris_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));

static void cris_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));

static bfd_boolean cris_elf_grok_prstatus
  PARAMS ((bfd *abfd, Elf_Internal_Note *note));

static bfd_boolean cris_elf_grok_psinfo
  PARAMS ((bfd *abfd, Elf_Internal_Note *note));

static bfd_boolean cris_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));

static bfd_reloc_status_type cris_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, bfd_vma));

static bfd_boolean cris_elf_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

static asection * cris_elf_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));

static bfd_boolean cris_elf_object_p PARAMS ((bfd *));

static void cris_elf_final_write_processing PARAMS ((bfd *, bfd_boolean));

static bfd_boolean cris_elf_print_private_bfd_data PARAMS ((bfd *, PTR));

static bfd_boolean cris_elf_merge_private_bfd_data PARAMS ((bfd *, bfd *));

struct elf_cris_link_hash_entry;
static bfd_boolean elf_cris_discard_excess_dso_dynamics
  PARAMS ((struct elf_cris_link_hash_entry *, PTR));
static bfd_boolean elf_cris_discard_excess_program_dynamics
  PARAMS ((struct elf_cris_link_hash_entry *, PTR));
static bfd_boolean elf_cris_adjust_gotplt_to_got
  PARAMS ((struct elf_cris_link_hash_entry *, PTR));
static bfd_boolean elf_cris_try_fold_plt_to_got
  PARAMS ((struct elf_cris_link_hash_entry *, PTR));
static struct bfd_hash_entry *elf_cris_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static struct bfd_link_hash_table *elf_cris_link_hash_table_create
  PARAMS ((bfd *));
static bfd_boolean elf_cris_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static bfd_boolean cris_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));

static bfd_boolean elf_cris_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean elf_cris_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static bfd_boolean elf_cris_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static void elf_cris_hide_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *, bfd_boolean));
static enum elf_reloc_type_class elf_cris_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));

static reloc_howto_type cris_elf_howto_table [] =
{
  /* This reloc does nothing.  */
  HOWTO (R_CRIS_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An 8 bit absolute relocation.  */
  HOWTO (R_CRIS_8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_8",		/* name */
	 FALSE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0x00ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_CRIS_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_16",		/* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.  */
  HOWTO (R_CRIS_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_32",		/* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An 8 bit PC-relative relocation.  */
  HOWTO (R_CRIS_8_PCREL,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_8_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0x0000,		/* src_mask */
	 0x00ff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 16 bit PC-relative relocation.  */
  HOWTO (R_CRIS_16_PCREL,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_16_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 32 bit PC-relative relocation.  */
  HOWTO (R_CRIS_32_PCREL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_32_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_CRIS_GNU_VTINHERIT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_CRIS_GNU_VTINHERIT", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_CRIS_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_elf_rel_vtable_reloc_fn,	/* special_function */
	 "R_CRIS_GNU_VTENTRY",	 /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* This is used only by the dynamic linker.  The symbol should exist
     both in the object being run and in some shared library.  The
     dynamic linker copies the data addressed by the symbol from the
     shared library into the object, because the object being
     run has to have the data at some particular address.  */
  HOWTO (R_CRIS_COPY,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_COPY",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_CRIS_32, but used when setting global offset table entries.  */
  HOWTO (R_CRIS_GLOB_DAT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_GLOB_DAT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Marks a procedure linkage table entry for a symbol.  */
  HOWTO (R_CRIS_JUMP_SLOT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_JUMP_SLOT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Used only by the dynamic linker.  When the object is run, this
     longword is set to the load address of the object, plus the
     addend.  */
  HOWTO (R_CRIS_RELATIVE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_RELATIVE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_CRIS_32, but referring to the GOT table entry for the symbol.  */
  HOWTO (R_CRIS_16_GOT,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_16_GOT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRIS_32_GOT,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_32_GOT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Like R_CRIS_32_GOT, but referring to (and requesting a) PLT part of
     the GOT table for the symbol.  */
  HOWTO (R_CRIS_16_GOTPLT,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_16_GOTPLT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_CRIS_32_GOTPLT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_32_GOTPLT",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32-bit offset from GOT to (local const) symbol: no GOT entry should
     be necessary.  */
  HOWTO (R_CRIS_32_GOTREL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_32_GOTREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32-bit offset from GOT to entry for this symbol in PLT and request
     to create PLT entry for symbol.  */
  HOWTO (R_CRIS_32_PLT_GOTREL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_32_PLT_GOTREL", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32-bit offset from PC (location after the relocation) + addend to
     entry for this symbol in PLT and request to create PLT entry for
     symbol.  */
  HOWTO (R_CRIS_32_PLT_PCREL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_CRIS_32_PLT_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE)			/* pcrel_offset */
};

/* Map BFD reloc types to CRIS ELF reloc types.  */

struct cris_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned int cris_reloc_val;
};

static const struct cris_reloc_map cris_reloc_map [] =
{
  { BFD_RELOC_NONE,		R_CRIS_NONE },
  { BFD_RELOC_8,		R_CRIS_8 },
  { BFD_RELOC_16,		R_CRIS_16 },
  { BFD_RELOC_32,		R_CRIS_32 },
  { BFD_RELOC_8_PCREL,		R_CRIS_8_PCREL },
  { BFD_RELOC_16_PCREL,		R_CRIS_16_PCREL },
  { BFD_RELOC_32_PCREL,		R_CRIS_32_PCREL },
  { BFD_RELOC_VTABLE_INHERIT,	R_CRIS_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY,	R_CRIS_GNU_VTENTRY },
  { BFD_RELOC_CRIS_COPY,	R_CRIS_COPY },
  { BFD_RELOC_CRIS_GLOB_DAT,	R_CRIS_GLOB_DAT },
  { BFD_RELOC_CRIS_JUMP_SLOT,	R_CRIS_JUMP_SLOT },
  { BFD_RELOC_CRIS_RELATIVE,	R_CRIS_RELATIVE },
  { BFD_RELOC_CRIS_16_GOT,	R_CRIS_16_GOT },
  { BFD_RELOC_CRIS_32_GOT,	R_CRIS_32_GOT },
  { BFD_RELOC_CRIS_16_GOTPLT,	R_CRIS_16_GOTPLT },
  { BFD_RELOC_CRIS_32_GOTPLT,	R_CRIS_32_GOTPLT },
  { BFD_RELOC_CRIS_32_GOTREL,	R_CRIS_32_GOTREL },
  { BFD_RELOC_CRIS_32_PLT_GOTREL, R_CRIS_32_PLT_GOTREL },
  { BFD_RELOC_CRIS_32_PLT_PCREL, R_CRIS_32_PLT_PCREL }
};

static reloc_howto_type *
cris_reloc_type_lookup (abfd, code)
     bfd * abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0; i < sizeof (cris_reloc_map) / sizeof (cris_reloc_map[0]); i++)
    if (cris_reloc_map [i].bfd_reloc_val == code)
      return & cris_elf_howto_table [cris_reloc_map[i].cris_reloc_val];

  return NULL;
}

/* Set the howto pointer for an CRIS ELF reloc.  */

static void
cris_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd * abfd ATTRIBUTE_UNUSED;
     arelent * cache_ptr;
     Elf_Internal_Rela * dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_CRIS_max);
  cache_ptr->howto = & cris_elf_howto_table [r_type];
}

/* Support for core dump NOTE sections.  */

static bfd_boolean
cris_elf_grok_prstatus (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  int offset;
  size_t raw_size;

  switch (note->descsz)
    {
      default:
	return FALSE;

      case 214:		/* Linux/CRIS */
	/* pr_cursig */
	elf_tdata (abfd)->core_signal = bfd_get_16 (abfd, note->descdata + 12);

	/* pr_pid */
	elf_tdata (abfd)->core_pid = bfd_get_32 (abfd, note->descdata + 22);

	/* pr_reg */
	offset = 70;
	raw_size = 140;

	break;
    }

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg",
					  raw_size, note->descpos + offset);
}

static bfd_boolean
cris_elf_grok_psinfo (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  switch (note->descsz)
    {
      default:
	return FALSE;

      case 124:		/* Linux/CRIS elf_prpsinfo */
	elf_tdata (abfd)->core_program
	 = _bfd_elfcore_strndup (abfd, note->descdata + 28, 16);
	elf_tdata (abfd)->core_command
	 = _bfd_elfcore_strndup (abfd, note->descdata + 44, 80);
    }

  /* Note that for some reason, a spurious space is tacked
     onto the end of the args in some (at least one anyway)
     implementations, so strip it off if it exists.  */

  {
    char *command = elf_tdata (abfd)->core_command;
    int n = strlen (command);

    if (0 < n && command[n - 1] == ' ')
      command[n - 1] = '\0';
  }

  return TRUE;
}

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/lib/ld.so.1"

/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 20

/* The first entry in an absolute procedure linkage table looks like this.  */

static const bfd_byte elf_cris_plt0_entry[PLT_ENTRY_SIZE] =
{
  0xfc, 0xe1,
  0x7e, 0x7e,	/* push mof.  */
  0x7f, 0x0d,   /*  (dip [pc+]) */
  0, 0, 0, 0,	/*  Replaced with address of .got + 4.  */
  0x30, 0x7a,	/* move [...],mof */
  0x7f, 0x0d,   /*  (dip [pc+]) */
  0, 0, 0, 0,	/*  Replaced with address of .got + 8.  */
  0x30, 0x09,	/* jump [...] */
};

/* Subsequent entries in an absolute procedure linkage table look like
   this.  */

static const bfd_byte elf_cris_plt_entry[PLT_ENTRY_SIZE] =
{
  0x7f, 0x0d,   /*  (dip [pc+]) */
  0, 0, 0, 0,	/*  Replaced with address of this symbol in .got.  */
  0x30, 0x09,	/* jump [...] */
  0x3f,	 0x7e,	/* move [pc+],mof */
  0, 0, 0, 0,	/*  Replaced with offset into relocation table.  */
  0x2f, 0xfe,	/* add.d [pc+],pc */
  0xec, 0xff,
  0xff, 0xff	/*  Replaced with offset to start of .plt.  */
};

/* The first entry in a PIC procedure linkage table looks like this.  */

static const bfd_byte elf_cris_pic_plt0_entry[PLT_ENTRY_SIZE] =
{
  0xfc, 0xe1, 0x7e, 0x7e,	/* push mof */
  0x04, 0x01, 0x30, 0x7a,	/* move [r0+4],mof */
  0x08, 0x01, 0x30, 0x09,	/* jump [r0+8] */
  0, 0, 0, 0, 0, 0, 0, 0,	/*  Pad out to 20 bytes.  */
};

/* Subsequent entries in a PIC procedure linkage table look like this.  */

static const bfd_byte elf_cris_pic_plt_entry[PLT_ENTRY_SIZE] =
{
  0x6f, 0x0d,   /*  (bdap [pc+].d,r0) */
  0, 0, 0, 0,	/*  Replaced with offset of this symbol in .got.  */
  0x30, 0x09,	/* jump [...] */
  0x3f, 0x7e,	/* move [pc+],mof */
  0, 0, 0, 0,	/*  Replaced with offset into relocation table.  */
  0x2f, 0xfe,	/* add.d [pc+],pc */
  0xec, 0xff,	/*  Replaced with offset to start of .plt.  */
  0xff, 0xff
};

/* We copy elf32-m68k.c and elf32-i386.c for the basic linker hash bits
   (and most other PIC/shlib stuff).  Check that we don't drift away
   without reason.

   The CRIS linker, like the m68k and i386 linkers (and probably the rest
   too) needs to keep track of the number of relocs that it decides to
   copy in check_relocs for each symbol.  This is so that it can discard
   PC relative relocs if it doesn't need them when linking with
   -Bsymbolic.  We store the information in a field extending the regular
   ELF linker hash table.  */

/* This structure keeps track of the number of PC relative relocs we have
   copied for a given symbol.  */

struct elf_cris_pcrel_relocs_copied
{
  /* Next section.  */
  struct elf_cris_pcrel_relocs_copied *next;
  /* A section in dynobj.  */
  asection *section;
  /* Number of relocs copied in this section.  */
  bfd_size_type count;
};

/* CRIS ELF linker hash entry.  */

struct elf_cris_link_hash_entry
{
  struct elf_link_hash_entry root;

  /* Number of PC relative relocs copied for this symbol.  */
  struct elf_cris_pcrel_relocs_copied *pcrel_relocs_copied;

  /* The GOTPLT references are CRIS-specific; the goal is to avoid having
     both a general GOT and a PLT-specific GOT entry for the same symbol,
     when it is referenced both as a function and as a function pointer.

     Number of GOTPLT references for a function.  */
  bfd_signed_vma gotplt_refcount;

  /* Actual GOTPLT index for this symbol, if applicable, or zero if not
     (zero is never used as an index).  FIXME: We should be able to fold
     this with gotplt_refcount in a union, like the got and plt unions in
     elf_link_hash_entry.  */
  bfd_size_type gotplt_offset;
};

/* CRIS ELF linker hash table.  */

struct elf_cris_link_hash_table
{
  struct elf_link_hash_table root;

  /* We can't use the PLT offset and calculate to get the GOTPLT offset,
     since we try and avoid creating GOTPLT:s when there's already a GOT.
     Instead, we keep and update the next available index here.  */
  bfd_size_type next_gotplt_entry;
};

/* Traverse a CRIS ELF linker hash table.  */

#define elf_cris_link_hash_traverse(table, func, info)			\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (bfd_boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func), \
    (info)))

/* Get the CRIS ELF linker hash table from a link_info structure.  */

#define elf_cris_hash_table(p) \
  ((struct elf_cris_link_hash_table *) (p)->hash)

/* Create an entry in a CRIS ELF linker hash table.  */

static struct bfd_hash_entry *
elf_cris_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf_cris_link_hash_entry *ret =
    (struct elf_cris_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct elf_cris_link_hash_entry *) NULL)
    ret = ((struct elf_cris_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct elf_cris_link_hash_entry)));
  if (ret == (struct elf_cris_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf_cris_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != (struct elf_cris_link_hash_entry *) NULL)
    {
      ret->pcrel_relocs_copied = NULL;
      ret->gotplt_refcount = 0;
      ret->gotplt_offset = 0;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create a CRIS ELF linker hash table.  */

static struct bfd_link_hash_table *
elf_cris_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf_cris_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct elf_cris_link_hash_table);

  ret = ((struct elf_cris_link_hash_table *) bfd_malloc (amt));
  if (ret == (struct elf_cris_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
				       elf_cris_link_hash_newfunc))
    {
      free (ret);
      return NULL;
    }

  /* Initialize to skip over the first three entries in the gotplt; they
     are used for run-time symbol evaluation.  */
  ret->next_gotplt_entry = 12;

  return &ret->root.root;
}

/* Perform a single relocation.  By default we use the standard BFD
   routines, with a few tweaks.  */

static bfd_reloc_status_type
cris_final_link_relocate (howto, input_bfd, input_section, contents, rel,
			  relocation)
     reloc_howto_type *  howto;
     bfd *               input_bfd;
     asection *          input_section;
     bfd_byte *          contents;
     Elf_Internal_Rela * rel;
     bfd_vma             relocation;
{
  bfd_reloc_status_type r;

  /* PC-relative relocations are relative to the position *after*
     the reloc.  Note that for R_CRIS_8_PCREL the adjustment is
     not a single byte, since PC must be 16-bit-aligned.  */
  switch (ELF32_R_TYPE (rel->r_info))
    {
      /* Check that the 16-bit GOT relocs are positive.  */
    case R_CRIS_16_GOTPLT:
    case R_CRIS_16_GOT:
      if ((bfd_signed_vma) relocation < 0)
	return bfd_reloc_overflow;
      break;

    case R_CRIS_32_PLT_PCREL:
    case R_CRIS_32_PCREL:
      relocation -= 2;
      /* Fall through.  */
    case R_CRIS_8_PCREL:
    case R_CRIS_16_PCREL:
      relocation -= 2;
      break;

    default:
      break;
    }

  r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				contents, rel->r_offset,
				relocation, rel->r_addend);
  return r;
}

/* Relocate an CRIS ELF section.  See elf32-fr30.c, from where this was
   copied, for further comments.  */

static bfd_boolean
cris_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_vma *local_got_offsets;
  asection *sgot;
  asection *splt;
  asection *sreloc;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  if (info->relocatable)
    return TRUE;

  dynobj = elf_hash_table (info)->dynobj;
  local_got_offsets = elf_local_got_offsets (input_bfd);
  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  sgot = NULL;
  splt = NULL;
  sreloc = NULL;

  if (dynobj != NULL)
    {
      splt = bfd_get_section_by_name (dynobj, ".plt");
      sgot = bfd_get_section_by_name (dynobj, ".got");
    }

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;
      const char *symname = NULL;
      int r_type;

      r_type = ELF32_R_TYPE (rel->r_info);

      if (   r_type == R_CRIS_GNU_VTINHERIT
	  || r_type == R_CRIS_GNU_VTENTRY)
	continue;

      /* This is a final link.  */
      r_symndx = ELF32_R_SYM (rel->r_info);
      howto  = cris_elf_howto_table + r_type;
      h      = NULL;
      sym    = NULL;
      sec    = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);

	  symname = (bfd_elf_string_from_elf_section
		     (input_bfd, symtab_hdr->sh_link, sym->st_name));
	  if (symname == NULL)
	    symname = bfd_section_name (input_bfd, sec);
	}
      else
	{
	  bfd_boolean warned;
	  bfd_boolean unresolved_reloc;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);

	  if (unresolved_reloc
	      /* Perhaps we should detect the cases that
		 sec->output_section is expected to be NULL like i386 and
		 m68k, but apparently (and according to elfxx-ia64.c) all
		 valid cases are where the symbol is defined in a shared
		 object which we link dynamically against.  This includes
		 PLT relocs for which we've created a PLT entry and other
		 relocs for which we're prepared to create dynamic
		 relocations.

		 For now, new situations cause us to just err when
		 sec->output_offset is NULL but the object with the symbol
		 is *not* dynamically linked against.  Thus this will
		 automatically remind us so we can see if there are other
		 valid cases we need to revisit.  */
	      && (sec->owner->flags & DYNAMIC) != 0)
	    relocation = 0;

	  else if (h->root.type == bfd_link_hash_defined
		   || h->root.type == bfd_link_hash_defweak)
	    {
	      /* Here follow the cases where the relocation value must
		 be zero (or when further handling is simplified when
		 zero).  I can't claim to understand the various
		 conditions and they weren't described in the files
		 where I copied them from (elf32-m68k.c and
		 elf32-i386.c), but let's mention examples of where
		 they happen.  FIXME: Perhaps define and use a
		 dynamic_symbol_p function like ia64.

		 - When creating a shared library, we can have an
		 ordinary relocation for a symbol defined in a shared
		 library (perhaps the one we create).  We then make
		 the relocation value zero, as the value seen now will
		 be added into the relocation addend in this shared
		 library, but must be handled only at dynamic-link
		 time.  FIXME: Not sure this example covers the
		 h->elf_link_hash_flags test, though it's there in
		 other targets.  */
	      if (info->shared
		  && ((! info->symbolic && h->dynindx != -1)
		      || (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)
		  && (input_section->flags & SEC_ALLOC) != 0
		  && (r_type == R_CRIS_8
		      || r_type == R_CRIS_16
		      || r_type == R_CRIS_32
		      || r_type == R_CRIS_8_PCREL
		      || r_type == R_CRIS_16_PCREL
		      || r_type == R_CRIS_32_PCREL))
		relocation = 0;
	      else if (unresolved_reloc)
		{
		  _bfd_error_handler
		    (_("%s: unresolvable relocation %s against symbol `%s' from %s section"),
		     bfd_archive_filename (input_bfd),
		     cris_elf_howto_table[r_type].name,
		     symname,
		     bfd_get_section_name (input_bfd, input_section));
		  bfd_set_error (bfd_error_bad_value);
		  return FALSE;
		}
	    }
	}

      switch (r_type)
	{
	case R_CRIS_16_GOTPLT:
	case R_CRIS_32_GOTPLT:
	  /* This is like the case for R_CRIS_32_GOT and R_CRIS_16_GOT,
	     but we require a PLT, and the PLT handling will take care of
	     filling in the PLT-specific GOT entry.  For the GOT offset,
	     calculate it as we do when filling it in for the .got.plt
	     section.  If we don't have a PLT, punt to GOT handling.  */
	  if (h != NULL
	      && ((struct elf_cris_link_hash_entry *) h)->gotplt_offset != 0)
	    {
	      asection *sgotplt
		= bfd_get_section_by_name (dynobj, ".got.plt");
	      bfd_vma got_offset;

	      BFD_ASSERT (h->dynindx != -1);
	      BFD_ASSERT (sgotplt != NULL);

	      got_offset
		= ((struct elf_cris_link_hash_entry *) h)->gotplt_offset;

	      relocation = got_offset;
	      break;
	    }

	  /* We didn't make a PLT entry for this symbol.  Maybe everything is
	     folded into the GOT.  Other than folding, this happens when
	     statically linking PIC code, or when using -Bsymbolic.  Check
	     that we instead have a GOT entry as done for us by
	     elf_cris_adjust_dynamic_symbol, and drop through into the
	     ordinary GOT cases.  This must not happen for the
	     executable, because any reference it does to a function
	     that is satisfied by a DSO must generate a PLT.  We assume
	     these call-specific relocs don't address non-functions.  */
	  if (h != NULL
	      && (h->got.offset == (bfd_vma) -1
		  || (!info->shared
		      && !((h->elf_link_hash_flags
			    & ELF_LINK_HASH_DEF_REGULAR) != 0
			   || ((h->elf_link_hash_flags
				& ELF_LINK_HASH_DEF_DYNAMIC) == 0
			      && h->root.type == bfd_link_hash_undefweak)))))
	    {
	      (*_bfd_error_handler)
		((h->got.offset == (bfd_vma) -1)
		 ? _("%s: No PLT nor GOT for relocation %s against\
 symbol `%s' from %s section")
		 : _("%s: No PLT for relocation %s against\
 symbol `%s' from %s section"),
		 bfd_archive_filename (input_bfd),
		 cris_elf_howto_table[r_type].name,
		 symname[0] != '\0' ? symname : _("[whose name is lost]"),
		 bfd_get_section_name (input_bfd, input_section));

	      /* FIXME: Perhaps blaming input is not the right thing to
		 do; this is probably an internal error.  But it is true
		 that we didn't like that particular input.  */
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }
	  /* Fall through.  */

	  /* The size of the actual relocation is not used here; we only
	     fill in the GOT table here.  */
	case R_CRIS_16_GOT:
	case R_CRIS_32_GOT:
	  {
	    bfd_vma off;

	    /* Note that despite using RELA relocations, the .got contents
	       is always filled in with the link-relative relocation
	       value; the addend.  */

	    if (h != NULL)
	      {
		off = h->got.offset;
		BFD_ASSERT (off != (bfd_vma) -1);

		if (!elf_hash_table (info)->dynamic_sections_created
		    || (! info->shared
			&& ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)
			    || h->type == STT_FUNC
			    || (h->elf_link_hash_flags
				& ELF_LINK_HASH_NEEDS_PLT)))
		    || (info->shared
			&& (info->symbolic || h->dynindx == -1)
			&& (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
		  {
		    /* This wasn't checked above for ! info->shared, but
		       must hold there if we get here; the symbol must
		       be defined in the regular program or be undefweak
		       or be a function or otherwise need a PLT.  */
		    BFD_ASSERT (!elf_hash_table (info)->dynamic_sections_created
				|| info->shared
				|| (h->elf_link_hash_flags
				    & ELF_LINK_HASH_DEF_REGULAR) != 0
				|| h->type == STT_FUNC
				|| (h->elf_link_hash_flags
				    & ELF_LINK_HASH_NEEDS_PLT)
				|| h->root.type == bfd_link_hash_undefweak);

		    /* This is actually a static link, or it is a
		       -Bsymbolic link and the symbol is defined locally,
		       or is undefweak, or the symbol was forced to be
		       local because of a version file, or we're not
		       creating a dynamic object.  We must initialize this
		       entry in the global offset table.  Since the offset
		       must always be a multiple of 4, we use the least
		       significant bit to record whether we have
		       initialized it already.

		       If this GOT entry should be runtime-initialized, we
		       will create a .rela.got relocation entry to
		       initialize the value.  This is done in the
		       finish_dynamic_symbol routine.  */
		    if ((off & 1) != 0)
		      off &= ~1;
		    else
		      {
			bfd_put_32 (output_bfd, relocation,
				    sgot->contents + off);
			h->got.offset |= 1;
		      }
		  }
	      }
	    else
	      {
		BFD_ASSERT (local_got_offsets != NULL
			    && local_got_offsets[r_symndx] != (bfd_vma) -1);

		off = local_got_offsets[r_symndx];

		/* The offset must always be a multiple of 4.  We use
		   the least significant bit to record whether we have
		   already generated the necessary reloc.  */
		if ((off & 1) != 0)
		  off &= ~1;
		else
		  {
		    bfd_put_32 (output_bfd, relocation, sgot->contents + off);

		    if (info->shared)
		      {
			asection *s;
			Elf_Internal_Rela outrel;
			bfd_byte *loc;

			s = bfd_get_section_by_name (dynobj, ".rela.got");
			BFD_ASSERT (s != NULL);

			outrel.r_offset = (sgot->output_section->vma
					   + sgot->output_offset
					   + off);
			outrel.r_info = ELF32_R_INFO (0, R_CRIS_RELATIVE);
			outrel.r_addend = relocation;
			loc = s->contents;
			loc += s->reloc_count++ * sizeof (Elf32_External_Rela);
			bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);
		      }

		    local_got_offsets[r_symndx] |= 1;
		  }
	      }

	    relocation = sgot->output_offset + off;
	    if (rel->r_addend != 0)
	      {
		/* We can't do anything for a relocation which is against
		   a symbol *plus offset*.  GOT holds relocations for
		   symbols.  Make this an error; the compiler isn't
		   allowed to pass us these kinds of things.  */
		if (h == NULL)
		  (*_bfd_error_handler)
		    (_("%s: relocation %s with non-zero addend %d against local symbol from %s section"),
		     bfd_archive_filename (input_bfd),
		     cris_elf_howto_table[r_type].name,
		     rel->r_addend,
		     bfd_get_section_name (input_bfd, input_section));
		else
		  (*_bfd_error_handler)
		    (_("%s: relocation %s with non-zero addend %d against symbol `%s' from %s section"),
		     bfd_archive_filename (input_bfd),
		     cris_elf_howto_table[r_type].name,
		     rel->r_addend,
		     symname[0] != '\0' ? symname : _("[whose name is lost]"),
		     bfd_get_section_name (input_bfd, input_section));

		bfd_set_error (bfd_error_bad_value);
		return FALSE;
	      }
	  }
	  break;

	case R_CRIS_32_GOTREL:
	  /* This relocation must only be performed against local symbols.
	     It's also ok when we link a program and the symbol is either
	     defined in an ordinary (non-DSO) object or is undefined weak.  */
	  if (h != NULL
	      && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT
	      && !(!info->shared
		   && ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
		       || ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
			   && h->root.type == bfd_link_hash_undefweak))))
	    {
	      (*_bfd_error_handler)
		(_("%s: relocation %s is not allowed for global symbol: `%s' from %s section"),
		 bfd_archive_filename (input_bfd),
		 cris_elf_howto_table[r_type].name,
		 symname,
		 bfd_get_section_name (input_bfd, input_section));
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }

	  /* This can happen if we get a link error with the input ELF
	     variant mismatching the output variant.  Emit an error so
	     it's noticed if it happens elsewhere.  */
	  if (sgot == NULL)
	    {
	      (*_bfd_error_handler)
		(_("%s: relocation %s in section %s with no GOT created"),
		 bfd_archive_filename (input_bfd),
		 cris_elf_howto_table[r_type].name,
		 bfd_get_section_name (input_bfd, input_section));
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }

	  /* This relocation is like a PC-relative one, except the
	     reference point is the location of GOT.  Note that
	     sgot->output_offset is not involved in this calculation.  We
	     always want the start of entire .got section, not the
	     position after the reserved header.  */
	  relocation -= sgot->output_section->vma;
	  break;

	case R_CRIS_32_PLT_PCREL:
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */

	  /* Resolve a PLT_PCREL reloc against a local symbol directly,
	     without using the procedure linkage table.  */
	  if (h == NULL || ELF_ST_VISIBILITY (h->other) != STV_DEFAULT)
	    break;

	  if (h->plt.offset == (bfd_vma) -1
	      || splt == NULL)
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

	case R_CRIS_32_PLT_GOTREL:
	  /* Like R_CRIS_32_PLT_PCREL, but the reference point is the
	     start of the .got section.  See also comment at
	     R_CRIS_32_GOT.  */
	  relocation -= sgot->output_section->vma;

	  /* Resolve a PLT_GOTREL reloc against a local symbol directly,
	     without using the procedure linkage table.  */
	  if (h == NULL || ELF_ST_VISIBILITY (h->other) != STV_DEFAULT)
	    break;

	  if (h->plt.offset == (bfd_vma) -1
	      || splt == NULL)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
		 happens when statically linking PIC code, or when
		 using -Bsymbolic.  */
	      break;
	    }

	  relocation = (splt->output_section->vma
			+ splt->output_offset
			+ h->plt.offset
			- sgot->output_section->vma);
	  break;

	case R_CRIS_8_PCREL:
	case R_CRIS_16_PCREL:
	case R_CRIS_32_PCREL:
	  /* If the symbol was local, we need no shlib-specific handling.  */
	  if (h == NULL || ELF_ST_VISIBILITY (h->other) != STV_DEFAULT)
	    break;

	  /* Fall through.  */
	case R_CRIS_8:
	case R_CRIS_16:
	case R_CRIS_32:
	  if (info->shared
	      && r_symndx != 0
	      && (input_section->flags & SEC_ALLOC) != 0
	      && ((r_type != R_CRIS_8_PCREL
		   && r_type != R_CRIS_16_PCREL
		   && r_type != R_CRIS_32_PCREL)
		  || (!info->symbolic
		      || (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)))
	    {
	      Elf_Internal_Rela outrel;
	      bfd_byte *loc;
	      bfd_boolean skip, relocate;

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

		  /* That section should have been created in
		     cris_elf_check_relocs, but that function will not be
		     called for objects which fail in
		     cris_elf_merge_private_bfd_data.  */
		  if (sreloc == NULL)
		    {
		      (*_bfd_error_handler)
			(_("%s: Internal inconsistency; no relocation section %s"),
			 bfd_archive_filename (input_bfd),
			 name);

		      bfd_set_error (bfd_error_bad_value);
		      return FALSE;
		    }
		}

	      skip = FALSE;
	      relocate = FALSE;

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info, input_section,
					 rel->r_offset);
	      if (outrel.r_offset == (bfd_vma) -1)
		skip = TRUE;
	      else if (outrel.r_offset == (bfd_vma) -2)
		skip = TRUE, relocate = TRUE;
	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);

	      if (skip)
		memset (&outrel, 0, sizeof outrel);
	      /* h->dynindx may be -1 if the symbol was marked to
		 become local.  */
	      else if (h != NULL
		       && ((! info->symbolic && h->dynindx != -1)
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))
		{
		  BFD_ASSERT (h->dynindx != -1);
		  outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
		  outrel.r_addend = relocation + rel->r_addend;
		}
	      else
		{
		  if (r_type == R_CRIS_32)
		    {
		      relocate = TRUE;
		      outrel.r_info = ELF32_R_INFO (0, R_CRIS_RELATIVE);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		  else
		    {
		      long indx;

		      if (bfd_is_abs_section (sec))
			indx = 0;
		      else if (sec == NULL || sec->owner == NULL)
			{
			  bfd_set_error (bfd_error_bad_value);
			  return FALSE;
			}
		      else
			{
			  asection *osec;

			  osec = sec->output_section;
			  indx = elf_section_data (osec)->dynindx;
			  BFD_ASSERT (indx > 0);
			}

		      outrel.r_info = ELF32_R_INFO (indx, r_type);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		}

	      loc = sreloc->contents;
	      loc += sreloc->reloc_count++ * sizeof (Elf32_External_Rela);
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);

	      /* This reloc will be computed at runtime, so there's no
                 need to do anything now, except for R_CRIS_32 relocations
                 that have been turned into R_CRIS_RELATIVE.  */
	      if (!relocate)
		continue;
	    }

	  break;
	}

      r = cris_final_link_relocate (howto, input_bfd, input_section,
				     contents, rel, relocation);

      if (r != bfd_reloc_ok)
	{
	  const char * msg = (const char *) NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		(info, symname, howto->name, (bfd_vma) 0,
		 input_bfd, input_section, rel->r_offset);
	      break;

	    case bfd_reloc_undefined:
	      r = info->callbacks->undefined_symbol
		(info, symname, input_bfd, input_section, rel->r_offset,
		 TRUE);
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
	      (info, msg, symname, input_bfd, input_section, rel->r_offset);

	  if (! r)
	    return FALSE;
	}
    }

  return TRUE;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static bfd_boolean
elf_cris_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj;
  int plt_off1 = 2, plt_off2 = 10, plt_off3 = 16;

  dynobj = elf_hash_table (info)->dynobj;

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *sgotplt;
      asection *sgot;
      asection *srela;
      bfd_vma got_base;

      bfd_vma gotplt_offset
	= ((struct elf_cris_link_hash_entry *) h)->gotplt_offset;
      Elf_Internal_Rela rela;
      bfd_byte *loc;
      bfd_boolean has_gotplt = gotplt_offset != 0;

      /* Get the index in the procedure linkage table which
	 corresponds to this symbol.  This is the index of this symbol
	 in all the symbols for which we are making plt entries.  The
	 first entry in the procedure linkage table is reserved.  */
      /* We have to count backwards here, and the result is only valid as
	 an index into .got.plt and its relocations.  FIXME: Constants...  */
      bfd_vma gotplt_index = gotplt_offset/4 - 3;

      /* Get the offset into the .got table of the entry that corresponds
	 to this function.  Note that we embed knowledge that "incoming"
	 .got goes after .got.plt in the output without padding (pointer
	 aligned).  However, that knowledge is present in several other
	 places too.  */
      bfd_vma got_offset
	= (has_gotplt
	   ? gotplt_offset
	   : h->got.offset + elf_cris_hash_table(info)->next_gotplt_entry);

      /* This symbol has an entry in the procedure linkage table.  Set it
	 up.  */

      BFD_ASSERT (h->dynindx != -1);

      splt = bfd_get_section_by_name (dynobj, ".plt");
      sgot = bfd_get_section_by_name (dynobj, ".got");
      sgotplt = bfd_get_section_by_name (dynobj, ".got.plt");
      srela = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (splt != NULL && sgotplt != NULL
		  && (! has_gotplt || srela != NULL));

      got_base = sgotplt->output_section->vma + sgotplt->output_offset;

      /* Fill in the entry in the procedure linkage table.  */
      if (! info->shared)
	{
	  memcpy (splt->contents + h->plt.offset, elf_cris_plt_entry,
		  PLT_ENTRY_SIZE);

	  /* We need to enter the absolute address of the GOT entry here.  */
	  bfd_put_32 (output_bfd, got_base + got_offset,
		      splt->contents + h->plt.offset + plt_off1);
	}
      else
	{
	  memcpy (splt->contents + h->plt.offset, elf_cris_pic_plt_entry,
		  PLT_ENTRY_SIZE);
	  bfd_put_32 (output_bfd, got_offset,
		      splt->contents + h->plt.offset + plt_off1);
	}

      /* Fill in the plt entry and make a relocation, if this is a "real"
	 PLT entry.  */
      if (has_gotplt)
	{
	  /* Fill in the offset into the reloc table.  */
	  bfd_put_32 (output_bfd,
		      gotplt_index * sizeof (Elf32_External_Rela),
		      splt->contents + h->plt.offset + plt_off2);

	  /* Fill in the offset to the first PLT entry, where to "jump".  */
	  bfd_put_32 (output_bfd, - (h->plt.offset + plt_off3 + 4),
		      splt->contents + h->plt.offset + plt_off3);

	  /* Fill in the entry in the global offset table with the address of
	     the relocating stub.  */
	  bfd_put_32 (output_bfd,
		      (splt->output_section->vma
		       + splt->output_offset
		       + h->plt.offset
		       + 8),
		      sgotplt->contents + got_offset);

	  /* Fill in the entry in the .rela.plt section.  */
	  rela.r_offset = (sgotplt->output_section->vma
			   + sgotplt->output_offset
			   + got_offset);
	  rela.r_info = ELF32_R_INFO (h->dynindx, R_CRIS_JUMP_SLOT);
	  rela.r_addend = 0;
	  loc = srela->contents + gotplt_index * sizeof (Elf32_External_Rela);
	  bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
	}

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;

	  /* FIXME: From elf32-sparc.c 2001-02-19 (1.18).  I still don't
	     know whether resetting the value is significant; if it really
	     is, rather than a quirk or bug in the sparc port, then I
	     believe we'd see this elsewhere.  */
	  /* If the symbol is weak, we do need to clear the value.
	     Otherwise, the PLT entry would provide a definition for
	     the symbol even if the symbol wasn't defined anywhere,
	     and so the symbol would never be NULL.  */
	  if ((h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR_NONWEAK)
	      == 0)
	    sym->st_value = 0;
	}
    }

  /* For an ordinary program, we emit .got relocs only for symbols that
     are in the dynamic-symbols table and are either defined by the
     program or are undefined weak symbols, or are function symbols
     where we do not output a PLT: the PLT reloc was output above and all
     references to the function symbol are redirected to the PLT.  */
  if (h->got.offset != (bfd_vma) -1
      && (info->shared
	  || (h->dynindx != -1
	      && h->plt.offset == (bfd_vma) -1
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
	      && h->root.type != bfd_link_hash_undefweak)))
    {
      asection *sgot;
      asection *srela;
      Elf_Internal_Rela rela;
      bfd_byte *loc;
      bfd_byte *where;

      /* This symbol has an entry in the global offset table.  Set it up.  */

      sgot = bfd_get_section_by_name (dynobj, ".got");
      srela = bfd_get_section_by_name (dynobj, ".rela.got");
      BFD_ASSERT (sgot != NULL && srela != NULL);

      rela.r_offset = (sgot->output_section->vma
		       + sgot->output_offset
		       + (h->got.offset &~ (bfd_vma) 1));

      /* If this is a static link, or it is a -Bsymbolic link and the
	 symbol is defined locally or was forced to be local because
	 of a version file, we just want to emit a RELATIVE reloc.
	 The entry in the global offset table will already have been
	 initialized in the relocate_section function.  */
      where = sgot->contents + (h->got.offset &~ (bfd_vma) 1);
      if (! elf_hash_table (info)->dynamic_sections_created
	  || (info->shared
	      && (info->symbolic || h->dynindx == -1)
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
	{
	  rela.r_info = ELF32_R_INFO (0, R_CRIS_RELATIVE);
	  rela.r_addend = bfd_get_signed_32 (output_bfd, where);
	}
      else
	{
	  bfd_put_32 (output_bfd, (bfd_vma) 0, where);
	  rela.r_info = ELF32_R_INFO (h->dynindx, R_CRIS_GLOB_DAT);
	  rela.r_addend = 0;
	}

      loc = srela->contents;
      loc += srela->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
    {
      asection *s;
      Elf_Internal_Rela rela;
      bfd_byte *loc;

      /* This symbol needs a copy reloc.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1
		  && (h->root.type == bfd_link_hash_defined
		      || h->root.type == bfd_link_hash_defweak));

      s = bfd_get_section_by_name (h->root.u.def.section->owner,
				   ".rela.bss");
      BFD_ASSERT (s != NULL);

      rela.r_offset = (h->root.u.def.value
		       + h->root.u.def.section->output_section->vma
		       + h->root.u.def.section->output_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_CRIS_COPY);
      rela.r_addend = 0;
      loc = s->contents + s->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rela, loc);
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return TRUE;
}

/* Finish up the dynamic sections.  */

static bfd_boolean
elf_cris_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sgot;
  asection *sdyn;

  dynobj = elf_hash_table (info)->dynobj;

  sgot = bfd_get_section_by_name (dynobj, ".got.plt");
  BFD_ASSERT (sgot != NULL);
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      asection *splt;
      Elf32_External_Dyn *dyncon, *dynconend;

      splt = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (splt != NULL && sdyn != NULL);

      dyncon = (Elf32_External_Dyn *) sdyn->contents;
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  asection *s;

	  bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;

	    case DT_PLTGOT:
	      s = bfd_get_section_by_name (output_bfd, ".got");
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->vma;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_JMPREL:
	      /* Yes, we *can* have a .plt and no .plt.rela, for instance
		 if all symbols are found in the .got (not .got.plt).  */
	      s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	      dyn.d_un.d_ptr = s != NULL ? s->vma : 0;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_PLTRELSZ:
	      s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	      if (s == NULL)
		dyn.d_un.d_val = 0;
	      else if (s->_cooked_size != 0)
		dyn.d_un.d_val = s->_cooked_size;
	      else
		dyn.d_un.d_val = s->_raw_size;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_RELASZ:
	      /* The procedure linkage table relocs (DT_JMPREL) should
		 not be included in the overall relocs (DT_RELA).
		 Therefore, we override the DT_RELASZ entry here to
		 make it not include the JMPREL relocs.  Since the
		 linker script arranges for .rela.plt to follow all
		 other relocation sections, we don't have to worry
		 about changing the DT_RELA entry.  */
	      s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	      if (s != NULL)
		{
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
      if (splt->_raw_size > 0)
	{
	  if (info->shared)
	    memcpy (splt->contents, elf_cris_pic_plt0_entry, PLT_ENTRY_SIZE);
	  else
            {
	      memcpy (splt->contents, elf_cris_plt0_entry, PLT_ENTRY_SIZE);
	      bfd_put_32 (output_bfd,
			  sgot->output_section->vma + sgot->output_offset + 4,
			  splt->contents + 6);
	      bfd_put_32 (output_bfd,
			  sgot->output_section->vma + sgot->output_offset + 8,
			  splt->contents + 14);

              elf_section_data (splt->output_section)->this_hdr.sh_entsize
               = PLT_ENTRY_SIZE;
            }
	}
    }

  /* Fill in the first three entries in the global offset table.  */
  if (sgot->_raw_size > 0)
    {
      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    sgot->contents);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + 4);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + 8);
    }

  elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 4;

  return TRUE;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
cris_elf_gc_mark_hook (sec, info, rel, h, sym)
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
	case R_CRIS_GNU_VTINHERIT:
	case R_CRIS_GNU_VTENTRY:
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

/* Update the got entry reference counts for the section being removed.  */

static bfd_boolean
cris_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;
  bfd *dynobj;
  asection *sgot;
  asection *srelgot;

  dynobj = elf_hash_table (info)->dynobj;
  if (dynobj == NULL)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  sgot = bfd_get_section_by_name (dynobj, ".got");
  srelgot = bfd_get_section_by_name (dynobj, ".rela.got");

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_CRIS_16_GOT:
	case R_CRIS_32_GOT:
	  r_symndx = ELF32_R_SYM (rel->r_info);
	  if (r_symndx >= symtab_hdr->sh_info)
	    {
	      h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	      if (h->got.refcount > 0)
		{
		  --h->got.refcount;
		  if (h->got.refcount == 0)
		    {
		      /* We don't need the .got entry any more.  */
		      sgot->_raw_size -= 4;
		      srelgot->_raw_size -= sizeof (Elf32_External_Rela);
		    }
		}
	      break;
	    }

	local_got_reloc:
	  if (local_got_refcounts != NULL)
	    {
	      if (local_got_refcounts[r_symndx] > 0)
		{
		  --local_got_refcounts[r_symndx];
		  if (local_got_refcounts[r_symndx] == 0)
		    {
		      /* We don't need the .got entry any more.  */
		      sgot->_raw_size -= 4;
		      if (info->shared)
			srelgot->_raw_size -= sizeof (Elf32_External_Rela);
		    }
		}
	    }
	  break;

	case R_CRIS_16_GOTPLT:
	case R_CRIS_32_GOTPLT:
	  /* For local symbols, treat these like GOT relocs.  */
	  r_symndx = ELF32_R_SYM (rel->r_info);
	  if (r_symndx < symtab_hdr->sh_info)
	    goto local_got_reloc;
	  /* Fall through.  */
	case R_CRIS_32_PLT_GOTREL:
	  /* FIXME: We don't garbage-collect away the .got section.  */
	  if (local_got_refcounts != NULL)
	    local_got_refcounts[-1]--;
	  /* Fall through.  */
	case R_CRIS_8_PCREL:
	case R_CRIS_16_PCREL:
	case R_CRIS_32_PCREL:
	case R_CRIS_32_PLT_PCREL:
	  r_symndx = ELF32_R_SYM (rel->r_info);
	  if (r_symndx >= symtab_hdr->sh_info)
	    {
	      h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	      if (ELF_ST_VISIBILITY (h->other) == STV_DEFAULT
		  && h->plt.refcount > 0)
		--h->plt.refcount;
	    }
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}

/* Make sure we emit a GOT entry if the symbol was supposed to have a PLT
   entry but we found we will not create any.  Called when we find we will
   not have any PLT for this symbol, by for example
   elf_cris_adjust_dynamic_symbol when we're doing a proper dynamic link,
   or elf_cris_size_dynamic_sections if no dynamic sections will be
   created (we're only linking static objects).  */

static bfd_boolean
elf_cris_adjust_gotplt_to_got (h, p)
     struct elf_cris_link_hash_entry *h;
     PTR p;
{
  struct bfd_link_info *info = (struct bfd_link_info *) p;
  bfd *dynobj = elf_hash_table (info)->dynobj;

  BFD_ASSERT (dynobj != NULL);

  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct elf_cris_link_hash_entry *) h->root.root.u.i.link;

  /* If nobody wanted a GOTPLT with this symbol, we're done.  */
  if (h->gotplt_refcount <= 0)
    return TRUE;

  if (h->root.got.refcount > 0)
    {
      /* There's a GOT entry for this symbol.  Just adjust the refcount.
	 Probably not necessary at this stage, but keeping it accurate
	 helps avoiding surprises later.  */
      h->root.got.refcount += h->gotplt_refcount;
      h->gotplt_refcount = -1;
    }
  else
    {
      /* No GOT entry for this symbol.  We need to create one.  */
      asection *sgot = bfd_get_section_by_name (dynobj, ".got");
      asection *srelgot
	= bfd_get_section_by_name (dynobj, ".rela.got");

      /* Put an accurate refcount there.  */
      h->root.got.refcount = h->gotplt_refcount;

      h->gotplt_refcount = -1;

      /* We always have a .got and a .rela.got section if there were
	 GOTPLT relocs in input.  */
      BFD_ASSERT (sgot != NULL && srelgot != NULL);

      /* Allocate space in the .got section.  */
      sgot->_raw_size += 4;

      /* Allocate relocation space.  */
      srelgot->_raw_size += sizeof (Elf32_External_Rela);
    }

  return TRUE;
}

/* Try to fold PLT entries with GOT entries.  There are two cases when we
   want to do this:

   - When all PLT references are GOTPLT references, and there are GOT
     references, and this is not the executable.  We don't have to
     generate a PLT at all.

   - When there are both (ordinary) PLT references and GOT references,
     and this isn't the executable.
     We want to make the PLT reference use the ordinary GOT entry rather
     than R_CRIS_JUMP_SLOT, a run-time dynamically resolved GOTPLT entry,
     since the GOT entry will have to be resolved at startup anyway.

   Though the latter case is handled when room for the PLT is allocated,
   not here.

   By folding into the GOT, we may need a round-trip to a PLT in the
   executable for calls, a loss in performance.  Still, losing a
   reloc is a win in size and at least in start-up time.

   Note that this function is called before symbols are forced local by
   version scripts.  The differing cases are handled by
   elf_cris_hide_symbol.  */

static bfd_boolean
elf_cris_try_fold_plt_to_got (h, p)
     struct elf_cris_link_hash_entry *h;
     PTR p;
{
  struct bfd_link_info *info = (struct bfd_link_info *) p;

  /* If there are no GOT references for this symbol, we can't fold any
     other reference so there's nothing to do.  Likewise if there are no
     PLT references; GOTPLT references included.  */
  if (h->root.got.refcount <= 0 || h->root.plt.refcount <= 0)
    return TRUE;

  /* GOTPLT relocs are supposed to be included into the PLT refcount.  */
  BFD_ASSERT (h->gotplt_refcount <= h->root.plt.refcount);

  if (h->gotplt_refcount == h->root.plt.refcount)
    {
      /* The only PLT references are GOTPLT references, and there are GOT
	 references.  Convert PLT to GOT references.  */
      if (! elf_cris_adjust_gotplt_to_got (h, info))
	return FALSE;

      /* Clear the PLT references, so no PLT will be created.  */
      h->root.plt.offset = (bfd_vma) -1;
    }

  return TRUE;
}

/* Our own version of hide_symbol, so that we can adjust a GOTPLT reloc
   to use a GOT entry (and create one) rather than requiring a GOTPLT
   entry.  */

static void
elf_cris_hide_symbol (info, h, force_local)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     bfd_boolean force_local;
{
  elf_cris_adjust_gotplt_to_got ((struct elf_cris_link_hash_entry *) h, info);

  _bfd_elf_link_hash_hide_symbol (info, h, force_local);
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static bfd_boolean
elf_cris_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj;
  asection *s;
  unsigned int power_of_two;

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
      /* If we link a program (not a DSO), we'll get rid of unnecessary
	 PLT entries; we point to the actual symbols -- even for pic
	 relocs, because a program built with -fpic should have the same
	 result as one built without -fpic, specifically considering weak
	 symbols.
	 FIXME: m68k and i386 differ here, for unclear reasons.  */
      if (! info->shared
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0)
	{
	  /* This case can occur if we saw a PLT reloc in an input file,
	     but the symbol was not defined by a dynamic object.  In such
	     a case, we don't actually need to build a procedure linkage
	     table, and we can just do an absolute or PC reloc instead, or
	     change a .got.plt index to a .got index for GOTPLT relocs.  */
	  BFD_ASSERT ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0);
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	  h->plt.offset = (bfd_vma) -1;
	  return
	    elf_cris_adjust_gotplt_to_got ((struct
					    elf_cris_link_hash_entry *) h,
					   info);
	}

      /* If we had a R_CRIS_GLOB_DAT that didn't have to point to a PLT;
	 where a pointer-equivalent symbol was unimportant (i.e. more
	 like R_CRIS_JUMP_SLOT after symbol evaluation) we could get rid
	 of the PLT.  We can't for the executable, because the GOT
	 entries will point to the PLT there (and be constant).  */
      if (info->shared
	  && !elf_cris_try_fold_plt_to_got ((struct elf_cris_link_hash_entry*)
					    h, info))
	return FALSE;

      /* GC or folding may have rendered this entry unused.  */
      if (h->plt.refcount <= 0)
	{
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	  h->plt.offset = (bfd_vma) -1;
	  return TRUE;
	}

      /* Make sure this symbol is output as a dynamic symbol.  */
      if (h->dynindx == -1)
	{
	  if (! bfd_elf_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);

      /* If this is the first .plt entry, make room for the special
	 first entry.  */
      if (s->_raw_size == 0)
	s->_raw_size += PLT_ENTRY_SIZE;

      /* If this symbol is not defined in a regular file, and we are
	 not generating a shared library, then set the symbol to this
	 location in the .plt.  */
      if (!info->shared
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  h->root.u.def.section = s;
	  h->root.u.def.value = s->_raw_size;
	}

      /* If there's already a GOT entry, use that, not a .got.plt.  A
	 GOT field still has a reference count when we get here; it's
	 not yet changed to an offset.  We can't do this for an
	 executable, because then the reloc associated with the PLT
	 would get a non-PLT reloc pointing to the PLT.  FIXME: Move
	 this to elf_cris_try_fold_plt_to_got.  */
      if (info->shared && h->got.refcount > 0)
	{
	  h->got.refcount += h->plt.refcount;

	  /* Mark the PLT offset to use the GOT entry by setting the low
	     bit in the plt offset; it is always a multiple of
	     pointer-size.  */
	  BFD_ASSERT ((s->_raw_size & 3) == 0);

	  /* Change the PLT refcount to an offset.  */
	  h->plt.offset = s->_raw_size;

	  /* By not setting gotplt_offset (i.e. it remains at 0), we signal
	     that the got entry should be used instead.  */
	  BFD_ASSERT (((struct elf_cris_link_hash_entry *)
		       h)->gotplt_offset == 0);

	  /* Make room for this entry.  */
	  s->_raw_size += PLT_ENTRY_SIZE;

	  return TRUE;
	}

      /* No GOT reference for this symbol; prepare for an ordinary PLT.  */
      h->plt.offset = s->_raw_size;

      /* Make room for this entry.  */
      s->_raw_size += PLT_ENTRY_SIZE;

      /* We also need to make an entry in the .got.plt section, which
	 will be placed in the .got section by the linker script.  */
      ((struct elf_cris_link_hash_entry *) h)->gotplt_offset
	= elf_cris_hash_table (info)->next_gotplt_entry;
      elf_cris_hash_table (info)->next_gotplt_entry += 4;

      s = bfd_get_section_by_name (dynobj, ".got.plt");
      BFD_ASSERT (s != NULL);
      s->_raw_size += 4;

      /* We also need to make an entry in the .rela.plt section.  */

      s = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (s != NULL);
      s->_raw_size += sizeof (Elf32_External_Rela);

      return TRUE;
    }

  /* Reinitialize the plt offset now that it is not used as a reference
     count any more.  */
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

  /* We must allocate the symbol in our .dynbss section, which will
     become part of the .bss section of the executable.  There will be
     an entry for this symbol in the .dynsym section.  The dynamic
     object will contain position independent code, so all references
     from the dynamic object to this symbol will go through the global
     offset table.  The dynamic linker will use the .dynsym entry to
     determine the address it must put in the global offset table, so
     both the dynamic object and the regular object will refer to the
     same memory location for the variable.  */

  s = bfd_get_section_by_name (dynobj, ".dynbss");
  BFD_ASSERT (s != NULL);

  /* We must generate a R_CRIS_COPY reloc to tell the dynamic linker to
     copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rela.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      srel = bfd_get_section_by_name (dynobj, ".rela.bss");
      BFD_ASSERT (srel != NULL);
      srel->_raw_size += sizeof (Elf32_External_Rela);
      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_COPY;
    }

  /* Historic precedent: m68k and i386 allow max 8-byte alignment for the
     thing to copy; so do we.  */

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
      if (!bfd_set_section_alignment (dynobj, s, power_of_two))
	return FALSE;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return TRUE;
}

/* Look through the relocs for a section during the first phase.  */

static bfd_boolean
cris_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sgot;
  asection *srelgot;
  asection *sreloc;

  if (info->relocatable)
    return TRUE;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof (Elf32_External_Sym);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  sgot = NULL;
  srelgot = NULL;
  sreloc = NULL;

  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;
      enum elf_cris_reloc_type r_type;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
        h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      r_type = ELF32_R_TYPE (rel->r_info);

      /* Some relocs require linker-created sections; we need to hang them
	 on the first input bfd we found that contained dynamic relocs.  */
      switch (r_type)
	{
	case R_CRIS_16_GOT:
	case R_CRIS_32_GOT:
	case R_CRIS_32_GOTREL:
	case R_CRIS_32_PLT_GOTREL:
	case R_CRIS_32_PLT_PCREL:
	case R_CRIS_16_GOTPLT:
	case R_CRIS_32_GOTPLT:
	  if (dynobj == NULL)
	    {
	      elf_hash_table (info)->dynobj = dynobj = abfd;

	      /* Create the .got section, so we can assume it's always
		 present whenever there's a dynobj.  */
	      if (!_bfd_elf_create_got_section (dynobj, info))
		return FALSE;
	    }
	  break;

	default:
	  break;
	}

      /* Some relocs require a global offset table (but perhaps not a
	 specific GOT entry).  */
      switch (r_type)
	{
	  /* For R_CRIS_16_GOTPLT and R_CRIS_32_GOTPLT, we need a GOT
	     entry only for local symbols.  Unfortunately, we don't know
	     until later on if there's a version script that forces the
	     symbol local.  We must have the .rela.got section in place
	     before we know if the symbol looks global now, so we need
	     to treat the reloc just like for R_CRIS_16_GOT and
	     R_CRIS_32_GOT.  */
	case R_CRIS_16_GOTPLT:
	case R_CRIS_32_GOTPLT:
	case R_CRIS_16_GOT:
	case R_CRIS_32_GOT:
	  if (srelgot == NULL
	      && (h != NULL || info->shared))
	    {
	      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
	      if (srelgot == NULL)
		{
		  srelgot = bfd_make_section (dynobj, ".rela.got");
		  if (srelgot == NULL
		      || !bfd_set_section_flags (dynobj, srelgot,
						 (SEC_ALLOC
						  | SEC_LOAD
						  | SEC_HAS_CONTENTS
						  | SEC_IN_MEMORY
						  | SEC_LINKER_CREATED
						  | SEC_READONLY))
		      || !bfd_set_section_alignment (dynobj, srelgot, 2))
		    return FALSE;
		}
	    }
	  /* Fall through.  */

	case R_CRIS_32_GOTREL:
	case R_CRIS_32_PLT_GOTREL:
	  if (sgot == NULL)
	    sgot = bfd_get_section_by_name (dynobj, ".got");

	  if (local_got_refcounts == NULL)
	    {
	      bfd_size_type amt;

	      /* We use index local_got_refcounts[-1] to count all
		 GOT-relative relocations that do not have explicit
		 GOT entries.  */
	      amt = symtab_hdr->sh_info + 1;
	      amt *= sizeof (bfd_signed_vma);
	      local_got_refcounts = ((bfd_signed_vma *) bfd_zalloc (abfd, amt));
	      if (local_got_refcounts == NULL)
		return FALSE;

	      local_got_refcounts++;
	      elf_local_got_refcounts (abfd) = local_got_refcounts;
	    }
	  break;

	default:
	  break;
	}

      switch (r_type)
        {
	case R_CRIS_16_GOTPLT:
	case R_CRIS_32_GOTPLT:
	  /* Mark that we need a GOT entry if the PLT entry (and its GOT
	     entry) is eliminated.  We can only do this for a non-local
	     symbol.  */
	  if (h != NULL)
	    {
	      ((struct elf_cris_link_hash_entry *) h)->gotplt_refcount++;
	      goto handle_gotplt_reloc;
	    }
	  /* If h is NULL then this is a local symbol, and we must make a
	     GOT entry for it, so handle it like a GOT reloc.  */
	  /* Fall through.  */

	case R_CRIS_16_GOT:
	case R_CRIS_32_GOT:
	  /* This symbol requires a global offset table entry.  */
	  if (h != NULL)
	    {
	      if (h->got.refcount == 0)
		{
		  /* Make sure this symbol is output as a dynamic symbol.  */
		  if (h->dynindx == -1)
		    {
		      if (!bfd_elf_link_record_dynamic_symbol (info, h))
			return FALSE;
		    }

		  /* Allocate space in the .got section.  */
		  sgot->_raw_size += 4;
		  /* Allocate relocation space.  */
		  srelgot->_raw_size += sizeof (Elf32_External_Rela);
		}
	      h->got.refcount++;
	    }
	  else
	    {
	      /* This is a global offset table entry for a local symbol.  */
	      if (local_got_refcounts[r_symndx] == 0)
		{
		  sgot->_raw_size += 4;
		  if (info->shared)
		    {
		      /* If we are generating a shared object, we need to
			 output a R_CRIS_RELATIVE reloc so that the dynamic
			 linker can adjust this GOT entry.  */
		      srelgot->_raw_size += sizeof (Elf32_External_Rela);
		    }
		}
	      local_got_refcounts[r_symndx]++;
	    }
	  break;

	case R_CRIS_32_GOTREL:
	  /* This reference requires a global offset table.
	     FIXME: The actual refcount isn't used currently; the .got
	     section can't be removed if there were any references in the
	     input.  */
	  local_got_refcounts[-1]++;
	  break;

	handle_gotplt_reloc:

	case R_CRIS_32_PLT_GOTREL:
	  /* This reference requires a global offset table.  */
	  local_got_refcounts[-1]++;
	  /* Fall through.  */

	case R_CRIS_32_PLT_PCREL:
	  /* This symbol requires a procedure linkage table entry.  We
	     actually build the entry in adjust_dynamic_symbol,
             because this might be a case of linking PIC code which is
             never referenced by a dynamic object, in which case we
             don't need to generate a procedure linkage table entry
             after all.  */

	  /* If this is a local symbol, we resolve it directly without
	     creating a procedure linkage table entry.  */
	  if (h == NULL || ELF_ST_VISIBILITY (h->other) != STV_DEFAULT)
	    continue;

	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	  h->plt.refcount++;
	  break;

	case R_CRIS_8:
	case R_CRIS_16:
	case R_CRIS_32:
	  /* Let's help debug shared library creation.  Any of these
	     relocs can be used in shared libs, but pages containing them
	     cannot be shared.  Don't warn for sections we don't care
	     about, such as debug sections or non-constant sections.  We
	     can't help tables of (global) function pointers, for example,
	     though they must be emitted in a data section to avoid having
	     impure text sections.  */
	  if (info->shared
	      && (sec->flags & SEC_ALLOC) != 0
	      && (sec->flags & SEC_READONLY) != 0)
	    {
	      /* FIXME: How do we make this optionally a warning only?  */
	      (*_bfd_error_handler)
		(_("%s, section %s:\n  relocation %s should not be used in a shared object; recompile with -fPIC"),
		 bfd_archive_filename (abfd),
		 sec->name,
		 cris_elf_howto_table[r_type].name);
	    }
	  /* Fall through.  */

	case R_CRIS_8_PCREL:
	case R_CRIS_16_PCREL:
	case R_CRIS_32_PCREL:
	  if (h != NULL)
	    {
	      h->elf_link_hash_flags |= ELF_LINK_NON_GOT_REF;

	      /* Make sure a plt entry is created for this symbol if it
		 turns out to be a function defined by a dynamic object.  */
	      if (ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
		h->plt.refcount++;
	    }

	  /* If we are creating a shared library and this is not a local
	     symbol, we need to copy the reloc into the shared library.
	     However when linking with -Bsymbolic and this is a global
	     symbol which is defined in an object we are including in the
	     link (i.e., DEF_REGULAR is set), then we can resolve the
	     reloc directly.  At this point we have not seen all the input
	     files, so it is possible that DEF_REGULAR is not set now but
	     will be set later (it is never cleared).  In case of a weak
	     definition, DEF_REGULAR may be cleared later by a strong
	     definition in a shared library.  We account for that
	     possibility below by storing information in the relocs_copied
	     field of the hash table entry.  A similar situation occurs
	     when creating shared libraries and symbol visibility changes
	     render the symbol local.  */

	  /* No need to do anything if we're not creating a shared object.  */
	  if (! info->shared)
	    break;

	  /* We don't need to handle relocs into sections not going into
	     the "real" output.  */
	  if ((sec->flags & SEC_ALLOC) == 0)
	    break;

	  /* We can only eliminate PC-relative relocs.  */
	  if (r_type == R_CRIS_8_PCREL
	      || r_type == R_CRIS_16_PCREL
	      || r_type == R_CRIS_32_PCREL)
	    {
	      /* If the symbol is local, then we can eliminate the reloc.  */
	      if (h == NULL || ELF_ST_VISIBILITY (h->other) != STV_DEFAULT)
		break;

	      /* If this is with -Bsymbolic and the symbol isn't weak, and
		 is defined by an ordinary object (the ones we include in
		 this shared library) then we can also eliminate the
		 reloc.  See comment above for more eliminable cases which
		 we can't identify at this time.  */
	      if (info->symbolic
		  && h->root.type != bfd_link_hash_defweak
		  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0)
		break;
	    }

	  /* We create a reloc section in dynobj and make room for this
	     reloc.  */
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
		  sreloc = bfd_make_section (dynobj, name);
		  if (sreloc == NULL
		      || !bfd_set_section_flags (dynobj, sreloc,
						 (SEC_ALLOC
						  | SEC_LOAD
						  | SEC_HAS_CONTENTS
						  | SEC_IN_MEMORY
						  | SEC_LINKER_CREATED
						  | SEC_READONLY))
		      || !bfd_set_section_alignment (dynobj, sreloc, 2))
		    return FALSE;
		}
	      if (sec->flags & SEC_READONLY)
		info->flags |= DF_TEXTREL;
	    }

	  sreloc->_raw_size += sizeof (Elf32_External_Rela);

	  /* If we are linking with -Bsymbolic, we count the number of PC
	     relative relocations we have entered for this symbol, so that
	     we can discard them again if the symbol is later defined by a
	     regular object.  We know that h is really a pointer to an
	     elf_cris_link_hash_entry.  */
	  if ((r_type == R_CRIS_8_PCREL
	       || r_type == R_CRIS_16_PCREL
	       || r_type == R_CRIS_32_PCREL)
	      && info->symbolic)
	    {
	      struct elf_cris_link_hash_entry *eh;
	      struct elf_cris_pcrel_relocs_copied *p;

	      eh = (struct elf_cris_link_hash_entry *) h;

	      for (p = eh->pcrel_relocs_copied; p != NULL; p = p->next)
		if (p->section == sreloc)
		  break;

	      if (p == NULL)
		{
		  p = ((struct elf_cris_pcrel_relocs_copied *)
		       bfd_alloc (dynobj, (bfd_size_type) sizeof *p));
		  if (p == NULL)
		    return FALSE;
		  p->next = eh->pcrel_relocs_copied;
		  eh->pcrel_relocs_copied = p;
		  p->section = sreloc;
		  p->count = 0;
		}

	      ++p->count;
	    }
	  break;

        /* This relocation describes the C++ object vtable hierarchy.
           Reconstruct it for later use during GC.  */
        case R_CRIS_GNU_VTINHERIT:
          if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

        /* This relocation describes which C++ vtable entries are actually
           used.  Record for later use during GC.  */
        case R_CRIS_GNU_VTENTRY:
          if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return FALSE;
          break;

	default:
	  /* Other relocs do not appear here.  */
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
        }
    }

  return TRUE;
}

/* Set the sizes of the dynamic sections.  */

static bfd_boolean
elf_cris_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  bfd_boolean plt;
  bfd_boolean relocs;

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
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}
    }
  else
    {
      /* Adjust all expected GOTPLT uses to use a GOT entry instead.  */
      elf_cris_link_hash_traverse (elf_cris_hash_table (info),
				   elf_cris_adjust_gotplt_to_got,
				   (PTR) info);

      /* We may have created entries in the .rela.got section.
	 However, if we are not creating the dynamic sections, we will
	 not actually use these entries.  Reset the size of .rela.got,
	 which will cause it to get stripped from the output file
	 below.  */
      s = bfd_get_section_by_name (dynobj, ".rela.got");
      if (s != NULL)
	s->_raw_size = 0;
    }

  /* If this is a -Bsymbolic shared link, then we need to discard all PC
     relative relocs against symbols defined in a regular object.  We
     allocated space for them in the check_relocs routine, but we will not
     fill them in in the relocate_section routine.  We also discard space
     for relocs that have become for local symbols due to symbol
     visibility changes.  For programs, we discard space for relocs for
     symbols not referenced by any dynamic object.  */
  if (info->shared)
    elf_cris_link_hash_traverse (elf_cris_hash_table (info),
				 elf_cris_discard_excess_dso_dynamics,
				 (PTR) info);
  else
    elf_cris_link_hash_traverse (elf_cris_hash_table (info),
				 elf_cris_discard_excess_program_dynamics,
				 (PTR) info);

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  plt = FALSE;
  relocs = FALSE;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      bfd_boolean strip;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      strip = FALSE;

      if (strcmp (name, ".plt") == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* Strip this section if we don't need it; see the
                 comment below.  */
	      strip = TRUE;
	    }
	  else
	    {
	      /* Remember whether there is a PLT.  */
	      plt = TRUE;
	    }
	}
      else if (strncmp (name, ".rela", 5) == 0)
	{
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
	      strip = TRUE;
	    }
	  else
	    {
	      /* Remember whether there are any reloc sections other
                 than .rela.plt.  */
	      if (strcmp (name, ".rela.plt") != 0)
		  relocs = TRUE;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strncmp (name, ".got", 4) != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	{
	  _bfd_strip_section_from_output (info, s);
	  continue;
	}

      /* Allocate memory for the section contents. We use bfd_zalloc here
	 in case unused entries are not reclaimed before the section's
	 contents are written out.  This should not happen, but this way
	 if it does, we will not write out garbage.  For reloc sections,
	 this will make entries have the type R_CRIS_NONE.  */
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	return FALSE;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf_cris_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  _bfd_elf_add_dynamic_entry (info, TAG, VAL)

      if (!info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return FALSE;
	}

      if (plt)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return FALSE;
	}

      if (relocs)
	{
	  if (!add_dynamic_entry (DT_RELA, 0)
	      || !add_dynamic_entry (DT_RELASZ, 0)
	      || !add_dynamic_entry (DT_RELAENT, sizeof (Elf32_External_Rela)))
	    return FALSE;
	}

      if ((info->flags & DF_TEXTREL) != 0)
	{
	  if (!add_dynamic_entry (DT_TEXTREL, 0))
	    return FALSE;
	  info->flags |= DF_TEXTREL;
	}
    }
#undef add_dynamic_entry

  return TRUE;
}

/* This function is called via elf_cris_link_hash_traverse if we are
   creating a shared object.  In the -Bsymbolic case, it discards the
   space allocated to copy PC relative relocs against symbols which
   are defined in regular objects.  For the normal non-symbolic case,
   we also discard space for relocs that have become local due to
   symbol visibility changes.  We allocated space for them in the
   check_relocs routine, but we won't fill them in in the
   relocate_section routine.  */

static bfd_boolean
elf_cris_discard_excess_dso_dynamics (h, inf)
     struct elf_cris_link_hash_entry *h;
     PTR inf;
{
  struct elf_cris_pcrel_relocs_copied *s;
  struct bfd_link_info *info = (struct bfd_link_info *) inf;

  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct elf_cris_link_hash_entry *) h->root.root.u.i.link;

  /* If a symbol has been forced local or we have found a regular
     definition for the symbolic link case, then we won't be needing
     any relocs.  */
  if ((h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
      && ((h->root.elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0
	  || info->symbolic))
    {
      for (s = h->pcrel_relocs_copied; s != NULL; s = s->next)
	s->section->_raw_size -= s->count * sizeof (Elf32_External_Rela);
    }

  return TRUE;
}

/* This function is called via elf_cris_link_hash_traverse if we are *not*
   creating a shared object.  We discard space for relocs for symbols put
   in the .got, but which we found we do not have to resolve at run-time.  */

static bfd_boolean
elf_cris_discard_excess_program_dynamics (h, inf)
     struct elf_cris_link_hash_entry *h;
     PTR inf;
{
  struct bfd_link_info *info = (struct bfd_link_info *) inf;

  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct elf_cris_link_hash_entry *) h->root.root.u.i.link;

  /* If we're not creating a shared library and have a symbol which is
     referred to by .got references, but the symbol is defined locally,
     (or rather, not defined by a DSO) then lose the reloc for the .got
     (don't allocate room for it).  Likewise for relocs for something
     for which we create a PLT.  */
  if ((h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
      || h->root.plt.refcount > 0)
    {
      if (h->root.got.refcount > 0
	  /* The size of this section is only valid and in sync with the
	     various reference counts if we do dynamic; don't decrement it
	     otherwise.  */
	  && elf_hash_table (info)->dynamic_sections_created)
	{
	  bfd *dynobj = elf_hash_table (info)->dynobj;
	  asection *srelgot;

	  BFD_ASSERT (dynobj != NULL);

	  srelgot = bfd_get_section_by_name (dynobj, ".rela.got");

	  BFD_ASSERT (srelgot != NULL);

	  srelgot->_raw_size -= sizeof (Elf32_External_Rela);
	}

      /* If the locally-defined symbol isn't used by a DSO, then we don't
	 have to export it as a dynamic symbol.  This was already done for
	 functions; doing this for all symbols would presumably not
	 introduce new problems.  Of course we don't do this if we're
	 exporting all dynamic symbols.  */
      if (! info->export_dynamic
	  && (h->root.elf_link_hash_flags
	      & (ELF_LINK_HASH_DEF_DYNAMIC|ELF_LINK_HASH_REF_DYNAMIC)) == 0)
	{
	  h->root.dynindx = -1;
	  _bfd_elf_strtab_delref (elf_hash_table (info)->dynstr,
				  h->root.dynstr_index);
	}
    }

  return TRUE;
}

/* Reject a file depending on presence and expectation of prefixed
   underscores on symbols.  */

static bfd_boolean
cris_elf_object_p (abfd)
     bfd *abfd;
{
  if ((elf_elfheader (abfd)->e_flags & EF_CRIS_UNDERSCORE))
    return (bfd_get_symbol_leading_char (abfd) == '_');
  else
    return (bfd_get_symbol_leading_char (abfd) == 0);
}

/* Mark presence or absence of leading underscore.  */

static void
cris_elf_final_write_processing (abfd, linker)
     bfd *abfd;
     bfd_boolean linker ATTRIBUTE_UNUSED;
{
  if (bfd_get_symbol_leading_char (abfd) == '_')
    elf_elfheader (abfd)->e_flags |= EF_CRIS_UNDERSCORE;
  else
    elf_elfheader (abfd)->e_flags &= ~EF_CRIS_UNDERSCORE;
}

/* Display the flags field.  */

static bfd_boolean
cris_elf_print_private_bfd_data (abfd, ptr)
     bfd *abfd;
     PTR ptr;
{
  FILE *file = (FILE *) ptr;

  BFD_ASSERT (abfd != NULL && ptr != NULL)

  _bfd_elf_print_private_bfd_data (abfd, ptr);

  fprintf (file, _("private flags = %lx:"), elf_elfheader (abfd)->e_flags);

  if (elf_elfheader (abfd)->e_flags & EF_CRIS_UNDERSCORE)
    fprintf (file, _(" [symbols have a _ prefix]"));

  fputc ('\n', file);
  return TRUE;
}

/* Don't mix files with and without a leading underscore.  */

static bfd_boolean
cris_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword old_flags, new_flags;

  if (! _bfd_generic_verify_endian_match (ibfd, obfd))
    return FALSE;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  if (! elf_flags_init (obfd))
    {
      /* This happens when ld starts out with a 'blank' output file.  */
      elf_flags_init (obfd) = TRUE;

      /* Set flags according to current bfd_target.  */
      cris_elf_final_write_processing (obfd, FALSE);
    }

  old_flags = elf_elfheader (obfd)->e_flags;
  new_flags = elf_elfheader (ibfd)->e_flags;

  /* Is this good or bad?  We'll follow with other excluding flags.  */
  if ((old_flags & EF_CRIS_UNDERSCORE) != (new_flags & EF_CRIS_UNDERSCORE))
    {
      (*_bfd_error_handler)
	((new_flags & EF_CRIS_UNDERSCORE)
	 ? _("%s: uses _-prefixed symbols, but writing file with non-prefixed symbols")
	 : _("%s: uses non-prefixed symbols, but writing file with _-prefixed symbols"),
	 bfd_archive_filename (ibfd));
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  return TRUE;
}


static enum elf_reloc_type_class
elf_cris_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  switch ((int) ELF32_R_TYPE (rela->r_info))
    {
    case R_CRIS_RELATIVE:
      return reloc_class_relative;
    case R_CRIS_JUMP_SLOT:
      return reloc_class_plt;
    case R_CRIS_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

#define ELF_ARCH		bfd_arch_cris
#define ELF_MACHINE_CODE	EM_CRIS
#define ELF_MAXPAGESIZE		0x2000

#define TARGET_LITTLE_SYM	bfd_elf32_cris_vec
#define TARGET_LITTLE_NAME	"elf32-cris"
#define elf_symbol_leading_char 0

#define elf_info_to_howto_rel			NULL
#define elf_info_to_howto			cris_info_to_howto_rela
#define elf_backend_relocate_section		cris_elf_relocate_section
#define elf_backend_gc_mark_hook		cris_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook		cris_elf_gc_sweep_hook
#define elf_backend_check_relocs                cris_elf_check_relocs
#define elf_backend_grok_prstatus		cris_elf_grok_prstatus
#define elf_backend_grok_psinfo			cris_elf_grok_psinfo

#define elf_backend_can_gc_sections		1
#define elf_backend_can_refcount		1

#define elf_backend_object_p			cris_elf_object_p
#define elf_backend_final_write_processing \
	cris_elf_final_write_processing
#define bfd_elf32_bfd_print_private_bfd_data \
	cris_elf_print_private_bfd_data
#define bfd_elf32_bfd_merge_private_bfd_data \
	cris_elf_merge_private_bfd_data

#define bfd_elf32_bfd_reloc_type_lookup		cris_reloc_type_lookup

#define bfd_elf32_bfd_link_hash_table_create \
	elf_cris_link_hash_table_create
#define elf_backend_adjust_dynamic_symbol \
	elf_cris_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
	elf_cris_size_dynamic_sections
#define elf_backend_finish_dynamic_symbol \
	elf_cris_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
	elf_cris_finish_dynamic_sections
#define elf_backend_create_dynamic_sections \
	_bfd_elf_create_dynamic_sections
#define bfd_elf32_bfd_final_link \
	bfd_elf_gc_common_final_link
#define elf_backend_hide_symbol			elf_cris_hide_symbol
#define elf_backend_reloc_type_class		elf_cris_reloc_type_class

#define elf_backend_want_got_plt	1
#define elf_backend_plt_readonly	1
#define elf_backend_want_plt_sym	0
#define elf_backend_got_header_size	12

/* Later, we my want to optimize RELA entries into REL entries for dynamic
   linking and libraries (if it's a win of any significance).  Until then,
   take the easy route.  */
#define elf_backend_may_use_rel_p 0
#define elf_backend_may_use_rela_p 1
#define elf_backend_rela_normal		1

#include "elf32-target.h"

#define INCLUDED_TARGET_FILE

#undef TARGET_LITTLE_SYM
#undef TARGET_LITTLE_NAME
#undef elf_symbol_leading_char

#define TARGET_LITTLE_SYM bfd_elf32_us_cris_vec
#define TARGET_LITTLE_NAME "elf32-us-cris"
#define elf_symbol_leading_char '_'

#include "elf32-target.h"
