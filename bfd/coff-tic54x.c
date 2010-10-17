/* BFD back-end for TMS320C54X coff binaries.
   Copyright 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
   Contributed by Timothy Wall (twall@cygnus.com)

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "coff/tic54x.h"
#include "coff/internal.h"
#include "libcoff.h"

#undef  F_LSYMS
#define	F_LSYMS		F_LSYMS_TICOFF

static void tic54x_reloc_processing
  PARAMS ((arelent *, struct internal_reloc *, asymbol **, bfd *, asection *));
static bfd_reloc_status_type tic54x_relocation
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_boolean tic54x_set_section_contents
  PARAMS ((bfd *, sec_ptr, const PTR, file_ptr, bfd_size_type));
static reloc_howto_type *coff_tic54x_rtype_to_howto
  PARAMS ((bfd *, asection *, struct internal_reloc *, struct coff_link_hash_entry *, struct internal_syment *, bfd_vma *));
static bfd_boolean tic54x_set_arch_mach
  PARAMS ((bfd *, enum bfd_architecture, unsigned long));
static reloc_howto_type * tic54x_coff_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void tic54x_lookup_howto
  PARAMS ((arelent *, struct internal_reloc *));
static bfd_boolean ticoff_bfd_is_local_label_name
  PARAMS ((bfd *, const char *));

/* 32-bit operations
   The octet order is screwy.  words are LSB first (LS octet, actually), but
   longwords are MSW first.  For example, 0x12345678 is encoded 0x5678 in the
   first word and 0x1234 in the second.  When looking at the data as stored in
   the COFF file, you would see the octets ordered as 0x78, 0x56, 0x34, 0x12.
   Don't bother with 64-bits, as there aren't any.  */

static bfd_vma
tic54x_getl32 (const void *p)
{
  const bfd_byte *addr = p;
  unsigned long v;

  v  = (unsigned long) addr[2];
  v |= (unsigned long) addr[3] << 8;
  v |= (unsigned long) addr[0] << 16;
  v |= (unsigned long) addr[1] << 24;
  return v;
}

static void
tic54x_putl32 (bfd_vma data, void *p)
{
  bfd_byte *addr = p;
  addr[2] = data & 0xff;
  addr[3] = (data >>  8) & 0xff;
  addr[0] = (data >> 16) & 0xff;
  addr[1] = (data >> 24) & 0xff;
}

static bfd_signed_vma
tic54x_getl_signed_32 (const void *p)
{
  const bfd_byte *addr = p;
  unsigned long v;

  v  = (unsigned long) addr[2];
  v |= (unsigned long) addr[3] << 8;
  v |= (unsigned long) addr[0] << 16;
  v |= (unsigned long) addr[1] << 24;
#define COERCE32(x) \
  ((bfd_signed_vma) (long) (((unsigned long) (x) ^ 0x80000000) - 0x80000000))
  return COERCE32 (v);
}

#define coff_get_section_load_page bfd_ticoff_get_section_load_page
#define coff_set_section_load_page bfd_ticoff_set_section_load_page

void
bfd_ticoff_set_section_load_page (sect, page)
  asection *sect;
  int page;
{
  sect->lma = (sect->lma & ADDR_MASK) | PG_TO_FLAG(page);
}

int
bfd_ticoff_get_section_load_page (sect)
  asection *sect;
{
  int page;

  /* Provide meaningful defaults for predefined sections.  */
  if (sect == &bfd_com_section)
    page = PG_DATA;

  else if (sect == &bfd_und_section
      || sect == &bfd_abs_section
      || sect == &bfd_ind_section)
    page = PG_PROG;

  else
    page = FLAG_TO_PG (sect->lma);

  return page;
}

/* Set the architecture appropriately.  Allow unkown architectures
   (e.g. binary).  */

static bfd_boolean
tic54x_set_arch_mach (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  if (arch == bfd_arch_unknown)
    arch = bfd_arch_tic54x;

  else if (arch != bfd_arch_tic54x)
    return FALSE;

  return bfd_default_set_arch_mach (abfd, arch, machine);
}

static bfd_reloc_status_type
tic54x_relocation (abfd, reloc_entry, symbol, data, input_section,
                   output_bfd, error_message)
  bfd *abfd ATTRIBUTE_UNUSED;
  arelent *reloc_entry;
  asymbol *symbol ATTRIBUTE_UNUSED;
  PTR data ATTRIBUTE_UNUSED;
  asection *input_section;
  bfd *output_bfd;
  char **error_message ATTRIBUTE_UNUSED;
{
  if (output_bfd != (bfd *) NULL)
    {
      /* This is a partial relocation, and we want to apply the
 	 relocation to the reloc entry rather than the raw data.
 	 Modify the reloc inplace to reflect what we now know.  */
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }
  return bfd_reloc_continue;
}

