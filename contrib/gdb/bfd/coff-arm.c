/* BFD back-end for ARM COFF files.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995 Free Software Foundation, Inc.
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

#include "coff/arm.h"

#include "coff/internal.h"

#ifdef COFF_WITH_PE
#include "coff/pe.h"
#endif

#include "libcoff.h"

static bfd_reloc_status_type
aoutarm_fix_pcrel_26_done PARAMS ((bfd *, arelent *, asymbol *, PTR,
				  asection *, bfd *, char **));

static bfd_reloc_status_type
aoutarm_fix_pcrel_26 PARAMS ((bfd *, arelent *, asymbol *, PTR,
			     asection *, bfd *, char **));


static bfd_reloc_status_type coff_arm_reloc 
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));


/* Used by the assembler. */
static bfd_reloc_status_type
coff_arm_reloc (abfd, reloc_entry, symbol, data, input_section, output_bfd,
		 error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  symvalue diff;
  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;

  diff = reloc_entry->addend;

#define DOIT(x) \
  x = ((x & ~howto->dst_mask) | (((x & howto->src_mask) + diff) & howto->dst_mask))

    if (diff != 0)
      {
	reloc_howto_type *howto = reloc_entry->howto;
	unsigned char *addr = (unsigned char *) data + reloc_entry->address;

	switch (howto->size)
	  {
	  case 0:
	    {
	      char x = bfd_get_8 (abfd, addr);
	      DOIT (x);
	      bfd_put_8 (abfd, x, addr);
	    }
	    break;

	  case 1:
	    {
	      short x = bfd_get_16 (abfd, addr);
	      DOIT (x);
	      bfd_put_16 (abfd, x, addr);
	    }
	    break;

	  case 2:
	    {
	      long x = bfd_get_32 (abfd, addr);
	      DOIT (x);
	      bfd_put_32 (abfd, x, addr);
	    }
	    break;

	  default:
	    abort ();
	  }
      }

  /* Now let bfd_perform_relocation finish everything up.  */
  return bfd_reloc_continue;
}

#ifndef PCRELOFFSET
#define PCRELOFFSET true
#endif

static reloc_howto_type aoutarm_std_reloc_howto[] = 
{
  /* type              rs size bsz  pcrel bitpos ovrf                     sf name     part_inpl readmask  setmask    pcdone */
  HOWTO(0,			/* type */
	0,			/* rs */
	0,			/* size */
	8,			/* bsz */
	false,			/* pcrel */
	0,			/* bitpos */
	complain_overflow_bitfield, /* ovf */
	coff_arm_reloc,		/* sf */
	"8",			/*name */
        true,			/* partial */
	0x000000ff,		/*read mask */
	0x000000ff,		/* setmask */
	PCRELOFFSET		/* pcdone */),
  HOWTO(1,  
	0, 
	1, 
	16, 
	false,
	0,
	complain_overflow_bitfield,
	coff_arm_reloc,
	"16", 
	true,
	0x0000ffff,
	0x0000ffff, 
	PCRELOFFSET),
  HOWTO( 2, 
	0,
	2, 
	32,
	false,
	0,
	complain_overflow_bitfield,
	coff_arm_reloc,
	"32",
        true,
	0xffffffff,
	0xffffffff,
	PCRELOFFSET),
  HOWTO( 3,
	2,
	2,
	26,
	true,
	0,
	complain_overflow_signed,
	aoutarm_fix_pcrel_26 ,
	"ARM26",
	false,
	0x00ffffff,
	0x00ffffff, 
	PCRELOFFSET),
  HOWTO( 4,        
	0,
	0,
	8, 
	true,
	0,
	complain_overflow_signed, 
	coff_arm_reloc,
	"DISP8",  
	true,
	0x000000ff,
	0x000000ff,
	true),
  HOWTO( 5, 
	0,
	1,
	16,
	true,
	0,
	complain_overflow_signed, 
	coff_arm_reloc,
	"DISP16",
	true,
	0x0000ffff,
	0x0000ffff,
	true),
  HOWTO( 6,
	0,
	2,
	32,
	true,
	0,
	complain_overflow_signed, 
 	coff_arm_reloc,
	"DISP32",
	true,
	0xffffffff,
	0xffffffff,
	true),
  HOWTO( 7,  
	2, 
	2,
	26,
	false,
	0,
	complain_overflow_signed,
	aoutarm_fix_pcrel_26_done, 
	"ARM26D",
	true,
	0x00ffffff,
	0x00ffffff, 
	false),
  {-1},
  HOWTO( 9,
	0,
	-1,
	16,
	false,
	0, 
	complain_overflow_bitfield,
	coff_arm_reloc,
	"NEG16",
        true, 
	0x0000ffff,
	0x0000ffff, 
	false),
  HOWTO( 10, 
	0, 
	-2,
	32,
	false,
	0,
	complain_overflow_bitfield,
	coff_arm_reloc,
	"NEG32",
        true,
	0xffffffff,
	0xffffffff,
	false),
  HOWTO( 11, 
	0,
	2, 
	32,
	false,
	0,
	complain_overflow_bitfield,
	coff_arm_reloc,
	"rva32",
        true,
	0xffffffff,
	0xffffffff,
	PCRELOFFSET),
};
#ifdef COFF_WITH_PE
/* Return true if this relocation should
   appear in the output .reloc section. */

