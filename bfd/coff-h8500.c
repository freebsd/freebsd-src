/* BFD back-end for Renesas H8/500 COFF binaries.
   Copyright 1993, 1994, 1995, 1997, 1999, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.
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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/h8500.h"
#include "coff/internal.h"
#include "libcoff.h"

static int  coff_h8500_select_reloc PARAMS ((reloc_howto_type *));
static void rtype2howto      PARAMS ((arelent *, struct internal_reloc *));
static void reloc_processing PARAMS ((arelent *, struct internal_reloc *, asymbol **, bfd *, asection *));
static void extra_case       PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *, arelent *, bfd_byte *, unsigned int *, unsigned int *));

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (1)

static reloc_howto_type r_imm8 =
HOWTO (R_H8500_IMM8, 0, 1, 8, FALSE, 0,
       complain_overflow_bitfield, 0, "r_imm8", TRUE, 0x000000ff, 0x000000ff, FALSE);

static reloc_howto_type r_imm16 =
HOWTO (R_H8500_IMM16, 0, 1, 16, FALSE, 0,
       complain_overflow_bitfield, 0, "r_imm16", TRUE, 0x0000ffff, 0x0000ffff, FALSE);

static reloc_howto_type r_imm24 =
HOWTO (R_H8500_IMM24, 0, 1, 24, FALSE, 0,
       complain_overflow_bitfield, 0, "r_imm24", TRUE, 0x00ffffff, 0x00ffffff, FALSE);

static reloc_howto_type r_imm32 =
HOWTO (R_H8500_IMM32, 0, 1, 32, FALSE, 0,
       complain_overflow_bitfield, 0, "r_imm32", TRUE, 0xffffffff, 0xffffffff, FALSE);

static reloc_howto_type r_high8 =
HOWTO (R_H8500_HIGH8, 0, 1, 8, FALSE, 0,
       complain_overflow_dont, 0, "r_high8", TRUE, 0x000000ff, 0x000000ff, FALSE);

static reloc_howto_type r_low16 =
HOWTO (R_H8500_LOW16, 0, 1, 16, FALSE, 0,
       complain_overflow_dont, 0, "r_low16", TRUE, 0x0000ffff, 0x0000ffff, FALSE);

static reloc_howto_type r_pcrel8 =
HOWTO (R_H8500_PCREL8, 0, 1, 8, TRUE, 0, complain_overflow_signed, 0, "r_pcrel8", TRUE, 0, 0, TRUE);

static reloc_howto_type r_pcrel16 =
HOWTO (R_H8500_PCREL16, 0, 1, 16, TRUE, 0, complain_overflow_signed, 0, "r_pcrel16", TRUE, 0, 0, TRUE);

static reloc_howto_type r_high16 =
HOWTO (R_H8500_HIGH16, 0, 1, 8, FALSE, 0,
       complain_overflow_dont, 0, "r_high16", TRUE, 0x000ffff, 0x0000ffff, FALSE);

/* Turn a howto into a reloc number.  */

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

/* Code to swap in the reloc.  */
#define SWAP_IN_RELOC_OFFSET	H_GET_32
#define SWAP_OUT_RELOC_OFFSET	H_PUT_32
#define SWAP_OUT_RELOC_EXTRA(abfd, src, dst) \
  dst->r_stuff[0] = 'S'; \
  dst->r_stuff[1] = 'C';

/* Code to turn a r_type into a howto ptr, uses the above howto table.  */

static void
rtype2howto (internal, dst)
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

/* Perform any necessary magic to the addend in a reloc entry.  */

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
    relent->sym_ptr_ptr = symbols + obj_convert (abfd)[reloc->r_symndx];
  else
    relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;

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
		 d);
      (*dst_ptr) += 1;
      (*src_ptr) += 1;
      break;

    case R_H8500_IMM16:
      bfd_put_16 (in_abfd,
		  bfd_coff_reloc16_get_value (reloc, link_info, input_section),
		  d);
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
		   >> 16),
		  d);

      (*dst_ptr) += 2;
      (*src_ptr) += 2;
      break;

    case R_H8500_IMM24:
      {
	int v = bfd_coff_reloc16_get_value (reloc, link_info, input_section);
	int o = bfd_get_32 (in_abfd, data+ *dst_ptr -1);
	v = (v & 0x00ffffff) | (o & 0xff00000);
	bfd_put_32 (in_abfd, (bfd_vma) v, data  + *dst_ptr -1);
	(*dst_ptr) += 3;
	(*src_ptr) += 3;;
      }
      break;
    case R_H8500_IMM32:
      {
	int v = bfd_coff_reloc16_get_value (reloc, link_info, input_section);
	bfd_put_32 (in_abfd, (bfd_vma) v, data  + *dst_ptr);
	(*dst_ptr) += 4;
	(*src_ptr) += 4;;
      }
      break;

    case R_H8500_PCREL8:
      {
	bfd_vma dst = bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section);
	bfd_vma dot = (*dst_ptr
		       + input_section->output_offset
		       + input_section->output_section->vma);
	int gap = dst - dot - 1; /* -1 since were in the odd byte of the
				    word and the pc's been incremented.  */

	if (gap > 128 || gap < -128)
	  {
	    if (! ((*link_info->callbacks->reloc_overflow)
		   (link_info, NULL,
		    bfd_asymbol_name (*reloc->sym_ptr_ptr),
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
	bfd_vma dot = (*dst_ptr
		       + input_section->output_offset
		       + input_section->output_section->vma);
	int gap = dst - dot - 1; /* -1 since were in the odd byte of the
				    word and the pc's been incremented.  */

	if (gap > 32767 || gap < -32768)
	  {
	    if (! ((*link_info->callbacks->reloc_overflow)
		   (link_info, NULL,
		    bfd_asymbol_name (*reloc->sym_ptr_ptr),
		    reloc->howto->name, reloc->addend, input_section->owner,
		    input_section, reloc->address)))
	      abort ();
	  }
	bfd_put_16 (in_abfd, (bfd_vma) gap, data + *dst_ptr);
	(*dst_ptr) += 2;
	(*src_ptr) += 2;
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

CREATE_BIG_COFF_TARGET_VEC (h8500coff_vec, "coff-h8500", 0, 0, '_', NULL, COFF_SWAP_TABLE)
