/* Support for the generic parts of COFF, for BFD.
   Copyright 1990, 1991, 1992, 1993 Free Software Foundation, Inc.
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

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
#include "seclet.h"
#include "libcoff.h"

static asection bfd_debug_section = { "*DEBUG*" };

/* Take a section header read from a coff file (in HOST byte order),
   and make a BFD "section" out of it.  This is used by ECOFF.  */
static          boolean
DEFUN(make_a_section_from_file,(abfd, hdr, target_index),
      bfd            *abfd AND
      struct internal_scnhdr  *hdr AND
      unsigned int target_index)
{
  asection       *return_section;
  char *name;
    
  /* Assorted wastage to null-terminate the name, thanks AT&T! */
  name = bfd_alloc(abfd, sizeof (hdr->s_name)+1);
  if (name == NULL) {
      bfd_error = no_memory;
      return false;
    }
  strncpy(name, (char *) &hdr->s_name[0], sizeof (hdr->s_name));
  name[sizeof (hdr->s_name)] = 0;

  return_section = bfd_make_section(abfd, name);
  if (return_section == NULL)
    return_section = bfd_coff_make_section_hook (abfd, name);

  /* Handle several sections of the same name.  For example, if an executable
     has two .bss sections, GDB better be able to find both of them
     (PR 3562).  */
  if (return_section == NULL)
    return_section = bfd_make_section_anyway (abfd, name);

  if (return_section == NULL)
    return false;

  /* s_paddr is presumed to be = to s_vaddr */

  return_section->vma = hdr->s_vaddr;
  return_section->_raw_size = hdr->s_size;
  return_section->filepos = hdr->s_scnptr;
  return_section->rel_filepos =  hdr->s_relptr;
  return_section->reloc_count = hdr->s_nreloc;

  bfd_coff_set_alignment_hook (abfd, return_section, hdr);

  return_section->line_filepos =  hdr->s_lnnoptr;

  return_section->lineno_count = hdr->s_nlnno;
  return_section->userdata = NULL;
  return_section->next = (asection *) NULL;
  return_section->flags = bfd_coff_styp_to_sec_flags_hook (abfd, hdr);

  return_section->target_index = target_index;

  /* At least on i386-coff, the line number count for a shared library
     section must be ignored.  */
  if ((return_section->flags & SEC_SHARED_LIBRARY) != 0)
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

static
bfd_target     *
DEFUN(coff_real_object_p,(abfd, nscns, internal_f, internal_a),
    bfd            *abfd AND
    unsigned        nscns AND
  struct internal_filehdr *internal_f AND
  struct internal_aouthdr *internal_a)
{
  PTR tdata;
  size_t          readsize;	/* length of file_info */
  unsigned int scnhsz;
  char *external_sections;

  /* Build a play area */
  tdata = bfd_coff_mkobject_hook (abfd, (PTR) internal_f, (PTR) internal_a);
  if (tdata == NULL)
    return 0;

  scnhsz = bfd_coff_scnhsz (abfd);
  readsize = nscns * scnhsz;
  external_sections = (char *)bfd_alloc(abfd, readsize);

  if (bfd_read((PTR)external_sections, 1, readsize, abfd) != readsize) {
    goto fail;
  }

  /* Now copy data as required; construct all asections etc */
  if (nscns != 0) {
    unsigned int    i;
    for (i = 0; i < nscns; i++) {
      struct internal_scnhdr tmp;
      bfd_coff_swap_scnhdr_in(abfd, (PTR) (external_sections + i * scnhsz),
			      (PTR) &tmp);
      make_a_section_from_file(abfd,&tmp, i+1);
    }
  }

/*  make_abs_section(abfd);*/
  
  if (bfd_coff_set_arch_mach_hook (abfd, (PTR) internal_f) == false)
    goto fail;

  if (!(internal_f->f_flags & F_RELFLG))
    abfd->flags |= HAS_RELOC;
  if ((internal_f->f_flags & F_EXEC))
    abfd->flags |= EXEC_P;
  if (!(internal_f->f_flags & F_LNNO))
    abfd->flags |= HAS_LINENO;
  if (!(internal_f->f_flags & F_LSYMS))
    abfd->flags |= HAS_LOCALS;


  bfd_get_symcount(abfd) = internal_f->f_nsyms;
  if (internal_f->f_nsyms)
    abfd->flags |= HAS_SYMS;

  if (internal_a != (struct internal_aouthdr *) NULL)
    bfd_get_start_address (abfd) = internal_a->entry;
  else
    bfd_get_start_address (abfd) = 0;

  return abfd->xvec;
 fail:
  bfd_release(abfd, tdata);
  return (bfd_target *)NULL;
}

/* Turn a COFF file into a BFD, but fail with wrong_format if it is
   not a COFF file.  This is also used by ECOFF.  */

bfd_target *
DEFUN(coff_object_p,(abfd),
      bfd            *abfd)
{
  unsigned int filhsz;
  unsigned int aoutsz;
  int   nscns;
  PTR filehdr;
  struct internal_filehdr internal_f;
  struct internal_aouthdr internal_a;

  bfd_error = system_call_error;

  /* figure out how much to read */
  filhsz = bfd_coff_filhsz (abfd);
  aoutsz = bfd_coff_aoutsz (abfd);

  filehdr = bfd_alloc (abfd, filhsz);
  if (filehdr == NULL)
    return 0;
  if (bfd_read(filehdr, 1, filhsz, abfd) != filhsz)
    return 0;
  bfd_coff_swap_filehdr_in(abfd, filehdr, &internal_f);
  bfd_release (abfd, filehdr);

  if (bfd_coff_bad_format_hook (abfd, &internal_f) == false) {
    bfd_error = wrong_format;
    return 0;
  }
  nscns =internal_f.f_nscns;

  if (internal_f.f_opthdr) {
    PTR opthdr;

    opthdr = bfd_alloc (abfd, aoutsz);
    if (opthdr == NULL)
      return 0;;
    if (bfd_read(opthdr, 1,aoutsz, abfd) != aoutsz) {
      return 0;
    }
    bfd_coff_swap_aouthdr_in(abfd, opthdr, (PTR)&internal_a);
  }

  /* Seek past the opt hdr stuff */
  bfd_seek(abfd, (file_ptr) (internal_f.f_opthdr + filhsz), SEEK_SET);

  return coff_real_object_p(abfd, nscns, &internal_f,
			    (internal_f.f_opthdr != 0
			     ? &internal_a
			     : (struct internal_aouthdr *) NULL));
}

/* Get the BFD section from a COFF symbol section number.  */

struct sec *
DEFUN(coff_section_from_bfd_index,(abfd, index),
      bfd            *abfd AND
      int             index)
{
  struct sec *answer = abfd->sections;

  if (index == N_ABS) 
  {
    return &bfd_abs_section;
  }
  if (index == N_UNDEF)
  {
    return &bfd_und_section;
  }
  if(index == N_DEBUG)
  {
    return &bfd_debug_section;
    
  }
  
  while (answer) {
      if (answer->target_index == index)
       return answer;
      answer = answer->next;
    }
  BFD_ASSERT(0);
  return &bfd_und_section;	/* For gcc -W and lint.  Never executed. */
}

/* Get the upper bound of a COFF symbol table.  */

unsigned int
coff_get_symtab_upper_bound(abfd)
bfd            *abfd;
{
  if (!bfd_coff_slurp_symbol_table(abfd))
    return 0;

  return (bfd_get_symcount(abfd) + 1) * (sizeof(coff_symbol_type *));
}


/* Canonicalize a COFF symbol table.  */

unsigned int
DEFUN(coff_get_symtab, (abfd, alocation),
      bfd            *abfd AND
      asymbol       **alocation)
{
    unsigned int    counter = 0;
    coff_symbol_type *symbase;
    coff_symbol_type **location = (coff_symbol_type **) (alocation);
    if (!bfd_coff_slurp_symbol_table(abfd))
     return 0;

    symbase = obj_symbols(abfd);
    while (counter <  bfd_get_symcount(abfd))
    {
	/* This nasty code looks at the symbol to decide whether or
	   not it is descibes a constructor/destructor entry point. It
	   is structured this way to (hopefully) speed non matches */
#if 0	
	if (0 && symbase->symbol.name[9] == '$') 
	{
	    bfd_constructor_entry(abfd, 
				 (asymbol **)location,
				  symbase->symbol.name[10] == 'I' ?
				  "CTOR" : "DTOR");
	}
#endif
	*(location++) = symbase++;
	counter++;
    }
    *location++ = 0;
    return bfd_get_symcount(abfd);
}

/* Set lineno_count for the output sections of a COFF file.  */

int
DEFUN(coff_count_linenumbers,(abfd),
      bfd            *abfd)
{
  unsigned int    limit = bfd_get_symcount(abfd);
  unsigned int    i;
  int total = 0;
  asymbol       **p;
 {
   asection       *s = abfd->sections->output_section;
   while (s) {
     BFD_ASSERT(s->lineno_count == 0);
     s = s->next;
   }
 }


  for (p = abfd->outsymbols, i = 0; i < limit; i++, p++) {
    asymbol        *q_maybe = *p;
    if (bfd_asymbol_flavour(q_maybe) == bfd_target_coff_flavour) {
      coff_symbol_type *q = coffsymbol(q_maybe);
      if (q->lineno) {
	/*
	  This symbol has a linenumber, increment the owning
	  section's linenumber count
	  */
	alent          *l = q->lineno;
	q->symbol.section->output_section->lineno_count++;
	total ++;
	l++;
	while (l->line_number) {
	  total ++;
	  q->symbol.section->output_section->lineno_count++;
	  l++;
	}
      }
    }
  }
  return total;
}

/* Takes a bfd and a symbol, returns a pointer to the coff specific
   area of the symbol if there is one.  */

coff_symbol_type *
DEFUN(coff_symbol_from,(ignore_abfd, symbol),
      bfd            *ignore_abfd AND
      asymbol        *symbol)
{
  if (bfd_asymbol_flavour(symbol) != bfd_target_coff_flavour)
    return (coff_symbol_type *)NULL;

  if (bfd_asymbol_bfd(symbol)->tdata.coff_obj_data == (coff_data_type*)NULL)
    return (coff_symbol_type *)NULL;

  return  (coff_symbol_type *) symbol;
}

static void
DEFUN(fixup_symbol_value,(coff_symbol_ptr, syment),
coff_symbol_type *coff_symbol_ptr AND
struct internal_syment *syment)
{

  /* Normalize the symbol flags */
  if (bfd_is_com_section (coff_symbol_ptr->symbol.section)) {
    /* a common symbol is undefined with a value */
    syment->n_scnum = N_UNDEF;
    syment->n_value = coff_symbol_ptr->symbol.value;
  }
  else if (coff_symbol_ptr->symbol.flags & BSF_DEBUGGING) {
    syment->n_value = coff_symbol_ptr->symbol.value;
  }
  else if (coff_symbol_ptr->symbol.section == & bfd_und_section) {
    syment->n_scnum = N_UNDEF;
    syment->n_value = 0;
  }
  else {
    if (coff_symbol_ptr->symbol.section) {
      syment->n_scnum	 =
       coff_symbol_ptr->symbol.section->output_section->target_index;

      syment->n_value =
       coff_symbol_ptr->symbol.value +
	coff_symbol_ptr->symbol.section->output_offset +
	 coff_symbol_ptr->symbol.section->output_section->vma;
    }
    else {
	BFD_ASSERT(0);
      /* This can happen, but I don't know why yet (steve@cygnus.com) */
      syment->n_scnum = N_ABS;
      syment->n_value = coff_symbol_ptr->symbol.value;
    }
  }
}

/* run through all the symbols in the symbol table and work out what
   their indexes into the symbol table will be when output

 Coff requires that each C_FILE symbol points to the next one in the
 chain, and that the last one points to the first external symbol. We
 do that here too.

*/
void
DEFUN(coff_renumber_symbols,(bfd_ptr),
      bfd *bfd_ptr)
{
  unsigned int symbol_count = bfd_get_symcount(bfd_ptr);
  asymbol **symbol_ptr_ptr = bfd_ptr->outsymbols;
  unsigned int native_index = 0;
  struct internal_syment *last_file = (struct internal_syment *)NULL;
  unsigned int symbol_index;

  /* COFF demands that undefined symbols come after all other symbols.
     Since we don't need to impose this extra knowledge on all our client
     programs, deal with that here.  Sort the symbol table; just move the
     undefined symbols to the end, leaving the rest alone.  */
  /* @@ Do we have some condition we could test for, so we don't always
     have to do this?  I don't think relocatability is quite right, but
     I'm not certain.  [raeburn:19920508.1711EST]  */
  {
    asymbol **newsyms;
    int i;

    newsyms = (asymbol **) bfd_alloc_by_size_t (bfd_ptr,
						sizeof (asymbol *)
						* (symbol_count + 1));
    bfd_ptr->outsymbols = newsyms;
    for (i = 0; i < symbol_count; i++)
      if (symbol_ptr_ptr[i]->section != &bfd_und_section)
	*newsyms++ = symbol_ptr_ptr[i];
    for (i = 0; i < symbol_count; i++)
      if (symbol_ptr_ptr[i]->section == &bfd_und_section)
	*newsyms++ = symbol_ptr_ptr[i];
    *newsyms = (asymbol *) NULL;
    symbol_ptr_ptr = bfd_ptr->outsymbols;
  }

  for (symbol_index = 0; symbol_index < symbol_count; symbol_index++)
      {
	coff_symbol_type *coff_symbol_ptr = coff_symbol_from(bfd_ptr, symbol_ptr_ptr[symbol_index]);
	if (coff_symbol_ptr && coff_symbol_ptr->native) {
	  combined_entry_type *s = coff_symbol_ptr->native;
	  int i;

	  if (s->u.syment.n_sclass == C_FILE)
	      {
		if (last_file != (struct internal_syment *)NULL) {
		  last_file->n_value = native_index;
		}
		last_file = &(s->u.syment);
	      }
	  else {

	    /* Modify the symbol values according to their section and
	       type */

	    fixup_symbol_value(coff_symbol_ptr, &(s->u.syment));
	  }
	  for (i = 0; i < s->u.syment.n_numaux + 1; i++) {
	    s[i].offset = native_index ++;
	  }
	}
	else {
	  native_index++;
	}
      }
  obj_conv_table_size (bfd_ptr) = native_index;
}

/*
 Run thorough the symbol table again, and fix it so that all pointers to
 entries are changed to the entries' index in the output symbol table.

*/
void
DEFUN(coff_mangle_symbols,(bfd_ptr),
      bfd *bfd_ptr)
{
  unsigned int symbol_count = bfd_get_symcount(bfd_ptr);
  asymbol **symbol_ptr_ptr = bfd_ptr->outsymbols;
  unsigned int symbol_index;

  for (symbol_index = 0; symbol_index < symbol_count; symbol_index++)
      {
	coff_symbol_type *coff_symbol_ptr =
	  coff_symbol_from(bfd_ptr, symbol_ptr_ptr[symbol_index]);

	if (coff_symbol_ptr && coff_symbol_ptr->native) {
	  int i;
	  combined_entry_type *s = coff_symbol_ptr->native;

	  for (i = 0; i < s->u.syment.n_numaux ; i++) {
	    combined_entry_type *a = s + i + 1;
	    if (a->fix_tag) {
	      a->u.auxent.x_sym.x_tagndx.l =
		a->u.auxent.x_sym.x_tagndx.p->offset;
	      a->fix_tag = 0;
	    }
	    if (a->fix_end) {
	      a->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.l =
		a->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.p->offset;
	      a->fix_end = 0;
	      
	    }

	  }
	}
      }
}

static int string_size;

static void
DEFUN(coff_fix_symbol_name,(abfd, symbol, native),
  bfd *abfd AND
  asymbol *symbol AND
  combined_entry_type *native)
{
  unsigned int    name_length;
  union internal_auxent *auxent;
  char *  name = ( char *)(symbol->name);

  if (name == (char *) NULL) {
    /* coff symbols always have names, so we'll make one up */
    symbol->name = "strange";
    name = (char *)symbol->name;
  }
  name_length = strlen(name);

  if (native->u.syment.n_sclass == C_FILE) {
    strncpy(native->u.syment._n._n_name, ".file", SYMNMLEN);
    auxent = &(native+1)->u.auxent;

    if (bfd_coff_long_filenames (abfd)) {
      if (name_length <= FILNMLEN) {
	strncpy(auxent->x_file.x_fname, name, FILNMLEN);
      }
      else {
	auxent->x_file.x_n.x_offset = string_size + 4;
	auxent->x_file.x_n.x_zeroes = 0;
	string_size += name_length + 1;
      }
    }
    else {
      strncpy(auxent->x_file.x_fname, name, FILNMLEN);
      if (name_length > FILNMLEN) {
	name[FILNMLEN] = '\0';
      }
    }
  }
  else
      {				/* NOT A C_FILE SYMBOL */
	if (name_length <= SYMNMLEN) {
	  /* This name will fit into the symbol neatly */
	  strncpy(native->u.syment._n._n_name, symbol->name, SYMNMLEN);
	}
	else {
	  native->u.syment._n._n_n._n_offset =  string_size + 4;
	  native->u.syment._n._n_n._n_zeroes = 0;
	  string_size += name_length + 1;
	}
      }
}

#define	set_index(symbol, idx)	((symbol)->udata =(PTR) (idx))

static unsigned int
DEFUN(coff_write_symbol,(abfd, symbol, native, written),
bfd *abfd AND
asymbol *symbol AND
combined_entry_type *native AND
unsigned int written)
{
  unsigned int    numaux = native->u.syment.n_numaux;
  int             type = native->u.syment.n_type;
  int             class =  native->u.syment.n_sclass;
  PTR buf;
  bfd_size_type symesz;

  /* @@ bfd_debug_section isn't accessible outside this file, but we know
     that C_FILE symbols belong there.  So move them.  */
  if (native->u.syment.n_sclass == C_FILE)
    symbol->section = &bfd_debug_section;

  if (symbol->section == &bfd_abs_section) 
  {
    native->u.syment.n_scnum = N_ABS;
  }
  else if (symbol->section == &bfd_debug_section) 
  {
    native->u.syment.n_scnum = N_DEBUG;
  }
  else if (symbol->section == &bfd_und_section)   
  {
    native->u.syment.n_scnum = N_UNDEF;
  }
  else 
  {
    native->u.syment.n_scnum =
     symbol->section->output_section->target_index;
  }
  
  
  coff_fix_symbol_name(abfd, symbol, native);

  symesz = bfd_coff_symesz (abfd);
  buf = bfd_alloc (abfd, symesz);
  bfd_coff_swap_sym_out(abfd, &native->u.syment, buf);
  bfd_write(buf, 1, symesz, abfd);
  bfd_release (abfd, buf);

  if (native->u.syment.n_numaux > 0)
    {
      bfd_size_type auxesz;
      unsigned int j;

      auxesz = bfd_coff_auxesz (abfd);
      buf = bfd_alloc (abfd, auxesz);
      for (j = 0; j < native->u.syment.n_numaux;  j++)
	{
	  bfd_coff_swap_aux_out(abfd,
				&((native + j + 1)->u.auxent),
				type,
				class,
				buf);
	  bfd_write(buf, 1, auxesz, abfd);
	}
      bfd_release (abfd, buf);
    }
  /*
    Reuse somewhere in the symbol to keep the index
    */
  set_index(symbol, written);
  return   written + 1 + numaux;
}


static unsigned int
DEFUN(coff_write_alien_symbol,(abfd, symbol, written),
      bfd *abfd AND
      asymbol *symbol AND
      unsigned int written)
{
  /*
    This symbol has been created by the loader, or come from a non
    coff format. It  has no native element to inherit, make our
    own
    */
  combined_entry_type *native;
  combined_entry_type dummy;
  native = &dummy;
  native->u.syment.n_type =  T_NULL;
  native->u.syment.n_flags =  0;
  if (symbol->section == &bfd_und_section) 
  {
      native->u.syment.n_scnum =  N_UNDEF;
      native->u.syment.n_value =  symbol->value;
    }
  else if (bfd_is_com_section (symbol->section))
  {
      native->u.syment.n_scnum =  N_UNDEF;
      native->u.syment.n_value =  symbol->value;

  }
  
  else if (symbol->flags & BSF_DEBUGGING) {
      /*
	remove name so it doesn't take up any space
	*/
      symbol->name = "";
    }
  else {
      native->u.syment.n_scnum  =   symbol->section->output_section->target_index;
      native->u.syment.n_value =   symbol->value +
       symbol->section->output_section->vma +
	symbol->section->output_offset;
      /* Copy the any flags from the the file hdr into the symbol  */
    {
      coff_symbol_type *c = coff_symbol_from(abfd, symbol);
      if (c != (coff_symbol_type *)NULL) {
	  native->u.syment.n_flags =   bfd_asymbol_bfd(&c->symbol)->flags;
	}
    }
    }

  native->u.syment.n_type =  0;
  if (symbol->flags & BSF_LOCAL)
   native->u.syment.n_sclass =  C_STAT;
  else
   native->u.syment.n_sclass =  C_EXT;
  native->u.syment.n_numaux =  0;

  return   coff_write_symbol(abfd, symbol, native, written);
}

static unsigned int
DEFUN(coff_write_native_symbol,(abfd, symbol,   written),
bfd *abfd AND
coff_symbol_type *symbol AND
unsigned int written)
{
  /*
    Does this symbol have an ascociated line number - if so then
    make it remember this symbol index. Also tag the auxent of
    this symbol to point to the right place in the lineno table
    */
  combined_entry_type *native = symbol->native;

  alent          *lineno = symbol->lineno;

  if (lineno && !symbol->done_lineno) {
    unsigned int    count = 0;
    lineno[count].u.offset = written;
    if (native->u.syment.n_numaux) {
      union internal_auxent  *a = &((native+1)->u.auxent);

      a->x_sym.x_fcnary.x_fcn.x_lnnoptr =
	symbol->symbol.section->output_section->moving_line_filepos;
    }
    /*
      And count and relocate all other linenumbers
      */

    count++;
    while (lineno[count].line_number) {
#if 0
/* 13 april 92. sac 
I've been told this, but still need proof:
> The second bug is also in `bfd/coffcode.h'.  This bug causes the linker to screw
> up the pc-relocations for all the line numbers in COFF code.  This bug isn't
> only specific to A29K implementations, but affects all systems using COFF
> format binaries.  Note that in COFF object files, the line number core offsets
> output by the assembler are relative to the start of each procedure, not
> to the start of the .text section.  This patch relocates the line numbers
> relative to the `native->u.syment.n_value' instead of the section virtual
> address.  modular!olson@cs.arizona.edu (Jon Olson)
*/
       lineno[count].u.offset += native->u.syment.n_value;

#else
      lineno[count].u.offset +=
	symbol->symbol.section->output_section->vma +
	  symbol->symbol.section->output_offset;
#endif
      count++;
    }
    symbol->done_lineno = true;
    
    symbol->symbol.section->output_section->moving_line_filepos +=
      count * bfd_coff_linesz (abfd);
  }
  return coff_write_symbol(abfd, &( symbol->symbol), native,written);
}

void
DEFUN(coff_write_symbols,(abfd),
      bfd            *abfd)
{
  unsigned int    i;
  unsigned int    limit = bfd_get_symcount(abfd);
  unsigned int    written = 0;

  asymbol       **p;

  string_size = 0;


  /* Seek to the right place */
  bfd_seek(abfd, obj_sym_filepos(abfd), SEEK_SET);

  /* Output all the symbols we have */

  written = 0;
  for (p = abfd->outsymbols, i = 0; i < limit; i++, p++)
      {
	asymbol        *symbol = *p;
	coff_symbol_type *c_symbol = coff_symbol_from(abfd, symbol);

	if (c_symbol == (coff_symbol_type *) NULL ||
	    c_symbol->native == (combined_entry_type *)NULL)
	    {
	      written = coff_write_alien_symbol(abfd, symbol, written);
	    }
	else
	    {
	      written = coff_write_native_symbol(abfd, c_symbol, written);
	    }

      }

  bfd_get_symcount(abfd) = written;

  /* Now write out strings */

  if (string_size != 0)
   {
     unsigned int    size = string_size + 4;
     bfd_byte buffer[4];

     bfd_h_put_32(abfd, size, buffer);
     bfd_write((PTR) buffer, 1, sizeof(buffer), abfd);
     for (p = abfd->outsymbols, i = 0;
	  i < limit;
	  i++, p++)
	 {
	   asymbol        *q = *p;
	   size_t          name_length = strlen(q->name);
	   int maxlen;
	   coff_symbol_type*	   c_symbol = coff_symbol_from(abfd, q);
	   maxlen = ((c_symbol != NULL && c_symbol->native != NULL) &&
		     (c_symbol->native->u.syment.n_sclass == C_FILE)) ?
	     FILNMLEN : SYMNMLEN;

	   if (name_length > maxlen) {
	     bfd_write((PTR) (q->name), 1, name_length + 1, abfd);
	   }
	 }
   }
  else {
    /* We would normally not write anything here, but we'll write
       out 4 so that any stupid coff reader which tries to read
       the string table even when there isn't one won't croak.  */
    unsigned int size = 4;
    bfd_byte buffer[4];

    bfd_h_put_32 (abfd, size, buffer);
    bfd_write((PTR) buffer, 1, sizeof (buffer), abfd);
  }
}

void
DEFUN(coff_write_linenumbers,(abfd),
      bfd            *abfd)
{
  asection       *s;
  bfd_size_type linesz;
  PTR buff;

  linesz = bfd_coff_linesz (abfd);
  buff = bfd_alloc (abfd, linesz);
  for (s = abfd->sections; s != (asection *) NULL; s = s->next) {
    if (s->lineno_count) {
      asymbol       **q = abfd->outsymbols;
      bfd_seek(abfd, s->line_filepos, SEEK_SET);
      /* Find all the linenumbers in this section */
      while (*q) {
	asymbol        *p = *q;
	if (p->section->output_section == s) {
	  alent          *l =
	   BFD_SEND(bfd_asymbol_bfd(p), _get_lineno, (bfd_asymbol_bfd(p), p));
	  if (l) {
	    /* Found a linenumber entry, output */
	    struct internal_lineno  out;
	    memset( (PTR)&out, 0, sizeof(out));
	    out.l_lnno = 0;
	    out.l_addr.l_symndx = l->u.offset;
	    bfd_coff_swap_lineno_out(abfd, &out, buff);
	    bfd_write(buff, 1, linesz, abfd);
	    l++;
	    while (l->line_number) {
	      out.l_lnno = l->line_number;
	      out.l_addr.l_symndx = l->u.offset;
	      bfd_coff_swap_lineno_out(abfd, &out, buff);
	      bfd_write(buff, 1, linesz, abfd);
	      l++;
	    }
	  }
	}
	q++;
      }
    }
  }
  bfd_release (abfd, buff);
}

alent   *
DEFUN(coff_get_lineno,(ignore_abfd, symbol),
      bfd            *ignore_abfd AND
      asymbol        *symbol)
{
  return coffsymbol(symbol)->lineno;
}

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
      struct foo {
	coff_symbol_type sym;
	/* @@FIXME This shouldn't use a fixed size!!  */
	combined_entry_type e[10];
      };
      struct foo *f;
      f = (struct foo *) bfd_alloc_by_size_t (abfd, sizeof (*f));
      memset ((char *) f, 0, sizeof (*f));
      coff_symbol_from (abfd, sym)->native = csym = f->e;
    }
  csym[0].u.syment.n_sclass = C_STAT;
  csym[0].u.syment.n_numaux = 1;
