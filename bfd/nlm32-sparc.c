/* Support for 32-bit SPARC NLM (NetWare Loadable Module)
   Copyright 1993, 1994, 1999, 2000, 2001, 2002, 2003, 2004, 2005
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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#define ARCH_SIZE 32

#include "nlm/sparc32-ext.h"
#define Nlm_External_Fixed_Header	Nlm32_sparc_External_Fixed_Header

#include "libnlm.h"

enum reloc_type
{
  R_SPARC_NONE = 0,
  R_SPARC_8,		R_SPARC_16,		R_SPARC_32,
  R_SPARC_DISP8,	R_SPARC_DISP16,		R_SPARC_DISP32,
  R_SPARC_WDISP30,	R_SPARC_WDISP22,
  R_SPARC_HI22,		R_SPARC_22,
  R_SPARC_13,		R_SPARC_LO10,
  R_SPARC_GOT10,	R_SPARC_GOT13,		R_SPARC_GOT22,
  R_SPARC_PC10,		R_SPARC_PC22,
  R_SPARC_WPLT30,
  R_SPARC_COPY,
  R_SPARC_GLOB_DAT,	R_SPARC_JMP_SLOT,
  R_SPARC_RELATIVE,
  R_SPARC_UA32,
  R_SPARC_max
};

static reloc_howto_type nlm32_sparc_howto_table[] =
{
  HOWTO (R_SPARC_NONE,    0,0, 0,FALSE,0,complain_overflow_dont,    0,"R_SPARC_NONE",    FALSE,0,0x00000000,TRUE),
  HOWTO (R_SPARC_8,       0,0, 8,FALSE,0,complain_overflow_bitfield,0,"R_SPARC_8",       FALSE,0,0x000000ff,TRUE),
  HOWTO (R_SPARC_16,      0,1,16,FALSE,0,complain_overflow_bitfield,0,"R_SPARC_16",      FALSE,0,0x0000ffff,TRUE),
  HOWTO (R_SPARC_32,      0,2,32,FALSE,0,complain_overflow_bitfield,0,"R_SPARC_32",      FALSE,0,0xffffffff,TRUE),
  HOWTO (R_SPARC_DISP8,   0,0, 8,TRUE, 0,complain_overflow_signed,  0,"R_SPARC_DISP8",   FALSE,0,0x000000ff,TRUE),
  HOWTO (R_SPARC_DISP16,  0,1,16,TRUE, 0,complain_overflow_signed,  0,"R_SPARC_DISP16",  FALSE,0,0x0000ffff,TRUE),
  HOWTO (R_SPARC_DISP32,  0,2,32,TRUE, 0,complain_overflow_signed,  0,"R_SPARC_DISP32",  FALSE,0,0x00ffffff,TRUE),
  HOWTO (R_SPARC_WDISP30, 2,2,30,TRUE, 0,complain_overflow_signed,  0,"R_SPARC_WDISP30", FALSE,0,0x3fffffff,TRUE),
  HOWTO (R_SPARC_WDISP22, 2,2,22,TRUE, 0,complain_overflow_signed,  0,"R_SPARC_WDISP22", FALSE,0,0x003fffff,TRUE),
  HOWTO (R_SPARC_HI22,   10,2,22,FALSE,0,complain_overflow_dont,    0,"R_SPARC_HI22",    FALSE,0,0x003fffff,TRUE),
  HOWTO (R_SPARC_22,      0,2,22,FALSE,0,complain_overflow_bitfield,0,"R_SPARC_22",      FALSE,0,0x003fffff,TRUE),
  HOWTO (R_SPARC_13,      0,2,13,FALSE,0,complain_overflow_bitfield,0,"R_SPARC_13",      FALSE,0,0x00001fff,TRUE),
  HOWTO (R_SPARC_LO10,    0,2,10,FALSE,0,complain_overflow_dont,    0,"R_SPARC_LO10",    FALSE,0,0x000003ff,TRUE),
  HOWTO (R_SPARC_GOT10,   0,2,10,FALSE,0,complain_overflow_bitfield,0,"R_SPARC_GOT10",   FALSE,0,0x000003ff,TRUE),
  HOWTO (R_SPARC_GOT13,   0,2,13,FALSE,0,complain_overflow_bitfield,0,"R_SPARC_GOT13",   FALSE,0,0x00001fff,TRUE),
  HOWTO (R_SPARC_GOT22,  10,2,22,FALSE,0,complain_overflow_bitfield,0,"R_SPARC_GOT22",   FALSE,0,0x003fffff,TRUE),
  HOWTO (R_SPARC_PC10,    0,2,10,FALSE,0,complain_overflow_bitfield,0,"R_SPARC_PC10",    FALSE,0,0x000003ff,TRUE),
  HOWTO (R_SPARC_PC22,    0,2,22,FALSE,0,complain_overflow_bitfield,0,"R_SPARC_PC22",    FALSE,0,0x003fffff,TRUE),
  HOWTO (R_SPARC_WPLT30,  0,0,00,FALSE,0,complain_overflow_dont,    0,"R_SPARC_WPLT30",  FALSE,0,0x00000000,TRUE),
  HOWTO (R_SPARC_COPY,    0,0,00,FALSE,0,complain_overflow_dont,    0,"R_SPARC_COPY",    FALSE,0,0x00000000,TRUE),
  HOWTO (R_SPARC_GLOB_DAT,0,0,00,FALSE,0,complain_overflow_dont,    0,"R_SPARC_GLOB_DAT",FALSE,0,0x00000000,TRUE),
  HOWTO (R_SPARC_JMP_SLOT,0,0,00,FALSE,0,complain_overflow_dont,    0,"R_SPARC_JMP_SLOT",FALSE,0,0x00000000,TRUE),
  HOWTO (R_SPARC_RELATIVE,0,0,00,FALSE,0,complain_overflow_dont,    0,"R_SPARC_RELATIVE",FALSE,0,0x00000000,TRUE),
  HOWTO (R_SPARC_UA32,    0,0,00,FALSE,0,complain_overflow_dont,    0,"R_SPARC_UA32",    FALSE,0,0x00000000,TRUE),
};

/* Read a NetWare sparc reloc.  */

