/* Support for 32-bit SPARC NLM (NetWare Loadable Module)
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

#include "nlm/sparc32-ext.h"
#define Nlm_External_Fixed_Header	Nlm32_sparc_External_Fixed_Header

#include "libnlm.h"

static boolean nlm_sparc_read_reloc
  PARAMS ((bfd *, nlmNAME(symbol_type) *, asection **, arelent *));
static boolean nlm_sparc_write_reloc
  PARAMS ((bfd *, asection *, arelent *));
static boolean nlm_sparc_mangle_relocs
  PARAMS ((bfd *, asection *, PTR, bfd_vma, bfd_size_type));
static boolean nlm_sparc_read_import
  PARAMS ((bfd *, nlmNAME(symbol_type) *));
static boolean nlm_sparc_write_import
  PARAMS ((bfd *, asection *, arelent *));
static boolean nlm_sparc_write_external
  PARAMS ((bfd *, bfd_size_type, asymbol *, struct reloc_and_sec *));

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

static reloc_howto_type nlm32_sparc_howto_table[] = 
{
  HOWTO(R_SPARC_NONE,    0,0, 0,false,0,complain_overflow_dont,    0,"R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_8,       0,0, 8,false,0,complain_overflow_bitfield,0,"R_SPARC_8",       false,0,0x000000ff,true),
  HOWTO(R_SPARC_16,      0,1,16,false,0,complain_overflow_bitfield,0,"R_SPARC_16",      false,0,0x0000ffff,true),
  HOWTO(R_SPARC_32,      0,2,32,false,0,complain_overflow_bitfield,0,"R_SPARC_32",      false,0,0xffffffff,true),
  HOWTO(R_SPARC_DISP8,   0,0, 8,true, 0,complain_overflow_signed,  0,"R_SPARC_DISP8",   false,0,0x000000ff,true),
  HOWTO(R_SPARC_DISP16,  0,1,16,true, 0,complain_overflow_signed,  0,"R_SPARC_DISP16",  false,0,0x0000ffff,true),
  HOWTO(R_SPARC_DISP32,  0,2,32,true, 0,complain_overflow_signed,  0,"R_SPARC_DISP32",  false,0,0x00ffffff,true),
  HOWTO(R_SPARC_WDISP30, 2,2,30,true, 0,complain_overflow_signed,  0,"R_SPARC_WDISP30", false,0,0x3fffffff,true),
  HOWTO(R_SPARC_WDISP22, 2,2,22,true, 0,complain_overflow_signed,  0,"R_SPARC_WDISP22", false,0,0x003fffff,true),
  HOWTO(R_SPARC_HI22,   10,2,22,false,0,complain_overflow_dont,    0,"R_SPARC_HI22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_22,      0,2,22,false,0,complain_overflow_bitfield,0,"R_SPARC_22",      false,0,0x003fffff,true),
  HOWTO(R_SPARC_13,      0,2,13,false,0,complain_overflow_bitfield,0,"R_SPARC_13",      false,0,0x00001fff,true),
  HOWTO(R_SPARC_LO10,    0,2,10,false,0,complain_overflow_dont,    0,"R_SPARC_LO10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_GOT10,   0,2,10,false,0,complain_overflow_bitfield,0,"R_SPARC_GOT10",   false,0,0x000003ff,true),
  HOWTO(R_SPARC_GOT13,   0,2,13,false,0,complain_overflow_bitfield,0,"R_SPARC_GOT13",   false,0,0x00001fff,true),
  HOWTO(R_SPARC_GOT22,  10,2,22,false,0,complain_overflow_bitfield,0,"R_SPARC_GOT22",   false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC10,    0,2,10,false,0,complain_overflow_bitfield,0,"R_SPARC_PC10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_PC22,    0,2,22,false,0,complain_overflow_bitfield,0,"R_SPARC_PC22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_WPLT30,  0,0,00,false,0,complain_overflow_dont,    0,"R_SPARC_WPLT30",  false,0,0x00000000,true),
  HOWTO(R_SPARC_COPY,    0,0,00,false,0,complain_overflow_dont,    0,"R_SPARC_COPY",    false,0,0x00000000,true),
  HOWTO(R_SPARC_GLOB_DAT,0,0,00,false,0,complain_overflow_dont,    0,"R_SPARC_GLOB_DAT",false,0,0x00000000,true),
  HOWTO(R_SPARC_JMP_SLOT,0,0,00,false,0,complain_overflow_dont,    0,"R_SPARC_JMP_SLOT",false,0,0x00000000,true),
  HOWTO(R_SPARC_RELATIVE,0,0,00,false,0,complain_overflow_dont,    0,"R_SPARC_RELATIVE",false,0,0x00000000,true),
  HOWTO(R_SPARC_UA32,    0,0,00,false,0,complain_overflow_dont,    0,"R_SPARC_UA32",    false,0,0x00000000,true),
};

/* Read a NetWare sparc reloc.  */

struct nlm32_sparc_reloc_ext {
  unsigned char offset[4];
  unsigned char addend[4];
  unsigned char type[1];
  unsigned char pad1[3];
};

