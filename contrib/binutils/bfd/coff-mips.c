/* BFD back-end for MIPS Extended-Coff files.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 98, 1999
   Free Software Foundation, Inc.
   Original version by Per Bothner.
   Full support added by Ian Lance Taylor, ian@cygnus.com.

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
#include "bfdlink.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/ecoff.h"
#include "coff/mips.h"
#include "libcoff.h"
#include "libecoff.h"

/* Prototypes for static functions.  */

static boolean mips_ecoff_bad_format_hook PARAMS ((bfd *abfd, PTR filehdr));
static void mips_ecoff_swap_reloc_in PARAMS ((bfd *, PTR,
					      struct internal_reloc *));
static void mips_ecoff_swap_reloc_out PARAMS ((bfd *,
					       const struct internal_reloc *,
					       PTR));
static void mips_adjust_reloc_in PARAMS ((bfd *,
					  const struct internal_reloc *,
					  arelent *));
static void mips_adjust_reloc_out PARAMS ((bfd *, const arelent *,
					   struct internal_reloc *));
static bfd_reloc_status_type mips_generic_reloc PARAMS ((bfd *abfd,
							 arelent *reloc,
							 asymbol *symbol,
							 PTR data,
							 asection *section,
							 bfd *output_bfd,
							 char **error));
static bfd_reloc_status_type mips_refhi_reloc PARAMS ((bfd *abfd,
						       arelent *reloc,
						       asymbol *symbol,
						       PTR data,
						       asection *section,
						       bfd *output_bfd,
						       char **error));
static bfd_reloc_status_type mips_reflo_reloc PARAMS ((bfd *abfd,
						       arelent *reloc,
						       asymbol *symbol,
						       PTR data,
						       asection *section,
						       bfd *output_bfd,
						       char **error));
static bfd_reloc_status_type mips_gprel_reloc PARAMS ((bfd *abfd,
						       arelent *reloc,
						       asymbol *symbol,
						       PTR data,
						       asection *section,
						       bfd *output_bfd,
						       char **error));
static bfd_reloc_status_type mips_relhi_reloc PARAMS ((bfd *abfd,
						       arelent *reloc,
						       asymbol *symbol,
						       PTR data,
						       asection *section,
						       bfd *output_bfd,
						       char **error));
static bfd_reloc_status_type mips_rello_reloc PARAMS ((bfd *abfd,
						       arelent *reloc,
						       asymbol *symbol,
						       PTR data,
						       asection *section,
						       bfd *output_bfd,
						       char **error));
static bfd_reloc_status_type mips_switch_reloc PARAMS ((bfd *abfd,
							arelent *reloc,
							asymbol *symbol,
							PTR data,
							asection *section,
							bfd *output_bfd,
							char **error));
static void mips_relocate_hi PARAMS ((struct internal_reloc *refhi,
				      struct internal_reloc *reflo,
				      bfd *input_bfd,
				      asection *input_section,
				      bfd_byte *contents,
				      size_t adjust,
				      bfd_vma relocation,
				      boolean pcrel));
static boolean mips_relocate_section PARAMS ((bfd *, struct bfd_link_info *,
					      bfd *, asection *,
					      bfd_byte *, PTR));
static boolean mips_read_relocs PARAMS ((bfd *, asection *));
static boolean mips_relax_section PARAMS ((bfd *, asection *,
					   struct bfd_link_info *,
					   boolean *));
static boolean mips_relax_pcrel16 PARAMS ((struct bfd_link_info *, bfd *,
					   asection *,
					   struct ecoff_link_hash_entry *,
					   bfd_byte *, bfd_vma));
static reloc_howto_type *mips_bfd_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));


/* ECOFF has COFF sections, but the debugging information is stored in
   a completely different format.  ECOFF targets use some of the
   swapping routines from coffswap.h, and some of the generic COFF
   routines in coffgen.c, but, unlike the real COFF targets, do not
   use coffcode.h itself.

   Get the generic COFF swapping routines, except for the reloc,
   symbol, and lineno ones.  Give them ECOFF names.  */
#define MIPSECOFF
#define NO_COFF_RELOCS
#define NO_COFF_SYMBOLS
#define NO_COFF_LINENOS
#define coff_swap_filehdr_in mips_ecoff_swap_filehdr_in
#define coff_swap_filehdr_out mips_ecoff_swap_filehdr_out
#define coff_swap_aouthdr_in mips_ecoff_swap_aouthdr_in
#define coff_swap_aouthdr_out mips_ecoff_swap_aouthdr_out
#define coff_swap_scnhdr_in mips_ecoff_swap_scnhdr_in
#define coff_swap_scnhdr_out mips_ecoff_swap_scnhdr_out
#include "coffswap.h"

/* Get the ECOFF swapping routines.  */
#define ECOFF_32
#include "ecoffswap.h"

/* How to process the various relocs types.  */

