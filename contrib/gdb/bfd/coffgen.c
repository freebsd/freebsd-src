/* Support for the generic parts of COFF, for BFD.
   Copyright 1990, 91, 92, 93, 94, 95, 1996 Free Software Foundation, Inc.
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

/* Most of this hacked by  Steve Chamberlain, sac@cygnus.com.
   Split out of coffcode.h by Ian Taylor, ian@cygnus.com.  */

/* This file contains COFF code that is not dependent on any
   particular COFF target.  There is only one version of this file in
   libbfd.a, so no target specific code may be put in here.  Or, to
   put it another way,

   ********** DO NOT PUT TARGET SPECIFIC CODE IN THIS FILE **********

   If you need to add some target specific behaviour, add a new hook
   function to bfd_coff_backend_data.

   Some of these functions are also called by the ECOFF routines.
   Those functions may not use any COFF specific information, such as
   coff_data (abfd).  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "libcoff.h"

static void coff_fix_symbol_name
  PARAMS ((bfd *, asymbol *, combined_entry_type *, bfd_size_type *,
	   asection **, bfd_size_type *));
static boolean coff_write_symbol
  PARAMS ((bfd *, asymbol *, combined_entry_type *, unsigned int *,
	   bfd_size_type *, asection **, bfd_size_type *));
static boolean coff_write_alien_symbol
  PARAMS ((bfd *, asymbol *, unsigned int *, bfd_size_type *,
	   asection **, bfd_size_type *));
static boolean coff_write_native_symbol
  PARAMS ((bfd *, coff_symbol_type *, unsigned int *, bfd_size_type *,
	   asection **, bfd_size_type *));
static void coff_pointerize_aux
  PARAMS ((bfd *, combined_entry_type *, combined_entry_type *,
	   unsigned int, combined_entry_type *));

#define STRING_SIZE_SIZE (4)

/* Take a section header read from a coff file (in HOST byte order),
   and make a BFD "section" out of it.  This is used by ECOFF.  */
static boolean
make_a_section_from_file (abfd, hdr, target_index)
     bfd *abfd;
     struct internal_scnhdr *hdr;
     unsigned int target_index;
{
  asection *return_section;
  char *name;

  /* Assorted wastage to null-terminate the name, thanks AT&T! */
  name = bfd_alloc (abfd, sizeof (hdr->s_name) + 1);
  if (name == NULL)
    return false;
  strncpy (name, (char *) &hdr->s_name[0], sizeof (hdr->s_name));
  name[sizeof (hdr->s_name)] = 0;

  return_section = bfd_make_section_anyway (abfd, name);
  if (return_section == NULL)
    return false;

  return_section->vma = hdr->s_vaddr;
  return_section->lma = hdr->s_paddr;
  return_section->_raw_size = hdr->s_size;
  return_section->filepos = hdr->s_scnptr;
  return_section->rel_filepos = hdr->s_relptr;
  return_section->reloc_count = hdr->s_nreloc;

  bfd_coff_set_alignment_hook (abfd, return_section, hdr);

  return_section->line_filepos = hdr->s_lnnoptr;

  return_section->lineno_count = hdr->s_nlnno;
  return_section->userdata = NULL;
  return_section->next = (asection *) NULL;
  return_section->flags = bfd_coff_styp_to_sec_flags_hook (abfd, hdr, name);

  return_section->target_index = target_index;

  /* At least on i386-coff, the line number count for a shared library
     section must be ignored.  */
  if ((return_section->flags & SEC_COFF_SHARED_LIBRARY) != 0)
    return_section->lineno_count = 0;

  if (hdr->s_nreloc != 0)
    return_section->flags |= SEC_RELOC;
  /* FIXME: should this check 'hdr->s_size > 0' */
  if (hdr->s_scnptr != 0)
    return_section->flags |= SEC_HAS_CONTENTS;
  return true;
}

/* Read in a COFF object and make it into a BFD.  This is used by
   ECOFF as well.  */

static const bfd_target *
coff_real_object_p (abfd, nscns, internal_f, internal_a)
     bfd *abfd;
     unsigned nscns;
     struct internal_filehdr *internal_f;
     struct internal_aouthdr *internal_a;
{
  flagword oflags = abfd->flags;
  bfd_vma ostart = bfd_get_start_address (abfd);
  PTR tdata;
  size_t readsize;		/* length of file_info */
  unsigned int scnhsz;
  char *external_sections;

  if (!(internal_f->f_flags & F_RELFLG))
    abfd->flags |= HAS_RELOC;
  if ((internal_f->f_flags & F_EXEC))
    abfd->flags |= EXEC_P;
  if (!(internal_f->f_flags & F_LNNO))
    abfd->flags |= HAS_LINENO;
  if (!(internal_f->f_flags & F_LSYMS))
    abfd->flags |= HAS_LOCALS;

  /* FIXME: How can we set D_PAGED correctly?  */
  if ((internal_f->f_flags & F_EXEC) != 0)
    abfd->flags |= D_PAGED;

  bfd_get_symcount (abfd) = internal_f->f_nsyms;
  if (internal_f->f_nsyms)
    abfd->flags |= HAS_SYMS;

  if (internal_a != (struct internal_aouthdr *) NULL)
    bfd_get_start_address (abfd) = internal_a->entry;
  else
    bfd_get_start_address (abfd) = 0;

  /* Set up the tdata area.  ECOFF uses its own routine, and overrides
     abfd->flags.  */
  tdata = bfd_coff_mkobject_hook (abfd, (PTR) internal_f, (PTR) internal_a);
  if (tdata == NULL)
    return 0;

  scnhsz = bfd_coff_scnhsz (abfd);
  readsize = nscns * scnhsz;
  external_sections = (char *) bfd_alloc (abfd, readsize);
  if (!external_sections)
    goto fail;

  if (bfd_read ((PTR) external_sections, 1, readsize, abfd) != readsize)
    goto fail;

  /* Now copy data as required; construct all asections etc */
  if (nscns != 0)
    {
      unsigned int i;
      for (i = 0; i < nscns; i++)
	{
	  struct internal_scnhdr tmp;
	  bfd_coff_swap_scnhdr_in (abfd,
				   (PTR) (external_sections + i * scnhsz),
				   (PTR) & tmp);
	  make_a_section_from_file (abfd, &tmp, i + 1);
	}
    }

  /*  make_abs_section (abfd); */

  if (bfd_coff_set_arch_mach_hook (abfd, (PTR) internal_f) == false)
    goto fail;

  return abfd->xvec;

 fail:
  bfd_release (abfd, tdata);
  abfd->flags = oflags;
  bfd_get_start_address (abfd) = ostart;
  return (const bfd_target *) NULL;
}

/* Turn a COFF file into a BFD, but fail with bfd_error_wrong_format if it is
   not a COFF file.  This is also used by ECOFF.  */

const bfd_target *
coff_object_p (abfd)
     bfd *abfd;
{
  unsigned int filhsz;
  unsigned int aoutsz;
  int nscns;
  PTR filehdr;
  struct internal_filehdr internal_f;
  struct internal_aouthdr internal_a;

  /* figure out how much to read */
  filhsz = bfd_coff_filhsz (abfd);
  aoutsz = bfd_coff_aoutsz (abfd);

  filehdr = bfd_alloc (abfd, filhsz);
  if (filehdr == NULL)
    return 0;
  if (bfd_read (filehdr, 1, filhsz, abfd) != filhsz)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return 0;
    }
  bfd_coff_swap_filehdr_in (abfd, filehdr, &internal_f);
  bfd_release (abfd, filehdr);

  if (bfd_coff_bad_format_hook (abfd, &internal_f) == false)
    {
      bfd_set_error (bfd_error_wrong_format);
      return 0;
    }
  nscns = internal_f.f_nscns;

  if (internal_f.f_opthdr)
    {
      PTR opthdr;

      opthdr = bfd_alloc (abfd, aoutsz);
      if (opthdr == NULL)
	return 0;;
      if (bfd_read (opthdr, 1, aoutsz, abfd) != aoutsz)
	{
	  return 0;
	}
      bfd_coff_swap_aouthdr_in (abfd, opthdr, (PTR) & internal_a);
    }

  /* Seek past the opt hdr stuff */
  if (bfd_seek (abfd, (file_ptr) (internal_f.f_opthdr + filhsz), SEEK_SET)
      != 0)
    return NULL;

  return coff_real_object_p (abfd, nscns, &internal_f,
			     (internal_f.f_opthdr != 0
			      ? &internal_a
			      : (struct internal_aouthdr *) NULL));
}

