/* Support for 32-bit i386 NLM (NetWare Loadable Module)
   Copyright (C) 1993 Free Software Foundation, Inc.

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

#define ARCH_SIZE 32

#include "nlm/i386-ext.h"
#define Nlm_External_Fixed_Header	Nlm32_i386_External_Fixed_Header

#include "libnlm.h"

static boolean nlm_i386_read_reloc
  PARAMS ((bfd *, nlmNAME(symbol_type) *, asection **, arelent *));
static boolean nlm_i386_write_import
  PARAMS ((bfd *, asection *, arelent *));
static boolean nlm_i386_mangle_relocs
  PARAMS ((bfd *, asection *, PTR, bfd_vma, bfd_size_type));
static boolean nlm_i386_read_import
  PARAMS ((bfd *, nlmNAME(symbol_type) *));
static boolean nlm_i386_write_external
  PARAMS ((bfd *, bfd_size_type, asymbol *, struct reloc_and_sec *));

/* Adjust the reloc location by an absolute value.  */

static reloc_howto_type nlm_i386_abs_howto =
  HOWTO (0,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "32",			/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false);		/* pcrel_offset */

/* Adjust the reloc location by a PC relative displacement.  */

static reloc_howto_type nlm_i386_pcrel_howto =
  HOWTO (1,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,			/* special_function */
	 "DISP32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 true);			/* pcrel_offset */

/* Read a NetWare i386 reloc.  */

static boolean
nlm_i386_read_reloc (abfd, sym, secp, rel)
     bfd *abfd;
     nlmNAME(symbol_type) *sym;
     asection **secp;
     arelent *rel;
{
  bfd_byte temp[4];
  bfd_vma val;
  const char *name;

  if (bfd_read (temp, sizeof (temp), 1, abfd) != sizeof (temp))
    return false;

  val = bfd_get_32 (abfd, temp);

  /* The value is an offset into either the code or data segment.
     This is the location which needs to be adjusted.

     If this is a relocation fixup rather than an imported symbol (the
     sym argument is NULL) then the high bit is 0 if the location
     needs to be adjusted by the address of the data segment, or 1 if
     the location needs to be adjusted by the address of the code
     segment.  If this is an imported symbol, then the high bit is 0
     if the location is 0 if the location should be adjusted by the
     offset to the symbol, or 1 if the location should adjusted by the
     absolute value of the symbol.

     The second most significant bit is 0 if the value is an offset
     into the data segment, or 1 if the value is an offset into the
     code segment.

     All this translates fairly easily into a BFD reloc.  */

  if (sym == NULL)
    {
      if ((val & NLM_HIBIT) == 0)
	name = NLM_INITIALIZED_DATA_NAME;
      else
	{
	  name = NLM_CODE_NAME;
	  val &=~ NLM_HIBIT;
	}
      rel->sym_ptr_ptr = bfd_get_section_by_name (abfd, name)->symbol_ptr_ptr;
      rel->howto = &nlm_i386_abs_howto;
    }
  else
    {
      /* In this case we do not need to set the sym_ptr_ptr field.  */
      rel->sym_ptr_ptr = NULL;
      if ((val & NLM_HIBIT) == 0)
	rel->howto = &nlm_i386_pcrel_howto;
      else
	{
	  rel->howto = &nlm_i386_abs_howto;
	  val &=~ NLM_HIBIT;
	}
    }

  if ((val & (NLM_HIBIT >> 1)) == 0)
    *secp = bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
  else
    {
      *secp = bfd_get_section_by_name (abfd, NLM_CODE_NAME);
      val &=~ (NLM_HIBIT >> 1);
    }

  rel->address = val;
  rel->addend = 0;

  return true;
}

/* Write a NetWare i386 reloc.  */

static boolean
nlm_i386_write_import (abfd, sec, rel)
     bfd *abfd;
     asection *sec;
     arelent *rel;
{
  asymbol *sym;
  bfd_vma val;
  bfd_byte temp[4];

  /* NetWare only supports two kinds of relocs.  We should check
     special_function here, as well, but at the moment coff-i386
     relocs uses a special_function which does not affect what we do
     here.  */
  if (rel->addend != 0
      || rel->howto == NULL
      || rel->howto->rightshift != 0
      || rel->howto->size != 2
      || rel->howto->bitsize != 32
      || rel->howto->bitpos != 0
      || rel->howto->src_mask != 0xffffffff
      || rel->howto->dst_mask != 0xffffffff)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  sym = *rel->sym_ptr_ptr;

  /* The value we write out is the offset into the appropriate
     segment.  This offset is the section vma, adjusted by the vma of
     the lowest section in that segment, plus the address of the
     relocation.  */
  val = bfd_get_section_vma (abfd, sec) + rel->address;

  /* The second most significant bit is 0 if the value is an offset
     into the data segment, or 1 if the value is an offset into the
     code segment.  */
  if (bfd_get_section_flags (abfd, sec) & SEC_CODE)
    {
      val -= nlm_get_text_low (abfd);
      val |= NLM_HIBIT >> 1;
    }
  else
    val -= nlm_get_data_low (abfd);

  if (! bfd_is_und_section (bfd_get_section (sym)))
    {
      /* NetWare only supports absolute internal relocs.  */
      if (rel->howto->pc_relative)
	{
	  bfd_set_error (bfd_error_invalid_operation);
	  return false;
	}

      /* The high bit is 1 if the reloc is against the code section, 0
	 if against the data section.  */
      if (bfd_get_section_flags (abfd, bfd_get_section (sym)) & SEC_CODE)
	val |= NLM_HIBIT;
    }
  else
    {
      /* The high bit is 1 if this is an absolute reloc, 0 if it is PC
	 relative.  */
      if (! rel->howto->pc_relative)
	val |= NLM_HIBIT;
      else
	{
	  /* PC relative relocs on NetWare must be pcrel_offset.  */
	  if (! rel->howto->pcrel_offset)
	    {
	      bfd_set_error (bfd_error_invalid_operation);
	      return false;
	    }
	}
    }
  
  bfd_put_32 (abfd, val, temp);
  if (bfd_write (temp, sizeof (temp), 1, abfd) != sizeof (temp))
    return false;

  return true;
}

/* I want to be able to use objcopy to turn a i386 a.out or COFF file
   into a NetWare i386 module.  That means that the relocs from the
   source file have to be mapped into relocs that apply to the target
   file.  This function is called by nlm_set_section_contents to give
   it a chance to rework the relocs.

   This is actually a fairly general concept.  However, this is not a
   general implementation.  */

static boolean
nlm_i386_mangle_relocs (abfd, sec, data, offset, count)
     bfd *abfd;
     asection *sec;
     PTR data;
     bfd_vma offset;
     bfd_size_type count;
{
  arelent **rel_ptr_ptr, **rel_end;

  rel_ptr_ptr = sec->orelocation;
  rel_end = rel_ptr_ptr + sec->reloc_count;
  for (; rel_ptr_ptr < rel_end; rel_ptr_ptr++)
    {
      arelent *rel;
      asymbol *sym;
      bfd_vma addend;

      rel = *rel_ptr_ptr;
      sym = *rel->sym_ptr_ptr;

      /* Note that no serious harm will ensue if we fail to change a
	 reloc.  We will wind up failing in nlm_i386_write_import.  */

      /* Make sure this reloc is within the data we have.  We only 4
	 byte relocs here, so we insist on having 4 bytes.  */
      if (rel->address < offset
	  || rel->address + 4 > offset + count)
	continue;

      /* NetWare doesn't support reloc addends, so we get rid of them
	 here by simply adding them into the object data.  We handle
	 the symbol value, if any, the same way.  */
      addend = rel->addend + sym->value;

      /* The value of a symbol is the offset into the section.  If the
	 symbol is in the .bss segment, we need to include the size of
	 the data segment in the offset as well.  Fortunately, we know
	 that at this point the size of the data section is in the NLM
	 header.  */
      if (((bfd_get_section_flags (abfd, bfd_get_section (sym))
	    & SEC_LOAD) == 0)
	  && ((bfd_get_section_flags (abfd, bfd_get_section (sym))
	       & SEC_ALLOC) != 0))
	addend += nlm_fixed_header (abfd)->dataImageSize;

      if (addend != 0
	  && rel->howto != NULL
	  && rel->howto->rightshift == 0
	  && rel->howto->size == 2
	  && rel->howto->bitsize == 32
	  && rel->howto->bitpos == 0
	  && rel->howto->src_mask == 0xffffffff
	  && rel->howto->dst_mask == 0xffffffff)
	{
	  bfd_vma val;

	  val = bfd_get_32 (abfd, (bfd_byte *) data + rel->address - offset);
	  val += addend;
	  bfd_put_32 (abfd, val, (bfd_byte *) data + rel->address - offset);
	  rel->addend = 0;
	}

      /* NetWare uses a reloc with pcrel_offset set.  We adjust
	 pc_relative relocs accordingly.  We are going to change the
	 howto field, so we can only do this if the current one is
	 compatible.  We should check special_function here, but at
	 the moment coff-i386 uses a special_function which does not
	 affect what we are doing here.  */
      if (rel->howto != NULL
	  && rel->howto->pc_relative
	  && ! rel->howto->pcrel_offset
	  && rel->howto->rightshift == 0
	  && rel->howto->size == 2
	  && rel->howto->bitsize == 32
	  && rel->howto->bitpos == 0
	  && rel->howto->src_mask == 0xffffffff
	  && rel->howto->dst_mask == 0xffffffff)
	{
	  bfd_vma val;

	  /* When pcrel_offset is not set, it means that the negative
	     of the address of the memory location is stored in the
	     memory location.  We must add it back in.  */
	  val = bfd_get_32 (abfd, (bfd_byte *) data + rel->address - offset);
	  val += rel->address;
	  bfd_put_32 (abfd, val, (bfd_byte *) data + rel->address - offset);

	  rel->howto = &nlm_i386_pcrel_howto;
	}
    }

  return true;
}

/* Read a NetWare i386 import record */
static boolean
nlm_i386_read_import (abfd, sym)
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
      
      if (nlm_i386_read_reloc (abfd, sym, &section,
			       &nlm_relocs -> reloc)
	  == false)
	return false;
      nlm_relocs -> section = section;
      nlm_relocs++;
      sym -> rcnt++;
    }
  return true;
}