struct nlm32_sparc_reloc_ext
{
  unsigned char offset[4];
  unsigned char addend[4];
  unsigned char type[1];
  unsigned char pad1[3];
};

static bfd_boolean
nlm_sparc_read_reloc (bfd *abfd,
		      nlmNAME (symbol_type) *sym ATTRIBUTE_UNUSED,
		      asection **secp,
		      arelent *rel)
{
  bfd_vma val, addend;
  unsigned int index;
  unsigned int type;
  struct nlm32_sparc_reloc_ext tmp_reloc;
  asection *code_sec, *data_sec;

  if (bfd_bread (&tmp_reloc, (bfd_size_type) 12, abfd) != 12)
    return FALSE;

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
       index < sizeof (nlm32_sparc_howto_table) / sizeof (reloc_howto_type);
       index++)
    if (nlm32_sparc_howto_table[index].type == type)
      {
	rel->howto = &nlm32_sparc_howto_table[index];
	break;
      }

#ifdef DEBUG
  fprintf (stderr, "%s:  address = %08lx, addend = %08lx, type = %d, howto = %08lx\n",
	   __FUNCTION__, rel->address, rel->addend, type, rel->howto);
#endif
  return TRUE;

}

/* Write a NetWare sparc reloc.  */

static bfd_boolean
nlm_sparc_write_reloc (bfd * abfd, asection * sec, arelent * rel)
{
  bfd_vma val;
  struct nlm32_sparc_reloc_ext tmp_reloc;
  unsigned int index;
  int type = -1;
  reloc_howto_type *tmp;

  for (index = 0;
       index < sizeof (nlm32_sparc_howto_table) / sizeof (reloc_howto_type);
       index++)
    {
      tmp = &nlm32_sparc_howto_table[index];

      if (tmp->rightshift == rel->howto->rightshift
	  && tmp->size == rel->howto->size
	  && tmp->bitsize == rel->howto->bitsize
	  && tmp->pc_relative == rel->howto->pc_relative
	  && tmp->bitpos == rel->howto->bitpos
	  && tmp->src_mask == rel->howto->src_mask
	  && tmp->dst_mask == rel->howto->dst_mask)
	{
	  type = tmp->type;
	  break;
	}
    }
  if (type == -1)
    abort ();

  /* Netware wants a list of relocs for each address.
     Format is:
    	long	offset
    	long	addend
    	char	type
     That should be it.  */

  /* The value we write out is the offset into the appropriate
     segment.  This offset is the section vma, adjusted by the vma of
     the lowest section in that segment, plus the address of the
     relocation.  */
  val = bfd_get_section_vma (abfd, sec) + rel->address;

#ifdef DEBUG
  fprintf (stderr, "%s:  val = %08lx, addend = %08lx, type = %d\n",
	   __FUNCTION__, val, rel->addend, rel->howto->type);
#endif
  bfd_put_32 (abfd, val, tmp_reloc.offset);
  bfd_put_32 (abfd, rel->addend, tmp_reloc.addend);
  bfd_put_8 (abfd, (short) (rel->howto->type), tmp_reloc.type);

  if (bfd_bwrite (&tmp_reloc, (bfd_size_type) 12, abfd) != 12)
    return FALSE;

  return TRUE;
}

