/* Support for 32-bit Alpha NLM (NetWare Loadable Module)
   Copyright (C) 1993 Free Software Foundation, Inc.
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

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

static boolean nlm_alpha_backend_object_p
  PARAMS ((bfd *));
static boolean nlm_alpha_write_prefix
  PARAMS ((bfd *));
static boolean nlm_alpha_read_reloc
  PARAMS ((bfd *, nlmNAME(symbol_type) *, asection **, arelent *));
static boolean nlm_alpha_mangle_relocs
  PARAMS ((bfd *, asection *, PTR, bfd_vma, bfd_size_type));
static boolean nlm_alpha_read_import
  PARAMS ((bfd *, nlmNAME(symbol_type) *));
static boolean nlm_alpha_write_import
  PARAMS ((bfd *, asection *, arelent *));
static boolean nlm_alpha_set_public_section
  PARAMS ((bfd *, nlmNAME(symbol_type) *));
static bfd_vma nlm_alpha_get_public_offset
  PARAMS ((bfd *, asymbol *));
static boolean nlm_alpha_write_external
  PARAMS ((bfd *, bfd_size_type, asymbol *, struct reloc_and_sec *));

/* Alpha NLM's have a prefix header before the standard NLM.  This
   function reads it in, verifies the version, and seeks the bfd to
   the location before the regular NLM header.  */

static boolean
nlm_alpha_backend_object_p (abfd)
     bfd *abfd;
{
  struct nlm32_alpha_external_prefix_header s;
  bfd_size_type size;

  if (bfd_read ((PTR) &s, sizeof s, 1, abfd) != sizeof s)
    return false;

  if (bfd_h_get_32 (abfd, s.magic) != NLM32_ALPHA_MAGIC)
    return false;

  /* FIXME: Should we check the format number?  */

  /* Skip to the end of the header.  */
  size = bfd_h_get_32 (abfd, s.size);
  if (bfd_seek (abfd, size, SEEK_SET) != 0)
    return false;

  return true;
}

/* Write out the prefix.  */

static boolean
nlm_alpha_write_prefix (abfd)
     bfd *abfd;
{
  struct nlm32_alpha_external_prefix_header s;

  memset (&s, 0, sizeof s);
  bfd_h_put_32 (abfd, (bfd_vma) NLM32_ALPHA_MAGIC, s.magic);
  bfd_h_put_32 (abfd, (bfd_vma) 2, s.format);
  bfd_h_put_32 (abfd, (bfd_vma) sizeof s, s.size);
  if (bfd_write ((PTR) &s, sizeof s, 1, abfd) != sizeof s)
    return false;
  return true;
}

/* How to process the various reloc types.  */

