/* MIPS-specific support for 64-bit ELF
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Ian Lance Taylor, Cygnus Support
   Linker support added by Mark Mitchell, CodeSourcery, LLC.
   <mark@codesourcery.com>

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

/* This file supports the 64-bit MIPS ELF ABI.

   The MIPS 64-bit ELF ABI uses an unusual reloc format.  This file
   overrides the usual ELF reloc handling, and handles reading and
   writing the relocations here.  */

/* TODO: Many things are unsupported, even if there is some code for it
 .       (which was mostly stolen from elf32-mips.c and slightly adapted).
 .
 .   - Relocation handling for REL relocs is wrong in many cases and
 .     generally untested.
 .   - Relocation handling for RELA relocs related to GOT support are
 .     also likely to be wrong.
 .   - Support for MIPS16 is only partially implemented.
 .   - Embedded PIC  is only partially implemented (is it needed?).
 .   - Combined relocs with RSS_* entries are unsupported.
 .   - The whole GOT handling for NewABI is missing, some parts of
 .     the OldABI version is still lying around and shold be removed.
 */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "aout/ar.h"
#include "bfdlink.h"
#include "genlink.h"
#include "elf-bfd.h"
#include "elf/mips.h"

/* Get the ECOFF swapping routines.  The 64-bit ABI is not supposed to
   use ECOFF.  However, we support it anyhow for an easier changeover.  */
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/internal.h"
#include "coff/ecoff.h"
/* The 64 bit versions of the mdebug data structures are in alpha.h.  */
#include "coff/alpha.h"
#define ECOFF_SIGNED_64
#include "ecoffswap.h"

struct mips_elf64_link_hash_entry;

static void mips_elf64_swap_reloc_in
  PARAMS ((bfd *, const Elf64_Mips_External_Rel *,
	   Elf64_Mips_Internal_Rel *));
static void mips_elf64_swap_reloca_in
  PARAMS ((bfd *, const Elf64_Mips_External_Rela *,
	   Elf64_Mips_Internal_Rela *));
static void mips_elf64_swap_reloc_out
  PARAMS ((bfd *, const Elf64_Mips_Internal_Rel *,
	   Elf64_Mips_External_Rel *));
static void mips_elf64_swap_reloca_out
  PARAMS ((bfd *, const Elf64_Mips_Internal_Rela *,
	   Elf64_Mips_External_Rela *));
static void mips_elf64_be_swap_reloc_in
  PARAMS ((bfd *, const bfd_byte *, Elf_Internal_Rel *));
static void mips_elf64_be_swap_reloc_out
  PARAMS ((bfd *, const Elf_Internal_Rel *, bfd_byte *));
static void mips_elf64_be_swap_reloca_in
  PARAMS ((bfd *, const bfd_byte *, Elf_Internal_Rela *));
static void mips_elf64_be_swap_reloca_out
  PARAMS ((bfd *, const Elf_Internal_Rela *, bfd_byte *));
static bfd_vma mips_elf64_high PARAMS ((bfd_vma));
static bfd_vma mips_elf64_higher PARAMS ((bfd_vma));
static bfd_vma mips_elf64_highest PARAMS ((bfd_vma));
static reloc_howto_type *mips_elf64_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void mips_elf64_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf64_Internal_Rel *));
static void mips_elf64_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf64_Internal_Rela *));
static long mips_elf64_get_reloc_upper_bound PARAMS ((bfd *, asection *));
static boolean mips_elf64_slurp_one_reloc_table
  PARAMS ((bfd *, asection *, asymbol **, const Elf_Internal_Shdr *));
static boolean mips_elf64_slurp_reloc_table
  PARAMS ((bfd *, asection *, asymbol **, boolean));
static void mips_elf64_write_relocs PARAMS ((bfd *, asection *, PTR));
static void mips_elf64_write_rel
  PARAMS((bfd *, asection *, Elf_Internal_Shdr *, int *, PTR));
static void mips_elf64_write_rela
  PARAMS((bfd *, asection *, Elf_Internal_Shdr *, int *, PTR));
