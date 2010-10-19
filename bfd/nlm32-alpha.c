/* Support for 32-bit Alpha NLM (NetWare Loadable Module)
   Copyright 1993, 1994, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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

/* This file describes the 32 bit Alpha NLM format.  You might think
   that an Alpha chip would use a 64 bit format, but, for some reason,
   it doesn't.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#define ARCH_SIZE 32

#include "nlm/alpha-ext.h"
#define Nlm_External_Fixed_Header	Nlm32_alpha_External_Fixed_Header

#include "libnlm.h"

/* Alpha NLM's have a prefix header before the standard NLM.  This
   function reads it in, verifies the version, and seeks the bfd to
   the location before the regular NLM header.  */

static bfd_boolean
nlm_alpha_backend_object_p (bfd *abfd)
{
  struct nlm32_alpha_external_prefix_header s;
  file_ptr size;

  if (bfd_bread (&s, (bfd_size_type) sizeof s, abfd) != sizeof s)
    return FALSE;

  if (H_GET_32 (abfd, s.magic) != NLM32_ALPHA_MAGIC)
    return FALSE;

  /* FIXME: Should we check the format number?  */

  /* Skip to the end of the header.  */
  size = H_GET_32 (abfd, s.size);
  if (bfd_seek (abfd, size, SEEK_SET) != 0)
    return FALSE;

  return TRUE;
}

/* Write out the prefix.  */

static bfd_boolean
nlm_alpha_write_prefix (bfd *abfd)
{
  struct nlm32_alpha_external_prefix_header s;

  memset (&s, 0, sizeof s);
  H_PUT_32 (abfd, NLM32_ALPHA_MAGIC, s.magic);
  H_PUT_32 (abfd, 2, s.format);
  H_PUT_32 (abfd, sizeof s, s.size);
  if (bfd_bwrite (&s, (bfd_size_type) sizeof s, abfd) != sizeof s)
    return FALSE;
  return TRUE;
}

#define ONES(n) (((bfd_vma) 1 << ((n) - 1) << 1) - 1)

/* How to process the various reloc types.  */

