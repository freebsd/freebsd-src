/* BFD back-end for Intel 960 COFF files.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1997, 1999, 2000, 2001,
   2002, 2003, 2004 Free Software Foundation, Inc.
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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#define I960 1
#define BADMAG(x) I960BADMAG(x)

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/i960.h"
#include "coff/internal.h"
#include "libcoff.h"		/* to allow easier abstraction-breaking */

static bfd_boolean coff_i960_is_local_label_name
  PARAMS ((bfd *, const char *));
static bfd_reloc_status_type optcall_callback
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type coff_i960_relocate
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *coff_i960_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static bfd_boolean coff_i960_start_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_boolean coff_i960_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));
static bfd_boolean coff_i960_adjust_symndx
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *,
	   struct internal_reloc *, bfd_boolean *));

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (3)
#define COFF_ALIGN_IN_SECTION_HEADER 1

#define GET_SCNHDR_ALIGN H_GET_32
#define PUT_SCNHDR_ALIGN H_PUT_32

/* The i960 does not support an MMU, so COFF_PAGE_SIZE can be
   arbitrarily small.  */
#define COFF_PAGE_SIZE 1

#define COFF_LONG_FILENAMES

/* This set of local label names is taken from gas.  */

static bfd_boolean
coff_i960_is_local_label_name (abfd, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name;
{
  return (name[0] == 'L'
	  || (name[0] == '.'
	      && (name[1] == 'C'
		  || name[1] == 'I'
		  || name[1] == '.')));
}

/* This is just like the usual CALC_ADDEND, but it includes the
   section VMA for PC relative relocs.  */
#ifndef CALC_ADDEND
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
             && ptr->section != (asection *) NULL)              \
      cache_ptr->addend = - (ptr->section->vma + ptr->value);   \
    else                                                        \
      cache_ptr->addend = 0;                                    \
    if (ptr && (reloc.r_type == 25 || reloc.r_type == 27))	\
      cache_ptr->addend += asect->vma;				\
  }
#endif

#define CALLS	 0x66003800	/* Template for 'calls' instruction	*/
#define BAL	 0x0b000000	/* Template for 'bal' instruction	*/
#define BAL_MASK 0x00ffffff