/* Get the BFD section from a COFF symbol section number.  */

asection *
coff_section_from_bfd_index (abfd, index)
     bfd *abfd;
     int index;
{
  struct sec *answer = abfd->sections;

  if (index == N_ABS)
    return bfd_abs_section_ptr;
  if (index == N_UNDEF)
    return bfd_und_section_ptr;
  if (index == N_DEBUG)
    return bfd_abs_section_ptr;

  while (answer)
    {
      if (answer->target_index == index)
	return answer;
      answer = answer->next;
    }

  /* We should not reach this point, but the SCO 3.2v4 /lib/libc_s.a
     has a bad symbol table in biglitpow.o.  */
  return bfd_und_section_ptr;
}

/* Get the upper bound of a COFF symbol table.  */

long
coff_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  if (!bfd_coff_slurp_symbol_table (abfd))
    return -1;

  return (bfd_get_symcount (abfd) + 1) * (sizeof (coff_symbol_type *));
}


/* Canonicalize a COFF symbol table.  */

long
coff_get_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  unsigned int counter;
  coff_symbol_type *symbase;
  coff_symbol_type **location = (coff_symbol_type **) alocation;

  if (!bfd_coff_slurp_symbol_table (abfd))
    return -1;

  symbase = obj_symbols (abfd);
  counter = bfd_get_symcount (abfd);
  while (counter-- > 0)
    *location++ = symbase++;

  *location = NULL;

  return bfd_get_symcount (abfd);
}

/* Get the name of a symbol.  The caller must pass in a buffer of size
   >= SYMNMLEN + 1.  */

const char *
_bfd_coff_internal_syment_name (abfd, sym, buf)
     bfd *abfd;
     const struct internal_syment *sym;
     char *buf;
{
  /* FIXME: It's not clear this will work correctly if sizeof
     (_n_zeroes) != 4.  */
  if (sym->_n._n_n._n_zeroes != 0
      || sym->_n._n_n._n_offset == 0)
    {
      memcpy (buf, sym->_n._n_name, SYMNMLEN);
      buf[SYMNMLEN] = '\0';
      return buf;
    }
  else
    {
      const char *strings;

      BFD_ASSERT (sym->_n._n_n._n_offset >= STRING_SIZE_SIZE);
      strings = obj_coff_strings (abfd);
      if (strings == NULL)
	{
	  strings = _bfd_coff_read_string_table (abfd);
	  if (strings == NULL)
	    return NULL;
	}
      return strings + sym->_n._n_n._n_offset;
    }
}

/* Read in and swap the relocs.  This returns a buffer holding the
   relocs for section SEC in file ABFD.  If CACHE is true and
   INTERNAL_RELOCS is NULL, the relocs read in will be saved in case
   the function is called again.  If EXTERNAL_RELOCS is not NULL, it
   is a buffer large enough to hold the unswapped relocs.  If
   INTERNAL_RELOCS is not NULL, it is a buffer large enough to hold
   the swapped relocs.  If REQUIRE_INTERNAL is true, then the return
   value must be INTERNAL_RELOCS.  The function returns NULL on error.  */

struct internal_reloc *
_bfd_coff_read_internal_relocs (abfd, sec, cache, external_relocs,
				require_internal, internal_relocs)
     bfd *abfd;
     asection *sec;
     boolean cache;
     bfd_byte *external_relocs;
     boolean require_internal;
     struct internal_reloc *internal_relocs;
{
  bfd_size_type relsz;
  bfd_byte *free_external = NULL;
  struct internal_reloc *free_internal = NULL;
  bfd_byte *erel;
  bfd_byte *erel_end;
  struct internal_reloc *irel;

  if (coff_section_data (abfd, sec) != NULL
      && coff_section_data (abfd, sec)->relocs != NULL)
    {
      if (! require_internal)
	return coff_section_data (abfd, sec)->relocs;
      memcpy (internal_relocs, coff_section_data (abfd, sec)->relocs,
	      sec->reloc_count * sizeof (struct internal_reloc));
      return internal_relocs;
    }

  relsz = bfd_coff_relsz (abfd);

  if (external_relocs == NULL)
    {
      free_external = (bfd_byte *) bfd_malloc (sec->reloc_count * relsz);
      if (free_external == NULL && sec->reloc_count > 0)
	goto error_return;
      external_relocs = free_external;
    }

  if (bfd_seek (abfd, sec->rel_filepos, SEEK_SET) != 0
      || (bfd_read (external_relocs, relsz, sec->reloc_count, abfd)
	  != relsz * sec->reloc_count))
    goto error_return;

  if (internal_relocs == NULL)
    {
      free_internal = ((struct internal_reloc *)
		       bfd_malloc (sec->reloc_count
				   * sizeof (struct internal_reloc)));
      if (free_internal == NULL && sec->reloc_count > 0)
	goto error_return;
      internal_relocs = free_internal;
    }

  /* Swap in the relocs.  */
  erel = external_relocs;
  erel_end = erel + relsz * sec->reloc_count;
  irel = internal_relocs;
  for (; erel < erel_end; erel += relsz, irel++)
    bfd_coff_swap_reloc_in (abfd, (PTR) erel, (PTR) irel);

  if (free_external != NULL)
    {
      free (free_external);
      free_external = NULL;
    }

  if (cache && free_internal != NULL)
    {
      if (coff_section_data (abfd, sec) == NULL)
	{
	  sec->used_by_bfd =
	    (PTR) bfd_zalloc (abfd,
			      sizeof (struct coff_section_tdata));
	  if (sec->used_by_bfd == NULL)
	    goto error_return;
	  coff_section_data (abfd, sec)->contents = NULL;
	}
      coff_section_data (abfd, sec)->relocs = free_internal;
    }

  return internal_relocs;

 error_return:
  if (free_external != NULL)
    free (free_external);
  if (free_internal != NULL)
    free (free_internal);
  return NULL;
}

/* Set lineno_count for the output sections of a COFF file.  */

int
coff_count_linenumbers (abfd)
     bfd *abfd;
{
  unsigned int limit = bfd_get_symcount (abfd);
  unsigned int i;
  int total = 0;
  asymbol **p;
  asection *s;

  if (limit == 0)
    {
      /* This may be from the backend linker, in which case the
         lineno_count in the sections is correct.  */
      for (s = abfd->sections; s != NULL; s = s->next)
	total += s->lineno_count;
      return total;
    }

  for (s = abfd->sections; s != NULL; s = s->next)
    BFD_ASSERT (s->lineno_count == 0);

  for (p = abfd->outsymbols, i = 0; i < limit; i++, p++)
    {
      asymbol *q_maybe = *p;

      if (bfd_asymbol_flavour (q_maybe) == bfd_target_coff_flavour)
	{
	  coff_symbol_type *q = coffsymbol (q_maybe);

	  /* The AIX 4.1 compiler can sometimes generate line numbers
             attached to debugging symbols.  We try to simply ignore
             those here.  */
	  if (q->lineno != NULL
	      && q->symbol.section->owner != NULL)
	    {
	      /* This symbol has line numbers.  Increment the owning
	         section's linenumber count.  */
	      alent *l = q->lineno;

	      ++q->symbol.section->output_section->lineno_count;
	      ++total;
	      ++l;
	      while (l->line_number != 0)
		{
		  ++total;
		  ++q->symbol.section->output_section->lineno_count;
		  ++l;
		}
	    }
	}
    }

  return total;
}

/* Takes a bfd and a symbol, returns a pointer to the coff specific
   area of the symbol if there is one.  */

/*ARGSUSED*/
coff_symbol_type *
coff_symbol_from (ignore_abfd, symbol)
     bfd *ignore_abfd;
     asymbol *symbol;
{
  if (bfd_asymbol_flavour (symbol) != bfd_target_coff_flavour)
    return (coff_symbol_type *) NULL;

  if (bfd_asymbol_bfd (symbol)->tdata.coff_obj_data == (coff_data_type *) NULL)
    return (coff_symbol_type *) NULL;

  return (coff_symbol_type *) symbol;
}

static void
fixup_symbol_value (coff_symbol_ptr, syment)
     coff_symbol_type *coff_symbol_ptr;
     struct internal_syment *syment;
{