/*  SF_SET_STATICS (sym);	@@ ??? */
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

/* This function transforms the offsets into the symbol table into
   pointers to syments.  */

static void
DEFUN(coff_pointerize_aux,(abfd, table_base, type, class, auxent),
bfd *abfd AND
combined_entry_type *table_base AND
int type AND
int class AND
combined_entry_type *auxent)
{
  /* Don't bother if this is a file or a section */
  if (class == C_STAT && type == T_NULL) return;
  if (class == C_FILE) return;

  /* Otherwise patch up */
#define N_TMASK coff_data (abfd)->local_n_tmask
#define N_BTSHFT coff_data (abfd)->local_n_btshft
  if (ISFCN(type) || ISTAG(class) || class == C_BLOCK) {
      auxent->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.p = table_base +
       auxent->u.auxent.x_sym.x_fcnary.x_fcn.x_endndx.l;
      auxent->fix_end = 1;
    }
  /* A negative tagndx is meaningless, but the SCO 3.2v4 cc can
     generate one, so we must be careful to ignore it.  */
  if (auxent->u.auxent.x_sym.x_tagndx.l > 0) {
      auxent->u.auxent.x_sym.x_tagndx.p =
       table_base +  auxent->u.auxent.x_sym.x_tagndx.l;
      auxent->fix_tag = 1;
    }
}