static boolean in_reloc_p (abfd, howto)
     bfd * abfd;
     reloc_howto_type *howto;
{
  return !howto->pc_relative && howto->type != 11;
}     
#endif


#define RTYPE2HOWTO(cache_ptr, dst) \
	    (cache_ptr)->howto = aoutarm_std_reloc_howto + (dst)->r_type;

#define coff_rtype_to_howto coff_arm_rtype_to_howto

static reloc_howto_type *
coff_arm_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h;
     struct internal_syment *sym;
     bfd_vma *addendp;
{
  reloc_howto_type *howto;

  howto = aoutarm_std_reloc_howto + rel->r_type;

  if (rel->r_type == 11)
    {
      *addendp -= pe_data(sec->output_section->owner)->pe_opthdr.ImageBase;
    }
  return howto;

}
/* Used by the assembler. */

static bfd_reloc_status_type
aoutarm_fix_pcrel_26_done (abfd, reloc_entry, symbol, data, input_section,
			  output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  /* This is dead simple at present.  */
  return bfd_reloc_ok;
}

/* Used by the assembler. */

static bfd_reloc_status_type
aoutarm_fix_pcrel_26 (abfd, reloc_entry, symbol, data, input_section,
		     output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  bfd_vma relocation;
  bfd_size_type addr = reloc_entry->address;
  long target = bfd_get_32 (abfd, (bfd_byte *) data + addr);
  bfd_reloc_status_type flag = bfd_reloc_ok;
  
  /* If this is an undefined symbol, return error */
  if (symbol->section == &bfd_und_section
      && (symbol->flags & BSF_WEAK) == 0)
    return output_bfd ? bfd_reloc_continue : bfd_reloc_undefined;

  /* If the sections are different, and we are doing a partial relocation,
     just ignore it for now.  */
  if (symbol->section->name != input_section->name
      && output_bfd != (bfd *)NULL)
    return bfd_reloc_continue;

  relocation = (target & 0x00ffffff) << 2;
  relocation = (relocation ^ 0x02000000) - 0x02000000; /* Sign extend */
  relocation += symbol->value;
  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;
  relocation -= input_section->output_section->vma;
  relocation -= input_section->output_offset;
  relocation -= addr;
  if (relocation & 3)
    return bfd_reloc_overflow;

  /* Check for overflow */
  if (relocation & 0x02000000)
    {
      if ((relocation & ~0x03ffffff) != ~0x03ffffff)
	flag = bfd_reloc_overflow;
    }
  else if (relocation & ~0x03ffffff)
    flag = bfd_reloc_overflow;

  target &= ~0x00ffffff;
  target |= (relocation >> 2) & 0x00ffffff;
  bfd_put_32 (abfd, target, (bfd_byte *) data + addr);

  /* Now the ARM magic... Change the reloc type so that it is marked as done.
     Strictly this is only necessary if we are doing a partial relocation.  */
  reloc_entry->howto = &aoutarm_std_reloc_howto[7];

  return flag;
}


static CONST struct reloc_howto_struct *
arm_reloc_type_lookup(abfd,code)
      bfd *abfd;
      bfd_reloc_code_real_type code;
{
#define ASTD(i,j)       case i: return &aoutarm_std_reloc_howto[j]
  if (code == BFD_RELOC_CTOR)
    switch (bfd_get_arch_info (abfd)->bits_per_address)
      {
      case 32:
        code = BFD_RELOC_32;
        break;
      default: return (CONST struct reloc_howto_struct *) 0;
      }

  switch (code)
    {
      ASTD (BFD_RELOC_16, 1);
      ASTD (BFD_RELOC_32, 2);
      ASTD (BFD_RELOC_ARM_PCREL_BRANCH, 3);
      ASTD (BFD_RELOC_8_PCREL, 4);
      ASTD (BFD_RELOC_16_PCREL, 5);
      ASTD (BFD_RELOC_32_PCREL, 6);
      ASTD (BFD_RELOC_RVA, 11);
    default: return (CONST struct reloc_howto_struct *) 0;
    }
}


#define coff_bfd_reloc_type_lookup arm_reloc_type_lookup

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (2)
#define COFF_PAGE_SIZE 0x1000
/* Turn a howto into a reloc  nunmber */

#define SELECT_RELOC(x,howto) { x.r_type = howto->type; }
#define BADMAG(x) ARMBADMAG(x)
#define ARM 1			/* Customize coffcode.h */


/* We use the special COFF backend linker.  */
#define coff_relocate_section _bfd_coff_generic_relocate_section


#include "coffcode.h"

const bfd_target
#ifdef TARGET_LITTLE_SYM
TARGET_LITTLE_SYM =
#else
armcoff_little_vec =
#endif
{
#ifdef TARGET_LITTLE_NAME
  TARGET_LITTLE_NAME,
#else
  "coff-arm-little",
#endif
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
#ifdef TARGET_UNDERSCORE
  TARGET_UNDERSCORE,		/* leading underscore */
#else
  0,				/* leading underscore */
#endif
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */

  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

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

const bfd_target
#ifdef TARGET_BIG_SYM
TARGET_BIG_SYM =
#else
armcoff_big_vec =
#endif
{
#ifdef TARGET_BIG_NAME
  TARGET_BIG_NAME,
#else
  "coff-arm-big",
#endif
  bfd_target_coff_flavour,
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
#ifdef TARGET_UNDERSCORE
  TARGET_UNDERSCORE,		/* leading underscore */
#else
  0,				/* leading underscore */
#endif
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
