/* BFD back-end for Sparc COFF files.
   Copyright 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
   Written by Cygnus Support.

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
#include "obstack.h"
#include "coff/sparc.h"
#include "coff/internal.h"
#include "libcoff.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (3)

#define BADMAG(x) ((x).f_magic != SPARCMAGIC && (x).f_magic != LYNXCOFFMAGIC)

/* The page size is a guess based on ELF.  */
#define COFF_PAGE_SIZE 0x10000

enum reloc_type
  {
    R_SPARC_NONE = 0,
    R_SPARC_8,		R_SPARC_16,		R_SPARC_32, 
    R_SPARC_DISP8,	R_SPARC_DISP16,		R_SPARC_DISP32, 
    R_SPARC_WDISP30,	R_SPARC_WDISP22,
    R_SPARC_HI22,	R_SPARC_22,
    R_SPARC_13,		R_SPARC_LO10,
    R_SPARC_GOT10,	R_SPARC_GOT13,		R_SPARC_GOT22,
    R_SPARC_PC10,	R_SPARC_PC22,
    R_SPARC_WPLT30,
    R_SPARC_COPY,
    R_SPARC_GLOB_DAT,	R_SPARC_JMP_SLOT,
    R_SPARC_RELATIVE,
    R_SPARC_UA32,
    R_SPARC_max
  };

#if 0
static CONST char *CONST reloc_type_names[] =
{
  "R_SPARC_NONE",
  "R_SPARC_8",		"R_SPARC_16",		"R_SPARC_32",
  "R_SPARC_DISP8",	"R_SPARC_DISP16",	"R_SPARC_DISP32",
  "R_SPARC_WDISP30",	"R_SPARC_WDISP22",
  "R_SPARC_HI22",	"R_SPARC_22",
  "R_SPARC_13",		"R_SPARC_LO10",
  "R_SPARC_GOT10",	"R_SPARC_GOT13",	"R_SPARC_GOT22",
  "R_SPARC_PC10",	"R_SPARC_PC22",
  "R_SPARC_WPLT30",
  "R_SPARC_COPY",
  "R_SPARC_GLOB_DAT",	"R_SPARC_JMP_SLOT",
  "R_SPARC_RELATIVE",
  "R_SPARC_UA32",
};
#endif

/* This is stolen pretty directly from elf.c.  */
static bfd_reloc_status_type
bfd_coff_generic_reloc PARAMS ((bfd *, arelent *, asymbol *, PTR,
				asection *, bfd *, char **));

static bfd_reloc_status_type
bfd_coff_generic_reloc (abfd, reloc_entry, symbol, data, input_section,
			output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  return bfd_reloc_continue;
}

