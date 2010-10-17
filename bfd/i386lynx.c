/* BFD back-end for i386 a.out binaries under LynxOS.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 2001, 2002, 2003
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

#define N_SHARED_LIB(x) 0

#define TEXT_START_ADDR 0
#define TARGET_PAGE_SIZE 4096
#define SEGMENT_SIZE TARGET_PAGE_SIZE
#define DEFAULT_ARCH bfd_arch_i386

/* Do not "beautify" the CONCAT* macro args.  Traditional C will not
   remove whitespace added here, and thus will fail to concatenate
   the tokens.  */
#define MY(OP) CONCAT2 (i386lynx_aout_,OP)
#define TARGETNAME "a.out-i386-lynx"

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#ifndef WRITE_HEADERS
#define WRITE_HEADERS(abfd, execp)					      \
      {									      \
	bfd_size_type text_size; /* dummy vars */			      \
	file_ptr text_end;						      \
	if (adata(abfd).magic == undecided_magic)			      \
	  NAME(aout,adjust_sizes_and_vmas) (abfd, &text_size, &text_end);     \
    									      \
	execp->a_syms = bfd_get_symcount (abfd) * EXTERNAL_NLIST_SIZE;	      \
	execp->a_entry = bfd_get_start_address (abfd);			      \
    									      \
	execp->a_trsize = ((obj_textsec (abfd)->reloc_count) *		      \
			   obj_reloc_entry_size (abfd));		      \
	execp->a_drsize = ((obj_datasec (abfd)->reloc_count) *		      \
			   obj_reloc_entry_size (abfd));		      \
	NAME(aout,swap_exec_header_out) (abfd, execp, &exec_bytes);	      \
									      \
	if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0		      \
	    || bfd_bwrite ((PTR) &exec_bytes, (bfd_size_type) EXEC_BYTES_SIZE, \
			  abfd) != EXEC_BYTES_SIZE)			      \
	  return FALSE;							      \
	/* Now write out reloc info, followed by syms and strings */	      \
  									      \
	if (bfd_get_symcount (abfd) != 0) 				      \
	    {								      \
	      if (bfd_seek (abfd, (file_ptr) (N_SYMOFF(*execp)), SEEK_SET)    \
		  != 0)							      \
	        return FALSE;						      \
									      \
	      if (! NAME(aout,write_syms) (abfd)) return FALSE;		      \
									      \
	      if (bfd_seek (abfd, (file_ptr) (N_TRELOFF(*execp)), SEEK_SET)   \
		  != 0)							      \
	        return FALSE;						      \
									      \
	      if (!NAME(lynx,squirt_out_relocs) (abfd, obj_textsec (abfd)))   \
		return FALSE;						      \
	      if (bfd_seek (abfd, (file_ptr) (N_DRELOFF(*execp)), SEEK_SET)   \
		  != 0)							      \
	        return 0;						      \
									      \
	      if (!NAME(lynx,squirt_out_relocs) (abfd, obj_datasec (abfd)))   \
		return FALSE;						      \
	    }								      \
      }
#endif

#include "libaout.h"
#include "aout/aout64.h"

void NAME (lynx,swap_std_reloc_out)
  PARAMS ((bfd *, arelent *, struct reloc_std_external *));
void NAME (lynx,swap_ext_reloc_out)
  PARAMS ((bfd *, arelent *, struct reloc_ext_external *));
void NAME (lynx,swap_ext_reloc_in)
  PARAMS ((bfd *, struct reloc_ext_external *, arelent *, asymbol **,
	   bfd_size_type));
void NAME (lynx,swap_std_reloc_in)
  PARAMS ((bfd *, struct reloc_std_external *, arelent *, asymbol **,
	   bfd_size_type));
bfd_boolean NAME (lynx,slurp_reloc_table)
  PARAMS ((bfd *, sec_ptr, asymbol **));
bfd_boolean NAME (lynx,squirt_out_relocs)
  PARAMS ((bfd *, asection *));
long NAME (lynx,canonicalize_reloc)
  PARAMS ((bfd *, sec_ptr, arelent **, asymbol **));

