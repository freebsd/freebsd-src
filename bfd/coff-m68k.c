/* BFD back-end for Motorola 68000 COFF binaries.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1999,
   2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
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
#include "coff/m68k.h"
#include "coff/internal.h"
#include "libcoff.h"

/* This source file is compiled multiple times for various m68k COFF
   variants.  The following macros control its behaviour:

   TARGET_SYM
     The C name of the BFD target vector.  The default is m68kcoff_vec.
   TARGET_NAME
     The user visible target name.  The default is "coff-m68k".
   NAMES_HAVE_UNDERSCORE
     Whether symbol names have an underscore.
   ONLY_DECLARE_RELOCS
     Only declare the relocation howto array.  Don't actually compile
     it.  The actual array will be picked up in another version of the
     file.
   STATIC_RELOCS
     Make the relocation howto array, and associated functions, static.
   COFF_COMMON_ADDEND
     If this is defined, then, for a relocation against a common
     symbol, the object file holds the value (the size) of the common
     symbol.  If this is not defined, then, for a relocation against a
     common symbol, the object file holds zero.  */

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (2)

#ifndef COFF_PAGE_SIZE
/* The page size is a guess based on ELF.  */
#define COFF_PAGE_SIZE 0x2000
#endif

#ifndef COFF_COMMON_ADDEND
#define RELOC_SPECIAL_FN 0
#else
static bfd_reloc_status_type m68kcoff_common_addend_special_fn
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *m68kcoff_common_addend_rtype_to_howto
  PARAMS ((bfd *, asection *, struct internal_reloc *,
	   struct coff_link_hash_entry *, struct internal_syment *,
	   bfd_vma *));
#define RELOC_SPECIAL_FN m68kcoff_common_addend_special_fn
#endif

static bfd_boolean m68k_coff_is_local_label_name
  PARAMS ((bfd *, const char *));

/* On the delta, a symbol starting with L% is local.  We won't see
   such a symbol on other platforms, so it should be safe to always
   consider it local here.  */

static bfd_boolean
m68k_coff_is_local_label_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  if (name[0] == 'L' && name[1] == '%')
    return TRUE;

  return _bfd_coff_is_local_label_name (abfd, name);
}

#ifndef STATIC_RELOCS
/* Clean up namespace.  */
#define m68kcoff_howto_table	_bfd_m68kcoff_howto_table
#define m68k_rtype2howto	_bfd_m68kcoff_rtype2howto
#define m68k_howto2rtype	_bfd_m68kcoff_howto2rtype
#define m68k_reloc_type_lookup	_bfd_m68kcoff_reloc_type_lookup
#endif

#ifdef ONLY_DECLARE_RELOCS
extern reloc_howto_type m68kcoff_howto_table[];
#else
#ifdef STATIC_RELOCS
static
#endif
reloc_howto_type m68kcoff_howto_table[] =
  {
    HOWTO (R_RELBYTE,	       0,  0,  	8,  FALSE, 0, complain_overflow_bitfield, RELOC_SPECIAL_FN, "8",	TRUE, 0x000000ff,0x000000ff, FALSE),
    HOWTO (R_RELWORD,	       0,  1, 	16, FALSE, 0, complain_overflow_bitfield, RELOC_SPECIAL_FN, "16",	TRUE, 0x0000ffff,0x0000ffff, FALSE),
    HOWTO (R_RELLONG,	       0,  2, 	32, FALSE, 0, complain_overflow_bitfield, RELOC_SPECIAL_FN, "32",	TRUE, 0xffffffff,0xffffffff, FALSE),
    HOWTO (R_PCRBYTE,	       0,  0, 	8,  TRUE,  0, complain_overflow_signed,   RELOC_SPECIAL_FN, "DISP8",    TRUE, 0x000000ff,0x000000ff, FALSE),
    HOWTO (R_PCRWORD,	       0,  1, 	16, TRUE,  0, complain_overflow_signed,   RELOC_SPECIAL_FN, "DISP16",   TRUE, 0x0000ffff,0x0000ffff, FALSE),
    HOWTO (R_PCRLONG,	       0,  2, 	32, TRUE,  0, complain_overflow_signed,   RELOC_SPECIAL_FN, "DISP32",   TRUE, 0xffffffff,0xffffffff, FALSE),
    HOWTO (R_RELLONG_NEG,      0, -2, 	32, FALSE, 0, complain_overflow_bitfield, RELOC_SPECIAL_FN, "-32",	TRUE, 0xffffffff,0xffffffff, FALSE),
  };
