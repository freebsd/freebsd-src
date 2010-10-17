/* Support for 32-bit PowerPC NLM (NetWare Loadable Module)
   Copyright 1994, 1995, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

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

/* The format of a PowerPC NLM changed.  Define OLDFORMAT to get the
   old format.  */

#define ARCH_SIZE 32

#include "nlm/ppc-ext.h"
#define Nlm_External_Fixed_Header	Nlm32_powerpc_External_Fixed_Header

#include "libnlm.h"

#ifdef OLDFORMAT
static bfd_boolean nlm_powerpc_backend_object_p
  PARAMS ((bfd *));
static bfd_boolean nlm_powerpc_write_prefix
  PARAMS ((bfd *));
#endif

static bfd_boolean nlm_powerpc_read_reloc
  PARAMS ((bfd *, nlmNAME(symbol_type) *, asection **, arelent *));
static bfd_boolean nlm_powerpc_mangle_relocs
  PARAMS ((bfd *, asection *, const PTR, bfd_vma, bfd_size_type));
static bfd_boolean nlm_powerpc_read_import
  PARAMS ((bfd *, nlmNAME(symbol_type) *));

#ifdef OLDFORMAT
static bfd_boolean nlm_powerpc_write_reloc
  PARAMS ((bfd *, asection *, arelent *, int));
#endif

static bfd_boolean nlm_powerpc_write_import
  PARAMS ((bfd *, asection *, arelent *));
static bfd_boolean nlm_powerpc_write_external
  PARAMS ((bfd *, bfd_size_type, asymbol *, struct reloc_and_sec *));

#ifndef OLDFORMAT
static bfd_boolean nlm_powerpc_set_public_section
  PARAMS ((bfd *, nlmNAME(symbol_type) *));
static bfd_vma nlm_powerpc_get_public_offset
  PARAMS ((bfd *, asymbol *));
#endif

#ifdef OLDFORMAT

/* The prefix header is only used in the old format.  */

/* PowerPC NLM's have a prefix header before the standard NLM.  This
   function reads it in, verifies the version, and seeks the bfd to
   the location before the regular NLM header.  */

static bfd_boolean
nlm_powerpc_backend_object_p (abfd)
     bfd *abfd;
{
  struct nlm32_powerpc_external_prefix_header s;

  if (bfd_bread ((PTR) &s, (bfd_size_type) sizeof s, abfd) != sizeof s)
    return FALSE;

  if (memcmp (s.signature, NLM32_POWERPC_SIGNATURE, sizeof s.signature) != 0
      || H_GET_32 (abfd, s.headerVersion) != NLM32_POWERPC_HEADER_VERSION)
    return FALSE;

  return TRUE;
}

/* Write out the prefix.  */

static bfd_boolean
nlm_powerpc_write_prefix (abfd)
     bfd *abfd;
{
  struct nlm32_powerpc_external_prefix_header s;

  memset (&s, 0, sizeof s);
  memcpy (s.signature, NLM32_POWERPC_SIGNATURE, sizeof s.signature);
  H_PUT_32 (abfd, NLM32_POWERPC_HEADER_VERSION, s.headerVersion);
  H_PUT_32 (abfd, 0, s.origins);

  /* FIXME: What should we do about the date?  */

  if (bfd_bwrite ((PTR) &s, (bfd_size_type) sizeof s, abfd) != sizeof s)
    return FALSE;

  return TRUE;
}

#endif /* OLDFORMAT */

#ifndef OLDFORMAT

/* There is only one type of reloc in a PowerPC NLM.  */

static reloc_howto_type nlm_powerpc_howto =
  HOWTO (0,			/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,			/* special_function */
	 "32",			/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE);		/* pcrel_offset */

/* Read a PowerPC NLM reloc.  */