  /* Normalize the symbol flags */
  if (bfd_is_com_section (coff_symbol_ptr->symbol.section))
    {
      /* a common symbol is undefined with a value */
      syment->n_scnum = N_UNDEF;
      syment->n_value = coff_symbol_ptr->symbol.value;
    }
  else if (coff_symbol_ptr->symbol.flags & BSF_DEBUGGING)
    {
      syment->n_value = coff_symbol_ptr->symbol.value;
    }
  else if (bfd_is_und_section (coff_symbol_ptr->symbol.section))
    {
      syment->n_scnum = N_UNDEF;
      syment->n_value = 0;
    }
  else
    {
      if (coff_symbol_ptr->symbol.section)
	{
	  syment->n_scnum =
	    coff_symbol_ptr->symbol.section->output_section->target_index;

	  syment->n_value =
	    coff_symbol_ptr->symbol.value +
	    coff_symbol_ptr->symbol.section->output_offset +
	    coff_symbol_ptr->symbol.section->output_section->vma;
	}
      else
	{
	  BFD_ASSERT (0);
	  /* This can happen, but I don't know why yet (steve@cygnus.com) */
	  syment->n_scnum = N_ABS;
	  syment->n_value = coff_symbol_ptr->symbol.value;
	}
    }
}

/* Run through all the symbols in the symbol table and work out what
   their indexes into the symbol table will be when output.

   Coff requires that each C_FILE symbol points to the next one in the
   chain, and that the last one points to the first external symbol. We
   do that here too.  */

boolean
coff_renumber_symbols (bfd_ptr, first_undef)
     bfd *bfd_ptr;
     int *first_undef;
{
  unsigned int symbol_count = bfd_get_symcount (bfd_ptr);
  asymbol **symbol_ptr_ptr = bfd_ptr->outsymbols;
  unsigned int native_index = 0;
  struct internal_syment *last_file = (struct internal_syment *) NULL;
  unsigned int symbol_index;

  /* COFF demands that undefined symbols come after all other symbols.
     Since we don't need to impose this extra knowledge on all our
     client programs, deal with that here.  Sort the symbol table;
     just move the undefined symbols to the end, leaving the rest
     alone.  The O'Reilly book says that defined global symbols come
     at the end before the undefined symbols, so we do that here as
     well.  */
  /* @@ Do we have some condition we could test for, so we don't always
     have to do this?  I don't think relocatability is quite right, but
     I'm not certain.  [raeburn:19920508.1711EST]  */
  {
    asymbol **newsyms;
    unsigned int i;

    newsyms = (asymbol **) bfd_alloc_by_size_t (bfd_ptr,
						sizeof (asymbol *)
						* (symbol_count + 1));
    if (!newsyms)
      return false;
    bfd_ptr->outsymbols = newsyms;
    for (i = 0; i < symbol_count; i++)
      if ((symbol_ptr_ptr[i]->flags & BSF_NOT_AT_END) != 0
	  || (!bfd_is_und_section (symbol_ptr_ptr[i]->section)
	      && !bfd_is_com_section (symbol_ptr_ptr[i]->section)
	      && ((symbol_ptr_ptr[i]->flags & (BSF_GLOBAL | BSF_FUNCTION))
		  != BSF_GLOBAL)))
	*newsyms++ = symbol_ptr_ptr[i];

    for (i = 0; i < symbol_count; i++)
      if (!bfd_is_und_section (symbol_ptr_ptr[i]->section)
	  && (bfd_is_com_section (symbol_ptr_ptr[i]->section)
	      || ((symbol_ptr_ptr[i]->flags & (BSF_GLOBAL
					       | BSF_NOT_AT_END
					       | BSF_FUNCTION))
		  == BSF_GLOBAL)))
	*newsyms++ = symbol_ptr_ptr[i];

    *first_undef = newsyms - bfd_ptr->outsymbols;

    for (i = 0; i < symbol_count; i++)
      if ((symbol_ptr_ptr[i]->flags & BSF_NOT_AT_END) == 0
	  && bfd_is_und_section (symbol_ptr_ptr[i]->section))
	*newsyms++ = symbol_ptr_ptr[i];
    *newsyms = (asymbol *) NULL;
    symbol_ptr_ptr = bfd_ptr->outsymbols;
  }

  for (symbol_index = 0; symbol_index < symbol_count; symbol_index++)
    {
      coff_symbol_type *coff_symbol_ptr = coff_symbol_from (bfd_ptr, symbol_ptr_ptr[symbol_index]);
      symbol_ptr_ptr[symbol_index]->udata.i = symbol_index; 
      if (coff_symbol_ptr && coff_symbol_ptr->native)
	{
	  combined_entry_type *s = coff_symbol_ptr->native;
	  int i;

	  if (s->u.syment.n_sclass == C_FILE)
	    {
	      if (last_file != (struct internal_syment *) NULL)
		last_file->n_value = native_index;
	      last_file = &(s->u.syment);
	    }
	  else
	    {

	      /* Modify the symbol values according to their section and
	         type */

	      fixup_symbol_value (coff_symbol_ptr, &(s->u.syment));
	    }
	  for (i = 0; i < s->u.syment.n_numaux + 1; i++)
	    s[i].offset = native_index++;
	}
      else
	{
	  native_index++;
	}
    }
  obj_conv_table_size (bfd_ptr) = native_index;

  return true;
}

/* Run thorough the symbol table again, and fix it so that all
   pointers to entries are changed to the entries' index in the output
   symbol table.  */

void
coff_mangle_symbols (bfd_ptr)
     bfd *bfd_ptr;
{
  unsigned int symbol_count = bfd_get_symcount (bfd_ptr);
  asymbol **symbol_ptr_ptr = bfd_ptr->outsymbols;
  unsigned int symbol_index;

  for (symbol_index = 0; symbol_index < symbol_count; symbol_index++)
    {
      coff_symbol_type *coff_symbol_ptr =
      coff_symbol_from (bfd_ptr, symbol_ptr_ptr[symbol_index]);

      if (coff_symbol_ptr && coff_symbol_ptr->native)
	{
	  int i;
	  combined_entry_type *s = coff_symbol_ptr->native;

	  if (s->fix_value)
	    {
	      /* FIXME: We should use a union here.  */
	      s->u.syment.n_value =
		((combined_entry_type *) s->u.syment.n_value)->offset;
	      s->fix_value = 0;
	    }
	  if (s->fix_line)
	    {
	      /* The value is the offset into the line number entries
                 for the symbol's section.  On output, the symbol's
                 section should be N_DEBUG.  */
	      s->u.syment.n_value =
		(coff_symbol_ptr->symbol.section->output_section->line_filepos
		 + s->u.syment.n_value * bfd_coff_linesz (bfd_ptr));
	      coff_symbol_ptr->symbol.section =
		coff_section_from_bfd_index (bfd_ptr, N_DEBUG);
	      BFD_ASSERT (coff_symbol_ptr->symbol.flags & BSF_DEBUGGING);
	    }
	  for (i = 0; i < s->u.syment.n_numaux; i++)
	    {
	      combined_entry_type *a = s + i + 1;
	      if (a->fix_tag)
		{
		  a->u.auxent.x_sym.x_tagndx.l =
		    a->u.auxent.x_sym.x_tagndx.p->offset;
		  a->fix_tag = 0;
		}
	      if (a->fix_end)
		{
		  a->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.l =
		    a->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.p->offset;
		  a->fix_end = 0;
		}
	      if (a->fix_scnlen)
		{
		  a->u.auxent.x_csect.x_scnlen.l =
		    a->u.auxent.x_csect.x_scnlen.p->offset;
		  a->fix_scnlen = 0;
		}
	    }
	}
    }
}