#ifdef LYNX_CORE

char *lynx_core_file_failing_command ();
int lynx_core_file_failing_signal ();
bfd_boolean lynx_core_file_matches_executable_p ();
const bfd_target *lynx_core_file_p ();

#define	MY_core_file_failing_command lynx_core_file_failing_command
#define	MY_core_file_failing_signal lynx_core_file_failing_signal
#define	MY_core_file_matches_executable_p lynx_core_file_matches_executable_p
#define	MY_core_file_p lynx_core_file_p

#endif /* LYNX_CORE */


#define KEEPIT udata.i

extern reloc_howto_type aout_32_ext_howto_table[];
extern reloc_howto_type aout_32_std_howto_table[];

/* Standard reloc stuff */
/* Output standard relocation information to a file in target byte order. */

void
NAME(lynx,swap_std_reloc_out) (abfd, g, natptr)
     bfd *abfd;
     arelent *g;
     struct reloc_std_external *natptr;
{
  int r_index;
  asymbol *sym = *(g->sym_ptr_ptr);
  int r_extern;
  unsigned int r_length;
  int r_pcrel;
  int r_baserel, r_jmptable, r_relative;
  unsigned int r_addend;
  asection *output_section = sym->section->output_section;

  PUT_WORD (abfd, g->address, natptr->r_address);

  r_length = g->howto->size;	/* Size as a power of two */
  r_pcrel = (int) g->howto->pc_relative;	/* Relative to PC? */
  /* r_baserel, r_jmptable, r_relative???  FIXME-soon */
  r_baserel = 0;
  r_jmptable = 0;
  r_relative = 0;

  r_addend = g->addend + (*(g->sym_ptr_ptr))->section->output_section->vma;

  /* name was clobbered by aout_write_syms to be symbol index */

  /* If this relocation is relative to a symbol then set the
     r_index to the symbols index, and the r_extern bit.

     Absolute symbols can come in in two ways, either as an offset
     from the abs section, or as a symbol which has an abs value.
     check for that here
  */


  if (bfd_is_com_section (output_section)
      || bfd_is_abs_section (output_section)
      || bfd_is_und_section (output_section))
    {
      if (bfd_abs_section_ptr->symbol == sym)
	{
	  /* Whoops, looked like an abs symbol, but is really an offset
	     from the abs section */
	  r_index = 0;
	  r_extern = 0;
	}
      else
	{
	  /* Fill in symbol */
	  r_extern = 1;
	  r_index = (*g->sym_ptr_ptr)->KEEPIT;
	}
    }
  else
    {
      /* Just an ordinary section */
      r_extern = 0;
      r_index = output_section->target_index;
    }

  /* now the fun stuff */
  if (bfd_header_big_endian (abfd))
    {
      natptr->r_index[0] = r_index >> 16;
      natptr->r_index[1] = r_index >> 8;
      natptr->r_index[2] = r_index;
      natptr->r_type[0] =
	(r_extern ? RELOC_STD_BITS_EXTERN_BIG : 0)
	| (r_pcrel ? RELOC_STD_BITS_PCREL_BIG : 0)
	| (r_baserel ? RELOC_STD_BITS_BASEREL_BIG : 0)
	| (r_jmptable ? RELOC_STD_BITS_JMPTABLE_BIG : 0)
	| (r_relative ? RELOC_STD_BITS_RELATIVE_BIG : 0)
	| (r_length << RELOC_STD_BITS_LENGTH_SH_BIG);
    }
  else
    {
      natptr->r_index[2] = r_index >> 16;
      natptr->r_index[1] = r_index >> 8;
      natptr->r_index[0] = r_index;
      natptr->r_type[0] =
	(r_extern ? RELOC_STD_BITS_EXTERN_LITTLE : 0)
	| (r_pcrel ? RELOC_STD_BITS_PCREL_LITTLE : 0)
	| (r_baserel ? RELOC_STD_BITS_BASEREL_LITTLE : 0)
	| (r_jmptable ? RELOC_STD_BITS_JMPTABLE_LITTLE : 0)
	| (r_relative ? RELOC_STD_BITS_RELATIVE_LITTLE : 0)
	| (r_length << RELOC_STD_BITS_LENGTH_SH_LITTLE);
    }
}