static reloc_howto_type nlm32_alpha_howto_table[] =
{
  /* Reloc type 0 is ignored by itself.  However, it appears after a
     GPDISP reloc to identify the location where the low order 16 bits
     of the gp register are loaded.  */
  HOWTO (ALPHA_R_IGNORE,	/* type */
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

  /* A 32 bit reference to a symbol.  */
  HOWTO (ALPHA_R_REFLONG,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "REFLONG",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit reference to a symbol.  */
  HOWTO (ALPHA_R_REFQUAD,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "REFQUAD",		/* name */
	 true,			/* partial_inplace */
	 0xffffffffffffffff,	/* src_mask */
	 0xffffffffffffffff,	/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 32 bit GP relative offset.  This is just like REFLONG except
     that when the value is used the value of the gp register will be
     added in.  */
  HOWTO (ALPHA_R_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "GPREL32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Used for an instruction that refers to memory off the GP
     register.  The offset is 16 bits of the 32 bit instruction.  This
     reloc always seems to be against the .lita section.  */
  HOWTO (ALPHA_R_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "LITERAL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* This reloc only appears immediately following a LITERAL reloc.
     It identifies a use of the literal.  It seems that the linker can
     use this to eliminate a portion of the .lita section.  The symbol
     index is special: 1 means the literal address is in the base
     register of a memory format instruction; 2 means the literal
     address is in the byte offset register of a byte-manipulation
     instruction; 3 means the literal address is in the target
     register of a jsr instruction.  This does not actually do any
     relocation.  */
  HOWTO (ALPHA_R_LITUSE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "LITUSE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

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
  HOWTO (ALPHA_R_GPDISP,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "GPDISP",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 21 bit branch.  The native assembler generates these for
     branches within the text segment, and also fills in the PC
     relative offset in the instruction.  It seems to me that this
     reloc, unlike the others, is not partial_inplace.  */
  HOWTO (ALPHA_R_BRADDR,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 21,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "BRADDR",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x1fffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A hint for a jump to a register.  */
  HOWTO (ALPHA_R_HINT,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 14,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "HINT",		/* name */
	 true,			/* partial_inplace */
	 0x3fff,		/* src_mask */
	 0x3fff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit PC relative offset.  */
  HOWTO (ALPHA_R_SREL64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "SREL64",		/* name */
	 true,			/* partial_inplace */
	 0xffffffffffffffff,	/* src_mask */
	 0xffffffffffffffff,	/* dst_mask */
	 false),		/* pcrel_offset */

  /* Push a value on the reloc evaluation stack.  */
  HOWTO (ALPHA_R_OP_PUSH,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "OP_PUSH",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Store the value from the stack at the given address.  Store it in
     a bitfield of size r_size starting at bit position r_offset.  */
  HOWTO (ALPHA_R_OP_STORE,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "OP_STORE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffffffffffff,	/* dst_mask */
	 false),		/* pcrel_offset */

  /* Subtract the reloc address from the value on the top of the
     relocation stack.  */
  HOWTO (ALPHA_R_OP_PSUB,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "OP_PSUB",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Shift the value on the top of the relocation stack right by the
     given value.  */
  HOWTO (ALPHA_R_OP_PRSHIFT,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			 /* special_function */
	 "OP_PRSHIFT",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Adjust the GP value for a new range in the object file.  */
  HOWTO (ALPHA_R_GPVALUE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "GPVALUE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false)			/* pcrel_offset */
};

static reloc_howto_type nlm32_alpha_nw_howto =
  HOWTO (ALPHA_R_NW_RELOC,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "NW_RELOC",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false);		/* pcrel_offset */

/* Read an Alpha NLM reloc.  This routine keeps some static data which
   it uses when handling local relocs.  This only works correctly
   because all the local relocs are read at once.  */

static boolean
nlm_alpha_read_reloc (abfd, sym, secp, rel)
     bfd *abfd;
     nlmNAME(symbol_type) *sym;
     asection **secp;
     arelent *rel;
{
  static bfd_vma gp_value;
  static bfd_vma lita_address;
  struct nlm32_alpha_external_reloc ext;
  bfd_vma r_vaddr;
  long r_symndx;
  int r_type, r_extern, r_offset, r_size;
  asection *code_sec, *data_sec;

  /* Read the reloc from the file.  */
  if (bfd_read (&ext, sizeof ext, 1, abfd) != sizeof ext)
    return false;

  /* Swap in the reloc information.  */
  r_vaddr = bfd_h_get_64 (abfd, (bfd_byte *) ext.r_vaddr);
  r_symndx = bfd_h_get_32 (abfd, (bfd_byte *) ext.r_symndx);

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
      || r_vaddr < bfd_section_size (abfd, code_sec))
    {
      *secp = code_sec;
      rel->address = r_vaddr;
    }
  else
    {
      *secp = data_sec;
      rel->address = r_vaddr - bfd_section_size (abfd, code_sec);
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

  return true;
}

/* Mangle Alpha NLM relocs for output.  */

static boolean
nlm_alpha_mangle_relocs (abfd, sec, data, offset, count)
     bfd *abfd;
     asection *sec;
     PTR data;
     bfd_vma offset;
     bfd_size_type count;
{
  return true;
}

/* Read an ALPHA NLM import record */

static boolean
nlm_alpha_read_import (abfd, sym)
     bfd *abfd;
     nlmNAME(symbol_type) *sym;
{
  struct nlm_relent *nlm_relocs;	/* relocation records for symbol */
  bfd_size_type rcount;			/* number of relocs */
  bfd_byte temp[NLM_TARGET_LONG_SIZE];	/* temporary 32-bit value */
  unsigned char symlength;		/* length of symbol name */
  char *name;

  if (bfd_read ((PTR) &symlength, sizeof (symlength), 1, abfd)
      != sizeof (symlength))
    return false;
  sym -> symbol.the_bfd = abfd;
  name = bfd_alloc (abfd, symlength + 1);
  if (name == NULL)
    return false;
  if (bfd_read (name, symlength, 1, abfd) != symlength)
    return false;
  name[symlength] = '\0';
  sym -> symbol.name = name;
  sym -> symbol.flags = 0;
  sym -> symbol.value = 0;
  sym -> symbol.section = bfd_und_section_ptr;
  if (bfd_read ((PTR) temp, sizeof (temp), 1, abfd) != sizeof (temp))
    return false;
  rcount = bfd_h_get_32 (abfd, temp);
  nlm_relocs = ((struct nlm_relent *)
		bfd_alloc (abfd, rcount * sizeof (struct nlm_relent)));
  if (!nlm_relocs)
    return false;
  sym -> relocs = nlm_relocs;
  sym -> rcnt = 0;
  while (sym -> rcnt < rcount)
    {
      asection *section;
      
      if (nlm_alpha_read_reloc (abfd, sym, &section,
				&nlm_relocs -> reloc)
	  == false)
	return false;
      nlm_relocs -> section = section;
      nlm_relocs++;
      sym -> rcnt++;
    }

  return true;
}

/* Write an Alpha NLM reloc.  */

static boolean
nlm_alpha_write_import (abfd, sec, rel)
     bfd *abfd;
     asection *sec;
     arelent *rel;
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
	r_vaddr += bfd_section_size (abfd,
				     bfd_get_section_by_name (abfd,
							      NLM_CODE_NAME));
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
      /* r_type == ALPHA_R_NW_RELOC */
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
  bfd_h_put_64 (abfd, r_vaddr, (bfd_byte *) ext.r_vaddr);
  bfd_h_put_32 (abfd, r_symndx, (bfd_byte *) ext.r_symndx);

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
  if (bfd_write (&ext, sizeof ext, 1, abfd) != sizeof ext)
    return false;

  return true;
}

/* Alpha NetWare does not use the high bit to determine whether a
   public symbol is in the code segment or the data segment.  Instead,
   it just uses the address.  The set_public_section and
   get_public_offset routines override the default code which uses the
   high bit.  */

/* Set the section for a public symbol.  */

static boolean
nlm_alpha_set_public_section (abfd, sym)
     bfd *abfd;
     nlmNAME(symbol_type) *sym;
{
  asection *code_sec, *data_sec;

  code_sec = bfd_get_section_by_name (abfd, NLM_CODE_NAME);
  data_sec = bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
  if (sym->symbol.value < bfd_section_size (abfd, code_sec))
    {
      sym->symbol.section = code_sec;
      sym->symbol.flags |= BSF_FUNCTION;
    }
  else
    {
      sym->symbol.section = data_sec;
      sym->symbol.value -= bfd_section_size (abfd, code_sec);
      /* The data segment had better be aligned.  */
      BFD_ASSERT ((bfd_section_size (abfd, code_sec) & 0xf) == 0);
    }
  return true;
}

/* Get the offset to write out for a public symbol.  */

static bfd_vma
nlm_alpha_get_public_offset (abfd, sym)
     bfd *abfd;
     asymbol *sym;
{
  return bfd_asymbol_value (sym);
}

/* Write an Alpha NLM external symbol.  */

static boolean
nlm_alpha_write_external (abfd, count, sym, relocs)
     bfd *abfd;
     bfd_size_type count;
     asymbol *sym;
     struct reloc_and_sec *relocs;
{
  int i;
  bfd_byte len;
  unsigned char temp[NLM_TARGET_LONG_SIZE];
  arelent r;

  len = strlen (sym->name);
  if ((bfd_write (&len, sizeof (bfd_byte), 1, abfd) != sizeof(bfd_byte))
      || bfd_write (sym->name, len, 1, abfd) != len)
    return false;

  bfd_put_32 (abfd, count + 2, temp);
  if (bfd_write (temp, sizeof (temp), 1, abfd) != sizeof (temp))
    return false;

  /* The first two relocs for each external symbol are the .lita
     address and the GP value.  */
  r.sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
  r.howto = &nlm32_alpha_nw_howto;

  r.address = nlm_alpha_backend_data (abfd)->lita_address;
  r.addend = nlm_alpha_backend_data (abfd)->lita_size + 1;
  if (nlm_alpha_write_import (abfd, (asection *) NULL, &r) == false)
    return false;

  r.address = nlm_alpha_backend_data (abfd)->gp;
  r.addend = 0;
  if (nlm_alpha_write_import (abfd, (asection *) NULL, &r) == false)
    return false;

  for (i = 0; i < count; i++)
    {
      if (nlm_alpha_write_import (abfd, relocs[i].sec,
				  relocs[i].rel) == false)
	return false;
    }

  return true;
}

#include "nlmswap.h"

static const struct nlm_backend_data nlm32_alpha_backend =
{
  "NetWare Alpha Module   \032",
  sizeof (Nlm32_alpha_External_Fixed_Header),
  sizeof (struct nlm32_alpha_external_prefix_header),
  bfd_arch_alpha,
  0,
  true, /* no uninitialized data permitted by Alpha NetWare.  */
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
  0,	/* write_export */
};

#define TARGET_LITTLE_NAME		"nlm32-alpha"
#define TARGET_LITTLE_SYM		nlmNAME(alpha_vec)
#define TARGET_BACKEND_DATA		&nlm32_alpha_backend

#include "nlm-target.h"