static reloc_howto_type coff_sparc_howto_table[] = 
{
  HOWTO(R_SPARC_NONE,    0,0, 0,false,0,complain_overflow_dont,    bfd_coff_generic_reloc,"R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_8,       0,0, 8,false,0,complain_overflow_bitfield,bfd_coff_generic_reloc,"R_SPARC_8",       false,0,0x000000ff,true),
  HOWTO(R_SPARC_16,      0,1,16,false,0,complain_overflow_bitfield,bfd_coff_generic_reloc,"R_SPARC_16",      false,0,0x0000ffff,true),
  HOWTO(R_SPARC_32,      0,2,32,false,0,complain_overflow_bitfield,bfd_coff_generic_reloc,"R_SPARC_32",      false,0,0xffffffff,true),
  HOWTO(R_SPARC_DISP8,   0,0, 8,true, 0,complain_overflow_signed,  bfd_coff_generic_reloc,"R_SPARC_DISP8",   false,0,0x000000ff,true),
  HOWTO(R_SPARC_DISP16,  0,1,16,true, 0,complain_overflow_signed,  bfd_coff_generic_reloc,"R_SPARC_DISP16",  false,0,0x0000ffff,true),
  HOWTO(R_SPARC_DISP32,  0,2,32,true, 0,complain_overflow_signed,  bfd_coff_generic_reloc,"R_SPARC_DISP32",  false,0,0x00ffffff,true),
  HOWTO(R_SPARC_WDISP30, 2,2,30,true, 0,complain_overflow_signed,  bfd_coff_generic_reloc,"R_SPARC_WDISP30", false,0,0x3fffffff,true),
  HOWTO(R_SPARC_WDISP22, 2,2,22,true, 0,complain_overflow_signed,  bfd_coff_generic_reloc,"R_SPARC_WDISP22", false,0,0x003fffff,true),
  HOWTO(R_SPARC_HI22,   10,2,22,false,0,complain_overflow_dont,    bfd_coff_generic_reloc,"R_SPARC_HI22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_22,      0,2,22,false,0,complain_overflow_bitfield,bfd_coff_generic_reloc,"R_SPARC_22",      false,0,0x003fffff,true),
  HOWTO(R_SPARC_13,      0,2,13,false,0,complain_overflow_bitfield,bfd_coff_generic_reloc,"R_SPARC_13",      false,0,0x00001fff,true),
  HOWTO(R_SPARC_LO10,    0,2,10,false,0,complain_overflow_dont,    bfd_coff_generic_reloc,"R_SPARC_LO10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_GOT10,   0,2,10,false,0,complain_overflow_bitfield,bfd_coff_generic_reloc,"R_SPARC_GOT10",   false,0,0x000003ff,true),
  HOWTO(R_SPARC_GOT13,   0,2,13,false,0,complain_overflow_bitfield,bfd_coff_generic_reloc,"R_SPARC_GOT13",   false,0,0x00001fff,true),
  HOWTO(R_SPARC_GOT22,  10,2,22,false,0,complain_overflow_bitfield,bfd_coff_generic_reloc,"R_SPARC_GOT22",   false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC10,    0,2,10,false,0,complain_overflow_bitfield,bfd_coff_generic_reloc,"R_SPARC_PC10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_PC22,    0,2,22,false,0,complain_overflow_bitfield,bfd_coff_generic_reloc,"R_SPARC_PC22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_WPLT30,  0,0,00,false,0,complain_overflow_dont,    bfd_coff_generic_reloc,"R_SPARC_WPLT30",  false,0,0x00000000,true),
  HOWTO(R_SPARC_COPY,    0,0,00,false,0,complain_overflow_dont,    bfd_coff_generic_reloc,"R_SPARC_COPY",    false,0,0x00000000,true),
  HOWTO(R_SPARC_GLOB_DAT,0,0,00,false,0,complain_overflow_dont,    bfd_coff_generic_reloc,"R_SPARC_GLOB_DAT",false,0,0x00000000,true),
  HOWTO(R_SPARC_JMP_SLOT,0,0,00,false,0,complain_overflow_dont,    bfd_coff_generic_reloc,"R_SPARC_JMP_SLOT",false,0,0x00000000,true),
  HOWTO(R_SPARC_RELATIVE,0,0,00,false,0,complain_overflow_dont,    bfd_coff_generic_reloc,"R_SPARC_RELATIVE",false,0,0x00000000,true),
  HOWTO(R_SPARC_UA32,    0,0,00,false,0,complain_overflow_dont,    bfd_coff_generic_reloc,"R_SPARC_UA32",    false,0,0x00000000,true),
};

struct coff_reloc_map {
  unsigned char bfd_reloc_val;
  unsigned char coff_reloc_val;
};

static CONST struct coff_reloc_map sparc_reloc_map[] =
{
  { BFD_RELOC_NONE, R_SPARC_NONE, },
  { BFD_RELOC_16, R_SPARC_16, },
  { BFD_RELOC_8, R_SPARC_8 },
  { BFD_RELOC_8_PCREL, R_SPARC_DISP8 },
  { BFD_RELOC_CTOR, R_SPARC_32 }, /* @@ Assumes 32 bits.  */
  { BFD_RELOC_32, R_SPARC_32 },
  { BFD_RELOC_32_PCREL, R_SPARC_DISP32 },
  { BFD_RELOC_HI22, R_SPARC_HI22 },
  { BFD_RELOC_LO10, R_SPARC_LO10, },
  { BFD_RELOC_32_PCREL_S2, R_SPARC_WDISP30 },
  { BFD_RELOC_SPARC22, R_SPARC_22 },
  { BFD_RELOC_SPARC13, R_SPARC_13 },
  { BFD_RELOC_SPARC_GOT10, R_SPARC_GOT10 },
  { BFD_RELOC_SPARC_GOT13, R_SPARC_GOT13 },
  { BFD_RELOC_SPARC_GOT22, R_SPARC_GOT22 },
  { BFD_RELOC_SPARC_PC10, R_SPARC_PC10 },
  { BFD_RELOC_SPARC_PC22, R_SPARC_PC22 },
  { BFD_RELOC_SPARC_WPLT30, R_SPARC_WPLT30 },
  { BFD_RELOC_SPARC_COPY, R_SPARC_COPY },
  { BFD_RELOC_SPARC_GLOB_DAT, R_SPARC_GLOB_DAT },
  { BFD_RELOC_SPARC_JMP_SLOT, R_SPARC_JMP_SLOT },
  { BFD_RELOC_SPARC_RELATIVE, R_SPARC_RELATIVE },
  { BFD_RELOC_SPARC_WDISP22, R_SPARC_WDISP22 },
  /*  { BFD_RELOC_SPARC_UA32, R_SPARC_UA32 }, not used?? */
};

static reloc_howto_type *
coff_sparc_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  unsigned int i;
  for (i = 0; i < sizeof (sparc_reloc_map) / sizeof (struct coff_reloc_map); i++)
    {
      if (sparc_reloc_map[i].bfd_reloc_val == code)
	return &coff_sparc_howto_table[(int) sparc_reloc_map[i].coff_reloc_val];
    }
  return 0;
}
#define coff_bfd_reloc_type_lookup	coff_sparc_reloc_type_lookup

static void
rtype2howto (cache_ptr, dst)
     arelent *cache_ptr;
     struct internal_reloc *dst;
{
  BFD_ASSERT (dst->r_type < (unsigned int) R_SPARC_max);
  cache_ptr->howto = &coff_sparc_howto_table[dst->r_type];
}

#define RTYPE2HOWTO(internal, relocentry) rtype2howto(internal,relocentry)

#define SWAP_IN_RELOC_OFFSET	bfd_h_get_32
#define SWAP_OUT_RELOC_OFFSET	bfd_h_put_32
/* This is just like the standard one, except that we don't set up an
   addend for relocs against global symbols (otherwise linking objects
   created by -r fails), and we add in the reloc offset at the end.  */
#define CALC_ADDEND(abfd, ptr, reloc, cache_ptr)                \
  {                                                             \
    coff_symbol_type *coffsym = (coff_symbol_type *) NULL;      \
    if (ptr && bfd_asymbol_bfd (ptr) != abfd)                   \
      coffsym = (obj_symbols (abfd)                             \
                 + (cache_ptr->sym_ptr_ptr - symbols));         \
    else if (ptr)                                               \
      coffsym = coff_symbol_from (abfd, ptr);                   \
    if (coffsym != (coff_symbol_type *) NULL                    \
        && coffsym->native->u.syment.n_scnum == 0)              \
      cache_ptr->addend = 0;                                    \
    else if (ptr && bfd_asymbol_bfd (ptr) == abfd               \
             && ptr->section != (asection *) NULL               \
	     && (ptr->flags & BSF_GLOBAL) == 0)			\
      cache_ptr->addend = - (ptr->section->vma + ptr->value);   \
    else                                                        \
      cache_ptr->addend = 0;                                    \
    cache_ptr->addend += reloc.r_offset;			\
  }

/* Clear the r_spare field in relocs.  */
#define SWAP_OUT_RELOC_EXTRA(abfd,src,dst) \
  do { \
       dst->r_spare[0] = 0; \
       dst->r_spare[1] = 0; \
     } while (0)

#define __A_MAGIC_SET__

/* Enable Sparc-specific hacks in coffcode.h. */

#define COFF_SPARC

#include "coffcode.h"

const bfd_target
#ifdef TARGET_SYM
  TARGET_SYM =
#else
  sparccoff_vec =
#endif
{
#ifdef TARGET_NAME
  TARGET_NAME,
#else
  "coff-sparc",			/* name */
#endif
  bfd_target_coff_flavour,
  BFD_ENDIAN_BIG,	/* data byte order is big */
  BFD_ENDIAN_BIG,	/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  '_',				/* leading underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */

  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

/* Note that we allow an object file to be treated as a core file as well. */
    {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
       bfd_generic_archive_p, coff_object_p},
    {bfd_false, coff_mkobject, _bfd_generic_mkarchive, /* bfd_set_format */
       bfd_false},
    {bfd_false, coff_write_object_contents, /* bfd_write_contents */
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