static reloc_howto_type mips_howto_table[] =
{
  /* Reloc type 0 is ignored.  The reloc reading code ensures that
     this is a reference to the .abs section, which will cause
     bfd_perform_relocation to do nothing.  */
  HOWTO (MIPS_R_IGNORE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "IGNORE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 16 bit reference to a symbol, normally from a data section.  */
  HOWTO (MIPS_R_REFHALF,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mips_generic_reloc,	/* special_function */
	 "REFHALF",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 32 bit reference to a symbol, normally from a data section.  */
  HOWTO (MIPS_R_REFWORD,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mips_generic_reloc,	/* special_function */
	 "REFWORD",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 26 bit absolute jump address.  */
  HOWTO (MIPS_R_JMPADDR,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 			/* This needs complex overflow
				   detection, because the upper four
				   bits must match the PC.  */
	 mips_generic_reloc,	/* special_function */
	 "JMPADDR",		/* name */
	 true,			/* partial_inplace */
	 0x3ffffff,		/* src_mask */
	 0x3ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high 16 bits of a symbol value.  Handled by the function
     mips_refhi_reloc.  */
  HOWTO (MIPS_R_REFHI,		/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mips_refhi_reloc,	/* special_function */
	 "REFHI",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The low 16 bits of a symbol value.  */
  HOWTO (MIPS_R_REFLO,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_reflo_reloc,	/* special_function */
	 "REFLO",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A reference to an offset from the gp register.  Handled by the
     function mips_gprel_reloc.  */
  HOWTO (MIPS_R_GPREL,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_gprel_reloc,	/* special_function */
	 "GPREL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A reference to a literal using an offset from the gp register.
     Handled by the function mips_gprel_reloc.  */
  HOWTO (MIPS_R_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_gprel_reloc,	/* special_function */
	 "LITERAL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (8),
  EMPTY_HOWTO (9),
  EMPTY_HOWTO (10),
  EMPTY_HOWTO (11),

  /* This reloc is a Cygnus extension used when generating position
     independent code for embedded systems.  It represents a 16 bit PC
     relative reloc rightshifted twice as used in the MIPS branch
     instructions.  */
  HOWTO (MIPS_R_PCREL16,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_generic_reloc,	/* special_function */
	 "PCREL16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* This reloc is a Cygnus extension used when generating position
     independent code for embedded systems.  It represents the high 16
     bits of a PC relative reloc.  The next reloc must be
     MIPS_R_RELLO, and the addend is formed from the addends of the
     two instructions, just as in MIPS_R_REFHI and MIPS_R_REFLO.  The
     final value is actually PC relative to the location of the
     MIPS_R_RELLO reloc, not the MIPS_R_RELHI reloc.  */
  HOWTO (MIPS_R_RELHI,		/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mips_relhi_reloc,	/* special_function */
	 "RELHI",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* This reloc is a Cygnus extension used when generating position
     independent code for embedded systems.  It represents the low 16
     bits of a PC relative reloc.  */
  HOWTO (MIPS_R_RELLO,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_rello_reloc,	/* special_function */
	 "RELLO",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  EMPTY_HOWTO (15),
  EMPTY_HOWTO (16),
  EMPTY_HOWTO (17),
  EMPTY_HOWTO (18),
  EMPTY_HOWTO (19),
  EMPTY_HOWTO (20),
  EMPTY_HOWTO (21),

  /* This reloc is a Cygnus extension used when generating position
     independent code for embedded systems.  It represents an entry in
     a switch table, which is the difference between two symbols in
     the .text section.  The symndx is actually the offset from the
     reloc address to the subtrahend.  See include/coff/mips.h for
     more details.  */
  HOWTO (MIPS_R_SWITCH,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_switch_reloc,	/* special_function */
	 "SWITCH",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 true)			/* pcrel_offset */
};

#define MIPS_HOWTO_COUNT \
  (sizeof mips_howto_table / sizeof mips_howto_table[0])

/* When the linker is doing relaxing, it may change a external PCREL16
   reloc.  This typically represents an instruction like
       bal foo
   We change it to
       .set  noreorder
       bal   $L1
       lui   $at,%hi(foo - $L1)
     $L1:
       addiu $at,%lo(foo - $L1)
       addu  $at,$at,$31
       jalr  $at
   PCREL16_EXPANSION_ADJUSTMENT is the number of bytes this changes the
   instruction by.  */

#define PCREL16_EXPANSION_ADJUSTMENT (4 * 4)

/* See whether the magic number matches.  */

static boolean
mips_ecoff_bad_format_hook (abfd, filehdr)
     bfd *abfd;
     PTR filehdr;
{
  struct internal_filehdr *internal_f = (struct internal_filehdr *) filehdr;

  switch (internal_f->f_magic)
    {
    case MIPS_MAGIC_1:
      /* I don't know what endianness this implies.  */
      return true;

    case MIPS_MAGIC_BIG:
    case MIPS_MAGIC_BIG2:
    case MIPS_MAGIC_BIG3:
      return bfd_big_endian (abfd);

    case MIPS_MAGIC_LITTLE:
    case MIPS_MAGIC_LITTLE2:
    case MIPS_MAGIC_LITTLE3:
      return bfd_little_endian (abfd);

    default:
      return false;
    }
}

/* Reloc handling.  MIPS ECOFF relocs are packed into 8 bytes in
   external form.  They use a bit which indicates whether the symbol
   is external.  */

/* Swap a reloc in.  */

static void
mips_ecoff_swap_reloc_in (abfd, ext_ptr, intern)
     bfd *abfd;
     PTR ext_ptr;
     struct internal_reloc *intern;
{
  const RELOC *ext = (RELOC *) ext_ptr;

  intern->r_vaddr = bfd_h_get_32 (abfd, (bfd_byte *) ext->r_vaddr);
  if (bfd_header_big_endian (abfd))
    {
      intern->r_symndx = (((int) ext->r_bits[0]
			   << RELOC_BITS0_SYMNDX_SH_LEFT_BIG)
			  | ((int) ext->r_bits[1]
			     << RELOC_BITS1_SYMNDX_SH_LEFT_BIG)
			  | ((int) ext->r_bits[2]
			     << RELOC_BITS2_SYMNDX_SH_LEFT_BIG));
      intern->r_type = ((ext->r_bits[3] & RELOC_BITS3_TYPE_BIG)
			>> RELOC_BITS3_TYPE_SH_BIG);
      intern->r_extern = (ext->r_bits[3] & RELOC_BITS3_EXTERN_BIG) != 0;
    }
  else
    {
      intern->r_symndx = (((int) ext->r_bits[0]
			   << RELOC_BITS0_SYMNDX_SH_LEFT_LITTLE)
			  | ((int) ext->r_bits[1]
			     << RELOC_BITS1_SYMNDX_SH_LEFT_LITTLE)
			  | ((int) ext->r_bits[2]
			     << RELOC_BITS2_SYMNDX_SH_LEFT_LITTLE));
      intern->r_type = (((ext->r_bits[3] & RELOC_BITS3_TYPE_LITTLE)
			 >> RELOC_BITS3_TYPE_SH_LITTLE)
			| ((ext->r_bits[3] & RELOC_BITS3_TYPEHI_LITTLE)
			   << RELOC_BITS3_TYPEHI_SH_LITTLE));
      intern->r_extern = (ext->r_bits[3] & RELOC_BITS3_EXTERN_LITTLE) != 0;
    }

  /* If this is a MIPS_R_SWITCH reloc, or an internal MIPS_R_RELHI or
     MIPS_R_RELLO reloc, r_symndx is actually the offset from the
     reloc address to the base of the difference (see
     include/coff/mips.h for more details).  We copy symndx into the
     r_offset field so as not to confuse ecoff_slurp_reloc_table in
     ecoff.c.  In adjust_reloc_in we then copy r_offset into the reloc
     addend.  */
  if (intern->r_type == MIPS_R_SWITCH
      || (! intern->r_extern
	  && (intern->r_type == MIPS_R_RELLO
	      || intern->r_type == MIPS_R_RELHI)))
    {
      BFD_ASSERT (! intern->r_extern);
      intern->r_offset = intern->r_symndx;
      if (intern->r_offset & 0x800000)
	intern->r_offset -= 0x1000000;
      intern->r_symndx = RELOC_SECTION_TEXT;
    }
}

/* Swap a reloc out.  */

static void
mips_ecoff_swap_reloc_out (abfd, intern, dst)
     bfd *abfd;
     const struct internal_reloc *intern;
     PTR dst;
{
  RELOC *ext = (RELOC *) dst;
  long r_symndx;

  BFD_ASSERT (intern->r_extern
	      || (intern->r_symndx >= 0 && intern->r_symndx <= 12));

  /* If this is a MIPS_R_SWITCH reloc, or an internal MIPS_R_RELLO or
     MIPS_R_RELHI reloc, we actually want to write the contents of
     r_offset out as the symbol index.  This undoes the change made by
     mips_ecoff_swap_reloc_in.  */
  if (intern->r_type != MIPS_R_SWITCH
      && (intern->r_extern
	  || (intern->r_type != MIPS_R_RELHI
	      && intern->r_type != MIPS_R_RELLO)))
    r_symndx = intern->r_symndx;
  else
    {
      BFD_ASSERT (intern->r_symndx == RELOC_SECTION_TEXT);
      r_symndx = intern->r_offset & 0xffffff;
    }

  bfd_h_put_32 (abfd, intern->r_vaddr, (bfd_byte *) ext->r_vaddr);
  if (bfd_header_big_endian (abfd))
    {
      ext->r_bits[0] = r_symndx >> RELOC_BITS0_SYMNDX_SH_LEFT_BIG;
      ext->r_bits[1] = r_symndx >> RELOC_BITS1_SYMNDX_SH_LEFT_BIG;
      ext->r_bits[2] = r_symndx >> RELOC_BITS2_SYMNDX_SH_LEFT_BIG;
      ext->r_bits[3] = (((intern->r_type << RELOC_BITS3_TYPE_SH_BIG)
			 & RELOC_BITS3_TYPE_BIG)
			| (intern->r_extern ? RELOC_BITS3_EXTERN_BIG : 0));
    }
  else
    {
      ext->r_bits[0] = r_symndx >> RELOC_BITS0_SYMNDX_SH_LEFT_LITTLE;
      ext->r_bits[1] = r_symndx >> RELOC_BITS1_SYMNDX_SH_LEFT_LITTLE;
      ext->r_bits[2] = r_symndx >> RELOC_BITS2_SYMNDX_SH_LEFT_LITTLE;
      ext->r_bits[3] = (((intern->r_type << RELOC_BITS3_TYPE_SH_LITTLE)
			 & RELOC_BITS3_TYPE_LITTLE)
			| ((intern->r_type >> RELOC_BITS3_TYPEHI_SH_LITTLE
			    & RELOC_BITS3_TYPEHI_LITTLE))
			| (intern->r_extern ? RELOC_BITS3_EXTERN_LITTLE : 0));
    }
}

/* Finish canonicalizing a reloc.  Part of this is generic to all
   ECOFF targets, and that part is in ecoff.c.  The rest is done in
   this backend routine.  It must fill in the howto field.  */

static void
mips_adjust_reloc_in (abfd, intern, rptr)
     bfd *abfd;
     const struct internal_reloc *intern;
     arelent *rptr;
{
  if (intern->r_type > MIPS_R_SWITCH)
    abort ();

  if (! intern->r_extern
      && (intern->r_type == MIPS_R_GPREL
	  || intern->r_type == MIPS_R_LITERAL))
    rptr->addend += ecoff_data (abfd)->gp;

  /* If the type is MIPS_R_IGNORE, make sure this is a reference to
     the absolute section so that the reloc is ignored.  */
  if (intern->r_type == MIPS_R_IGNORE)
    rptr->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;

  /* If this is a MIPS_R_SWITCH reloc, or an internal MIPS_R_RELHI or
     MIPS_R_RELLO reloc, we want the addend field of the BFD relocto
     hold the value which was originally in the symndx field of the
     internal MIPS ECOFF reloc.  This value was copied into
     intern->r_offset by mips_swap_reloc_in, and here we copy it into
     the addend field.  */
  if (intern->r_type == MIPS_R_SWITCH
      || (! intern->r_extern
	  && (intern->r_type == MIPS_R_RELHI
	      || intern->r_type == MIPS_R_RELLO)))
    rptr->addend = intern->r_offset;

  rptr->howto = &mips_howto_table[intern->r_type];
}

/* Make any adjustments needed to a reloc before writing it out.  None
   are needed for MIPS.  */

static void
mips_adjust_reloc_out (abfd, rel, intern)
     bfd *abfd ATTRIBUTE_UNUSED;
     const arelent *rel;
     struct internal_reloc *intern;
{
  /* For a MIPS_R_SWITCH reloc, or an internal MIPS_R_RELHI or
     MIPS_R_RELLO reloc, we must copy rel->addend into
     intern->r_offset.  This will then be written out as the symbol
     index by mips_ecoff_swap_reloc_out.  This operation parallels the
     action of mips_adjust_reloc_in.  */
  if (intern->r_type == MIPS_R_SWITCH
      || (! intern->r_extern
	  && (intern->r_type == MIPS_R_RELHI
	      || intern->r_type == MIPS_R_RELLO)))
    intern->r_offset = rel->addend;
}

/* ECOFF relocs are either against external symbols, or against
   sections.  If we are producing relocateable output, and the reloc
   is against an external symbol, and nothing has given us any
   additional addend, the resulting reloc will also be against the
   same symbol.  In such a case, we don't want to change anything
   about the way the reloc is handled, since it will all be done at
   final link time.  Rather than put special case code into
   bfd_perform_relocation, all the reloc types use this howto
   function.  It just short circuits the reloc if producing
   relocateable output against an external symbol.  */

static bfd_reloc_status_type
mips_generic_reloc (abfd,
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
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  return bfd_reloc_continue;
}

/* Do a REFHI relocation.  This has to be done in combination with a
   REFLO reloc, because there is a carry from the REFLO to the REFHI.
   Here we just save the information we need; we do the actual
   relocation when we see the REFLO.  MIPS ECOFF requires that the
   REFLO immediately follow the REFHI.  As a GNU extension, we permit
   an arbitrary number of HI relocs to be associated with a single LO
   reloc.  This extension permits gcc to output the HI and LO relocs
   itself.  */

struct mips_hi
{
  struct mips_hi *next;
  bfd_byte *addr;
  bfd_vma addend;
};

/* FIXME: This should not be a static variable.  */

static struct mips_hi *mips_refhi_list;

static bfd_reloc_status_type
mips_refhi_reloc (abfd,
		  reloc_entry,
		  symbol,
		  data,
		  input_section,
		  output_bfd,
		  error_message)
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
  struct mips_hi *n;

  /* If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

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

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Save the information, and let REFLO do the actual relocation.  */
  n = (struct mips_hi *) bfd_malloc (sizeof *n);
  if (n == NULL)
    return bfd_reloc_outofrange;
  n->addr = (bfd_byte *) data + reloc_entry->address;
  n->addend = relocation;
  n->next = mips_refhi_list;
  mips_refhi_list = n;

  if (output_bfd != (bfd *) NULL)
    reloc_entry->address += input_section->output_offset;

  return ret;
}

/* Do a REFLO relocation.  This is a straightforward 16 bit inplace
   relocation; this function exists in order to do the REFHI
   relocation described above.  */

static bfd_reloc_status_type
mips_reflo_reloc (abfd,
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
  if (mips_refhi_list != NULL)
    {
      struct mips_hi *l;

      l = mips_refhi_list;
      while (l != NULL)
	{
	  unsigned long insn;
	  unsigned long val;
	  unsigned long vallo;
	  struct mips_hi *next;

	  /* Do the REFHI relocation.  Note that we actually don't
	     need to know anything about the REFLO itself, except
	     where to find the low 16 bits of the addend needed by the
	     REFHI.  */
	  insn = bfd_get_32 (abfd, l->addr);
	  vallo = (bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address)
		   & 0xffff);
	  val = ((insn & 0xffff) << 16) + vallo;
	  val += l->addend;

	  /* The low order 16 bits are always treated as a signed
	     value.  Therefore, a negative value in the low order bits
	     requires an adjustment in the high order bits.  We need
	     to make this adjustment in two ways: once for the bits we
	     took from the data, and once for the bits we are putting
	     back in to the data.  */
	  if ((vallo & 0x8000) != 0)
	    val -= 0x10000;
	  if ((val & 0x8000) != 0)
	    val += 0x10000;

	  insn = (insn &~ 0xffff) | ((val >> 16) & 0xffff);
	  bfd_put_32 (abfd, insn, l->addr);

	  next = l->next;
	  free (l);
	  l = next;
	}

      mips_refhi_list = NULL;
    }

  /* Now do the REFLO reloc in the usual way.  */
  return mips_generic_reloc (abfd, reloc_entry, symbol, data,
			      input_section, output_bfd, error_message);
}

/* Do a GPREL relocation.  This is a 16 bit value which must become
   the offset from the gp register.  */

static bfd_reloc_status_type
mips_gprel_reloc (abfd,
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
  bfd_vma gp;
  bfd_vma relocation;
  unsigned long val;
  unsigned long insn;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ECOFF
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

  if (bfd_is_und_section (symbol->section)
      && relocateable == false)
    return bfd_reloc_undefined;

  /* We have to figure out the gp value, so that we can adjust the
     symbol value correctly.  We look up the symbol _gp in the output
     BFD.  If we can't find it, we're stuck.  We cache it in the ECOFF
     target data.  We don't need to adjust the symbol value for an
     external symbol if we are producing relocateable output.  */
  gp = _bfd_get_gp_value (output_bfd);
  if (gp == 0
      && (relocateable == false
	  || (symbol->flags & BSF_SECTION_SYM) != 0))
    {
      if (relocateable != false)
	{
	  /* Make up a value.  */
	  gp = symbol->section->output_section->vma + 0x4000;
	  _bfd_set_gp_value (output_bfd, gp);
	}
      else
	{
	  unsigned int count;
	  asymbol **sym;
	  unsigned int i;

	  count = bfd_get_symcount (output_bfd);
	  sym = bfd_get_outsymbols (output_bfd);

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
		      gp = bfd_asymbol_value (*sym);
		      _bfd_set_gp_value (output_bfd, gp);
		      break;
		    }
		}
	    }

	  if (i >= count)
	    {
	      /* Only get the error once.  */
	      gp = 4;
	      _bfd_set_gp_value (output_bfd, gp);
	      *error_message =
		(char *) _("GP relative relocation when _gp not defined");
	      return bfd_reloc_dangerous;
	    }
	}
    }

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
  val = ((insn & 0xffff) + reloc_entry->addend) & 0xffff;
  if (val & 0x8000)
    val -= 0x10000;

  /* Adjust val for the final section location and GP value.  If we
     are producing relocateable output, we don't want to do this for
     an external symbol.  */
  if (relocateable == false
      || (symbol->flags & BSF_SECTION_SYM) != 0)
    val += relocation - gp;

  insn = (insn &~ 0xffff) | (val & 0xffff);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  if (relocateable != false)
    reloc_entry->address += input_section->output_offset;

  /* Make sure it fit in 16 bits.  */
  if (val >= 0x8000 && val < 0xffff8000)
    return bfd_reloc_overflow;

  return bfd_reloc_ok;
}

/* Do a RELHI relocation.  We do this in conjunction with a RELLO
   reloc, just as REFHI and REFLO are done together.  RELHI and RELLO
   are Cygnus extensions used when generating position independent
   code for embedded systems.  */

/* FIXME: This should not be a static variable.  */

static struct mips_hi *mips_relhi_list;

static bfd_reloc_status_type
mips_relhi_reloc (abfd,
		  reloc_entry,
		  symbol,
		  data,
		  input_section,
		  output_bfd,
		  error_message)
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
  struct mips_hi *n;

  /* If this is a reloc against a section symbol, then it is correct
     in the object file.  The only time we want to change this case is
     when we are relaxing, and that is handled entirely by
     mips_relocate_section and never calls this function.  */
  if ((symbol->flags & BSF_SECTION_SYM) != 0)
    {
      if (output_bfd != (bfd *) NULL)
	reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* This is an external symbol.  If we're relocating, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

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

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Save the information, and let RELLO do the actual relocation.  */
  n = (struct mips_hi *) bfd_malloc (sizeof *n);
  if (n == NULL)
    return bfd_reloc_outofrange;
  n->addr = (bfd_byte *) data + reloc_entry->address;
  n->addend = relocation;
  n->next = mips_relhi_list;
  mips_relhi_list = n;

  if (output_bfd != (bfd *) NULL)
    reloc_entry->address += input_section->output_offset;

  return ret;
}

/* Do a RELLO relocation.  This is a straightforward 16 bit PC
   relative relocation; this function exists in order to do the RELHI
   relocation described above.  */

static bfd_reloc_status_type
mips_rello_reloc (abfd,
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
  if (mips_relhi_list != NULL)
    {
      struct mips_hi *l;

      l = mips_relhi_list;
      while (l != NULL)
	{
	  unsigned long insn;
	  unsigned long val;
	  unsigned long vallo;
	  struct mips_hi *next;

	  /* Do the RELHI relocation.  Note that we actually don't
	     need to know anything about the RELLO itself, except
	     where to find the low 16 bits of the addend needed by the
	     RELHI.  */
	  insn = bfd_get_32 (abfd, l->addr);
	  vallo = (bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address)
		   & 0xffff);
	  val = ((insn & 0xffff) << 16) + vallo;
	  val += l->addend;

	  /* If the symbol is defined, make val PC relative.  If the
	     symbol is not defined we don't want to do this, because
	     we don't want the value in the object file to incorporate
	     the address of the reloc.  */
	  if (! bfd_is_und_section (bfd_get_section (symbol))
	      && ! bfd_is_com_section (bfd_get_section (symbol)))
	    val -= (input_section->output_section->vma
		    + input_section->output_offset
		    + reloc_entry->address);

	  /* The low order 16 bits are always treated as a signed
	     value.  Therefore, a negative value in the low order bits
	     requires an adjustment in the high order bits.  We need
	     to make this adjustment in two ways: once for the bits we
	     took from the data, and once for the bits we are putting
	     back in to the data.  */
	  if ((vallo & 0x8000) != 0)
	    val -= 0x10000;
	  if ((val & 0x8000) != 0)
	    val += 0x10000;

	  insn = (insn &~ 0xffff) | ((val >> 16) & 0xffff);
	  bfd_put_32 (abfd, insn, l->addr);

	  next = l->next;
	  free (l);
	  l = next;
	}

      mips_relhi_list = NULL;
    }

  /* If this is a reloc against a section symbol, then it is correct
     in the object file.  The only time we want to change this case is
     when we are relaxing, and that is handled entirely by
     mips_relocate_section and never calls this function.  */
  if ((symbol->flags & BSF_SECTION_SYM) != 0)
    {
      if (output_bfd != (bfd *) NULL)
	reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* bfd_perform_relocation does not handle pcrel_offset relocations
     correctly when generating a relocateable file, so handle them
     directly here.  */
  if (output_bfd != (bfd *) NULL)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* Now do the RELLO reloc in the usual way.  */
  return mips_generic_reloc (abfd, reloc_entry, symbol, data,
			      input_section, output_bfd, error_message);
}

/* This is the special function for the MIPS_R_SWITCH reloc.  This
   special reloc is normally correct in the object file, and only
   requires special handling when relaxing.  We don't want
   bfd_perform_relocation to tamper with it at all.  */

/*ARGSUSED*/
static bfd_reloc_status_type
mips_switch_reloc (abfd,
		   reloc_entry,
		   symbol,
		   data,
		   input_section,
		   output_bfd,
		   error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry ATTRIBUTE_UNUSED;
     asymbol *symbol ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     char **error_message ATTRIBUTE_UNUSED;
{
  return bfd_reloc_ok;
}

/* Get the howto structure for a generic reloc type.  */

static reloc_howto_type *
mips_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  int mips_type;

  switch (code)
    {
    case BFD_RELOC_16:
      mips_type = MIPS_R_REFHALF;
      break;
    case BFD_RELOC_32:
    case BFD_RELOC_CTOR:
      mips_type = MIPS_R_REFWORD;
      break;
    case BFD_RELOC_MIPS_JMP:
      mips_type = MIPS_R_JMPADDR;
      break;
    case BFD_RELOC_HI16_S:
      mips_type = MIPS_R_REFHI;
      break;
    case BFD_RELOC_LO16:
      mips_type = MIPS_R_REFLO;
      break;
    case BFD_RELOC_MIPS_GPREL:
      mips_type = MIPS_R_GPREL;
      break;
    case BFD_RELOC_MIPS_LITERAL:
      mips_type = MIPS_R_LITERAL;
      break;
    case BFD_RELOC_16_PCREL_S2:
      mips_type = MIPS_R_PCREL16;
      break;
    case BFD_RELOC_PCREL_HI16_S:
      mips_type = MIPS_R_RELHI;
      break;
    case BFD_RELOC_PCREL_LO16:
      mips_type = MIPS_R_RELLO;
      break;
    case BFD_RELOC_GPREL32:
      mips_type = MIPS_R_SWITCH;
      break;
    default:
      return (reloc_howto_type *) NULL;
    }

  return &mips_howto_table[mips_type];
}

/* A helper routine for mips_relocate_section which handles the REFHI
   and RELHI relocations.  The REFHI relocation must be followed by a
   REFLO relocation (and RELHI by a RELLO), and the addend used is
   formed from the addends of both instructions.  */

static void
mips_relocate_hi (refhi, reflo, input_bfd, input_section, contents, adjust,
		  relocation, pcrel)
     struct internal_reloc *refhi;
     struct internal_reloc *reflo;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     size_t adjust;
     bfd_vma relocation;
     boolean pcrel;
{
  unsigned long insn;
  unsigned long val;
  unsigned long vallo;

  if (refhi == NULL)
    return;
  
  insn = bfd_get_32 (input_bfd,
		     contents + adjust + refhi->r_vaddr - input_section->vma);
  if (reflo == NULL)
    vallo = 0;
  else
    vallo = (bfd_get_32 (input_bfd,
			 contents + adjust + reflo->r_vaddr - input_section->vma)
	     & 0xffff);
 
  val = ((insn & 0xffff) << 16) + vallo;
  val += relocation;

  /* The low order 16 bits are always treated as a signed value.
     Therefore, a negative value in the low order bits requires an
     adjustment in the high order bits.  We need to make this
     adjustment in two ways: once for the bits we took from the data,
     and once for the bits we are putting back in to the data.  */
  if ((vallo & 0x8000) != 0)
    val -= 0x10000;

  if (pcrel)
    val -= (input_section->output_section->vma
	    + input_section->output_offset
	    + (reflo->r_vaddr - input_section->vma + adjust));

  if ((val & 0x8000) != 0)
    val += 0x10000;

  insn = (insn &~ 0xffff) | ((val >> 16) & 0xffff);
  bfd_put_32 (input_bfd, (bfd_vma) insn,
	      contents + adjust + refhi->r_vaddr - input_section->vma);
}

/* Relocate a section while linking a MIPS ECOFF file.  */

static boolean
mips_relocate_section (output_bfd, info, input_bfd, input_section,
		       contents, external_relocs)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     PTR external_relocs;
{
  asection **symndx_to_section;
  struct ecoff_link_hash_entry **sym_hashes;
  bfd_vma gp;
  boolean gp_undefined;
  size_t adjust;
  long *offsets;
  struct external_reloc *ext_rel;
  struct external_reloc *ext_rel_end;
  unsigned int i;
  boolean got_lo;
  struct internal_reloc lo_int_rel;

  BFD_ASSERT (input_bfd->xvec->byteorder
	      == output_bfd->xvec->byteorder);

  /* We keep a table mapping the symndx found in an internal reloc to
     the appropriate section.  This is faster than looking up the
     section by name each time.  */
  symndx_to_section = ecoff_data (input_bfd)->symndx_to_section;
  if (symndx_to_section == (asection **) NULL)
    {
      symndx_to_section = ((asection **)
			   bfd_alloc (input_bfd,
				      (NUM_RELOC_SECTIONS
				       * sizeof (asection *))));
      if (!symndx_to_section)
	return false;

      symndx_to_section[RELOC_SECTION_NONE] = NULL;
      symndx_to_section[RELOC_SECTION_TEXT] =
	bfd_get_section_by_name (input_bfd, ".text");
      symndx_to_section[RELOC_SECTION_RDATA] =
	bfd_get_section_by_name (input_bfd, ".rdata");
      symndx_to_section[RELOC_SECTION_DATA] =
	bfd_get_section_by_name (input_bfd, ".data");
      symndx_to_section[RELOC_SECTION_SDATA] =
	bfd_get_section_by_name (input_bfd, ".sdata");
      symndx_to_section[RELOC_SECTION_SBSS] =
	bfd_get_section_by_name (input_bfd, ".sbss");
      symndx_to_section[RELOC_SECTION_BSS] =
	bfd_get_section_by_name (input_bfd, ".bss");
      symndx_to_section[RELOC_SECTION_INIT] =
	bfd_get_section_by_name (input_bfd, ".init");
      symndx_to_section[RELOC_SECTION_LIT8] =
	bfd_get_section_by_name (input_bfd, ".lit8");
      symndx_to_section[RELOC_SECTION_LIT4] =
	bfd_get_section_by_name (input_bfd, ".lit4");
      symndx_to_section[RELOC_SECTION_XDATA] = NULL;
      symndx_to_section[RELOC_SECTION_PDATA] = NULL;
      symndx_to_section[RELOC_SECTION_FINI] =
	bfd_get_section_by_name (input_bfd, ".fini");
      symndx_to_section[RELOC_SECTION_LITA] = NULL;
      symndx_to_section[RELOC_SECTION_ABS] = NULL;

      ecoff_data (input_bfd)->symndx_to_section = symndx_to_section;
    }

  sym_hashes = ecoff_data (input_bfd)->sym_hashes;

  gp = _bfd_get_gp_value (output_bfd);
  if (gp == 0)
    gp_undefined = true;
  else
    gp_undefined = false;

  got_lo = false;

  adjust = 0;

  if (ecoff_section_data (input_bfd, input_section) == NULL)
    offsets = NULL;
  else
    offsets = ecoff_section_data (input_bfd, input_section)->offsets;

  ext_rel = (struct external_reloc *) external_relocs;
  ext_rel_end = ext_rel + input_section->reloc_count;
  for (i = 0; ext_rel < ext_rel_end; ext_rel++, i++)
    {
      struct internal_reloc int_rel;
      boolean use_lo = false;
      bfd_vma addend;
      reloc_howto_type *howto;
      struct ecoff_link_hash_entry *h = NULL;
      asection *s = NULL;
      bfd_vma relocation;
      bfd_reloc_status_type r;

      if (! got_lo)
	mips_ecoff_swap_reloc_in (input_bfd, (PTR) ext_rel, &int_rel);
      else
	{
	  int_rel = lo_int_rel;
	  got_lo = false;
	}

      BFD_ASSERT (int_rel.r_type
		  < sizeof mips_howto_table / sizeof mips_howto_table[0]);

      /* The REFHI and RELHI relocs requires special handling.  they
	 must be followed by a REFLO or RELLO reloc, respectively, and
	 the addend is formed from both relocs.  */
      if (int_rel.r_type == MIPS_R_REFHI
	  || int_rel.r_type == MIPS_R_RELHI)
	{
	  struct external_reloc *lo_ext_rel;

	  /* As a GNU extension, permit an arbitrary number of REFHI
             or RELHI relocs before the REFLO or RELLO reloc.  This
             permits gcc to emit the HI and LO relocs itself.  */
	  for (lo_ext_rel = ext_rel + 1;
	       lo_ext_rel < ext_rel_end;
	       lo_ext_rel++)
	    {
	      mips_ecoff_swap_reloc_in (input_bfd, (PTR) lo_ext_rel,
					&lo_int_rel);
	      if (lo_int_rel.r_type != int_rel.r_type)
		break;
	    }

	  if (lo_ext_rel < ext_rel_end
	      && (lo_int_rel.r_type
		  == (int_rel.r_type == MIPS_R_REFHI
		      ? MIPS_R_REFLO
		      : MIPS_R_RELLO))
	      && int_rel.r_extern == lo_int_rel.r_extern
	      && int_rel.r_symndx == lo_int_rel.r_symndx)
	    {
	      use_lo = true;
	      if (lo_ext_rel == ext_rel + 1)
		got_lo = true;
	    }
	}

      howto = &mips_howto_table[int_rel.r_type];

      /* The SWITCH reloc must be handled specially.  This reloc is
	 marks the location of a difference between two portions of an
	 object file.  The symbol index does not reference a symbol,
	 but is actually the offset from the reloc to the subtrahend
	 of the difference.  This reloc is correct in the object file,
	 and needs no further adjustment, unless we are relaxing.  If
	 we are relaxing, we may have to add in an offset.  Since no
	 symbols are involved in this reloc, we handle it completely
	 here.  */
      if (int_rel.r_type == MIPS_R_SWITCH)
	{
	  if (offsets != NULL
	      && offsets[i] != 0)
	    {
	      r = _bfd_relocate_contents (howto, input_bfd,
					  (bfd_vma) offsets[i],
					  (contents
					   + adjust
					   + int_rel.r_vaddr
					   - input_section->vma));
	      BFD_ASSERT (r == bfd_reloc_ok);
	    }

	  continue;
	}

      if (int_rel.r_extern)
	{
	  h = sym_hashes[int_rel.r_symndx];
	  /* If h is NULL, that means that there is a reloc against an
	     external symbol which we thought was just a debugging
	     symbol.  This should not happen.  */
	  if (h == (struct ecoff_link_hash_entry *) NULL)
	    abort ();
	}
      else
	{
	  if (int_rel.r_symndx < 0 || int_rel.r_symndx >= NUM_RELOC_SECTIONS)
	    s = NULL;
	  else
	    s = symndx_to_section[int_rel.r_symndx];

	  if (s == (asection *) NULL)
	    abort ();
	}

      /* The GPREL reloc uses an addend: the difference in the GP
	 values.  */
      if (int_rel.r_type != MIPS_R_GPREL
	  && int_rel.r_type != MIPS_R_LITERAL)
	addend = 0;
      else
	{
	  if (gp_undefined)
	    {
	      if (! ((*info->callbacks->reloc_dangerous)
		     (info, _("GP relative relocation when GP not defined"),
		      input_bfd, input_section,
		      int_rel.r_vaddr - input_section->vma)))
		return false;
	      /* Only give the error once per link.  */
	      gp = 4;
	      _bfd_set_gp_value (output_bfd, gp);
	      gp_undefined = false;
	    }
	  if (! int_rel.r_extern)
	    {
	      /* This is a relocation against a section.  The current
		 addend in the instruction is the difference between
		 INPUT_SECTION->vma and the GP value of INPUT_BFD.  We
		 must change this to be the difference between the
		 final definition (which will end up in RELOCATION)
		 and the GP value of OUTPUT_BFD (which is in GP).  */
	      addend = ecoff_data (input_bfd)->gp - gp;
	    }
	  else if (! info->relocateable
		   || h->root.type == bfd_link_hash_defined
		   || h->root.type == bfd_link_hash_defweak)
	    {
	      /* This is a relocation against a defined symbol.  The
		 current addend in the instruction is simply the
		 desired offset into the symbol (normally zero).  We
		 are going to change this into a relocation against a
		 defined symbol, so we want the instruction to hold
		 the difference between the final definition of the
		 symbol (which will end up in RELOCATION) and the GP
		 value of OUTPUT_BFD (which is in GP).  */
	      addend = - gp;
	    }
	  else
	    {
	      /* This is a relocation against an undefined or common
		 symbol.  The current addend in the instruction is
		 simply the desired offset into the symbol (normally
		 zero).  We are generating relocateable output, and we
		 aren't going to define this symbol, so we just leave
		 the instruction alone.  */
	      addend = 0;
	    }
	}

      /* If we are relaxing, mips_relax_section may have set
	 offsets[i] to some value.  A value of 1 means we must expand
	 a PC relative branch into a multi-instruction of sequence,
	 and any other value is an addend.  */
      if (offsets != NULL
	  && offsets[i] != 0)
	{
	  BFD_ASSERT (! info->relocateable);
	  BFD_ASSERT (int_rel.r_type == MIPS_R_PCREL16
		      || int_rel.r_type == MIPS_R_RELHI
		      || int_rel.r_type == MIPS_R_RELLO);
	  if (offsets[i] != 1)
	    addend += offsets[i];
	  else
	    {
	      bfd_byte *here;

	      BFD_ASSERT (int_rel.r_extern
			  && int_rel.r_type == MIPS_R_PCREL16);

	      /* Move the rest of the instructions up.  */
	      here = (contents
		      + adjust
		      + int_rel.r_vaddr
		      - input_section->vma);
	      memmove (here + PCREL16_EXPANSION_ADJUSTMENT, here,
		       (size_t) (input_section->_raw_size
				 - (int_rel.r_vaddr - input_section->vma)));
		       
	      /* Generate the new instructions.  */
	      if (! mips_relax_pcrel16 (info, input_bfd, input_section,
					h, here,
					(input_section->output_section->vma
					 + input_section->output_offset
					 + (int_rel.r_vaddr
					    - input_section->vma)
					 + adjust)))
		return false;

	      /* We must adjust everything else up a notch.  */
	      adjust += PCREL16_EXPANSION_ADJUSTMENT;

	      /* mips_relax_pcrel16 handles all the details of this
		 relocation.  */
	      continue;
	    }
	}

      /* If we are relaxing, and this is a reloc against the .text
	 segment, we may need to adjust it if some branches have been
	 expanded.  The reloc types which are likely to occur in the
	 .text section are handled efficiently by mips_relax_section,
	 and thus do not need to be handled here.  */
      if (ecoff_data (input_bfd)->debug_info.adjust != NULL
	  && ! int_rel.r_extern
	  && int_rel.r_symndx == RELOC_SECTION_TEXT
	  && (strcmp (bfd_get_section_name (input_bfd, input_section),
		      ".text") != 0
	      || (int_rel.r_type != MIPS_R_PCREL16
		  && int_rel.r_type != MIPS_R_SWITCH
		  && int_rel.r_type != MIPS_R_RELHI
		  && int_rel.r_type != MIPS_R_RELLO)))
	{
	  bfd_vma adr;
	  struct ecoff_value_adjust *a;

	  /* We need to get the addend so that we know whether we need
	     to adjust the address.  */
	  BFD_ASSERT (int_rel.r_type == MIPS_R_REFWORD);

	  adr = bfd_get_32 (input_bfd,
			    (contents
			     + adjust
			     + int_rel.r_vaddr
			     - input_section->vma));

	  for (a = ecoff_data (input_bfd)->debug_info.adjust;
	       a != (struct ecoff_value_adjust *) NULL;
	       a = a->next)
	    {
	      if (adr >= a->start && adr < a->end)
		addend += a->adjust;
	    }
	}

      if (info->relocateable)
	{
	  /* We are generating relocateable output, and must convert
	     the existing reloc.  */
	  if (int_rel.r_extern)
	    {
	      if ((h->root.type == bfd_link_hash_defined
		   || h->root.type == bfd_link_hash_defweak)
		  && ! bfd_is_abs_section (h->root.u.def.section))
		{
		  const char *name;

		  /* This symbol is defined in the output.  Convert
		     the reloc from being against the symbol to being
		     against the section.  */

		  /* Clear the r_extern bit.  */
		  int_rel.r_extern = 0;

		  /* Compute a new r_symndx value.  */
		  s = h->root.u.def.section;
		  name = bfd_get_section_name (output_bfd,
					       s->output_section);

		  int_rel.r_symndx = -1;
		  switch (name[1])
		    {
		    case 'b':
		      if (strcmp (name, ".bss") == 0)
			int_rel.r_symndx = RELOC_SECTION_BSS;
		      break;
		    case 'd':
		      if (strcmp (name, ".data") == 0)
			int_rel.r_symndx = RELOC_SECTION_DATA;
		      break;
		    case 'f':
		      if (strcmp (name, ".fini") == 0)
			int_rel.r_symndx = RELOC_SECTION_FINI;
		      break;
		    case 'i':
		      if (strcmp (name, ".init") == 0)
			int_rel.r_symndx = RELOC_SECTION_INIT;
		      break;
		    case 'l':
		      if (strcmp (name, ".lit8") == 0)
			int_rel.r_symndx = RELOC_SECTION_LIT8;
		      else if (strcmp (name, ".lit4") == 0)
			int_rel.r_symndx = RELOC_SECTION_LIT4;
		      break;
		    case 'r':
		      if (strcmp (name, ".rdata") == 0)
			int_rel.r_symndx = RELOC_SECTION_RDATA;
		      break;
		    case 's':
		      if (strcmp (name, ".sdata") == 0)
			int_rel.r_symndx = RELOC_SECTION_SDATA;
		      else if (strcmp (name, ".sbss") == 0)
			int_rel.r_symndx = RELOC_SECTION_SBSS;
		      break;
		    case 't':
		      if (strcmp (name, ".text") == 0)
			int_rel.r_symndx = RELOC_SECTION_TEXT;
		      break;
		    }
		      
		  if (int_rel.r_symndx == -1)
		    abort ();

		  /* Add the section VMA and the symbol value.  */
		  relocation = (h->root.u.def.value
				+ s->output_section->vma
				+ s->output_offset);

		  /* For a PC relative relocation, the object file
		     currently holds just the addend.  We must adjust
		     by the address to get the right value.  */
		  if (howto->pc_relative)
		    {
		      relocation -= int_rel.r_vaddr - input_section->vma;

		      /* If we are converting a RELHI or RELLO reloc
			 from being against an external symbol to
			 being against a section, we must put a
			 special value into the r_offset field.  This
			 value is the old addend.  The r_offset for
			 both the RELHI and RELLO relocs are the same,
			 and we set both when we see RELHI.  */
		      if (int_rel.r_type == MIPS_R_RELHI)
			{
			  long addhi, addlo;

			  addhi = bfd_get_32 (input_bfd,
					      (contents
					       + adjust
					       + int_rel.r_vaddr
					       - input_section->vma));
			  addhi &= 0xffff;
			  if (addhi & 0x8000)
			    addhi -= 0x10000;
			  addhi <<= 16;

			  if (! use_lo)
			    addlo = 0;
			  else
			    {
			      addlo = bfd_get_32 (input_bfd,
						  (contents
						   + adjust
						   + lo_int_rel.r_vaddr
						   - input_section->vma));
			      addlo &= 0xffff;
			      if (addlo & 0x8000)
				addlo -= 0x10000;

			      lo_int_rel.r_offset = addhi + addlo;
			    }

			  int_rel.r_offset = addhi + addlo;
			}
		    }

		  h = NULL;
		}
	      else
		{
		  /* Change the symndx value to the right one for the
		     output BFD.  */
		  int_rel.r_symndx = h->indx;
		  if (int_rel.r_symndx == -1)
		    {
		      /* This symbol is not being written out.  */
		      if (! ((*info->callbacks->unattached_reloc)
			     (info, h->root.root.string, input_bfd,
			      input_section,
			      int_rel.r_vaddr - input_section->vma)))
			return false;
		      int_rel.r_symndx = 0;
		    }
		  relocation = 0;
		}
	    }
	  else
	    {
	      /* This is a relocation against a section.  Adjust the
		 value by the amount the section moved.  */
	      relocation = (s->output_section->vma
			    + s->output_offset
			    - s->vma);
	    }

	  relocation += addend;
	  addend = 0;

	  /* Adjust a PC relative relocation by removing the reference
	     to the original address in the section and including the
	     reference to the new address.  However, external RELHI
	     and RELLO relocs are PC relative, but don't include any
	     reference to the address.  The addend is merely an
	     addend.  */
	  if (howto->pc_relative
	      && (! int_rel.r_extern
		  || (int_rel.r_type != MIPS_R_RELHI
		      && int_rel.r_type != MIPS_R_RELLO)))
	    relocation -= (input_section->output_section->vma
			   + input_section->output_offset
			   - input_section->vma);

	  /* Adjust the contents.  */
	  if (relocation == 0)
	    r = bfd_reloc_ok;
	  else
	    {
	      if (int_rel.r_type != MIPS_R_REFHI
		  && int_rel.r_type != MIPS_R_RELHI)
		r = _bfd_relocate_contents (howto, input_bfd, relocation,
					    (contents
					     + adjust
					     + int_rel.r_vaddr
					     - input_section->vma));
	      else
		{
		  mips_relocate_hi (&int_rel,
				    use_lo ? &lo_int_rel : NULL,
				    input_bfd, input_section, contents,
				    adjust, relocation,
				    int_rel.r_type == MIPS_R_RELHI);
		  r = bfd_reloc_ok;
		}
	    }

	  /* Adjust the reloc address.  */
	  int_rel.r_vaddr += (input_section->output_section->vma
			      + input_section->output_offset
			      - input_section->vma);

	  /* Save the changed reloc information.  */
	  mips_ecoff_swap_reloc_out (input_bfd, &int_rel, (PTR) ext_rel);
	}
      else
	{
	  /* We are producing a final executable.  */
	  if (int_rel.r_extern)
	    {
	      /* This is a reloc against a symbol.  */
	      if (h->root.type == bfd_link_hash_defined
		  || h->root.type == bfd_link_hash_defweak)
		{
		  asection *hsec;

		  hsec = h->root.u.def.section;
		  relocation = (h->root.u.def.value
				+ hsec->output_section->vma
				+ hsec->output_offset);
		}
	      else
		{
		  if (! ((*info->callbacks->undefined_symbol)
			 (info, h->root.root.string, input_bfd,
			  input_section,
			  int_rel.r_vaddr - input_section->vma, true)))
		    return false;
		  relocation = 0;
		}
	    }
	  else
	    {
	      /* This is a reloc against a section.  */
	      relocation = (s->output_section->vma
			    + s->output_offset
			    - s->vma);

	      /* A PC relative reloc is already correct in the object
		 file.  Make it look like a pcrel_offset relocation by
		 adding in the start address.  */
	      if (howto->pc_relative)
		{
		  if (int_rel.r_type != MIPS_R_RELHI || ! use_lo)
		    relocation += int_rel.r_vaddr + adjust;
		  else
		    relocation += lo_int_rel.r_vaddr + adjust;
		}
	    }

	  if (int_rel.r_type != MIPS_R_REFHI
	      && int_rel.r_type != MIPS_R_RELHI)
	    r = _bfd_final_link_relocate (howto,
					  input_bfd,
					  input_section,
					  contents,
					  (int_rel.r_vaddr
					   - input_section->vma
					   + adjust),
					  relocation,
					  addend);
	  else
	    {
	      mips_relocate_hi (&int_rel,
				use_lo ? &lo_int_rel : NULL,
				input_bfd, input_section, contents, adjust,
				relocation,
				int_rel.r_type == MIPS_R_RELHI);
	      r = bfd_reloc_ok;
	    }
	}

      /* MIPS_R_JMPADDR requires peculiar overflow detection.  The
	 instruction provides a 28 bit address (the two lower bits are
	 implicit zeroes) which is combined with the upper four bits
	 of the instruction address.  */
      if (r == bfd_reloc_ok
	  && int_rel.r_type == MIPS_R_JMPADDR
	  && (((relocation
		+ addend
		+ (int_rel.r_extern ? 0 : s->vma))
	       & 0xf0000000)
	      != ((input_section->output_section->vma
		   + input_section->output_offset
		   + (int_rel.r_vaddr - input_section->vma)
		   + adjust)
		  & 0xf0000000)))
	r = bfd_reloc_overflow;

      if (r != bfd_reloc_ok)
	{
	  switch (r)
	    {
	    default:
	    case bfd_reloc_outofrange:
	      abort ();
	    case bfd_reloc_overflow:
	      {
		const char *name;

		if (int_rel.r_extern)
		  name = h->root.root.string;
		else
		  name = bfd_section_name (input_bfd, s);
		if (! ((*info->callbacks->reloc_overflow)
		       (info, name, howto->name, (bfd_vma) 0,
			input_bfd, input_section,
			int_rel.r_vaddr - input_section->vma)))
		  return false;
	      }
	      break;
	    }
	}
    }

  return true;
}

/* Read in the relocs for a section.  */

static boolean
mips_read_relocs (abfd, sec)
     bfd *abfd;
     asection *sec;
{
  struct ecoff_section_tdata *section_tdata;

  section_tdata = ecoff_section_data (abfd, sec);
  if (section_tdata == (struct ecoff_section_tdata *) NULL)
    {
      sec->used_by_bfd =
	(PTR) bfd_alloc (abfd, sizeof (struct ecoff_section_tdata));
      if (sec->used_by_bfd == NULL)
	return false;

      section_tdata = ecoff_section_data (abfd, sec);
      section_tdata->external_relocs = NULL;
      section_tdata->contents = NULL;
      section_tdata->offsets = NULL;
    }

  if (section_tdata->external_relocs == NULL)
    {
      bfd_size_type external_relocs_size;

      external_relocs_size = (ecoff_backend (abfd)->external_reloc_size
			      * sec->reloc_count);

      section_tdata->external_relocs =
	(PTR) bfd_alloc (abfd, external_relocs_size);
      if (section_tdata->external_relocs == NULL && external_relocs_size != 0)
	return false;

      if (bfd_seek (abfd, sec->rel_filepos, SEEK_SET) != 0
	  || (bfd_read (section_tdata->external_relocs, 1,
			external_relocs_size, abfd)
	      != external_relocs_size))
	return false;
    }

  return true;
}

/* Relax a section when linking a MIPS ECOFF file.  This is used for
   embedded PIC code, which always uses PC relative branches which
   only have an 18 bit range on MIPS.  If a branch is not in range, we
   generate a long instruction sequence to compensate.  Each time we
   find a branch to expand, we have to check all the others again to
   make sure they are still in range.  This is slow, but it only has
   to be done when -relax is passed to the linker.

   This routine figures out which branches need to expand; the actual
   expansion is done in mips_relocate_section when the section
   contents are relocated.  The information is stored in the offsets
   field of the ecoff_section_tdata structure.  An offset of 1 means
   that the branch must be expanded into a multi-instruction PC
   relative branch (such an offset will only occur for a PC relative
   branch to an external symbol).  Any other offset must be a multiple
   of four, and is the amount to change the branch by (such an offset
   will only occur for a PC relative branch within the same section).

   We do not modify the section relocs or contents themselves so that
   if memory usage becomes an issue we can discard them and read them
   again.  The only information we must save in memory between this
   routine and the mips_relocate_section routine is the table of
   offsets.  */

static boolean
mips_relax_section (abfd, sec, info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *info;
     boolean *again;
{
  struct ecoff_section_tdata *section_tdata;
  bfd_byte *contents = NULL;
  long *offsets;
  struct external_reloc *ext_rel;
  struct external_reloc *ext_rel_end;
  unsigned int i;

  /* Assume we are not going to need another pass.  */
  *again = false;

  /* If we are not generating an ECOFF file, this is much too
     confusing to deal with.  */
  if (info->hash->creator->flavour != bfd_get_flavour (abfd))
    return true;

  /* If there are no relocs, there is nothing to do.  */
  if (sec->reloc_count == 0)
    return true;

  /* We are only interested in PC relative relocs, and why would there
     ever be one from anything but the .text section?  */
  if (strcmp (bfd_get_section_name (abfd, sec), ".text") != 0)
    return true;

  /* Read in the relocs, if we haven't already got them.  */
  section_tdata = ecoff_section_data (abfd, sec);
  if (section_tdata == (struct ecoff_section_tdata *) NULL
      || section_tdata->external_relocs == NULL)
    {
      if (! mips_read_relocs (abfd, sec))
	goto error_return;
      section_tdata = ecoff_section_data (abfd, sec);
    }

  if (sec->_cooked_size == 0)
    {
      /* We must initialize _cooked_size only the first time we are
	 called.  */
      sec->_cooked_size = sec->_raw_size;
    }

  contents = section_tdata->contents;
  offsets = section_tdata->offsets;

  /* Look for any external PC relative relocs.  Internal PC relative
     relocs are already correct in the object file, so they certainly
     can not overflow.  */
  ext_rel = (struct external_reloc *) section_tdata->external_relocs;
  ext_rel_end = ext_rel + sec->reloc_count;
  for (i = 0; ext_rel < ext_rel_end; ext_rel++, i++)
    {
      struct internal_reloc int_rel;
      struct ecoff_link_hash_entry *h;
      asection *hsec;
      bfd_signed_vma relocation;
      struct external_reloc *adj_ext_rel;
      unsigned int adj_i;
      unsigned long ext_count;
      struct ecoff_link_hash_entry **adj_h_ptr;
      struct ecoff_link_hash_entry **adj_h_ptr_end;
      struct ecoff_value_adjust *adjust;

      /* If we have already expanded this reloc, we certainly don't
	 need to do it again.  */
      if (offsets != (long *) NULL && offsets[i] == 1)
	continue;

      /* Quickly check that this reloc is external PCREL16.  */
      if (bfd_header_big_endian (abfd))
	{
	  if ((ext_rel->r_bits[3] & RELOC_BITS3_EXTERN_BIG) == 0
	      || (((ext_rel->r_bits[3] & RELOC_BITS3_TYPE_BIG)
		   >> RELOC_BITS3_TYPE_SH_BIG)
		  != MIPS_R_PCREL16))
	    continue;
	}
      else
	{
	  if ((ext_rel->r_bits[3] & RELOC_BITS3_EXTERN_LITTLE) == 0
	      || (((ext_rel->r_bits[3] & RELOC_BITS3_TYPE_LITTLE)
		   >> RELOC_BITS3_TYPE_SH_LITTLE)
		  != MIPS_R_PCREL16))
	    continue;
	}

      mips_ecoff_swap_reloc_in (abfd, (PTR) ext_rel, &int_rel);

      h = ecoff_data (abfd)->sym_hashes[int_rel.r_symndx];
      if (h == (struct ecoff_link_hash_entry *) NULL)
	abort ();

      if (h->root.type != bfd_link_hash_defined
	  && h->root.type != bfd_link_hash_defweak)
	{
	  /* Just ignore undefined symbols.  These will presumably
	     generate an error later in the link.  */
	  continue;
	}

      /* Get the value of the symbol.  */
      hsec = h->root.u.def.section;
      relocation = (h->root.u.def.value
		    + hsec->output_section->vma
		    + hsec->output_offset);

      /* Subtract out the current address.  */
      relocation -= (sec->output_section->vma
		     + sec->output_offset
		     + (int_rel.r_vaddr - sec->vma));

      /* The addend is stored in the object file.  In the normal case
	 of ``bal symbol'', the addend will be -4.  It will only be
	 different in the case of ``bal symbol+constant''.  To avoid
	 always reading in the section contents, we don't check the
	 addend in the object file (we could easily check the contents
	 if we happen to have already read them in, but I fear that
	 this could be confusing).  This means we will screw up if
	 there is a branch to a symbol that is in range, but added to
	 a constant which puts it out of range; in such a case the
	 link will fail with a reloc overflow error.  Since the
	 compiler will never generate such code, it should be easy
	 enough to work around it by changing the assembly code in the
	 source file.  */
      relocation -= 4;

      /* Now RELOCATION is the number we want to put in the object
	 file.  See whether it fits.  */
      if (relocation >= -0x20000 && relocation < 0x20000)
	continue;

      /* Now that we know this reloc needs work, which will rarely
	 happen, go ahead and grab the section contents.  */
      if (contents == (bfd_byte *) NULL)
	{
	  if (info->keep_memory)
	    contents = (bfd_byte *) bfd_alloc (abfd, sec->_raw_size);
	  else
	    contents = (bfd_byte *) bfd_malloc ((size_t) sec->_raw_size);
	  if (contents == (bfd_byte *) NULL)
	    goto error_return;
	  if (! bfd_get_section_contents (abfd, sec, (PTR) contents,
					  (file_ptr) 0, sec->_raw_size))
	    goto error_return;
	  if (info->keep_memory)
	    section_tdata->contents = contents;
	}

      /* We only support changing the bal instruction.  It would be
	 possible to handle other PC relative branches, but some of
	 them (the conditional branches) would require a different
	 length instruction sequence which would complicate both this
	 routine and mips_relax_pcrel16.  It could be written if
	 somebody felt it were important.  Ignoring this reloc will
	 presumably cause a reloc overflow error later on.  */
      if (bfd_get_32 (abfd, contents + int_rel.r_vaddr - sec->vma)
	  != 0x0411ffff) /* bgezal $0,. == bal . */
	continue;

      /* Bother.  We need to expand this reloc, and we will need to
	 make another relaxation pass since this change may put other
	 relocs out of range.  We need to examine the local branches
	 and we need to allocate memory to hold the offsets we must
	 add to them.  We also need to adjust the values of all
	 symbols in the object file following this location.  */

      sec->_cooked_size += PCREL16_EXPANSION_ADJUSTMENT;
      *again = true;

      if (offsets == (long *) NULL)
	{
	  size_t size;

	  size = sec->reloc_count * sizeof (long);
	  offsets = (long *) bfd_alloc (abfd, size);
	  if (offsets == (long *) NULL)
	    goto error_return;
	  memset (offsets, 0, size);
	  section_tdata->offsets = offsets;
	}

      offsets[i] = 1;

      /* Now look for all PC relative references that cross this reloc
	 and adjust their offsets.  */
      adj_ext_rel = (struct external_reloc *) section_tdata->external_relocs;
      for (adj_i = 0; adj_ext_rel < ext_rel_end; adj_ext_rel++, adj_i++)
	{
	  struct internal_reloc adj_int_rel;
	  bfd_vma start, stop;
	  int change;

	  mips_ecoff_swap_reloc_in (abfd, (PTR) adj_ext_rel, &adj_int_rel);

	  if (adj_int_rel.r_type == MIPS_R_PCREL16)
	    {
	      unsigned long insn;

	      /* We only care about local references.  External ones
		 will be relocated correctly anyhow.  */
	      if (adj_int_rel.r_extern)
		continue;

	      /* We are only interested in a PC relative reloc within
		 this section.  FIXME: Cross section PC relative
		 relocs may not be handled correctly; does anybody
		 care?  */
	      if (adj_int_rel.r_symndx != RELOC_SECTION_TEXT)
		continue;

	      start = adj_int_rel.r_vaddr;

	      insn = bfd_get_32 (abfd,
				 contents + adj_int_rel.r_vaddr - sec->vma);

	      stop = (insn & 0xffff) << 2;
	      if ((stop & 0x20000) != 0)
		stop -= 0x40000;
	      stop += adj_int_rel.r_vaddr + 4;
	    }
	  else if (adj_int_rel.r_type == MIPS_R_RELHI)
	    {
	      struct internal_reloc rello;
	      long addhi, addlo;

	      /* The next reloc must be MIPS_R_RELLO, and we handle
		 them together.  */
	      BFD_ASSERT (adj_ext_rel + 1 < ext_rel_end);

	      mips_ecoff_swap_reloc_in (abfd, (PTR) (adj_ext_rel + 1), &rello);

	      BFD_ASSERT (rello.r_type == MIPS_R_RELLO);
	      
	      addhi = bfd_get_32 (abfd,
				   contents + adj_int_rel.r_vaddr - sec->vma);
	      addhi &= 0xffff;
	      if (addhi & 0x8000)
		addhi -= 0x10000;
	      addhi <<= 16;

	      addlo = bfd_get_32 (abfd, contents + rello.r_vaddr - sec->vma);
	      addlo &= 0xffff;
	      if (addlo & 0x8000)
		addlo -= 0x10000;

	      if (adj_int_rel.r_extern)
		{
		  /* The value we want here is
		       sym - RELLOaddr + addend
		     which we can express as
		       sym - (RELLOaddr - addend)
		     Therefore if we are expanding the area between
		     RELLOaddr and RELLOaddr - addend we must adjust
		     the addend.  This is admittedly ambiguous, since
		     we might mean (sym + addend) - RELLOaddr, but in
		     practice we don't, and there is no way to handle
		     that case correctly since at this point we have
		     no idea whether any reloc is being expanded
		     between sym and sym + addend.  */
		  start = rello.r_vaddr - (addhi + addlo);
		  stop = rello.r_vaddr;
		}
	      else
		{
		  /* An internal RELHI/RELLO pair represents the
		     difference between two addresses, $LC0 - foo.
		     The symndx value is actually the difference
		     between the reloc address and $LC0.  This lets us
		     compute $LC0, and, by considering the addend,
		     foo.  If the reloc we are expanding falls between
		     those two relocs, we must adjust the addend.  At
		     this point, the symndx value is actually in the
		     r_offset field, where it was put by
		     mips_ecoff_swap_reloc_in.  */
		  start = rello.r_vaddr - adj_int_rel.r_offset;
		  stop = start + addhi + addlo;
		}
	    }
	  else if (adj_int_rel.r_type == MIPS_R_SWITCH)
	    {
	      /* A MIPS_R_SWITCH reloc represents a word of the form
		   .word $L3-$LS12
		 The value in the object file is correct, assuming the
		 original value of $L3.  The symndx value is actually
		 the difference between the reloc address and $LS12.
		 This lets us compute the original value of $LS12 as
		   vaddr - symndx
		 and the original value of $L3 as
		   vaddr - symndx + addend
		 where addend is the value from the object file.  At
		 this point, the symndx value is actually found in the
		 r_offset field, since it was moved by
		 mips_ecoff_swap_reloc_in.  */
	      start = adj_int_rel.r_vaddr - adj_int_rel.r_offset;
	      stop = start + bfd_get_32 (abfd,
					 (contents
					  + adj_int_rel.r_vaddr
					  - sec->vma));
	    }
	  else
	    continue;

	  /* If the range expressed by this reloc, which is the
	     distance between START and STOP crosses the reloc we are
	     expanding, we must adjust the offset.  The sign of the
	     adjustment depends upon the direction in which the range
	     crosses the reloc being expanded.  */
	  if (start <= int_rel.r_vaddr && stop > int_rel.r_vaddr)
	    change = PCREL16_EXPANSION_ADJUSTMENT;
	  else if (start > int_rel.r_vaddr && stop <= int_rel.r_vaddr)
	    change = - PCREL16_EXPANSION_ADJUSTMENT;
	  else
	    change = 0;

	  offsets[adj_i] += change;

	  if (adj_int_rel.r_type == MIPS_R_RELHI)
	    {
	      adj_ext_rel++;
	      adj_i++;
	      offsets[adj_i] += change;
	    }
	}

      /* Find all symbols in this section defined by this object file
	 and adjust their values.  Note that we decide whether to
	 adjust the value based on the value stored in the ECOFF EXTR
	 structure, because the value stored in the hash table may
	 have been changed by an earlier expanded reloc and thus may
	 no longer correctly indicate whether the symbol is before or
	 after the expanded reloc.  */
      ext_count = ecoff_data (abfd)->debug_info.symbolic_header.iextMax;
      adj_h_ptr = ecoff_data (abfd)->sym_hashes;
      adj_h_ptr_end = adj_h_ptr + ext_count;
      for (; adj_h_ptr < adj_h_ptr_end; adj_h_ptr++)
	{
	  struct ecoff_link_hash_entry *adj_h;

	  adj_h = *adj_h_ptr;
	  if (adj_h != (struct ecoff_link_hash_entry *) NULL
	      && (adj_h->root.type == bfd_link_hash_defined
		  || adj_h->root.type == bfd_link_hash_defweak)
	      && adj_h->root.u.def.section == sec
	      && adj_h->esym.asym.value > int_rel.r_vaddr)
	    adj_h->root.u.def.value += PCREL16_EXPANSION_ADJUSTMENT;
	}

      /* Add an entry to the symbol value adjust list.  This is used
	 by bfd_ecoff_debug_accumulate to adjust the values of
	 internal symbols and FDR's.  */
      adjust = ((struct ecoff_value_adjust *)
		bfd_alloc (abfd, sizeof (struct ecoff_value_adjust)));
      if (adjust == (struct ecoff_value_adjust *) NULL)
	goto error_return;

      adjust->start = int_rel.r_vaddr;
      adjust->end = sec->vma + sec->_raw_size;
      adjust->adjust = PCREL16_EXPANSION_ADJUSTMENT;

      adjust->next = ecoff_data (abfd)->debug_info.adjust;
      ecoff_data (abfd)->debug_info.adjust = adjust;
    }

  if (contents != (bfd_byte *) NULL && ! info->keep_memory)
    free (contents);

  return true;

 error_return:
  if (contents != (bfd_byte *) NULL && ! info->keep_memory)
    free (contents);
  return false;
}

/* This routine is called from mips_relocate_section when a PC
   relative reloc must be expanded into the five instruction sequence.
   It handles all the details of the expansion, including resolving
   the reloc.  */

static boolean
mips_relax_pcrel16 (info, input_bfd, input_section, h, location, address)
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     bfd *input_bfd;
     asection *input_section ATTRIBUTE_UNUSED;
     struct ecoff_link_hash_entry *h;
     bfd_byte *location;
     bfd_vma address;
{
  bfd_vma relocation;

  /* 0x0411ffff is bgezal $0,. == bal .  */
  BFD_ASSERT (bfd_get_32 (input_bfd, location) == 0x0411ffff);

  /* We need to compute the distance between the symbol and the
     current address plus eight.  */
  relocation = (h->root.u.def.value
		+ h->root.u.def.section->output_section->vma
		+ h->root.u.def.section->output_offset);
  relocation -= address + 8;

  /* If the lower half is negative, increment the upper 16 half.  */
  if ((relocation & 0x8000) != 0)
    relocation += 0x10000;

  bfd_put_32 (input_bfd, 0x04110001, location); /* bal .+8 */
  bfd_put_32 (input_bfd,
	      0x3c010000 | ((relocation >> 16) & 0xffff), /* lui $at,XX */
	      location + 4);
  bfd_put_32 (input_bfd,
	      0x24210000 | (relocation & 0xffff), /* addiu $at,$at,XX */
	      location + 8);
  bfd_put_32 (input_bfd, 0x003f0821, location + 12); /* addu $at,$at,$ra */
  bfd_put_32 (input_bfd, 0x0020f809, location + 16); /* jalr $at */

  return true;
}

/* Given a .sdata section and a .rel.sdata in-memory section, store
   relocation information into the .rel.sdata section which can be
   used at runtime to relocate the section.  This is called by the
   linker when the --embedded-relocs switch is used.  This is called
   after the add_symbols entry point has been called for all the
   objects, and before the final_link entry point is called.  This
   function presumes that the object was compiled using
   -membedded-pic.  */

boolean
bfd_mips_ecoff_create_embedded_relocs (abfd, info, datasec, relsec, errmsg)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *datasec;
     asection *relsec;
     char **errmsg;
{
  struct ecoff_link_hash_entry **sym_hashes;
  struct ecoff_section_tdata *section_tdata;
  struct external_reloc *ext_rel;
  struct external_reloc *ext_rel_end;
  bfd_byte *p;

  BFD_ASSERT (! info->relocateable);

  *errmsg = NULL;

  if (datasec->reloc_count == 0)
    return true;

  sym_hashes = ecoff_data (abfd)->sym_hashes;

  if (! mips_read_relocs (abfd, datasec))
    return false;

  relsec->contents = (bfd_byte *) bfd_alloc (abfd, datasec->reloc_count * 4);
  if (relsec->contents == NULL)
    return false;

  p = relsec->contents;

  section_tdata = ecoff_section_data (abfd, datasec);
  ext_rel = (struct external_reloc *) section_tdata->external_relocs;
  ext_rel_end = ext_rel + datasec->reloc_count;
  for (; ext_rel < ext_rel_end; ext_rel++, p += 4)
    {
      struct internal_reloc int_rel;
      boolean text_relative;

      mips_ecoff_swap_reloc_in (abfd, (PTR) ext_rel, &int_rel);

      /* We are going to write a four byte word into the runtime reloc
	 section.  The word will be the address in the data section
	 which must be relocated.  This must be on a word boundary,
	 which means the lower two bits must be zero.  We use the
	 least significant bit to indicate how the value in the data
	 section must be relocated.  A 0 means that the value is
	 relative to the text section, while a 1 indicates that the
	 value is relative to the data section.  Given that we are
	 assuming the code was compiled using -membedded-pic, there
	 should not be any other possibilities.  */

      /* We can only relocate REFWORD relocs at run time.  */
      if (int_rel.r_type != MIPS_R_REFWORD)
	{
	  *errmsg = _("unsupported reloc type");
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      if (int_rel.r_extern)
	{
	  struct ecoff_link_hash_entry *h;

	  h = sym_hashes[int_rel.r_symndx];
	  /* If h is NULL, that means that there is a reloc against an
	     external symbol which we thought was just a debugging
	     symbol.  This should not happen.  */
	  if (h == (struct ecoff_link_hash_entry *) NULL)
	    abort ();
	  if ((h->root.type == bfd_link_hash_defined
	       || h->root.type == bfd_link_hash_defweak)
	      && (h->root.u.def.section->flags & SEC_CODE) != 0)
	    text_relative = true;
	  else
	    text_relative = false;
	}
      else
	{
	  switch (int_rel.r_symndx)
	    {
	    case RELOC_SECTION_TEXT:
	      text_relative = true;
	      break;
	    case RELOC_SECTION_SDATA:
	    case RELOC_SECTION_SBSS:
	    case RELOC_SECTION_LIT8:
	      text_relative = false;
	      break;
	    default:
	      /* No other sections should appear in -membedded-pic
                 code.  */
	      *errmsg = _("reloc against unsupported section");
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	}

      if ((int_rel.r_offset & 3) != 0)
	{
	  *errmsg = _("reloc not properly aligned");
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      bfd_put_32 (abfd,
		  (int_rel.r_vaddr - datasec->vma + datasec->output_offset
		   + (text_relative ? 0 : 1)),
		  p);
    }

  return true;
}

/* This is the ECOFF backend structure.  The backend field of the
   target vector points to this.  */

static const struct ecoff_backend_data mips_ecoff_backend_data =
{
  /* COFF backend structure.  */
  {
    (void (*) PARAMS ((bfd *,PTR,int,int,int,int,PTR))) bfd_void, /* aux_in */
    (void (*) PARAMS ((bfd *,PTR,PTR))) bfd_void, /* sym_in */
    (void (*) PARAMS ((bfd *,PTR,PTR))) bfd_void, /* lineno_in */
    (unsigned (*) PARAMS ((bfd *,PTR,int,int,int,int,PTR)))bfd_void,/*aux_out*/
    (unsigned (*) PARAMS ((bfd *,PTR,PTR))) bfd_void, /* sym_out */
    (unsigned (*) PARAMS ((bfd *,PTR,PTR))) bfd_void, /* lineno_out */
    (unsigned (*) PARAMS ((bfd *,PTR,PTR))) bfd_void, /* reloc_out */
    mips_ecoff_swap_filehdr_out, mips_ecoff_swap_aouthdr_out,
    mips_ecoff_swap_scnhdr_out,
    FILHSZ, AOUTSZ, SCNHSZ, 0, 0, 0, 0, FILNMLEN, true, false, 4,
    mips_ecoff_swap_filehdr_in, mips_ecoff_swap_aouthdr_in,
    mips_ecoff_swap_scnhdr_in, NULL,
    mips_ecoff_bad_format_hook, _bfd_ecoff_set_arch_mach_hook,
    _bfd_ecoff_mkobject_hook, _bfd_ecoff_styp_to_sec_flags,
    _bfd_ecoff_set_alignment_hook, _bfd_ecoff_slurp_symbol_table,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL
  },
  /* Supported architecture.  */
  bfd_arch_mips,
  /* Initial portion of armap string.  */
  "__________",
  /* The page boundary used to align sections in a demand-paged
     executable file.  E.g., 0x1000.  */
  0x1000,
  /* True if the .rdata section is part of the text segment, as on the
     Alpha.  False if .rdata is part of the data segment, as on the
     MIPS.  */
  false,
  /* Bitsize of constructor entries.  */
  32,
  /* Reloc to use for constructor entries.  */
  &mips_howto_table[MIPS_R_REFWORD],
  {
    /* Symbol table magic number.  */
    magicSym,
    /* Alignment of debugging information.  E.g., 4.  */
    4,
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
    _bfd_ecoff_slurp_symbolic_info
  },
  /* External reloc size.  */
  RELSZ,
  /* Reloc swapping functions.  */
  mips_ecoff_swap_reloc_in,
  mips_ecoff_swap_reloc_out,
  /* Backend reloc tweaking.  */
  mips_adjust_reloc_in,
  mips_adjust_reloc_out,
  /* Relocate section contents while linking.  */
  mips_relocate_section,
  /* Do final adjustments to filehdr and aouthdr.  */
  NULL,
  /* Read an element from an archive at a given file position.  */
  _bfd_get_elt_at_filepos
};

/* Looking up a reloc type is MIPS specific.  */
#define _bfd_ecoff_bfd_reloc_type_lookup mips_bfd_reloc_type_lookup

/* Getting relocated section contents is generic.  */
#define _bfd_ecoff_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents

/* Handling file windows is generic.  */
#define _bfd_ecoff_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

/* Relaxing sections is MIPS specific.  */
#define _bfd_ecoff_bfd_relax_section mips_relax_section

/* GC of sections is not done.  */
#define _bfd_ecoff_bfd_gc_sections bfd_generic_gc_sections

extern const bfd_target ecoff_big_vec;

const bfd_target ecoff_little_vec =
{
  "ecoff-littlemips",		/* name */
  bfd_target_ecoff_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_CODE | SEC_DATA),
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

  {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
     _bfd_ecoff_archive_p, _bfd_dummy_target},
  {bfd_false, _bfd_ecoff_mkobject,  /* bfd_set_format */
     _bfd_generic_mkarchive, bfd_false},
  {bfd_false, _bfd_ecoff_write_object_contents, /* bfd_write_contents */
     _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (_bfd_ecoff),
     BFD_JUMP_TABLE_COPY (_bfd_ecoff),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_ecoff),
     BFD_JUMP_TABLE_SYMBOLS (_bfd_ecoff),
     BFD_JUMP_TABLE_RELOCS (_bfd_ecoff),
     BFD_JUMP_TABLE_WRITE (_bfd_ecoff),
     BFD_JUMP_TABLE_LINK (_bfd_ecoff),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  & ecoff_big_vec,
  
  (PTR) &mips_ecoff_backend_data
};

const bfd_target ecoff_big_vec =
{
  "ecoff-bigmips",		/* name */
  bfd_target_ecoff_flavour,
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_CODE | SEC_DATA),
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16,
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16,
 {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
    _bfd_ecoff_archive_p, _bfd_dummy_target},
 {bfd_false, _bfd_ecoff_mkobject, /* bfd_set_format */
    _bfd_generic_mkarchive, bfd_false},
 {bfd_false, _bfd_ecoff_write_object_contents, /* bfd_write_contents */
    _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (_bfd_ecoff),
     BFD_JUMP_TABLE_COPY (_bfd_ecoff),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_ecoff),
     BFD_JUMP_TABLE_SYMBOLS (_bfd_ecoff),
     BFD_JUMP_TABLE_RELOCS (_bfd_ecoff),
     BFD_JUMP_TABLE_WRITE (_bfd_ecoff),
     BFD_JUMP_TABLE_LINK (_bfd_ecoff),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  & ecoff_little_vec,
  
  (PTR) &mips_ecoff_backend_data
};

const bfd_target ecoff_biglittle_vec =
{
  "ecoff-biglittlemips",		/* name */
  bfd_target_ecoff_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_CODE | SEC_DATA),
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

  {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
     _bfd_ecoff_archive_p, _bfd_dummy_target},
  {bfd_false, _bfd_ecoff_mkobject,  /* bfd_set_format */
     _bfd_generic_mkarchive, bfd_false},
  {bfd_false, _bfd_ecoff_write_object_contents, /* bfd_write_contents */
     _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (_bfd_ecoff),
     BFD_JUMP_TABLE_COPY (_bfd_ecoff),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_ecoff),
     BFD_JUMP_TABLE_SYMBOLS (_bfd_ecoff),
     BFD_JUMP_TABLE_RELOCS (_bfd_ecoff),
     BFD_JUMP_TABLE_WRITE (_bfd_ecoff),
     BFD_JUMP_TABLE_LINK (_bfd_ecoff),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,
  
  (PTR) &mips_ecoff_backend_data
};