/* Write out an external reference.  */

static boolean
nlm_i386_write_external (abfd, count, sym, relocs)
     bfd *abfd;
     bfd_size_type count;
     asymbol *sym;
     struct reloc_and_sec *relocs;
{
  unsigned int i;
  bfd_byte len;
  unsigned char temp[NLM_TARGET_LONG_SIZE];

  len = strlen (sym->name);
  if ((bfd_write (&len, sizeof (bfd_byte), 1, abfd) != sizeof(bfd_byte))
      || bfd_write (sym->name, len, 1, abfd) != len)
    return false;

  bfd_put_32 (abfd, count, temp);
  if (bfd_write (temp, sizeof(temp), 1, abfd) != sizeof (temp))
    return false;

  for (i = 0; i < count; i++)
    {
      if (nlm_i386_write_import (abfd, relocs[i].sec,
				 relocs[i].rel) == false)
	return false;
    }

  return true;
}

#include "nlmswap.h"

static const struct nlm_backend_data nlm32_i386_backend =
{
  "NetWare Loadable Module\032",
  sizeof (Nlm32_i386_External_Fixed_Header),
  0,	/* optional_prefix_size */
  bfd_arch_i386,
  0,
  false,
  0,	/* backend_object_p */
  0,	/* write_prefix_func */
  nlm_i386_read_reloc,
  nlm_i386_mangle_relocs,
  nlm_i386_read_import,
  nlm_i386_write_import,
  0,	/* set_public_section */
  0,	/* get_public_offset */
  nlm_swap_fixed_header_in,
  nlm_swap_fixed_header_out,
  nlm_i386_write_external,
  0,	/* write_export */
};

#define TARGET_LITTLE_NAME		"nlm32-i386"
#define TARGET_LITTLE_SYM		nlmNAME(i386_vec)
#define TARGET_BACKEND_DATA		&nlm32_i386_backend

#include "nlm-target.h"