static char *
DEFUN(build_string_table,(abfd),
bfd *abfd)
{
  char string_table_size_buffer[4];
  unsigned int string_table_size;
  char *string_table;

  /* At this point we should be "seek"'d to the end of the
     symbols === the symbol table size.  */
  if (bfd_read((char *) string_table_size_buffer,
	       sizeof(string_table_size_buffer),
	       1, abfd) != sizeof(string_table_size)) {
    bfd_error = system_call_error;
    return (NULL);
  }				/* on error */

  string_table_size = bfd_h_get_32(abfd, (bfd_byte *) string_table_size_buffer);

  if ((string_table = (PTR) bfd_alloc(abfd, string_table_size -= 4)) == NULL) {
    bfd_error = no_memory;
    return (NULL);
  }				/* on mallocation error */
  if (bfd_read(string_table, string_table_size, 1, abfd) != string_table_size) {
    bfd_error = system_call_error;
    return (NULL);
  }
  return string_table;
}

/* Allocate space for the ".debug" section, and read it.
   We did not read the debug section until now, because
   we didn't want to go to the trouble until someone needed it. */

static char *
DEFUN(build_debug_section,(abfd),
	bfd *abfd)
{
  char *debug_section;
  long position;

  asection *sect = bfd_get_section_by_name (abfd, ".debug");

  if (!sect) {
     bfd_error = no_debug_section;
     return NULL;
  }

  debug_section = (PTR) bfd_alloc (abfd,
				   bfd_get_section_size_before_reloc (sect));
  if (debug_section == NULL) {
    bfd_error = no_memory;
    return NULL;
  }

  /* Seek to the beginning of the `.debug' section and read it. 
     Save the current position first; it is needed by our caller.
     Then read debug section and reset the file pointer.  */

  position = bfd_tell (abfd);
  bfd_seek (abfd, sect->filepos, SEEK_SET);
  if (bfd_read (debug_section, 
		bfd_get_section_size_before_reloc (sect), 1, abfd)
      != bfd_get_section_size_before_reloc(sect)) {
    bfd_error = system_call_error;
    return NULL;
  }
  bfd_seek (abfd, position, SEEK_SET);
  return debug_section;
}