static void
coff_fix_symbol_name (abfd, symbol, native, string_size_p,
		      debug_string_section_p, debug_string_size_p)
     bfd *abfd;
     asymbol *symbol;
     combined_entry_type *native;
     bfd_size_type *string_size_p;
     asection **debug_string_section_p;
     bfd_size_type *debug_string_size_p;
{
  unsigned int name_length;
  union internal_auxent *auxent;
  char *name = (char *) (symbol->name);

  if (name == (char *) NULL)
    {
      /* coff symbols always have names, so we'll make one up */
      symbol->name = "strange";
      name = (char *) symbol->name;
    }
  name_length = strlen (name);

  if (native->u.syment.n_sclass == C_FILE
      && native->u.syment.n_numaux > 0)
    {
      strncpy (native->u.syment._n._n_name, ".file", SYMNMLEN);
      auxent = &(native + 1)->u.auxent;

      if (bfd_coff_long_filenames (abfd))
	{
	  if (name_length <= FILNMLEN)
	    {
	      strncpy (auxent->x_file.x_fname, name, FILNMLEN);
	    }
	  else
	    {
	      auxent->x_file.x_n.x_offset = *string_size_p + STRING_SIZE_SIZE;
	      auxent->x_file.x_n.x_zeroes = 0;
	      *string_size_p += name_length + 1;
	    }
	}
      else
	{
	  strncpy (auxent->x_file.x_fname, name, FILNMLEN);
	  if (name_length > FILNMLEN)
	    {
	      name[FILNMLEN] = '\0';
	    }
	}
    }
  else
    {
      if (name_length <= SYMNMLEN)
	{
	  /* This name will fit into the symbol neatly */
	  strncpy (native->u.syment._n._n_name, symbol->name, SYMNMLEN);
	}
      else if (!bfd_coff_symname_in_debug (abfd, &native->u.syment))
	{
	  native->u.syment._n._n_n._n_offset = (*string_size_p
						+ STRING_SIZE_SIZE);
	  native->u.syment._n._n_n._n_zeroes = 0;
	  *string_size_p += name_length + 1;
	}
      else
	{
	  long filepos;
	  bfd_byte buf[2];

	  /* This name should be written into the .debug section.  For
	     some reason each name is preceded by a two byte length
	     and also followed by a null byte.  FIXME: We assume that
	     the .debug section has already been created, and that it
	     is large enough.  */
	  if (*debug_string_section_p == (asection *) NULL)
	    *debug_string_section_p = bfd_get_section_by_name (abfd, ".debug");
	  filepos = bfd_tell (abfd);
	  bfd_put_16 (abfd, name_length + 1, buf);
	  if (!bfd_set_section_contents (abfd,
					 *debug_string_section_p,
					 (PTR) buf,
					 (file_ptr) *debug_string_size_p,
					 (bfd_size_type) 2)
	      || !bfd_set_section_contents (abfd,
					    *debug_string_section_p,
					    (PTR) symbol->name,
					    ((file_ptr) *debug_string_size_p
					     + 2),
					    (bfd_size_type) name_length + 1))
	    abort ();
	  if (bfd_seek (abfd, filepos, SEEK_SET) != 0)
	    abort ();
	  native->u.syment._n._n_n._n_offset = *debug_string_size_p + 2;
	  native->u.syment._n._n_n._n_zeroes = 0;
	  *debug_string_size_p += name_length + 3;
	}
    }
}

/* We need to keep track of the symbol index so that when we write out
   the relocs we can get the index for a symbol.  This method is a
   hack.  FIXME.  */

#define set_index(symbol, idx)	((symbol)->udata.i = (idx))

/* Write a symbol out to a COFF file.  */

static boolean
coff_write_symbol (abfd, symbol, native, written, string_size_p,
		   debug_string_section_p, debug_string_size_p)
     bfd *abfd;
     asymbol *symbol;
     combined_entry_type *native;
     unsigned int *written;
     bfd_size_type *string_size_p;
     asection **debug_string_section_p;
     bfd_size_type *debug_string_size_p;
{
  unsigned int numaux = native->u.syment.n_numaux;
  int type = native->u.syment.n_type;
  int class = native->u.syment.n_sclass;
  PTR buf;
  bfd_size_type symesz;

  if (native->u.syment.n_sclass == C_FILE)
    symbol->flags |= BSF_DEBUGGING;

  if (symbol->flags & BSF_DEBUGGING
      && bfd_is_abs_section (symbol->section))
    {
      native->u.syment.n_scnum = N_DEBUG;
    }
  else if (bfd_is_abs_section (symbol->section))
    {
      native->u.syment.n_scnum = N_ABS;
    }
  else if (bfd_is_und_section (symbol->section))
    {
      native->u.syment.n_scnum = N_UNDEF;
    }
  else
    {
      native->u.syment.n_scnum =
	symbol->section->output_section->target_index;
    }

  coff_fix_symbol_name (abfd, symbol, native, string_size_p,
			debug_string_section_p, debug_string_size_p);

  symesz = bfd_coff_symesz (abfd);
  buf = bfd_alloc (abfd, symesz);
  if (!buf)
    return false;
  bfd_coff_swap_sym_out (abfd, &native->u.syment, buf);
  if (bfd_write (buf, 1, symesz, abfd) != symesz)
    return false;
  bfd_release (abfd, buf);

  if (native->u.syment.n_numaux > 0)
    {
      bfd_size_type auxesz;
      unsigned int j;

      auxesz = bfd_coff_auxesz (abfd);
      buf = bfd_alloc (abfd, auxesz);
      if (!buf)
	return false;
      for (j = 0; j < native->u.syment.n_numaux; j++)
	{
	  bfd_coff_swap_aux_out (abfd,
				 &((native + j + 1)->u.auxent),
				 type,
				 class,
				 j,
				 native->u.syment.n_numaux,
				 buf);
	  if (bfd_write (buf, 1, auxesz, abfd) != auxesz)
	    return false;
	}
      bfd_release (abfd, buf);
    }

  /* Store the index for use when we write out the relocs.  */
  set_index (symbol, *written);

  *written += numaux + 1;
  return true;
}

/* Write out a symbol to a COFF file that does not come from a COFF
   file originally.  This symbol may have been created by the linker,
   or we may be linking a non COFF file to a COFF file.  */

static boolean
coff_write_alien_symbol (abfd, symbol, written, string_size_p,
			 debug_string_section_p, debug_string_size_p)
     bfd *abfd;
     asymbol *symbol;
     unsigned int *written;
     bfd_size_type *string_size_p;
     asection **debug_string_section_p;
     bfd_size_type *debug_string_size_p;
{
  combined_entry_type *native;
  combined_entry_type dummy;

  native = &dummy;
  native->u.syment.n_type = T_NULL;
  native->u.syment.n_flags = 0;
  if (bfd_is_und_section (symbol->section))
    {
      native->u.syment.n_scnum = N_UNDEF;
      native->u.syment.n_value = symbol->value;
    }
  else if (bfd_is_com_section (symbol->section))
    {
      native->u.syment.n_scnum = N_UNDEF;
      native->u.syment.n_value = symbol->value;
    }
  else if (symbol->flags & BSF_DEBUGGING)
    {
      /* There isn't much point to writing out a debugging symbol
         unless we are prepared to convert it into COFF debugging
         format.  So, we just ignore them.  We must clobber the symbol
         name to keep it from being put in the string table.  */
      symbol->name = "";
      return true;
    }
  else
    {
      native->u.syment.n_scnum =
	symbol->section->output_section->target_index;
      native->u.syment.n_value = (symbol->value
				  + symbol->section->output_section->vma
				  + symbol->section->output_offset);

      /* Copy the any flags from the the file header into the symbol.
         FIXME: Why?  */
      {
	coff_symbol_type *c = coff_symbol_from (abfd, symbol);
	if (c != (coff_symbol_type *) NULL)
	  native->u.syment.n_flags = bfd_asymbol_bfd (&c->symbol)->flags;
      }
    }

  native->u.syment.n_type = 0;
  if (symbol->flags & BSF_LOCAL)
    native->u.syment.n_sclass = C_STAT;
  else
    native->u.syment.n_sclass = C_EXT;
  native->u.syment.n_numaux = 0;

  return coff_write_symbol (abfd, symbol, native, written, string_size_p,
			    debug_string_section_p, debug_string_size_p);
}

/* Write a native symbol to a COFF file.  */