static boolean
nlm_sparc_read_reloc (abfd, sym, secp, rel)
     bfd *abfd;
     nlmNAME(symbol_type) *sym;
     asection **secp;
     arelent *rel;
{
  bfd_vma val, addend;
  unsigned int index;
  unsigned int type;
  struct nlm32_sparc_reloc_ext tmp_reloc;
  asection *code_sec, *data_sec;

  if (bfd_read (&tmp_reloc, 12, 1, abfd) != 12)
    return false;

  code_sec = bfd_get_section_by_name (abfd, NLM_CODE_NAME);
  data_sec = bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);

  *secp = code_sec;

  val = bfd_get_32 (abfd, tmp_reloc.offset);
  addend = bfd_get_32 (abfd, tmp_reloc.addend);
  type = bfd_get_8 (abfd, tmp_reloc.type);

  rel->address = val;
  rel->addend = addend;
  rel->howto = NULL;

  for (index = 0;
       index < sizeof(nlm32_sparc_howto_table) / sizeof(reloc_howto_type);
       index++)
    if (nlm32_sparc_howto_table[index].type == type) {
      rel->howto = &nlm32_sparc_howto_table[index];
      break;
    }

#ifdef DEBUG
  fprintf (stderr, "%s:  address = %08lx, addend = %08lx, type = %d, howto = %08lx\n",
	   __FUNCTION__, rel->address, rel->addend, type, rel->howto);
#endif
  return true;

}

/* Write a NetWare sparc reloc.  */

static boolean
nlm_sparc_write_reloc (abfd, sec, rel)
     bfd *abfd;
     asection *sec;
     arelent *rel;
{
  bfd_vma val;
  struct nlm32_sparc_reloc_ext tmp_reloc;
  unsigned int index;
  int type = -1;
  reloc_howto_type *tmp;

  
  for (index = 0;
       index < sizeof (nlm32_sparc_howto_table) / sizeof(reloc_howto_type);
       index++) {
    tmp = &nlm32_sparc_howto_table[index];

    if (tmp->rightshift == rel->howto->rightshift
	&& tmp->size == rel->howto->size
	&& tmp->bitsize == rel->howto->bitsize
	&& tmp->pc_relative == rel->howto->pc_relative
	&& tmp->bitpos == rel->howto->bitpos
	&& tmp->src_mask == rel->howto->src_mask
	&& tmp->dst_mask == rel->howto->dst_mask) {
      type = tmp->type;
      break;
    }
  }
  if (type == -1)
    abort();

  /*
   * Netware wants a list of relocs for each address.
   * Format is:
   *	long	offset
   *	long	addend
   *	char	type
   * That should be it.
   */

  /* The value we write out is the offset into the appropriate
     segment.  This offset is the section vma, adjusted by the vma of
     the lowest section in that segment, plus the address of the
     relocation.  */
#if 0
  val = bfd_get_section_vma (abfd, (*rel->sym_ptr_ptr)->section) + rel->address;
#else
  val = bfd_get_section_vma (abfd, sec) + rel->address;
#endif

#ifdef DEBUG
  fprintf (stderr, "%s:  val = %08lx, addend = %08lx, type = %d\n",
	   __FUNCTION__, val, rel->addend, rel->howto->type);
#endif
  bfd_put_32 (abfd, val, tmp_reloc.offset);
  bfd_put_32 (abfd, rel->addend, tmp_reloc.addend);
  bfd_put_8 (abfd, (short)(rel->howto->type), tmp_reloc.type);

  if (bfd_write (&tmp_reloc, 12, 1, abfd) != 12)
    return false;

  return true;
}

/* Mangle relocs for SPARC NetWare.  We can just use the standard
   SPARC relocs.  */

static boolean
nlm_sparc_mangle_relocs (abfd, sec, data, offset, count)
     bfd *abfd;
     asection *sec;
     PTR data;
     bfd_vma offset;
     bfd_size_type count;
{
  return true;
}

/* Read a NetWare sparc import record */
static boolean
nlm_sparc_read_import (abfd, sym)
     bfd *abfd;
     nlmNAME(symbol_type) *sym;
{
  struct nlm_relent *nlm_relocs;	/* relocation records for symbol */
  bfd_size_type rcount;			/* number of relocs */
  bfd_byte temp[NLM_TARGET_LONG_SIZE];	/* temporary 32-bit value */
  unsigned char symlength;		/* length of symbol name */
  char *name;
  
  /*
   * First, read in the number of relocation
   * entries for this symbol
   */
  if (bfd_read ((PTR) temp, 4, 1, abfd) != 4)
    return false;
  
  rcount = bfd_get_32 (abfd, temp);
  
  /*
   * Next, read in the length of the symbol
   */
  
  if (bfd_read ((PTR) &symlength, sizeof (symlength), 1, abfd)
      != sizeof (symlength))
    return false;
  sym -> symbol.the_bfd = abfd;
  name = bfd_alloc (abfd, symlength + 1);
  if (name == NULL)
    return false;
  
  /*
   * Then read in the symbol
   */
  
  if (bfd_read (name, symlength, 1, abfd) != symlength)
    return false;
  name[symlength] = '\0';
  sym -> symbol.name = name;
  sym -> symbol.flags = 0;
  sym -> symbol.value = 0;
  sym -> symbol.section = bfd_und_section_ptr;
  
  /*
   * Next, start reading in the relocs.
   */
  
  nlm_relocs = ((struct nlm_relent *)
		bfd_alloc (abfd, rcount * sizeof (struct nlm_relent)));
  if (!nlm_relocs)
    return false;
  sym -> relocs = nlm_relocs;
  sym -> rcnt = 0;
  while (sym -> rcnt < rcount)
    {
      asection *section;
      
      if (nlm_sparc_read_reloc (abfd, sym, &section,
			      &nlm_relocs -> reloc)
	  == false)
	return false;
      nlm_relocs -> section = section;
      nlm_relocs++;
      sym -> rcnt++;
    }
  return true;
}