static reloc_howto_type nlm32_alpha_howto_table[] =
{
  /* Reloc type 0 is ignored by itself.  However, it appears after a
     GPDISP reloc to identify the location where the low order 16 bits
     of the gp register are loaded.  */
  HOWTO (ALPHA_R_IGNORE,	/* Type.  */
	 0,			/* Rightshift.  */
	 0,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 8,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "IGNORE",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* A 32 bit reference to a symbol.  */
  HOWTO (ALPHA_R_REFLONG,	/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "REFLONG",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0xffffffff,		/* Source mask.  */
	 0xffffffff,		/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* A 64 bit reference to a symbol.  */
  HOWTO (ALPHA_R_REFQUAD,	/* Type.  */
	 0,			/* Rightshift.  */
	 4,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 64,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "REFQUAD",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 ONES (64),		/* Source mask.  */
	 ONES (64),		/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* A 32 bit GP relative offset.  This is just like REFLONG except
     that when the value is used the value of the gp register will be
     added in.  */
  HOWTO (ALPHA_R_GPREL32,	/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "GPREL32",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0xffffffff,		/* Source mask.  */
	 0xffffffff,		/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* Used for an instruction that refers to memory off the GP
     register.  The offset is 16 bits of the 32 bit instruction.  This
     reloc always seems to be against the .lita section.  */
  HOWTO (ALPHA_R_LITERAL,	/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "LITERAL",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0xffff,		/* Source mask.  */
	 0xffff,		/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* This reloc only appears immediately following a LITERAL reloc.
     It identifies a use of the literal.  It seems that the linker can
     use this to eliminate a portion of the .lita section.  The symbol
     index is special: 1 means the literal address is in the base
     register of a memory format instruction; 2 means the literal
     address is in the byte offset register of a byte-manipulation
     instruction; 3 means the literal address is in the target
     register of a jsr instruction.  This does not actually do any
     relocation.  */
  HOWTO (ALPHA_R_LITUSE,	/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "LITUSE",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* Load the gp register.  This is always used for a ldah instruction
     which loads the upper 16 bits of the gp register.  The next reloc
     will be an IGNORE reloc which identifies the location of the lda
     instruction which loads the lower 16 bits.  The symbol index of
     the GPDISP instruction appears to actually be the number of bytes
     between the ldah and lda instructions.  This gives two different
     ways to determine where the lda instruction is; I don't know why
     both are used.  The value to use for the relocation is the
     difference between the GP value and the current location; the
     load will always be done against a register holding the current
     address.  */
  HOWTO (ALPHA_R_GPDISP,	/* Type.  */
	 16,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "GPDISP",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0xffff,		/* Source mask.  */
	 0xffff,		/* Dest mask.  */
	 TRUE),			/* PCrel_offset.  */

  /* A 21 bit branch.  The native assembler generates these for
     branches within the text segment, and also fills in the PC
     relative offset in the instruction.  It seems to me that this
     reloc, unlike the others, is not partial_inplace.  */
  HOWTO (ALPHA_R_BRADDR,	/* Type.  */
	 2,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 21,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "BRADDR",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0x1fffff,		/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* A hint for a jump to a register.  */
  HOWTO (ALPHA_R_HINT,		/* Type.  */
	 2,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 14,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "HINT",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0x3fff,		/* Source mask.  */
	 0x3fff,		/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* 16 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL16,	/* Type.  */
	 0,			/* Rightshift.  */
	 1,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "SREL16",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0xffff,		/* Source mask.  */
	 0xffff,		/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* 32 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL32,	/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "SREL32",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0xffffffff,		/* Source mask.  */
	 0xffffffff,		/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* A 64 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL64,	/* Type.  */
	 0,			/* Rightshift.  */
	 4,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 64,			/* Bitsize.  */
	 TRUE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_signed, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "SREL64",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 ONES (64),		/* Source mask.  */
	 ONES (64),		/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* Push a value on the reloc evaluation stack.  */
  HOWTO (ALPHA_R_OP_PUSH,	/* Type.  */
	 0,			/* Rightshift.  */
	 0,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 0,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "OP_PUSH",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* Store the value from the stack at the given address.  Store it in
     a bitfield of size r_size starting at bit position r_offset.  */
  HOWTO (ALPHA_R_OP_STORE,	/* Type.  */
	 0,			/* Rightshift.  */
	 4,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 64,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "OP_STORE",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 ONES (64),		/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* Subtract the reloc address from the value on the top of the
     relocation stack.  */
  HOWTO (ALPHA_R_OP_PSUB,	/* Type.  */
	 0,			/* Rightshift.  */
	 0,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 0,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "OP_PSUB",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* Shift the value on the top of the relocation stack right by the
     given value.  */
  HOWTO (ALPHA_R_OP_PRSHIFT,	/* Type.  */
	 0,			/* Rightshift.  */
	 0,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 0,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont, /* Complain_on_overflow.  */
	 0,			 /* Special_function.  */
	 "OP_PRSHIFT",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE),		/* PCrel_offset.  */

  /* Adjust the GP value for a new range in the object file.  */
  HOWTO (ALPHA_R_GPVALUE,	/* Type.  */
	 0,			/* Rightshift.  */
	 0,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 0,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "GPVALUE",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE)			/* PCrel_offset.  */
};

static reloc_howto_type nlm32_alpha_nw_howto =
  HOWTO (ALPHA_R_NW_RELOC,	/* Type.  */
	 0,			/* Rightshift.  */
	 0,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 0,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_dont, /* Complain_on_overflow.  */
	 0,			/* Special_function.  */
	 "NW_RELOC",		/* Name.  */
	 FALSE,			/* Partial_inplace.  */
	 0,			/* Source mask.  */
	 0,			/* Dest mask.  */
	 FALSE);		/* PCrel_offset.  */

/* Read an Alpha NLM reloc.  This routine keeps some static data which
   it uses when handling local relocs.  This only works correctly
   because all the local relocs are read at once.  */

static bfd_boolean
nlm_alpha_read_reloc (bfd *abfd,
		      nlmNAME (symbol_type) *sym,
		      asection **secp,
		      arelent *rel)
{
  static bfd_vma gp_value;
  static bfd_vma lita_address;
  struct nlm32_alpha_external_reloc ext;
  bfd_vma r_vaddr;
  long r_symndx;
  int r_type, r_extern, r_offset, r_size;
  asection *code_sec, *data_sec;

  /* Read the reloc from the file.  */
  if (bfd_bread (&ext, (bfd_size_type) sizeof ext, abfd) != sizeof ext)
    return FALSE;

  /* Swap in the reloc information.  */
  r_vaddr = H_GET_64 (abfd, ext.r_vaddr);
  r_symndx = H_GET_32 (abfd, ext.r_symndx);

  BFD_ASSERT (bfd_little_endian (abfd));

  r_type = ((ext.r_bits[0] & RELOC_BITS0_TYPE_LITTLE)
	    >> RELOC_BITS0_TYPE_SH_LITTLE);
  r_extern = (ext.r_bits[1] & RELOC_BITS1_EXTERN_LITTLE) != 0;
  r_offset = ((ext.r_bits[1] & RELOC_BITS1_OFFSET_LITTLE)
	      >> RELOC_BITS1_OFFSET_SH_LITTLE);
  /* Ignore the reserved bits.  */
  r_size = ((ext.r_bits[3] & RELOC_BITS3_SIZE_LITTLE)
	    >> RELOC_BITS3_SIZE_SH_LITTLE);

  /* Fill in the BFD arelent structure.  */
  code_sec = bfd_get_section_by_name (abfd, NLM_CODE_NAME);
  data_sec = bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
  if (r_extern)
    {
      /* External relocations are only used for imports.  */
      BFD_ASSERT (sym != NULL);
      /* We don't need to set sym_ptr_ptr for this case.  It is set in
	 nlm_canonicalize_reloc.  */
      rel->sym_ptr_ptr = NULL;
      rel->addend = 0;
    }
  else
    {
      /* Internal relocations are only used for local relocation
	 fixups.  If they are not NW_RELOC or GPDISP or IGNORE, they
	 must be against .text or .data.  */
      BFD_ASSERT (r_type == ALPHA_R_NW_RELOC || sym == NULL);
      if (r_type == ALPHA_R_NW_RELOC
	  || r_type == ALPHA_R_GPDISP
	  || r_type == ALPHA_R_IGNORE)
	{
	  rel->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	  rel->addend = 0;
	}
      else if (r_symndx == ALPHA_RELOC_SECTION_TEXT)
	{
	  rel->sym_ptr_ptr = code_sec->symbol_ptr_ptr;
	  BFD_ASSERT (bfd_get_section_vma (abfd, code_sec) == 0);
	  rel->addend = 0;
	}
      else if (r_symndx == ALPHA_RELOC_SECTION_DATA)
	{
	  rel->sym_ptr_ptr = data_sec->symbol_ptr_ptr;
	  rel->addend = - bfd_get_section_vma (abfd, data_sec);
	}
      else
	{
	  BFD_ASSERT (0);
	  rel->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	  rel->addend = 0;
	}
    }

  /* We use the address to determine whether the reloc is in the .text
     or .data section.  R_NW_RELOC relocs don't really have a section,
     so we put them in .text.  */
  if (r_type == ALPHA_R_NW_RELOC
      || r_vaddr < code_sec->size)
    {
      *secp = code_sec;
      rel->address = r_vaddr;
    }
  else
    {
      *secp = data_sec;
      rel->address = r_vaddr - code_sec->size;
    }

  /* We must adjust the addend based on the type.  */
  BFD_ASSERT ((r_type >= 0 && r_type <= ALPHA_R_GPVALUE)
	      || r_type == ALPHA_R_NW_RELOC);

  switch (r_type)
    {
    case ALPHA_R_BRADDR:
    case ALPHA_R_SREL16:
    case ALPHA_R_SREL32:
    case ALPHA_R_SREL64:
      /* The PC relative relocs do not seem to use the section VMA as
	 a negative addend.  */
      rel->addend = 0;
      break;

    case ALPHA_R_GPREL32:
      /* Copy the gp value for this object file into the addend, to
	 ensure that we are not confused by the linker.  */
      if (! r_extern)
	rel->addend += gp_value;
      break;

    case ALPHA_R_LITERAL:
      BFD_ASSERT (! r_extern);
      rel->addend += lita_address;
      break;

    case ALPHA_R_LITUSE:
    case ALPHA_R_GPDISP:
      /* The LITUSE and GPDISP relocs do not use a symbol, or an
	 addend, but they do use a special code.  Put this code in the
	 addend field.  */
      rel->addend = r_symndx;
      rel->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
      break;

    case ALPHA_R_OP_STORE:
      /* The STORE reloc needs the size and offset fields.  We store
	 them in the addend.  */
      BFD_ASSERT (r_offset < 256 && r_size < 256);
      rel->addend = (r_offset << 8) + r_size;
      break;

    case ALPHA_R_OP_PUSH:
    case ALPHA_R_OP_PSUB:
    case ALPHA_R_OP_PRSHIFT:
      /* The PUSH, PSUB and PRSHIFT relocs do not actually use an
	 address.  I believe that the address supplied is really an
	 addend.  */
      rel->addend = r_vaddr;
      break;

    case ALPHA_R_GPVALUE:
      /* Record the new gp value.  */
      gp_value += r_symndx;
      rel->addend = gp_value;
      break;

    case ALPHA_R_IGNORE:
      /* If the type is ALPHA_R_IGNORE, make sure this is a reference
	 to the absolute section so that the reloc is ignored.  For
	 some reason the address of this reloc type is not adjusted by
	 the section vma.  We record the gp value for this object file
	 here, for convenience when doing the GPDISP relocation.  */
      rel->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
      rel->address = r_vaddr;
      rel->addend = gp_value;
      break;

    case ALPHA_R_NW_RELOC:
      /* If this is SETGP, we set the addend to 0.  Otherwise we set
	 the addend to the size of the .lita section (this is
	 r_symndx) plus 1.  We have already set the address of the
	 reloc to r_vaddr.  */
      if (r_size == ALPHA_R_NW_RELOC_SETGP)
	{
	  gp_value = r_vaddr;
	  rel->addend = 0;
	}
      else if (r_size == ALPHA_R_NW_RELOC_LITA)
	{
	  lita_address = r_vaddr;
	  rel->addend = r_symndx + 1;
	}
      else
	BFD_ASSERT (0);
      rel->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
      break;

    default:
      break;
    }

  if (r_type == ALPHA_R_NW_RELOC)
    rel->howto = &nlm32_alpha_nw_howto;
  else
    rel->howto = &nlm32_alpha_howto_table[r_type];

  return TRUE;
}

/* Mangle Alpha NLM relocs for output.  */

static bfd_boolean
nlm_alpha_mangle_relocs (bfd *abfd ATTRIBUTE_UNUSED,
			 asection *sec ATTRIBUTE_UNUSED,
			 const void * data ATTRIBUTE_UNUSED,
			 bfd_vma offset ATTRIBUTE_UNUSED,
			 bfd_size_type count ATTRIBUTE_UNUSED)
{
  return TRUE;
}

/* Read an ALPHA NLM import record.  */

static bfd_boolean
nlm_alpha_read_import (bfd *abfd, nlmNAME (symbol_type) * sym)
{
  struct nlm_relent *nlm_relocs;	/* Relocation records for symbol.  */
  bfd_size_type rcount;			/* Number of relocs.  */
  bfd_byte temp[NLM_TARGET_LONG_SIZE];	/* Temporary 32-bit value.  */
  unsigned char symlength;		/* Length of symbol name.  */
  char *name;
  bfd_size_type amt;

  if (bfd_bread (& symlength, (bfd_size_type) sizeof (symlength), abfd)
      != sizeof (symlength))
    return FALSE;
  sym -> symbol.the_bfd = abfd;
  name = bfd_alloc (abfd, (bfd_size_type) symlength + 1);
  if (name == NULL)
    return FALSE;
  if (bfd_bread (name, (bfd_size_type) symlength, abfd) != symlength)
    return FALSE;
  name[symlength] = '\0';
  sym -> symbol.name = name;
  sym -> symbol.flags = 0;
  sym -> symbol.value = 0;
  sym -> symbol.section = bfd_und_section_ptr;
  if (bfd_bread (temp, (bfd_size_type) sizeof (temp), abfd)
      != sizeof (temp))
    return FALSE;
  rcount = H_GET_32 (abfd, temp);
  amt = rcount * sizeof (struct nlm_relent);
  nlm_relocs = bfd_alloc (abfd, amt);
  if (!nlm_relocs)
    return FALSE;
  sym -> relocs = nlm_relocs;
  sym -> rcnt = 0;
  while (sym -> rcnt < rcount)
    {
      asection *section;

      if (! nlm_alpha_read_reloc (abfd, sym, &section, &nlm_relocs -> reloc))
	return FALSE;
      nlm_relocs -> section = section;
      nlm_relocs++;
      sym -> rcnt++;
    }

  return TRUE;
}

/* Write an Alpha NLM reloc.  */

static bfd_boolean
nlm_alpha_write_import (bfd * abfd, asection * sec, arelent * rel)
{
  asymbol *sym;
  bfd_vma r_vaddr;
  long r_symndx;
  int r_type, r_extern, r_offset, r_size;
  struct nlm32_alpha_external_reloc ext;

  sym = *rel->sym_ptr_ptr;

  /* Get values for the relocation fields.  */
  r_type = rel->howto->type;
  if (r_type != ALPHA_R_NW_RELOC)
    {
      r_vaddr = bfd_get_section_vma (abfd, sec) + rel->address;
      if ((sec->flags & SEC_CODE) == 0)
	r_vaddr += bfd_get_section_by_name (abfd, NLM_CODE_NAME) -> size;
      if (bfd_is_und_section (bfd_get_section (sym)))
	{
	  r_extern = 1;
	  r_symndx = 0;
	}
      else
	{
	  r_extern = 0;
	  if (bfd_get_section_flags (abfd, bfd_get_section (sym)) & SEC_CODE)
	    r_symndx = ALPHA_RELOC_SECTION_TEXT;
	  else
	    r_symndx = ALPHA_RELOC_SECTION_DATA;
	}
      r_offset = 0;
      r_size = 0;

      switch (r_type)
	{
	case ALPHA_R_LITUSE:
	case ALPHA_R_GPDISP:
	  r_symndx = rel->addend;
	  break;

	case ALPHA_R_OP_STORE:
	  r_size = rel->addend & 0xff;
	  r_offset = (rel->addend >> 8) & 0xff;
	  break;

	case ALPHA_R_OP_PUSH:
	case ALPHA_R_OP_PSUB:
	case ALPHA_R_OP_PRSHIFT:
	  r_vaddr = rel->addend;
	  break;

	case ALPHA_R_IGNORE:
	  r_vaddr = rel->address;
	  break;

	default:
	  break;
	}
    }
  else
    {
      /* r_type == ALPHA_R_NW_RELOC.  */
      r_vaddr = rel->address;
      if (rel->addend == 0)
	{
	  r_symndx = 0;
	  r_size = ALPHA_R_NW_RELOC_SETGP;
	}
      else
	{
	  r_symndx = rel->addend - 1;
	  r_size = ALPHA_R_NW_RELOC_LITA;
	}
      r_extern = 0;
      r_offset = 0;
    }

  /* Swap out the relocation fields.  */
  H_PUT_64 (abfd, r_vaddr, ext.r_vaddr);
  H_PUT_32 (abfd, r_symndx, ext.r_symndx);

  BFD_ASSERT (bfd_little_endian (abfd));

  ext.r_bits[0] = ((r_type << RELOC_BITS0_TYPE_SH_LITTLE)
		   & RELOC_BITS0_TYPE_LITTLE);
  ext.r_bits[1] = ((r_extern ? RELOC_BITS1_EXTERN_LITTLE : 0)
		   | ((r_offset << RELOC_BITS1_OFFSET_SH_LITTLE)
		      & RELOC_BITS1_OFFSET_LITTLE));
  ext.r_bits[2] = 0;
  ext.r_bits[3] = ((r_size << RELOC_BITS3_SIZE_SH_LITTLE)
		   & RELOC_BITS3_SIZE_LITTLE);

  /* Write out the relocation.  */
  if (bfd_bwrite (&ext, (bfd_size_type) sizeof ext, abfd) != sizeof ext)
    return FALSE;

  return TRUE;
}

/* Alpha NetWare does not use the high bit to determine whether a
   public symbol is in the code segment or the data segment.  Instead,
   it just uses the address.  The set_public_section and
   get_public_offset routines override the default code which uses the
   high bit.  */

/* Set the section for a public symbol.  */

static bfd_boolean
nlm_alpha_set_public_section (bfd * abfd, nlmNAME (symbol_type) * sym)
{
  asection *code_sec, *data_sec;

  code_sec = bfd_get_section_by_name (abfd, NLM_CODE_NAME);
  data_sec = bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
  if (sym->symbol.value < code_sec->size)
    {
      sym->symbol.section = code_sec;
      sym->symbol.flags |= BSF_FUNCTION;
    }
  else
    {
      sym->symbol.section = data_sec;
      sym->symbol.value -= code_sec->size;
      /* The data segment had better be aligned.  */
      BFD_ASSERT ((code_sec->size & 0xf) == 0);
    }
  return TRUE;
}

/* Get the offset to write out for a public symbol.  */

static bfd_vma
nlm_alpha_get_public_offset (bfd * abfd ATTRIBUTE_UNUSED, asymbol * sym)
{
  return bfd_asymbol_value (sym);
}

/* Write an Alpha NLM external symbol.  */

static bfd_boolean
nlm_alpha_write_external (bfd *abfd,
			  bfd_size_type count,
			  asymbol *sym,
			  struct reloc_and_sec *relocs)
{
  bfd_size_type i;
  bfd_byte len;
  unsigned char temp[NLM_TARGET_LONG_SIZE];
  arelent r;

  len = strlen (sym->name);
  if ((bfd_bwrite (&len, (bfd_size_type) sizeof (bfd_byte), abfd)
       != sizeof (bfd_byte))
      || bfd_bwrite (sym->name, (bfd_size_type) len, abfd) != len)
    return FALSE;

  bfd_put_32 (abfd, count + 2, temp);
  if (bfd_bwrite (temp, (bfd_size_type) sizeof (temp), abfd) != sizeof (temp))
    return FALSE;

  /* The first two relocs for each external symbol are the .lita
     address and the GP value.  */
  r.sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
  r.howto = &nlm32_alpha_nw_howto;

  r.address = nlm_alpha_backend_data (abfd)->lita_address;
  r.addend = nlm_alpha_backend_data (abfd)->lita_size + 1;
  if (! nlm_alpha_write_import (abfd, NULL, &r))
    return FALSE;

  r.address = nlm_alpha_backend_data (abfd)->gp;
  r.addend = 0;
  if (! nlm_alpha_write_import (abfd, NULL, &r))
    return FALSE;

  for (i = 0; i < count; i++)
    if (! nlm_alpha_write_import (abfd, relocs[i].sec, relocs[i].rel))
      return FALSE;

  return TRUE;
}

#include "nlmswap.h"

static const struct nlm_backend_data nlm32_alpha_backend =
{
  "NetWare Alpha Module   \032",
  sizeof (Nlm32_alpha_External_Fixed_Header),
  sizeof (struct nlm32_alpha_external_prefix_header),
  bfd_arch_alpha,
  0,
  TRUE, /* No uninitialized data permitted by Alpha NetWare.  */
  nlm_alpha_backend_object_p,
  nlm_alpha_write_prefix,
  nlm_alpha_read_reloc,
  nlm_alpha_mangle_relocs,
  nlm_alpha_read_import,
  nlm_alpha_write_import,
  nlm_alpha_set_public_section,
  nlm_alpha_get_public_offset,
  nlm_swap_fixed_header_in,
  nlm_swap_fixed_header_out,
  nlm_alpha_write_external,
  0,	/* Write_export.  */
};

#define TARGET_LITTLE_NAME		"nlm32-alpha"
#define TARGET_LITTLE_SYM		nlmNAME (alpha_vec)
#define TARGET_BACKEND_DATA		& nlm32_alpha_backend

#include "nlm-target.h"