#endif /* not ONLY_DECLARE_RELOCS */

#ifndef BADMAG
#define BADMAG(x) M68KBADMAG(x)
#endif
#define M68 1		/* Customize coffcode.h */

/* Turn a howto into a reloc number */

#ifdef ONLY_DECLARE_RELOCS
extern void m68k_rtype2howto PARAMS ((arelent *internal, int relocentry));
extern int m68k_howto2rtype PARAMS ((reloc_howto_type *));
extern reloc_howto_type *m68k_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
#else

#ifdef STATIC_RELOCS
#define STAT_REL static
#else
#define STAT_REL
#endif

STAT_REL reloc_howto_type * m68k_reloc_type_lookup PARAMS ((bfd *, bfd_reloc_code_real_type));
STAT_REL int m68k_howto2rtype PARAMS ((reloc_howto_type *));
STAT_REL void m68k_rtype2howto PARAMS ((arelent *, int));


STAT_REL void
m68k_rtype2howto(internal, relocentry)
     arelent *internal;
     int relocentry;
{
  switch (relocentry)
    {
    case R_RELBYTE:	internal->howto = m68kcoff_howto_table + 0; break;
    case R_RELWORD:	internal->howto = m68kcoff_howto_table + 1; break;
    case R_RELLONG:	internal->howto = m68kcoff_howto_table + 2; break;
    case R_PCRBYTE:	internal->howto = m68kcoff_howto_table + 3; break;
    case R_PCRWORD:	internal->howto = m68kcoff_howto_table + 4; break;
    case R_PCRLONG:	internal->howto = m68kcoff_howto_table + 5; break;
    case R_RELLONG_NEG:	internal->howto = m68kcoff_howto_table + 6; break;
    }
}

STAT_REL int
m68k_howto2rtype (internal)
     reloc_howto_type *internal;
{
  if (internal->pc_relative)
    {
      switch (internal->bitsize)
	{
	case 32: return R_PCRLONG;
	case 16: return R_PCRWORD;
	case 8: return R_PCRBYTE;
	}
    }
  else
    {
      switch (internal->bitsize)
	{
	case 32: return R_RELLONG;
	case 16: return R_RELWORD;
	case 8: return R_RELBYTE;
	}
    }
  return R_RELLONG;
}

STAT_REL reloc_howto_type *
m68k_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    default:			return NULL;
    case BFD_RELOC_8:		return m68kcoff_howto_table + 0;
    case BFD_RELOC_16:		return m68kcoff_howto_table + 1;
    case BFD_RELOC_CTOR:
    case BFD_RELOC_32:		return m68kcoff_howto_table + 2;
    case BFD_RELOC_8_PCREL:	return m68kcoff_howto_table + 3;
    case BFD_RELOC_16_PCREL:	return m68kcoff_howto_table + 4;
    case BFD_RELOC_32_PCREL:	return m68kcoff_howto_table + 5;
      /* FIXME: There doesn't seem to be a code for R_RELLONG_NEG.  */
    }
  /*NOTREACHED*/
}

#endif /* not ONLY_DECLARE_RELOCS */

#define RTYPE2HOWTO(internal, relocentry) \
  m68k_rtype2howto(internal, (relocentry)->r_type)

#define SELECT_RELOC(external, internal) \
  external.r_type = m68k_howto2rtype (internal)

#define coff_bfd_reloc_type_lookup m68k_reloc_type_lookup

#ifndef COFF_COMMON_ADDEND
#ifndef coff_rtype_to_howto

#define coff_rtype_to_howto m68kcoff_rtype_to_howto

static reloc_howto_type *m68kcoff_rtype_to_howto
  PARAMS ((bfd *, asection *, struct internal_reloc *,
	   struct coff_link_hash_entry *, struct internal_syment *,
	   bfd_vma *));

static reloc_howto_type *
m68kcoff_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h ATTRIBUTE_UNUSED;
     struct internal_syment *sym ATTRIBUTE_UNUSED;
     bfd_vma *addendp;
{
  arelent relent;
  reloc_howto_type *howto;

  RTYPE2HOWTO (&relent, rel);

  howto = relent.howto;

  if (howto->pc_relative)
    *addendp += sec->vma;

  return howto;
}

#endif /* ! defined (coff_rtype_to_howto) */
#endif /* ! defined (COFF_COMMON_ADDEND) */

#ifdef COFF_COMMON_ADDEND