static struct bfd_hash_entry *mips_elf64_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static bfd_reloc_status_type mips_elf64_hi16_reloc
  PARAMS ((bfd *, arelent *, asymbol *,	PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_higher_reloc
  PARAMS ((bfd *, arelent *, asymbol *,	PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_highest_reloc
  PARAMS ((bfd *, arelent *, asymbol *,	PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_gprel16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_gprel16_reloca
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_literal_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_gprel32_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_shift6_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips_elf64_got16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static boolean mips_elf64_assign_gp PARAMS ((bfd *, bfd_vma *));
static bfd_reloc_status_type mips_elf64_final_gp
  PARAMS ((bfd *, asymbol *, boolean, char **, bfd_vma *));
static bfd_reloc_status_type gprel16_with_gp
  PARAMS ((bfd *, asymbol *, arelent *, asection *, boolean, PTR, bfd_vma));
static int mips_elf64_additional_program_headers PARAMS ((bfd *));
static struct bfd_link_hash_table *mips_elf64_link_hash_table_create
  PARAMS((bfd *));
static bfd_vma mips_elf64_got_offset_from_index
  PARAMS ((bfd *, bfd *, bfd_vma));
static struct mips_elf64_got_info *_mips_elf64_got_info
  PARAMS ((bfd *, asection **));
static bfd_vma mips_elf64_sign_extend PARAMS ((bfd_vma, int));
static boolean mips_elf64_overflow_p PARAMS ((bfd_vma, int));
static bfd_vma mips_elf64_global_got_index
  PARAMS ((bfd *, struct elf_link_hash_entry *));
static boolean mips_elf64_sort_hash_table_f
  PARAMS ((struct mips_elf64_link_hash_entry *, PTR));
static boolean mips_elf64_sort_hash_table
  PARAMS ((struct bfd_link_info *, unsigned long));
static void mips_elf64_swap_msym_out
  PARAMS ((bfd *, const Elf32_Internal_Msym *, Elf32_External_Msym *));
static bfd_vma mips_elf64_create_local_got_entry
  PARAMS ((bfd *abfd, struct mips_elf64_got_info *, asection *,
	   bfd_vma value));
static bfd_vma mips_elf64_local_got_index
  PARAMS ((bfd *, struct bfd_link_info *, bfd_vma));
static bfd_vma mips_elf64_got_page
  PARAMS ((bfd *, struct bfd_link_info *, bfd_vma, bfd_vma *));
static bfd_vma mips_elf64_got16_entry
  PARAMS ((bfd *, struct bfd_link_info *, bfd_vma, boolean));
static boolean mips_elf64_local_relocation_p
  PARAMS ((bfd *, const Elf_Internal_Rela *, asection **, boolean));
static const Elf_Internal_Rela *mips_elf64_next_relocation
  PARAMS ((unsigned int, const Elf_Internal_Rela *,
	   const Elf_Internal_Rela *));
static boolean mips_elf64_create_dynamic_relocation
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Rela *,
	   struct mips_elf64_link_hash_entry *, asection *, bfd_vma,
	   bfd_vma *, asection *));
static bfd_reloc_status_type mips_elf64_calculate_relocation
  PARAMS ((bfd *, bfd *, asection *, struct bfd_link_info *,
	   const Elf_Internal_Rela *, bfd_vma, reloc_howto_type *,
	   Elf_Internal_Sym *, asection **, bfd_vma *, const char **,
	   boolean *));
static bfd_vma mips_elf64_obtain_contents
  PARAMS ((reloc_howto_type *, const Elf_Internal_Rela *, bfd *, bfd_byte *));
static boolean mips_elf64_perform_relocation
  PARAMS ((struct bfd_link_info *, reloc_howto_type *,
	   const Elf_Internal_Rela *, bfd_vma,
	   bfd *, asection *, bfd_byte *, boolean));
static boolean mips_elf64_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
boolean mips_elf64_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
boolean mips_elf64_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *h));
boolean mips_elf64_always_size_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean mips_elf64_check_mips16_stubs
  PARAMS ((struct mips_elf64_link_hash_entry *, PTR));
boolean mips_elf64_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
boolean mips_elf64_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
boolean mips_elf64_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *info));
asection *mips_elf64_gc_mark_hook
  PARAMS ((bfd *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
boolean mips_elf64_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static boolean mips_elf64_create_got_section
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean mips_elf64_record_global_got_symbol
  PARAMS ((struct elf_link_hash_entry *, struct bfd_link_info *,
	   struct mips_elf64_got_info *));
static asection *mips_elf64_create_msym_section PARAMS((bfd *));
static void mips_elf64_allocate_dynamic_relocations
  PARAMS ((bfd *, unsigned int));
static boolean mips_elf64_stub_section_p PARAMS ((bfd *, asection *));
boolean mips_elf64_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static boolean mips_elf64_output_extsym
	PARAMS ((struct mips_elf64_link_hash_entry *, PTR));
static void mips_elf64_swap_gptab_in
  PARAMS ((bfd *, const Elf32_External_gptab *, Elf32_gptab *));
static void mips_elf64_swap_gptab_out
  PARAMS ((bfd *, const Elf32_gptab *, Elf32_External_gptab *));
static int gptab_compare PARAMS ((const PTR, const PTR));
boolean mips_elf64_final_link PARAMS ((bfd *, struct bfd_link_info *));

extern const bfd_target bfd_elf64_bigmips_vec;
extern const bfd_target bfd_elf64_littlemips_vec;

static bfd_vma prev_reloc_addend = 0;
static bfd_size_type prev_reloc_address = 0;

/* Whether we are trying to be compatible with IRIX6 (or little endianers
   which are otherwise IRIX-ABI compliant).  */
#define SGI_COMPAT(abfd) \
  ((abfd->xvec == &bfd_elf64_bigmips_vec) \
   || (abfd->xvec == &bfd_elf64_littlemips_vec) ? true : false)

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

/* The number of local .got entries we reserve.  */
#define MIPS_RESERVED_GOTNO (2)

/* Instructions which appear in a stub.  */
#define ELF_MIPS_GP_OFFSET(abfd) 0x7ff0
#define STUB_LW    0xdf998010   /* ld t9,0x8010(gp) */
#define STUB_MOVE  0x03e07825   /* move t7,ra */
#define STUB_JALR  0x0320f809   /* jal t9 */
#define STUB_LI16  0x34180000   /* ori t8,zero,0 */
#define MIPS_FUNCTION_STUB_SIZE (16)

/* The relocation table used for SHT_REL sections.  */

#define UNUSED_RELOC(num) { num, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

static reloc_howto_type mips_elf64_howto_table_rel[] =
{
  /* No relocation.  */
  HOWTO (R_MIPS_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit relocation.  */
  HOWTO (R_MIPS_16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit relocation.  */
  HOWTO (R_MIPS_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit symbol relative relocation.  */
  HOWTO (R_MIPS_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 26 bit jump address.  */
  HOWTO (R_MIPS_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
				/* This needs complex overflow
				   detection, because the upper 36
				   bits must match the PC + 4.  */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_26",		/* name */
	 true,			/* partial_inplace */
	 0x03ffffff,		/* src_mask */
	 0x03ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of symbol value.  */
  HOWTO (R_MIPS_HI16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HI16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_MIPS_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_LO16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* GP relative reference.  */
  HOWTO (R_MIPS_GPREL16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_gprel16_reloc, /* special_function */
	 "R_MIPS_GPREL16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to literal section.  */
  HOWTO (R_MIPS_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_literal_reloc, /* special_function */
	 "R_MIPS_LITERAL",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to global offset table.  */
  HOWTO (R_MIPS_GOT16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_got16_reloc, /* special_function */
	 "R_MIPS_GOT16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit PC relative reference.  */
  HOWTO (R_MIPS_PC16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PC16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16 bit call through global offset table.  */
  /* FIXME: This is not handled correctly.  */
  HOWTO (R_MIPS_CALL16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit GP relative reference.  */
  HOWTO (R_MIPS_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf64_gprel32_reloc, /* special_function */
	 "R_MIPS_GPREL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  UNUSED_RELOC (13),
  UNUSED_RELOC (14),
  UNUSED_RELOC (15),

  /* A 5 bit shift field.  */
  HOWTO (R_MIPS_SHIFT5,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT5",	/* name */
	 true,			/* partial_inplace */
	 0x000007c0,		/* src_mask */
	 0x000007c0,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 6 bit shift field.  */
  HOWTO (R_MIPS_SHIFT6,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mips_elf64_shift6_reloc, /* special_function */
	 "R_MIPS_SHIFT6",	/* name */
	 true,			/* partial_inplace */
	 0x000007c4,		/* src_mask */
	 0x000007c4,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit relocation.  */
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_DISP",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement to page pointer in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_PAGE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_PAGE",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Offset from page pointer in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_OFST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_OFST",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_HI16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit substraction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_SUB,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SUB",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_A,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_A",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction, and change all relocations
     to refer to the old instruction at the address.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_B,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_B",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Delete a 32 bit instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_DELETE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_DELETE",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the higher value of a 64 bit addend.  */
  HOWTO (R_MIPS_HIGHER,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf64_higher_reloc, /* special_function */
	 "R_MIPS_HIGHER",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the highest value of a 64 bit addend.  */
  HOWTO (R_MIPS_HIGHEST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf64_highest_reloc, /* special_function */
	 "R_MIPS_HIGHEST",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_CALL_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_HI16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_CALL_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Section displacement, used by an associated event location section.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_SCN_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SCN_DISP",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_REL16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL16",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* These two are obsolete.  */
  EMPTY_HOWTO (R_MIPS_ADD_IMMEDIATE),
  EMPTY_HOWTO (R_MIPS_PJUMP),

  /* Similiar to R_MIPS_REL32, but used for relocations in a GOT section.
     It must be used for multigot GOT's (and only there).  */
  HOWTO (R_MIPS_RELGOT,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_RELGOT",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Protected jump conversion.  This is an optimization hint.  No
     relocation is required for correctness.  */
  HOWTO (R_MIPS_JALR,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_JALR",	        /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x00000000,		/* dst_mask */
	 false),		/* pcrel_offset */
};

/* The relocation table used for SHT_RELA sections.  */

static reloc_howto_type mips_elf64_howto_table_rela[] =
{
  /* No relocation.  */
  HOWTO (R_MIPS_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit relocation.  */
  HOWTO (R_MIPS_16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit relocation.  */
  HOWTO (R_MIPS_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit symbol relative relocation.  */
  HOWTO (R_MIPS_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 26 bit jump address.  */
  HOWTO (R_MIPS_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
				/* This needs complex overflow
				   detection, because the upper 36
				   bits must match the PC + 4.  */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_26",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x03ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* R_MIPS_HI16 and R_MIPS_LO16 are unsupported for 64 bit REL.  */
  /* High 16 bits of symbol value.  */
  HOWTO (R_MIPS_HI16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HI16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_MIPS_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_LO16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* GP relative reference.  */
  HOWTO (R_MIPS_GPREL16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_gprel16_reloca, /* special_function */
	 "R_MIPS_GPREL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to literal section.  */
  HOWTO (R_MIPS_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf64_literal_reloc, /* special_function */
	 "R_MIPS_LITERAL",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to global offset table.  */
  /* FIXME: This is not handled correctly.  */
  HOWTO (R_MIPS_GOT16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_MIPS_GOT16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit PC relative reference.  */
  HOWTO (R_MIPS_PC16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PC16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16 bit call through global offset table.  */
  /* FIXME: This is not handled correctly.  */
  HOWTO (R_MIPS_CALL16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit GP relative reference.  */
  HOWTO (R_MIPS_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf64_gprel32_reloc, /* special_function */
	 "R_MIPS_GPREL32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  UNUSED_RELOC (13),
  UNUSED_RELOC (14),
  UNUSED_RELOC (15),

  /* A 5 bit shift field.  */
  HOWTO (R_MIPS_SHIFT5,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT5",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x000007c0,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 6 bit shift field.  */
  HOWTO (R_MIPS_SHIFT6,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mips_elf64_shift6_reloc, /* special_function */
	 "R_MIPS_SHIFT6",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x000007c4,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit relocation.  */
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_DISP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement to page pointer in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_PAGE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_PAGE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Offset from page pointer in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_OFST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_OFST",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_HI16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_LO16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit substraction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_SUB,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SUB",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_A,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_A",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction, and change all relocations
     to refer to the old instruction at the address.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_B,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_B",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Delete a 32 bit instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_DELETE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_DELETE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the higher value of a 64 bit addend.  */
  HOWTO (R_MIPS_HIGHER,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_MIPS_HIGHER",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the highest value of a 64 bit addend.  */
  HOWTO (R_MIPS_HIGHEST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_MIPS_HIGHEST",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_CALL_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_HI16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_CALL_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_LO16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Section displacement, used by an associated event location section.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_SCN_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SCN_DISP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_REL16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* These two are obsolete.  */
  EMPTY_HOWTO (R_MIPS_ADD_IMMEDIATE),
  EMPTY_HOWTO (R_MIPS_PJUMP),

  /* Similiar to R_MIPS_REL32, but used for relocations in a GOT section.
     It must be used for multigot GOT's (and only there).  */
  HOWTO (R_MIPS_RELGOT,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_RELGOT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Protected jump conversion.  This is an optimization hint.  No
     relocation is required for correctness.  */
  HOWTO (R_MIPS_JALR,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_JALR",	        /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x00000000,		/* dst_mask */
	 false),		/* pcrel_offset */
};

/* Swap in a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_swap_reloc_in (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_External_Rel *src;
     Elf64_Mips_Internal_Rel *dst;
{
  dst->r_offset = H_GET_64 (abfd, src->r_offset);
  dst->r_sym = H_GET_32 (abfd, src->r_sym);
  dst->r_ssym = H_GET_8 (abfd, src->r_ssym);
  dst->r_type3 = H_GET_8 (abfd, src->r_type3);
  dst->r_type2 = H_GET_8 (abfd, src->r_type2);
  dst->r_type = H_GET_8 (abfd, src->r_type);
}

/* Swap in a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_swap_reloca_in (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_External_Rela *src;
     Elf64_Mips_Internal_Rela *dst;
{
  dst->r_offset = H_GET_64 (abfd, src->r_offset);
  dst->r_sym = H_GET_32 (abfd, src->r_sym);
  dst->r_ssym = H_GET_8 (abfd, src->r_ssym);
  dst->r_type3 = H_GET_8 (abfd, src->r_type3);
  dst->r_type2 = H_GET_8 (abfd, src->r_type2);
  dst->r_type = H_GET_8 (abfd, src->r_type);
  dst->r_addend = H_GET_S64 (abfd, src->r_addend);
}

/* Swap out a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_swap_reloc_out (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_Internal_Rel *src;
     Elf64_Mips_External_Rel *dst;
{
  H_PUT_64 (abfd, src->r_offset, dst->r_offset);
  H_PUT_32 (abfd, src->r_sym, dst->r_sym);
  H_PUT_8 (abfd, src->r_ssym, dst->r_ssym);
  H_PUT_8 (abfd, src->r_type3, dst->r_type3);
  H_PUT_8 (abfd, src->r_type2, dst->r_type2);
  H_PUT_8 (abfd, src->r_type, dst->r_type);
}

/* Swap out a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_swap_reloca_out (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_Internal_Rela *src;
     Elf64_Mips_External_Rela *dst;
{
  H_PUT_64 (abfd, src->r_offset, dst->r_offset);
  H_PUT_32 (abfd, src->r_sym, dst->r_sym);
  H_PUT_8 (abfd, src->r_ssym, dst->r_ssym);
  H_PUT_8 (abfd, src->r_type3, dst->r_type3);
  H_PUT_8 (abfd, src->r_type2, dst->r_type2);
  H_PUT_8 (abfd, src->r_type, dst->r_type);
  H_PUT_S64 (abfd, src->r_addend, dst->r_addend);
}

/* Swap in a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_be_swap_reloc_in (abfd, src, dst)
     bfd *abfd;
     const bfd_byte *src;
     Elf_Internal_Rel *dst;
{
  Elf64_Mips_Internal_Rel mirel;

  mips_elf64_swap_reloc_in (abfd,
			    (const Elf64_Mips_External_Rel *) src,
			    &mirel);

  dst[0].r_offset = mirel.r_offset;
  dst[0].r_info = ELF64_R_INFO (mirel.r_sym, mirel.r_type);
  dst[1].r_offset = mirel.r_offset;
  dst[1].r_info = ELF64_R_INFO (mirel.r_ssym, mirel.r_type2);
  dst[2].r_offset = mirel.r_offset;
  dst[2].r_info = ELF64_R_INFO (STN_UNDEF, mirel.r_type3);
}

/* Swap in a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_be_swap_reloca_in (abfd, src, dst)
     bfd *abfd;
     const bfd_byte *src;
     Elf_Internal_Rela *dst;
{
  Elf64_Mips_Internal_Rela mirela;

  mips_elf64_swap_reloca_in (abfd,
			     (const Elf64_Mips_External_Rela *) src,
			     &mirela);

  dst[0].r_offset = mirela.r_offset;
  dst[0].r_info = ELF64_R_INFO (mirela.r_sym, mirela.r_type);
  dst[0].r_addend = mirela.r_addend;
  dst[1].r_offset = mirela.r_offset;
  dst[1].r_info = ELF64_R_INFO (mirela.r_ssym, mirela.r_type2);
  dst[1].r_addend = 0;
  dst[2].r_offset = mirela.r_offset;
  dst[2].r_info = ELF64_R_INFO (STN_UNDEF, mirela.r_type3);
  dst[2].r_addend = 0;
}

/* Swap out a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_be_swap_reloc_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Rel *src;
     bfd_byte *dst;
{
  Elf64_Mips_Internal_Rel mirel;

  mirel.r_offset = src[0].r_offset;
  BFD_ASSERT(src[0].r_offset == src[1].r_offset);
  BFD_ASSERT(src[0].r_offset == src[2].r_offset);

  mirel.r_type = ELF64_MIPS_R_TYPE (src[0].r_info);
  mirel.r_sym = ELF64_R_SYM (src[0].r_info);
  mirel.r_type2 = ELF64_MIPS_R_TYPE2 (src[1].r_info);
  mirel.r_ssym = ELF64_MIPS_R_SSYM (src[1].r_info);
  mirel.r_type3 = ELF64_MIPS_R_TYPE3 (src[2].r_info);

  mips_elf64_swap_reloc_out (abfd, &mirel,
			     (Elf64_Mips_External_Rel *) dst);
}

/* Swap out a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_be_swap_reloca_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Rela *src;
     bfd_byte *dst;
{
  Elf64_Mips_Internal_Rela mirela;

  mirela.r_offset = src[0].r_offset;
  BFD_ASSERT(src[0].r_offset == src[1].r_offset);
  BFD_ASSERT(src[0].r_offset == src[2].r_offset);

  mirela.r_type = ELF64_MIPS_R_TYPE (src[0].r_info);
  mirela.r_sym = ELF64_R_SYM (src[0].r_info);
  mirela.r_addend = src[0].r_addend;
  BFD_ASSERT(src[1].r_addend == 0);
  BFD_ASSERT(src[2].r_addend == 0);

  mirela.r_type2 = ELF64_MIPS_R_TYPE2 (src[1].r_info);
  mirela.r_ssym = ELF64_MIPS_R_SSYM (src[1].r_info);
  mirela.r_type3 = ELF64_MIPS_R_TYPE3 (src[2].r_info);

  mips_elf64_swap_reloca_out (abfd, &mirela,
			      (Elf64_Mips_External_Rela *) dst);
}

/* Calculate the %high function.  */

static bfd_vma
mips_elf64_high (value)
     bfd_vma value;
{
  return ((value + (bfd_vma) 0x8000) >> 16) & 0xffff;
}

/* Calculate the %higher function.  */

static bfd_vma
mips_elf64_higher (value)
     bfd_vma value;
{
  return ((value + (bfd_vma) 0x80008000) >> 32) & 0xffff;
}

/* Calculate the %highest function.  */

static bfd_vma 
mips_elf64_highest (value)
     bfd_vma value;
{
  return ((value + (bfd_vma) 0x800080008000) >> 48) & 0xffff;
}

/* Do a R_MIPS_HI16 relocation.  */

bfd_reloc_status_type
mips_elf64_hi16_reloc (abfd,
		     reloc_entry,
		     symbol,
		     data,
		     input_section,
		     output_bfd,
		     error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* If we're relocating, and this is an external symbol, we don't
     want to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (((reloc_entry->addend & 0xffff) + 0x8000) & ~0xffff)
    reloc_entry->addend += 0x8000;

  return bfd_reloc_continue;
}

/* Do a R_MIPS_HIGHER relocation.  */

bfd_reloc_status_type
mips_elf64_higher_reloc (abfd,
			 reloc_entry,
			 symbol,
			 data,
			 input_section,
			 output_bfd,
			 error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* If we're relocating, and this is an external symbol, we don't
     want to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (((reloc_entry->addend & 0xffffffff) + 0x80008000)
      & ~0xffffffff)
    reloc_entry->addend += 0x80008000;

  return bfd_reloc_continue;
}

/* Do a R_MIPS_HIGHEST relocation.  */

bfd_reloc_status_type
mips_elf64_highest_reloc (abfd,
			  reloc_entry,
			  symbol,
			  data,
			  input_section,
			  output_bfd,
			  error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* If we're relocating, and this is an external symbol, we don't
     want to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (((reloc_entry->addend & 0xffffffffffff) + 0x800080008000)
      & ~0xffffffffffff)
    reloc_entry->addend += 0x800080008000;

  return bfd_reloc_continue;
}

/* Do a R_MIPS_GOT16 reloc.  This is a reloc against the global offset
   table used for PIC code.  If the symbol is an external symbol, the
   instruction is modified to contain the offset of the appropriate
   entry in the global offset table.  If the symbol is a section
   symbol, the next reloc is a R_MIPS_LO16 reloc.  The two 16 bit
   addends are combined to form the real addend against the section
   symbol; the GOT16 is modified to contain the offset of an entry in
   the global offset table, and the LO16 is modified to offset it
   appropriately.  Thus an offset larger than 16 bits requires a
   modified value in the global offset table.

   This implementation suffices for the assembler, but the linker does
   not yet know how to create global offset tables.  */

bfd_reloc_status_type
mips_elf64_got16_reloc (abfd,
		      reloc_entry,
		      symbol,
		      data,
		      input_section,
		      output_bfd,
		      error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  /* If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* If we're relocating, and this is a local symbol, we can handle it
     just like HI16.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) != 0)
    return mips_elf64_hi16_reloc (abfd, reloc_entry, symbol, data,
				  input_section, output_bfd, error_message);

  abort ();
}

/* Set the GP value for OUTPUT_BFD.  Returns false if this is a
   dangerous relocation.  */

static boolean
mips_elf64_assign_gp (output_bfd, pgp)
     bfd *output_bfd;
     bfd_vma *pgp;
{
  unsigned int count;
  asymbol **sym;
  unsigned int i;

  /* If we've already figured out what GP will be, just return it.  */
  *pgp = _bfd_get_gp_value (output_bfd);
  if (*pgp)
    return true;

  count = bfd_get_symcount (output_bfd);
  sym = bfd_get_outsymbols (output_bfd);

  /* The linker script will have created a symbol named `_gp' with the
     appropriate value.  */
  if (sym == (asymbol **) NULL)
    i = count;
  else
    {
      for (i = 0; i < count; i++, sym++)
	{
	  register CONST char *name;

	  name = bfd_asymbol_name (*sym);
	  if (*name == '_' && strcmp (name, "_gp") == 0)
	    {
	      *pgp = bfd_asymbol_value (*sym);
	      _bfd_set_gp_value (output_bfd, *pgp);
	      break;
	    }
	}
    }

  if (i >= count)
    {
      /* Only get the error once.  */
      *pgp = 4;
      _bfd_set_gp_value (output_bfd, *pgp);
      return false;
    }

  return true;
}

/* We have to figure out the gp value, so that we can adjust the
   symbol value correctly.  We look up the symbol _gp in the output
   BFD.  If we can't find it, we're stuck.  We cache it in the ELF
   target data.  We don't need to adjust the symbol value for an
   external symbol if we are producing relocateable output.  */

static bfd_reloc_status_type
mips_elf64_final_gp (output_bfd, symbol, relocateable, error_message, pgp)
     bfd *output_bfd;
     asymbol *symbol;
     boolean relocateable;
     char **error_message;
     bfd_vma *pgp;
{
  if (bfd_is_und_section (symbol->section)
      && ! relocateable)
    {
      *pgp = 0;
      return bfd_reloc_undefined;
    }

  *pgp = _bfd_get_gp_value (output_bfd);
  if (*pgp == 0
      && (! relocateable
	  || (symbol->flags & BSF_SECTION_SYM) != 0))
    {
      if (relocateable)
	{
	  /* Make up a value.  */
	  *pgp = symbol->section->output_section->vma + 0x4000;
	  _bfd_set_gp_value (output_bfd, *pgp);
	}
      else if (!mips_elf64_assign_gp (output_bfd, pgp))
	{
	  *error_message =
	    (char *) _("GP relative relocation when _gp not defined");
	  return bfd_reloc_dangerous;
	}
    }

  return bfd_reloc_ok;
}

/* Do a R_MIPS_GPREL16 relocation.  This is a 16 bit value which must
   become the offset from the gp register.  */

bfd_reloc_status_type
mips_elf64_gprel16_reloc (abfd, reloc_entry, symbol, data, input_section,
			  output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != (bfd *) NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf64_final_gp (output_bfd, symbol, relocateable, error_message,
			     &gp);
  if (ret != bfd_reloc_ok)
    return ret;

  return gprel16_with_gp (abfd, symbol, reloc_entry, input_section,
			  relocateable, data, gp);
}

static bfd_reloc_status_type
gprel16_with_gp (abfd, symbol, reloc_entry, input_section, relocateable, data,
		 gp)
     bfd *abfd;
     asymbol *symbol;
     arelent *reloc_entry;
     asection *input_section;
     boolean relocateable;
     PTR data;
     bfd_vma gp;
{
  bfd_vma relocation;
  unsigned long insn;
  unsigned long val;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  insn = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

  /* Set val to the offset into the section or symbol.  */
  if (reloc_entry->howto->src_mask == 0)
    {
      /* This case occurs with the 64-bit MIPS ELF ABI.  */
      val = reloc_entry->addend;
    }
  else
    {
      val = ((insn & 0xffff) + reloc_entry->addend) & 0xffff;
      if (val & 0x8000)
	val -= 0x10000;
    }

  /* Adjust val for the final section location and GP value.  If we
     are producing relocateable output, we don't want to do this for
     an external symbol.  */
  if (! relocateable
      || (symbol->flags & BSF_SECTION_SYM) != 0)
    val += relocation - gp;

  insn = (insn & ~0xffff) | (val & 0xffff);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  if (relocateable)
    reloc_entry->address += input_section->output_offset;

  else if ((long) val >= 0x8000 || (long) val < -0x8000)
    return bfd_reloc_overflow;

  return bfd_reloc_ok;
}

/* Do a R_MIPS_GPREL16 RELA relocation.  */

bfd_reloc_status_type
mips_elf64_gprel16_reloca (abfd, reloc_entry, symbol, data, input_section,
			   output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_vma gp;

  /* This works only for NewABI.  */
  BFD_ASSERT (reloc_entry->howto->src_mask == 0);

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != (bfd *) NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  if (prev_reloc_address != reloc_entry->address)
    prev_reloc_address = reloc_entry->address;
  else
    {
      mips_elf64_final_gp (output_bfd, symbol, relocateable, error_message,
			   &gp);
      prev_reloc_addend = reloc_entry->addend + reloc_entry->address - gp;
      if (symbol->flags & BSF_LOCAL)
	prev_reloc_addend += _bfd_get_gp_value (abfd);
/*fprintf(stderr, "Addend: %lx, Next Addend: %lx\n", reloc_entry->addend, prev_reloc_addend);*/
    }

  return bfd_reloc_ok;
}

/* Do a R_MIPS_LITERAL relocation.  */

bfd_reloc_status_type
mips_elf64_literal_reloc (abfd, reloc_entry, symbol, data, input_section,
			  output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  /* If we're relocating, and this is an external symbol, we don't
     want to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* FIXME: The entries in the .lit8 and .lit4 sections should be merged.
     Currently we simply call mips_elf64_gprel16_reloc.  */
  return mips_elf64_gprel16_reloc (abfd, reloc_entry, symbol, data,
				   input_section, output_bfd, error_message);
}

/* Do a R_MIPS_GPREL32 relocation.  Is this 32 bit value the offset
   from the gp register? XXX */

bfd_reloc_status_type
mips_elf64_gprel32_reloc (abfd,
			reloc_entry,
			symbol,
			data,
			input_section,
			output_bfd,
			error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;
  bfd_vma relocation;
  unsigned long val;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      *error_message = (char *)
	_("32bits gp relative relocation occurs for an external symbol");
      return bfd_reloc_outofrange;
    }

  if (output_bfd != (bfd *) NULL)
    {
      relocateable = true;
      gp = _bfd_get_gp_value (output_bfd);
    }
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;

      ret = mips_elf64_final_gp (output_bfd, symbol, relocateable,
				 error_message, &gp);
      if (ret != bfd_reloc_ok)
	return ret;
    }

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  if (reloc_entry->howto->src_mask == 0)
    {
      /* This case arises with the 64-bit MIPS ELF ABI.  */
      val = 0;
    }
  else
    val = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

  /* Set val to the offset into the section or symbol.  */
  val += reloc_entry->addend;

  /* Adjust val for the final section location and GP value.  If we
     are producing relocateable output, we don't want to do this for
     an external symbol.  */
  if (! relocateable
      || (symbol->flags & BSF_SECTION_SYM) != 0)
    val += relocation - gp;

  bfd_put_32 (abfd, val, (bfd_byte *) data + reloc_entry->address);

  if (relocateable)
    reloc_entry->address += input_section->output_offset;

  return bfd_reloc_ok;
}

/* Do a R_MIPS_SHIFT6 relocation. The MSB of the shift is stored at bit 2,
   the rest is at bits 6-10. The bitpos alredy got right by the howto.   */

bfd_reloc_status_type
mips_elf64_shift6_reloc (abfd, reloc_entry, symbol, data, input_section,
			 output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* If we're relocating, and this is an external symbol, we don't
     want to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  reloc_entry->addend = (reloc_entry->addend & 0x00007c0)
			| (reloc_entry->addend & 0x00000800) >> 9;

  return bfd_reloc_continue;
}

static int
mips_elf64_additional_program_headers (abfd)
     bfd *abfd;
{
  int ret = 0;

  /* See if we need a PT_MIPS_OPTIONS segment.  */
  if (bfd_get_section_by_name (abfd, ".MIPS.options"))
    ++ret;

  return ret;
}

/* Given a BFD reloc type, return a howto structure.  */

static reloc_howto_type *
mips_elf64_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  /* FIXME: We default to RELA here instead of choosing the right
     relocation variant.  */
  reloc_howto_type *howto_table = mips_elf64_howto_table_rela;

  switch (code)
    {
    case BFD_RELOC_NONE:
      return &howto_table[R_MIPS_NONE];
    case BFD_RELOC_16:
      return &howto_table[R_MIPS_16];
    case BFD_RELOC_32:
      return &howto_table[R_MIPS_32];
    case BFD_RELOC_64:
    case BFD_RELOC_CTOR:
      /* We need to handle these specially.  Select the right
	 relocation (R_MIPS_32 or R_MIPS_64) based on the
	 size of addresses on this architecture.  */
      if (bfd_arch_bits_per_address (abfd) == 32)
	return &howto_table[R_MIPS_32];
      else
	return &howto_table[R_MIPS_64];

    case BFD_RELOC_16_PCREL:
      return &howto_table[R_MIPS_PC16];
    case BFD_RELOC_HI16_S:
      return &howto_table[R_MIPS_HI16];
    case BFD_RELOC_LO16:
      return &howto_table[R_MIPS_LO16];
    case BFD_RELOC_GPREL16:
      return &howto_table[R_MIPS_GPREL16];
    case BFD_RELOC_GPREL32:
      return &howto_table[R_MIPS_GPREL32];
    case BFD_RELOC_MIPS_JMP:
      return &howto_table[R_MIPS_26];
    case BFD_RELOC_MIPS_LITERAL:
      return &howto_table[R_MIPS_LITERAL];
    case BFD_RELOC_MIPS_GOT16:
      return &howto_table[R_MIPS_GOT16];
    case BFD_RELOC_MIPS_CALL16:
      return &howto_table[R_MIPS_CALL16];
    case BFD_RELOC_MIPS_SHIFT5:
      return &howto_table[R_MIPS_SHIFT5];
    case BFD_RELOC_MIPS_SHIFT6:
      return &howto_table[R_MIPS_SHIFT6];
    case BFD_RELOC_MIPS_GOT_DISP:
      return &howto_table[R_MIPS_GOT_DISP];
    case BFD_RELOC_MIPS_GOT_PAGE:
      return &howto_table[R_MIPS_GOT_PAGE];
    case BFD_RELOC_MIPS_GOT_OFST:
      return &howto_table[R_MIPS_GOT_OFST];
    case BFD_RELOC_MIPS_GOT_HI16:
      return &howto_table[R_MIPS_GOT_HI16];
    case BFD_RELOC_MIPS_GOT_LO16:
      return &howto_table[R_MIPS_GOT_LO16];
    case BFD_RELOC_MIPS_SUB:
      return &howto_table[R_MIPS_SUB];
    case BFD_RELOC_MIPS_INSERT_A:
      return &howto_table[R_MIPS_INSERT_A];
    case BFD_RELOC_MIPS_INSERT_B:
      return &howto_table[R_MIPS_INSERT_B];
    case BFD_RELOC_MIPS_DELETE:
      return &howto_table[R_MIPS_DELETE];
    case BFD_RELOC_MIPS_HIGHEST:
      return &howto_table[R_MIPS_HIGHEST];
    case BFD_RELOC_MIPS_HIGHER:
      return &howto_table[R_MIPS_HIGHER];
    case BFD_RELOC_MIPS_CALL_HI16:
      return &howto_table[R_MIPS_CALL_HI16];
    case BFD_RELOC_MIPS_CALL_LO16:
      return &howto_table[R_MIPS_CALL_LO16];
    case BFD_RELOC_MIPS_SCN_DISP:
      return &howto_table[R_MIPS_SCN_DISP];
    case BFD_RELOC_MIPS_REL16:
      return &howto_table[R_MIPS_REL16];
    /* Use of R_MIPS_ADD_IMMEDIATE and R_MIPS_PJUMP is deprecated.  */
    case BFD_RELOC_MIPS_RELGOT:
      return &howto_table[R_MIPS_RELGOT];
    case BFD_RELOC_MIPS_JALR:
      return &howto_table[R_MIPS_JALR];
/*
    case BFD_RELOC_MIPS16_JMP:
      return &elf_mips16_jump_howto;
    case BFD_RELOC_MIPS16_GPREL:
      return &elf_mips16_gprel_howto;
    case BFD_RELOC_VTABLE_INHERIT:
      return &elf_mips_gnu_vtinherit_howto;
    case BFD_RELOC_VTABLE_ENTRY:
      return &elf_mips_gnu_vtentry_howto;
    case BFD_RELOC_PCREL_HI16_S:
      return &elf_mips_gnu_rel_hi16;
    case BFD_RELOC_PCREL_LO16:
      return &elf_mips_gnu_rel_lo16;
    case BFD_RELOC_16_PCREL_S2:
      return &elf_mips_gnu_rel16_s2;
    case BFD_RELOC_64_PCREL:
      return &elf_mips_gnu_pcrel64;
    case BFD_RELOC_32_PCREL:
      return &elf_mips_gnu_pcrel32;
*/
    default:
      bfd_set_error (bfd_error_bad_value);
      return NULL;
    }
}

/* Prevent relocation handling by bfd for MIPS ELF64.  */

static void
mips_elf64_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr ATTRIBUTE_UNUSED;
     Elf64_Internal_Rel *dst ATTRIBUTE_UNUSED;
{
  BFD_ASSERT (0);
}

static void
mips_elf64_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr ATTRIBUTE_UNUSED;
     Elf64_Internal_Rela *dst ATTRIBUTE_UNUSED;
{
  BFD_ASSERT (0);
}

/* Since each entry in an SHT_REL or SHT_RELA section can represent up
   to three relocs, we must tell the user to allocate more space.  */

static long
mips_elf64_get_reloc_upper_bound (abfd, sec)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
{
  return (sec->reloc_count * 3 + 1) * sizeof (arelent *);
}

/* Read the relocations from one reloc section.  */

static boolean
mips_elf64_slurp_one_reloc_table (abfd, asect, symbols, rel_hdr)
     bfd *abfd;
     asection *asect;
     asymbol **symbols;
     const Elf_Internal_Shdr *rel_hdr;
{
  PTR allocated = NULL;
  bfd_byte *native_relocs;
  arelent *relents;
  arelent *relent;
  bfd_vma count;
  bfd_vma i;
  int entsize;
  reloc_howto_type *howto_table;

  allocated = (PTR) bfd_malloc (rel_hdr->sh_size);
  if (allocated == NULL)
    return false;

  if (bfd_seek (abfd, rel_hdr->sh_offset, SEEK_SET) != 0
      || (bfd_bread (allocated, rel_hdr->sh_size, abfd) != rel_hdr->sh_size))
    goto error_return;

  native_relocs = (bfd_byte *) allocated;

  relents = asect->relocation + asect->reloc_count;

  entsize = rel_hdr->sh_entsize;
  BFD_ASSERT (entsize == sizeof (Elf64_Mips_External_Rel)
	      || entsize == sizeof (Elf64_Mips_External_Rela));

  count = rel_hdr->sh_size / entsize;

  if (entsize == sizeof (Elf64_Mips_External_Rel))
    howto_table = mips_elf64_howto_table_rel;
  else
    howto_table = mips_elf64_howto_table_rela;

  relent = relents;
  for (i = 0; i < count; i++, native_relocs += entsize)
    {
      Elf64_Mips_Internal_Rela rela;
      boolean used_sym, used_ssym;
      int ir;

      if (entsize == sizeof (Elf64_Mips_External_Rela))
	mips_elf64_swap_reloca_in (abfd,
				   (Elf64_Mips_External_Rela *) native_relocs,
				   &rela);
      else
	{
	  Elf64_Mips_Internal_Rel rel;

	  mips_elf64_swap_reloc_in (abfd,
				    (Elf64_Mips_External_Rel *) native_relocs,
				    &rel);
	  rela.r_offset = rel.r_offset;
	  rela.r_sym = rel.r_sym;
	  rela.r_ssym = rel.r_ssym;
	  rela.r_type3 = rel.r_type3;
	  rela.r_type2 = rel.r_type2;
	  rela.r_type = rel.r_type;
	  rela.r_addend = 0;
	}

      /* Each entry represents up to three actual relocations.  */

      used_sym = false;
      used_ssym = false;
      for (ir = 0; ir < 3; ir++)
	{
	  enum elf_mips_reloc_type type;

	  switch (ir)
	    {
	    default:
	      abort ();
	    case 0:
	      type = (enum elf_mips_reloc_type) rela.r_type;
	      break;
	    case 1:
	      type = (enum elf_mips_reloc_type) rela.r_type2;
	      break;
	    case 2:
	      type = (enum elf_mips_reloc_type) rela.r_type3;
	      break;
	    }

	  if (type == R_MIPS_NONE)
	    {
	      /* There are no more relocations in this entry.  If this
                 is the first entry, we need to generate a dummy
                 relocation so that the generic linker knows that
                 there has been a break in the sequence of relocations
                 applying to a particular address.  */
	      if (ir == 0)
		{
		  relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
		  if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
		    relent->address = rela.r_offset;
		  else
		    relent->address = rela.r_offset - asect->vma;
		  relent->addend = 0;
		  relent->howto = &howto_table[(int) R_MIPS_NONE];
		  ++relent;
		}
	      break;
	    }

	  /* Some types require symbols, whereas some do not.  */
	  switch (type)
	    {
	    case R_MIPS_NONE:
	    case R_MIPS_LITERAL:
	    case R_MIPS_INSERT_A:
	    case R_MIPS_INSERT_B:
	    case R_MIPS_DELETE:
	      relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	      break;

	    default:
	      if (! used_sym)
		{
		  if (rela.r_sym == 0)
		    relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
		  else
		    {
		      asymbol **ps, *s;

		      ps = symbols + rela.r_sym - 1;
		      s = *ps;
		      if ((s->flags & BSF_SECTION_SYM) == 0)
			relent->sym_ptr_ptr = ps;
		      else
			relent->sym_ptr_ptr = s->section->symbol_ptr_ptr;
		    }

		  used_sym = true;
		}
	      else if (! used_ssym)
		{
		  switch (rela.r_ssym)
		    {
		    case RSS_UNDEF:
		      relent->sym_ptr_ptr =
			bfd_abs_section_ptr->symbol_ptr_ptr;
		      break;

		    case RSS_GP:
		    case RSS_GP0:
		    case RSS_LOC:
		      /* FIXME: I think these need to be handled using
                         special howto structures.  */
		      BFD_ASSERT (0);
		      break;

		    default:
		      BFD_ASSERT (0);
		      break;
		    }

		  used_ssym = true;
		}
	      else
		relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;

	      break;
	    }

	  /* The address of an ELF reloc is section relative for an
	     object file, and absolute for an executable file or
	     shared library.  The address of a BFD reloc is always
	     section relative.  */
	  if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	    relent->address = rela.r_offset;
	  else
	    relent->address = rela.r_offset - asect->vma;

	  relent->addend = rela.r_addend;

	  relent->howto = &howto_table[(int) type];

	  ++relent;
	}
    }

  asect->reloc_count += relent - relents;

  if (allocated != NULL)
    free (allocated);

  return true;

 error_return:
  if (allocated != NULL)
    free (allocated);
  return false;
}

/* Read the relocations.  On Irix 6, there can be two reloc sections
   associated with a single data section.  */

static boolean
mips_elf64_slurp_reloc_table (abfd, asect, symbols, dynamic)
     bfd *abfd;
     asection *asect;
     asymbol **symbols;
     boolean dynamic;
{
  bfd_size_type amt;
  struct bfd_elf_section_data * const d = elf_section_data (asect);

  if (dynamic)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  if (asect->relocation != NULL
      || (asect->flags & SEC_RELOC) == 0
      || asect->reloc_count == 0)
    return true;

  /* Allocate space for 3 arelent structures for each Rel structure.  */
  amt = asect->reloc_count;
  amt *= 3 * sizeof (arelent);
  asect->relocation = (arelent *) bfd_alloc (abfd, amt);
  if (asect->relocation == NULL)
    return false;

  /* The slurp_one_reloc_table routine increments reloc_count.  */
  asect->reloc_count = 0;

  if (! mips_elf64_slurp_one_reloc_table (abfd, asect, symbols, &d->rel_hdr))
    return false;
  if (d->rel_hdr2 != NULL)
    {
      if (! mips_elf64_slurp_one_reloc_table (abfd, asect, symbols,
					      d->rel_hdr2))
	return false;
    }

  return true;
}

/* Write out the relocations.  */

static void
mips_elf64_write_relocs (abfd, sec, data)
     bfd *abfd;
     asection *sec;
     PTR data;
{
  boolean *failedp = (boolean *) data;
  int count;
  Elf_Internal_Shdr *rel_hdr;
  unsigned int idx;

  /* If we have already failed, don't do anything.  */
  if (*failedp)
    return;

  if ((sec->flags & SEC_RELOC) == 0)
    return;

  /* The linker backend writes the relocs out itself, and sets the
     reloc_count field to zero to inhibit writing them here.  Also,
     sometimes the SEC_RELOC flag gets set even when there aren't any
     relocs.  */
  if (sec->reloc_count == 0)
    return;

  /* We can combine up to three relocs that refer to the same address
     if the latter relocs have no associated symbol.  */
  count = 0;
  for (idx = 0; idx < sec->reloc_count; idx++)
    {
      bfd_vma addr;
      unsigned int i;

      ++count;

      addr = sec->orelocation[idx]->address;
      for (i = 0; i < 2; i++)
	{
	  arelent *r;

	  if (idx + 1 >= sec->reloc_count)
	    break;
	  r = sec->orelocation[idx + 1];
	  if (r->address != addr
	      || ! bfd_is_abs_section ((*r->sym_ptr_ptr)->section)
	      || (*r->sym_ptr_ptr)->value != 0)
	    break;

	  /* We can merge the reloc at IDX + 1 with the reloc at IDX.  */

	  ++idx;
	}
    }

  rel_hdr = &elf_section_data (sec)->rel_hdr;

  /* Do the actual relocation.  */

  if (rel_hdr->sh_entsize == sizeof(Elf64_Mips_External_Rel))
    mips_elf64_write_rel (abfd, sec, rel_hdr, &count, data);
  else if (rel_hdr->sh_entsize == sizeof(Elf64_Mips_External_Rela))
    mips_elf64_write_rela (abfd, sec, rel_hdr, &count, data);
  else
    BFD_ASSERT (0);
}

static void
mips_elf64_write_rel (abfd, sec, rel_hdr, count, data)
     bfd *abfd;
     asection *sec;
     Elf_Internal_Shdr *rel_hdr;
     int *count;
     PTR data;
{
  boolean *failedp = (boolean *) data;
  Elf64_Mips_External_Rel *ext_rel;
  unsigned int idx;
  asymbol *last_sym = 0;
  int last_sym_idx = 0;

  rel_hdr->sh_size = (bfd_vma)(rel_hdr->sh_entsize * *count);
  rel_hdr->contents = (PTR) bfd_alloc (abfd, rel_hdr->sh_size);
  if (rel_hdr->contents == NULL)
    {
      *failedp = true;
      return;
    }

  ext_rel = (Elf64_Mips_External_Rel *) rel_hdr->contents;
  for (idx = 0; idx < sec->reloc_count; idx++, ext_rel++)
    {
      arelent *ptr;
      Elf64_Mips_Internal_Rel int_rel;
      asymbol *sym;
      int n;
      unsigned int i;

      ptr = sec->orelocation[idx];

      /* The address of an ELF reloc is section relative for an object
	 file, and absolute for an executable file or shared library.
	 The address of a BFD reloc is always section relative.  */
      if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	int_rel.r_offset = ptr->address;
      else
	int_rel.r_offset = ptr->address + sec->vma;

      sym = *ptr->sym_ptr_ptr;
      if (sym == last_sym)
	n = last_sym_idx;
      else
	{
	  last_sym = sym;
	  n = _bfd_elf_symbol_from_bfd_symbol (abfd, &sym);
	  if (n < 0)
	    {
	      *failedp = true;
	      return;
	    }
	  last_sym_idx = n;
	}

      int_rel.r_sym = n;
      int_rel.r_ssym = RSS_UNDEF;

      if ((*ptr->sym_ptr_ptr)->the_bfd->xvec != abfd->xvec
	  && ! _bfd_elf_validate_reloc (abfd, ptr))
	{
	  *failedp = true;
	  return;
	}

      int_rel.r_type = ptr->howto->type;
      int_rel.r_type2 = (int) R_MIPS_NONE;
      int_rel.r_type3 = (int) R_MIPS_NONE;

      for (i = 0; i < 2; i++)
	{
	  arelent *r;

	  if (idx + 1 >= sec->reloc_count)
	    break;
	  r = sec->orelocation[idx + 1];
	  if (r->address != ptr->address
	      || ! bfd_is_abs_section ((*r->sym_ptr_ptr)->section)
	      || (*r->sym_ptr_ptr)->value != 0)
	    break;

	  /* We can merge the reloc at IDX + 1 with the reloc at IDX.  */

	  if (i == 0)
	    int_rel.r_type2 = r->howto->type;
	  else
	    int_rel.r_type3 = r->howto->type;

	  ++idx;
	}

      mips_elf64_swap_reloc_out (abfd, &int_rel, ext_rel);
    }

  BFD_ASSERT (ext_rel - (Elf64_Mips_External_Rel *) rel_hdr->contents
	      == *count);
}

static void
mips_elf64_write_rela (abfd, sec, rela_hdr, count, data)
     bfd *abfd;
     asection *sec;
     Elf_Internal_Shdr *rela_hdr;
     int *count;
     PTR data;
{
  boolean *failedp = (boolean *) data;
  Elf64_Mips_External_Rela *ext_rela;
  unsigned int idx;
  asymbol *last_sym = 0;
  int last_sym_idx = 0;

  rela_hdr->sh_size = (bfd_vma)(rela_hdr->sh_entsize * *count);
  rela_hdr->contents = (PTR) bfd_alloc (abfd, rela_hdr->sh_size);
  if (rela_hdr->contents == NULL)
    {
      *failedp = true;
      return;
    }

  ext_rela = (Elf64_Mips_External_Rela *) rela_hdr->contents;
  for (idx = 0; idx < sec->reloc_count; idx++, ext_rela++)
    {
      arelent *ptr;
      Elf64_Mips_Internal_Rela int_rela;
      asymbol *sym;
      int n;
      unsigned int i;

      ptr = sec->orelocation[idx];

      /* The address of an ELF reloc is section relative for an object
	 file, and absolute for an executable file or shared library.
	 The address of a BFD reloc is always section relative.  */
      if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	int_rela.r_offset = ptr->address;
      else
	int_rela.r_offset = ptr->address + sec->vma;

      sym = *ptr->sym_ptr_ptr;
      if (sym == last_sym)
	n = last_sym_idx;
      else
	{
	  last_sym = sym;
	  n = _bfd_elf_symbol_from_bfd_symbol (abfd, &sym);
	  if (n < 0)
	    {
	      *failedp = true;
	      return;
	    }
	  last_sym_idx = n;
	}

      int_rela.r_sym = n;
      int_rela.r_addend = ptr->addend;
      int_rela.r_ssym = RSS_UNDEF;

      if ((*ptr->sym_ptr_ptr)->the_bfd->xvec != abfd->xvec
	  && ! _bfd_elf_validate_reloc (abfd, ptr))
	{
	  *failedp = true;
	  return;
	}

      int_rela.r_type = ptr->howto->type;
      int_rela.r_type2 = (int) R_MIPS_NONE;
      int_rela.r_type3 = (int) R_MIPS_NONE;

      for (i = 0; i < 2; i++)
	{
	  arelent *r;

	  if (idx + 1 >= sec->reloc_count)
	    break;
	  r = sec->orelocation[idx + 1];
	  if (r->address != ptr->address
	      || ! bfd_is_abs_section ((*r->sym_ptr_ptr)->section)
	      || (*r->sym_ptr_ptr)->value != 0)
	    break;

	  /* We can merge the reloc at IDX + 1 with the reloc at IDX.  */

	  if (i == 0)
	    int_rela.r_type2 = r->howto->type;
	  else
	    int_rela.r_type3 = r->howto->type;

	  ++idx;
	}

      mips_elf64_swap_reloca_out (abfd, &int_rela, ext_rela);
    }

  BFD_ASSERT (ext_rela - (Elf64_Mips_External_Rela *) rela_hdr->contents
	      == *count);
}

/* This structure is used to hold .got information when linking.  It
   is stored in the tdata field of the bfd_elf_section_data structure.  */

struct mips_elf64_got_info
{
  /* The global symbol in the GOT with the lowest index in the dynamic
     symbol table.  */
  struct elf_link_hash_entry *global_gotsym;
  /* The number of global .got entries.  */
  unsigned int global_gotno;
  /* The number of local .got entries.  */
  unsigned int local_gotno;
  /* The number of local .got entries we have used.  */
  unsigned int assigned_gotno;
};

/* The MIPS ELF64 linker needs additional information for each symbol in
   the global hash table.  */

struct mips_elf64_link_hash_entry
{
  struct elf_link_hash_entry root;

  /* External symbol information.  */
  EXTR esym;

  /* Number of R_MIPS_32, R_MIPS_REL32, or R_MIPS_64 relocs against
     this symbol.  */ 
  unsigned int possibly_dynamic_relocs;

  /* If the R_MIPS_32, R_MIPS_REL32, or R_MIPS_64 reloc is against
     a readonly section.  */
  boolean readonly_reloc;

  /* The index of the first dynamic relocation (in the .rel.dyn
     section) against this symbol.  */
  unsigned int min_dyn_reloc_index;

  /* We must not create a stub for a symbol that has relocations
     related to taking the function's address, i.e. any but
     R_MIPS_CALL*16 ones -- see "MIPS ABI Supplement, 3rd Edition",
     p. 4-20.  */
  boolean no_fn_stub;

  /* If there is a stub that 32 bit functions should use to call this
     16 bit function, this points to the section containing the stub.  */
  asection *fn_stub;

  /* Whether we need the fn_stub; this is set if this symbol appears
     in any relocs other than a 16 bit call.  */
  boolean need_fn_stub;

  /* If there is a stub that 16 bit functions should use to call this
     32 bit function, this points to the section containing the stub.  */
  asection *call_stub;

  /* This is like the call_stub field, but it is used if the function
     being called returns a floating point value.  */
  asection *call_fp_stub;
};

  /* The mips16 compiler uses a couple of special sections to handle
     floating point arguments.

     Section names that look like .mips16.fn.FNNAME contain stubs that
     copy floating point arguments from the fp regs to the gp regs and
     then jump to FNNAME.  If any 32 bit function calls FNNAME, the
     call should be redirected to the stub instead.  If no 32 bit
     function calls FNNAME, the stub should be discarded.  We need to
     consider any reference to the function, not just a call, because
     if the address of the function is taken we will need the stub,
     since the address might be passed to a 32 bit function.

     Section names that look like .mips16.call.FNNAME contain stubs
     that copy floating point arguments from the gp regs to the fp
     regs and then jump to FNNAME.  If FNNAME is a 32 bit function,
     then any 16 bit function that calls FNNAME should be redirected
     to the stub instead.  If FNNAME is not a 32 bit function, the
     stub should be discarded.

     .mips16.call.fp.FNNAME sections are similar, but contain stubs
     which call FNNAME and then copy the return value from the fp regs
     to the gp regs.  These stubs store the return value in $18 while
     calling FNNAME; any function which might call one of these stubs
     must arrange to save $18 around the call.  (This case is not
     needed for 32 bit functions that call 16 bit functions, because
     16 bit functions always return floating point values in both
     $f0/$f1 and $2/$3.)

     Note that in all cases FNNAME might be defined statically.
     Therefore, FNNAME is not used literally.  Instead, the relocation
     information will indicate which symbol the section is for.

     We record any stubs that we find in the symbol table.  */

#define FN_STUB ".mips16.fn."
#define CALL_STUB ".mips16.call."
#define CALL_FP_STUB ".mips16.call.fp."

/* MIPS ELF64 linker hash table.  */

struct mips_elf64_link_hash_table
{
  struct elf_link_hash_table root;
  /* This is set if we see any mips16 stub sections.  */
  boolean mips16_stubs_seen;
};

/* Look up an entry in a MIPS ELF64 linker hash table.  */

#define mips_elf64_link_hash_lookup(table, string, create, copy, follow) \
  ((struct mips_elf64_link_hash_entry *)				\
   elf_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

/* Traverse a MIPS ELF linker hash table.  */

#define mips_elf64_link_hash_traverse(table, func, info)		\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the MIPS ELF64 linker hash table from a link_info structure.  */

#define mips_elf64_hash_table(p) \
  ((struct mips_elf64_link_hash_table *) ((p)->hash))

/* Create an entry in a MIPS ELF64 linker hash table.  */

static struct bfd_hash_entry *
mips_elf64_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct mips_elf64_link_hash_entry *ret =
    (struct mips_elf64_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct mips_elf64_link_hash_entry *) NULL)
    ret = ((struct mips_elf64_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct mips_elf64_link_hash_entry)));
  if (ret == (struct mips_elf64_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct mips_elf64_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != (struct mips_elf64_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      memset (&ret->esym, 0, sizeof (EXTR));
      /* We use -2 as a marker to indicate that the information has
	 not been set.  -1 means there is no associated ifd.  */
      ret->esym.ifd = -2;
      ret->possibly_dynamic_relocs = 0;
      ret->readonly_reloc = false;
      ret->min_dyn_reloc_index = 0;
      ret->no_fn_stub = false;
      ret->fn_stub = NULL;
      ret->need_fn_stub = false;
      ret->call_stub = NULL;
      ret->call_fp_stub = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create a MIPS ELF64 linker hash table.  */

struct bfd_link_hash_table *
mips_elf64_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct mips_elf64_link_hash_table *ret;

  ret = ((struct mips_elf64_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct mips_elf64_link_hash_table)));
  if (ret == (struct mips_elf64_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
				       mips_elf64_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  ret->mips16_stubs_seen = false;

  return &ret->root.root;
}

/* Returns the offset for the entry at the INDEXth position
   in the GOT.  */

static bfd_vma
mips_elf64_got_offset_from_index (dynobj, output_bfd, index)
     bfd *dynobj;
     bfd *output_bfd;
     bfd_vma index;
{
  asection *sgot;
  bfd_vma gp;

  sgot = bfd_get_section_by_name (dynobj, ".got");
  gp = _bfd_get_gp_value (output_bfd);
  return (sgot->output_section->vma + sgot->output_offset + index - 
	  gp);
}

/* Returns the GOT information associated with the link indicated by
   INFO.  If SGOTP is non-NULL, it is filled in with the GOT 
   section.  */

static struct mips_elf64_got_info *
_mips_elf64_got_info (abfd, sgotp)
     bfd *abfd;
     asection **sgotp;
{
  asection *sgot;
  struct mips_elf64_got_info *g;

  sgot = bfd_get_section_by_name (abfd, ".got");
  BFD_ASSERT (sgot != NULL);
  BFD_ASSERT (elf_section_data (sgot) != NULL);
  g = (struct mips_elf64_got_info *) elf_section_data (sgot)->tdata;
  BFD_ASSERT (g != NULL);

  if (sgotp)
    *sgotp = sgot;
  return g;
}

/* Sign-extend VALUE, which has the indicated number of BITS.  */

static bfd_vma
mips_elf64_sign_extend (value, bits)
     bfd_vma value;
     int bits;
{
  if (value & ((bfd_vma)1 << (bits - 1)))
    /* VALUE is negative.  */
    value |= ((bfd_vma) - 1) << bits;      
  
  return value;
}

/* Return non-zero if the indicated VALUE has overflowed the maximum
   range expressable by a signed number with the indicated number of
   BITS.  */

static boolean
mips_elf64_overflow_p (value, bits)
     bfd_vma value;
     int bits;
{
  bfd_signed_vma svalue = (bfd_signed_vma) value;

  if (svalue > (1 << (bits - 1)) - 1)
    /* The value is too big.  */
    return true;
  else if (svalue < -(1 << (bits - 1)))
    /* The value is too small.  */
    return true;
    
  /* All is well.  */
  return false;
}

/* Returns the GOT index for the global symbol indicated by H.  */

static bfd_vma 
mips_elf64_global_got_index (abfd, h)
     bfd *abfd;
     struct elf_link_hash_entry *h;
{
  bfd_vma index;
  asection *sgot;
  struct mips_elf64_got_info *g;

  g = _mips_elf64_got_info (abfd, &sgot);

  /* Once we determine the global GOT entry with the lowest dynamic
     symbol table index, we must put all dynamic symbols with greater
     indices into the GOT.  That makes it easy to calculate the GOT
     offset.  */
  BFD_ASSERT (h->dynindx >= g->global_gotsym->dynindx);
  index = ((h->dynindx - g->global_gotsym->dynindx + g->local_gotno) 
	   * (get_elf_backend_data (abfd)->s->arch_size / 8));
  BFD_ASSERT (index < sgot->_raw_size);

  return index;
}

struct mips_elf64_hash_sort_data
{
  /* The symbol in the global GOT with the lowest dynamic symbol table
     index.  */
  struct elf_link_hash_entry *low;
  /* The least dynamic symbol table index corresponding to a symbol
     with a GOT entry.  */
  long min_got_dynindx;
  /* The greatest dynamic symbol table index not corresponding to a
     symbol without a GOT entry.  */
  long max_non_got_dynindx;
};

/* If H needs a GOT entry, assign it the highest available dynamic
   index.  Otherwise, assign it the lowest available dynamic 
   index.  */

static boolean
mips_elf64_sort_hash_table_f (h, data)
     struct mips_elf64_link_hash_entry *h;
     PTR data;
{
  struct mips_elf64_hash_sort_data *hsd 
    = (struct mips_elf64_hash_sort_data *) data;

  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct mips_elf64_link_hash_entry *) h->root.root.u.i.link;

  /* Symbols without dynamic symbol table entries aren't interesting
     at all.  */
  if (h->root.dynindx == -1)
    return true;

  if (h->root.got.offset != 1)
    h->root.dynindx = hsd->max_non_got_dynindx++;
  else
    {
      h->root.dynindx = --hsd->min_got_dynindx;
      hsd->low = (struct elf_link_hash_entry *) h;
    }

  return true;
}

/* Sort the dynamic symbol table so that symbols that need GOT entries
   appear towards the end.  This reduces the amount of GOT space
   required.  MAX_LOCAL is used to set the number of local symbols
   known to be in the dynamic symbol table.  During
   mips_elf64_size_dynamic_sections, this value is 1.  Afterward, the
   section symbols are added and the count is higher.  */

static boolean
mips_elf64_sort_hash_table (info, max_local)
     struct bfd_link_info *info;
     unsigned long max_local;
{
  struct mips_elf64_hash_sort_data hsd;
  struct mips_elf64_got_info *g;
  bfd *dynobj;

  dynobj = elf_hash_table (info)->dynobj;

  hsd.low = NULL;
  hsd.min_got_dynindx = elf_hash_table (info)->dynsymcount;
  hsd.max_non_got_dynindx = max_local;
  mips_elf64_link_hash_traverse (((struct mips_elf64_link_hash_table *) 
				  elf_hash_table (info)), 
				 mips_elf64_sort_hash_table_f, 
				 &hsd);

  /* There shoud have been enough room in the symbol table to
     accomodate both the GOT and non-GOT symbols.  */
  BFD_ASSERT (hsd.max_non_got_dynindx <= hsd.min_got_dynindx);

  /* Now we know which dynamic symbol has the lowest dynamic symbol
     table index in the GOT.  */
  g = _mips_elf64_got_info (dynobj, NULL);
  g->global_gotsym = hsd.low;

  return true;
}

#if 0
/* Swap in an MSYM entry.  */

static void
mips_elf64_swap_msym_in (abfd, ex, in)
     bfd *abfd;
     const Elf32_External_Msym *ex;
     Elf32_Internal_Msym *in;
{
  in->ms_hash_value = H_GET_32 (abfd, ex->ms_hash_value);
  in->ms_info = H_GET_32 (abfd, ex->ms_info);
}
#endif
/* Swap out an MSYM entry.  */

static void
mips_elf64_swap_msym_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_Internal_Msym *in;
     Elf32_External_Msym *ex;
{
  H_PUT_32 (abfd, in->ms_hash_value, ex->ms_hash_value);
  H_PUT_32 (abfd, in->ms_info, ex->ms_info);
}

/* Create a local GOT entry for VALUE.  Return the index of the entry,
   or -1 if it could not be created.  */

static bfd_vma
mips_elf64_create_local_got_entry (abfd, g, sgot, value)
     bfd *abfd;
     struct mips_elf64_got_info *g;
     asection *sgot;
     bfd_vma value;
{
  CONST bfd_vma got_size = get_elf_backend_data (abfd)->s->arch_size / 8;
  
  if (g->assigned_gotno >= g->local_gotno)
    {
      /* We didn't allocate enough space in the GOT.  */
      (*_bfd_error_handler)
	(_("not enough GOT space for local GOT entries"));
      bfd_set_error (bfd_error_bad_value);
      return (bfd_vma) -1;
    }

  bfd_put_64 (abfd, value, (sgot->contents + got_size * g->assigned_gotno));
  return got_size * g->assigned_gotno++;
}

/* Returns the GOT offset at which the indicated address can be found.
   If there is not yet a GOT entry for this value, create one.  Returns
   -1 if no satisfactory GOT offset can be found.  */

static bfd_vma
mips_elf64_local_got_index (abfd, info, value)
     bfd *abfd;
     struct bfd_link_info *info;
     bfd_vma value;
{
  CONST bfd_vma got_size = get_elf_backend_data (abfd)->s->arch_size / 8;
  asection *sgot;
  struct mips_elf64_got_info *g;
  bfd_byte *entry;

  g = _mips_elf64_got_info (elf_hash_table (info)->dynobj, &sgot);

  /* Look to see if we already have an appropriate entry.  */
  for (entry = (sgot->contents + got_size * MIPS_RESERVED_GOTNO); 
       entry != sgot->contents + got_size * g->assigned_gotno;
       entry += got_size)
    {
      bfd_vma address = bfd_get_64 (abfd, entry);
      if (address == value)
	return entry - sgot->contents;
    }

  return mips_elf64_create_local_got_entry (abfd, g, sgot, value);
}

/* Find a GOT entry that is within 32KB of the VALUE.  These entries
   are supposed to be placed at small offsets in the GOT, i.e.,
   within 32KB of GP.  Return the index into the GOT for this page,
   and store the offset from this entry to the desired address in
   OFFSETP, if it is non-NULL.  */

static bfd_vma
mips_elf64_got_page (abfd, info, value, offsetp)
     bfd *abfd;
     struct bfd_link_info *info;
     bfd_vma value;
     bfd_vma *offsetp;
{
  CONST bfd_vma got_size = get_elf_backend_data (abfd)->s->arch_size / 8;
  asection *sgot;
  struct mips_elf64_got_info *g;
  bfd_byte *entry;
  bfd_byte *last_entry;
  bfd_vma index = 0;
  bfd_vma address;

  g = _mips_elf64_got_info (elf_hash_table (info)->dynobj, &sgot);

  /* Look to see if we aleady have an appropriate entry.  */
  last_entry = sgot->contents + got_size * g->assigned_gotno;
  for (entry = (sgot->contents + got_size * MIPS_RESERVED_GOTNO);
       entry != last_entry;
       entry += got_size)
    {
      address = bfd_get_64 (abfd, entry);

      if (!mips_elf64_overflow_p (value - address, 16))
	{
	  /* This entry will serve as the page pointer.  We can add a
	     16-bit number to it to get the actual address.  */
	  index = entry - sgot->contents;
	  break;
	}
    }

  /* If we didn't have an appropriate entry, we create one now.  */
  if (entry == last_entry)
    index = mips_elf64_create_local_got_entry (abfd, g, sgot, value);

  if (offsetp)
    {
      address = bfd_get_64 (abfd, entry);
      *offsetp = value - address;
    }

  return index;
}

/* Find a GOT entry whose higher-order 16 bits are the same as those
   for value.  Return the index into the GOT for this entry.  */

static bfd_vma
mips_elf64_got16_entry (abfd, info, value, external)
     bfd *abfd;
     struct bfd_link_info *info;
     bfd_vma value;
     boolean external;
{
  CONST bfd_vma got_size = get_elf_backend_data (abfd)->s->arch_size / 8;
  asection *sgot;
  struct mips_elf64_got_info *g;
  bfd_byte *entry;
  bfd_byte *last_entry;
  bfd_vma index = 0;
  bfd_vma address;

  if (! external)
    {
      /* Although the ABI says that it is "the high-order 16 bits" that we
	 want, it is really the %high value.  The complete value is
	 calculated with a `addiu' of a LO16 relocation, just as with a
	 HI16/LO16 pair.  */
      value = mips_elf64_high (value) << 16;
    }

  g = _mips_elf64_got_info (elf_hash_table (info)->dynobj, &sgot);

  /* Look to see if we already have an appropriate entry.  */
  last_entry = sgot->contents + got_size * g->assigned_gotno;
  for (entry = (sgot->contents + got_size * MIPS_RESERVED_GOTNO);
       entry != last_entry;
       entry += got_size)
    {
      address = bfd_get_64 (abfd, entry);
      if (address == value)
	{
	  /* This entry has the right high-order 16 bits, and the low-order
	     16 bits are set to zero.  */
	  index = entry - sgot->contents;
	  break;
	}
    }

  /* If we didn't have an appropriate entry, we create one now.  */
  if (entry == last_entry)
    index = mips_elf64_create_local_got_entry (abfd, g, sgot, value);

  return index;
}

/* Return whether a relocation is against a local symbol.  */

static boolean
mips_elf64_local_relocation_p (input_bfd, relocation, local_sections,
			     check_forced)
     bfd *input_bfd;
     const Elf_Internal_Rela *relocation;
     asection **local_sections;
     boolean check_forced;
{
  unsigned long r_symndx;
  Elf_Internal_Shdr *symtab_hdr;
  struct mips_elf64_link_hash_entry* h;
  size_t extsymoff;

  r_symndx = ELF64_R_SYM (relocation->r_info);
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  extsymoff = (elf_bad_symtab (input_bfd)) ? 0 : symtab_hdr->sh_info;

  if (r_symndx < extsymoff)
    return true;
  if (elf_bad_symtab (input_bfd) && local_sections[r_symndx] != NULL)
    return true;

  if (check_forced)
    {
       /* Look up the hash table to check whether the symbol
 	 was forced local.  */
       h = (struct mips_elf64_link_hash_entry *)
 	  elf_sym_hashes (input_bfd) [r_symndx - extsymoff];
       /* Find the real hash-table entry for this symbol.  */
       while (h->root.root.type == bfd_link_hash_indirect
 	     || h->root.root.type == bfd_link_hash_warning)
         h = (struct mips_elf64_link_hash_entry *) h->root.root.u.i.link;
       if ((h->root.elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
         return true;
    }

  return false;
}

/* Returns the first relocation of type r_type found, beginning with
   RELOCATION.  RELEND is one-past-the-end of the relocation table.  */

static const Elf_Internal_Rela *
mips_elf64_next_relocation (r_type, relocation, relend)
     unsigned int r_type;
     const Elf_Internal_Rela *relocation;
     const Elf_Internal_Rela *relend;
{
  /* According to the MIPS ELF ABI, the R_MIPS_LO16 relocation must be
     immediately following.  However, for the IRIX6 ABI, the next
     relocation may be a composed relocation consisting of several
     relocations for the same address.  In that case, the R_MIPS_LO16
     relocation may occur as one of these.  We permit a similar
     extension in general, as that is useful for GCC.  */
  while (relocation < relend)
    {
      if (ELF64_MIPS_R_TYPE (relocation->r_info) == r_type)
	return relocation;

      ++relocation;
    }

  /* We didn't find it.  */
  bfd_set_error (bfd_error_bad_value);
  return NULL;
}

/* Create a rel.dyn relocation for the dynamic linker to resolve.  REL
   is the original relocation, which is now being transformed into a
   dynamic relocation.  The ADDENDP is adjusted if necessary; the
   caller should store the result in place of the original addend.  */

static boolean
mips_elf64_create_dynamic_relocation (output_bfd, info, rel, h, sec,
				    symbol, addendp, input_section)
     bfd *output_bfd;
     struct bfd_link_info *info;
     const Elf_Internal_Rela *rel;
     struct mips_elf64_link_hash_entry *h;
     asection *sec;
     bfd_vma symbol;
     bfd_vma *addendp;
     asection *input_section;
{
  Elf_Internal_Rel outrel[3];
  boolean skip;
  asection *sreloc;
  bfd *dynobj;
  int r_type;

  r_type = ELF64_MIPS_R_TYPE (rel->r_info);
  dynobj = elf_hash_table (info)->dynobj;
  sreloc = bfd_get_section_by_name (dynobj, ".rel.dyn");
  BFD_ASSERT (sreloc != NULL);
  BFD_ASSERT (sreloc->contents != NULL);
  BFD_ASSERT ((sreloc->reloc_count
	       * get_elf_backend_data (output_bfd)->s->sizeof_rel)
	      < sreloc->_raw_size);

  skip = false;
  outrel[0].r_offset = _bfd_elf_section_offset (output_bfd, info,
						input_section,
						rel[0].r_offset);
  /* FIXME: For -2 runtime relocation needs to be skipped, but
     properly resolved statically and installed.  */
  BFD_ASSERT (outrel[0].r_offset != (bfd_vma) -2);

  /* We begin by assuming that the offset for the dynamic relocation
     is the same as for the original relocation.  We'll adjust this
     later to reflect the correct output offsets.  */
  if (elf_section_data (input_section)->sec_info_type != ELF_INFO_TYPE_STABS)
    {
      outrel[1].r_offset = rel[1].r_offset;
      outrel[2].r_offset = rel[2].r_offset;
    }
  else
    {
      /* Except that in a stab section things are more complex.
	 Because we compress stab information, the offset given in the
	 relocation may not be the one we want; we must let the stabs
	 machinery tell us the offset.  */
      outrel[1].r_offset = outrel[0].r_offset;
      outrel[2].r_offset = outrel[0].r_offset;
      /* If we didn't need the relocation at all, this value will be
	 -1.  */
      if (outrel[0].r_offset == (bfd_vma) -1)
	skip = true;
    }

  /* If we've decided to skip this relocation, just output an empty
     record.  Note that R_MIPS_NONE == 0, so that this call to memset
     is a way of setting R_TYPE to R_MIPS_NONE.  */
  if (skip)
    memset (outrel, 0, sizeof (Elf_Internal_Rel) * 3);
  else
    {
      long indx;
      bfd_vma section_offset;

      /* We must now calculate the dynamic symbol table index to use
	 in the relocation.  */
      if (h != NULL
	  && (! info->symbolic || (h->root.elf_link_hash_flags
				   & ELF_LINK_HASH_DEF_REGULAR) == 0))
	{
	  indx = h->root.dynindx;
	  /* h->root.dynindx may be -1 if this symbol was marked to
	     become local.  */
	  if (indx == -1)
		indx = 0;
	}
      else
	{
	  if (sec != NULL && bfd_is_abs_section (sec))
	    indx = 0;
	  else if (sec == NULL || sec->owner == NULL)
	    {
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	  else
	    {
	      indx = elf_section_data (sec->output_section)->dynindx;
	      if (indx == 0)
		abort ();
	    }

	  /* Figure out how far the target of the relocation is from
	     the beginning of its section.  */
	  section_offset = symbol - sec->output_section->vma;
	  /* The relocation we're building is section-relative.
	     Therefore, the original addend must be adjusted by the
	     section offset.  */
	  *addendp += section_offset;
	  /* Now, the relocation is just against the section.  */
	  symbol = sec->output_section->vma;
	}
      
      /* If the relocation was previously an absolute relocation and
	 this symbol will not be referred to by the relocation, we must
	 adjust it by the value we give it in the dynamic symbol table.
	 Otherwise leave the job up to the dynamic linker.  */
      if (!indx && r_type != R_MIPS_REL32)
	*addendp += symbol;

      /* The relocation is always an REL32 relocation because we don't
	 know where the shared library will wind up at load-time.  */
      outrel[0].r_info = ELF64_R_INFO (indx, R_MIPS_REL32);

      /* Adjust the output offset of the relocation to reference the
	 correct location in the output file.  */
      outrel[0].r_offset += (input_section->output_section->vma
			     + input_section->output_offset);
      outrel[1].r_offset += (input_section->output_section->vma
			     + input_section->output_offset);
      outrel[2].r_offset += (input_section->output_section->vma
			     + input_section->output_offset);
    }

  /* Put the relocation back out.  */
  mips_elf64_be_swap_reloc_out (output_bfd, outrel,
				(sreloc->contents 
				 + sreloc->reloc_count
				   * sizeof (Elf64_Mips_External_Rel)));

  /* Record the index of the first relocation referencing H.  This
     information is later emitted in the .msym section.  */
  if (h != NULL
      && (h->min_dyn_reloc_index == 0 
	  || sreloc->reloc_count < h->min_dyn_reloc_index))
    h->min_dyn_reloc_index = sreloc->reloc_count;

  /* We've now added another relocation.  */
  ++sreloc->reloc_count;

  /* Make sure the output section is writable.  The dynamic linker
     will be writing to it.  */
  elf_section_data (input_section->output_section)->this_hdr.sh_flags
    |= SHF_WRITE;

  return true;
}

/* Calculate the value produced by the RELOCATION (which comes from
   the INPUT_BFD).  The ADDEND is the addend to use for this
   RELOCATION; RELOCATION->R_ADDEND is ignored.

   The result of the relocation calculation is stored in VALUEP.
   REQUIRE_JALXP indicates whether or not the opcode used with this
   relocation must be JALX.

   This function returns bfd_reloc_continue if the caller need take no
   further action regarding this relocation, bfd_reloc_notsupported if
   something goes dramatically wrong, bfd_reloc_overflow if an
   overflow occurs, and bfd_reloc_ok to indicate success.  */

static bfd_reloc_status_type
mips_elf64_calculate_relocation (abfd, input_bfd, input_section, info,
				 relocation, addend, howto, local_syms,
				 local_sections, valuep, namep, require_jalxp)
     bfd *abfd;
     bfd *input_bfd;
     asection *input_section;
     struct bfd_link_info *info;
     const Elf_Internal_Rela *relocation;
     bfd_vma addend;
     reloc_howto_type *howto;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
     bfd_vma *valuep;
     const char **namep;
     boolean *require_jalxp;
{
  /* The eventual value we will return.  */
  bfd_vma value;
  /* The address of the symbol against which the relocation is
     occurring.  */
  bfd_vma symbol = 0;
  /* The final GP value to be used for the relocatable, executable, or
     shared object file being produced.  */
  bfd_vma gp = (bfd_vma) - 1;
  /* The place (section offset or address) of the storage unit being
     relocated.  */
  bfd_vma p;
  /* The value of GP used to create the relocatable object.  */
  bfd_vma gp0 = (bfd_vma) - 1;
  /* The offset into the global offset table at which the address of
     the relocation entry symbol, adjusted by the addend, resides
     during execution.  */
  bfd_vma g = (bfd_vma) - 1;
  /* The section in which the symbol referenced by the relocation is
     located.  */
  asection *sec = NULL;
  struct mips_elf64_link_hash_entry* h = NULL;
  /* True if the symbol referred to by this relocation is a local
     symbol.  */
  boolean local_p;
  Elf_Internal_Shdr *symtab_hdr;
  size_t extsymoff;
  unsigned long r_symndx;
  int r_type;
  /* True if overflow occurred during the calculation of the
     relocation value.  */
  boolean overflowed_p;
  /* True if this relocation refers to a MIPS16 function.  */
  boolean target_is_16_bit_code_p = false;

  /* Parse the relocation.  */
  r_symndx = ELF64_R_SYM (relocation->r_info);
  r_type = ELF64_MIPS_R_TYPE (relocation->r_info);
  p = (input_section->output_section->vma 
       + input_section->output_offset
       + relocation->r_offset);

  /* Assume that there will be no overflow.  */
  overflowed_p = false;

  /* Figure out whether or not the symbol is local, and get the offset
     used in the array of hash table entries.  */
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  local_p = mips_elf64_local_relocation_p (input_bfd, relocation,
					 local_sections, false);
  if (! elf_bad_symtab (input_bfd))
    extsymoff = symtab_hdr->sh_info;
  else
    {
      /* The symbol table does not follow the rule that local symbols
	 must come before globals.  */
      extsymoff = 0;
    }

  /* Figure out the value of the symbol.  */
  if (local_p)
    {
      Elf_Internal_Sym *sym;

      sym = local_syms + r_symndx;
      sec = local_sections[r_symndx];

      symbol = sec->output_section->vma + sec->output_offset;
      if (ELF_ST_TYPE (sym->st_info) != STT_SECTION)
	symbol += sym->st_value;

      /* MIPS16 text labels should be treated as odd.  */
      if (sym->st_other == STO_MIPS16)
	++symbol;

      /* Record the name of this symbol, for our caller.  */
      *namep = bfd_elf_string_from_elf_section (input_bfd,
						symtab_hdr->sh_link,
						sym->st_name);
      if (*namep == '\0')
	*namep = bfd_section_name (input_bfd, sec);

      target_is_16_bit_code_p = (sym->st_other == STO_MIPS16);
    }
  else
    {
      /* For global symbols we look up the symbol in the hash-table.  */
      h = ((struct mips_elf64_link_hash_entry *) 
	   elf_sym_hashes (input_bfd) [r_symndx - extsymoff]);
      /* Find the real hash-table entry for this symbol.  */
      while (h->root.root.type == bfd_link_hash_indirect
	     || h->root.root.type == bfd_link_hash_warning)
	h = (struct mips_elf64_link_hash_entry *) h->root.root.u.i.link;
      
      /* Record the name of this symbol, for our caller.  */
      *namep = h->root.root.root.string;

      /* If this symbol is defined, calculate its address.  */
      if ((h->root.root.type == bfd_link_hash_defined
	   || h->root.root.type == bfd_link_hash_defweak)
	  && h->root.root.u.def.section)
	{
	  sec = h->root.root.u.def.section;
	  if (sec->output_section)
	    symbol = (h->root.root.u.def.value 
		      + sec->output_section->vma
		      + sec->output_offset);
	  else
	    symbol = h->root.root.u.def.value;
	}
      else if (h->root.root.type == bfd_link_hash_undefweak)
	/* We allow relocations against undefined weak symbols, giving
	   it the value zero, so that you can undefined weak functions
	   and check to see if they exist by looking at their
	   addresses.  */
	symbol = 0;
      else if (info->shared
	       && (!info->symbolic || info->allow_shlib_undefined)
	       && !info->no_undefined
	       && ELF_ST_VISIBILITY (h->root.other) == STV_DEFAULT)
	symbol = 0;
      else if (strcmp (h->root.root.root.string, "_DYNAMIC_LINK") == 0 ||
              strcmp (h->root.root.root.string, "_DYNAMIC_LINKING") == 0)
	{
	  /* If this is a dynamic link, we should have created a
	     _DYNAMIC_LINK symbol or _DYNAMIC_LINKING(for normal mips) symbol 
	     in in mips_elf64_create_dynamic_sections.
	     Otherwise, we should define the symbol with a value of 0.
	     FIXME: It should probably get into the symbol table
	     somehow as well.  */
	  BFD_ASSERT (! info->shared);
	  BFD_ASSERT (bfd_get_section_by_name (abfd, ".dynamic") == NULL);
	  symbol = 0;
	}
      else
	{
	  if (! ((*info->callbacks->undefined_symbol)
		 (info, h->root.root.root.string, input_bfd,
		  input_section, relocation->r_offset,
		  (!info->shared || info->no_undefined
		   || ELF_ST_VISIBILITY (h->root.other)))))
	    return bfd_reloc_undefined;
	  symbol = 0;
	}

      target_is_16_bit_code_p = (h->root.other == STO_MIPS16);
    }

  /* If this is a 64-bit call to a 16-bit function with a stub, we
     need to redirect the call to the stub, unless we're already *in*
     a stub.  */
  if (r_type != R_MIPS16_26 && !info->relocateable
      && ((h != NULL && h->fn_stub != NULL)
	  || (local_p && elf_tdata (input_bfd)->local_stubs != NULL
	      && elf_tdata (input_bfd)->local_stubs[r_symndx] != NULL))
      && !mips_elf64_stub_section_p (input_bfd, input_section))
    {
      /* This is a 64-bit call to a 16-bit function.  We should
	 have already noticed that we were going to need the
	 stub.  */
      if (local_p)
	sec = elf_tdata (input_bfd)->local_stubs[r_symndx];
      else
	{
	  BFD_ASSERT (h->need_fn_stub);
	  sec = h->fn_stub;
	}

      symbol = sec->output_section->vma + sec->output_offset;
    }
  /* If this is a 16-bit call to a 64-bit function with a stub, we
     need to redirect the call to the stub.  */
  else if (r_type == R_MIPS16_26 && !info->relocateable
	   && h != NULL
	   && (h->call_stub != NULL || h->call_fp_stub != NULL)
	   && !target_is_16_bit_code_p)
    {
      /* If both call_stub and call_fp_stub are defined, we can figure
	 out which one to use by seeing which one appears in the input
	 file.  */
      if (h->call_stub != NULL && h->call_fp_stub != NULL)
	{
	  asection *o;

	  sec = NULL;
	  for (o = input_bfd->sections; o != NULL; o = o->next)
	    {
	      if (strncmp (bfd_get_section_name (input_bfd, o),
			   CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0)
		{
		  sec = h->call_fp_stub;
		  break;
		}
	    }
	  if (sec == NULL)
	    sec = h->call_stub;
	}
      else if (h->call_stub != NULL)
	sec = h->call_stub;
      else
	sec = h->call_fp_stub;

      BFD_ASSERT (sec->_raw_size > 0);
      symbol = sec->output_section->vma + sec->output_offset;
    }

  /* Calls from 16-bit code to 32-bit code and vice versa require the
     special jalx instruction.  */
  *require_jalxp = (!info->relocateable
		    && ((r_type == R_MIPS16_26) != target_is_16_bit_code_p));

  local_p = mips_elf64_local_relocation_p (input_bfd, relocation,
					   local_sections, true);

  /* If we haven't already determined the GOT offset, or the GP value,
     and we're going to need it, get it now.  */
  switch (r_type)
    {
    case R_MIPS_CALL16:
    case R_MIPS_GOT16:
    case R_MIPS_GOT_DISP:
    case R_MIPS_GOT_HI16:
    case R_MIPS_CALL_HI16:
    case R_MIPS_GOT_LO16:
    case R_MIPS_CALL_LO16:
      /* Find the index into the GOT where this value is located.  */
      if (!local_p)
	{
	  BFD_ASSERT (addend == 0);
	  g = mips_elf64_global_got_index (elf_hash_table (info)->dynobj,
					 (struct elf_link_hash_entry*) h);
	  if (! elf_hash_table(info)->dynamic_sections_created
	      || (info->shared
		  && (info->symbolic || h->root.dynindx == -1)
		  && (h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
	    {
	      /* This is a static link or a -Bsymbolic link.  The
		 symbol is defined locally, or was forced to be local.
		 We must initialize this entry in the GOT.  */
	      bfd *tmpbfd = elf_hash_table (info)->dynobj;

	      asection *sgot = bfd_get_section_by_name (tmpbfd, ".got");
	      bfd_put_64 (tmpbfd, symbol + addend, sgot->contents + g);
	    }
	}
      else if (r_type == R_MIPS_GOT16 || r_type == R_MIPS_CALL16)
	/* There's no need to create a local GOT entry here; the
	   calculation for a local GOT16 entry does not involve G.  */
	break;
      else
	{
	  g = mips_elf64_local_got_index (abfd, info, symbol + addend);
	  if (g == (bfd_vma) -1)
	    return false;
	}

      /* Convert GOT indices to actual offsets.  */
      g = mips_elf64_got_offset_from_index (elf_hash_table (info)->dynobj,
					    abfd, g);
      break;
      
    case R_MIPS_HI16:
    case R_MIPS_LO16:
    case R_MIPS_GPREL16:
    case R_MIPS_GPREL32:
    case R_MIPS_LITERAL:
      gp0 = _bfd_get_gp_value (input_bfd);
      gp = _bfd_get_gp_value (abfd);
      break;

    default:
      break;
    }

  /* Figure out what kind of relocation is being performed.  */
  switch (r_type)
    {
    case R_MIPS_NONE:
      return bfd_reloc_continue;

    case R_MIPS_16:
      value = symbol + mips_elf64_sign_extend (addend, 16);
      overflowed_p = mips_elf64_overflow_p (value, 16);
      break;

    case R_MIPS_32:
    case R_MIPS_REL32:
    case R_MIPS_64:
      if ((info->shared
	   || (elf_hash_table (info)->dynamic_sections_created
	       && h != NULL
	       && ((h->root.elf_link_hash_flags
		    & ELF_LINK_HASH_DEF_DYNAMIC) != 0)
	       && ((h->root.elf_link_hash_flags
		    & ELF_LINK_HASH_DEF_REGULAR) == 0)))
	  && r_symndx != 0
	  && (input_section->flags & SEC_ALLOC) != 0)
	{
	  /* If we're creating a shared library, or this relocation is
	     against a symbol in a shared library, then we can't know
	     where the symbol will end up.  So, we create a relocation
	     record in the output, and leave the job up to the dynamic
	     linker.  */
	  value = addend;
	  if (!mips_elf64_create_dynamic_relocation (abfd, info, relocation,
						     h, sec, symbol, &value,
						     input_section))
	    return false;
	}
      else
	{
	  if (r_type != R_MIPS_REL32)
	    value = symbol + addend;
	  else
	    value = addend;
	}
      value &= howto->dst_mask;
      break;

    case R_MIPS_PC32:
    case R_MIPS_PC64:
    case R_MIPS_GNU_REL_LO16:
      value = symbol + addend - p;
      value &= howto->dst_mask;
      break;

    case R_MIPS_GNU_REL16_S2:
      value = symbol + mips_elf64_sign_extend (addend << 2, 18) - p;
      overflowed_p = mips_elf64_overflow_p (value, 18);
      value = (value >> 2) & howto->dst_mask;
      break;

    case R_MIPS_GNU_REL_HI16:
      value = mips_elf64_high (addend + symbol - p);
      value &= howto->dst_mask;
      break;

    case R_MIPS16_26:
      /* The calculation for R_MIPS16_26 is just the same as for an
	 R_MIPS_26.  It's only the storage of the relocated field into
	 the output file that's different.  That's handled in
	 mips_elf_perform_relocation.  So, we just fall through to the
	 R_MIPS_26 case here.  */
    case R_MIPS_26:
      if (local_p)
	value = (((addend << 2) | ((p + 4) & 0xf0000000)) + symbol) >> 2;
      else
	value = (mips_elf64_sign_extend (addend << 2, 28) + symbol) >> 2;
      value &= howto->dst_mask;
      break;

    case R_MIPS_HI16:
      value = mips_elf64_high (addend + symbol);
      value &= howto->dst_mask;
      break;

    case R_MIPS_LO16:
	value = (addend + symbol) & 0xffff;
	value &= howto->dst_mask;
      break;

    case R_MIPS_LITERAL:
      /* Because we don't merge literal sections, we can handle this
	 just like R_MIPS_GPREL16.  In the long run, we should merge
	 shared literals, and then we will need to additional work
	 here.  */

      /* Fall through.  */

    case R_MIPS_GPREL16:
      if (local_p)
	value = mips_elf64_sign_extend (addend, 16) + symbol + gp0 - gp;
      else
	value = mips_elf64_sign_extend (addend, 16) + symbol - gp;
      overflowed_p = mips_elf64_overflow_p (value, 16);
      break;
      
    case R_MIPS_PC16:
      value = mips_elf64_sign_extend (addend, 16) + symbol - p;
      overflowed_p = mips_elf64_overflow_p (value, 16);
      value = (bfd_vma) ((bfd_signed_vma) value / 4);
      break;

    case R_MIPS_GOT16:
    case R_MIPS_CALL16:
      if (local_p)
	{
	  boolean forced;
	  
	  /* The special case is when the symbol is forced to be local.  We
	     need the full address in the GOT since no R_MIPS_LO16 relocation
	     follows.  */
	  forced = ! mips_elf64_local_relocation_p (input_bfd, relocation,
						  local_sections, false);
	  value = mips_elf64_got16_entry (abfd, info, symbol + addend, forced);
	  if (value == (bfd_vma) -1)
	    return false;
	  value 
	    = mips_elf64_got_offset_from_index (elf_hash_table (info)->dynobj,
					      abfd,
					      value);
	  overflowed_p = mips_elf64_overflow_p (value, 16);
	  break;
	}

      /* Fall through.  */

    case R_MIPS_GOT_DISP:
      value = g;
      overflowed_p = mips_elf64_overflow_p (value, 16);
      break;

    case R_MIPS_GPREL32:
      value = (addend + symbol + gp0 - gp) & howto->dst_mask;
      break;

    case R_MIPS_GOT_HI16:
    case R_MIPS_CALL_HI16:
      /* We're allowed to handle these two relocations identically.
	 The dynamic linker is allowed to handle the CALL relocations
	 differently by creating a lazy evaluation stub.  */
      value = g;
      value = mips_elf64_high (value);
      value &= howto->dst_mask;
      break;

    case R_MIPS_GOT_LO16:
    case R_MIPS_CALL_LO16:
      value = g & howto->dst_mask;
      break;

    case R_MIPS_GOT_PAGE:
      value = mips_elf64_got_page (abfd, info, symbol + addend, NULL);
      if (value == (bfd_vma) -1)
	return false;
      value = mips_elf64_got_offset_from_index (elf_hash_table (info)->dynobj,
					      abfd,
					      value);
      overflowed_p = mips_elf64_overflow_p (value, 16);
      break;
      
    case R_MIPS_GOT_OFST:
      mips_elf64_got_page (abfd, info, symbol + addend, &value);
      overflowed_p = mips_elf64_overflow_p (value, 16);
      break;

    case R_MIPS_SUB:
      value = symbol - addend;
      value &= howto->dst_mask;
      break;

    case R_MIPS_HIGHER:
      value = mips_elf64_higher (addend + symbol);
      value &= howto->dst_mask;
      break;

    case R_MIPS_HIGHEST:
      value = mips_elf64_highest (addend + symbol);
      value &= howto->dst_mask;
      break;
      
    case R_MIPS_SCN_DISP:
      value = symbol + addend - sec->output_offset;
      value &= howto->dst_mask;
      break;

    case R_MIPS_PJUMP:
    case R_MIPS_JALR:
      /* Both of these may be ignored.  R_MIPS_JALR is an optimization
	 hint; we could improve performance by honoring that hint.  */
      return bfd_reloc_continue;

    case R_MIPS_GNU_VTINHERIT:
    case R_MIPS_GNU_VTENTRY:
      /* We don't do anything with these at present.  */
      return bfd_reloc_continue;

    default:
      /* An unrecognized relocation type.  */
      return bfd_reloc_notsupported;
    }

  /* Store the VALUE for our caller.  */
  *valuep = value;
  return overflowed_p ? bfd_reloc_overflow : bfd_reloc_ok;
}

/* Obtain the field relocated by RELOCATION.  */

static bfd_vma
mips_elf64_obtain_contents (howto, relocation, input_bfd, contents)
     reloc_howto_type *howto;
     const Elf_Internal_Rela *relocation;
     bfd *input_bfd;
     bfd_byte *contents;
{
  bfd_byte *location = contents + relocation->r_offset;

  /* Obtain the bytes.  */
  return bfd_get (8 * bfd_get_reloc_size (howto), input_bfd, location);
}

/* It has been determined that the result of the RELOCATION is the
   VALUE.  Use HOWTO to place VALUE into the output file at the
   appropriate position.  The SECTION is the section to which the
   relocation applies.  If REQUIRE_JALX is true, then the opcode used
   for the relocation must be either JAL or JALX, and it is
   unconditionally converted to JALX.

   Returns false if anything goes wrong.  */

static boolean
mips_elf64_perform_relocation (info, howto, relocation, value,
			     input_bfd, input_section,
			     contents, require_jalx)
     struct bfd_link_info *info;
     reloc_howto_type *howto;
     const Elf_Internal_Rela *relocation;
     bfd_vma value;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     boolean require_jalx;
{
  bfd_vma x;
  bfd_byte *location;
  int r_type = ELF32_R_TYPE (relocation->r_info);

  /* Figure out where the relocation is occurring.  */
  location = contents + relocation->r_offset;

  /* Obtain the current value.  */
  x = mips_elf64_obtain_contents (howto, relocation, input_bfd, contents);

  /* Clear the field we are setting.  */
  x &= ~howto->dst_mask;

  /* If this is the R_MIPS16_26 relocation, we must store the
     value in a funny way.  */
  if (r_type == R_MIPS16_26)
    {
      /* R_MIPS16_26 is used for the mips16 jal and jalx instructions.
	 Most mips16 instructions are 16 bits, but these instructions
	 are 32 bits.

	 The format of these instructions is:

	 +--------------+--------------------------------+
	 !     JALX     ! X!   Imm 20:16  !   Imm 25:21  !
	 +--------------+--------------------------------+
	 !	  	  Immediate  15:0		    !
	 +-----------------------------------------------+

	 JALX is the 5-bit value 00011.  X is 0 for jal, 1 for jalx.
	 Note that the immediate value in the first word is swapped.

	 When producing a relocateable object file, R_MIPS16_26 is
	 handled mostly like R_MIPS_26.  In particular, the addend is
	 stored as a straight 26-bit value in a 32-bit instruction.
	 (gas makes life simpler for itself by never adjusting a
	 R_MIPS16_26 reloc to be against a section, so the addend is
	 always zero).  However, the 32 bit instruction is stored as 2
	 16-bit values, rather than a single 32-bit value.  In a
	 big-endian file, the result is the same; in a little-endian
	 file, the two 16-bit halves of the 32 bit value are swapped.
	 This is so that a disassembler can recognize the jal
	 instruction.

	 When doing a final link, R_MIPS16_26 is treated as a 32 bit
	 instruction stored as two 16-bit values.  The addend A is the
	 contents of the targ26 field.  The calculation is the same as
	 R_MIPS_26.  When storing the calculated value, reorder the
	 immediate value as shown above, and don't forget to store the
	 value as two 16-bit values.

	 To put it in MIPS ABI terms, the relocation field is T-targ26-16,
	 defined as

	 big-endian:
	 +--------+----------------------+
	 |        |                      |
	 |        |    targ26-16         |
	 |31    26|25                   0|
	 +--------+----------------------+

	 little-endian:
	 +----------+------+-------------+
	 |          |      |             |
	 |  sub1    |      |     sub2    |
	 |0        9|10  15|16         31|
	 +----------+--------------------+
	 where targ26-16 is sub1 followed by sub2 (i.e., the addend field A is
	 ((sub1 << 16) | sub2)).

	 When producing a relocateable object file, the calculation is
	 (((A < 2) | ((P + 4) & 0xf0000000) + S) >> 2)
	 When producing a fully linked file, the calculation is
	 let R = (((A < 2) | ((P + 4) & 0xf0000000) + S) >> 2)
	 ((R & 0x1f0000) << 5) | ((R & 0x3e00000) >> 5) | (R & 0xffff)  */

      if (!info->relocateable)
	/* Shuffle the bits according to the formula above.  */
	value = (((value & 0x1f0000) << 5)
		 | ((value & 0x3e00000) >> 5)
		 | (value & 0xffff));
    }
  else if (r_type == R_MIPS16_GPREL)
    {
      /* R_MIPS16_GPREL is used for GP-relative addressing in mips16
	 mode.  A typical instruction will have a format like this:

	 +--------------+--------------------------------+
	 !    EXTEND    !     Imm 10:5    !   Imm 15:11  !
	 +--------------+--------------------------------+
	 !    Major     !   rx   !   ry   !   Imm  4:0   !
	 +--------------+--------------------------------+

	 EXTEND is the five bit value 11110.  Major is the instruction
	 opcode.

	 This is handled exactly like R_MIPS_GPREL16, except that the
	 addend is retrieved and stored as shown in this diagram; that
	 is, the Imm fields above replace the V-rel16 field.

         All we need to do here is shuffle the bits appropriately.  As
	 above, the two 16-bit halves must be swapped on a
	 little-endian system.  */
      value = (((value & 0x7e0) << 16)
	       | ((value & 0xf800) << 5)
	       | (value & 0x1f));
    }

  /* Set the field.  */
  x |= (value & howto->dst_mask);

  /* If required, turn JAL into JALX.  */
  if (require_jalx)
    {
      boolean ok;
      bfd_vma opcode = x >> 26;
      bfd_vma jalx_opcode;

      /* Check to see if the opcode is already JAL or JALX.  */
      if (r_type == R_MIPS16_26)
	{
	  ok = ((opcode == 0x6) || (opcode == 0x7));
	  jalx_opcode = 0x7;
	}
      else
	{
	  ok = ((opcode == 0x3) || (opcode == 0x1d));
	  jalx_opcode = 0x1d;
	}

      /* If the opcode is not JAL or JALX, there's a problem.  */
      if (!ok)
	{
	  (*_bfd_error_handler)
	    (_("%s: %s+0x%lx: jump to stub routine which is not jal"),
	     bfd_archive_filename (input_bfd),
	     input_section->name,
	     (unsigned long) relocation->r_offset);
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      /* Make this the JALX opcode.  */
      x = (x & ~(0x3f << 26)) | (jalx_opcode << 26);
    }

  /* Swap the high- and low-order 16 bits on little-endian systems
     when doing a MIPS16 relocation.  */
  if ((r_type == R_MIPS16_GPREL || r_type == R_MIPS16_26)
      && bfd_little_endian (input_bfd))
    x = (((x & 0xffff) << 16) | ((x & 0xffff0000) >> 16));

  /* Put the value into the output.  */
  bfd_put (8 * bfd_get_reloc_size (howto), input_bfd, x, location);
  return true;
}

/* Returns true if SECTION is a MIPS16 stub section.  */

static boolean
mips_elf64_stub_section_p (abfd, section)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section;
{
  const char *name = bfd_get_section_name (abfd, section);

  return (strncmp (name, FN_STUB, sizeof FN_STUB - 1) == 0
	  || strncmp (name, CALL_STUB, sizeof CALL_STUB - 1) == 0
	  || strncmp (name, CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0);
}

/* Relocate a MIPS ELF64 section.  */

static boolean
mips_elf64_relocate_section (output_bfd, info, input_bfd, input_section,
			     contents, relocs, local_syms, local_sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
{
  Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *relend;
  bfd_vma addend = 0;
  boolean use_saved_addend_p = false;
  struct elf_backend_data *bed;

  bed = get_elf_backend_data (output_bfd);
  relend = relocs + input_section->reloc_count * bed->s->int_rels_per_ext_rel;
  for (rel = relocs; rel < relend; ++rel)
    {
      const char *name;
      bfd_vma value;
      reloc_howto_type *howto;
      boolean require_jalx;
      /* True if the relocation is a RELA relocation, rather than a
         REL relocation.  */
      boolean rela_relocation_p = true;
      int r_type = ELF64_MIPS_R_TYPE (rel->r_info);
      const char *msg = (const char *) NULL;

      /* Find the relocation howto for this relocation.  */
      howto = &mips_elf64_howto_table_rela[r_type];

      if (!use_saved_addend_p)
	{
	  Elf_Internal_Shdr *rel_hdr;

	  /* If these relocations were originally of the REL variety,
	     we must pull the addend out of the field that will be
	     relocated.  Otherwise, we simply use the contents of the
	     RELA relocation.  To determine which flavor or relocation
	     this is, we depend on the fact that the INPUT_SECTION's
	     REL_HDR is read before its REL_HDR2.  */
	  rel_hdr = &elf_section_data (input_section)->rel_hdr;
	  if ((size_t) (rel - relocs)
	      >= (NUM_SHDR_ENTRIES (rel_hdr) * bed->s->int_rels_per_ext_rel))
	    rel_hdr = elf_section_data (input_section)->rel_hdr2;
	  if (rel_hdr->sh_entsize
	      == (get_elf_backend_data (input_bfd)->s->sizeof_rel))
	    {
	      /* Note that this is a REL relocation.  */
	      rela_relocation_p = false;

	      /* Find the relocation howto for this relocation.  */
	      howto = &mips_elf64_howto_table_rel[r_type];

	      /* Get the addend, which is stored in the input file.  */
	      addend = mips_elf64_obtain_contents (howto, 
						   rel,
						   input_bfd,
						   contents);
	      addend &= howto->src_mask;

	      /* For some kinds of relocations, the ADDEND is a
		 combination of the addend stored in two different
		 relocations.   */
	      if (r_type == R_MIPS_HI16
		  || r_type == R_MIPS_GNU_REL_HI16
		  || (r_type == R_MIPS_GOT16
		      && mips_elf64_local_relocation_p (input_bfd, rel,
						      local_sections, false)))
		{
		  bfd_vma l;
		  const Elf_Internal_Rela *lo16_relocation;
		  reloc_howto_type *lo16_howto;
		  int lo;

		  /* The combined value is the sum of the HI16 addend,
		     left-shifted by sixteen bits, and the LO16
		     addend, sign extended.  (Usually, the code does
		     a `lui' of the HI16 value, and then an `addiu' of
		     the LO16 value.)  

		     Scan ahead to find a matching LO16 relocation.  */
		  if (r_type == R_MIPS_GNU_REL_HI16)
		    lo = R_MIPS_GNU_REL_LO16;
		  else
		    lo = R_MIPS_LO16;
		  lo16_relocation 
		    = mips_elf64_next_relocation (lo, rel, relend); 
		  if (lo16_relocation == NULL)
		    return false;

		  /* Obtain the addend kept there.  */
		  if (rela_relocation_p == false)
		    lo16_howto = &mips_elf64_howto_table_rel[lo];
		  else
		    lo16_howto = &mips_elf64_howto_table_rela[lo];
		  l = mips_elf64_obtain_contents (lo16_howto,
						lo16_relocation,
						input_bfd, contents);
		  l &= lo16_howto->src_mask;
		  l = mips_elf64_sign_extend (l, 16);

		  addend <<= 16;

		  /* Compute the combined addend.  */
		  addend += l;
		}
	    }
	  else
	    addend = rel->r_addend;
	}

      if (info->relocateable)
	{
	  Elf_Internal_Sym *sym;
	  unsigned long r_symndx;

	  /* Since we're just relocating, all we need to do is copy
	     the relocations back out to the object file, unless
	     they're against a section symbol, in which case we need
	     to adjust by the section offset, or unless they're GP
	     relative in which case we need to adjust by the amount
	     that we're adjusting GP in this relocateable object.  */

	  if (!mips_elf64_local_relocation_p (input_bfd, rel, local_sections,
					    false))
	    /* There's nothing to do for non-local relocations.  */
	    continue;

	  if (r_type == R_MIPS_GPREL16
	      || r_type == R_MIPS_GPREL32
	      || r_type == R_MIPS_LITERAL)
	    addend -= (_bfd_get_gp_value (output_bfd)
		       - _bfd_get_gp_value (input_bfd));
	  else if (r_type == R_MIPS_26 || r_type == R_MIPS_GNU_REL16_S2)
	    /* The addend is stored without its two least
	       significant bits (which are always zero.)  In a
	       non-relocateable link, calculate_relocation will do
	       this shift; here, we must do it ourselves.  */
	    addend <<= 2;

	  r_symndx = ELF64_R_SYM (rel->r_info);
	  sym = local_syms + r_symndx;
	  if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
	    /* Adjust the addend appropriately.  */
	    addend += local_sections[r_symndx]->output_offset;

#if 0
	  /* If the relocation is for a R_MIPS_HI16 or R_MIPS_GOT16,
	     then we only want to write out the high-order 16 bits.
	     The subsequent R_MIPS_LO16 will handle the low-order bits.  */
	  if (r_type == R_MIPS_HI16 || r_type == R_MIPS_GOT16
	      || r_type == R_MIPS_GNU_REL_HI16)
	    addend = mips_elf64_high (addend);
	  else if (r_type == R_MIPS_HIGHER)
	    addend = mips_elf64_higher (addend);
	  else if (r_type == R_MIPS_HIGHEST)
	    addend = mips_elf64_highest (addend);
#endif
	  /* If the relocation is for an R_MIPS_26 relocation, then
	     the two low-order bits are not stored in the object file;
	     they are implicitly zero.  */
	  if (r_type == R_MIPS_26 || r_type == R_MIPS_GNU_REL16_S2)
	    addend >>= 2;

	  if (rela_relocation_p)
	    /* If this is a RELA relocation, just update the addend.
	       We have to cast away constness for REL.  */
	    rel->r_addend = addend;
	  else
	    {
	      /* Otherwise, we have to write the value back out.  Note
		 that we use the source mask, rather than the
		 destination mask because the place to which we are
		 writing will be source of the addend in the final
		 link.  */
	      addend &= howto->src_mask;

	      if (!mips_elf64_perform_relocation (info, howto, rel, addend,
						  input_bfd, input_section,
						  contents, false))
		return false;
	    }

	  /* Go on to the next relocation.  */
	  continue;
	}

      /* In the N32 and 64-bit ABIs there may be multiple consecutive
	 relocations for the same offset.  In that case we are
	 supposed to treat the output of each relocation as the addend
	 for the next.  */
      if (rel + 1 < relend 
	  && rel->r_offset == rel[1].r_offset
	  && ELF64_MIPS_R_TYPE (rel[1].r_info) != R_MIPS_NONE)
	use_saved_addend_p = true;
      else
	use_saved_addend_p = false;

      /* Figure out what value we are supposed to relocate.  */
      switch (mips_elf64_calculate_relocation (output_bfd, input_bfd,
					       input_section, info, rel,
					       addend, howto, local_syms,
					       local_sections, &value, &name,
					       &require_jalx))
	{
	case bfd_reloc_continue:
	  /* There's nothing to do.  */
	  continue;

	case bfd_reloc_undefined:
	  /* mips_elf64_calculate_relocation already called the
	     undefined_symbol callback.  There's no real point in
	     trying to perform the relocation at this point, so we
	     just skip ahead to the next relocation.  */
	  continue;

	case bfd_reloc_notsupported:
	  msg = _("internal error: unsupported relocation error");
	  info->callbacks->warning
	    (info, msg, name, input_bfd, input_section, rel->r_offset);
	  return false;

	case bfd_reloc_overflow:
	  if (use_saved_addend_p)
	    /* Ignore overflow until we reach the last relocation for
	       a given location.  */
	    ;
	  else
	    {
	      BFD_ASSERT (name != NULL);
	      if (! ((*info->callbacks->reloc_overflow)
		     (info, name, howto->name, (bfd_vma) 0,
		      input_bfd, input_section, rel->r_offset)))
		return false;
	    }
	  break;

	case bfd_reloc_ok:
	  break;

	default:
	  abort ();
	  break;
	}

      /* If we've got another relocation for the address, keep going
	 until we reach the last one.  */
      if (use_saved_addend_p)
	{
	  addend = value;
	  continue;
	}

      /* Actually perform the relocation.  */
      if (!mips_elf64_perform_relocation (info, howto, rel, value, input_bfd,
					  input_section, contents,
					  require_jalx))
	return false;
    }

  return true;
}

/* Create dynamic sections when linking against a dynamic object.  */

boolean
mips_elf64_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED | SEC_READONLY);

  /* Mips ABI requests the .dynamic section to be read only.  */
  s = bfd_get_section_by_name (abfd, ".dynamic");
  if (s != NULL)
    {
      if (! bfd_set_section_flags (abfd, s, flags))
	return false;
    }

  /* We need to create .got section.  */
  if (! mips_elf64_create_got_section (abfd, info))
    return false;

  /* Create the .msym section on IRIX6.  It is used by the dynamic
     linker to speed up dynamic relocations, and to avoid computing
     the ELF hash for symbols.  */
  if (!mips_elf64_create_msym_section (abfd))
    return false;

  /* Create .stub section.  */
  if (bfd_get_section_by_name (abfd, ".MIPS.stubs") == NULL)
    {
      s = bfd_make_section (abfd, ".MIPS.stubs");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags | SEC_CODE)
	  || ! bfd_set_section_alignment (abfd, s, 3))
	return false;
    }

  return true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

boolean
mips_elf64_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj;
  struct mips_elf64_link_hash_entry *hmips;
  asection *s;

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

  /* If this symbol is defined in a dynamic object, we need to copy
     any R_MIPS_32 or R_MIPS_REL32 relocs against it into the output
     file.  */
  hmips = (struct mips_elf64_link_hash_entry *) h;
  if (! info->relocateable
      && hmips->possibly_dynamic_relocs != 0
      && (h->root.type == bfd_link_hash_defweak
	  || (h->elf_link_hash_flags
	      & ELF_LINK_HASH_DEF_REGULAR) == 0))
    {
      mips_elf64_allocate_dynamic_relocations (dynobj,
					       hmips->possibly_dynamic_relocs);
      if (hmips->readonly_reloc)
	/* We tell the dynamic linker that there are relocations
	   against the text segment.  */
	info->flags |= DF_TEXTREL;
    }

  /* For a function, create a stub, if allowed.  */
  if (! hmips->no_fn_stub
      && (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      if (! elf_hash_table (info)->dynamic_sections_created)
	return true;

      /* If this symbol is not defined in a regular file, then set
	 the symbol to the stub location.  This is required to make
	 function pointers compare as equal between the normal
	 executable and the shared library.  */
      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* We need .stub section.  */
	  s = bfd_get_section_by_name (dynobj, ".MIPS.stubs");
	  BFD_ASSERT (s != NULL);

	  h->root.u.def.section = s;
	  h->root.u.def.value = s->_raw_size;

	  /* XXX Write this stub address somewhere.  */
	  h->plt.offset = s->_raw_size;

	  /* Make room for this stub code.  */
	  s->_raw_size += MIPS_FUNCTION_STUB_SIZE;

	  /* The last half word of the stub will be filled with the index
	     of this symbol in .dynsym section.  */
	  return true;
	}
    }
  else if ((h->type == STT_FUNC)
	   && (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) == 0)
    {
      /* This will set the entry for this symbol in the GOT to 0, and
         the dynamic linker will take care of this.  */
      h->root.u.def.value = 0;
      return true;
    }

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
		  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
      return true;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  */

  return true;
}

/* This function is called after all the input files have been read,
   and the input sections have been assigned to output sections.  */

boolean
mips_elf64_always_size_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
{
  if (info->relocateable
      || ! mips_elf64_hash_table (info)->mips16_stubs_seen)
    return true;

  mips_elf64_link_hash_traverse (mips_elf64_hash_table (info),
				 mips_elf64_check_mips16_stubs,
				 (PTR) NULL);

  return true;
}

/* Check the mips16 stubs for a particular symbol, and see if we can
   discard them.  */

static boolean
mips_elf64_check_mips16_stubs (h, data)
     struct mips_elf64_link_hash_entry *h;
     PTR data ATTRIBUTE_UNUSED;
{
  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct mips_elf64_link_hash_entry *) h->root.root.u.i.link;

  if (h->fn_stub != NULL
      && ! h->need_fn_stub)
    {
      /* We don't need the fn_stub; the only references to this symbol
         are 16 bit calls.  Clobber the size to 0 to prevent it from
         being included in the link.  */
      h->fn_stub->_raw_size = 0;
      h->fn_stub->_cooked_size = 0;
      h->fn_stub->flags &= ~SEC_RELOC;
      h->fn_stub->reloc_count = 0;
      h->fn_stub->flags |= SEC_EXCLUDE;
    }

  if (h->call_stub != NULL
      && h->root.other == STO_MIPS16)
    {
      /* We don't need the call_stub; this is a 16 bit function, so
         calls from other 16 bit functions are OK.  Clobber the size
         to 0 to prevent it from being included in the link.  */
      h->call_stub->_raw_size = 0;
      h->call_stub->_cooked_size = 0;
      h->call_stub->flags &= ~SEC_RELOC;
      h->call_stub->reloc_count = 0;
      h->call_stub->flags |= SEC_EXCLUDE;
    }

  if (h->call_fp_stub != NULL
      && h->root.other == STO_MIPS16)
    {
      /* We don't need the call_stub; this is a 16 bit function, so
         calls from other 16 bit functions are OK.  Clobber the size
         to 0 to prevent it from being included in the link.  */
      h->call_fp_stub->_raw_size = 0;
      h->call_fp_stub->_cooked_size = 0;
      h->call_fp_stub->flags &= ~SEC_RELOC;
      h->call_fp_stub->reloc_count = 0;
      h->call_fp_stub->flags |= SEC_EXCLUDE;
    }

  return true;
}

/* Set the sizes of the dynamic sections.  */

boolean
mips_elf64_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  boolean reltext;
  struct mips_elf64_got_info *g = NULL;

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (! info->shared)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size = strlen ("/usr/lib64/libc.so.1") + 1;
	  s->contents = (bfd_byte *) "/usr/lib64/libc.so.1";
	}
    }

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  reltext = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      boolean strip;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      strip = false;

      if (strncmp (name, ".rel", 4) == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* We only strip the section if the output section name
                 has the same name.  Otherwise, there might be several
                 input sections for this output section.  FIXME: This
                 code is probably not needed these days anyhow, since
                 the linker now does not create empty output sections.  */
	      if (s->output_section != NULL
		  && strcmp (name,
			     bfd_get_section_name (s->output_section->owner,
						   s->output_section)) == 0)
		strip = true;
	    }
	  else
	    {
	      const char *outname;
	      asection *target;

	      /* If this relocation section applies to a read only
                 section, then we probably need a DT_TEXTREL entry.
                 If the relocation section is .rel.dyn, we always
                 assert a DT_TEXTREL entry rather than testing whether
                 there exists a relocation to a read only section or
                 not.  */
	      outname = bfd_get_section_name (output_bfd,
					      s->output_section);
	      target = bfd_get_section_by_name (output_bfd, outname + 4);
	      if ((target != NULL
		   && (target->flags & SEC_READONLY) != 0
		   && (target->flags & SEC_ALLOC) != 0)
		  || strcmp (outname, "rel.dyn") == 0)
		reltext = true;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      if (strcmp (name, "rel.dyn") != 0)
		s->reloc_count = 0;
	    }
	}
      else if (strncmp (name, ".got", 4) == 0)
	{
	  int i;
	  bfd_size_type loadable_size = 0;
	  bfd_size_type local_gotno;
	  bfd *sub;

	  BFD_ASSERT (elf_section_data (s) != NULL);
	  g = (struct mips_elf64_got_info *) elf_section_data (s)->tdata;
	  BFD_ASSERT (g != NULL);

	  /* Calculate the total loadable size of the output.  That
	     will give us the maximum number of GOT_PAGE entries
	     required.  */
	  for (sub = info->input_bfds; sub; sub = sub->link_next)
	    {
	      asection *subsection;

	      for (subsection = sub->sections;
		   subsection;
		   subsection = subsection->next)
		{
		  if ((subsection->flags & SEC_ALLOC) == 0)
		    continue;
		  loadable_size += (subsection->_raw_size + 0xf) & ~0xf;
		}
	    }
	  loadable_size += MIPS_FUNCTION_STUB_SIZE;

	  /* Assume there are two loadable segments consisting of
	     contiguous sections.  Is 5 enough?  */
	  local_gotno = (loadable_size >> 16) + 5;
	    /* It's possible we will need GOT_PAGE entries as well as
	       GOT16 entries.  Often, these will be able to share GOT
	       entries, but not always.  */
	    local_gotno *= 2;

	  g->local_gotno += local_gotno;
	  s->_raw_size += local_gotno * 8;

	  /* There has to be a global GOT entry for every symbol with
	     a dynamic symbol table index of DT_MIPS_GOTSYM or
	     higher.  Therefore, it make sense to put those symbols
	     that need GOT entries at the end of the symbol table.  We
	     do that here.  */
 	  if (!mips_elf64_sort_hash_table (info, 1))
 	    return false;

	  if (g->global_gotsym != NULL)
	    i = elf_hash_table (info)->dynsymcount - g->global_gotsym->dynindx;
	  else
	    /* If there are no global symbols, or none requiring
	       relocations, then GLOBAL_GOTSYM will be NULL.  */
	    i = 0;
	  g->global_gotno = i;
	  s->_raw_size += i * 8;
	}
      else if (strcmp (name, ".MIPS.stubs") == 0)
	{
	  /* Irix rld assumes that the function stub isn't at the end
	     of .text section. So put a dummy. XXX  */
	  s->_raw_size += MIPS_FUNCTION_STUB_SIZE;
	}
      else if (strcmp (name, ".msym")
	       == 0)
	s->_raw_size = (sizeof (Elf32_External_Msym)
			* (elf_hash_table (info)->dynsymcount
			   + bfd_count_sections (output_bfd)));
      else if (strncmp (name, ".init", 5) != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	{
	  _bfd_strip_section_from_output (info, s);
	  continue;
	}

      /* Allocate memory for the section contents.  */
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return false;
	}
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf_mips_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
      if (! info->shared)
	{
	  /* SGI object has the equivalence of DT_DEBUG in the
	     DT_MIPS_RLD_MAP entry.  */
	  if (!bfd_elf64_add_dynamic_entry (info, DT_MIPS_RLD_MAP, 0))
	    return false;
	  if (!SGI_COMPAT (output_bfd))
	    {
	      if (!bfd_elf64_add_dynamic_entry (info, DT_DEBUG, 0))
		return false;
	    }
	}
      else
	{
	  /* Shared libraries on traditional mips have DT_DEBUG.  */
	  if (!SGI_COMPAT (output_bfd))
	    {
	      if (!bfd_elf64_add_dynamic_entry (info, DT_DEBUG, 0))
		return false;
	    }
	}

      if (reltext && SGI_COMPAT (output_bfd))
	info->flags |= DF_TEXTREL;

      if ((info->flags & DF_TEXTREL) != 0)
	{
	  if (! bfd_elf64_add_dynamic_entry (info, DT_TEXTREL, 0))
	    return false;
	}

      if (! bfd_elf64_add_dynamic_entry (info, DT_PLTGOT, 0))
	return false;

      if (bfd_get_section_by_name (dynobj, "rel.dyn"))
	{
	  if (! bfd_elf64_add_dynamic_entry (info, DT_REL, 0))
	    return false;

	  if (! bfd_elf64_add_dynamic_entry (info, DT_RELSZ, 0))
	    return false;

	  if (! bfd_elf64_add_dynamic_entry (info, DT_RELENT, 0))
	    return false;
	}

      if (SGI_COMPAT (output_bfd))
	{
	  if (!bfd_elf64_add_dynamic_entry (info, DT_MIPS_CONFLICTNO, 0))
	    return false;
	}

      if (SGI_COMPAT (output_bfd))
	{
	  if (!bfd_elf64_add_dynamic_entry (info, DT_MIPS_LIBLISTNO, 0))
	    return false;
	}

      if (bfd_get_section_by_name (dynobj, ".conflict") != NULL)
	{
	  if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_CONFLICT, 0))
	    return false;

	  s = bfd_get_section_by_name (dynobj, ".liblist");
	  BFD_ASSERT (s != NULL);

	  if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_LIBLIST, 0))
	    return false;
	}

      if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_RLD_VERSION, 0))
	return false;

      if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_FLAGS, 0))
	return false;

#if 0
      /* Time stamps in executable files are a bad idea.  */
      if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_TIME_STAMP, 0))
	return false;
#endif

#if 0 /* FIXME  */
      if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_ICHECKSUM, 0))
	return false;
#endif

#if 0 /* FIXME  */
      if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_IVERSION, 0))
	return false;
#endif

      if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_BASE_ADDRESS, 0))
	return false;

      if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_LOCAL_GOTNO, 0))
	return false;

      if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_SYMTABNO, 0))
	return false;

      if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_UNREFEXTNO, 0))
	return false;

      if (! bfd_elf64_add_dynamic_entry (info, DT_MIPS_GOTSYM, 0))
	return false;

      if ((bfd_get_section_by_name(dynobj, ".MIPS.options"))
	  && !bfd_elf64_add_dynamic_entry (info, DT_MIPS_OPTIONS, 0))
	return false;

      if (bfd_get_section_by_name (dynobj, ".msym")
	  && !bfd_elf64_add_dynamic_entry (info, DT_MIPS_MSYM, 0))
	return false;
    }

  return true;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

boolean
mips_elf64_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj;
  bfd_vma gval;
  asection *sgot;
  asection *smsym;
  struct mips_elf64_got_info *g;
  const char *name;
  struct mips_elf64_link_hash_entry *mh;

  dynobj = elf_hash_table (info)->dynobj;
  gval = sym->st_value;
  mh = (struct mips_elf64_link_hash_entry *) h;

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *s;
      bfd_byte stub[MIPS_FUNCTION_STUB_SIZE];

      /* This symbol has a stub.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1);

      s = bfd_get_section_by_name (dynobj, ".MIPS.stubs");
      BFD_ASSERT (s != NULL);

      /* FIXME: Can h->dynindex be more than 64K?  */
      if (h->dynindx & 0xffff0000)
	return false;

      /* Fill the stub.  */
      bfd_put_32 (output_bfd, STUB_LW, stub);
      bfd_put_32 (output_bfd, STUB_MOVE, stub + 4);
      bfd_put_32 (output_bfd, STUB_JALR, stub + 8);
      bfd_put_32 (output_bfd, STUB_LI16 + h->dynindx, stub + 12);

      BFD_ASSERT (h->plt.offset <= s->_raw_size);
      memcpy (s->contents + h->plt.offset, stub, MIPS_FUNCTION_STUB_SIZE);

      /* Mark the symbol as undefined.  plt.offset != -1 occurs
	 only for the referenced symbol.  */
      sym->st_shndx = SHN_UNDEF;

      /* The run-time linker uses the st_value field of the symbol
	 to reset the global offset table entry for this external
	 to its stub address when unlinking a shared object.  */
      gval = s->output_section->vma + s->output_offset + h->plt.offset;
      sym->st_value = gval;
    }

  BFD_ASSERT (h->dynindx != -1
	      || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0);

  sgot = bfd_get_section_by_name (dynobj, ".got");
  BFD_ASSERT (sgot != NULL);
  BFD_ASSERT (elf_section_data (sgot) != NULL);
  g = (struct mips_elf64_got_info *) elf_section_data (sgot)->tdata;
  BFD_ASSERT (g != NULL);

  /* Run through the global symbol table, creating GOT entries for all
     the symbols that need them.  */
  if (g->global_gotsym != NULL
      && h->dynindx >= g->global_gotsym->dynindx)
    {
      bfd_vma offset;
      bfd_vma value;

      if (sym->st_value)
	value = sym->st_value;
      else
	{
	  /* For an entity defined in a shared object, this will be
	     NULL.  (For functions in shared objects for
	     which we have created stubs, ST_VALUE will be non-NULL.
	     That's because such the functions are now no longer defined
	     in a shared object.)  */

	  if (info->shared && h->root.type == bfd_link_hash_undefined)
	    value = 0;
	  else
	    value = h->root.u.def.value;
	}
      offset = mips_elf64_global_got_index (dynobj, h);
      bfd_put_64 (output_bfd, value, sgot->contents + offset);
    }

  /* Create a .msym entry, if appropriate.  */
  smsym = bfd_get_section_by_name (dynobj, ".msym");
  if (smsym)
    {
      Elf32_Internal_Msym msym;

      msym.ms_hash_value = bfd_elf_hash (h->root.root.string);
      /* It is undocumented what the `1' indicates, but IRIX6 uses
	 this value.  */
      msym.ms_info = ELF32_MS_INFO (mh->min_dyn_reloc_index, 1);
      mips_elf64_swap_msym_out
	(dynobj, &msym,
	 ((Elf32_External_Msym *) smsym->contents) + h->dynindx);
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  name = h->root.root.string;
  if (strcmp (name, "_DYNAMIC") == 0
      || strcmp (name, "_GLOBAL_OFFSET_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;
  else if (strcmp (name, "_DYNAMIC_LINK") == 0
	   || strcmp (name, "_DYNAMIC_LINKING") == 0)
    {
      sym->st_shndx = SHN_ABS;
      sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);
      sym->st_value = 1;
    }
  else if (sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS)
    {
      if (h->type == STT_FUNC)
	sym->st_shndx = SHN_MIPS_TEXT;
      else if (h->type == STT_OBJECT)
	sym->st_shndx = SHN_MIPS_DATA;
    }

  /* Handle the IRIX6-specific symbols.  */

    {
  /* The linker script takes care of providing names and values for
     these, but we must place them into the right sections.  */
  static const char* const text_section_symbols[] = {
    "_ftext",
    "_etext",
    "__dso_displacement",
    "__elf_header",
    "__program_header_table",
    NULL
  };

  static const char* const data_section_symbols[] = {
    "_fdata",
    "_edata",
    "_end",
    "_fbss",
    NULL
  };

  const char* const *p;
  int i;

  for (i = 0; i < 2; ++i)
    for (p = (i == 0) ? text_section_symbols : data_section_symbols;
	 *p;
	 ++p)
      if (strcmp (*p, name) == 0)
	{
	  /* All of these symbols are given type STT_SECTION by the
	     IRIX6 linker.  */
	  sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);

	  /* The IRIX linker puts these symbols in special sections.  */
	  if (i == 0)
	    sym->st_shndx = SHN_MIPS_TEXT;
	  else
	    sym->st_shndx = SHN_MIPS_DATA;

	  break;
	}
    }

  return true;
}

/* Finish up the dynamic sections.  */

boolean
mips_elf64_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn;
  asection *sgot;
  struct mips_elf64_got_info *g;

  dynobj = elf_hash_table (info)->dynobj;

  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  sgot = bfd_get_section_by_name (dynobj, ".got");
  if (sgot == NULL)
    g = NULL;
  else
    {
      BFD_ASSERT (elf_section_data (sgot) != NULL);
      g = (struct mips_elf64_got_info *) elf_section_data (sgot)->tdata;
      BFD_ASSERT (g != NULL);
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      bfd_byte *b;

      BFD_ASSERT (sdyn != NULL);
      BFD_ASSERT (g != NULL);

      for (b = sdyn->contents;
	   b < sdyn->contents + sdyn->_raw_size;
	   b += get_elf_backend_data (dynobj)->s->sizeof_dyn)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  size_t elemsize;
	  asection *s;
	  boolean swap_out_p;

	  /* Read in the current dynamic entry.  */
	  (*get_elf_backend_data (dynobj)->s->swap_dyn_in) (dynobj, b, &dyn);

	  /* Assume that we're going to modify it and write it out.  */
	  swap_out_p = true;

	  switch (dyn.d_tag)
	    {
	    case DT_RELENT:
	      s = bfd_get_section_by_name(dynobj, "rel.dyn");
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_val = get_elf_backend_data (dynobj)->s->sizeof_rel;
	      break;

	    case DT_STRSZ:
	      /* Rewrite DT_STRSZ.  */
	      dyn.d_un.d_val =
		_bfd_elf_strtab_size (elf_hash_table (info)->dynstr);
	      break;

	    case DT_PLTGOT:
	      name = ".got";
	      goto get_vma;
	    case DT_MIPS_CONFLICT:
	      name = ".conflict";
	      goto get_vma;
	    case DT_MIPS_LIBLIST:
	      name = ".liblist";
	    get_vma:
	      s = bfd_get_section_by_name (output_bfd, name);
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->vma;
	      break;

	    case DT_MIPS_RLD_VERSION:
	      dyn.d_un.d_val = 1; /* XXX */
	      break;

	    case DT_MIPS_FLAGS:
	      dyn.d_un.d_val = RHF_NOTPOT; /* XXX */
	      break;

	    case DT_MIPS_CONFLICTNO:
	      name = ".conflict";
	      elemsize = sizeof (Elf32_Conflict);
	      goto set_elemno;

	    case DT_MIPS_LIBLISTNO:
	      name = ".liblist";
	      elemsize = sizeof (Elf32_Lib);
	    set_elemno:
	      s = bfd_get_section_by_name (output_bfd, name);
	      if (s != NULL)
		{
		  if (s->_cooked_size != 0)
		    dyn.d_un.d_val = s->_cooked_size / elemsize;
		  else
		    dyn.d_un.d_val = s->_raw_size / elemsize;
		}
	      else
		dyn.d_un.d_val = 0;
	      break;

	    case DT_MIPS_TIME_STAMP:
	      time ((time_t *) &dyn.d_un.d_val);
	      break;

	    case DT_MIPS_ICHECKSUM:
	      /* XXX FIXME: */
	      swap_out_p = false;
	      break;

	    case DT_MIPS_IVERSION:
	      /* XXX FIXME: */
	      swap_out_p = false;
	      break;

	    case DT_MIPS_BASE_ADDRESS:
	      s = output_bfd->sections;
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->vma & ~(0xffff);
	      break;

	    case DT_MIPS_LOCAL_GOTNO:
	      dyn.d_un.d_val = g->local_gotno;
	      break;

	    case DT_MIPS_UNREFEXTNO:
	      /* The index into the dynamic symbol table which is the
		 entry of the first external symbol that is not
		 referenced within the same object.  */
	      dyn.d_un.d_val = bfd_count_sections (output_bfd) + 1;
	      break;

	    case DT_MIPS_GOTSYM:
	      if (g->global_gotsym)
		{
		  dyn.d_un.d_val = g->global_gotsym->dynindx;
		  break;
		}
	      /* In case if we don't have global got symbols we default
		 to setting DT_MIPS_GOTSYM to the same value as
		 DT_MIPS_SYMTABNO, so we just fall through.  */

	    case DT_MIPS_SYMTABNO:
	      name = ".dynsym";
	      elemsize = get_elf_backend_data (output_bfd)->s->sizeof_sym;
	      s = bfd_get_section_by_name (output_bfd, name);
	      BFD_ASSERT (s != NULL);

	      if (s->_cooked_size != 0)
		dyn.d_un.d_val = s->_cooked_size / elemsize;
	      else
		dyn.d_un.d_val = s->_raw_size / elemsize;
	      break;

	    case DT_MIPS_HIPAGENO:
	      dyn.d_un.d_val = g->local_gotno - MIPS_RESERVED_GOTNO;
	      break;

	    case DT_MIPS_OPTIONS:
	      s = bfd_get_section_by_name(output_bfd, ".MIPS.options");
	      dyn.d_un.d_ptr = s->vma;
	      break;

	    case DT_MIPS_MSYM:
	      s = bfd_get_section_by_name(output_bfd, ".msym");
	      dyn.d_un.d_ptr = s->vma;
	      break;

	    default:
	      swap_out_p = false;
	      break;
	    }

	  if (swap_out_p)
	    (*get_elf_backend_data (dynobj)->s->swap_dyn_out)
	      (dynobj, &dyn, b);
	}
    }

  /* The first entry of the global offset table will be filled at
     runtime. The second entry will be used by some runtime loaders.
     This isn't the case of Irix rld.  */
  if (sgot != NULL && sgot->_raw_size > 0)
    {
      bfd_put_64 (output_bfd, (bfd_vma) 0, sgot->contents);
      bfd_put_64 (output_bfd, (bfd_vma) 0x80000000, sgot->contents + 8);
    }

  if (sgot != NULL)
    elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 8;

  {
    asection *smsym;
    asection *s;

    /* ??? The section symbols for the output sections were set up in
       _bfd_elf_final_link.  SGI sets the STT_NOTYPE attribute for these
       symbols.  Should we do so?  */

    smsym = bfd_get_section_by_name (dynobj, ".msym");
    if (smsym != NULL)
      {
	Elf32_Internal_Msym msym;

	msym.ms_hash_value = 0;
	msym.ms_info = ELF32_MS_INFO (0, 1);

	for (s = output_bfd->sections; s != NULL; s = s->next)
	  {
	    long dynindx = elf_section_data (s)->dynindx;

	    mips_elf64_swap_msym_out
	      (output_bfd, &msym,
	       (((Elf32_External_Msym *) smsym->contents)
		+ dynindx));
	  }
      }

    /* Clean up a first relocation in .rel.dyn.  */
    s = bfd_get_section_by_name (dynobj, "rel.dyn");
    if (s != NULL && s->_raw_size > 0)
      memset (s->contents, 0, get_elf_backend_data (dynobj)->s->sizeof_rel);
  }

  return true;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

asection *
mips_elf64_gc_mark_hook (abfd, info, rel, h, sym)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  if (h != NULL)
    {
      switch (ELF64_R_TYPE (rel->r_info))
	{
	case R_MIPS_GNU_VTINHERIT:
	case R_MIPS_GNU_VTENTRY:
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
      return bfd_section_from_elf_index (abfd, sym->st_shndx);
    }

  return NULL;
}

/* Update the got entry reference counts for the section being removed.  */

boolean
mips_elf64_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
#if 0
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;
  unsigned long r_symndx;
  struct elf_link_hash_entry *h;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    switch (ELF64_R_TYPE (rel->r_info))
      {
      case R_MIPS_GOT16:
      case R_MIPS_CALL16:
      case R_MIPS_CALL_HI16:
      case R_MIPS_CALL_LO16:
      case R_MIPS_GOT_HI16:
      case R_MIPS_GOT_LO16:
	/* ??? It would seem that the existing MIPS code does no sort
	   of reference counting or whatnot on its GOT and PLT entries,
	   so it is not possible to garbage collect them at this time.  */
	break;

      default:
	break;
      }
#endif

  return true;
}

/* Create the .got section to hold the global offset table. */

static boolean
mips_elf64_create_got_section (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct elf_link_hash_entry *h;
  struct mips_elf64_got_info *g;

  /* This function may be called more than once.  */
  if (bfd_get_section_by_name (abfd, ".got"))
    return true;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  s = bfd_make_section (abfd, ".got");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags)
      || ! bfd_set_section_alignment (abfd, s, 4))
    return false;

  /* Define the symbol _GLOBAL_OFFSET_TABLE_.  We don't do this in the
     linker script because we don't want to define the symbol if we
     are not creating a global offset table.  */
  h = NULL;
  if (! (_bfd_generic_link_add_one_symbol
	 (info, abfd, "_GLOBAL_OFFSET_TABLE_", BSF_GLOBAL, s,
	  (bfd_vma) 0, (const char *) NULL, false,
	  get_elf_backend_data (abfd)->collect,
	  (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags &=~ ELF_LINK_NON_ELF;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (info->shared
      && ! bfd_elf64_link_record_dynamic_symbol (info, h))
    return false;

  /* The first several global offset table entries are reserved.  */
  s->_raw_size = MIPS_RESERVED_GOTNO * (get_elf_backend_data (abfd)->s->arch_size / 8);

  g = (struct mips_elf64_got_info *) bfd_alloc (abfd,
					  sizeof (struct mips_elf64_got_info));
  if (g == NULL)
    return false;
  g->global_gotsym = NULL;
  g->local_gotno = MIPS_RESERVED_GOTNO;
  g->assigned_gotno = MIPS_RESERVED_GOTNO;
  if (elf_section_data (s) == NULL)
    {
      s->used_by_bfd =
	(PTR) bfd_zalloc (abfd, sizeof (struct bfd_elf_section_data));
      if (elf_section_data (s) == NULL)
	return false;
    }
  elf_section_data (s)->tdata = (PTR) g;
  elf_section_data (s)->this_hdr.sh_flags 
    |= SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL;

  return true;
}

/* If H is a symbol that needs a global GOT entry, but has a dynamic
   symbol table index lower than any we've seen to date, record it for
   posterity.  */

static boolean
mips_elf64_record_global_got_symbol (h, info, g)
     struct elf_link_hash_entry *h;
     struct bfd_link_info *info;
     struct mips_elf64_got_info *g ATTRIBUTE_UNUSED;
{
  /* A global symbol in the GOT must also be in the dynamic symbol
     table.  */
  if (h->dynindx == -1
      && !bfd_elf64_link_record_dynamic_symbol (info, h))
    return false;
  
  /* If we've already marked this entry as needing GOT space, we don't
     need to do it again.  */
  if (h->got.offset != (bfd_vma) - 1)
    return true;

  /* By setting this to a value other than -1, we are indicating that
     there needs to be a GOT entry for H.  Avoid using zero, as the
     generic ELF copy_indirect_symbol tests for <= 0.  */
  h->got.offset = 1;

  return true;
}

/* Returns the .msym section for ABFD, creating it if it does not
   already exist.  Returns NULL to indicate error.  */

static asection *
mips_elf64_create_msym_section (abfd)
     bfd *abfd;
{
  asection *s;

  s = bfd_get_section_by_name (abfd, ".msym");
  if (!s)
    {
      s = bfd_make_section (abfd, ".msym");
      if (!s
	  || !bfd_set_section_flags (abfd, s,
				     SEC_ALLOC
				     | SEC_LOAD
				     | SEC_HAS_CONTENTS
				     | SEC_LINKER_CREATED
				     | SEC_READONLY)
	  || !bfd_set_section_alignment (abfd, s, 3))
	return NULL;
    }

  return s;
}

/* Add room for N relocations to the .rel.dyn section in ABFD.  */

static void
mips_elf64_allocate_dynamic_relocations (abfd, n)
     bfd *abfd;
     unsigned int n;
{
  asection *s;

  s = bfd_get_section_by_name (abfd, ".rel.dyn");
  BFD_ASSERT (s != NULL);
  
  if (s->_raw_size == 0)
    {
      /* Make room for a null element. */
      s->_raw_size += get_elf_backend_data (abfd)->s->sizeof_rel;
      ++s->reloc_count;
    }
  s->_raw_size += n * get_elf_backend_data (abfd)->s->sizeof_rel;
}

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table.  */

boolean
mips_elf64_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  const char *name;
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  struct mips_elf64_got_info *g;
  size_t extsymoff;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sgot;
  asection *sreloc;
  struct elf_backend_data *bed;

  if (info->relocateable)
    return true;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  extsymoff = (elf_bad_symtab (abfd)) ? 0 : symtab_hdr->sh_info;

  /* Check for the mips16 stub sections.  */

  name = bfd_get_section_name (abfd, sec);
  if (strncmp (name, FN_STUB, sizeof FN_STUB - 1) == 0)
    {
      unsigned long r_symndx;

      /* Look at the relocation information to figure out which symbol
         this is for.  */

      r_symndx = ELF64_R_SYM (relocs->r_info);

      if (r_symndx < extsymoff
	  || sym_hashes[r_symndx - extsymoff] == NULL)
	{
	  asection *o;

	  /* This stub is for a local symbol.  This stub will only be
             needed if there is some relocation in this BFD, other
             than a 16 bit function call, which refers to this symbol.  */
	  for (o = abfd->sections; o != NULL; o = o->next)
	    {
	      Elf_Internal_Rela *sec_relocs;
	      const Elf_Internal_Rela *r, *rend;

	      /* We can ignore stub sections when looking for relocs.  */
	      if ((o->flags & SEC_RELOC) == 0
		  || o->reloc_count == 0
		  || strncmp (bfd_get_section_name (abfd, o), FN_STUB,
			      sizeof FN_STUB - 1) == 0
		  || strncmp (bfd_get_section_name (abfd, o), CALL_STUB,
			      sizeof CALL_STUB - 1) == 0
		  || strncmp (bfd_get_section_name (abfd, o), CALL_FP_STUB,
			      sizeof CALL_FP_STUB - 1) == 0)
		continue;

	      sec_relocs = (_bfd_elf64_link_read_relocs
			    (abfd, o, (PTR) NULL,
			     (Elf_Internal_Rela *) NULL,
			     info->keep_memory));
	      if (sec_relocs == NULL)
		return false;

	      rend = sec_relocs + o->reloc_count;
	      for (r = sec_relocs; r < rend; r++)
		if (ELF64_R_SYM (r->r_info) == r_symndx
		    && ELF64_R_TYPE (r->r_info) != R_MIPS16_26)
		  break;

	      if (! info->keep_memory)
		free (sec_relocs);

	      if (r < rend)
		break;
	    }

	  if (o == NULL)
	    {
	      /* There is no non-call reloc for this stub, so we do
                 not need it.  Since this function is called before
                 the linker maps input sections to output sections, we
                 can easily discard it by setting the SEC_EXCLUDE
                 flag.  */
	      sec->flags |= SEC_EXCLUDE;
	      return true;
	    }

	  /* Record this stub in an array of local symbol stubs for
             this BFD.  */
	  if (elf_tdata (abfd)->local_stubs == NULL)
	    {
	      unsigned long symcount;
	      asection **n;
	      bfd_size_type amt;

	      if (elf_bad_symtab (abfd))
		symcount = NUM_SHDR_ENTRIES (symtab_hdr);
	      else
		symcount = symtab_hdr->sh_info;
	      amt = symcount * sizeof (asection *);
	      n = (asection **) bfd_zalloc (abfd, amt);
	      if (n == NULL)
		return false;
	      elf_tdata (abfd)->local_stubs = n;
	    }

	  elf_tdata (abfd)->local_stubs[r_symndx] = sec;

	  /* We don't need to set mips16_stubs_seen in this case.
             That flag is used to see whether we need to look through
             the global symbol table for stubs.  We don't need to set
             it here, because we just have a local stub.  */
	}
      else
	{
	  struct mips_elf64_link_hash_entry *h;

	  h = ((struct mips_elf64_link_hash_entry *)
	       sym_hashes[r_symndx - extsymoff]);

	  /* H is the symbol this stub is for.  */

	  h->fn_stub = sec;
	  mips_elf64_hash_table (info)->mips16_stubs_seen = true;
	}
    }
  else if (strncmp (name, CALL_STUB, sizeof CALL_STUB - 1) == 0
	   || strncmp (name, CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0)
    {
      unsigned long r_symndx;
      struct mips_elf64_link_hash_entry *h;
      asection **loc;

      /* Look at the relocation information to figure out which symbol
         this is for.  */

      r_symndx = ELF64_R_SYM (relocs->r_info);

      if (r_symndx < extsymoff
	  || sym_hashes[r_symndx - extsymoff] == NULL)
	{
	  /* This stub was actually built for a static symbol defined
	     in the same file.  We assume that all static symbols in
	     mips16 code are themselves mips16, so we can simply
	     discard this stub.  Since this function is called before
	     the linker maps input sections to output sections, we can
	     easily discard it by setting the SEC_EXCLUDE flag.  */
	  sec->flags |= SEC_EXCLUDE;
	  return true;
	}

      h = ((struct mips_elf64_link_hash_entry *)
	   sym_hashes[r_symndx - extsymoff]);

      /* H is the symbol this stub is for.  */

      if (strncmp (name, CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0)
	loc = &h->call_fp_stub;
      else
	loc = &h->call_stub;

      /* If we already have an appropriate stub for this function, we
	 don't need another one, so we can discard this one.  Since
	 this function is called before the linker maps input sections
	 to output sections, we can easily discard it by setting the
	 SEC_EXCLUDE flag.  We can also discard this section if we
	 happen to already know that this is a mips16 function; it is
	 not necessary to check this here, as it is checked later, but
	 it is slightly faster to check now.  */
      if (*loc != NULL || h->root.other == STO_MIPS16)
	{
	  sec->flags |= SEC_EXCLUDE;
	  return true;
	}

      *loc = sec;
      mips_elf64_hash_table (info)->mips16_stubs_seen = true;
    }

  if (dynobj == NULL)
    {
      sgot = NULL;
      g = NULL;
    }
  else
    {
      sgot = bfd_get_section_by_name (dynobj, ".got");
      if (sgot == NULL)
	g = NULL;
      else
	{
	  BFD_ASSERT (elf_section_data (sgot) != NULL);
	  g = (struct mips_elf64_got_info *) elf_section_data (sgot)->tdata;
	  BFD_ASSERT (g != NULL);
	}
    }

  sreloc = NULL;
  bed = get_elf_backend_data (abfd);
  rel_end = relocs + sec->reloc_count * bed->s->int_rels_per_ext_rel;
  for (rel = relocs; rel < rel_end; ++rel)
    {
      unsigned long r_symndx;
      int r_type;
      struct elf_link_hash_entry *h;

      r_symndx = ELF64_R_SYM (rel->r_info);
      r_type = ELF64_MIPS_R_TYPE (rel->r_info);

      if (r_symndx < extsymoff)
	h = NULL;
      else if (r_symndx >= extsymoff + NUM_SHDR_ENTRIES (symtab_hdr))
	{
	  (*_bfd_error_handler)
	    (_("%s: Malformed reloc detected for section %s"),
	     bfd_archive_filename (abfd), name);
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      else
	{
	  h = sym_hashes[r_symndx - extsymoff];

	  /* This may be an indirect symbol created because of a version.  */
	  if (h != NULL)
	    {
	      while (h->root.type == bfd_link_hash_indirect)
		h = (struct elf_link_hash_entry *) h->root.u.i.link;
	    }
	}

      /* Some relocs require a global offset table.  */
      if (dynobj == NULL || sgot == NULL)
	{
	  switch (r_type)
	    {
	    case R_MIPS_GOT16:
	    case R_MIPS_CALL16:
	    case R_MIPS_CALL_HI16:
	    case R_MIPS_CALL_LO16:
	    case R_MIPS_GOT_HI16:
	    case R_MIPS_GOT_LO16:
	    case R_MIPS_GOT_PAGE:
	    case R_MIPS_GOT_OFST:
	    case R_MIPS_GOT_DISP:
	      if (dynobj == NULL)
		elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (! mips_elf64_create_got_section (dynobj, info))
		return false;
	      g = _mips_elf64_got_info (dynobj, &sgot);
	      break;

	    case R_MIPS_32:
	    case R_MIPS_REL32:
	    case R_MIPS_64:
	      if (dynobj == NULL
		  && (info->shared || h != NULL)
		  && (sec->flags & SEC_ALLOC) != 0)
		elf_hash_table (info)->dynobj = dynobj = abfd;
	      break;

	    default:
	      break;
	    }
	}

      if (!h && (r_type == R_MIPS_CALL_LO16
		 || r_type == R_MIPS_GOT_LO16
		 || r_type == R_MIPS_GOT_DISP))
	{
	  /* We may need a local GOT entry for this relocation.  We
	     don't count R_MIPS_GOT_PAGE because we can estimate the
	     maximum number of pages needed by looking at the size of
	     the segment.  Similar comments apply to R_MIPS_GOT16 and
	     R_MIPS_CALL16.  We don't count R_MIPS_GOT_HI16, or
	     R_MIPS_CALL_HI16 because these are always followed by an
	     R_MIPS_GOT_LO16 or R_MIPS_CALL_LO16.

	     This estimation is very conservative since we can merge
	     duplicate entries in the GOT.  In order to be less
	     conservative, we could actually build the GOT here,
	     rather than in relocate_section.  */
	  g->local_gotno++;
	  sgot->_raw_size += get_elf_backend_data (dynobj)->s->arch_size / 8;
	}

      switch (r_type)
	{
	case R_MIPS_CALL16:
	  if (h == NULL)
	    {
	      (*_bfd_error_handler)
		(_("%s: CALL16 reloc at 0x%lx not against global symbol"),
		 bfd_archive_filename (abfd), (unsigned long) rel->r_offset);
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	  /* Fall through.  */

	case R_MIPS_CALL_HI16:
	case R_MIPS_CALL_LO16:
	  if (h != NULL)
	    {
	      /* This symbol requires a global offset table entry.  */
	      if (!mips_elf64_record_global_got_symbol (h, info, g))
		return false;

	      /* We need a stub, not a plt entry for the undefined
		 function.  But we record it as if it needs plt.  See
		 elf_adjust_dynamic_symbol in elflink.h.  */
	      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	      h->type = STT_FUNC;
	    }
	  break;

	case R_MIPS_GOT16:
	case R_MIPS_GOT_HI16:
	case R_MIPS_GOT_LO16:
	case R_MIPS_GOT_DISP:
	  /* This symbol requires a global offset table entry.  */
	  if (h && !mips_elf64_record_global_got_symbol (h, info, g))
	    return false;
	  break;

	case R_MIPS_32:
	case R_MIPS_REL32:
	case R_MIPS_64:
	  if ((info->shared || h != NULL)
	      && (sec->flags & SEC_ALLOC) != 0)
	    {
	      if (sreloc == NULL)
		{
		  const char *name = ".rel.dyn";

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  if (sreloc == NULL)
		    {
		      sreloc = bfd_make_section (dynobj, name);
		      if (sreloc == NULL
			  || ! bfd_set_section_flags (dynobj, sreloc,
						      (SEC_ALLOC
						       | SEC_LOAD
						       | SEC_HAS_CONTENTS
						       | SEC_IN_MEMORY
						       | SEC_LINKER_CREATED
						       | SEC_READONLY))
			  || ! bfd_set_section_alignment (dynobj, sreloc,
							  4))
			return false;
		    }
		}
#define MIPS_READONLY_SECTION (SEC_ALLOC | SEC_LOAD | SEC_READONLY)
	      if (info->shared)
		{
		  /* When creating a shared object, we must copy these
		     reloc types into the output file as R_MIPS_REL32
		     relocs.  We make room for this reloc in the
		     .rel.dyn reloc section.  */
		  mips_elf64_allocate_dynamic_relocations (dynobj, 1);
		  if ((sec->flags & MIPS_READONLY_SECTION)
		      == MIPS_READONLY_SECTION)
		    /* We tell the dynamic linker that there are
		       relocations against the text segment.  */
		    info->flags |= DF_TEXTREL;
		}
	      else
		{
		  struct mips_elf64_link_hash_entry *hmips;

		  /* We only need to copy this reloc if the symbol is
                     defined in a dynamic object.  */
		  hmips = (struct mips_elf64_link_hash_entry *) h;
		  ++hmips->possibly_dynamic_relocs;
		  if ((sec->flags & MIPS_READONLY_SECTION)
		      == MIPS_READONLY_SECTION)
		    /* We need it to tell the dynamic linker if there
		       are relocations against the text segment.  */
		    hmips->readonly_reloc = true;
		}
	     
	      /* Even though we don't directly need a GOT entry for
		 this symbol, a symbol must have a dynamic symbol
		 table index greater that DT_MIPS_GOTSYM if there are
		 dynamic relocations against it.  */
	      if (h != NULL
		  && !mips_elf64_record_global_got_symbol (h, info, g))
		return false;
	    }
	  break;

	case R_MIPS_26:
	case R_MIPS_GPREL16:
	case R_MIPS_LITERAL:
	case R_MIPS_GPREL32:
	  break;

	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	case R_MIPS_GNU_VTINHERIT:
	  if (!_bfd_elf64_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return false;
	  break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_MIPS_GNU_VTENTRY:
	  if (!_bfd_elf64_gc_record_vtentry (abfd, sec, h, rel->r_offset))
	    return false;
	  break;

	default:
	  break;
	}
    }

  return true;
}

/* Structure used to pass information to mips_elf64_output_extsym.  */

struct extsym_info
{
  bfd *abfd;
  struct bfd_link_info *info;
  struct ecoff_debug_info *debug;
  const struct ecoff_debug_swap *swap;
  boolean failed;
};

/* This routine is used to write out ECOFF debugging external symbol
   information.  It is called via mips_elf64_link_hash_traverse.  The
   ECOFF external symbol information must match the ELF external
   symbol information.  Unfortunately, at this point we don't know
   whether a symbol is required by reloc information, so the two
   tables may wind up being different.  We must sort out the external
   symbol information before we can set the final size of the .mdebug
   section, and we must set the size of the .mdebug section before we
   can relocate any sections, and we can't know which symbols are
   required by relocation until we relocate the sections.
   Fortunately, it is relatively unlikely that any symbol will be
   stripped but required by a reloc.  In particular, it can not happen
   when generating a final executable.  */

static boolean
mips_elf64_output_extsym (h, data)
     struct mips_elf64_link_hash_entry *h;
     PTR data;
{
  struct extsym_info *einfo = (struct extsym_info *) data;
  boolean strip;
  asection *sec, *output_section;

  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct mips_elf64_link_hash_entry *) h->root.root.u.i.link;

  if (h->root.indx == -2)
    strip = false;
  else if (((h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	    || (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0)
	   && (h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
	   && (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    strip = true;
  else if (einfo->info->strip == strip_all
	   || (einfo->info->strip == strip_some
	       && bfd_hash_lookup (einfo->info->keep_hash,
				   h->root.root.root.string,
				   false, false) == NULL))
    strip = true;
  else
    strip = false;

  if (strip)
    return true;

  if (h->esym.ifd == -2)
    {
      h->esym.jmptbl = 0;
      h->esym.cobol_main = 0;
      h->esym.weakext = 0;
      h->esym.reserved = 0;
      h->esym.ifd = ifdNil;
      h->esym.asym.value = 0;
      h->esym.asym.st = stGlobal;

      if (h->root.root.type == bfd_link_hash_undefined
	      || h->root.root.type == bfd_link_hash_undefweak)
	{
	  const char *name;

	  /* Use undefined class.  Also, set class and type for some
             special symbols.  */
	  name = h->root.root.root.string;
	  h->esym.asym.sc = scUndefined;
	}
      else if (h->root.root.type != bfd_link_hash_defined
	  && h->root.root.type != bfd_link_hash_defweak)
	h->esym.asym.sc = scAbs;
      else
	{
	  const char *name;

	  sec = h->root.root.u.def.section;
	  output_section = sec->output_section;

	  /* When making a shared library and symbol h is the one from
	     the another shared library, OUTPUT_SECTION may be null.  */
	  if (output_section == NULL)
	    h->esym.asym.sc = scUndefined;
	  else
	    {
	      name = bfd_section_name (output_section->owner, output_section);

	      if (strcmp (name, ".text") == 0)
		h->esym.asym.sc = scText;
	      else if (strcmp (name, ".data") == 0)
		h->esym.asym.sc = scData;
	      else if (strcmp (name, ".sdata") == 0)
		h->esym.asym.sc = scSData;
	      else if (strcmp (name, ".rodata") == 0
		       || strcmp (name, ".rdata") == 0)
		h->esym.asym.sc = scRData;
	      else if (strcmp (name, ".bss") == 0)
		h->esym.asym.sc = scBss;
	      else if (strcmp (name, ".sbss") == 0)
		h->esym.asym.sc = scSBss;
	      else if (strcmp (name, ".init") == 0)
		h->esym.asym.sc = scInit;
	      else if (strcmp (name, ".fini") == 0)
		h->esym.asym.sc = scFini;
	      else
		h->esym.asym.sc = scAbs;
	    }
	}

      h->esym.asym.reserved = 0;
      h->esym.asym.index = indexNil;
    }

  if (h->root.root.type == bfd_link_hash_common)
    h->esym.asym.value = h->root.root.u.c.size;
  else if (h->root.root.type == bfd_link_hash_defined
	   || h->root.root.type == bfd_link_hash_defweak)
    {
      if (h->esym.asym.sc == scCommon)
	h->esym.asym.sc = scBss;
      else if (h->esym.asym.sc == scSCommon)
	h->esym.asym.sc = scSBss;

      sec = h->root.root.u.def.section;
      output_section = sec->output_section;
      if (output_section != NULL)
	h->esym.asym.value = (h->root.root.u.def.value
			      + sec->output_offset
			      + output_section->vma);
      else
	h->esym.asym.value = 0;
    }
  else if ((h->root.elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      struct mips_elf64_link_hash_entry *hd = h;
      boolean no_fn_stub = h->no_fn_stub;

      while (hd->root.root.type == bfd_link_hash_indirect)
	{
	  hd = (struct mips_elf64_link_hash_entry *)h->root.root.u.i.link;
	  no_fn_stub = no_fn_stub || hd->no_fn_stub;
	}

      if (!no_fn_stub)
	{
	  /* Set type and value for a symbol with a function stub.  */
	  h->esym.asym.st = stProc;
	  sec = hd->root.root.u.def.section;
	  if (sec == NULL)
	    h->esym.asym.value = 0;
	  else
	    {
	      output_section = sec->output_section;
	      if (output_section != NULL)
		h->esym.asym.value = (hd->root.plt.offset
				      + sec->output_offset
				      + output_section->vma);
	      else
		h->esym.asym.value = 0;
	    }
#if 0 /* FIXME?  */
	  h->esym.ifd = 0;
#endif
	}
    }

  if (! bfd_ecoff_debug_one_external (einfo->abfd, einfo->debug, einfo->swap,
				      h->root.root.root.string,
				      &h->esym))
    {
      einfo->failed = true;
      return false;
    }

  return true;
}

/* Swap an entry in a .gptab section.  Note that these routines rely
   on the equivalence of the two elements of the union.  */

static void
mips_elf64_swap_gptab_in (abfd, ex, in)
     bfd *abfd;
     const Elf32_External_gptab *ex;
     Elf32_gptab *in;
{
  in->gt_entry.gt_g_value = H_GET_32 (abfd, ex->gt_entry.gt_g_value);
  in->gt_entry.gt_bytes = H_GET_32 (abfd, ex->gt_entry.gt_bytes);
}

static void
mips_elf64_swap_gptab_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_gptab *in;
     Elf32_External_gptab *ex;
{
  H_PUT_32 (abfd, (bfd_vma) in->gt_entry.gt_g_value,
		ex->gt_entry.gt_g_value);
  H_PUT_32 (abfd, (bfd_vma) in->gt_entry.gt_bytes,
		ex->gt_entry.gt_bytes);
}

/* A comparison routine used to sort .gptab entries.  */

static int
gptab_compare (p1, p2)
     const PTR p1;
     const PTR p2;
{
  const Elf32_gptab *a1 = (const Elf32_gptab *) p1;
  const Elf32_gptab *a2 = (const Elf32_gptab *) p2;

  return a1->gt_entry.gt_g_value - a2->gt_entry.gt_g_value;
}

/* We need to use a special link routine to handle the .mdebug section.
   We need to merge all instances of this section together, not write
   them all out sequentially.  */

boolean
mips_elf64_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection **secpp;
  asection *o;
  struct bfd_link_order *p;
  asection *mdebug_sec, *gptab_data_sec, *gptab_bss_sec;
  struct ecoff_debug_info debug;
  const struct ecoff_debug_swap *swap
    = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;
  HDRR *symhdr = &debug.symbolic_header;
  PTR mdebug_handle = NULL;
  asection *s;
  EXTR esym;
  unsigned int i;
  static const char * const secname[] =
      { ".text", ".init", ".fini", ".data",
          ".rodata", ".sdata", ".sbss", ".bss" };
  static const int sc[] = { scText, scInit, scFini, scData,
                          scRData, scSData, scSBss, scBss };

  /* If all the things we linked together were PIC, but we're
     producing an executable (rather than a shared object), then the
     resulting file is CPIC (i.e., it calls PIC code.)  */
  if (!info->shared
      && !info->relocateable
      && elf_elfheader (abfd)->e_flags & EF_MIPS_PIC)
    {
      elf_elfheader (abfd)->e_flags &= ~EF_MIPS_PIC;
      elf_elfheader (abfd)->e_flags |= EF_MIPS_CPIC;
    }

  /* We'd carefully arranged the dynamic symbol indices, and then the
     generic size_dynamic_sections renumbered them out from under us.
     Rather than trying somehow to prevent the renumbering, just do
     the sort again.  */
  if (elf_hash_table (info)->dynamic_sections_created)
    {
      bfd *dynobj;
      asection *got;
      struct mips_elf64_got_info *g;

      /* When we resort, we must tell mips_elf64_sort_hash_table what
	 the lowest index it may use is.  That's the number of section
	 symbols we're going to add.  The generic ELF linker only
	 adds these symbols when building a shared object.  Note that
	 we count the sections after (possibly) removing the .options
	 section above.  */
      if (!mips_elf64_sort_hash_table (info, (info->shared 
					    ? bfd_count_sections (abfd) + 1
					    : 1)))
        return false;

      /* Make sure we didn't grow the global .got region.  */
      dynobj = elf_hash_table (info)->dynobj;
      got = bfd_get_section_by_name (dynobj, ".got");
      g = (struct mips_elf64_got_info *) elf_section_data (got)->tdata;

      if (g->global_gotsym != NULL)
	BFD_ASSERT ((elf_hash_table (info)->dynsymcount
		     - g->global_gotsym->dynindx)
		    <= g->global_gotno);
    }

  /* We include .MIPS.options, even though we don't process it quite right.
     (Some entries are supposed to be merged.)  At IRIX6 empirically we seem
     to be better off including it than not.  */
  for (secpp = &abfd->sections; *secpp != NULL; secpp = &(*secpp)->next)
    {
      if (strcmp ((*secpp)->name, ".MIPS.options") == 0)
	{
	  for (p = (*secpp)->link_order_head; p != NULL; p = p->next)
	    if (p->type == bfd_indirect_link_order)
	      p->u.indirect.section->flags &=~ SEC_HAS_CONTENTS;
	  (*secpp)->link_order_head = NULL;
	  bfd_section_list_remove (abfd, secpp);
	  --abfd->section_count;
	    
	  break;
	}
    }

  /* Get a value for the GP register.  */
  if (elf_gp (abfd) == 0)
    {
      struct bfd_link_hash_entry *h;

      h = bfd_link_hash_lookup (info->hash, "_gp", false, false, true);
      if (h != (struct bfd_link_hash_entry *) NULL
	  && h->type == bfd_link_hash_defined)
	elf_gp (abfd) = (h->u.def.value
			 + h->u.def.section->output_section->vma
			 + h->u.def.section->output_offset);
      else if (info->relocateable)
	{
	  bfd_vma lo = MINUS_ONE;

	  /* Find the GP-relative section with the lowest offset.  */
	  for (o = abfd->sections; o != NULL; o = o->next)
	    if (o->vma < lo 
		&& (elf_section_data (o)->this_hdr.sh_flags & SHF_MIPS_GPREL))
	      lo = o->vma;

	  /* And calculate GP relative to that.  */
	  elf_gp (abfd) = (lo + 0x7ff0);
	}
      else
	{
	  /* If the relocate_section function needs to do a reloc
	     involving the GP value, it should make a reloc_dangerous
	     callback to warn that GP is not defined.  */
	}
    }

  /* Go through the sections and collect the .mdebug information.  */
  mdebug_sec = NULL;
  gptab_data_sec = NULL;
  gptab_bss_sec = NULL;
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      if (strcmp (o->name, ".mdebug") == 0)
	{
	  struct extsym_info einfo;
	  bfd_vma last;

	  /* We have found the .mdebug section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  symhdr->magic = swap->sym_magic;
	  /* FIXME: What should the version stamp be?  */
	  symhdr->vstamp = 0;
	  symhdr->ilineMax = 0;
	  symhdr->cbLine = 0;
	  symhdr->idnMax = 0;
	  symhdr->ipdMax = 0;
	  symhdr->isymMax = 0;
	  symhdr->ioptMax = 0;
	  symhdr->iauxMax = 0;
	  symhdr->issMax = 0;
	  symhdr->issExtMax = 0;
	  symhdr->ifdMax = 0;
	  symhdr->crfd = 0;
	  symhdr->iextMax = 0;

	  /* We accumulate the debugging information itself in the
	     debug_info structure.  */
	  debug.line = NULL;
	  debug.external_dnr = NULL;
	  debug.external_pdr = NULL;
	  debug.external_sym = NULL;
	  debug.external_opt = NULL;
	  debug.external_aux = NULL;
	  debug.ss = NULL;
	  debug.ssext = debug.ssext_end = NULL;
	  debug.external_fdr = NULL;
	  debug.external_rfd = NULL;
	  debug.external_ext = debug.external_ext_end = NULL;

	  mdebug_handle = bfd_ecoff_debug_init (abfd, &debug, swap, info);
	  if (mdebug_handle == (PTR) NULL)
	    return false;

          esym.jmptbl = 0;
          esym.cobol_main = 0;
          esym.weakext = 0;
          esym.reserved = 0;
          esym.ifd = ifdNil;
          esym.asym.iss = issNil;
          esym.asym.st = stLocal;
          esym.asym.reserved = 0;
          esym.asym.index = indexNil;
          last = 0;
	  for (i = 0; i < sizeof (secname) / sizeof (secname[0]); i++)
            {
              esym.asym.sc = sc[i];
              s = bfd_get_section_by_name (abfd, secname[i]);
              if (s != NULL)
                {
                  esym.asym.value = s->vma;
                  last = s->vma + s->_raw_size;
                }
              else
                esym.asym.value = last;
              if (!bfd_ecoff_debug_one_external (abfd, &debug, swap,
                                                 secname[i], &esym))
                return false;
            }

	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      const struct ecoff_debug_swap *input_swap;
	      struct ecoff_debug_info input_debug;
	      char *eraw_src;
	      char *eraw_end;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      if (bfd_get_flavour (input_bfd) != bfd_target_elf_flavour
		  || (get_elf_backend_data (input_bfd)
		      ->elf_backend_ecoff_debug_swap) == NULL)
		{
		  /* I don't know what a non MIPS ELF bfd would be
		     doing with a .mdebug section, but I don't really
		     want to deal with it.  */
		  continue;
		}

	      input_swap = (get_elf_backend_data (input_bfd)
			    ->elf_backend_ecoff_debug_swap);

	      BFD_ASSERT (p->size == input_section->_raw_size);

	      /* The ECOFF linking code expects that we have already
		 read in the debugging information and set up an
		 ecoff_debug_info structure, so we do that now.  */
	      if (! _bfd_mips_elf_read_ecoff_info (input_bfd, input_section,
						   &input_debug))
		return false;

	      if (! (bfd_ecoff_debug_accumulate
		     (mdebug_handle, abfd, &debug, swap, input_bfd,
		      &input_debug, input_swap, info)))
		return false;

	      /* Loop through the external symbols.  For each one with
		 interesting information, try to find the symbol in
		 the linker global hash table and save the information
		 for the output external symbols.  */
	      eraw_src = input_debug.external_ext;
	      eraw_end = (eraw_src
			  + (input_debug.symbolic_header.iextMax
			     * input_swap->external_ext_size));
	      for (;
		   eraw_src < eraw_end;
		   eraw_src += input_swap->external_ext_size)
		{
		  EXTR ext;
		  const char *name;
		  struct mips_elf64_link_hash_entry *h;

		  (*input_swap->swap_ext_in) (input_bfd, (PTR) eraw_src, &ext);
		  if (ext.asym.sc == scNil
		      || ext.asym.sc == scUndefined
		      || ext.asym.sc == scSUndefined)
		    continue;

		  name = input_debug.ssext + ext.asym.iss;
		  h = mips_elf64_link_hash_lookup (mips_elf64_hash_table (info),
						 name, false, false, true);
		  if (h == NULL || h->esym.ifd != -2)
		    continue;

		  if (ext.ifd != -1)
		    {
		      BFD_ASSERT (ext.ifd
				  < input_debug.symbolic_header.ifdMax);
		      ext.ifd = input_debug.ifdmap[ext.ifd];
		    }

		  h->esym = ext;
		}

	      /* Free up the information we just read.  */
	      free (input_debug.line);
	      free (input_debug.external_dnr);
	      free (input_debug.external_pdr);
	      free (input_debug.external_sym);
	      free (input_debug.external_opt);
	      free (input_debug.external_aux);
	      free (input_debug.ss);
	      free (input_debug.ssext);
	      free (input_debug.external_fdr);
	      free (input_debug.external_rfd);
	      free (input_debug.external_ext);

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* Build the external symbol information.  */
	  einfo.abfd = abfd;
	  einfo.info = info;
	  einfo.debug = &debug;
	  einfo.swap = swap;
	  einfo.failed = false;
	  mips_elf64_link_hash_traverse (mips_elf64_hash_table (info),
					 mips_elf64_output_extsym,
					 (PTR) &einfo);
	  if (einfo.failed)
	    return false;

	  /* Set the size of the .mdebug section.  */
	  o->_raw_size = bfd_ecoff_debug_size (abfd, &debug, swap);

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  mdebug_sec = o;
	}

      if (strncmp (o->name, ".gptab.", sizeof ".gptab." - 1) == 0)
	{
	  const char *subname;
	  unsigned int c;
	  Elf32_gptab *tab;
	  Elf32_External_gptab *ext_tab;
	  unsigned int i;

	  /* The .gptab.sdata and .gptab.sbss sections hold
	     information describing how the small data area would
	     change depending upon the -G switch.  These sections
	     not used in executables files.  */
	  if (! info->relocateable)
	    {
	      asection **secpp;

	      for (p = o->link_order_head;
		   p != (struct bfd_link_order *) NULL;
		   p = p->next)
		{
		  asection *input_section;

		  if (p->type != bfd_indirect_link_order)
		    {
		      if (p->type == bfd_fill_link_order)
			continue;
		      abort ();
		    }

		  input_section = p->u.indirect.section;

		  /* Hack: reset the SEC_HAS_CONTENTS flag so that
		     elf_link_input_bfd ignores this section.  */
		  input_section->flags &=~ SEC_HAS_CONTENTS;
		}

	      /* Skip this section later on (I don't think this
		 currently matters, but someday it might).  */
	      o->link_order_head = (struct bfd_link_order *) NULL;

	      /* Really remove the section.  */
	      for (secpp = &abfd->sections;
		   *secpp != o;
		   secpp = &(*secpp)->next)
		;
	      bfd_section_list_remove (abfd, secpp);
	      --abfd->section_count;

	      continue;
	    }

	  /* There is one gptab for initialized data, and one for
	     uninitialized data.  */
	  if (strcmp (o->name, ".gptab.sdata") == 0)
	    gptab_data_sec = o;
	  else if (strcmp (o->name, ".gptab.sbss") == 0)
	    gptab_bss_sec = o;
	  else
	    {
	      (*_bfd_error_handler)
		(_("%s: illegal section name `%s'"),
		 bfd_archive_filename (abfd), o->name);
	      bfd_set_error (bfd_error_nonrepresentable_section);
	      return false;
	    }

	  /* The linker script always combines .gptab.data and
	     .gptab.sdata into .gptab.sdata, and likewise for
	     .gptab.bss and .gptab.sbss.  It is possible that there is
	     no .sdata or .sbss section in the output file, in which
	     case we must change the name of the output section.  */
	  subname = o->name + sizeof ".gptab" - 1;
	  if (bfd_get_section_by_name (abfd, subname) == NULL)
	    {
	      if (o == gptab_data_sec)
		o->name = ".gptab.data";
	      else
		o->name = ".gptab.bss";
	      subname = o->name + sizeof ".gptab" - 1;
	      BFD_ASSERT (bfd_get_section_by_name (abfd, subname) != NULL);
	    }

	  /* Set up the first entry.  */
	  c = 1;
	  tab = (Elf32_gptab *) bfd_malloc (c * sizeof (Elf32_gptab));
	  if (tab == NULL)
	    return false;
	  tab[0].gt_header.gt_current_g_value = elf_gp_size (abfd);
	  tab[0].gt_header.gt_unused = 0;

	  /* Combine the input sections.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      bfd_size_type size;
	      unsigned long last;
	      bfd_size_type gpentry;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* Combine the gptab entries for this input section one
		 by one.  We know that the input gptab entries are
		 sorted by ascending -G value.  */
	      size = bfd_section_size (input_bfd, input_section);
	      last = 0;
	      for (gpentry = sizeof (Elf32_External_gptab);
		   gpentry < size;
		   gpentry += sizeof (Elf32_External_gptab))
		{
		  Elf32_External_gptab ext_gptab;
		  Elf32_gptab int_gptab;
		  unsigned long val;
		  unsigned long add;
		  boolean exact;
		  unsigned int look;

		  if (! (bfd_get_section_contents
			 (input_bfd, input_section, (PTR) &ext_gptab,
			  gpentry, sizeof (Elf32_External_gptab))))
		    {
		      free (tab);
		      return false;
		    }

		  mips_elf64_swap_gptab_in (input_bfd, &ext_gptab,
						&int_gptab);
		  val = int_gptab.gt_entry.gt_g_value;
		  add = int_gptab.gt_entry.gt_bytes - last;

		  exact = false;
		  for (look = 1; look < c; look++)
		    {
		      if (tab[look].gt_entry.gt_g_value >= val)
			tab[look].gt_entry.gt_bytes += add;

		      if (tab[look].gt_entry.gt_g_value == val)
			exact = true;
		    }

		  if (! exact)
		    {
		      Elf32_gptab *new_tab;
		      unsigned int max;

		      /* We need a new table entry.  */
		      new_tab = ((Elf32_gptab *)
				 bfd_realloc ((PTR) tab,
					      (c + 1) * sizeof (Elf32_gptab)));
		      if (new_tab == NULL)
			{
			  free (tab);
			  return false;
			}
		      tab = new_tab;
		      tab[c].gt_entry.gt_g_value = val;
		      tab[c].gt_entry.gt_bytes = add;

		      /* Merge in the size for the next smallest -G
			 value, since that will be implied by this new
			 value.  */
		      max = 0;
		      for (look = 1; look < c; look++)
			{
			  if (tab[look].gt_entry.gt_g_value < val
			      && (max == 0
				  || (tab[look].gt_entry.gt_g_value
				      > tab[max].gt_entry.gt_g_value)))
			    max = look;
			}
		      if (max != 0)
			tab[c].gt_entry.gt_bytes +=
			  tab[max].gt_entry.gt_bytes;

		      ++c;
		    }

		  last = int_gptab.gt_entry.gt_bytes;
		}

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* The table must be sorted by -G value.  */
	  if (c > 2)
	    qsort (tab + 1, c - 1, sizeof (tab[0]), gptab_compare);

	  /* Swap out the table.  */
	  ext_tab = ((Elf32_External_gptab *)
		     bfd_alloc (abfd, c * sizeof (Elf32_External_gptab)));
	  if (ext_tab == NULL)
	    {
	      free (tab);
	      return false;
	    }

	  for (i = 0; i < c; i++)
	    mips_elf64_swap_gptab_out (abfd, tab + i, ext_tab + i);
	  free (tab);

	  o->_raw_size = c * sizeof (Elf32_External_gptab);
	  o->contents = (bfd_byte *) ext_tab;

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;
	}
    }

  /* Invoke the regular ELF backend linker to do all the work.  */
  if (!bfd_elf64_bfd_final_link (abfd, info))
     return false;

  /* Now write out the computed sections.  */
  if (mdebug_sec != (asection *) NULL)
    {
      BFD_ASSERT (abfd->output_has_begun);
      if (! bfd_ecoff_write_accumulated_debug (mdebug_handle, abfd, &debug,
					       swap, info,
					       mdebug_sec->filepos))
	return false;

      bfd_ecoff_debug_free (mdebug_handle, abfd, &debug, swap, info);
    }
  if (gptab_data_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_data_sec,
				      gptab_data_sec->contents,
				      (file_ptr) 0,
				      gptab_data_sec->_raw_size))
	return false;
    }

  if (gptab_bss_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_bss_sec,
				      gptab_bss_sec->contents,
				      (file_ptr) 0,
				      gptab_bss_sec->_raw_size))
	return false;
    }

  return true;
}

/* ECOFF swapping routines.  These are used when dealing with the
   .mdebug section, which is in the ECOFF debugging format.  */
static const struct ecoff_debug_swap mips_elf64_ecoff_debug_swap =
{
  /* Symbol table magic number.  */
  magicSym2,
  /* Alignment of debugging information.  E.g., 4.  */
  8,
  /* Sizes of external symbolic information.  */
  sizeof (struct hdr_ext),
  sizeof (struct dnr_ext),
  sizeof (struct pdr_ext),
  sizeof (struct sym_ext),
  sizeof (struct opt_ext),
  sizeof (struct fdr_ext),
  sizeof (struct rfd_ext),
  sizeof (struct ext_ext),
  /* Functions to swap in external symbolic data.  */
  ecoff_swap_hdr_in,
  ecoff_swap_dnr_in,
  ecoff_swap_pdr_in,
  ecoff_swap_sym_in,
  ecoff_swap_opt_in,
  ecoff_swap_fdr_in,
  ecoff_swap_rfd_in,
  ecoff_swap_ext_in,
  _bfd_ecoff_swap_tir_in,
  _bfd_ecoff_swap_rndx_in,
  /* Functions to swap out external symbolic data.  */
  ecoff_swap_hdr_out,
  ecoff_swap_dnr_out,
  ecoff_swap_pdr_out,
  ecoff_swap_sym_out,
  ecoff_swap_opt_out,
  ecoff_swap_fdr_out,
  ecoff_swap_rfd_out,
  ecoff_swap_ext_out,
  _bfd_ecoff_swap_tir_out,
  _bfd_ecoff_swap_rndx_out,
  /* Function to read in symbolic data.  */
  _bfd_mips_elf_read_ecoff_info
};

/* Relocations in the 64 bit MIPS ELF ABI are more complex than in
   standard ELF.  This structure is used to redirect the relocation
   handling routines.  */

const struct elf_size_info mips_elf64_size_info =
{
  sizeof (Elf64_External_Ehdr),
  sizeof (Elf64_External_Phdr),
  sizeof (Elf64_External_Shdr),
  sizeof (Elf64_Mips_External_Rel),
  sizeof (Elf64_Mips_External_Rela),
  sizeof (Elf64_External_Sym),
  sizeof (Elf64_External_Dyn),
  sizeof (Elf_External_Note),
  4,            /* hash-table entry size */
  3,            /* internal relocations per external relocations */
  64,		/* arch_size */
  8,		/* file_align */
  ELFCLASS64,
  EV_CURRENT,
  bfd_elf64_write_out_phdrs,
  bfd_elf64_write_shdrs_and_ehdr,
  mips_elf64_write_relocs,
  bfd_elf64_swap_symbol_out,
  mips_elf64_slurp_reloc_table,
  bfd_elf64_slurp_symbol_table,
  bfd_elf64_swap_dyn_in,
  bfd_elf64_swap_dyn_out,
  mips_elf64_be_swap_reloc_in,
  mips_elf64_be_swap_reloc_out,
  mips_elf64_be_swap_reloca_in,
  mips_elf64_be_swap_reloca_out
};

#define ELF_ARCH			bfd_arch_mips
#define ELF_MACHINE_CODE		EM_MIPS

#define ELF_MAXPAGESIZE			0x1000

#define elf_backend_collect		true
#define elf_backend_type_change_ok	true
#define elf_backend_can_gc_sections	true
#define elf_info_to_howto		mips_elf64_info_to_howto_rela
#define elf_info_to_howto_rel		mips_elf64_info_to_howto_rel
#define elf_backend_object_p		_bfd_mips_elf_object_p
#define elf_backend_symbol_processing	_bfd_mips_elf_symbol_processing
#define elf_backend_section_processing	_bfd_mips_elf_section_processing
#define elf_backend_section_from_shdr	_bfd_mips_elf_section_from_shdr
#define elf_backend_fake_sections	_bfd_mips_elf_fake_sections
#define elf_backend_section_from_bfd_section \
					_bfd_mips_elf_section_from_bfd_section
#define elf_backend_add_symbol_hook	_bfd_mips_elf_add_symbol_hook
#define elf_backend_link_output_symbol_hook \
					_bfd_mips_elf_link_output_symbol_hook
#define elf_backend_create_dynamic_sections \
					mips_elf64_create_dynamic_sections
#define elf_backend_check_relocs	mips_elf64_check_relocs
#define elf_backend_adjust_dynamic_symbol \
					mips_elf64_adjust_dynamic_symbol
#define elf_backend_always_size_sections \
					mips_elf64_always_size_sections
#define elf_backend_size_dynamic_sections \
					mips_elf64_size_dynamic_sections
#define elf_backend_relocate_section    mips_elf64_relocate_section
#define elf_backend_finish_dynamic_symbol \
					mips_elf64_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					mips_elf64_finish_dynamic_sections
#define elf_backend_final_write_processing \
					_bfd_mips_elf_final_write_processing
#define elf_backend_additional_program_headers \
					mips_elf64_additional_program_headers
#define elf_backend_modify_segment_map	_bfd_mips_elf_modify_segment_map
#define elf_backend_gc_mark_hook	mips_elf64_gc_mark_hook
#define elf_backend_gc_sweep_hook	mips_elf64_gc_sweep_hook
#define elf_backend_ecoff_debug_swap	&mips_elf64_ecoff_debug_swap
#define elf_backend_size_info		mips_elf64_size_info

#define elf_backend_got_header_size	(4 * MIPS_RESERVED_GOTNO)
#define elf_backend_plt_header_size	0

/* MIPS ELF64 can use a mixture of REL and RELA, but some Relocations
 * work better/work only in RELA, so we default to this.  */
#define elf_backend_may_use_rel_p	1
#define elf_backend_may_use_rela_p	1
#define elf_backend_default_use_rela_p	1

/* We don't set bfd_elf64_bfd_is_local_label_name because the 32-bit
   MIPS-specific function only applies to IRIX5, which had no 64-bit
   ABI.  */
#define bfd_elf64_find_nearest_line	_bfd_mips_elf_find_nearest_line
#define bfd_elf64_set_section_contents	_bfd_mips_elf_set_section_contents
#define bfd_elf64_bfd_link_hash_table_create \
					mips_elf64_link_hash_table_create
#define bfd_elf64_bfd_final_link	mips_elf64_final_link
#define bfd_elf64_bfd_merge_private_bfd_data \
					_bfd_mips_elf_merge_private_bfd_data
#define bfd_elf64_bfd_set_private_flags	_bfd_mips_elf_set_private_flags
#define bfd_elf64_bfd_print_private_bfd_data \
					_bfd_mips_elf_print_private_bfd_data

#define bfd_elf64_get_reloc_upper_bound mips_elf64_get_reloc_upper_bound
#define bfd_elf64_bfd_reloc_type_lookup	mips_elf64_reloc_type_lookup
#define bfd_elf64_archive_functions
extern boolean bfd_elf64_archive_slurp_armap
  PARAMS((bfd *));
extern boolean bfd_elf64_archive_write_armap
  PARAMS((bfd *, unsigned int, struct orl *, unsigned int, int));
#define bfd_elf64_archive_slurp_extended_name_table \
				_bfd_archive_coff_slurp_extended_name_table
#define bfd_elf64_archive_construct_extended_name_table \
				_bfd_archive_coff_construct_extended_name_table
#define bfd_elf64_archive_truncate_arname \
					_bfd_archive_coff_truncate_arname
#define bfd_elf64_archive_read_ar_hdr	_bfd_archive_coff_read_ar_hdr
#define bfd_elf64_archive_openr_next_archived_file \
				_bfd_archive_coff_openr_next_archived_file
#define bfd_elf64_archive_get_elt_at_index \
					_bfd_archive_coff_get_elt_at_index
#define bfd_elf64_archive_generic_stat_arch_elt \
					_bfd_archive_coff_generic_stat_arch_elt
#define bfd_elf64_archive_update_armap_timestamp \
				_bfd_archive_coff_update_armap_timestamp

/* The SGI style (n)64 NewABI.  */
#define TARGET_LITTLE_SYM		bfd_elf64_littlemips_vec
#define TARGET_LITTLE_NAME		"elf64-littlemips"
#define TARGET_BIG_SYM			bfd_elf64_bigmips_vec
#define TARGET_BIG_NAME			"elf64-bigmips"

#include "elf64-target.h"

#define INCLUDED_TARGET_FILE            /* More a type of flag.  */

/* The SYSV-style 'traditional' (n)64 NewABI.  */
#undef TARGET_LITTLE_SYM
#undef TARGET_LITTLE_NAME
#undef TARGET_BIG_SYM
#undef TARGET_BIG_NAME

#define TARGET_LITTLE_SYM               bfd_elf64_tradlittlemips_vec
#define TARGET_LITTLE_NAME              "elf64-tradlittlemips"
#define TARGET_BIG_SYM                  bfd_elf64_tradbigmips_vec
#define TARGET_BIG_NAME                 "elf64-tradbigmips"

/* Include the target file again for this target.  */
#include "elf64-target.h"
