/* BFD back-end for WDC 65816 COFF binaries.
   Copyright 1995 Free Software Foundation, Inc.
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
#include "coff/w65.h"
#include "coff/internal.h"
#include "libcoff.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (1)
static reloc_howto_type howto_table[] =
{
  HOWTO (R_W65_ABS8,    0,  0, 8, false, 0, complain_overflow_bitfield, 0, "abs8", true, 0x000000ff, 0x000000ff, false),
  HOWTO (R_W65_ABS16,   1,  0, 16, false, 0, complain_overflow_bitfield, 0, "abs16", true, 0x0000ffff, 0x0000ffff, false),
  HOWTO (R_W65_ABS24,   0,  2, 32, false, 0, complain_overflow_bitfield, 0, "abs24", true, 0x00ffffff, 0x00ffffff, false),
  HOWTO (R_W65_ABS8S8,  0,  0, 8, false, 0, complain_overflow_bitfield, 0, ">abs8", true, 0x000000ff, 0x000000ff, false),
  HOWTO (R_W65_ABS8S16, 0,  0, 8, false, 0, complain_overflow_bitfield, 0, "^abs8", true, 0x000000ff, 0x000000ff, false),
  HOWTO (R_W65_ABS16S8, 1,  0, 16, false, 0, complain_overflow_bitfield, 0, ">abs16", true, 0x0000ffff, 0x0000ffff, false),
  HOWTO (R_W65_ABS16S16,1,  0, 16, false, 0, complain_overflow_bitfield, 0, "^abs16", true, 0x0000ffff, 0x0000ffff, false),
  HOWTO (R_W65_PCR8,    0,  0, 8, false, 0, complain_overflow_bitfield, 0, "pcrel8", true, 0x000000ff, 0x000000ff, true),
  HOWTO (R_W65_PCR16,   1,  0, 16, false, 0, complain_overflow_bitfield, 0, "pcrel16", true, 0x0000ffff, 0x0000ffff, true),
  HOWTO (R_W65_DP,    0,  0, 8, false, 0, complain_overflow_bitfield, 0, "dp", true, 0x000000ff, 0x000000ff, false),

};


/* Turn a howto into a reloc number */

#define SELECT_RELOC(x,howto) \
  { x.r_type = select_reloc(howto); }

#define BADMAG(x) (W65BADMAG(x))
#define W65 1			/* Customize coffcode.h */
#define __A_MAGIC_SET__


/* Code to swap in the reloc */
#define SWAP_IN_RELOC_OFFSET   bfd_h_get_32
#define SWAP_OUT_RELOC_OFFSET bfd_h_put_32
#define SWAP_OUT_RELOC_EXTRA(abfd, src, dst) \
  dst->r_stuff[0] = 'S'; \
  dst->r_stuff[1] = 'C';


static int
select_reloc (howto)
     reloc_howto_type *howto;
{
  return howto->type ;
}

/* Code to turn a r_type into a howto ptr, uses the above howto table
   */

static void
rtype2howto (internal, dst)
     arelent *internal;
     struct internal_reloc *dst;
{
      internal->howto = howto_table + dst->r_type - 1;
}

#define RTYPE2HOWTO(internal, relocentry) rtype2howto(internal,relocentry)


/* Perform any necessaru magic to the addend in a reloc entry */


#define CALC_ADDEND(abfd, symbol, ext_reloc, cache_ptr) \
 cache_ptr->addend =  ext_reloc.r_offset;


#define RELOC_PROCESSING(relent,reloc,symbols,abfd,section) \
 reloc_processing(relent, reloc, symbols, abfd, section)

static void
reloc_processing (relent, reloc, symbols, abfd, section)
     arelent * relent;
     struct internal_reloc *reloc;
     asymbol ** symbols;
     bfd * abfd;
     asection * section;
{
  relent->address = reloc->r_vaddr;
  rtype2howto (relent, reloc);

  if (((int) reloc->r_symndx) > 0)
    {
      relent->sym_ptr_ptr = symbols + obj_convert (abfd)[reloc->r_symndx];
    }
  else
    {
      relent->sym_ptr_ptr = (asymbol **)&(bfd_abs_symbol);
    }



  relent->addend = reloc->r_offset;

  relent->address -= section->vma;
  /*  relent->section = 0;*/
}