/* Extended stuff */
/* Output extended relocation information to a file in target byte order. */

void
NAME(lynx,swap_ext_reloc_out) (abfd, g, natptr)
     bfd *abfd;
     arelent *g;
     register struct reloc_ext_external *natptr;
{
  int r_index;
  int r_extern;
  unsigned int r_type;
  unsigned int r_addend;
  asymbol *sym = *(g->sym_ptr_ptr);
  asection *output_section = sym->section->output_section;

  PUT_WORD (abfd, g->address, natptr->r_address);

  r_type = (unsigned int) g->howto->type;

  r_addend = g->addend + (*(g->sym_ptr_ptr))->section->output_section->vma;


  /* If this relocation is relative to a symbol then set the
     r_index to the symbols index, and the r_extern bit.

     Absolute symbols can come in in two ways, either as an offset
     from the abs section, or as a symbol which has an abs value.
     check for that here
     */

  if (bfd_is_com_section (output_section)
      || bfd_is_abs_section (output_section)
      || bfd_is_und_section (output_section))
    {
      if (bfd_abs_section_ptr->symbol == sym)
	{
	  /* Whoops, looked like an abs symbol, but is really an offset
	 from the abs section */
	  r_index = 0;
	  r_extern = 0;
	}
      else
	{
	  r_extern = 1;
	  r_index = (*g->sym_ptr_ptr)->KEEPIT;
	}
    }
  else
    {
      /* Just an ordinary section */
      r_extern = 0;
      r_index = output_section->target_index;
    }


  /* now the fun stuff */
  if (bfd_header_big_endian (abfd))
    {
      natptr->r_index[0] = r_index >> 16;
      natptr->r_index[1] = r_index >> 8;
      natptr->r_index[2] = r_index;
      natptr->r_type[0] =
	(r_extern ? RELOC_EXT_BITS_EXTERN_BIG : 0)
	| (r_type << RELOC_EXT_BITS_TYPE_SH_BIG);
    }
  else
    {
      natptr->r_index[2] = r_index >> 16;
      natptr->r_index[1] = r_index >> 8;
      natptr->r_index[0] = r_index;
      natptr->r_type[0] =
	(r_extern ? RELOC_EXT_BITS_EXTERN_LITTLE : 0)
	| (r_type << RELOC_EXT_BITS_TYPE_SH_LITTLE);
    }

  PUT_WORD (abfd, r_addend, natptr->r_addend);
}

/* BFD deals internally with all things based from the section they're
   in. so, something in 10 bytes into a text section  with a base of
   50 would have a symbol (.text+10) and know .text vma was 50.

   Aout keeps all it's symbols based from zero, so the symbol would
   contain 60. This macro subs the base of each section from the value
   to give the true offset from the section */


#define MOVE_ADDRESS(ad)       						\
  if (r_extern) {							\
   /* undefined symbol */						\
     cache_ptr->sym_ptr_ptr = symbols + r_index;			\
     cache_ptr->addend = ad;						\
     } else {								\
    /* defined, section relative. replace symbol with pointer to    	\
       symbol which points to section  */				\
    switch (r_index) {							\
    case N_TEXT:							\
    case N_TEXT | N_EXT:						\
      cache_ptr->sym_ptr_ptr  = obj_textsec(abfd)->symbol_ptr_ptr;	\
      cache_ptr->addend = ad  - su->textsec->vma;			\
      break;								\
    case N_DATA:							\
    case N_DATA | N_EXT:						\
      cache_ptr->sym_ptr_ptr  = obj_datasec(abfd)->symbol_ptr_ptr;	\
      cache_ptr->addend = ad - su->datasec->vma;			\
      break;								\
    case N_BSS:								\
    case N_BSS | N_EXT:							\
      cache_ptr->sym_ptr_ptr  = obj_bsssec(abfd)->symbol_ptr_ptr;	\
      cache_ptr->addend = ad - su->bsssec->vma;				\
      break;								\
    default:								\
    case N_ABS:								\
    case N_ABS | N_EXT:							\
     cache_ptr->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;	\
      cache_ptr->addend = ad;						\
      break;								\
    }									\
  }     								\