static boolean
coff_write_native_symbol (abfd, symbol, written, string_size_p,
			  debug_string_section_p, debug_string_size_p)
     bfd *abfd;
     coff_symbol_type *symbol;
     unsigned int *written;
     bfd_size_type *string_size_p;
     asection **debug_string_section_p;
     bfd_size_type *debug_string_size_p;
{
  combined_entry_type *native = symbol->native;
  alent *lineno = symbol->lineno;

  /* If this symbol has an associated line number, we must store the
     symbol index in the line number field.  We also tag the auxent to
     point to the right place in the lineno table.  */
  if (lineno && !symbol->done_lineno && symbol->symbol.section->owner != NULL)
    {
      unsigned int count = 0;
      lineno[count].u.offset = *written;
      if (native->u.syment.n_numaux)
	{
	  union internal_auxent *a = &((native + 1)->u.auxent);

	  a->x_sym.x_fcnary.x_fcn.x_lnnoptr =
	    symbol->symbol.section->output_section->moving_line_filepos;
	}

      /* Count and relocate all other linenumbers.  */
      count++;
      while (lineno[count].line_number != 0)
	{
#if 0
	  /* 13 april 92. sac 
	     I've been told this, but still need proof:
	     > The second bug is also in `bfd/coffcode.h'.  This bug
	     > causes the linker to screw up the pc-relocations for
	     > all the line numbers in COFF code.  This bug isn't only
	     > specific to A29K implementations, but affects all
	     > systems using COFF format binaries.  Note that in COFF
	     > object files, the line number core offsets output by
	     > the assembler are relative to the start of each
	     > procedure, not to the start of the .text section.  This
	     > patch relocates the line numbers relative to the
	     > `native->u.syment.n_value' instead of the section
	     > virtual address.
	     > modular!olson@cs.arizona.edu (Jon Olson)
	   */
	  lineno[count].u.offset += native->u.syment.n_value;
#else
	  lineno[count].u.offset +=
	    (symbol->symbol.section->output_section->vma
	     + symbol->symbol.section->output_offset);
#endif
	  count++;
	}
      symbol->done_lineno = true;

      symbol->symbol.section->output_section->moving_line_filepos +=
	count * bfd_coff_linesz (abfd);
    }

  return coff_write_symbol (abfd, &(symbol->symbol), native, written,
			    string_size_p, debug_string_section_p,
			    debug_string_size_p);
}

/* Write out the COFF symbols.  */

boolean
coff_write_symbols (abfd)
     bfd *abfd;
{
  bfd_size_type string_size;
  asection *debug_string_section;
  bfd_size_type debug_string_size;
  unsigned int i;
  unsigned int limit = bfd_get_symcount (abfd);
  unsigned int written = 0;
  asymbol **p;

  string_size = 0;
  debug_string_section = NULL;
  debug_string_size = 0;

  /* Seek to the right place */
  if (bfd_seek (abfd, obj_sym_filepos (abfd), SEEK_SET) != 0)
    return false;

  /* Output all the symbols we have */

  written = 0;
  for (p = abfd->outsymbols, i = 0; i < limit; i++, p++)
    {
      asymbol *symbol = *p;
      coff_symbol_type *c_symbol = coff_symbol_from (abfd, symbol);

      if (c_symbol == (coff_symbol_type *) NULL
	  || c_symbol->native == (combined_entry_type *) NULL)
	{
	  if (!coff_write_alien_symbol (abfd, symbol, &written, &string_size,
					&debug_string_section,
					&debug_string_size))
	    return false;
	}
      else
	{
	  if (!coff_write_native_symbol (abfd, c_symbol, &written,
					 &string_size, &debug_string_section,
					 &debug_string_size))
	    return false;
	}
    }

  obj_raw_syment_count (abfd) = written;

  /* Now write out strings */

  if (string_size != 0)
    {
      unsigned int size = string_size + STRING_SIZE_SIZE;
      bfd_byte buffer[STRING_SIZE_SIZE];

#if STRING_SIZE_SIZE == 4
      bfd_h_put_32 (abfd, size, buffer);
#else
 #error Change bfd_h_put_32
#endif
      if (bfd_write ((PTR) buffer, 1, sizeof (buffer), abfd) != sizeof (buffer))
	return false;
      for (p = abfd->outsymbols, i = 0;
	   i < limit;
	   i++, p++)
	{
	  asymbol *q = *p;
	  size_t name_length = strlen (q->name);
	  coff_symbol_type *c_symbol = coff_symbol_from (abfd, q);
	  size_t maxlen;

	  /* Figure out whether the symbol name should go in the string
	     table.  Symbol names that are short enough are stored
	     directly in the syment structure.  File names permit a
	     different, longer, length in the syment structure.  On
	     XCOFF, some symbol names are stored in the .debug section
	     rather than in the string table.  */

	  if (c_symbol == NULL
	      || c_symbol->native == NULL)
	    {
	      /* This is not a COFF symbol, so it certainly is not a
	         file name, nor does it go in the .debug section.  */
	      maxlen = SYMNMLEN;
	    }
	  else if (bfd_coff_symname_in_debug (abfd,
					      &c_symbol->native->u.syment))
	    {
	      /* This symbol name is in the XCOFF .debug section.
	         Don't write it into the string table.  */
	      maxlen = name_length;
	    }
	  else if (c_symbol->native->u.syment.n_sclass == C_FILE
		   && c_symbol->native->u.syment.n_numaux > 0)
	    maxlen = FILNMLEN;
	  else
	    maxlen = SYMNMLEN;

	  if (name_length > maxlen)
	    {
	      if (bfd_write ((PTR) (q->name), 1, name_length + 1, abfd)
		  != name_length + 1)
		return false;
	    }
	}
    }
  else
    {
      /* We would normally not write anything here, but we'll write
         out 4 so that any stupid coff reader which tries to read the
         string table even when there isn't one won't croak.  */
      unsigned int size = STRING_SIZE_SIZE;
      bfd_byte buffer[STRING_SIZE_SIZE];

#if STRING_SIZE_SIZE == 4
      bfd_h_put_32 (abfd, size, buffer);
#else
 #error Change bfd_h_put_32
#endif
      if (bfd_write ((PTR) buffer, 1, STRING_SIZE_SIZE, abfd)
	  != STRING_SIZE_SIZE)
	return false;
    }

  /* Make sure the .debug section was created to be the correct size.
     We should create it ourselves on the fly, but we don't because
     BFD won't let us write to any section until we know how large all
     the sections are.  We could still do it by making another pass
     over the symbols.  FIXME.  */
  BFD_ASSERT (debug_string_size == 0
	      || (debug_string_section != (asection *) NULL
		  && (BFD_ALIGN (debug_string_size,
				 1 << debug_string_section->alignment_power)
		      == bfd_section_size (abfd, debug_string_section))));

  return true;
}

boolean
coff_write_linenumbers (abfd)
     bfd *abfd;
{
  asection *s;
  bfd_size_type linesz;
  PTR buff;

  linesz = bfd_coff_linesz (abfd);
  buff = bfd_alloc (abfd, linesz);
  if (!buff)
    return false;
  for (s = abfd->sections; s != (asection *) NULL; s = s->next)
    {
      if (s->lineno_count)
	{
	  asymbol **q = abfd->outsymbols;
	  if (bfd_seek (abfd, s->line_filepos, SEEK_SET) != 0)
	    return false;
	  /* Find all the linenumbers in this section */
	  while (*q)
	    {
	      asymbol *p = *q;
	      if (p->section->output_section == s)
		{
		  alent *l =
		  BFD_SEND (bfd_asymbol_bfd (p), _get_lineno,
			    (bfd_asymbol_bfd (p), p));
		  if (l)
		    {
		      /* Found a linenumber entry, output */
		      struct internal_lineno out;
		      memset ((PTR) & out, 0, sizeof (out));
		      out.l_lnno = 0;
		      out.l_addr.l_symndx = l->u.offset;
		      bfd_coff_swap_lineno_out (abfd, &out, buff);
		      if (bfd_write (buff, 1, linesz, abfd) != linesz)
			return false;
		      l++;
		      while (l->line_number)
			{
			  out.l_lnno = l->line_number;
			  out.l_addr.l_symndx = l->u.offset;
			  bfd_coff_swap_lineno_out (abfd, &out, buff);
			  if (bfd_write (buff, 1, linesz, abfd) != linesz)
			    return false;
			  l++;
			}
		    }
		}
	      q++;
	    }
	}
    }
  bfd_release (abfd, buff);
  return true;
}

/*ARGSUSED */
alent *
coff_get_lineno (ignore_abfd, symbol)
     bfd *ignore_abfd;
     asymbol *symbol;
{
  return coffsymbol (symbol)->lineno;
}

#if 0

/* This is only called from coff_add_missing_symbols, which has been
   disabled.  */