static int
h8300_reloc16_estimate(abfd, input_section, reloc, shrink, link_info)
     bfd *abfd;
     asection *input_section;
     arelent *reloc;
     unsigned int shrink;
     struct bfd_link_info *link_info;
{
  bfd_vma value;  
  bfd_vma dot;
  bfd_vma gap;

  /* The address of the thing to be relocated will have moved back by 
   the size of the shrink  - but we don't change reloc->address here,
   since we need it to know where the relocation lives in the source
   uncooked section */

  /*  reloc->address -= shrink;   conceptual */

  bfd_vma address = reloc->address - shrink;
  

  switch (reloc->howto->type)
    {     
    case R_MOVB2:
    case R_JMP2:
      shrink+=2;
      break;

      /* Thing is a move one byte */
    case R_MOVB1:
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);

      if (value >= 0xff00)
	{ 

	  /* Change the reloc type from 16bit, possible 8 to 8bit
	     possible 16 */
	  reloc->howto = reloc->howto + 1;	  
	  /* The place to relc moves back by one */
	  /* This will be two bytes smaller in the long run */
	  shrink +=2 ;
	  bfd_perform_slip(abfd, 2, input_section, address);
	}      

      break;
      /* This is the 24 bit branch which could become an 8 bitter, 
       the relocation points to the first byte of the insn, not the
       actual data */

    case R_JMPL1:
      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);
	
      dot = input_section->output_section->vma +
	input_section->output_offset + address;
  
      /* See if the address we're looking at within 127 bytes of where
	 we are, if so then we can use a small branch rather than the
	 jump we were going to */

      gap = value - dot ;
  
      if (-120 < (long)gap && (long)gap < 120 )
	{ 

	  /* Change the reloc type from 24bit, possible 8 to 8bit
	     possible 32 */
	  reloc->howto = reloc->howto + 1;	  
	  /* This will be two bytes smaller in the long run */
	  shrink +=2 ;
	  bfd_perform_slip(abfd, 2, input_section, address);
	}
      break;

    case R_JMP1:

      value = bfd_coff_reloc16_get_value(reloc, link_info, input_section);
	
      dot = input_section->output_section->vma +
	input_section->output_offset + address;
  
      /* See if the address we're looking at within 127 bytes of where
	 we are, if so then we can use a small branch rather than the
	 jump we were going to */

      gap = value - (dot - shrink);
  

      if (-120 < (long)gap && (long)gap < 120 )
	{ 

	  /* Change the reloc type from 16bit, possible 8 to 8bit
	     possible 16 */
	  reloc->howto = reloc->howto + 1;	  
	  /* The place to relc moves back by one */

	  /* This will be two bytes smaller in the long run */
	  shrink +=2 ;
	  bfd_perform_slip(abfd, 2, input_section, address);
	}
      break;
    }

  
  return shrink;
}


/* First phase of a relaxing link */

/* Reloc types
   large		small
   R_MOVB1		R_MOVB2		mov.b with 16bit or 8 bit address
   R_JMP1		R_JMP2		jmp or pcrel branch
   R_JMPL1		R_JMPL_B8	24jmp or pcrel branch
   R_MOVLB1		R_MOVLB2	24 or 8 bit reloc for mov.b

*/