static bfd_reloc_status_type
optcall_callback (abfd, reloc_entry, symbol_in, data,
		  input_section, ignore_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol_in;
     PTR data;
     asection *input_section;
     bfd *ignore_bfd ATTRIBUTE_UNUSED;
     char **error_message;
{
  /* This item has already been relocated correctly, but we may be
   * able to patch in yet better code - done by digging out the
   * correct info on this symbol */
  bfd_reloc_status_type result;
  coff_symbol_type *cs = coffsymbol(symbol_in);

  /* Don't do anything with symbols which aren't tied up yet,
     except move the reloc.  */
  if (bfd_is_und_section (cs->symbol.section)) {
    reloc_entry->address += input_section->output_offset;
    return bfd_reloc_ok;
  }

  /* So the target symbol has to be of coff type, and the symbol
     has to have the correct native information within it */
  if ((bfd_asymbol_flavour(&cs->symbol) != bfd_target_coff_flavour)
      || (cs->native == (combined_entry_type *)NULL))
    {
      /* This is interesting, consider the case where we're outputting coff
	 from a mix n match input, linking from coff to a symbol defined in a
	 bout file will cause this match to be true. Should I complain?  This
	 will only work if the bout symbol is non leaf.  */
      *error_message =
	(char *) _("uncertain calling convention for non-COFF symbol");
      result = bfd_reloc_dangerous;
    }
  else
    {
    switch (cs->native->u.syment.n_sclass)
      {
      case C_LEAFSTAT:
      case C_LEAFEXT:
  	/* This is a call to a leaf procedure, replace instruction with a bal
	   to the correct location.  */
	{
	  union internal_auxent *aux = &((cs->native+2)->u.auxent);
	  int word = bfd_get_32 (abfd, (bfd_byte *)data + reloc_entry->address);
	  int olf = (aux->x_bal.x_balntry - cs->native->u.syment.n_value);
	  BFD_ASSERT(cs->native->u.syment.n_numaux==2);

	  /* We replace the original call instruction with a bal to
	     the bal entry point - the offset of which is described in
	     the 2nd auxent of the original symbol. We keep the native
	     sym and auxents untouched, so the delta between the two
	     is the offset of the bal entry point.  */
	  word = ((word +  olf)  & BAL_MASK) | BAL;
  	  bfd_put_32 (abfd, (bfd_vma) word,
		      (bfd_byte *) data + reloc_entry->address);
  	}
	result = bfd_reloc_ok;
	break;
      case C_SCALL:
	/* This is a call to a system call, replace with a calls to # */
	BFD_ASSERT(0);
	result = bfd_reloc_ok;
	break;
      default:
	result = bfd_reloc_ok;
	break;
      }
  }
  return result;
}

/* i960 COFF is used by VxWorks 5.1.  However, VxWorks 5.1 does not
   appear to correctly handle a reloc against a symbol defined in the
   same object file.  It appears to simply discard such relocs, rather
   than adding their values into the object file.  We handle this here
   by converting all relocs against defined symbols into relocs
   against the section symbol, when generating a relocatable output
   file.

   Note that this function is only called if we are not using the COFF
   specific backend linker.  It only does something when doing a
   relocatable link, which will almost certainly fail when not
   generating COFF i960 output, so this function is actually no longer
   useful.  It was used before this target was converted to use the
   COFF specific backend linker.  */

static bfd_reloc_status_type
coff_i960_relocate (abfd, reloc_entry, symbol, data, input_section,
		    output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  asection *osec;

  if (output_bfd == NULL)
    {
      /* Not generating relocatable output file.  */
      return bfd_reloc_continue;
    }

  if (bfd_is_und_section (bfd_get_section (symbol)))
    {
      /* Symbol is not defined, so no need to worry about it.  */
      return bfd_reloc_continue;
    }

  if (bfd_is_com_section (bfd_get_section (symbol)))
    {
      /* I don't really know what the right action is for a common
         symbol.  */
      return bfd_reloc_continue;
    }

  /* Convert the reloc to use the section symbol.  FIXME: This method
     is ridiculous.  */
  osec = bfd_get_section (symbol)->output_section;
  if (coff_section_data (output_bfd, osec) != NULL
      && coff_section_data (output_bfd, osec)->tdata != NULL)
    reloc_entry->sym_ptr_ptr =
      (asymbol **) coff_section_data (output_bfd, osec)->tdata;
  else
    {
      const char *sec_name;
      asymbol **syms, **sym_end;

      sec_name = bfd_get_section_name (output_bfd, osec);
      syms = bfd_get_outsymbols (output_bfd);
      sym_end = syms + bfd_get_symcount (output_bfd);
      for (; syms < sym_end; syms++)
	{
	  if (bfd_asymbol_name (*syms) != NULL
	      && (*syms)->value == 0
	      && strcmp ((*syms)->section->output_section->name,
			 sec_name) == 0)
	    break;
	}

      if (syms >= sym_end)
	abort ();

      reloc_entry->sym_ptr_ptr = syms;

      if (coff_section_data (output_bfd, osec) == NULL)
	{
	  bfd_size_type amt = sizeof (struct coff_section_tdata);
	  osec->used_by_bfd = (PTR) bfd_zalloc (abfd, amt);
	  if (osec->used_by_bfd == NULL)
	    return bfd_reloc_overflow;
	}
      coff_section_data (output_bfd, osec)->tdata = (PTR) syms;
    }

  /* Let bfd_perform_relocation do its thing, which will include
     stuffing the symbol addend into the object file.  */
  return bfd_reloc_continue;
}

static reloc_howto_type howto_rellong =
  HOWTO ((unsigned int) R_RELLONG, 0, 2, 32,FALSE, 0,
	 complain_overflow_bitfield, coff_i960_relocate,"rellong", TRUE,
	 0xffffffff, 0xffffffff, 0);
static reloc_howto_type howto_iprmed =
  HOWTO (R_IPRMED, 0, 2, 24,TRUE,0, complain_overflow_signed,
	 coff_i960_relocate, "iprmed ", TRUE, 0x00ffffff, 0x00ffffff, 0);
static reloc_howto_type howto_optcall =
  HOWTO (R_OPTCALL, 0,2,24,TRUE,0, complain_overflow_signed,
	 optcall_callback, "optcall", TRUE, 0x00ffffff, 0x00ffffff, 0);

static reloc_howto_type *
coff_i960_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    default:
      return 0;
    case BFD_RELOC_I960_CALLJ:
      return &howto_optcall;
    case BFD_RELOC_32:
    case BFD_RELOC_CTOR:
      return &howto_rellong;
    case BFD_RELOC_24_PCREL:
      return &howto_iprmed;
    }
}

/* The real code is in coffcode.h */