asymbol *
coff_section_symbol (abfd, name)
     bfd *abfd;
     char *name;
{
  asection *sec = bfd_make_section_old_way (abfd, name);
  asymbol *sym;
  combined_entry_type *csym;

  sym = sec->symbol;
  csym = coff_symbol_from (abfd, sym)->native;
  /* Make sure back-end COFF stuff is there.  */
  if (csym == 0)
    {
      struct foo
	{
	  coff_symbol_type sym;
	  /* @@FIXME This shouldn't use a fixed size!!  */
	  combined_entry_type e[10];
	};
      struct foo *f;
      f = (struct foo *) bfd_alloc_by_size_t (abfd, sizeof (*f));
      if (!f)
	{
	  bfd_set_error (bfd_error_no_error);
	  return NULL;
	}
      memset ((char *) f, 0, sizeof (*f));
      coff_symbol_from (abfd, sym)->native = csym = f->e;
    }
  csym[0].u.syment.n_sclass = C_STAT;
  csym[0].u.syment.n_numaux = 1;
/*  SF_SET_STATICS (sym);       @@ ??? */
  csym[1].u.auxent.x_scn.x_scnlen = sec->_raw_size;
  csym[1].u.auxent.x_scn.x_nreloc = sec->reloc_count;
  csym[1].u.auxent.x_scn.x_nlinno = sec->lineno_count;

  if (sec->output_section == NULL)
    {
      sec->output_section = sec;
      sec->output_offset = 0;
    }

  return sym;
}

#endif /* 0 */

/* This function transforms the offsets into the symbol table into
   pointers to syments.  */

static void
coff_pointerize_aux (abfd, table_base, symbol, indaux, auxent)
     bfd *abfd;
     combined_entry_type *table_base;
     combined_entry_type *symbol;
     unsigned int indaux;
     combined_entry_type *auxent;
{
  int type = symbol->u.syment.n_type;
  int class = symbol->u.syment.n_sclass;

  if (coff_backend_info (abfd)->_bfd_coff_pointerize_aux_hook)
    {
      if ((*coff_backend_info (abfd)->_bfd_coff_pointerize_aux_hook)
	  (abfd, table_base, symbol, indaux, auxent))
	return;
    }

  /* Don't bother if this is a file or a section */
  if (class == C_STAT && type == T_NULL)
    return;
  if (class == C_FILE)
    return;

  /* Otherwise patch up */
#define N_TMASK coff_data (abfd)->local_n_tmask
#define N_BTSHFT coff_data (abfd)->local_n_btshft
  if ((ISFCN (type) || ISTAG (class) || class == C_BLOCK)
      && auxent->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.l > 0)
    {
      auxent->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.p =
	table_base + auxent->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.l;
      auxent->fix_end = 1;
    }
  /* A negative tagndx is meaningless, but the SCO 3.2v4 cc can
     generate one, so we must be careful to ignore it.  */
  if (auxent->u.auxent.x_sym.x_tagndx.l > 0)
    {
      auxent->u.auxent.x_sym.x_tagndx.p =
	table_base + auxent->u.auxent.x_sym.x_tagndx.l;
      auxent->fix_tag = 1;
    }
}

/* Allocate space for the ".debug" section, and read it.
   We did not read the debug section until now, because
   we didn't want to go to the trouble until someone needed it. */

static char *
build_debug_section (abfd)
     bfd *abfd;
{
  char *debug_section;
  long position;

  asection *sect = bfd_get_section_by_name (abfd, ".debug");

  if (!sect)
    {
      bfd_set_error (bfd_error_no_debug_section);
      return NULL;
    }

  debug_section = (PTR) bfd_alloc (abfd,
				   bfd_get_section_size_before_reloc (sect));
  if (debug_section == NULL)
    return NULL;

  /* Seek to the beginning of the `.debug' section and read it. 
     Save the current position first; it is needed by our caller.
     Then read debug section and reset the file pointer.  */

  position = bfd_tell (abfd);
  if (bfd_seek (abfd, sect->filepos, SEEK_SET) != 0
      || (bfd_read (debug_section,
		    bfd_get_section_size_before_reloc (sect), 1, abfd)
	  != bfd_get_section_size_before_reloc (sect))
      || bfd_seek (abfd, position, SEEK_SET) != 0)
    return NULL;
  return debug_section;
}


/* Return a pointer to a malloc'd copy of 'name'.  'name' may not be
   \0-terminated, but will not exceed 'maxlen' characters.  The copy *will*
   be \0-terminated.  */
static char *
copy_name (abfd, name, maxlen)
     bfd *abfd;
     char *name;
     int maxlen;
{
  int len;
  char *newname;

  for (len = 0; len < maxlen; ++len)
    {
      if (name[len] == '\0')
	{
	  break;
	}
    }

  if ((newname = (PTR) bfd_alloc (abfd, len + 1)) == NULL)
    return (NULL);
  strncpy (newname, name, len);
  newname[len] = '\0';
  return newname;
}

/* Read in the external symbols.  */

boolean
_bfd_coff_get_external_symbols (abfd)
     bfd *abfd;
{
  bfd_size_type symesz;
  size_t size;
  PTR syms;

  if (obj_coff_external_syms (abfd) != NULL)
    return true;

  symesz = bfd_coff_symesz (abfd);

  size = obj_raw_syment_count (abfd) * symesz;

  syms = (PTR) bfd_malloc (size);
  if (syms == NULL && size != 0)
    return false;

  if (bfd_seek (abfd, obj_sym_filepos (abfd), SEEK_SET) != 0
      || bfd_read (syms, size, 1, abfd) != size)
    {
      if (syms != NULL)
	free (syms);
      return false;
    }

  obj_coff_external_syms (abfd) = syms;

  return true;
}

/* Read in the external strings.  The strings are not loaded until
   they are needed.  This is because we have no simple way of
   detecting a missing string table in an archive.  */

const char *
_bfd_coff_read_string_table (abfd)
     bfd *abfd;
{
  char extstrsize[STRING_SIZE_SIZE];
  size_t strsize;
  char *strings;

  if (obj_coff_strings (abfd) != NULL)
    return obj_coff_strings (abfd);

  if (bfd_seek (abfd,
		(obj_sym_filepos (abfd)
		 + obj_raw_syment_count (abfd) * bfd_coff_symesz (abfd)),
		SEEK_SET) != 0)
    return NULL;
    
  if (bfd_read (extstrsize, sizeof extstrsize, 1, abfd) != sizeof extstrsize)
    {
      if (bfd_get_error () != bfd_error_file_truncated)
	return NULL;

      /* There is no string table.  */
      strsize = STRING_SIZE_SIZE;
    }
  else
    {
#if STRING_SIZE_SIZE == 4
      strsize = bfd_h_get_32 (abfd, (bfd_byte *) extstrsize);
#else
 #error Change bfd_h_get_32
#endif
    }

  if (strsize < STRING_SIZE_SIZE)
    {
      (*_bfd_error_handler)
	("%s: bad string table size %lu", bfd_get_filename (abfd),
	 (unsigned long) strsize);
      bfd_set_error (bfd_error_bad_value);
      return NULL;
    }

  strings = (char *) bfd_malloc (strsize);
  if (strings == NULL)
    return NULL;

  if (bfd_read (strings + STRING_SIZE_SIZE,
		strsize - STRING_SIZE_SIZE, 1, abfd)
      != strsize - STRING_SIZE_SIZE)
    {
      free (strings);
      return NULL;
    }

  obj_coff_strings (abfd) = strings;

  return strings;
}

/* Free up the external symbols and strings read from a COFF file.  */

boolean
_bfd_coff_free_symbols (abfd)
     bfd *abfd;
{
  if (obj_coff_external_syms (abfd) != NULL
      && ! obj_coff_keep_syms (abfd))
    {
      free (obj_coff_external_syms (abfd));
      obj_coff_external_syms (abfd) = NULL;
    }
  if (obj_coff_strings (abfd) != NULL
      && ! obj_coff_keep_strings (abfd))
    {
      free (obj_coff_strings (abfd));
      obj_coff_strings (abfd) = NULL;
    }
  return true;
}

/* Read a symbol table into freshly bfd_allocated memory, swap it, and
   knit the symbol names into a normalized form.  By normalized here I
   mean that all symbols have an n_offset pointer that points to a null-
   terminated string.  */