/* If COFF_COMMON_ADDEND is defined, then when using m68k COFF the
   value stored in the .text section for a reference to a common
   symbol is the value itself plus any desired offset.  (taken from
   work done by Ian Taylor, Cygnus Support, for I386 COFF).  */

/* If we are producing relocatable output, we need to do some
   adjustments to the object file that are not done by the
   bfd_perform_relocation function.  This function is called by every
   reloc type to make any required adjustments.  */

static bfd_reloc_status_type
m68kcoff_common_addend_special_fn (abfd, reloc_entry, symbol, data,
				   input_section, output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  symvalue diff;

  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;

  if (bfd_is_com_section (symbol->section))
    {
      /* We are relocating a common symbol.  The current value in the
	 object file is ORIG + OFFSET, where ORIG is the value of the
	 common symbol as seen by the object file when it was compiled
	 (this may be zero if the symbol was undefined) and OFFSET is
	 the offset into the common symbol (normally zero, but may be
	 non-zero when referring to a field in a common structure).
	 ORIG is the negative of reloc_entry->addend, which is set by
	 the CALC_ADDEND macro below.  We want to replace the value in
	 the object file with NEW + OFFSET, where NEW is the value of
	 the common symbol which we are going to put in the final
	 object file.  NEW is symbol->value.  */
      diff = symbol->value + reloc_entry->addend;
    }
  else
    {
      /* For some reason bfd_perform_relocation always effectively
	 ignores the addend for a COFF target when producing
	 relocatable output.  This seems to be always wrong for 386
	 COFF, so we handle the addend here instead.  */
      diff = reloc_entry->addend;
    }

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
	    bfd_put_16 (abfd, (bfd_vma) x, addr);
	  }
	  break;

	case 2:
	  {
	    long x = bfd_get_32 (abfd, addr);
	    DOIT (x);
	    bfd_put_32 (abfd, (bfd_vma) x, addr);
	  }
	  break;

	default:
	  abort ();
	}
    }

  /* Now let bfd_perform_relocation finish everything up.  */
  return bfd_reloc_continue;
}

/* Compute the addend of a reloc.  If the reloc is to a common symbol,
   the object file contains the value of the common symbol.  By the
   time this is called, the linker may be using a different symbol
   from a different object file with a different value.  Therefore, we
   hack wildly to locate the original symbol from this file so that we
   can make the correct adjustment.  This macro sets coffsym to the
   symbol from the original file, and uses it to set the addend value
   correctly.  If this is not a common symbol, the usual addend
   calculation is done, except that an additional tweak is needed for
   PC relative relocs.
   FIXME: This macro refers to symbols and asect; these are from the
   calling function, not the macro arguments.  */

#define CALC_ADDEND(abfd, ptr, reloc, cache_ptr)		\
  {								\
    coff_symbol_type *coffsym = (coff_symbol_type *) NULL;	\
    if (ptr && bfd_asymbol_bfd (ptr) != abfd)			\
      coffsym = (obj_symbols (abfd)				\
	         + (cache_ptr->sym_ptr_ptr - symbols));		\
    else if (ptr)						\
      coffsym = coff_symbol_from (abfd, ptr);			\
    if (coffsym != (coff_symbol_type *) NULL			\
	&& coffsym->native->u.syment.n_scnum == 0)		\
      cache_ptr->addend = - coffsym->native->u.syment.n_value;	\
    else if (ptr && bfd_asymbol_bfd (ptr) == abfd		\
	     && ptr->section != (asection *) NULL)		\
      cache_ptr->addend = - (ptr->section->vma + ptr->value);	\
    else							\
      cache_ptr->addend = 0;					\
    if (ptr && (reloc.r_type == R_PCRBYTE			\
		|| reloc.r_type == R_PCRWORD			\
		|| reloc.r_type == R_PCRLONG))			\
      cache_ptr->addend += asect->vma;				\
  }

#ifndef coff_rtype_to_howto

/* coff-m68k.c uses the special COFF backend linker.  We need to
   adjust common symbols.  */

static reloc_howto_type *
m68kcoff_common_addend_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h;
     struct internal_syment *sym;
     bfd_vma *addendp;
{
  arelent relent;
  reloc_howto_type *howto;

  RTYPE2HOWTO (&relent, rel);

  howto = relent.howto;

  if (howto->pc_relative)
    *addendp += sec->vma;

  if (sym != NULL && sym->n_scnum == 0 && sym->n_value != 0)
    {
      /* This is a common symbol.  The section contents include the
	 size (sym->n_value) as an addend.  The relocate_section
	 function will be adding in the final value of the symbol.  We
	 need to subtract out the current size in order to get the
	 correct result.  */
      BFD_ASSERT (h != NULL);
      *addendp -= sym->n_value;
    }

  /* If the output symbol is common (in which case this must be a
     relocatable link), we need to add in the final size of the
     common symbol.  */
  if (h != NULL && h->root.type == bfd_link_hash_common)
    *addendp += h->root.u.c.size;

  return howto;
}

