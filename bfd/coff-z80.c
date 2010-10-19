/* BFD back-end for Zilog Z80 COFF binaries.
   Copyright 2005 Free Software Foundation, Inc.
   Contributed by Arnold Metselaar <arnold_m@operamail.com>

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/z80.h"
#include "coff/internal.h"
#include "libcoff.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER 0

static reloc_howto_type r_imm32 =
HOWTO (R_IMM32, 0, 1, 32, FALSE, 0,
       complain_overflow_dont, 0, "r_imm32", TRUE, 0xffffffff, 0xffffffff,
       FALSE);

static reloc_howto_type r_imm24 =
HOWTO (R_IMM24, 0, 1, 24, FALSE, 0,
       complain_overflow_dont, 0, "r_imm24", TRUE, 0x00ffffff, 0x00ffffff,
       FALSE);

static reloc_howto_type r_imm16 =
HOWTO (R_IMM16, 0, 1, 16, FALSE, 0,
       complain_overflow_dont, 0, "r_imm16", TRUE, 0x0000ffff, 0x0000ffff,
       FALSE);

static reloc_howto_type r_imm8 =
HOWTO (R_IMM8, 0, 0, 8, FALSE, 0,
       complain_overflow_bitfield, 0, "r_imm8", TRUE, 0x000000ff, 0x000000ff,
       FALSE);

static reloc_howto_type r_jr =
HOWTO (R_JR, 0, 0, 8, TRUE, 0, 
       complain_overflow_signed, 0, "r_jr", FALSE, 0, 0xFF,
       FALSE);

static reloc_howto_type r_off8 =
HOWTO (R_OFF8, 0, 0, 8, FALSE, 0, 
       complain_overflow_signed, 0,"r_off8", FALSE, 0, 0xff,
       FALSE);


#define BADMAG(x) Z80BADMAG(x)
#define Z80 1			/* Customize coffcode.h.  */
#define __A_MAGIC_SET__

/* Code to swap in the reloc.  */

#define SWAP_IN_RELOC_OFFSET	H_GET_32
#define SWAP_OUT_RELOC_OFFSET	H_PUT_32

#define SWAP_OUT_RELOC_EXTRA(abfd, src, dst) \
  dst->r_stuff[0] = 'S'; \
  dst->r_stuff[1] = 'C';

/* Code to turn a r_type into a howto ptr, uses the above howto table.  */

static void
rtype2howto (arelent *internal, struct internal_reloc *dst)
{
  switch (dst->r_type)
    {
    default:
      abort ();
      break;
    case R_IMM8:
      internal->howto = &r_imm8;
      break;
    case R_IMM16:
      internal->howto = &r_imm16;
      break;
    case R_IMM24:
      internal->howto = &r_imm24;
      break;
    case R_IMM32:
      internal->howto = &r_imm32;
      break;
    case R_JR:
      internal->howto = &r_jr;
      break;
    case R_OFF8:
      internal->howto = &r_off8;
      break;
    }
}

#define RTYPE2HOWTO(internal, relocentry) rtype2howto (internal, relocentry)

static reloc_howto_type *
coff_z80_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			    bfd_reloc_code_real_type code)
{
  switch (code)
    {
    case BFD_RELOC_8:		return & r_imm8;
    case BFD_RELOC_16:		return & r_imm16;
    case BFD_RELOC_24:		return & r_imm24;
    case BFD_RELOC_32:		return & r_imm32;
    case BFD_RELOC_8_PCREL:	return & r_jr;
    case BFD_RELOC_Z80_DISP8:	return & r_off8;
    default:			BFD_FAIL ();
      return NULL;
    }
}

/* Perform any necessary magic to the addend in a reloc entry.  */

#define CALC_ADDEND(abfd, symbol, ext_reloc, cache_ptr) \
 cache_ptr->addend =  ext_reloc.r_offset;

#define RELOC_PROCESSING(relent,reloc,symbols,abfd,section) \
 reloc_processing(relent, reloc, symbols, abfd, section)

static void
reloc_processing (arelent *relent,
                  struct internal_reloc *reloc,
                  asymbol **symbols,
                  bfd *abfd,
                  asection *section)
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
extra_case (bfd *in_abfd,
            struct bfd_link_info *link_info,
            struct bfd_link_order *link_order,
            arelent *reloc,
            bfd_byte *data,
            unsigned int *src_ptr,
            unsigned int *dst_ptr)
{
  asection * input_section = link_order->u.indirect.section;
  int val;

  switch (reloc->howto->type)
    {
    case R_OFF8:
	val = bfd_coff_reloc16_get_value (reloc, link_info,
					   input_section);
	if (val>127 || val<-128) /* Test for overflow.  */
	  {
	    if (! ((*link_info->callbacks->reloc_overflow)
		   (link_info, NULL,
		    bfd_asymbol_name (*reloc->sym_ptr_ptr),
		    reloc->howto->name, reloc->addend, input_section->owner,
		    input_section, reloc->address)))
	      abort ();
	  }
	bfd_put_8 (in_abfd, val, data + *dst_ptr);
	(*dst_ptr) += 1;
	(*src_ptr) += 1;
      break;

    case R_IMM8:
      val = bfd_get_8 ( in_abfd, data+*src_ptr)
	+ bfd_coff_reloc16_get_value (reloc, link_info, input_section);
      bfd_put_8 (in_abfd, val, data + *dst_ptr);
      (*dst_ptr) += 1;
      (*src_ptr) += 1;
      break;

    case R_IMM16:
      val = bfd_get_16 ( in_abfd, data+*src_ptr)
	+ bfd_coff_reloc16_get_value (reloc, link_info, input_section);
      bfd_put_16 (in_abfd, val, data + *dst_ptr);
      (*dst_ptr) += 2;
      (*src_ptr) += 2;
      break;

    case R_IMM24:
      val = bfd_get_16 ( in_abfd, data+*src_ptr)
	+ (bfd_get_8 ( in_abfd, data+*src_ptr+2) << 16)
	+ bfd_coff_reloc16_get_value (reloc, link_info, input_section);
      bfd_put_16 (in_abfd, val, data + *dst_ptr);
      bfd_put_8 (in_abfd, val >> 16, data + *dst_ptr+2);
      (*dst_ptr) += 3;
      (*src_ptr) += 3;
      break;

    case R_IMM32:
      val = bfd_get_32 ( in_abfd, data+*src_ptr)
	+ bfd_coff_reloc16_get_value (reloc, link_info, input_section);
      bfd_put_32 (in_abfd, val, data + *dst_ptr);
      (*dst_ptr) += 4;
      (*src_ptr) += 4;
      break;

    case R_JR:
      {
	bfd_vma dst = bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section);
	bfd_vma dot = (*dst_ptr
		       + input_section->output_offset
		       + input_section->output_section->vma);
	int gap = dst - dot - 1;  /* -1, Since the offset is relative
				     to the value of PC after reading
				     the offset.  */

	if (gap >= 128 || gap < -128)
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

    default:
      abort ();
    }
}

#define coff_reloc16_extra_cases    extra_case
#define coff_bfd_reloc_type_lookup  coff_z80_reloc_type_lookup

#include "coffcode.h"

#undef  coff_bfd_get_relocated_section_contents
#define coff_bfd_get_relocated_section_contents \
  bfd_coff_reloc16_get_relocated_section_contents

#undef  coff_bfd_relax_section
#define coff_bfd_relax_section bfd_coff_reloc16_relax_section

CREATE_LITTLE_COFF_TARGET_VEC (z80coff_vec, "coff-z80", 0, 0, '\0', NULL, 
			       COFF_SWAP_TABLE)

