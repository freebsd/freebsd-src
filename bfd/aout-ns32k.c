/* BFD back-end for ns32k a.out-ish binaries.
   Copyright 1990, 1991, 1992, 1994, 1995, 1996, 1998, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
   Contributed by Ian Dall (idall@eleceng.adelaide.edu.au).

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
#include "aout/aout64.h"
#include "ns32k.h"

/* Do not "beautify" the CONCAT* macro args.  Traditional C will not
   remove whitespace added here, and thus will fail to concatenate
   the tokens.  */
#define MYNS(OP) CONCAT2 (ns32kaout_,OP)

reloc_howto_type *
MYNS(bfd_reloc_type_lookup)
  PARAMS((bfd *abfd AND
	  bfd_reloc_code_real_type code));

bfd_boolean
MYNS(write_object_contents)
  PARAMS((bfd *abfd));

/* Avoid multiple definitions from aoutx if supporting
   standard a.out format(s) as well as this one.  */
#define NAME(x,y) CONCAT3 (ns32kaout,_32_,y)

void bfd_ns32k_arch PARAMS ((void));

#include "libaout.h"

#define MY(OP) MYNS(OP)

#define MY_swap_std_reloc_in  MY(swap_std_reloc_in)
#define MY_swap_std_reloc_out MY(swap_std_reloc_out)

static void
MY_swap_std_reloc_in PARAMS ((bfd *, struct reloc_std_external *,
			      arelent *, asymbol **,
			      bfd_size_type));
static void
MY_swap_std_reloc_out PARAMS ((bfd *, arelent *,
			       struct reloc_std_external *));
reloc_howto_type *
MY(reloc_howto) PARAMS ((bfd *, struct reloc_std_external *,
			 int *, int *, int *));
void
MY(put_reloc) PARAMS ((bfd *, int, int, bfd_vma, reloc_howto_type *,
		       struct reloc_std_external *));

/* The ns32k series is ah, unusual, when it comes to relocation.
   There are three storage methods for relocatable objects.  There
   are displacements, immediate operands and ordinary twos complement
   data. Of these, only the last fits into the standard relocation
   scheme.  Immediate operands are stored huffman encoded and
   immediate operands are stored big endian (where as the natural byte
   order is little endian for this architecture).

   Note that the ns32k displacement storage method is orthogonal to
   whether the relocation is pc relative or not. The "displacement"
   storage scheme is used for essentially all address constants. The
   displacement can be relative to zero (absolute displacement),
   relative to the pc (pc relative), the stack pointer, the frame
   pointer, the static base register and general purpose register etc.

   For example:

   sym1: .long .	 # pc relative 2's complement
   sym1: .long foo	 # 2's complement not pc relative

   self:  movd @self, r0 # pc relative displacement
          movd foo, r0   # non pc relative displacement

   self:  movd self, r0  # pc relative immediate
          movd foo, r0   # non pc relative immediate

   In addition, for historical reasons the encoding of the relocation types
   in the a.out format relocation entries is such that even the relocation
   methods which are standard are not encoded the standard way.  */