void
NAME(lynx,swap_ext_reloc_in) (abfd, bytes, cache_ptr, symbols, symcount)
     bfd *abfd;
     struct reloc_ext_external *bytes;
     arelent *cache_ptr;
     asymbol **symbols;
     bfd_size_type symcount ATTRIBUTE_UNUSED;
{
  int r_index;
  int r_extern;
  unsigned int r_type;
  struct aoutdata *su = &(abfd->tdata.aout_data->a);

  cache_ptr->address = (GET_SWORD (abfd, bytes->r_address));

  r_index = bytes->r_index[1];
  r_extern = (0 != (bytes->r_index[0] & RELOC_EXT_BITS_EXTERN_BIG));
  r_type = (bytes->r_index[0] & RELOC_EXT_BITS_TYPE_BIG)
    >> RELOC_EXT_BITS_TYPE_SH_BIG;

  cache_ptr->howto = aout_32_ext_howto_table + r_type;
  MOVE_ADDRESS (GET_SWORD (abfd, bytes->r_addend));
}

void
NAME(lynx,swap_std_reloc_in) (abfd, bytes, cache_ptr, symbols, symcount)
     bfd *abfd;
     struct reloc_std_external *bytes;
     arelent *cache_ptr;
     asymbol **symbols;
     bfd_size_type symcount ATTRIBUTE_UNUSED;
{
  int r_index;
  int r_extern;
  unsigned int r_length;
  int r_pcrel;
  int r_baserel, r_jmptable, r_relative;
  struct aoutdata *su = &(abfd->tdata.aout_data->a);

  cache_ptr->address = H_GET_32 (abfd, bytes->r_address);

  r_index = bytes->r_index[1];
  r_extern = (0 != (bytes->r_index[0] & RELOC_STD_BITS_EXTERN_BIG));
  r_pcrel = (0 != (bytes->r_index[0] & RELOC_STD_BITS_PCREL_BIG));
  r_baserel = (0 != (bytes->r_index[0] & RELOC_STD_BITS_BASEREL_BIG));
  r_jmptable = (0 != (bytes->r_index[0] & RELOC_STD_BITS_JMPTABLE_BIG));
  r_relative = (0 != (bytes->r_index[0] & RELOC_STD_BITS_RELATIVE_BIG));
  r_length = (bytes->r_index[0] & RELOC_STD_BITS_LENGTH_BIG)
    >> RELOC_STD_BITS_LENGTH_SH_BIG;

  cache_ptr->howto = aout_32_std_howto_table + r_length + 4 * r_pcrel;
  /* FIXME-soon:  Roll baserel, jmptable, relative bits into howto setting */

  MOVE_ADDRESS (0);
}

/* Reloc hackery */