/* Mangle relocs for SPARC NetWare.  We can just use the standard
   SPARC relocs.  */

static bfd_boolean
nlm_sparc_mangle_relocs (bfd *abfd ATTRIBUTE_UNUSED,
			 asection *sec ATTRIBUTE_UNUSED,
			 const void * data ATTRIBUTE_UNUSED,
			 bfd_vma offset ATTRIBUTE_UNUSED,
			 bfd_size_type count ATTRIBUTE_UNUSED)
{
  return TRUE;
}

/* Read a NetWare sparc import record.  */

static bfd_boolean
nlm_sparc_read_import (bfd *abfd, nlmNAME (symbol_type) *sym)
{
  struct nlm_relent *nlm_relocs;	/* Relocation records for symbol.  */
  bfd_size_type rcount;			/* Number of relocs.  */
  bfd_byte temp[NLM_TARGET_LONG_SIZE];	/* Temporary 32-bit value.  */
  unsigned char symlength;		/* Length of symbol name.  */
  char *name;

  /* First, read in the number of relocation
     entries for this symbol.  */
  if (bfd_bread (temp, (bfd_size_type) 4, abfd) != 4)
    return FALSE;

  rcount = bfd_get_32 (abfd, temp);

  /* Next, read in the length of the symbol.  */
  if (bfd_bread (& symlength, (bfd_size_type) sizeof (symlength), abfd)
      != sizeof (symlength))
    return FALSE;
  sym -> symbol.the_bfd = abfd;
  name = bfd_alloc (abfd, (bfd_size_type) symlength + 1);
  if (name == NULL)
    return FALSE;

  /* Then read in the symbol.  */
  if (bfd_bread (name, (bfd_size_type) symlength, abfd) != symlength)
    return FALSE;
  name[symlength] = '\0';
  sym -> symbol.name = name;
  sym -> symbol.flags = 0;
  sym -> symbol.value = 0;
  sym -> symbol.section = bfd_und_section_ptr;

  /* Next, start reading in the relocs.  */
  nlm_relocs = bfd_alloc (abfd, rcount * sizeof (struct nlm_relent));
  if (!nlm_relocs)
    return FALSE;
  sym -> relocs = nlm_relocs;
  sym -> rcnt = 0;
  while (sym -> rcnt < rcount)
    {
      asection *section;

      if (! nlm_sparc_read_reloc (abfd, sym, &section, &nlm_relocs -> reloc))
	return FALSE;
      nlm_relocs -> section = section;
      nlm_relocs++;
      sym -> rcnt++;
    }

  return TRUE;
}