#define coff_rtype_to_howto m68kcoff_common_addend_rtype_to_howto

#endif /* ! defined (coff_rtype_to_howto) */

#endif /* COFF_COMMON_ADDEND */

#if !defined ONLY_DECLARE_RELOCS && ! defined STATIC_RELOCS
/* Given a .data section and a .emreloc in-memory section, store
   relocation information into the .emreloc section which can be
   used at runtime to relocate the section.  This is called by the
   linker when the --embedded-relocs switch is used.  This is called
   after the add_symbols entry point has been called for all the
   objects, and before the final_link entry point is called.  */

bfd_boolean
bfd_m68k_coff_create_embedded_relocs (abfd, info, datasec, relsec, errmsg)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *datasec;
     asection *relsec;
     char **errmsg;
{
  char *extsyms;
  bfd_size_type symesz;
  struct internal_reloc *irel, *irelend;
  bfd_byte *p;
  bfd_size_type amt;

  BFD_ASSERT (! info->relocatable);

  *errmsg = NULL;

  if (datasec->reloc_count == 0)
    return TRUE;

  extsyms = obj_coff_external_syms (abfd);
  symesz = bfd_coff_symesz (abfd);

  irel = _bfd_coff_read_internal_relocs (abfd, datasec, TRUE, NULL, FALSE,
					 NULL);
  irelend = irel + datasec->reloc_count;

  amt = (bfd_size_type) datasec->reloc_count * 12;
  relsec->contents = (bfd_byte *) bfd_alloc (abfd, amt);
  if (relsec->contents == NULL)
    return FALSE;

  p = relsec->contents;

  for (; irel < irelend; irel++, p += 12)
    {
      asection *targetsec;

      /* We are going to write a four byte longword into the runtime
       reloc section.  The longword will be the address in the data
       section which must be relocated.  It is followed by the name
       of the target section NUL-padded or truncated to 8
       characters.  */

      /* We can only relocate absolute longword relocs at run time.  */
      if (irel->r_type != R_RELLONG)
	{
	  *errmsg = _("unsupported reloc type");
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}

      if (irel->r_symndx == -1)
	targetsec = bfd_abs_section_ptr;
      else
	{
	  struct coff_link_hash_entry *h;

	  h = obj_coff_sym_hashes (abfd)[irel->r_symndx];
	  if (h == NULL)
	    {
	      struct internal_syment isym;

	      bfd_coff_swap_sym_in (abfd, extsyms + symesz * irel->r_symndx,
				    &isym);
	      targetsec = coff_section_from_bfd_index (abfd, isym.n_scnum);
	    }
	  else if (h->root.type == bfd_link_hash_defined
		   || h->root.type == bfd_link_hash_defweak)
	    targetsec = h->root.u.def.section;
	  else
	    targetsec = NULL;
	}

      bfd_put_32 (abfd,
		  (irel->r_vaddr - datasec->vma + datasec->output_offset), p);
      memset (p + 4, 0, 8);
      if (targetsec != NULL)
	strncpy (p + 4, targetsec->output_section->name, 8);
    }

  return TRUE;
}
#endif /* neither ONLY_DECLARE_RELOCS not STATIC_RELOCS  */

#define coff_bfd_is_local_label_name m68k_coff_is_local_label_name

#define coff_relocate_section _bfd_coff_generic_relocate_section

#include "coffcode.h"

#ifndef TARGET_SYM
#define TARGET_SYM m68kcoff_vec
#endif

#ifndef TARGET_NAME
#define TARGET_NAME "coff-m68k"
#endif

#ifdef NAMES_HAVE_UNDERSCORE
CREATE_BIG_COFF_TARGET_VEC (TARGET_SYM, TARGET_NAME, D_PAGED, 0, '_', NULL, COFF_SWAP_TABLE)
#else
CREATE_BIG_COFF_TARGET_VEC (TARGET_SYM, TARGET_NAME, D_PAGED, 0, 0, NULL, COFF_SWAP_TABLE)
#endif