reloc_howto_type MY(howto_table)[] =
  {
    /* type           rs   size bsz  pcrel bitpos ovrf                  sf name          part_inpl readmask setmask pcdone */
    /* ns32k immediate operands.  */
    HOWTO (BFD_RELOC_NS32K_IMM_8, 0, 0, 8, FALSE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_imm, "NS32K_IMM_8",
	   TRUE, 0x000000ff,0x000000ff, FALSE),
    HOWTO (BFD_RELOC_NS32K_IMM_16, 0, 1, 16, FALSE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_imm,  "NS32K_IMM_16",
	   TRUE, 0x0000ffff,0x0000ffff, FALSE),
    HOWTO (BFD_RELOC_NS32K_IMM_32, 0, 2, 32, FALSE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_imm, "NS32K_IMM_32",
	   TRUE, 0xffffffff,0xffffffff, FALSE),
    HOWTO (BFD_RELOC_NS32K_IMM_8_PCREL, 0, 0, 8, TRUE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_imm, "PCREL_NS32K_IMM_8",
	   TRUE, 0x000000ff, 0x000000ff, FALSE),
    HOWTO (BFD_RELOC_NS32K_IMM_16_PCREL, 0, 1, 16, TRUE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_imm, "PCREL_NS32K_IMM_16",
	   TRUE, 0x0000ffff,0x0000ffff, FALSE),
    HOWTO (BFD_RELOC_NS32K_IMM_32_PCREL, 0, 2, 32, TRUE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_imm, "PCREL_NS32K_IMM_32",
	   TRUE, 0xffffffff,0xffffffff, FALSE),

    /* ns32k displacements.  */
    HOWTO (BFD_RELOC_NS32K_DISP_8, 0, 0, 7, FALSE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_disp, "NS32K_DISP_8",
	   TRUE, 0x000000ff,0x000000ff, FALSE),
    HOWTO (BFD_RELOC_NS32K_DISP_16, 0, 1, 14, FALSE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_disp, "NS32K_DISP_16",
	   TRUE, 0x0000ffff, 0x0000ffff, FALSE),
    HOWTO (BFD_RELOC_NS32K_DISP_32, 0, 2, 30, FALSE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_disp, "NS32K_DISP_32",
	   TRUE, 0xffffffff, 0xffffffff, FALSE),
    HOWTO (BFD_RELOC_NS32K_DISP_8_PCREL, 0, 0, 7, TRUE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_disp, "PCREL_NS32K_DISP_8",
	   TRUE, 0x000000ff,0x000000ff, FALSE),
    HOWTO (BFD_RELOC_NS32K_DISP_16_PCREL, 0, 1, 14, TRUE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_disp, "PCREL_NS32K_DISP_16",
	   TRUE, 0x0000ffff,0x0000ffff, FALSE),
    HOWTO (BFD_RELOC_NS32K_DISP_32_PCREL, 0, 2, 30, TRUE, 0, complain_overflow_signed,
	   _bfd_ns32k_reloc_disp, "PCREL_NS32K_DISP_32",
	   TRUE, 0xffffffff,0xffffffff, FALSE),

    /* Normal 2's complement.  */
    HOWTO (BFD_RELOC_8, 0, 0, 8, FALSE, 0, complain_overflow_bitfield,0,
	   "8", TRUE, 0x000000ff,0x000000ff, FALSE),
    HOWTO (BFD_RELOC_16, 0, 1, 16, FALSE, 0, complain_overflow_bitfield,0,
	   "16", TRUE, 0x0000ffff,0x0000ffff, FALSE),
    HOWTO (BFD_RELOC_32, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,0,
	   "32", TRUE, 0xffffffff,0xffffffff, FALSE),
    HOWTO (BFD_RELOC_8_PCREL, 0, 0, 8, TRUE, 0, complain_overflow_signed, 0,
	   "PCREL_8", TRUE, 0x000000ff,0x000000ff, FALSE),
    HOWTO (BFD_RELOC_16_PCREL, 0, 1, 16, TRUE, 0, complain_overflow_signed, 0,
	   "PCREL_16", TRUE, 0x0000ffff,0x0000ffff, FALSE),
    HOWTO (BFD_RELOC_32_PCREL, 0, 2, 32, TRUE, 0, complain_overflow_signed, 0,
	   "PCREL_32", TRUE, 0xffffffff,0xffffffff, FALSE),
  };

#define CTOR_TABLE_RELOC_HOWTO(BFD) (MY(howto_table) + 14)

#define RELOC_STD_BITS_NS32K_TYPE_BIG 0x06
#define RELOC_STD_BITS_NS32K_TYPE_LITTLE 0x60
#define RELOC_STD_BITS_NS32K_TYPE_SH_BIG 1
#define RELOC_STD_BITS_NS32K_TYPE_SH_LITTLE 5