reloc_howto_type tic54x_howto_table[] =
  {
    /* type,rightshift,size (0=byte, 1=short, 2=long),
       bit size, pc_relative, bitpos, dont complain_on_overflow,
       special_function, name, partial_inplace, src_mask, dst_mask, pcrel_offset.  */

    /* NORMAL BANK */
    /* 16-bit direct reference to symbol's address.  */
    HOWTO (R_RELWORD,0,1,16,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"REL16",FALSE,0xFFFF,0xFFFF,FALSE),

    /* 7 LSBs of an address */
    HOWTO (R_PARTLS7,0,1,7,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"LS7",FALSE,0x007F,0x007F,FALSE),

    /* 9 MSBs of an address */
    /* TI assembler doesn't shift its encoding, and is thus incompatible */
    HOWTO (R_PARTMS9,7,1,9,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"MS9",FALSE,0x01FF,0x01FF,FALSE),

    /* 23-bit relocation */
    HOWTO (R_EXTWORD,0,2,23,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"RELEXT",FALSE,0x7FFFFF,0x7FFFFF,FALSE),

    /* 16 bits of 23-bit extended address */
    HOWTO (R_EXTWORD16,0,1,16,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"RELEXT16",FALSE,0x7FFFFF,0x7FFFFF,FALSE),

    /* upper 7 bits of 23-bit extended address */
    HOWTO (R_EXTWORDMS7,16,1,7,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"RELEXTMS7",FALSE,0x7F,0x7F,FALSE),

    /* ABSOLUTE BANK */
    /* 16-bit direct reference to symbol's address, absolute */
    HOWTO (R_RELWORD,0,1,16,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"AREL16",FALSE,0xFFFF,0xFFFF,FALSE),

    /* 7 LSBs of an address, absolute */
    HOWTO (R_PARTLS7,0,1,7,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"ALS7",FALSE,0x007F,0x007F,FALSE),

    /* 9 MSBs of an address, absolute */
    /* TI assembler doesn't shift its encoding, and is thus incompatible */
    HOWTO (R_PARTMS9,7,1,9,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"AMS9",FALSE,0x01FF,0x01FF,FALSE),

    /* 23-bit direct reference, absolute */
    HOWTO (R_EXTWORD,0,2,23,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"ARELEXT",FALSE,0x7FFFFF,0x7FFFFF,FALSE),

    /* 16 bits of 23-bit extended address, absolute */
    HOWTO (R_EXTWORD16,0,1,16,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"ARELEXT16",FALSE,0x7FFFFF,0x7FFFFF,FALSE),

    /* upper 7 bits of 23-bit extended address, absolute */
    HOWTO (R_EXTWORDMS7,16,1,7,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"ARELEXTMS7",FALSE,0x7F,0x7F,FALSE),

    /* 32-bit relocation exclusively for stabs */
    HOWTO (R_RELLONG,0,2,32,FALSE,0,complain_overflow_dont,
	   tic54x_relocation,"STAB",FALSE,0xFFFFFFFF,0xFFFFFFFF,FALSE),
  };

#define coff_bfd_reloc_type_lookup tic54x_coff_reloc_type_lookup

/* For the case statement use the code values used tc_gen_reloc (defined in
   bfd/reloc.c) to map to the howto table entries.  */

reloc_howto_type *
tic54x_coff_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_16:
      return &tic54x_howto_table[0];
    case BFD_RELOC_TIC54X_PARTLS7:
      return &tic54x_howto_table[1];
    case BFD_RELOC_TIC54X_PARTMS9:
      return &tic54x_howto_table[2];
    case BFD_RELOC_TIC54X_23:
      return &tic54x_howto_table[3];
    case BFD_RELOC_TIC54X_16_OF_23:
      return &tic54x_howto_table[4];
    case BFD_RELOC_TIC54X_MS7_OF_23:
      return &tic54x_howto_table[5];
    case BFD_RELOC_32:
      return &tic54x_howto_table[12];
    default:
      return (reloc_howto_type *) NULL;
    }
}

/* Code to turn a r_type into a howto ptr, uses the above howto table.
   Called after some initial checking by the tic54x_rtype_to_howto fn below.  */

static void
tic54x_lookup_howto (internal, dst)
     arelent *internal;
     struct internal_reloc *dst;
{
  unsigned i;
  int bank = (dst->r_symndx == -1) ? HOWTO_BANK : 0;