static bfd_boolean
nlm_powerpc_read_reloc (abfd, sym, secp, rel)
     bfd *abfd;
     nlmNAME(symbol_type) *sym;
     asection **secp;
     arelent *rel;
{
  bfd_byte temp[4];
  bfd_vma val;
  const char *name;

  if (bfd_bread (temp, (bfd_size_type) sizeof (temp), abfd) != sizeof (temp))
    return FALSE;

  val = bfd_get_32 (abfd, temp);

  /* The value is a word offset into either the code or data segment.
     This is the location which needs to be adjusted.

     The high bit is 0 if the value is an offset into the data
     segment, or 1 if the value is an offset into the text segment.

     If this is a relocation fixup rather than an imported symbol (the
     sym argument is NULL), then the second most significant bit is 0
     if the address of the data segment should be added to the
     location addressed by the value, or 1 if the address of the text
     segment should be added.

     If this is an imported symbol, the second most significant bit is
     not used and must be 0.  */

  if ((val & NLM_HIBIT) == 0)
    name = NLM_INITIALIZED_DATA_NAME;
  else
    {
      name = NLM_CODE_NAME;
      val &=~ NLM_HIBIT;
    }
  *secp = bfd_get_section_by_name (abfd, name);

  if (sym == NULL)
    {
      if ((val & (NLM_HIBIT >> 1)) == 0)
	name = NLM_INITIALIZED_DATA_NAME;
      else
	{
	  name = NLM_CODE_NAME;
	  val &=~ (NLM_HIBIT >> 1);
	}
      rel->sym_ptr_ptr = bfd_get_section_by_name (abfd, name)->symbol_ptr_ptr;
    }

  rel->howto = &nlm_powerpc_howto;

  rel->address = val << 2;
  rel->addend = 0;

  return TRUE;
}

#else /* OLDFORMAT */

/* This reloc handling is only applicable to the old format.  */

/* How to process the various reloc types.  PowerPC NLMs use XCOFF
   reloc types, and I have just copied the XCOFF reloc table here.  */