static bfd_boolean
nlm_sparc_write_import (bfd * abfd, asection * sec, arelent * rel)
{
  char temp[4];
  asection *code, *data, *bss, *symsec;
  bfd_vma base;

  code = bfd_get_section_by_name (abfd, NLM_CODE_NAME);
  data = bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
  bss = bfd_get_section_by_name (abfd, NLM_UNINITIALIZED_DATA_NAME);
  symsec = (*rel->sym_ptr_ptr)->section;

  if (symsec == code)
    base = 0;
  else if (symsec == data)
    base = code->size;
  else if (symsec == bss)
    base = code->size + data->size;
  else
    base = 0;

#ifdef DEBUG
  fprintf (stderr, "%s:  <%x, 1>\n\t",
	   __FUNCTION__, base + (*rel->sym_ptr_ptr)->value);
#endif
  bfd_put_32 (abfd, base + (*rel->sym_ptr_ptr)->value, temp);
  if (bfd_bwrite (temp, (bfd_size_type) 4, abfd) != 4)
    return FALSE;
  bfd_put_32 (abfd, (bfd_vma) 1, temp);
  if (bfd_bwrite (temp, (bfd_size_type) 4, abfd) != 4)
    return FALSE;
  if (! nlm_sparc_write_reloc (abfd, sec, rel))
    return FALSE;
  return TRUE;
}

/* Write out an external reference.  */

static bfd_boolean
nlm_sparc_write_external (bfd *abfd,
			  bfd_size_type count,
			  asymbol *sym,
			  struct reloc_and_sec *relocs)
{
  unsigned int i;
  bfd_byte len;
  unsigned char temp[NLM_TARGET_LONG_SIZE];

  bfd_put_32 (abfd, count, temp);
  if (bfd_bwrite (temp, (bfd_size_type) sizeof (temp), abfd) != sizeof (temp))
    return FALSE;

  len = strlen (sym->name);
  if ((bfd_bwrite (&len, (bfd_size_type) sizeof (bfd_byte), abfd)
       != sizeof (bfd_byte))
      || bfd_bwrite (sym->name, (bfd_size_type) len, abfd) != len)
    return FALSE;

  for (i = 0; i < count; i++)
    if (! nlm_sparc_write_reloc (abfd, relocs[i].sec, relocs[i].rel))
      return FALSE;

  return TRUE;
}

static bfd_boolean
nlm_sparc_write_export (bfd * abfd, asymbol * sym, bfd_vma value)
{
  bfd_byte len;
  bfd_byte temp[4];

#ifdef DEBUG
  fprintf (stderr, "%s: <%x, %d, %s>\n",
	   __FUNCTION__, value, strlen (sym->name), sym->name);
#endif
  bfd_put_32 (abfd, value, temp);
  len = strlen (sym->name);

  if (bfd_bwrite (temp, (bfd_size_type) 4, abfd) != 4
      || bfd_bwrite (&len, (bfd_size_type) 1, abfd) != 1
      || bfd_bwrite (sym->name, (bfd_size_type) len, abfd) != len)
    return FALSE;

  return TRUE;
}

#undef nlm_swap_fixed_header_in
#undef nlm_swap_fixed_header_out

#include "nlmswap.h"

static const struct nlm_backend_data nlm32_sparc_backend =
{
  "NetWare SPARC Module   \032",
  sizeof (Nlm32_sparc_External_Fixed_Header),
  0,	/* Optional_prefix_size.  */
  bfd_arch_sparc,
  0,
  FALSE,
  0,	/* Backend_object_p.  */
  0,	/* Write_prefix_func.  */
  nlm_sparc_read_reloc,
  nlm_sparc_mangle_relocs,
  nlm_sparc_read_import,
  nlm_sparc_write_import,
  0,	/* Set_public_section.  */
  0,	/* Get_public_offset.  */
  nlm_swap_fixed_header_in,
  nlm_swap_fixed_header_out,
  nlm_sparc_write_external,
  nlm_sparc_write_export
};

#define TARGET_BIG_NAME		"nlm32-sparc"
#define TARGET_BIG_SYM		nlmNAME (sparc_vec)
#define TARGET_BACKEND_DATA	& nlm32_sparc_backend

#include "nlm-target.h"