combined_entry_type *
coff_get_normalized_symtab (abfd)
     bfd *abfd;
{
  combined_entry_type *internal;
  combined_entry_type *internal_ptr;
  combined_entry_type *symbol_ptr;
  combined_entry_type *internal_end;
  bfd_size_type symesz;
  char *raw_src;
  char *raw_end;
  const char *string_table = NULL;
  char *debug_section = NULL;
  unsigned long size;

  if (obj_raw_syments (abfd) != NULL)
    return obj_raw_syments (abfd);

  size = obj_raw_syment_count (abfd) * sizeof (combined_entry_type);
  internal = (combined_entry_type *) bfd_zalloc (abfd, size);
  if (internal == NULL && size != 0)
    return NULL;
  internal_end = internal + obj_raw_syment_count (abfd);

  if (! _bfd_coff_get_external_symbols (abfd))
    return NULL;

  raw_src = (char *) obj_coff_external_syms (abfd);

  /* mark the end of the symbols */
  symesz = bfd_coff_symesz (abfd);
  raw_end = (char *) raw_src + obj_raw_syment_count (abfd) * symesz;

  /* FIXME SOMEDAY.  A string table size of zero is very weird, but
     probably possible.  If one shows up, it will probably kill us.  */

  /* Swap all the raw entries */
  for (internal_ptr = internal;
       raw_src < raw_end;
       raw_src += symesz, internal_ptr++)
    {

      unsigned int i;
      bfd_coff_swap_sym_in (abfd, (PTR) raw_src,
			    (PTR) & internal_ptr->u.syment);
      symbol_ptr = internal_ptr;

      for (i = 0;
	   i < symbol_ptr->u.syment.n_numaux;
	   i++)
	{
	  internal_ptr++;
	  raw_src += symesz;
	  bfd_coff_swap_aux_in (abfd, (PTR) raw_src,
				symbol_ptr->u.syment.n_type,
				symbol_ptr->u.syment.n_sclass,
				i, symbol_ptr->u.syment.n_numaux,
				&(internal_ptr->u.auxent));
	  coff_pointerize_aux (abfd, internal, symbol_ptr, i,
			       internal_ptr);
	}
    }

  /* Free the raw symbols, but not the strings (if we have them).  */
  obj_coff_keep_strings (abfd) = true;
  if (! _bfd_coff_free_symbols (abfd))
    return NULL;

  for (internal_ptr = internal; internal_ptr < internal_end;
       internal_ptr++)
    {
      if (internal_ptr->u.syment.n_sclass == C_FILE
	  && internal_ptr->u.syment.n_numaux > 0)
	{
	  /* make a file symbol point to the name in the auxent, since
	     the text ".file" is redundant */
	  if ((internal_ptr + 1)->u.auxent.x_file.x_n.x_zeroes == 0)
	    {
	      /* the filename is a long one, point into the string table */
	      if (string_table == NULL)
		{
		  string_table = _bfd_coff_read_string_table (abfd);
		  if (string_table == NULL)
		    return NULL;
		}

	      internal_ptr->u.syment._n._n_n._n_offset =
		((long)
		 (string_table
		  + (internal_ptr + 1)->u.auxent.x_file.x_n.x_offset));
	    }
	  else
	    {
	      /* ordinary short filename, put into memory anyway */
	      internal_ptr->u.syment._n._n_n._n_offset = (long)
		copy_name (abfd, (internal_ptr + 1)->u.auxent.x_file.x_fname,
			   FILNMLEN);
	    }
	}
      else
	{
	  if (internal_ptr->u.syment._n._n_n._n_zeroes != 0)
	    {
	      /* This is a "short" name.  Make it long.  */
	      unsigned long i = 0;
	      char *newstring = NULL;

	      /* find the length of this string without walking into memory
	         that isn't ours.  */
	      for (i = 0; i < 8; ++i)
		{
		  if (internal_ptr->u.syment._n._n_name[i] == '\0')
		    {
		      break;
		    }		/* if end of string */
		}		/* possible lengths of this string. */

	      if ((newstring = (PTR) bfd_alloc (abfd, ++i)) == NULL)
		return (NULL);
	      memset (newstring, 0, i);
	      strncpy (newstring, internal_ptr->u.syment._n._n_name, i - 1);
	      internal_ptr->u.syment._n._n_n._n_offset = (long int) newstring;
	      internal_ptr->u.syment._n._n_n._n_zeroes = 0;
	    }
	  else if (internal_ptr->u.syment._n._n_n._n_offset == 0)
	    internal_ptr->u.syment._n._n_n._n_offset = (long int) "";
	  else if (!bfd_coff_symname_in_debug (abfd, &internal_ptr->u.syment))
	    {
	      /* Long name already.  Point symbol at the string in the
                 table.  */
	      if (string_table == NULL)
		{
		  string_table = _bfd_coff_read_string_table (abfd);
		  if (string_table == NULL)
		    return NULL;
		}
	      internal_ptr->u.syment._n._n_n._n_offset =
		((long int)
		 (string_table
		  + internal_ptr->u.syment._n._n_n._n_offset));
	    }
	  else
	    {
	      /* Long name in debug section.  Very similar.  */
	      if (debug_section == NULL)
		debug_section = build_debug_section (abfd);
	      internal_ptr->u.syment._n._n_n._n_offset = (long int)
		(debug_section + internal_ptr->u.syment._n._n_n._n_offset);
	    }
	}
      internal_ptr += internal_ptr->u.syment.n_numaux;
    }

  obj_raw_syments (abfd) = internal;
  BFD_ASSERT (obj_raw_syment_count (abfd)
	      == (unsigned int) (internal_ptr - internal));

  return (internal);
}				/* coff_get_normalized_symtab() */

long
coff_get_reloc_upper_bound (abfd, asect)
     bfd *abfd;
     sec_ptr asect;
{
  if (bfd_get_format (abfd) != bfd_object)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }
  return (asect->reloc_count + 1) * sizeof (arelent *);
}

asymbol *
coff_make_empty_symbol (abfd)
     bfd *abfd;
{
  coff_symbol_type *new = (coff_symbol_type *) bfd_alloc (abfd, sizeof (coff_symbol_type));
  if (new == NULL)
    return (NULL);
  memset (new, 0, sizeof *new);
  new->symbol.section = 0;
  new->native = 0;
  new->lineno = (alent *) NULL;
  new->done_lineno = false;
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}

/* Make a debugging symbol.  */

asymbol *
coff_bfd_make_debug_symbol (abfd, ptr, sz)
     bfd *abfd;
     PTR ptr;
     unsigned long sz;
{
  coff_symbol_type *new = (coff_symbol_type *) bfd_alloc (abfd, sizeof (coff_symbol_type));
  if (new == NULL)
    return (NULL);
  /* @@ This shouldn't be using a constant multiplier.  */
  new->native = (combined_entry_type *) bfd_zalloc (abfd, sizeof (combined_entry_type) * 10);
  if (!new->native)
    return (NULL);
  new->symbol.section = bfd_abs_section_ptr;
  new->symbol.flags = BSF_DEBUGGING;
  new->lineno = (alent *) NULL;
  new->done_lineno = false;
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}

/*ARGSUSED */
void
coff_get_symbol_info (abfd, symbol, ret)
     bfd *abfd;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
  if (coffsymbol (symbol)->native != NULL
      && coffsymbol (symbol)->native->fix_value)
    {
      combined_entry_type *psym;

      psym = ((combined_entry_type *)
	      coffsymbol (symbol)->native->u.syment.n_value);
      ret->value = (bfd_vma) (psym - obj_raw_syments (abfd));
    }
}

/* Print out information about COFF symbol.  */