reloc_howto_type *
MY(reloc_howto) (abfd, rel, r_index, r_extern, r_pcrel)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct reloc_std_external *rel;
     int *r_index;
     int *r_extern;
     int *r_pcrel;
{
  unsigned int r_length;
  int r_ns32k_type;

  /*  BFD_ASSERT(bfd_header_little_endian (abfd)); */
  *r_index =  ((rel->r_index[2] << 16)
	       | (rel->r_index[1] << 8)
	       |  rel->r_index[0] );
  *r_extern  = (0 != (rel->r_type[0] & RELOC_STD_BITS_EXTERN_LITTLE));
  *r_pcrel   = (0 != (rel->r_type[0] & RELOC_STD_BITS_PCREL_LITTLE));
  r_length  =  ((rel->r_type[0] & RELOC_STD_BITS_LENGTH_LITTLE)
		>> RELOC_STD_BITS_LENGTH_SH_LITTLE);
  r_ns32k_type  =  ((rel->r_type[0] & RELOC_STD_BITS_NS32K_TYPE_LITTLE)
		    >> RELOC_STD_BITS_NS32K_TYPE_SH_LITTLE);
  return (MY(howto_table) + r_length + 3 * (*r_pcrel) + 6 * r_ns32k_type);
}

#define MY_reloc_howto(BFD, REL, IN, EX, PC) \
  MY(reloc_howto) (BFD, REL, &IN, &EX, &PC)

void
MY(put_reloc) (abfd, r_extern, r_index, value, howto, reloc)
     bfd *abfd;
     int r_extern;
     int r_index;
     bfd_vma value;
     reloc_howto_type *howto;
     struct reloc_std_external *reloc;
{
  unsigned int r_length;
  int r_pcrel;
  int r_ns32k_type;

  PUT_WORD (abfd, value, reloc->r_address);
  r_length = howto->size ;	/* Size as a power of two.  */
  r_pcrel  = (int) howto->pc_relative; /* Relative to PC?  */
  r_ns32k_type = (howto - MY(howto_table) )/6;

  /*  BFD_ASSERT (bfd_header_little_endian (abfd)); */
  reloc->r_index[2] = r_index >> 16;
  reloc->r_index[1] = r_index >> 8;
  reloc->r_index[0] = r_index;
  reloc->r_type[0] =
    (r_extern?    RELOC_STD_BITS_EXTERN_LITTLE: 0)
      | (r_pcrel?     RELOC_STD_BITS_PCREL_LITTLE: 0)
	| (r_length <<  RELOC_STD_BITS_LENGTH_SH_LITTLE)
	  | (r_ns32k_type <<  RELOC_STD_BITS_NS32K_TYPE_SH_LITTLE);
}

#define MY_put_reloc(BFD, EXT, IDX, VAL, HOWTO, RELOC) \
  MY(put_reloc) (BFD, EXT, IDX, VAL, HOWTO, RELOC)

#define STAT_FOR_EXEC

#define MY_final_link_relocate _bfd_ns32k_final_link_relocate
#define MY_relocate_contents _bfd_ns32k_relocate_contents

#include "aoutx.h"