static void
h8300_reloc16_extra_cases (abfd, link_info, link_order, reloc, data, src_ptr,
			   dst_ptr)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     arelent *reloc;
     bfd_byte *data;
     unsigned int *src_ptr;
     unsigned int *dst_ptr;
{
  unsigned int src_address = *src_ptr;
  unsigned int dst_address = *dst_ptr;
  asection *input_section = link_order->u.indirect.section;

  switch (reloc->howto->type)
    {
    case R_W65_ABS8:
    case R_W65_DP:
      {
	unsigned int gap = bfd_coff_reloc16_get_value (reloc, link_info,
						       input_section);
	bfd_put_8 (abfd, gap, data + dst_address);
	dst_address += 1;
	src_address += 1;
      }
      break;

    case R_W65_ABS8S8:
      {
	unsigned int gap = bfd_coff_reloc16_get_value (reloc, link_info,
						       input_section);
	gap >>= 8;
	bfd_put_8 (abfd, gap, data + dst_address);
	dst_address += 1;
	src_address += 1;
      }
      break;

    case R_W65_ABS8S16:
      {
	unsigned int gap = bfd_coff_reloc16_get_value (reloc, link_info,
						       input_section);
	gap >>=16;
	bfd_put_8 (abfd, gap, data + dst_address);
	dst_address += 1;
	src_address += 1;
      }
      break;

    case R_W65_ABS16:
      {
	unsigned int gap = bfd_coff_reloc16_get_value (reloc, link_info,
						       input_section);

	bfd_put_16 (abfd, gap, data + dst_address);
	dst_address += 2;
	src_address += 2;
      }
      break;
    case R_W65_ABS16S8:
      {
	unsigned int gap = bfd_coff_reloc16_get_value (reloc, link_info,
						       input_section);
	gap >>= 8;
	bfd_put_16 (abfd, gap, data + dst_address);
	dst_address += 2;
	src_address += 2;
      }
      break;
    case R_W65_ABS16S16:
      {
	unsigned int gap = bfd_coff_reloc16_get_value (reloc, link_info,
						       input_section);
	gap >>= 16;
	bfd_put_16 (abfd, gap, data + dst_address);
	dst_address += 2;
	src_address += 2;
      }
      break;

    case R_W65_ABS24:
      {
	unsigned int gap = bfd_coff_reloc16_get_value (reloc, link_info,
						       input_section);
	bfd_put_16 (abfd, gap, data + dst_address);
	bfd_put_8 (abfd, gap>>16, data+dst_address+2);
	dst_address += 3;
	src_address += 3;
      }
      break;

    case R_W65_PCR8:
      {
	int gap = bfd_coff_reloc16_get_value (reloc, link_info,
					      input_section);
	bfd_vma dot = link_order->offset 
	  + dst_address 
	    + link_order->u.indirect.section->output_section->vma;

	gap -= dot + 1;
	if (gap < -128 || gap > 127) {
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort();
	}
	bfd_put_8 (abfd, gap, data + dst_address);
	dst_address += 1;
	src_address += 1;
      }
      break;

    case R_W65_PCR16:
      {
	bfd_vma gap = bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section);
	bfd_vma dot = link_order->offset 
	  + dst_address 
	    + link_order->u.indirect.section->output_section->vma;


	/* This wraps within the page, so ignore the relativeness, look at the
	   high part */
	if ((gap & 0xf0000) != (dot & 0xf0000)) {
	  if (! ((*link_info->callbacks->reloc_overflow)
		 (link_info, bfd_asymbol_name (*reloc->sym_ptr_ptr),
		  reloc->howto->name, reloc->addend, input_section->owner,
		  input_section, reloc->address)))
	    abort();
	}

	gap -= dot + 2;
	bfd_put_16 (abfd, gap, data + dst_address);
	dst_address += 2;
	src_address += 2;
      }
      break;
    default:
      printf("ignoring reloc %s\n", reloc->howto->name);
      break;

    }
  *src_ptr = src_address;
  *dst_ptr = dst_address;

}

#define coff_reloc16_extra_cases h8300_reloc16_extra_cases
#define coff_reloc16_estimate h8300_reloc16_estimate

#include "coffcode.h"


#undef coff_bfd_get_relocated_section_contents
#undef coff_bfd_relax_section
#define coff_bfd_get_relocated_section_contents \
  bfd_coff_reloc16_get_relocated_section_contents
#define coff_bfd_relax_section bfd_coff_reloc16_relax_section



bfd_target w65_vec =
{
  "coff-w65",			/* name */
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | BFD_IS_RELAXABLE ),
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC),	/* section flags */
  '_',				/* leading char */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
  bfd_getl32, bfd_getl_signed_32, bfd_putl32,
  bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* hdrs */

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
