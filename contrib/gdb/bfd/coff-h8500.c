/* BFD back-end for Hitachi H8/500 COFF binaries.
   Copyright 1993, 1994 Free Software Foundation, Inc.
   Contributed by Cygnus Support.
   Written by Steve Chamberlain, <sac@cygnus.com>.

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
#include "obstack.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/h8500.h"
#include "coff/internal.h"
#include "libcoff.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (1)

static reloc_howto_type r_imm8 =
HOWTO (R_H8500_IMM8, 0, 1, 8, false, 0,
       complain_overflow_bitfield, 0, "r_imm8", true, 0x000000ff, 0x000000ff, false);

static reloc_howto_type r_imm16 =
HOWTO (R_H8500_IMM16, 0, 1, 16, false, 0,
       complain_overflow_bitfield, 0, "r_imm16", true, 0x0000ffff, 0x0000ffff, false);

static reloc_howto_type r_imm24 =
HOWTO (R_H8500_IMM24, 0, 1, 24, false, 0,
       complain_overflow_bitfield, 0, "r_imm24", true, 0x00ffffff, 0x00ffffff, false);

static reloc_howto_type r_imm32 =
HOWTO (R_H8500_IMM32, 0, 1, 32, false, 0,
       complain_overflow_bitfield, 0, "r_imm32", true, 0xffffffff, 0xffffffff, false);


static reloc_howto_type r_high8 =
HOWTO (R_H8500_HIGH8, 0, 1, 8, false, 0,
       complain_overflow_dont, 0, "r_high8", true, 0x000000ff, 0x000000ff, false);

static reloc_howto_type r_low16 =
HOWTO (R_H8500_LOW16, 0, 1, 16, false, 0,
       complain_overflow_dont, 0, "r_low16", true, 0x0000ffff, 0x0000ffff, false);

static reloc_howto_type r_pcrel8 =
HOWTO (R_H8500_PCREL8, 0, 1, 8, true, 0, complain_overflow_signed, 0, "r_pcrel8", true, 0, 0, true);


static reloc_howto_type r_pcrel16 =
HOWTO (R_H8500_PCREL16, 0, 1, 16, true, 0, complain_overflow_signed, 0, "r_pcrel16", true, 0, 0, true);

static reloc_howto_type r_high16 =
HOWTO (R_H8500_HIGH16, 0, 1, 8, false, 0,
       complain_overflow_dont, 0, "r_high16", true, 0x000ffff, 0x0000ffff, false);


/* Turn a howto into a reloc number */

static int 
coff_h8500_select_reloc (howto)
     reloc_howto_type *howto;
{
  return howto->type;
}

#define SELECT_RELOC(x,howto) x.r_type = coff_h8500_select_reloc(howto)


#define BADMAG(x) H8500BADMAG(x)
#define H8500 1			/* Customize coffcode.h */

#define __A_MAGIC_SET__

/* Code to swap in the reloc */
#define SWAP_IN_RELOC_OFFSET   bfd_h_get_32
#define SWAP_OUT_RELOC_OFFSET bfd_h_put_32
#define SWAP_OUT_RELOC_EXTRA(abfd, src, dst) \
  dst->r_stuff[0] = 'S'; \
  dst->r_stuff[1] = 'C';

/* Code to turn a r_type into a howto ptr, uses the above howto table
   */

static void
rtype2howto(internal, dst)
     arelent * internal;
     struct internal_reloc *dst;
{
  switch (dst->r_type)
    {
    default:
      abort ();
      break;
    case R_H8500_IMM8:
      internal->howto = &r_imm8;
      break;
    case R_H8500_IMM16:
      internal->howto = &r_imm16;
      break;
    case R_H8500_IMM24:
      internal->howto = &r_imm24;
      break;
    case R_H8500_IMM32:
      internal->howto = &r_imm32;
      break;
    case R_H8500_PCREL8:
      internal->howto = &r_pcrel8;
      break;
    case R_H8500_PCREL16:
      internal->howto = &r_pcrel16;
      break;
    case R_H8500_HIGH8:
      internal->howto = &r_high8;
      break;
    case R_H8500_HIGH16:
      internal->howto = &r_high16;
      break;
    case R_H8500_LOW16:
      internal->howto = &r_low16;
      break;
    }
}

#define RTYPE2HOWTO(internal, relocentry) rtype2howto(internal,relocentry)


/* Perform any necessaru magic to the addend in a reloc entry */


#define CALC_ADDEND(abfd, symbol, ext_reloc, cache_ptr) \
 cache_ptr->addend =  ext_reloc.r_offset;


#define RELOC_PROCESSING(relent,reloc,symbols,abfd,section) \
 reloc_processing(relent, reloc, symbols, abfd, section)

static void reloc_processing (relent, reloc, symbols, abfd, section)
     arelent * relent;
     struct internal_reloc *reloc;
     asymbol ** symbols;
     bfd * abfd;
     asection * section;
{
  relent->address = reloc->r_vaddr;
  rtype2howto (relent, reloc);

  if (reloc->r_symndx > 0)
    {
      relent->sym_ptr_ptr = symbols + obj_convert (abfd)[reloc->r_symndx];
    }
  else
    {
      relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
    }


  relent->addend = reloc->r_offset;
  relent->address -= section->vma;
}