bfd_boolean
NAME(lynx,slurp_reloc_table) (abfd, asect, symbols)
     bfd *abfd;
     sec_ptr asect;
     asymbol **symbols;
{
  bfd_size_type count;
  bfd_size_type reloc_size;
  PTR relocs;
  arelent *reloc_cache;
  size_t each_size;

  if (asect->relocation)
    return TRUE;

  if (asect->flags & SEC_CONSTRUCTOR)
    return TRUE;

  if (asect == obj_datasec (abfd))
    {
      reloc_size = exec_hdr (abfd)->a_drsize;
      goto doit;
    }

  if (asect == obj_textsec (abfd))
    {
      reloc_size = exec_hdr (abfd)->a_trsize;
      goto doit;
    }

  bfd_set_error (bfd_error_invalid_operation);
  return FALSE;

doit:
  if (bfd_seek (abfd, asect->rel_filepos, SEEK_SET) != 0)
    return FALSE;
  each_size = obj_reloc_entry_size (abfd);

  count = reloc_size / each_size;


  reloc_cache = (arelent *) bfd_zmalloc (count * sizeof (arelent));
  if (!reloc_cache && count != 0)
    return FALSE;

  relocs = (PTR) bfd_alloc (abfd, reloc_size);
  if (!relocs && reloc_size != 0)
    {
      free (reloc_cache);
      return FALSE;
    }

  if (bfd_bread (relocs, reloc_size, abfd) != reloc_size)
    {
      bfd_release (abfd, relocs);
      free (reloc_cache);
      return FALSE;
    }

  if (each_size == RELOC_EXT_SIZE)
    {
      register struct reloc_ext_external *rptr = (struct reloc_ext_external *) relocs;
      unsigned int counter = 0;
      arelent *cache_ptr = reloc_cache;

      for (; counter < count; counter++, rptr++, cache_ptr++)
	{
	  NAME(lynx,swap_ext_reloc_in) (abfd, rptr, cache_ptr, symbols,
					(bfd_size_type) bfd_get_symcount (abfd));
	}
    }
  else
    {
      register struct reloc_std_external *rptr = (struct reloc_std_external *) relocs;
      unsigned int counter = 0;
      arelent *cache_ptr = reloc_cache;

      for (; counter < count; counter++, rptr++, cache_ptr++)
	{
	  NAME(lynx,swap_std_reloc_in) (abfd, rptr, cache_ptr, symbols,
					(bfd_size_type) bfd_get_symcount (abfd));
	}

    }

  bfd_release (abfd, relocs);
  asect->relocation = reloc_cache;
  asect->reloc_count = count;
  return TRUE;
}



/* Write out a relocation section into an object file.  */

bfd_boolean
NAME(lynx,squirt_out_relocs) (abfd, section)
     bfd *abfd;
     asection *section;
{
  arelent **generic;
  unsigned char *native, *natptr;
  size_t each_size;

  unsigned int count = section->reloc_count;
  bfd_size_type natsize;

  if (count == 0)
    return TRUE;

  each_size = obj_reloc_entry_size (abfd);
  natsize = count;
  natsize *= each_size;
  native = (unsigned char *) bfd_zalloc (abfd, natsize);
  if (!native)
    return FALSE;

  generic = section->orelocation;

  if (each_size == RELOC_EXT_SIZE)
    {
      for (natptr = native;
	   count != 0;
	   --count, natptr += each_size, ++generic)
	NAME(lynx,swap_ext_reloc_out) (abfd, *generic, (struct reloc_ext_external *) natptr);
    }
  else
    {
      for (natptr = native;
	   count != 0;
	   --count, natptr += each_size, ++generic)
	NAME(lynx,swap_std_reloc_out) (abfd, *generic, (struct reloc_std_external *) natptr);
    }

  if (bfd_bwrite ((PTR) native, natsize, abfd) != natsize)
    {
      bfd_release (abfd, native);
      return FALSE;
    }
  bfd_release (abfd, native);

  return TRUE;
}

/* This is stupid.  This function should be a boolean predicate */
long
NAME(lynx,canonicalize_reloc) (abfd, section, relptr, symbols)
     bfd *abfd;
     sec_ptr section;
     arelent **relptr;
     asymbol **symbols;
{
  arelent *tblptr = section->relocation;
  unsigned int count;

  if (!(tblptr || NAME(lynx,slurp_reloc_table) (abfd, section, symbols)))
    return -1;

  if (section->flags & SEC_CONSTRUCTOR)
    {
      arelent_chain *chain = section->constructor_chain;
      for (count = 0; count < section->reloc_count; count++)
	{
	  *relptr++ = &chain->relent;
	  chain = chain->next;
	}
    }
  else
    {
      tblptr = section->relocation;

      for (count = 0; count++ < section->reloc_count;)
	{
	  *relptr++ = tblptr++;
	}
    }
  *relptr = 0;

  return section->reloc_count;
}

#define MY_canonicalize_reloc NAME(lynx,canonicalize_reloc)

#include "aout-target.h"