/* Return a pointer to a malloc'd copy of 'name'.  'name' may not be
 \0-terminated, but will not exceed 'maxlen' characters.  The copy *will*
 be \0-terminated.  */
static char *
DEFUN(copy_name,(abfd, name, maxlen),
      bfd *abfd AND
      char *name AND
      int maxlen)
{
  int  len;
  char *newname;

  for (len = 0; len < maxlen; ++len) {
    if (name[len] == '\0') {
      break;
    }
  }

  if ((newname = (PTR) bfd_alloc(abfd, len+1)) == NULL) {
    bfd_error = no_memory;
    return (NULL);
  }
  strncpy(newname, name, len);
  newname[len] = '\0';
  return newname;
}

/* Read a symbol table into freshly bfd_allocated memory, swap it, and
   knit the symbol names into a normalized form.  By normalized here I
   mean that all symbols have an n_offset pointer that points to a null-
   terminated string.  */

combined_entry_type *
DEFUN(coff_get_normalized_symtab,(abfd),
bfd            *abfd)
{
  combined_entry_type          *internal;
  combined_entry_type          *internal_ptr;
  combined_entry_type          *symbol_ptr;
  combined_entry_type         *internal_end;
  bfd_size_type symesz;
  PTR raw;
  char *raw_src;
  char *raw_end;
  char           *string_table = NULL;
  char		 *debug_section = NULL;
  unsigned long   size;

  unsigned int raw_size;
  if (obj_raw_syments(abfd) != (combined_entry_type *)NULL) {
      return obj_raw_syments(abfd);
    }
  if ((size = bfd_get_symcount(abfd) * sizeof(combined_entry_type)) == 0) {
      bfd_error = no_symbols;
      return (NULL);
    }

  internal = (combined_entry_type *)bfd_alloc(abfd, size);
  internal_end = internal + bfd_get_symcount(abfd);

  symesz = bfd_coff_symesz (abfd);
  raw_size =      bfd_get_symcount(abfd) * symesz;
  raw = bfd_alloc(abfd,raw_size);

  if (bfd_seek(abfd, obj_sym_filepos(abfd), SEEK_SET) == -1
      || bfd_read(raw, raw_size, 1, abfd) != raw_size) {
      bfd_error = system_call_error;
      return (NULL);
    }
  /* mark the end of the symbols */
  raw_end = (char *) raw + bfd_get_symcount(abfd) * symesz;
  /*
    FIXME SOMEDAY.  A string table size of zero is very weird, but
    probably possible.  If one shows up, it will probably kill us.
    */

  /* Swap all the raw entries */
  for (raw_src = (char *) raw, internal_ptr = internal;
       raw_src < raw_end;
       raw_src += symesz, internal_ptr++) {

      unsigned int i;
      bfd_coff_swap_sym_in(abfd, (PTR)raw_src, (PTR)&internal_ptr->u.syment);
      internal_ptr->fix_tag = 0;
      internal_ptr->fix_end = 0;
      symbol_ptr = internal_ptr;

      for (i = 0;
	   i < symbol_ptr->u.syment.n_numaux;
	   i++) 
      {
	internal_ptr++;
	raw_src += symesz;
      
	internal_ptr->fix_tag = 0;
	internal_ptr->fix_end = 0;
	bfd_coff_swap_aux_in(abfd, (PTR) raw_src,
			     symbol_ptr->u.syment.n_type,
			     symbol_ptr->u.syment.n_sclass,
			     &(internal_ptr->u.auxent));
	/* Remember that bal entries arn't pointerized */
	if (i != 1 || symbol_ptr->u.syment.n_sclass != C_LEAFPROC)
	{
	  
	coff_pointerize_aux(abfd,
			    internal,
			    symbol_ptr->u.syment.n_type,
			    symbol_ptr->u.syment.n_sclass,
			    internal_ptr);
      }
	
      }
    }

  /* Free all the raw stuff */
  bfd_release(abfd, raw);

  for (internal_ptr = internal; internal_ptr < internal_end;
       internal_ptr ++)
  {
    if (internal_ptr->u.syment.n_sclass == C_FILE) {
	/* make a file symbol point to the name in the auxent, since
	   the text ".file" is redundant */
	if ((internal_ptr+1)->u.auxent.x_file.x_n.x_zeroes == 0) {
	    /* the filename is a long one, point into the string table */
	    if (string_table == NULL) {
		string_table = build_string_table(abfd);
	      }

	    internal_ptr->u.syment._n._n_n._n_offset =
	     (long) (string_table - 4 +
		    (internal_ptr+1)->u.auxent.x_file.x_n.x_offset);
	  }
	else {
	    /* ordinary short filename, put into memory anyway */
	    internal_ptr->u.syment._n._n_n._n_offset = (long)
	     copy_name(abfd, (internal_ptr+1)->u.auxent.x_file.x_fname,
		       FILNMLEN);
	  }
      }
    else {
	if (internal_ptr->u.syment._n._n_n._n_zeroes != 0) {
	    /* This is a "short" name.  Make it long.  */
	    unsigned long   i = 0;
	    char           *newstring = NULL;

	    /* find the length of this string without walking into memory
	       that isn't ours.  */
	    for (i = 0; i < 8; ++i) {
		if (internal_ptr->u.syment._n._n_name[i] == '\0') {
		    break;
		  }		/* if end of string */
	      }			/* possible lengths of this string. */

	    if ((newstring = (PTR) bfd_alloc(abfd, ++i)) == NULL) {
		bfd_error = no_memory;
		return (NULL);
	      }			/* on error */
	    memset(newstring, 0, i);
	    strncpy(newstring, internal_ptr->u.syment._n._n_name, i-1);
	    internal_ptr->u.syment._n._n_n._n_offset =  (long int) newstring;
	    internal_ptr->u.syment._n._n_n._n_zeroes = 0;
	  }
	else if (!bfd_coff_symname_in_debug(abfd, &internal_ptr->u.syment)) {
	    /* Long name already.  Point symbol at the string in the table.  */
	    if (string_table == NULL) {
		string_table = build_string_table(abfd);
	      }
	    internal_ptr->u.syment._n._n_n._n_offset = (long int)
	     (string_table - 4 + internal_ptr->u.syment._n._n_n._n_offset);
	  }
	else {
	    /* Long name in debug section.  Very similar.  */
	    if (debug_section == NULL) {
		debug_section = build_debug_section(abfd);
	      }
	    internal_ptr->u.syment._n._n_n._n_offset = (long int)
	     (debug_section + internal_ptr->u.syment._n._n_n._n_offset);
	  }
      }
    internal_ptr += internal_ptr->u.syment.n_numaux;
  }

  obj_raw_syments(abfd) = internal;
  obj_raw_syment_count(abfd) = internal_ptr - internal;

  return (internal);
}				/* coff_get_normalized_symtab() */