static reloc_howto_type nlm_powerpc_howto_table[] =
{
  /* Standard 32 bit relocation.  */
  HOWTO (0,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_POS",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* 32 bit relocation, but store negative value.  */
  HOWTO (1,	                /* type */
	 0,	                /* rightshift */
	 -2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_NEG",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* 32 bit PC relative relocation.  */
  HOWTO (2,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 TRUE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_REL",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* 16 bit TOC relative relocation.  */
  HOWTO (3,	                /* type */
	 0,	                /* rightshift */
	 1,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_TOC",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* I don't really know what this is.  */
  HOWTO (4,	                /* type */
	 1,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RTB",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffffffff,	        /* src_mask */
	 0xffffffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* External TOC relative symbol.  */
  HOWTO (5,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_GL",                /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* Local TOC relative symbol.  */
  HOWTO (6,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_TCL",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  { 7 },

  /* Non modifiable absolute branch.  */
  HOWTO (8,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 26,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_BA",                /* name */
	 TRUE,	                /* partial_inplace */
	 0x3fffffc,	        /* src_mask */
	 0x3fffffc,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  { 9 },

  /* Non modifiable relative branch.  */
  HOWTO (0xa,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 26,	                /* bitsize */
	 TRUE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_BR",                /* name */
	 TRUE,	                /* partial_inplace */
	 0x3fffffc,	        /* src_mask */
	 0x3fffffc,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  { 0xb },

  /* Indirect load.  */
  HOWTO (0xc,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RL",                /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* Load address.  */
  HOWTO (0xd,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RLA",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  { 0xe },

  /* Non-relocating reference.  */
  HOWTO (0xf,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_REF",               /* name */
	 FALSE,	                /* partial_inplace */
	 0,		        /* src_mask */
	 0,     	   	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  { 0x10 },
  { 0x11 },

  /* TOC relative indirect load.  */
  HOWTO (0x12,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_TRL",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* TOC relative load address.  */
  HOWTO (0x13,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_TRLA",              /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* Modifiable relative branch.  */
  HOWTO (0x14,	                /* type */
	 1,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RRTBI",             /* name */
	 TRUE,	                /* partial_inplace */
	 0xffffffff,	        /* src_mask */
	 0xffffffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* Modifiable absolute branch.  */
  HOWTO (0x15,	                /* type */
	 1,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RRTBA",             /* name */
	 TRUE,	                /* partial_inplace */
	 0xffffffff,	        /* src_mask */
	 0xffffffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* Modifiable call absolute indirect.  */
  HOWTO (0x16,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_CAI",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* Modifiable call relative.  */
  HOWTO (0x17,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_REL",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* Modifiable branch absolute.  */
  HOWTO (0x18,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RBA",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* Modifiable branch absolute.  */
  HOWTO (0x19,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_RBAC",              /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* Modifiable branch relative.  */
  HOWTO (0x1a,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 26,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_REL",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE),                /* pcrel_offset */

  /* Modifiable branch absolute.  */
  HOWTO (0x1b,	                /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 16,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 0,		        /* special_function */
	 "R_REL",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffff,	        /* src_mask */
	 0xffff,        	/* dst_mask */
	 FALSE)                 /* pcrel_offset */
};

#define HOWTO_COUNT (sizeof nlm_powerpc_howto_table		\
		     / sizeof nlm_powerpc_howto_table[0])

/* Read a PowerPC NLM reloc.  */

static bfd_boolean
nlm_powerpc_read_reloc (abfd, sym, secp, rel)
     bfd *abfd;
     nlmNAME(symbol_type) *sym;
     asection **secp;
     arelent *rel;
{
  struct nlm32_powerpc_external_reloc ext;
  bfd_vma l_vaddr;
  unsigned long l_symndx;
  int l_rtype;
  int l_rsecnm;
  asection *code_sec, *data_sec, *bss_sec;

  /* Read the reloc from the file.  */
  if (bfd_bread (&ext, (bfd_size_type) sizeof ext, abfd) != sizeof ext)
    return FALSE;

  /* Swap in the fields.  */
  l_vaddr = H_GET_32 (abfd, ext.l_vaddr);
  l_symndx = H_GET_32 (abfd, ext.l_symndx);
  l_rtype = H_GET_16 (abfd, ext.l_rtype);
  l_rsecnm = H_GET_16 (abfd, ext.l_rsecnm);

  /* Get the sections now, for convenience.  */
  code_sec = bfd_get_section_by_name (abfd, NLM_CODE_NAME);
  data_sec = bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
  bss_sec = bfd_get_section_by_name (abfd, NLM_UNINITIALIZED_DATA_NAME);

  /* Work out the arelent fields.  */
  if (sym != NULL)
    {
      /* This is an import.  sym_ptr_ptr is filled in by
	 nlm_canonicalize_reloc.  */
      rel->sym_ptr_ptr = NULL;
    }
  else
    {
      asection *sec;

      if (l_symndx == 0)
	sec = code_sec;
      else if (l_symndx == 1)
	sec = data_sec;
      else if (l_symndx == 2)
	sec = bss_sec;
      else
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}

      rel->sym_ptr_ptr = sec->symbol_ptr_ptr;
    }

  rel->addend = 0;

  BFD_ASSERT ((l_rtype & 0xff) < HOWTO_COUNT);

  rel->howto = nlm_powerpc_howto_table + (l_rtype & 0xff);

  BFD_ASSERT (rel->howto->name != NULL
	      && ((l_rtype & 0x8000) != 0
		  ? (rel->howto->complain_on_overflow
		     == complain_overflow_signed)
		  : (rel->howto->complain_on_overflow
		     == complain_overflow_bitfield))
	      && ((l_rtype >> 8) & 0x1f) == rel->howto->bitsize - 1);

  if (l_rsecnm == 0)
    *secp = code_sec;
  else if (l_rsecnm == 1)
    {
      *secp = data_sec;
      l_vaddr -= bfd_section_size (abfd, code_sec);
    }
  else
    {
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  rel->address = l_vaddr;

  return TRUE;
}

#endif /* OLDFORMAT */

/* Mangle PowerPC NLM relocs for output.  */

static bfd_boolean
nlm_powerpc_mangle_relocs (abfd, sec, data, offset, count)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const PTR data ATTRIBUTE_UNUSED;
     bfd_vma offset ATTRIBUTE_UNUSED;
     bfd_size_type count ATTRIBUTE_UNUSED;
{
  return TRUE;
}

/* Read a PowerPC NLM import record */

static bfd_boolean
nlm_powerpc_read_import (abfd, sym)
     bfd *abfd;
     nlmNAME(symbol_type) *sym;
{
  struct nlm_relent *nlm_relocs;	/* relocation records for symbol */
  bfd_size_type rcount;			/* number of relocs */
  bfd_byte temp[NLM_TARGET_LONG_SIZE];	/* temporary 32-bit value */
  unsigned char symlength;		/* length of symbol name */
  char *name;

  if (bfd_bread ((PTR) &symlength, (bfd_size_type) sizeof (symlength), abfd)
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
  if (bfd_bread ((PTR) temp, (bfd_size_type) sizeof (temp), abfd)
      != sizeof (temp))
    return FALSE;
  rcount = H_GET_32 (abfd, temp);
  nlm_relocs = ((struct nlm_relent *)
		bfd_alloc (abfd, rcount * sizeof (struct nlm_relent)));
  if (nlm_relocs == (struct nlm_relent *) NULL)
    return FALSE;
  sym -> relocs = nlm_relocs;
  sym -> rcnt = 0;
  while (sym -> rcnt < rcount)
    {
      asection *section;

      if (! nlm_powerpc_read_reloc (abfd, sym, &section, &nlm_relocs -> reloc))
	return FALSE;
      nlm_relocs -> section = section;
      nlm_relocs++;
      sym -> rcnt++;
    }
  return TRUE;
}

#ifndef OLDFORMAT

/* Write a PowerPC NLM reloc.  */

static bfd_boolean
nlm_powerpc_write_import (abfd, sec, rel)
     bfd *abfd;
     asection *sec;
     arelent *rel;
{
  asymbol *sym;
  bfd_vma val;
  bfd_byte temp[4];

  /* PowerPC NetWare only supports one kind of reloc.  */
  if (rel->addend != 0
      || rel->howto == NULL
      || rel->howto->rightshift != 0
      || rel->howto->size != 2
      || rel->howto->bitsize != 32
      || rel->howto->bitpos != 0
      || rel->howto->pc_relative
      || (rel->howto->src_mask != 0xffffffff && rel->addend != 0)
      || rel->howto->dst_mask != 0xffffffff)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return FALSE;
    }

  sym = *rel->sym_ptr_ptr;

  /* The value we write out is the offset into the appropriate
     segment, rightshifted by two.  This offset is the section vma,
     adjusted by the vma of the lowest section in that segment, plus
     the address of the relocation.  */
  val = bfd_get_section_vma (abfd, sec) + rel->address;
  if ((val & 3) != 0)
    {
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }
  val >>= 2;

  /* The high bit is 0 if the reloc is in the data section, or 1 if
     the reloc is in the code section.  */
  if (bfd_get_section_flags (abfd, sec) & SEC_DATA)
    val -= nlm_get_data_low (abfd);
  else
    {
      val -= nlm_get_text_low (abfd);
      val |= NLM_HIBIT;
    }

  if (! bfd_is_und_section (bfd_get_section (sym)))
    {
      /* This is an internal relocation fixup.  The second most
	 significant bit is 0 if this is a reloc against the data
	 segment, or 1 if it is a reloc against the text segment.  */
      if (bfd_get_section_flags (abfd, bfd_get_section (sym)) & SEC_CODE)
	val |= NLM_HIBIT >> 1;
    }

  bfd_put_32 (abfd, val, temp);
  if (bfd_bwrite (temp, (bfd_size_type) sizeof (temp), abfd) != sizeof (temp))
    return FALSE;

  return TRUE;
}

#else /* OLDFORMAT */

/* This is used for the reloc handling in the old format.  */

/* Write a PowerPC NLM reloc.  */

static bfd_boolean
nlm_powerpc_write_reloc (abfd, sec, rel, indx)
     bfd *abfd;
     asection *sec;
     arelent *rel;
     int indx;
{
  struct nlm32_powerpc_external_reloc ext;
  asection *code_sec, *data_sec, *bss_sec;
  asymbol *sym;
  asection *symsec;
  unsigned long l_symndx;
  int l_rtype;
  int l_rsecnm;
  reloc_howto_type *howto;
  bfd_size_type address;

  /* Get the sections now, for convenience.  */
  code_sec = bfd_get_section_by_name (abfd, NLM_CODE_NAME);
  data_sec = bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
  bss_sec = bfd_get_section_by_name (abfd, NLM_UNINITIALIZED_DATA_NAME);

  sym = *rel->sym_ptr_ptr;
  symsec = bfd_get_section (sym);
  if (indx != -1)
    {
      BFD_ASSERT (bfd_is_und_section (symsec));
      l_symndx = indx + 3;
    }
  else
    {
      if (symsec == code_sec)
	l_symndx = 0;
      else if (symsec == data_sec)
	l_symndx = 1;
      else if (symsec == bss_sec)
	l_symndx = 2;
      else
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}
    }

  H_PUT_32 (abfd, l_symndx, ext.l_symndx);

  for (howto = nlm_powerpc_howto_table;
       howto < nlm_powerpc_howto_table + HOWTO_COUNT;
       howto++)
    {
      if (howto->rightshift == rel->howto->rightshift
	  && howto->size == rel->howto->size
	  && howto->bitsize == rel->howto->bitsize
	  && howto->pc_relative == rel->howto->pc_relative
	  && howto->bitpos == rel->howto->bitpos
	  && (howto->partial_inplace == rel->howto->partial_inplace
	      || (! rel->howto->partial_inplace
		  && rel->addend == 0))
	  && (howto->src_mask == rel->howto->src_mask
	      || (rel->howto->src_mask == 0
		  && rel->addend == 0))
	  && howto->dst_mask == rel->howto->dst_mask
	  && howto->pcrel_offset == rel->howto->pcrel_offset)
	break;
    }
  if (howto >= nlm_powerpc_howto_table + HOWTO_COUNT)
    {
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  l_rtype = howto->type;
  if (howto->complain_on_overflow == complain_overflow_signed)
    l_rtype |= 0x8000;
  l_rtype |= (howto->bitsize - 1) << 8;
  H_PUT_16 (abfd, l_rtype, ext.l_rtype);

  address = rel->address;

  if (sec == code_sec)
    l_rsecnm = 0;
  else if (sec == data_sec)
    {
      l_rsecnm = 1;
      address += bfd_section_size (abfd, code_sec);
    }
  else
    {
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  H_PUT_16 (abfd, l_rsecnm, ext.l_rsecnm);
  H_PUT_32 (abfd, address, ext.l_vaddr);

  if (bfd_bwrite (&ext, (bfd_size_type) sizeof ext, abfd) != sizeof ext)
    return FALSE;

  return TRUE;
}

/* Write a PowerPC NLM import.  */

static bfd_boolean
nlm_powerpc_write_import (abfd, sec, rel)
     bfd *abfd;
     asection *sec;
     arelent *rel;
{
  return nlm_powerpc_write_reloc (abfd, sec, rel, -1);
}

#endif /* OLDFORMAT */

/* Write a PowerPC NLM external symbol.  This routine keeps a static
   count of the symbol index.  FIXME: I don't know if this is
   necessary, and the index never gets reset.  */

static bfd_boolean
nlm_powerpc_write_external (abfd, count, sym, relocs)
     bfd *abfd;
     bfd_size_type count;
     asymbol *sym;
     struct reloc_and_sec *relocs;
{
  unsigned int i;
  bfd_byte len;
  unsigned char temp[NLM_TARGET_LONG_SIZE];
#ifdef OLDFORMAT
  static int indx;
#endif

  len = strlen (sym->name);
  if ((bfd_bwrite (&len, (bfd_size_type) sizeof (bfd_byte), abfd)
       != sizeof (bfd_byte))
      || bfd_bwrite (sym->name, (bfd_size_type) len, abfd) != len)
    return FALSE;

  bfd_put_32 (abfd, count, temp);
  if (bfd_bwrite (temp, (bfd_size_type) sizeof (temp), abfd) != sizeof (temp))
    return FALSE;

  for (i = 0; i < count; i++)
    {
#ifndef OLDFORMAT
      if (! nlm_powerpc_write_import (abfd, relocs[i].sec, relocs[i].rel))
	return FALSE;
#else
      if (! nlm_powerpc_write_reloc (abfd, relocs[i].sec,
				     relocs[i].rel, indx))
	return FALSE;
#endif
    }

#ifdef OLDFORMAT
  ++indx;
#endif

  return TRUE;
}

#ifndef OLDFORMAT

/* PowerPC Netware uses a word offset, not a byte offset, for public
   symbols.  */

/* Set the section for a public symbol.  */

static bfd_boolean
nlm_powerpc_set_public_section (abfd, sym)
     bfd *abfd;
     nlmNAME(symbol_type) *sym;
{
  if (sym->symbol.value & NLM_HIBIT)
    {
      sym->symbol.value &= ~NLM_HIBIT;
      sym->symbol.flags |= BSF_FUNCTION;
      sym->symbol.section =
	bfd_get_section_by_name (abfd, NLM_CODE_NAME);
    }
  else
    {
      sym->symbol.section =
	bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
    }

  sym->symbol.value <<= 2;

  return TRUE;
}

/* Get the offset to write out for a public symbol.  */

static bfd_vma
nlm_powerpc_get_public_offset (abfd, sym)
     bfd *abfd;
     asymbol *sym;
{
  bfd_vma offset;
  asection *sec;

  offset = bfd_asymbol_value (sym);
  sec = bfd_get_section (sym);
  if (sec->flags & SEC_CODE)
    {
      offset -= nlm_get_text_low (abfd);
      offset |= NLM_HIBIT;
    }
  else if (sec->flags & (SEC_DATA | SEC_ALLOC))
    {
      /* SEC_ALLOC is for the .bss section.  */
      offset -= nlm_get_data_low (abfd);
    }
  else
    {
      /* We can't handle an exported symbol that is not in the code or
	 data segment.  */
      bfd_set_error (bfd_error_invalid_operation);
      /* FIXME: No way to return error.  */
      abort ();
    }

  return offset;
}

#endif /* ! defined (OLDFORMAT) */

#include "nlmswap.h"

static const struct nlm_backend_data nlm32_powerpc_backend =
{
  "NetWare PowerPC Module \032",
  sizeof (Nlm32_powerpc_External_Fixed_Header),
#ifndef OLDFORMAT
  0,	/* optional_prefix_size */
#else
  sizeof (struct nlm32_powerpc_external_prefix_header),
#endif
  bfd_arch_powerpc,
  0,
  FALSE,
#ifndef OLDFORMAT
  0,	/* backend_object_p */
  0,	/* write_prefix */
#else
  nlm_powerpc_backend_object_p,
  nlm_powerpc_write_prefix,
#endif
  nlm_powerpc_read_reloc,
  nlm_powerpc_mangle_relocs,
  nlm_powerpc_read_import,
  nlm_powerpc_write_import,
#ifndef OLDFORMAT
  nlm_powerpc_set_public_section,
  nlm_powerpc_get_public_offset,
#else
  0,	/* set_public_section */
  0,	/* get_public_offset */
#endif
  nlm_swap_fixed_header_in,
  nlm_swap_fixed_header_out,
  nlm_powerpc_write_external,
  0,	/* write_export */
};

#define TARGET_BIG_NAME			"nlm32-powerpc"
#define TARGET_BIG_SYM			nlmNAME(powerpc_vec)
#define TARGET_BACKEND_DATA		&nlm32_powerpc_backend

#include "nlm-target.h"