reloc_howto_type *
MY(bfd_reloc_type_lookup) (abfd,code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{

#define ENTRY(i,j)	case i: return &MY(howto_table)[j]

  int ext = obj_reloc_entry_size (abfd) == RELOC_EXT_SIZE;

  BFD_ASSERT(ext == 0);
  if (code == BFD_RELOC_CTOR)
    switch (bfd_get_arch_info (abfd)->bits_per_address)
      {
      case 32:
	code = BFD_RELOC_32;
	break;
      default:
	break;
      }
  switch (code)
    {
      ENTRY(BFD_RELOC_NS32K_IMM_8, 0);
      ENTRY(BFD_RELOC_NS32K_IMM_16, 1);
      ENTRY(BFD_RELOC_NS32K_IMM_32, 2);
      ENTRY(BFD_RELOC_NS32K_IMM_8_PCREL, 3);
      ENTRY(BFD_RELOC_NS32K_IMM_16_PCREL, 4);
      ENTRY(BFD_RELOC_NS32K_IMM_32_PCREL, 5);
      ENTRY(BFD_RELOC_NS32K_DISP_8, 6);
      ENTRY(BFD_RELOC_NS32K_DISP_16, 7);
      ENTRY(BFD_RELOC_NS32K_DISP_32, 8);
      ENTRY(BFD_RELOC_NS32K_DISP_8_PCREL, 9);
      ENTRY(BFD_RELOC_NS32K_DISP_16_PCREL, 10);
      ENTRY(BFD_RELOC_NS32K_DISP_32_PCREL, 11);
      ENTRY(BFD_RELOC_8, 12);
      ENTRY(BFD_RELOC_16, 13);
      ENTRY(BFD_RELOC_32, 14);
      ENTRY(BFD_RELOC_8_PCREL, 15);
      ENTRY(BFD_RELOC_16_PCREL, 16);
      ENTRY(BFD_RELOC_32_PCREL, 17);
    default:
      return (reloc_howto_type *) NULL;
    }
#undef ENTRY
}

static void
MY_swap_std_reloc_in (abfd, bytes, cache_ptr, symbols, symcount)
     bfd *abfd;
     struct reloc_std_external *bytes;
     arelent *cache_ptr;
     asymbol **symbols;
     bfd_size_type symcount ATTRIBUTE_UNUSED;
{
  int r_index;
  int r_extern;
  int r_pcrel;
  struct aoutdata  *su = &(abfd->tdata.aout_data->a);

  cache_ptr->address = H_GET_32 (abfd, bytes->r_address);

  /* Now the fun stuff.  */
  cache_ptr->howto = MY_reloc_howto(abfd, bytes, r_index, r_extern, r_pcrel);

  MOVE_ADDRESS (0);
}

static void
MY_swap_std_reloc_out (abfd, g, natptr)
     bfd *abfd;
     arelent *g;
     struct reloc_std_external *natptr;
{
  int r_index;
  asymbol *sym = *(g->sym_ptr_ptr);
  int r_extern;
  unsigned int r_addend;
  asection *output_section = sym->section->output_section;

  r_addend = g->addend + (*(g->sym_ptr_ptr))->section->output_section->vma;

  /* Name was clobbered by aout_write_syms to be symbol index.  */

  /* If this relocation is relative to a symbol then set the
     r_index to the symbols index, and the r_extern bit.

     Absolute symbols can come in in two ways, either as an offset
     from the abs section, or as a symbol which has an abs value.
     Check for that here.  */
  if (bfd_is_com_section (output_section)
      || output_section == &bfd_abs_section
      || output_section == &bfd_und_section)
    {
      if (bfd_abs_section.symbol == sym)
	{
	  /* Whoops, looked like an abs symbol, but is really an offset
	     from the abs section.  */
	  r_index = 0;
	  r_extern = 0;
	}
      else
	{
	  /* Fill in symbol.  */
	  r_extern = 1;
#undef KEEPIT
#define KEEPIT udata.i
	  r_index =  (*(g->sym_ptr_ptr))->KEEPIT;
#undef KEEPIT
	}
    }
  else
    {
      /* Just an ordinary section.  */
      r_extern = 0;
      r_index  = output_section->target_index;
    }

  MY_put_reloc (abfd, r_extern, r_index, g->address, g->howto, natptr);
}

bfd_reloc_status_type
_bfd_ns32k_relocate_contents (howto, input_bfd, relocation, location)
     reloc_howto_type *howto;
     bfd *input_bfd;
     bfd_vma relocation;
     bfd_byte *location;
{
  int r_ns32k_type = (howto - MY(howto_table)) / 6;
  bfd_vma (*get_data) PARAMS ((bfd_byte *, int));
  void (*put_data) PARAMS ((bfd_vma, bfd_byte *, int));

  switch (r_ns32k_type)
    {
    case 0:
      get_data = _bfd_ns32k_get_immediate;
      put_data = _bfd_ns32k_put_immediate;
      break;
    case 1:
      get_data = _bfd_ns32k_get_displacement;
      put_data = _bfd_ns32k_put_displacement;
      break;
    case 2:
      return _bfd_relocate_contents (howto, input_bfd, relocation,
				    location);
      /* NOT REACHED */
      break;
    default:
      return bfd_reloc_notsupported;
    }
  return _bfd_do_ns32k_reloc_contents (howto, input_bfd, relocation,
				       location, get_data, put_data);
}