  for (i = 0; i < sizeof tic54x_howto_table/sizeof tic54x_howto_table[0]; i++)
    {
      if (tic54x_howto_table[i].type == dst->r_type)
	{
	  internal->howto = tic54x_howto_table + i + bank;
	  return;
	}
    }

  (*_bfd_error_handler) (_("Unrecognized reloc type 0x%x"),
			 (unsigned int) dst->r_type);
  abort ();
}

#define RELOC_PROCESSING(RELENT,RELOC,SYMS,ABFD,SECT)\
 tic54x_reloc_processing(RELENT,RELOC,SYMS,ABFD,SECT)

#define coff_rtype_to_howto coff_tic54x_rtype_to_howto

static reloc_howto_type *
coff_tic54x_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h ATTRIBUTE_UNUSED;
     struct internal_syment *sym ATTRIBUTE_UNUSED;
     bfd_vma *addendp;
{
  arelent genrel;

  if (rel->r_symndx == -1 && addendp != NULL)
    {
      /* This is a TI "internal relocation", which means that the relocation
	 amount is the amount by which the current section is being relocated
	 in the output section.  */
      *addendp = (sec->output_section->vma + sec->output_offset) - sec->vma;
    }

  tic54x_lookup_howto (&genrel, rel);

  return genrel.howto;
}

/* Replace the stock _bfd_coff_is_local_label_name to recognize TI COFF local
   labels.  */

static bfd_boolean
ticoff_bfd_is_local_label_name (abfd, name)
  bfd *abfd ATTRIBUTE_UNUSED;
  const char *name;
{
  if (TICOFF_LOCAL_LABEL_P(name))
    return TRUE;
  return FALSE;
}

#define coff_bfd_is_local_label_name ticoff_bfd_is_local_label_name

/* Customize coffcode.h; the default coff_ functions are set up to use COFF2;
   coff_bad_format_hook uses BADMAG, so set that for COFF2.  The COFF1
   and COFF0 vectors use custom _bad_format_hook procs instead of setting
   BADMAG.  */
#define BADMAG(x) COFF2_BADMAG(x)
#include "coffcode.h"

static bfd_boolean
tic54x_set_section_contents (abfd, section, location, offset, bytes_to_do)
     bfd *abfd;
     sec_ptr section;
     const PTR location;
     file_ptr offset;
     bfd_size_type bytes_to_do;
{
  return coff_set_section_contents (abfd, section, location,
                                    offset, bytes_to_do);
}

static void
tic54x_reloc_processing (relent, reloc, symbols, abfd, section)
     arelent *relent;
     struct internal_reloc *reloc;
     asymbol **symbols;
     bfd *abfd;
     asection *section;
{
  asymbol *ptr;

  relent->address = reloc->r_vaddr;

  if (reloc->r_symndx != -1)
    {
      if (reloc->r_symndx < 0 || reloc->r_symndx >= obj_conv_table_size (abfd))
        {
          (*_bfd_error_handler)
            (_("%s: warning: illegal symbol index %ld in relocs"),
             bfd_archive_filename (abfd), reloc->r_symndx);
          relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
          ptr = NULL;
        }
      else
        {
          relent->sym_ptr_ptr = (symbols
                                 + obj_convert (abfd)[reloc->r_symndx]);
          ptr = *(relent->sym_ptr_ptr);
        }
    }
  else
    {
      relent->sym_ptr_ptr = section->symbol_ptr_ptr;
      ptr = *(relent->sym_ptr_ptr);
    }

  /* The symbols definitions that we have read in have been
     relocated as if their sections started at 0. But the offsets
     refering to the symbols in the raw data have not been
     modified, so we have to have a negative addend to compensate.

     Note that symbols which used to be common must be left alone.  */

  /* Calculate any reloc addend by looking at the symbol.  */
  CALC_ADDEND (abfd, ptr, *reloc, relent);

  relent->address -= section->vma;
  /* !!     relent->section = (asection *) NULL;*/

  /* Fill in the relent->howto field from reloc->r_type.  */
  tic54x_lookup_howto (relent, reloc);
}

/* TI COFF v0, DOS tools (little-endian headers).  */
const bfd_target tic54x_coff0_vec =
  {
    "coff0-c54x",			/* name */
    bfd_target_coff_flavour,
    BFD_ENDIAN_LITTLE,		/* data byte order is little */
    BFD_ENDIAN_LITTLE,		/* header byte order is little (DOS tools) */

    (HAS_RELOC | EXEC_P |		/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT ),

    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    '_',				/* leading symbol underscore */
    '/',				/* ar_pad_char */
    15,				/* ar_max_namelen */
    bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    tic54x_getl32, tic54x_getl_signed_32, tic54x_putl32,
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
    BFD_JUMP_TABLE_WRITE (tic54x),
    BFD_JUMP_TABLE_LINK (coff),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),
    NULL,

    (PTR) & ticoff0_swap_table
  };