#define RTYPE2HOWTO(cache_ptr, dst) \
{							\
   reloc_howto_type *howto_ptr;				\
   switch ((dst)->r_type) {				\
     case 17: howto_ptr = &howto_rellong; break;	\
     case 25: howto_ptr = &howto_iprmed; break;		\
     case 27: howto_ptr = &howto_optcall; break;	\
     default: howto_ptr = 0; break;			\
     }							\
   (cache_ptr)->howto = howto_ptr;			\
 }

/* i960 COFF is used by VxWorks 5.1.  However, VxWorks 5.1 does not
   appear to correctly handle a reloc against a symbol defined in the
   same object file.  It appears to simply discard such relocs, rather
   than adding their values into the object file.  We handle this by
   converting all relocs against global symbols into relocs against
   internal symbols at the start of the section.  This routine is
   called at the start of the linking process, and it creates the
   necessary symbols.  */

static bfd_boolean
coff_i960_start_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  bfd_size_type symesz = bfd_coff_symesz (abfd);
  asection *o;
  bfd_byte *esym;

  if (! info->relocatable)
    return TRUE;

  esym = (bfd_byte *) bfd_malloc (symesz);
  if (esym == NULL)
    return FALSE;

  if (bfd_seek (abfd, obj_sym_filepos (abfd), SEEK_SET) != 0)
    return FALSE;

  for (o = abfd->sections; o != NULL; o = o->next)
    {
      struct internal_syment isym;

      strncpy (isym._n._n_name, o->name, SYMNMLEN);
      isym.n_value = 0;
      isym.n_scnum = o->target_index;
      isym.n_type = T_NULL;
      isym.n_sclass = C_STAT;
      isym.n_numaux = 0;

      bfd_coff_swap_sym_out (abfd, (PTR) &isym, (PTR) esym);

      if (bfd_bwrite (esym, symesz, abfd) != symesz)
	{
	  free (esym);
	  return FALSE;
	}

      obj_raw_syment_count (abfd) += 1;
    }

  free (esym);

  return TRUE;
}

/* The reloc processing routine for the optimized COFF linker.  */