unsigned int
DEFUN (coff_get_reloc_upper_bound, (abfd, asect),
       bfd            *abfd AND
       sec_ptr         asect)
{
  if (bfd_get_format(abfd) != bfd_object) {
    bfd_error = invalid_operation;
    return 0;
  }
  return (asect->reloc_count + 1) * sizeof(arelent *);
}

asymbol *
DEFUN (coff_make_empty_symbol, (abfd),
       bfd            *abfd)
{
  coff_symbol_type *new = (coff_symbol_type *) bfd_alloc(abfd, sizeof(coff_symbol_type));
  if (new == NULL) {
    bfd_error = no_memory;
    return (NULL);
  }				/* on error */
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
  coff_symbol_type *new = (coff_symbol_type *) bfd_alloc(abfd, sizeof(coff_symbol_type));
  if (new == NULL) {
    bfd_error = no_memory;
    return (NULL);
  }				/* on error */
  /* @@ This shouldn't be using a constant multiplier.  */
  new->native = (combined_entry_type *) bfd_zalloc (abfd, sizeof (combined_entry_type) * 10);
  new->symbol.section = &bfd_debug_section;
  new->lineno = (alent *) NULL;
  new->done_lineno = false;
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}

void
coff_get_symbol_info (abfd, symbol, ret)
     bfd *abfd;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
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
	       coffsymbol(symbol)->native ? "n" : "g",
	       coffsymbol(symbol)->lineno ? "l" : " ");
      break;

    case bfd_print_symbol_all:
      if (coffsymbol(symbol)->native) 
	{
	  unsigned int aux;
	  combined_entry_type *combined = coffsymbol (symbol)->native;
	  combined_entry_type *root = obj_raw_syments (abfd);
	  struct lineno_cache_entry *l = coffsymbol(symbol)->lineno;
	
	  fprintf (file,"[%3d]", combined - root);

	  fprintf (file,
		   "(sc %2d)(fl 0x%02x)(ty %3x)(sc %3d) (nx %d) 0x%08x %s",
		   combined->u.syment.n_scnum,
		   combined->u.syment.n_flags,
		   combined->u.syment.n_type,
		   combined->u.syment.n_sclass,
		   combined->u.syment.n_numaux,
		   combined->u.syment.n_value,
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
	      switch (combined->u.syment.n_sclass)
		{
		case C_FILE:
		  fprintf (file, "File ");
		  break;
		default:

		  fprintf (file, "AUX lnno %d size 0x%x tagndx %d",
			   auxp->u.auxent.x_sym.x_misc.x_lnsz.x_lnno,
			   auxp->u.auxent.x_sym.x_misc.x_lnsz.x_size,
			   tagndx);
		  break;
		}
	    }
	
	  if (l)
	    {
	      fprintf (file, "\n%s :", l->u.sym->name);
	      l++;
	      while (l->line_number) 
		{
		  fprintf (file, "\n%4d : 0x%x",
			  l->line_number,
			  l->u.offset + symbol->section->vma);
		  l++;
		}
	    }
	} 
      else
	{
	  bfd_print_symbol_vandf ((PTR) file, symbol);
	  fprintf (file, " %-5s %s %s %s",
		   symbol->section->name,
		   coffsymbol(symbol)->native ? "n" : "g",
		   coffsymbol(symbol)->lineno ? "l" : " ",
		   symbol->name);
	}
    }
}