/* TI COFF v0, SPARC tools (big-endian headers).  */
const bfd_target tic54x_coff0_beh_vec =
  {
    "coff0-beh-c54x",			/* name */
    bfd_target_coff_flavour,
    BFD_ENDIAN_LITTLE,		/* data byte order is little */
    BFD_ENDIAN_BIG,		/* header byte order is big */

    (HAS_RELOC | EXEC_P |		/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT ),

    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    '_',				/* leading symbol underscore */
    '/',				/* ar_pad_char */
    15,				/* ar_max_namelen */
    bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    tic54x_getl32, tic54x_getl_signed_32, tic54x_putl32,
    bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
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
    BFD_JUMP_TABLE_WRITE (tic54x),
    BFD_JUMP_TABLE_LINK (coff),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    & tic54x_coff0_vec,

    (PTR) & ticoff0_swap_table
  };

/* TI COFF v1, DOS tools (little-endian headers).  */
const bfd_target tic54x_coff1_vec =
  {
    "coff1-c54x",			/* name */
    bfd_target_coff_flavour,
    BFD_ENDIAN_LITTLE,		/* data byte order is little */
    BFD_ENDIAN_LITTLE,		/* header byte order is little (DOS tools) */

    (HAS_RELOC | EXEC_P |		/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT ),

    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    '_',				/* leading symbol underscore */
    '/',				/* ar_pad_char */
    15,				/* ar_max_namelen */
    bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    tic54x_getl32, tic54x_getl_signed_32, tic54x_putl32,
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
    BFD_JUMP_TABLE_WRITE (tic54x),
    BFD_JUMP_TABLE_LINK (coff),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    & tic54x_coff0_beh_vec,

    (PTR) & ticoff1_swap_table
};

/* TI COFF v1, SPARC tools (big-endian headers).  */
const bfd_target tic54x_coff1_beh_vec =
  {
    "coff1-beh-c54x",			/* name */
    bfd_target_coff_flavour,
    BFD_ENDIAN_LITTLE,		/* data byte order is little */
    BFD_ENDIAN_BIG,		/* header byte order is big */

    (HAS_RELOC | EXEC_P |		/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT ),

    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    '_',				/* leading symbol underscore */
    '/',				/* ar_pad_char */
    15,				/* ar_max_namelen */
    bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    tic54x_getl32, tic54x_getl_signed_32, tic54x_putl32,
    bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
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
    BFD_JUMP_TABLE_WRITE (tic54x),
    BFD_JUMP_TABLE_LINK (coff),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    & tic54x_coff1_vec,

    (PTR) & ticoff1_swap_table
  };

/* TI COFF v2, TI DOS tools output (little-endian headers).  */
const bfd_target tic54x_coff2_vec =
  {
    "coff2-c54x",			/* name */
    bfd_target_coff_flavour,
    BFD_ENDIAN_LITTLE,		/* data byte order is little */
    BFD_ENDIAN_LITTLE,		/* header byte order is little (DOS tools) */

    (HAS_RELOC | EXEC_P |		/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT ),

    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    '_',				/* leading symbol underscore */
    '/',				/* ar_pad_char */
    15,				/* ar_max_namelen */
    bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    tic54x_getl32, tic54x_getl_signed_32, tic54x_putl32,
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
    BFD_JUMP_TABLE_WRITE (tic54x),
    BFD_JUMP_TABLE_LINK (coff),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    & tic54x_coff1_beh_vec,

    COFF_SWAP_TABLE
  };

/* TI COFF v2, TI SPARC tools output (big-endian headers).  */
const bfd_target tic54x_coff2_beh_vec =
  {
    "coff2-beh-c54x",			/* name */
    bfd_target_coff_flavour,
    BFD_ENDIAN_LITTLE,		/* data byte order is little */
    BFD_ENDIAN_BIG,		/* header byte order is big */

    (HAS_RELOC | EXEC_P |		/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | WP_TEXT ),

    (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
    '_',				/* leading symbol underscore */
    '/',				/* ar_pad_char */
    15,				/* ar_max_namelen */
    bfd_getl64, bfd_getl_signed_64, bfd_putl64,
    tic54x_getl32, tic54x_getl_signed_32, tic54x_putl32,
    bfd_getl16, bfd_getl_signed_16, bfd_putl16,	/* data */
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
    BFD_JUMP_TABLE_WRITE (tic54x),
    BFD_JUMP_TABLE_LINK (coff),
    BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

    & tic54x_coff2_vec,

    COFF_SWAP_TABLE
  };