static void
extra_case (in_abfd, link_info, link_order, reloc, data, src_ptr, dst_ptr)
     bfd *in_abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     arelent *reloc;
     bfd_byte *data;
     unsigned int *src_ptr;
     unsigned int *dst_ptr;
{
  bfd_byte *d = data+*dst_ptr;
  asection *input_section = link_order->u.indirect.section;
  switch (reloc->howto->type)
    {
    case R_H8500_IMM8:
      bfd_put_8 (in_abfd,
		 bfd_coff_reloc16_get_value (reloc, link_info, input_section),
		 d);
      (*dst_ptr) += 1;
      (*src_ptr) += 1;
      break;

    case R_H8500_HIGH8:
      bfd_put_8 (in_abfd,
		 (bfd_coff_reloc16_get_value (reloc, link_info, input_section)
		  >> 16),
		 d );
      (*dst_ptr) += 1;
      (*src_ptr) += 1;
      break;

    case R_H8500_IMM16:
      bfd_put_16 (in_abfd,
		  bfd_coff_reloc16_get_value (reloc, link_info, input_section),
		  d  );
      (*dst_ptr) += 2;
      (*src_ptr) += 2;
      break;

    case R_H8500_LOW16:
      bfd_put_16 (in_abfd,
		  bfd_coff_reloc16_get_value (reloc, link_info, input_section),
		  d);

      (*dst_ptr) += 2;
      (*src_ptr) += 2;
      break;
      
    case R_H8500_HIGH16:
      bfd_put_16 (in_abfd,
		  (bfd_coff_reloc16_get_value (reloc, link_info, input_section)
		   >>16),
		  d);

      (*dst_ptr) += 2;
      (*src_ptr) += 2;
      break;

    case R_H8500_IMM24:
      {
	int v = bfd_coff_reloc16_get_value(reloc, link_info, input_section);
	int o = bfd_get_32(in_abfd, data+ *dst_ptr -1);
	v = (v & 0x00ffffff) | (o & 0xff00000);
	bfd_put_32 (in_abfd, v, data  + *dst_ptr -1);
	(*dst_ptr) +=3;
	(*src_ptr)+=3;;
      }
      break;
    case R_H8500_IMM32:
      {
	int v = bfd_coff_reloc16_get_value(reloc, link_info, input_section);
	bfd_put_32 (in_abfd, v, data  + *dst_ptr);
	(*dst_ptr) +=4;
	(*src_ptr)+=4;;
      }
      break;


    case R_H8500_PCREL8:
      {
	bfd_vma dst = bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section);
	bfd_vma dot = link_order->offset
	  + *dst_ptr
	    + link_order->u.indirect.section->output_section->vma;
	int gap = dst - dot - 1; /* -1 since were in the odd byte of the
				    word and the pc's been incremented */

	if (gap > 128 || gap < -128)
	  {
	    if (! ((*link_info->callbacks->reloc_overflow)
		   (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		    reloc->howto->name, reloc->addend, input_section->owner,
		    input_section, reloc->address)))
	      abort ();
	  }
	bfd_put_8 (in_abfd, gap, data + *dst_ptr);
	(*dst_ptr)++;
	(*src_ptr)++;
	break;
      }
    case R_H8500_PCREL16:
      {
	bfd_vma dst = bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section);
	bfd_vma dot = link_order->offset
	  + *dst_ptr
	    + link_order->u.indirect.section->output_section->vma;
	int gap = dst - dot - 1; /* -1 since were in the odd byte of the
				    word and the pc's been incremented */

	if (gap > 32767 || gap < -32768)
	  {
	    if (! ((*link_info->callbacks->reloc_overflow)
		   (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		    reloc->howto->name, reloc->addend, input_section->owner,
		    input_section, reloc->address)))
	      abort ();
	  }
	bfd_put_16 (in_abfd, gap, data + *dst_ptr);
	(*dst_ptr)+=2;
	(*src_ptr)+=2;
	break;
      }

    default:
      abort ();
    }
}

#define coff_reloc16_extra_cases extra_case

#include "coffcode.h"


#undef  coff_bfd_get_relocated_section_contents
#undef coff_bfd_relax_section
#define coff_bfd_get_relocated_section_contents \
  bfd_coff_reloc16_get_relocated_section_contents
#define coff_bfd_relax_section bfd_coff_reloc16_relax_section

const bfd_target h8500coff_vec =
{
  "coff-h8500",			/* name */
  bfd_target_coff_flavour,
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC),	/* section flags */
  '_',				/* leading symbol underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */

  {_bfd_dummy_target, coff_object_p,	/* bfd_check_format */
   bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, coff_mkobject, _bfd_generic_mkarchive,	/* bfd_set_format */
   bfd_false},
  {bfd_false, coff_write_object_contents,	/* bfd_write_contents */
   _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (coff),
     BFD_JUMP_TABLE_COPY (coff),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
     BFD_JUMP_TABLE_SYMBOLS (coff),
     BFD_JUMP_TABLE_RELOCS (coff),
     BFD_JUMP_TABLE_WRITE (coff),
     BFD_JUMP_TABLE_LINK (coff),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  COFF_SWAP_TABLE,
};