/* Provided a BFD, a section and an offset into the section, calculate
   and return the name of the source file and the line nearest to the
   wanted location.  */

boolean
DEFUN(coff_find_nearest_line,(abfd,
			      section,
			      ignore_symbols,
			      offset,
			      filename_ptr,
			      functionname_ptr,
			      line_ptr),
      bfd            *abfd AND
      asection       *section AND
      asymbol       **ignore_symbols AND
      bfd_vma         offset AND
      CONST char      **filename_ptr AND
      CONST char       **functionname_ptr AND
      unsigned int   *line_ptr)
{
  static bfd     *cache_abfd;
  static asection *cache_section;
  static bfd_vma  cache_offset;
  static unsigned int cache_i;
  static CONST char *cache_function;
  static unsigned int    line_base = 0;

  unsigned int    i = 0;
  coff_data_type *cof = coff_data(abfd);
  /* Run through the raw syments if available */
  combined_entry_type *p;
  alent          *l;


  *filename_ptr = 0;
  *functionname_ptr = 0;
  *line_ptr = 0;

  /* Don't try and find line numbers in a non coff file */
  if (abfd->xvec->flavour != bfd_target_coff_flavour)
    return false;

  if (cof == NULL)
    return false;

  p = cof->raw_syments;

  for (i = 0; i < cof->raw_syment_count; i++) {
    if (p->u.syment.n_sclass == C_FILE) {
      /* File name has been moved into symbol */
      *filename_ptr = (char *) p->u.syment._n._n_n._n_offset;
      break;
    }
    p += 1 +  p->u.syment.n_numaux;
  }
  /* Now wander though the raw linenumbers of the section */
  /*
    If this is the same BFD as we were previously called with and this is
    the same section, and the offset we want is further down then we can
    prime the lookup loop
    */
  if (abfd == cache_abfd &&
      section == cache_section &&
      offset >= cache_offset) {
    i = cache_i;
    *functionname_ptr = cache_function;
  }
  else {
    i = 0;
  }
  l = &section->lineno[i];

  for (; i < section->lineno_count; i++) {
    if (l->line_number == 0) {
      /* Get the symbol this line number points at */
      coff_symbol_type *coff = (coff_symbol_type *) (l->u.sym);
      if (coff->symbol.value > offset)
	break;
      *functionname_ptr = coff->symbol.name;
      if (coff->native) {
	combined_entry_type  *s = coff->native;
	s = s + 1 + s->u.syment.n_numaux;
	/*
	  S should now point to the .bf of the function
	  */
	if (s->u.syment.n_numaux) {
	  /*
	    The linenumber is stored in the auxent
	    */
	  union internal_auxent   *a = &((s + 1)->u.auxent);
	  line_base = a->x_sym.x_misc.x_lnsz.x_lnno;
	  *line_ptr = line_base;
	}
      }
    }
    else {
      if (l->u.offset > offset)
	break;
      *line_ptr = l->line_number + line_base - 1;
    }
    l++;
  }

  cache_abfd = abfd;
  cache_section = section;
  cache_offset = offset;
  cache_i = i;
  cache_function = *functionname_ptr;

  return true;
}

int
DEFUN(coff_sizeof_headers,(abfd, reloc),
      bfd *abfd AND
      boolean reloc)
{
    size_t size;

    if (reloc == false) {
	size = bfd_coff_filhsz (abfd) + bfd_coff_aoutsz (abfd);
    }
    else {
	size = bfd_coff_filhsz (abfd);
    }

    size +=  abfd->section_count * bfd_coff_scnhsz (abfd);
    return size;
}