void
coff_print_symbol (abfd, filep, symbol, how)
     bfd *abfd;
     PTR filep;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  FILE *file = (FILE *) filep;

  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;

    case bfd_print_symbol_more:
      fprintf (file, "coff %s %s",
	       coffsymbol (symbol)->native ? "n" : "g",
	       coffsymbol (symbol)->lineno ? "l" : " ");
      break;

    case bfd_print_symbol_all:
      if (coffsymbol (symbol)->native)
	{
	  unsigned long val;
	  unsigned int aux;
	  combined_entry_type *combined = coffsymbol (symbol)->native;
	  combined_entry_type *root = obj_raw_syments (abfd);
	  struct lineno_cache_entry *l = coffsymbol (symbol)->lineno;

	  fprintf (file, "[%3ld]", (long) (combined - root));

	  if (! combined->fix_value)
	    val = (unsigned long) combined->u.syment.n_value;
	  else
	    val = ((unsigned long)
		   ((combined_entry_type *) combined->u.syment.n_value
		    - root));

	  fprintf (file,
		   "(sec %2d)(fl 0x%02x)(ty %3x)(scl %3d) (nx %d) 0x%08lx %s",
		   combined->u.syment.n_scnum,
		   combined->u.syment.n_flags,
		   combined->u.syment.n_type,
		   combined->u.syment.n_sclass,
		   combined->u.syment.n_numaux,
		   val,
		   symbol->name);

	  for (aux = 0; aux < combined->u.syment.n_numaux; aux++)
	    {
	      combined_entry_type *auxp = combined + aux + 1;
	      long tagndx;

	      if (auxp->fix_tag)
		tagndx = auxp->u.auxent.x_sym.x_tagndx.p - root;
	      else
		tagndx = auxp->u.auxent.x_sym.x_tagndx.l;

	      fprintf (file, "\n");

	      if (bfd_coff_print_aux (abfd, file, root, combined, auxp, aux))
		continue;

	      switch (combined->u.syment.n_sclass)
		{
		case C_FILE:
		  fprintf (file, "File ");
		  break;

		case C_STAT:
		  if (combined->u.syment.n_type == T_NULL)
		    /* probably a section symbol? */
		    {
		      fprintf (file, "AUX scnlen 0x%lx nreloc %d nlnno %d",
			       (long) auxp->u.auxent.x_scn.x_scnlen,
			       auxp->u.auxent.x_scn.x_nreloc,
			       auxp->u.auxent.x_scn.x_nlinno);
		      break;
		    }
		  /* else fall through */

		default:
		  fprintf (file, "AUX lnno %d size 0x%x tagndx %ld",
			   auxp->u.auxent.x_sym.x_misc.x_lnsz.x_lnno,
			   auxp->u.auxent.x_sym.x_misc.x_lnsz.x_size,
			   tagndx);
		  if (auxp->fix_end)
		    fprintf (file, " endndx %ld",
			     ((long)
			      (auxp->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.p
			       - root)));
		  break;
		}
	    }

	  if (l)
	    {
	      fprintf (file, "\n%s :", l->u.sym->name);
	      l++;
	      while (l->line_number)
		{
		  fprintf (file, "\n%4d : 0x%lx",
			   l->line_number,
			   ((unsigned long)
			    (l->u.offset + symbol->section->vma)));
		  l++;
		}
	    }
	}
      else
	{
	  bfd_print_symbol_vandf ((PTR) file, symbol);
	  fprintf (file, " %-5s %s %s %s",
		   symbol->section->name,
		   coffsymbol (symbol)->native ? "n" : "g",
		   coffsymbol (symbol)->lineno ? "l" : " ",
		   symbol->name);
	}
    }
}

/* Provided a BFD, a section and an offset into the section, calculate
   and return the name of the source file and the line nearest to the
   wanted location.  */

/*ARGSUSED*/
boolean
coff_find_nearest_line (abfd, section, symbols, offset, filename_ptr,
			functionname_ptr, line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     CONST char **filename_ptr;
     CONST char **functionname_ptr;
     unsigned int *line_ptr;
{
  boolean found;
  unsigned int i;
  unsigned int line_base;
  coff_data_type *cof = coff_data (abfd);
  /* Run through the raw syments if available */
  combined_entry_type *p;
  combined_entry_type *pend;
  alent *l;
  struct coff_section_tdata *sec_data;

  /* Before looking through the symbol table, try to use a .stab
     section to find the information.  */
  if (! _bfd_stab_section_find_nearest_line (abfd, symbols, section, offset,
					     &found, filename_ptr,
					     functionname_ptr, line_ptr,
					     &coff_data (abfd)->line_info))
    return false;
  if (found)
    return true;

  *filename_ptr = 0;
  *functionname_ptr = 0;
  *line_ptr = 0;

  /* Don't try and find line numbers in a non coff file */
  if (abfd->xvec->flavour != bfd_target_coff_flavour)
    return false;

  if (cof == NULL)
    return false;

  /* Find the first C_FILE symbol.  */
  p = cof->raw_syments;
  pend = p + cof->raw_syment_count;
  while (p < pend)
    {
      if (p->u.syment.n_sclass == C_FILE)
	break;
      p += 1 + p->u.syment.n_numaux;
    }

  if (p < pend)
    {
      bfd_vma maxdiff;

      /* Look through the C_FILE symbols to find the best one.  */
      *filename_ptr = (char *) p->u.syment._n._n_n._n_offset;
      maxdiff = (bfd_vma) 0 - (bfd_vma) 1;
      while (1)
	{
	  combined_entry_type *p2;

	  for (p2 = p + 1 + p->u.syment.n_numaux;
	       p2 < pend;
	       p2 += 1 + p2->u.syment.n_numaux)
	    {
	      if (p2->u.syment.n_scnum > 0
		  && (section
		      == coff_section_from_bfd_index (abfd,
						      p2->u.syment.n_scnum)))
		break;
	      if (p2->u.syment.n_sclass == C_FILE)
		{
		  p2 = pend;
		  break;
		}
	    }

	  if (p2 < pend
	      && offset >= (bfd_vma) p2->u.syment.n_value
	      && offset - (bfd_vma) p2->u.syment.n_value < maxdiff)
	    {
	      *filename_ptr = (char *) p->u.syment._n._n_n._n_offset;
	      maxdiff = offset - p2->u.syment.n_value;
	    }

	  /* Avoid endless loops on erroneous files by ensuring that
	     we always move forward in the file.  */
	  if (p - cof->raw_syments >= p->u.syment.n_value)
	    break;

	  p = cof->raw_syments + p->u.syment.n_value;
	  if (p > pend || p->u.syment.n_sclass != C_FILE)
	    break;
	}
    }

  /* Now wander though the raw linenumbers of the section */
  /* If we have been called on this section before, and the offset we
     want is further down then we can prime the lookup loop.  */
  sec_data = coff_section_data (abfd, section);
  if (sec_data != NULL
      && sec_data->i > 0
      && offset >= sec_data->offset)
    {
      i = sec_data->i;
      *functionname_ptr = sec_data->function;
      line_base = sec_data->line_base;
    }
  else
    {
      i = 0;
      line_base = 0;
    }

  if (section->lineno != NULL)
    {
      l = &section->lineno[i];

      for (; i < section->lineno_count; i++)
	{
	  if (l->line_number == 0)
	    {
	      /* Get the symbol this line number points at */
	      coff_symbol_type *coff = (coff_symbol_type *) (l->u.sym);
	      if (coff->symbol.value > offset)
		break;
	      *functionname_ptr = coff->symbol.name;
	      if (coff->native)
		{
		  combined_entry_type *s = coff->native;
		  s = s + 1 + s->u.syment.n_numaux;

		  /* In XCOFF a debugging symbol can follow the
		     function symbol.  */
		  if (s->u.syment.n_scnum == N_DEBUG)
		    s = s + 1 + s->u.syment.n_numaux;

		  /* S should now point to the .bf of the function.  */
		  if (s->u.syment.n_numaux)
		    {
		      /* The linenumber is stored in the auxent.  */
		      union internal_auxent *a = &((s + 1)->u.auxent);
		      line_base = a->x_sym.x_misc.x_lnsz.x_lnno;
		      *line_ptr = line_base;
		    }
		}
	    }
	  else
	    {
	      if (l->u.offset + bfd_get_section_vma (abfd, section) > offset)
		break;
	      *line_ptr = l->line_number + line_base - 1;
	    }
	  l++;
	}
    }

  /* Cache the results for the next call.  */
  if (sec_data == NULL && section->owner == abfd)
    {
      section->used_by_bfd =
	((PTR) bfd_zalloc (abfd,
			   sizeof (struct coff_section_tdata)));
      sec_data = (struct coff_section_tdata *) section->used_by_bfd;
    }
  if (sec_data != NULL)
    {
      sec_data->offset = offset;
      sec_data->i = i;
      sec_data->function = *functionname_ptr;
      sec_data->line_base = line_base;
    }

  return true;
}

int
coff_sizeof_headers (abfd, reloc)
     bfd *abfd;
     boolean reloc;
{
  size_t size;

  if (reloc == false)
    {
      size = bfd_coff_filhsz (abfd) + bfd_coff_aoutsz (abfd);
    }
  else
    {
      size = bfd_coff_filhsz (abfd);
    }

  size += abfd->section_count * bfd_coff_scnhsz (abfd);
  return size;
}