static boolean
nlm_sparc_write_import (abfd, sec, rel)
     bfd *abfd;
     asection *sec;
     arelent *rel;
{
  char temp[4];
  asection *code, *data, *bss, *symsec;
  bfd_vma base;

  code = bfd_get_section_by_name (abfd, NLM_CODE_NAME);
  data = bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
  bss = bfd_get_section_by_name (abfd, NLM_UNINITIALIZED_DATA_NAME);
  symsec = (*rel->sym_ptr_ptr)->section;

  if (symsec == code) {
    base = 0;
  } else if (symsec == data) {
    base = bfd_section_size (abfd, code);
  } else if (symsec == bss) {
    base = bfd_section_size (abfd, code) + bfd_section_size (abfd, data);
  } else
    base = 0;

#ifdef DEBUG
  fprintf (stderr, "%s:  <%x, 1>\n\t",
	   __FUNCTION__, base + (*rel->sym_ptr_ptr)->value);
#endif
  bfd_put_32 (abfd, base + (*rel->sym_ptr_ptr)->value, temp);
  if (bfd_write ((PTR)temp, 4, 1, abfd) != 4)
    return false;
  bfd_put_32 (abfd, 1, temp);
  if (bfd_write ((PTR)temp, 4, 1, abfd) != 4)
    return false;
  if (nlm_sparc_write_reloc (abfd, sec, rel) == false)
    return false;
  return true;
}

/* Write out an external reference.  */

static boolean
nlm_sparc_write_external (abfd, count, sym, relocs)
     bfd *abfd;
     bfd_size_type count;
     asymbol *sym;
     struct reloc_and_sec *relocs;
{
  unsigned int i;
  bfd_byte len;
  unsigned char temp[NLM_TARGET_LONG_SIZE];

  bfd_put_32 (abfd, count, temp);
  if (bfd_write (temp, sizeof(temp), 1, abfd) != sizeof (temp))
    return false;

  len = strlen (sym->name);
  if ((bfd_write (&len, sizeof (bfd_byte), 1, abfd) != sizeof(bfd_byte))
      || bfd_write (sym->name, len, 1, abfd) != len)
    return false;

  for (i = 0; i < count; i++)
    {
      if (nlm_sparc_write_reloc (abfd, relocs[i].sec,
				 relocs[i].rel) == false)
	return false;
    }

  return true;
}

static boolean
nlm_sparc_write_export (abfd, sym, value)
     bfd *abfd;
     asymbol *sym;
     bfd_vma value;
{
  bfd_byte len;
  bfd_byte temp[4];

#ifdef DEBUG
  fprintf (stderr, "%s: <%x, %d, %s>\n",
	   __FUNCTION__, value, strlen (sym->name), sym->name);
#endif
  bfd_put_32 (abfd, value, temp);
  len = strlen (sym->name);

  if (bfd_write (temp, 4, 1, abfd) != 4
      || bfd_write (&len, 1, 1, abfd) != 1
      || bfd_write (sym->name, len, 1, abfd) != len)
    return false;

  return true;
}

#undef nlm_swap_fixed_header_in
#undef nlm_swap_fixed_header_out

#include "nlmswap.h"

static const struct nlm_backend_data nlm32_sparc_backend =
{
  "NetWare SPARC Module   \032",
  sizeof (Nlm32_sparc_External_Fixed_Header),
  0,	/* optional_prefix_size */
  bfd_arch_sparc,
  0,
  false,
  0,	/* backend_object_p */
  0,	/* write_prefix_func */
  nlm_sparc_read_reloc,
  nlm_sparc_mangle_relocs,
  nlm_sparc_read_import,
  nlm_sparc_write_import,
  0,	/* set_public_section */
  0,	/* get_public_offset */
  nlm_swap_fixed_header_in,
  nlm_swap_fixed_header_out,
  nlm_sparc_write_external,
  nlm_sparc_write_export
};

#define TARGET_BIG_NAME		"nlm32-sparc"
#define TARGET_BIG_SYM		nlmNAME(sparc_vec)
#define TARGET_BACKEND_DATA		&nlm32_sparc_backend

#include "nlm-target.h"