static bfd_boolean
coff_i960_relocate_section (output_bfd, info, input_bfd, input_section,
			    contents, relocs, syms, sections)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     struct internal_reloc *relocs;
     struct internal_syment *syms;
     asection **sections;
{
  struct internal_reloc *rel;
  struct internal_reloc *relend;

  rel = relocs;
  relend = rel + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      long symndx;
      struct coff_link_hash_entry *h;
      struct internal_syment *sym;
      bfd_vma addend;
      bfd_vma val;
      reloc_howto_type *howto;
      bfd_reloc_status_type rstat = bfd_reloc_ok;
      bfd_boolean done;

      symndx = rel->r_symndx;

      if (symndx == -1)
	{
	  h = NULL;
	  sym = NULL;
	}
      else
	{
	  h = obj_coff_sym_hashes (input_bfd)[symndx];
	  sym = syms + symndx;
	}

      if (sym != NULL && sym->n_scnum != 0)
	addend = - sym->n_value;
      else
	addend = 0;

      switch (rel->r_type)
	{
	case 17: howto = &howto_rellong; break;
	case 25: howto = &howto_iprmed; break;
	case 27: howto = &howto_optcall; break;
	default:
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}

      val = 0;

      if (h == NULL)
	{
	  asection *sec;

	  if (symndx == -1)
	    {
	      sec = bfd_abs_section_ptr;
	      val = 0;
	    }
	  else
	    {
	      sec = sections[symndx];
              val = (sec->output_section->vma
		     + sec->output_offset
		     + sym->n_value
		     - sec->vma);
	    }
	}
      else
	{
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      asection *sec;

	      sec = h->root.u.def.section;
	      val = (h->root.u.def.value
		     + sec->output_section->vma
		     + sec->output_offset);
	    }
	  else if (! info->relocatable)
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd, input_section,
		      rel->r_vaddr - input_section->vma, TRUE)))
		return FALSE;
	    }
	}

      done = FALSE;

      if (howto->type == R_OPTCALL && ! info->relocatable && symndx != -1)
	{
	  int class;

	  if (h != NULL)
	    class = h->class;
	  else
	    class = sym->n_sclass;

	  switch (class)
	    {
	    case C_NULL:
	      /* This symbol is apparently not from a COFF input file.
                 We warn, and then assume that it is not a leaf
                 function.  */
	      if (! ((*info->callbacks->reloc_dangerous)
		     (info,
		      _("uncertain calling convention for non-COFF symbol"),
		      input_bfd, input_section,
		      rel->r_vaddr - input_section->vma)))
		return FALSE;
	      break;
	    case C_LEAFSTAT:
	    case C_LEAFEXT:
	      /* This is a call to a leaf procedure; use the bal
                 instruction.  */
	      {
		long olf;
		unsigned long word;

		if (h != NULL)
		  {
		    BFD_ASSERT (h->numaux == 2);
		    olf = h->aux[1].x_bal.x_balntry;
		  }
		else
		  {
		    bfd_byte *esyms;
		    union internal_auxent aux;

		    BFD_ASSERT (sym->n_numaux == 2);
		    esyms = (bfd_byte *) obj_coff_external_syms (input_bfd);
		    esyms += (symndx + 2) * bfd_coff_symesz (input_bfd);
		    bfd_coff_swap_aux_in (input_bfd, (PTR) esyms, sym->n_type,
					  sym->n_sclass, 1, sym->n_numaux,
					  (PTR) &aux);
		    olf = aux.x_bal.x_balntry;
		  }

		word = bfd_get_32 (input_bfd,
				   (contents
				    + (rel->r_vaddr - input_section->vma)));
		word = ((word + olf - val) & BAL_MASK) | BAL;
		bfd_put_32 (input_bfd,
			    (bfd_vma) word,
			    contents + (rel->r_vaddr - input_section->vma));
		done = TRUE;
	      }
	      break;
	    case C_SCALL:
	      BFD_ASSERT (0);
	      break;
	    }
	}

      if (! done)
	{
	  if (howto->pc_relative)
	    addend += input_section->vma;
	  rstat = _bfd_final_link_relocate (howto, input_bfd, input_section,
					    contents,
					    rel->r_vaddr - input_section->vma,
					    val, addend);
	}

      switch (rstat)
	{
	default:
	  abort ();
	case bfd_reloc_ok:
	  break;
	case bfd_reloc_overflow:
	  {
	    const char *name;
	    char buf[SYMNMLEN + 1];

	    if (symndx == -1)
	      name = "*ABS*";
	    else if (h != NULL)
	      name = NULL;
	    else
	      {
		name = _bfd_coff_internal_syment_name (input_bfd, sym, buf);
		if (name == NULL)
		  return FALSE;
	      }

	    if (! ((*info->callbacks->reloc_overflow)
		   (info, (h ? &h->root : NULL), name, howto->name,
		    (bfd_vma) 0, input_bfd, input_section,
		    rel->r_vaddr - input_section->vma)))
	      return FALSE;
	  }
	}
    }

  return TRUE;
}

/* Adjust the symbol index of any reloc against a global symbol to
   instead be a reloc against the internal symbol we created specially
   for the section.  */

static bfd_boolean
coff_i960_adjust_symndx (obfd, info, ibfd, sec, irel, adjustedp)
     bfd *obfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     bfd *ibfd;
     asection *sec ATTRIBUTE_UNUSED;
     struct internal_reloc *irel;
     bfd_boolean *adjustedp;
{
  struct coff_link_hash_entry *h;

  *adjustedp = FALSE;

  h = obj_coff_sym_hashes (ibfd)[irel->r_symndx];
  if (h == NULL
      || (h->root.type != bfd_link_hash_defined
	  && h->root.type != bfd_link_hash_defweak))
    return TRUE;

  irel->r_symndx = h->root.u.def.section->output_section->target_index - 1;
  *adjustedp = TRUE;

  return TRUE;
}

#define coff_bfd_is_local_label_name coff_i960_is_local_label_name

#define coff_start_final_link coff_i960_start_final_link

#define coff_relocate_section coff_i960_relocate_section

#define coff_adjust_symndx coff_i960_adjust_symndx

#define coff_bfd_reloc_type_lookup coff_i960_reloc_type_lookup

#include "coffcode.h"

extern const bfd_target icoff_big_vec;

CREATE_LITTLE_COFF_TARGET_VEC (icoff_little_vec, "coff-Intel-little", 0, 0, '_', & icoff_big_vec, COFF_SWAP_TABLE)

const bfd_target icoff_big_vec =
{
  "coff-Intel-big",		/* name */
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  '_',				/* leading underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */

bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

  {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
     bfd_generic_archive_p, _bfd_dummy_target},
  {bfd_false, coff_mkobject,	/* bfd_set_format */
     _bfd_generic_mkarchive, bfd_false},
  {bfd_false, coff_write_object_contents,	/* bfd_write_contents */
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

  & icoff_little_vec,

  COFF_SWAP_TABLE
};
