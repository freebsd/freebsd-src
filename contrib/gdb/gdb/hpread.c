/* Read hp debug symbols and convert to internal format, for GDB.
   Copyright 1993, 1996 Free Software Foundation, Inc.

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by the Center for Software Science at the University of Utah
   and by Cygnus Support.  */

#include "defs.h"
#include "bfd.h"
#include "gdb_string.h"
#include "hp-symtab.h"
#include "syms.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "buildsym.h"
#include "complaints.h"
#include "gdb-stabs.h"
#include "gdbtypes.h"
#include "demangle.h"

/* Private information attached to an objfile which we use to find
   and internalize the HP C debug symbols within that objfile.  */

struct hpread_symfile_info
{
  /* The contents of each of the debug sections (there are 4 of them).  */
  char *gntt;
  char *lntt;
  char *slt;
  char *vt;

  /* We keep the size of the $VT$ section for range checking.  */
  unsigned int vt_size;

  /* Some routines still need to know the number of symbols in the
     main debug sections ($LNTT$ and $GNTT$). */
  unsigned int lntt_symcount;
  unsigned int gntt_symcount;

  /* To keep track of all the types we've processed.  */
  struct type **type_vector;
  int type_vector_length;

  /* Keeps track of the beginning of a range of source lines.  */
  sltpointer sl_index;

  /* Some state variables we'll need.  */
  int within_function;

  /* Keep track of the current function's address.  We may need to look
     up something based on this address.  */
  unsigned int current_function_value;
};

/* Accessor macros to get at the fields.  */
#define HPUX_SYMFILE_INFO(o) \
  ((struct hpread_symfile_info *)((o)->sym_private))
#define GNTT(o)                 (HPUX_SYMFILE_INFO(o)->gntt)
#define LNTT(o)                 (HPUX_SYMFILE_INFO(o)->lntt)
#define SLT(o)                  (HPUX_SYMFILE_INFO(o)->slt)
#define VT(o)                   (HPUX_SYMFILE_INFO(o)->vt)
#define VT_SIZE(o)              (HPUX_SYMFILE_INFO(o)->vt_size)
#define LNTT_SYMCOUNT(o)        (HPUX_SYMFILE_INFO(o)->lntt_symcount)
#define GNTT_SYMCOUNT(o)        (HPUX_SYMFILE_INFO(o)->gntt_symcount)
#define TYPE_VECTOR(o)          (HPUX_SYMFILE_INFO(o)->type_vector)
#define TYPE_VECTOR_LENGTH(o)   (HPUX_SYMFILE_INFO(o)->type_vector_length)
#define SL_INDEX(o)             (HPUX_SYMFILE_INFO(o)->sl_index)
#define WITHIN_FUNCTION(o)      (HPUX_SYMFILE_INFO(o)->within_function)
#define CURRENT_FUNCTION_VALUE(o) (HPUX_SYMFILE_INFO(o)->current_function_value)

/* Given the native debug symbol SYM, set NAMEP to the name associated
   with the debug symbol.  Note we may be called with a debug symbol which
   has no associated name, in that case we return an empty string.

   Also note we "know" that the name for any symbol is always in the
   same place.  Hence we don't have to conditionalize on the symbol type.  */
#define SET_NAMESTRING(SYM, NAMEP, OBJFILE) \
  if (! hpread_has_name ((SYM)->dblock.kind)) \
    *NAMEP = ""; \
  else if (((unsigned)(SYM)->dsfile.name) >= VT_SIZE (OBJFILE)) \
    { \
      complain (&string_table_offset_complaint, (char *) symnum); \
      *NAMEP = ""; \
    } \
  else \
    *NAMEP = (SYM)->dsfile.name + VT (OBJFILE)

/* We put a pointer to this structure in the read_symtab_private field
   of the psymtab.  */

struct symloc
{
  /* The offset within the file symbol table of first local symbol for
     this file.  */

  int ldsymoff;

  /* Length (in bytes) of the section of the symbol table devoted to
     this file's symbols (actually, the section bracketed may contain
     more than just this file's symbols).  If ldsymlen is 0, the only
     reason for this thing's existence is the dependency list.
     Nothing else will happen when it is read in.  */

  int ldsymlen;
};

#define LDSYMOFF(p) (((struct symloc *)((p)->read_symtab_private))->ldsymoff)
#define LDSYMLEN(p) (((struct symloc *)((p)->read_symtab_private))->ldsymlen)
#define SYMLOC(p) ((struct symloc *)((p)->read_symtab_private))

/* FIXME: Shouldn't this stuff be in a .h file somewhere?  */
/* Nonzero means give verbose info on gdb action.  */
extern int info_verbose;

/* Complaints about the symbols we have encountered.  */
extern struct complaint string_table_offset_complaint;
extern struct complaint lbrac_unmatched_complaint;
extern struct complaint lbrac_mismatch_complaint;


void hpread_symfile_init  PARAMS ((struct objfile *));

static struct type *hpread_alloc_type
  PARAMS ((dnttpointer, struct objfile *));

static struct type **hpread_lookup_type
  PARAMS ((dnttpointer, struct objfile *));

static struct type *hpread_read_enum_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *));

static struct type *hpread_read_set_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *));

static struct type *hpread_read_subrange_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *));

static struct type *hpread_read_struct_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *));

void hpread_build_psymtabs
  PARAMS ((struct objfile *, struct section_offsets *, int));

void hpread_symfile_finish PARAMS ((struct objfile *));

static struct partial_symtab *hpread_start_psymtab
  PARAMS ((struct objfile *, struct section_offsets *, char *, CORE_ADDR, int,
	   struct partial_symbol **, struct partial_symbol **));

static struct partial_symtab *hpread_end_psymtab
  PARAMS ((struct partial_symtab *, char **, int, int, CORE_ADDR,
	   struct partial_symtab **, int));

static struct symtab *hpread_expand_symtab
  PARAMS ((struct objfile *, int, int, CORE_ADDR, int,
	   struct section_offsets *, char *));

static void hpread_process_one_debug_symbol
  PARAMS ((union dnttentry *, char *, struct section_offsets *,
	   struct objfile *, CORE_ADDR, int, char *, int));

static sltpointer hpread_record_lines
  PARAMS ((struct subfile *, sltpointer, sltpointer,
	   struct objfile *, CORE_ADDR));

static struct type *hpread_read_function_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *));

static struct type * hpread_type_lookup
  PARAMS ((dnttpointer, struct objfile *));

static unsigned long hpread_get_depth
  PARAMS ((sltpointer, struct objfile *));

static unsigned long hpread_get_line
  PARAMS ((sltpointer, struct objfile *));

static CORE_ADDR hpread_get_location
  PARAMS ((sltpointer, struct objfile *));

static int hpread_type_translate PARAMS ((dnttpointer));
static unsigned long hpread_get_textlow PARAMS ((int, int, struct objfile *));
static union dnttentry *hpread_get_gntt PARAMS ((int, struct objfile *));
static union dnttentry *hpread_get_lntt PARAMS ((int, struct objfile *));
static union sltentry *hpread_get_slt PARAMS ((int, struct objfile *));
static void hpread_psymtab_to_symtab PARAMS ((struct partial_symtab *));
static void hpread_psymtab_to_symtab_1 PARAMS ((struct partial_symtab *));
static int hpread_has_name PARAMS ((enum dntt_entry_type));


/* Initialization for reading native HP C debug symbols from OBJFILE.

   It's only purpose in life is to set up the symbol reader's private
   per-objfile data structures, and read in the raw contents of the debug
   sections (attaching pointers to the debug info into the private data
   structures).

   Since BFD doesn't know how to read debug symbols in a format-independent
   way (and may never do so...), we have to do it ourselves.  Note we may
   be called on a file without native HP C debugging symbols.
   FIXME, there should be a cleaner peephole into the BFD environment here.  */

void
hpread_symfile_init (objfile)
     struct objfile *objfile;
{
  asection *vt_section, *slt_section, *lntt_section, *gntt_section;

  /* Allocate struct to keep track of the symfile */
  objfile->sym_private = (PTR)
    xmmalloc (objfile->md, sizeof (struct hpread_symfile_info));
  memset (objfile->sym_private, 0, sizeof (struct hpread_symfile_info));

  /* We haven't read in any types yet.  */
  TYPE_VECTOR (objfile) = 0;

  /* Read in data from the $GNTT$ subspace.  */
  gntt_section = bfd_get_section_by_name (objfile->obfd, "$GNTT$");
  if (!gntt_section)
    return;

  GNTT (objfile)
    = obstack_alloc (&objfile->symbol_obstack,
		     bfd_section_size (objfile->obfd, gntt_section));

  bfd_get_section_contents (objfile->obfd, gntt_section, GNTT (objfile),
			    0, bfd_section_size (objfile->obfd, gntt_section));

  GNTT_SYMCOUNT (objfile)
    = bfd_section_size (objfile->obfd, gntt_section)
			/ sizeof (struct dntt_type_block);

  /* Read in data from the $LNTT$ subspace.   Also keep track of the number
     of LNTT symbols.  */
  lntt_section = bfd_get_section_by_name (objfile->obfd, "$LNTT$");
  if (!lntt_section)
    return;

  LNTT (objfile)
    = obstack_alloc (&objfile->symbol_obstack,
		     bfd_section_size (objfile->obfd, lntt_section));

  bfd_get_section_contents (objfile->obfd, lntt_section, LNTT (objfile),
			    0, bfd_section_size (objfile->obfd, lntt_section));

  LNTT_SYMCOUNT (objfile)
    = bfd_section_size (objfile->obfd, lntt_section)
			/ sizeof (struct dntt_type_block);

  /* Read in data from the $SLT$ subspace.  $SLT$ contains information
     on source line numbers.  */
  slt_section = bfd_get_section_by_name (objfile->obfd, "$SLT$");
  if (!slt_section)
    return;

  SLT (objfile) =
    obstack_alloc (&objfile->symbol_obstack,
		   bfd_section_size (objfile->obfd, slt_section));

  bfd_get_section_contents (objfile->obfd, slt_section, SLT (objfile),
			    0, bfd_section_size (objfile->obfd, slt_section));

  /* Read in data from the $VT$ subspace.  $VT$ contains things like
     names and constants.  Keep track of the number of symbols in the VT.  */
  vt_section = bfd_get_section_by_name (objfile->obfd, "$VT$");
  if (!vt_section)
    return;

  VT_SIZE (objfile) = bfd_section_size (objfile->obfd, vt_section);

  VT (objfile) =
    (char *) obstack_alloc (&objfile->symbol_obstack,
			    VT_SIZE (objfile));

  bfd_get_section_contents (objfile->obfd, vt_section, VT (objfile),
			    0, VT_SIZE (objfile));
}

/* Scan and build partial symbols for a symbol file.

   The minimal symbol table (either SOM or HP a.out) has already been
   read in; all we need to do is setup partial symbols based on the
   native debugging information.

   We assume hpread_symfile_init has been called to initialize the
   symbol reader's private data structures.

   SECTION_OFFSETS contains offsets relative to which the symbols in the
   various sections are (depending where the sections were actually loaded).
   MAINLINE is true if we are reading the main symbol
   table (as opposed to a shared lib or dynamically loaded file).  */

void
hpread_build_psymtabs (objfile, section_offsets, mainline)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     int mainline;
{
  char *namestring;
  int past_first_source_file = 0;
  struct cleanup *old_chain;

  int hp_symnum, symcount, i;

  union dnttentry *dn_bufp;
  unsigned long valu;
  char *p;
  int texthigh = 0;
  int have_name = 0;

  /* Current partial symtab */
  struct partial_symtab *pst;

  /* List of current psymtab's include files */
  char **psymtab_include_list;
  int includes_allocated;
  int includes_used;

  /* Index within current psymtab dependency list */
  struct partial_symtab **dependency_list;
  int dependencies_used, dependencies_allocated;

  /* Just in case the stabs reader left turds lying around.  */
  pending_blocks = 0;
  make_cleanup (really_free_pendings, 0);

  pst = (struct partial_symtab *) 0;

  /* We shouldn't use alloca, instead use malloc/free.  Doing so avoids
     a number of problems with cross compilation and creating useless holes
     in the stack when we have to allocate new entries.  FIXME.  */

  includes_allocated = 30;
  includes_used = 0;
  psymtab_include_list = (char **) alloca (includes_allocated *
					   sizeof (char *));

  dependencies_allocated = 30;
  dependencies_used = 0;
  dependency_list =
    (struct partial_symtab **) alloca (dependencies_allocated *
				       sizeof (struct partial_symtab *));

  old_chain = make_cleanup (free_objfile, objfile);

  last_source_file = 0;

  /* Make two passes, one ofr the GNTT symbols, the other for the
     LNTT symbols.  */
  for (i = 0; i < 1; i++)
    {
      int within_function = 0;

      if (i)
	symcount = GNTT_SYMCOUNT (objfile);
      else
	symcount = LNTT_SYMCOUNT (objfile);

      for (hp_symnum = 0; hp_symnum < symcount; hp_symnum++)
	{
	  QUIT;
	  if (i)
	    dn_bufp = hpread_get_gntt (hp_symnum, objfile);
	  else
	    dn_bufp = hpread_get_lntt (hp_symnum, objfile);

	  if (dn_bufp->dblock.extension)
	    continue;

	  /* Only handle things which are necessary for minimal symbols.
	     everything else is ignored.  */
	  switch (dn_bufp->dblock.kind)
	    {
	    case DNTT_TYPE_SRCFILE:
	      {
		/* A source file of some kind.  Note this may simply
		   be an included file.  */
		SET_NAMESTRING (dn_bufp, &namestring, objfile);

		/* Check if this is the source file we are already working
		   with.  */
		if (pst && !strcmp (namestring, pst->filename))
		  continue;

		/* Check if this is an include file, if so check if we have
		   already seen it.  Add it to the include list */
		p = strrchr (namestring, '.');
		if (!strcmp (p, ".h"))
		  {
		    int j, found;

		    found = 0;
		    for (j = 0; j < includes_used; j++)
		      if (!strcmp (namestring, psymtab_include_list[j]))
			{
			  found = 1;
			  break;
			}
		    if (found)
		      continue;

		    /* Add it to the list of includes seen so far and
		       allocate more include space if necessary.  */
		    psymtab_include_list[includes_used++] = namestring;
		    if (includes_used >= includes_allocated)
		      {
			char **orig = psymtab_include_list;

			psymtab_include_list = (char **)
			  alloca ((includes_allocated *= 2) *
				  sizeof (char *));
			memcpy ((PTR) psymtab_include_list, (PTR) orig,
				includes_used * sizeof (char *));
		      }
		    continue;
		  }


		if (pst)
		  {
		    if (!have_name)
		      {
			pst->filename = (char *)
			  obstack_alloc (&pst->objfile->psymbol_obstack,
					 strlen (namestring) + 1);
			strcpy (pst->filename, namestring);
			have_name = 1;
			continue;
		      }
		    continue;
		  }

		/* This is a bonafide new source file.
		   End the current partial symtab and start a new one.  */

		if (pst && past_first_source_file)
		  {
		    hpread_end_psymtab (pst, psymtab_include_list,
					includes_used,
					(hp_symnum
					 * sizeof (struct dntt_type_block)),
					texthigh,
					dependency_list, dependencies_used);
		    pst = (struct partial_symtab *) 0;
		    includes_used = 0;
		    dependencies_used = 0;
		  }
		else
		  past_first_source_file = 1;

		valu = hpread_get_textlow (i, hp_symnum, objfile);
		valu += ANOFFSET (section_offsets, SECT_OFF_TEXT);
		pst = hpread_start_psymtab (objfile, section_offsets,
					    namestring, valu,
					    (hp_symnum
					     * sizeof (struct dntt_type_block)),
					    objfile->global_psymbols.next,
					    objfile->static_psymbols.next);
		texthigh = valu;
		have_name = 1;
		continue;
	      }

	    case DNTT_TYPE_MODULE:
	      /* A source file.  It's still unclear to me what the
		 real difference between a DNTT_TYPE_SRCFILE and DNTT_TYPE_MODULE
		 is supposed to be.  */
	      SET_NAMESTRING (dn_bufp, &namestring, objfile);
	      valu = hpread_get_textlow (i, hp_symnum, objfile);
	      valu += ANOFFSET (section_offsets, SECT_OFF_TEXT);
	      if (!pst)
		{
		  pst = hpread_start_psymtab (objfile, section_offsets,
					      namestring, valu,
					      (hp_symnum
					       * sizeof (struct dntt_type_block)),
					      objfile->global_psymbols.next,
					      objfile->static_psymbols.next);
		  texthigh = valu;
		  have_name = 0;
		}
	      continue;
	    case DNTT_TYPE_FUNCTION:
	    case DNTT_TYPE_ENTRY:
	      /* The beginning of a function.  DNTT_TYPE_ENTRY may also denote
		 a secondary entry point.  */
	      valu = dn_bufp->dfunc.hiaddr + ANOFFSET (section_offsets,
						       SECT_OFF_TEXT);
	      if (valu > texthigh)
		texthigh = valu;
	      valu = dn_bufp->dfunc.lowaddr +
		ANOFFSET (section_offsets, SECT_OFF_TEXT);
	      SET_NAMESTRING (dn_bufp, &namestring, objfile);
	      add_psymbol_to_list (namestring, strlen (namestring),
				   VAR_NAMESPACE, LOC_BLOCK,
				   &objfile->static_psymbols, valu,
				   0, language_unknown, objfile);
	      within_function = 1;
	      continue;
	    case DNTT_TYPE_BEGIN:
	    case DNTT_TYPE_END:
	      /* Scope block begin/end.  We only care about function
		 and file blocks right now.  */
	      if (dn_bufp->dend.endkind == DNTT_TYPE_MODULE)
		{
		  hpread_end_psymtab (pst, psymtab_include_list, includes_used,
				      (hp_symnum
				       * sizeof (struct dntt_type_block)),
				      texthigh,
				      dependency_list, dependencies_used);
		  pst = (struct partial_symtab *) 0;
		  includes_used = 0;
		  dependencies_used = 0;
		  have_name = 0;
		}
	      if (dn_bufp->dend.endkind == DNTT_TYPE_FUNCTION)
		within_function = 0;
	      continue;
	    case DNTT_TYPE_SVAR:
	    case DNTT_TYPE_DVAR:
	    case DNTT_TYPE_TYPEDEF:
	    case DNTT_TYPE_TAGDEF:
	      {
		/* Variables, typedefs an the like.  */
		enum address_class storage;
		namespace_enum namespace;

		/* Don't add locals to the partial symbol table.  */
		if (within_function
		    && (dn_bufp->dblock.kind == DNTT_TYPE_SVAR
			|| dn_bufp->dblock.kind == DNTT_TYPE_DVAR))
		  continue;

		/* TAGDEFs go into the structure namespace.  */
		if (dn_bufp->dblock.kind == DNTT_TYPE_TAGDEF)
		  namespace = STRUCT_NAMESPACE;
		else
		  namespace = VAR_NAMESPACE;

		/* What kind of "storage" does this use?  */
		if (dn_bufp->dblock.kind == DNTT_TYPE_SVAR)
		  storage = LOC_STATIC;
		else if (dn_bufp->dblock.kind == DNTT_TYPE_DVAR
			 && dn_bufp->ddvar.regvar)
		  storage = LOC_REGISTER;
		else if (dn_bufp->dblock.kind == DNTT_TYPE_DVAR)
		  storage = LOC_LOCAL;
		else
		  storage = LOC_UNDEF;

		SET_NAMESTRING (dn_bufp, &namestring, objfile);
		if (!pst)
		  {
		    pst = hpread_start_psymtab (objfile, section_offsets,
						"globals", 0,
						(hp_symnum
						 * sizeof (struct dntt_type_block)),
						objfile->global_psymbols.next,
						objfile->static_psymbols.next);
		  }
		if (dn_bufp->dsvar.global)
		  {
		    add_psymbol_to_list (namestring, strlen (namestring),
					 namespace, storage,
					 &objfile->global_psymbols,
					 dn_bufp->dsvar.location,
					 0, language_unknown, objfile);
		  }
		else
		  {
		    add_psymbol_to_list (namestring, strlen (namestring),
					 namespace, storage,
					 &objfile->static_psymbols,
					 dn_bufp->dsvar.location,
					 0, language_unknown, objfile);
		  }
		continue;
	      }
	    case DNTT_TYPE_MEMENUM:
	    case DNTT_TYPE_CONST:
	      /* Constants and members of enumerated types.  */
	      SET_NAMESTRING (dn_bufp, &namestring, objfile);
	      if (!pst)
		{
		  pst = hpread_start_psymtab (objfile, section_offsets,
					      "globals", 0,
					      (hp_symnum 
					       * sizeof (struct dntt_type_block)),
					      objfile->global_psymbols.next,
					      objfile->static_psymbols.next);
		}
	      add_psymbol_to_list (namestring, strlen (namestring),
				   VAR_NAMESPACE, LOC_CONST,
				   &objfile->static_psymbols, 0,
				   0, language_unknown, objfile);
	      continue;
	    default:
	      continue;
	    }
	}
    }

  /* End any pending partial symbol table.  */
  if (pst)
    {
      hpread_end_psymtab (pst, psymtab_include_list, includes_used,
			  hp_symnum * sizeof (struct dntt_type_block),
			  0, dependency_list, dependencies_used);
    }

  discard_cleanups (old_chain);
}

/* Perform any local cleanups required when we are done with a particular
   objfile.  I.E, we are in the process of discarding all symbol information
   for an objfile, freeing up all memory held for it, and unlinking the
   objfile struct from the global list of known objfiles. */

void
hpread_symfile_finish (objfile)
     struct objfile *objfile;
{
  if (objfile->sym_private != NULL)
    {
      mfree (objfile->md, objfile->sym_private);
    }
}


/* The remaining functions are all for internal use only.  */

/* Various small functions to get entries in the debug symbol sections.  */

static union dnttentry *
hpread_get_lntt (index, objfile)
     int index;
     struct objfile *objfile;
{
  return (union dnttentry *)
    &(LNTT (objfile)[(index * sizeof (struct dntt_type_block))]);
}

static union dnttentry *
hpread_get_gntt (index, objfile)
     int index;
     struct objfile *objfile;
{
  return (union dnttentry *)
    &(GNTT (objfile)[(index * sizeof (struct dntt_type_block))]);
}

static union sltentry *
hpread_get_slt (index, objfile)
     int index;
     struct objfile *objfile;
{
  return (union sltentry *)&(SLT (objfile)[index * sizeof (union sltentry)]);
}

/* Get the low address associated with some symbol (typically the start
   of a particular source file or module).  Since that information is not
   stored as part of the DNTT_TYPE_MODULE or DNTT_TYPE_SRCFILE symbol we must infer it from
   the existance of DNTT_TYPE_FUNCTION symbols.  */

static unsigned long
hpread_get_textlow (global, index, objfile)
     int global;
     int index;
     struct objfile *objfile;
{
  union dnttentry *dn_bufp;
  struct minimal_symbol *msymbol;

  /* Look for a DNTT_TYPE_FUNCTION symbol.  */
  do
    {
      if (global)
	dn_bufp = hpread_get_gntt (index++, objfile);
      else
	dn_bufp = hpread_get_lntt (index++, objfile);
    } while (dn_bufp->dblock.kind != DNTT_TYPE_FUNCTION
	     && dn_bufp->dblock.kind != DNTT_TYPE_END);

  /* Avoid going past a DNTT_TYPE_END when looking for a DNTT_TYPE_FUNCTION.  This
     might happen when a sourcefile has no functions.  */
  if (dn_bufp->dblock.kind == DNTT_TYPE_END)
    return 0;

  /* The minimal symbols are typically more accurate for some reason.  */
  msymbol = lookup_minimal_symbol (dn_bufp->dfunc.name + VT (objfile), NULL,
				   objfile);
  if (msymbol)
    return SYMBOL_VALUE_ADDRESS (msymbol);
  else
    return dn_bufp->dfunc.lowaddr;
}

/* Get the nesting depth for the source line identified by INDEX.  */

static unsigned long
hpread_get_depth (index, objfile)
     sltpointer index;
     struct objfile *objfile;
{
  union sltentry *sl_bufp;

  sl_bufp = hpread_get_slt (index, objfile);
  return sl_bufp->sspec.backptr.dnttp.index;
}

/* Get the source line number the the line identified by INDEX.  */

static unsigned long
hpread_get_line (index, objfile)
     sltpointer index;
     struct objfile *objfile;
{
  union sltentry *sl_bufp;

  sl_bufp = hpread_get_slt (index, objfile);
  return sl_bufp->snorm.line;
}

static CORE_ADDR
hpread_get_location (index, objfile)
     sltpointer index;
     struct objfile *objfile;
{
  union sltentry *sl_bufp;
  int i;

  /* code location of special sltentrys is determined from context */
  sl_bufp = hpread_get_slt (index, objfile);

  if (sl_bufp->snorm.sltdesc == SLT_END)
    {
      /* find previous normal sltentry and get address */
      for (i = 0; ((sl_bufp->snorm.sltdesc != SLT_NORMAL) &&
		   (sl_bufp->snorm.sltdesc != SLT_EXIT)); i++)
	sl_bufp = hpread_get_slt (index - i, objfile);
      return sl_bufp->snorm.address;
    }

  /* find next normal sltentry and get address */
  for (i = 0; ((sl_bufp->snorm.sltdesc != SLT_NORMAL) &&
	       (sl_bufp->snorm.sltdesc != SLT_EXIT)); i++)
    sl_bufp = hpread_get_slt (index + i, objfile);
  return sl_bufp->snorm.address;
}


/* Return 1 if an HP debug symbol of type KIND has a name associated with
   it, else return 0.  */

static int
hpread_has_name (kind)
     enum dntt_entry_type kind;
{
  switch (kind)
    {
    case DNTT_TYPE_SRCFILE:
    case DNTT_TYPE_MODULE:
    case DNTT_TYPE_FUNCTION:
    case DNTT_TYPE_ENTRY:
    case DNTT_TYPE_IMPORT:
    case DNTT_TYPE_LABEL:
    case DNTT_TYPE_FPARAM:
    case DNTT_TYPE_SVAR:
    case DNTT_TYPE_DVAR:
    case DNTT_TYPE_CONST:
    case DNTT_TYPE_TYPEDEF:
    case DNTT_TYPE_TAGDEF:
    case DNTT_TYPE_MEMENUM:
    case DNTT_TYPE_FIELD:
    case DNTT_TYPE_SA:
      return 1;

    case DNTT_TYPE_BEGIN:
    case DNTT_TYPE_END:
    case DNTT_TYPE_WITH:
    case DNTT_TYPE_COMMON:
    case DNTT_TYPE_POINTER:
    case DNTT_TYPE_ENUM:
    case DNTT_TYPE_SET:
    case DNTT_TYPE_SUBRANGE:
    case DNTT_TYPE_ARRAY:
    case DNTT_TYPE_STRUCT:
    case DNTT_TYPE_UNION:
    case DNTT_TYPE_VARIANT:
    case DNTT_TYPE_FILE:
    case DNTT_TYPE_FUNCTYPE:
    case DNTT_TYPE_COBSTRUCT:
    case DNTT_TYPE_XREF:
    case DNTT_TYPE_MACRO:
    default:
      return 0;
    }
}

/* Allocate and partially fill a partial symtab.  It will be
   completely filled at the end of the symbol list.

   SYMFILE_NAME is the name of the symbol-file we are reading from, and ADDR
   is the address relative to which its symbols are (incremental) or 0
   (normal). */

static struct partial_symtab *
hpread_start_psymtab (objfile, section_offsets,
		  filename, textlow, ldsymoff, global_syms, static_syms)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     char *filename;
     CORE_ADDR textlow;
     int ldsymoff;
     struct partial_symbol **global_syms;
     struct partial_symbol **static_syms;
{
  struct partial_symtab *result =
  start_psymtab_common (objfile, section_offsets,
			filename, textlow, global_syms, static_syms);

  result->read_symtab_private = (char *)
    obstack_alloc (&objfile->psymbol_obstack, sizeof (struct symloc));
  LDSYMOFF (result) = ldsymoff;
  result->read_symtab = hpread_psymtab_to_symtab;

  return result;
}


/* Close off the current usage of PST.  
   Returns PST or NULL if the partial symtab was empty and thrown away.

   FIXME:  List variables and peculiarities of same.  */

static struct partial_symtab *
hpread_end_psymtab (pst, include_list, num_includes, capping_symbol_offset,
	     capping_text, dependency_list, number_dependencies)
     struct partial_symtab *pst;
     char **include_list;
     int num_includes;
     int capping_symbol_offset;
     CORE_ADDR capping_text;
     struct partial_symtab **dependency_list;
     int number_dependencies;
{
  int i;
  struct objfile *objfile = pst -> objfile;

  if (capping_symbol_offset != -1)
      LDSYMLEN(pst) = capping_symbol_offset - LDSYMOFF(pst);
  pst->texthigh = capping_text;

  pst->n_global_syms =
    objfile->global_psymbols.next - (objfile->global_psymbols.list + pst->globals_offset);
  pst->n_static_syms =
    objfile->static_psymbols.next - (objfile->static_psymbols.list + pst->statics_offset);

  pst->number_of_dependencies = number_dependencies;
  if (number_dependencies)
    {
      pst->dependencies = (struct partial_symtab **)
	obstack_alloc (&objfile->psymbol_obstack,
		       number_dependencies * sizeof (struct partial_symtab *));
      memcpy (pst->dependencies, dependency_list,
	     number_dependencies * sizeof (struct partial_symtab *));
    }
  else
    pst->dependencies = 0;

  for (i = 0; i < num_includes; i++)
    {
      struct partial_symtab *subpst =
	allocate_psymtab (include_list[i], objfile);

      subpst->section_offsets = pst->section_offsets;
      subpst->read_symtab_private =
	  (char *) obstack_alloc (&objfile->psymbol_obstack,
				  sizeof (struct symloc));
      LDSYMOFF(subpst) =
	LDSYMLEN(subpst) =
	  subpst->textlow =
	    subpst->texthigh = 0;

      /* We could save slight bits of space by only making one of these,
	 shared by the entire set of include files.  FIXME-someday.  */
      subpst->dependencies = (struct partial_symtab **)
	obstack_alloc (&objfile->psymbol_obstack,
		       sizeof (struct partial_symtab *));
      subpst->dependencies[0] = pst;
      subpst->number_of_dependencies = 1;

      subpst->globals_offset =
	subpst->n_global_syms =
	  subpst->statics_offset =
	    subpst->n_static_syms = 0;

      subpst->readin = 0;
      subpst->symtab = 0;
      subpst->read_symtab = pst->read_symtab;
    }

  sort_pst_symbols (pst);

  /* If there is already a psymtab or symtab for a file of this name, remove it.
     (If there is a symtab, more drastic things also happen.)
     This happens in VxWorks.  */
  free_named_symtabs (pst->filename);

  if (num_includes == 0
      && number_dependencies == 0
      && pst->n_global_syms == 0
      && pst->n_static_syms == 0)
    {
      /* Throw away this psymtab, it's empty.  We can't deallocate it, since
	 it is on the obstack, but we can forget to chain it on the list.  */
      /* Empty psymtabs happen as a result of header files which don't have
	 any symbols in them.  There can be a lot of them.  But this check
	 is wrong, in that a psymtab with N_SLINE entries but nothing else
	 is not empty, but we don't realize that.  Fixing that without slowing
	 things down might be tricky.  */
      struct partial_symtab *prev_pst;

      /* First, snip it out of the psymtab chain */

      if (pst->objfile->psymtabs == pst)
	pst->objfile->psymtabs = pst->next;
      else
	for (prev_pst = pst->objfile->psymtabs; prev_pst; prev_pst = pst->next)
	  if (prev_pst->next == pst)
	    prev_pst->next = pst->next;

      /* Next, put it on a free list for recycling */

      pst->next = pst->objfile->free_psymtabs;
      pst->objfile->free_psymtabs = pst;

      /* Indicate that psymtab was thrown away.  */
      pst = (struct partial_symtab *)NULL;
    }
  return pst;
}

/* Do the dirty work of reading in the full symbol from a partial symbol
   table.  */

static void
hpread_psymtab_to_symtab_1 (pst)
     struct partial_symtab *pst;
{
  struct cleanup *old_chain;
  int i;

  /* Get out quick if passed junk.  */
  if (!pst)
    return;

  /* Complain if we've already read in this symbol table.  */
  if (pst->readin)
    {
      fprintf (stderr, "Psymtab for %s already read in.  Shouldn't happen.\n",
	       pst->filename);
      return;
    }

  /* Read in all partial symtabs on which this one is dependent */
  for (i = 0; i < pst->number_of_dependencies; i++)
    if (!pst->dependencies[i]->readin)
      {
	/* Inform about additional files that need to be read in.  */
	if (info_verbose)
	  {
	    fputs_filtered (" ", stdout);
	    wrap_here ("");
	    fputs_filtered ("and ", stdout);
	    wrap_here ("");
	    printf_filtered ("%s...", pst->dependencies[i]->filename);
	    wrap_here ("");	/* Flush output */
	    fflush (stdout);
	  }
	hpread_psymtab_to_symtab_1 (pst->dependencies[i]);
      }

  /* If it's real...  */
  if (LDSYMLEN (pst))
    {
      /* Init stuff necessary for reading in symbols */
      buildsym_init ();
      old_chain = make_cleanup (really_free_pendings, 0);

      pst->symtab =
	hpread_expand_symtab (pst->objfile, LDSYMOFF (pst), LDSYMLEN (pst),
			      pst->textlow, pst->texthigh - pst->textlow,
			      pst->section_offsets, pst->filename);
      sort_symtab_syms (pst->symtab);

      do_cleanups (old_chain);
    }

  pst->readin = 1;
}

/* Read in all of the symbols for a given psymtab for real.
   Be verbose about it if the user wants that.  */

static void
hpread_psymtab_to_symtab (pst)
     struct partial_symtab *pst;
{
  /* Get out quick if given junk.  */
  if (!pst)
    return;

  /* Sanity check.  */
  if (pst->readin)
    {
      fprintf (stderr, "Psymtab for %s already read in.  Shouldn't happen.\n",
	       pst->filename);
      return;
    }

  if (LDSYMLEN (pst) || pst->number_of_dependencies)
    {
      /* Print the message now, before reading the string table,
         to avoid disconcerting pauses.  */
      if (info_verbose)
	{
	  printf_filtered ("Reading in symbols for %s...", pst->filename);
	  fflush (stdout);
	}

      hpread_psymtab_to_symtab_1 (pst);

      /* Match with global symbols.  This only needs to be done once,
         after all of the symtabs and dependencies have been read in.   */
      scan_file_globals (pst->objfile);

      /* Finish up the debug error message.  */
      if (info_verbose)
	printf_filtered ("done.\n");
    }
}
/* Read in a defined section of a specific object file's symbols.

   DESC is the file descriptor for the file, positioned at the
   beginning of the symtab
   SYM_OFFSET is the offset within the file of
   the beginning of the symbols we want to read
   SYM_SIZE is the size of the symbol info to read in.
   TEXT_OFFSET is the beginning of the text segment we are reading symbols for
   TEXT_SIZE is the size of the text segment read in.
   SECTION_OFFSETS are the relocation offsets which get added to each symbol. */

static struct symtab *
hpread_expand_symtab (objfile, sym_offset, sym_size, text_offset, text_size,
		      section_offsets, filename)
     struct objfile *objfile;
     int sym_offset;
     int sym_size;
     CORE_ADDR text_offset;
     int text_size;
     struct section_offsets *section_offsets;
     char *filename;
{
  char *namestring;
  union dnttentry *dn_bufp;
  unsigned max_symnum;

  int sym_index = sym_offset / sizeof (struct dntt_type_block);

  current_objfile = objfile;
  subfile_stack = 0;

  last_source_file = 0;

  dn_bufp = hpread_get_lntt (sym_index, objfile);
  if (!((dn_bufp->dblock.kind == (unsigned char) DNTT_TYPE_SRCFILE) ||
	(dn_bufp->dblock.kind == (unsigned char) DNTT_TYPE_MODULE)))
    start_symtab ("globals", NULL, 0);

  max_symnum = sym_size / sizeof (struct dntt_type_block);

  /* Read in and process each debug symbol within the specified range.  */
  for (symnum = 0;
       symnum < max_symnum;
       symnum++)
    {
      QUIT;			/* Allow this to be interruptable */
      dn_bufp = hpread_get_lntt (sym_index + symnum, objfile);

      if (dn_bufp->dblock.extension)
	continue;

      /* Yow!  We call SET_NAMESTRING on things without names!  */
      SET_NAMESTRING (dn_bufp, &namestring, objfile);

      hpread_process_one_debug_symbol (dn_bufp, namestring, section_offsets,
				       objfile, text_offset, text_size,
				       filename, symnum + sym_index);
    }

  current_objfile = NULL;

  return end_symtab (text_offset + text_size, objfile, 0);
}


/* Convert basic types from HP debug format into GDB internal format.  */

static int
hpread_type_translate (typep)
     dnttpointer typep;
{
  if (!typep.dntti.immediate)
    abort ();

  switch (typep.dntti.type)
    {
    case HP_TYPE_BOOLEAN:
    case HP_TYPE_BOOLEAN_S300_COMPAT:
    case HP_TYPE_BOOLEAN_VAX_COMPAT:
      return FT_BOOLEAN;
      /* Ugh.  No way to distinguish between signed and unsigned chars.  */
    case HP_TYPE_CHAR:
    case HP_TYPE_WIDE_CHAR:
      return FT_CHAR;
    case HP_TYPE_INT:
      if (typep.dntti.bitlength <= 8)
	return FT_CHAR;
      if (typep.dntti.bitlength <= 16)
	return FT_SHORT;
      if (typep.dntti.bitlength <= 32)
	return FT_INTEGER;
      return FT_LONG_LONG;
    case HP_TYPE_LONG:
      return FT_LONG;
    case HP_TYPE_UNSIGNED_LONG:
      if (typep.dntti.bitlength <= 8)
	return FT_UNSIGNED_CHAR;
      if (typep.dntti.bitlength <= 16)
	return FT_UNSIGNED_SHORT;
      if (typep.dntti.bitlength <= 32)
	return FT_UNSIGNED_LONG;
      return FT_UNSIGNED_LONG_LONG;
    case HP_TYPE_UNSIGNED_INT:
      if (typep.dntti.bitlength <= 8)
	return FT_UNSIGNED_CHAR;
      if (typep.dntti.bitlength <= 16)
	return FT_UNSIGNED_SHORT;
      if (typep.dntti.bitlength <= 32)
	return FT_UNSIGNED_INTEGER;
      return FT_UNSIGNED_LONG_LONG;
    case HP_TYPE_REAL:
    case HP_TYPE_REAL_3000:
    case HP_TYPE_DOUBLE:
      if (typep.dntti.bitlength == 64)
	return FT_DBL_PREC_FLOAT;
      if (typep.dntti.bitlength == 128)
	return FT_EXT_PREC_FLOAT;
      return FT_FLOAT;
    case HP_TYPE_COMPLEX:
    case HP_TYPE_COMPLEXS3000:
      if (typep.dntti.bitlength == 128)
	return FT_DBL_PREC_COMPLEX;
      if (typep.dntti.bitlength == 192)
	return FT_EXT_PREC_COMPLEX;
      return FT_COMPLEX;
    case HP_TYPE_STRING200:
    case HP_TYPE_LONGSTRING200:
    case HP_TYPE_FTN_STRING_SPEC:
    case HP_TYPE_MOD_STRING_SPEC:
    case HP_TYPE_MOD_STRING_3000:
    case HP_TYPE_FTN_STRING_S300_COMPAT:
    case HP_TYPE_FTN_STRING_VAX_COMPAT:
      return FT_STRING;
    default:
      abort ();
    }
}

/* Return the type associated with the index found in HP_TYPE.  */

static struct type **
hpread_lookup_type (hp_type, objfile)
     dnttpointer hp_type;
     struct objfile *objfile;
{
  unsigned old_len;
  int index = hp_type.dnttp.index;

  if (hp_type.dntti.immediate)
    return NULL;

  if (index < LNTT_SYMCOUNT (objfile))
    {
      if (index >= TYPE_VECTOR_LENGTH (objfile))
	{
	  old_len = TYPE_VECTOR_LENGTH (objfile);
	  if (old_len == 0)
	    {
	      TYPE_VECTOR_LENGTH (objfile) = 100;
	      TYPE_VECTOR (objfile) = (struct type **)
		xmalloc (TYPE_VECTOR_LENGTH (objfile) * sizeof (struct type *));
	    }
	  while (index >= TYPE_VECTOR_LENGTH (objfile))
	    TYPE_VECTOR_LENGTH (objfile) *= 2;
	  TYPE_VECTOR (objfile) = (struct type **)
	    xrealloc ((char *) TYPE_VECTOR (objfile),
		      (TYPE_VECTOR_LENGTH (objfile) * sizeof (struct type *)));
	  memset (&TYPE_VECTOR (objfile)[old_len], 0,
		  (TYPE_VECTOR_LENGTH (objfile) - old_len) *
		  sizeof (struct type *));
	}
      return &TYPE_VECTOR (objfile)[index];
    }
  else
    return NULL;
}

/* Possibly allocate a GDB internal type so we can internalize HP_TYPE.
   Note we'll just return the address of a GDB internal type if we already
   have it lying around.  */

static struct type *
hpread_alloc_type (hp_type, objfile)
     dnttpointer hp_type;
     struct objfile *objfile;
{
  struct type **type_addr;

  type_addr = hpread_lookup_type (hp_type, objfile);
  if (*type_addr == 0)
    *type_addr = alloc_type (objfile);

  TYPE_CPLUS_SPECIFIC (*type_addr)
    = (struct cplus_struct_type *) &cplus_struct_default;
  return *type_addr;
}

/* Read a native enumerated type and return it in GDB internal form.  */

static struct type *
hpread_read_enum_type (hp_type, dn_bufp, objfile)
     dnttpointer hp_type;
     union dnttentry *dn_bufp;
     struct objfile *objfile;
{
  struct type *type;
  struct pending **symlist, *osyms, *syms;
  int o_nsyms, nsyms = 0;
  dnttpointer mem;
  union dnttentry *memp;
  char *name;
  long n;
  struct symbol *sym;

  type = hpread_alloc_type (hp_type, objfile);
  TYPE_LENGTH (type) = 4;

  symlist = &file_symbols;
  osyms = *symlist;
  o_nsyms = osyms ? osyms->nsyms : 0;

  /* Get a name for each member and add it to our list of members.  */
  mem = dn_bufp->denum.firstmem;
  while (mem.dnttp.extension && mem.word != DNTTNIL)
    {
      memp = hpread_get_lntt (mem.dnttp.index, objfile);

      name = VT (objfile) + memp->dmember.name;
      sym = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
					     sizeof (struct symbol));
      memset (sym, 0, sizeof (struct symbol));
      SYMBOL_NAME (sym) = name;
      SYMBOL_CLASS (sym) = LOC_CONST;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      SYMBOL_VALUE (sym) = memp->dmember.value;
      add_symbol_to_list (sym, symlist);
      nsyms++;
      mem = memp->dmember.nextmem;
    }

  /* Now that we know more about the enum, fill in more info.  */
  TYPE_CODE (type) = TYPE_CODE_ENUM;
  TYPE_FLAGS (type) &= ~TYPE_FLAG_STUB;
  TYPE_NFIELDS (type) = nsyms;
  TYPE_FIELDS (type) = (struct field *)
    obstack_alloc (&objfile->type_obstack, sizeof (struct field) * nsyms);

  /* Find the symbols for the members and put them into the type.
     The symbols can be found in the symlist that we put them on
     to cause them to be defined.  osyms contains the old value
     of that symlist; everything up to there was defined by us.

     Note that we preserve the order of the enum constants, so
     that in something like "enum {FOO, LAST_THING=FOO}" we print
     FOO, not LAST_THING.  */
  for (syms = *symlist, n = 0; syms; syms = syms->next)
    {
      int j = 0;
      if (syms == osyms)
	j = o_nsyms;
      for (; j < syms->nsyms; j++, n++)
	{
	  struct symbol *xsym = syms->symbol[j];
	  SYMBOL_TYPE (xsym) = type;
	  TYPE_FIELD_NAME (type, n) = SYMBOL_NAME (xsym);
	  TYPE_FIELD_VALUE (type, n) = 0;
	  TYPE_FIELD_BITPOS (type, n) = SYMBOL_VALUE (xsym);
	  TYPE_FIELD_BITSIZE (type, n) = 0;
	}
      if (syms == osyms)
	break;
    }

  return type;
}

/* Read and internalize a native function debug symbol.  */

static struct type *
hpread_read_function_type (hp_type, dn_bufp, objfile)
     dnttpointer hp_type;
     union dnttentry *dn_bufp;
     struct objfile *objfile;
{
  struct type *type, *type1;
  struct pending **symlist, *osyms, *syms;
  int o_nsyms, nsyms = 0;
  dnttpointer param;
  union dnttentry *paramp;
  char *name;
  long n;
  struct symbol *sym;

  param = dn_bufp->dfunc.firstparam;

  /* See if we've already read in this type.  */
  type = hpread_alloc_type (hp_type, objfile);
  if (TYPE_CODE (type) == TYPE_CODE_FUNC)
    return type;

  /* Nope, so read it in and store it away.  */
  type1 = lookup_function_type (hpread_type_lookup (dn_bufp->dfunc.retval,
						    objfile));
  memcpy ((char *) type, (char *) type1, sizeof (struct type));

  symlist = &local_symbols;
  osyms = *symlist;
  o_nsyms = osyms ? osyms->nsyms : 0;

  /* Now examine each parameter noting its type, location, and a
     wealth of other information.  */
  while (param.word && param.word != DNTTNIL)
    {
      paramp = hpread_get_lntt (param.dnttp.index, objfile);
      nsyms++;
      param = paramp->dfparam.nextparam;

      /* Get the name.  */
      name = VT (objfile) + paramp->dfparam.name;
      sym = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
					     sizeof (struct symbol));
      (void) memset (sym, 0, sizeof (struct symbol));
      SYMBOL_NAME (sym) = name;

      /* Figure out where it lives.  */
      if (paramp->dfparam.regparam)
	SYMBOL_CLASS (sym) = LOC_REGPARM;
      else if (paramp->dfparam.indirect)
	SYMBOL_CLASS (sym) = LOC_REF_ARG;
      else
	SYMBOL_CLASS (sym) = LOC_ARG;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      if (paramp->dfparam.copyparam)
	{
	  SYMBOL_VALUE (sym) = paramp->dfparam.location ;
#ifdef HPREAD_ADJUST_STACK_ADDRESS
	  SYMBOL_VALUE (sym)
	    += HPREAD_ADJUST_STACK_ADDRESS (CURRENT_FUNCTION_VALUE (objfile));
#endif
	  /* This is likely a pass-by-invisible reference parameter,
	     Hack on the symbol class to make GDB happy.  */
	  SYMBOL_CLASS (sym) = LOC_REGPARM_ADDR;
	}
      else
	SYMBOL_VALUE (sym) = paramp->dfparam.location;

      /* Get its type.  */
      SYMBOL_TYPE (sym) = hpread_type_lookup (paramp->dfparam.type, objfile);

      /* Add it to the list.  */
      add_symbol_to_list (sym, symlist);
    }

  /* Note how many parameters we found.  */
  TYPE_NFIELDS (type) = nsyms;
  TYPE_FIELDS (type) = (struct field *)
    obstack_alloc (&objfile->type_obstack,
		   sizeof (struct field) * nsyms);

  /* Find the symbols for the values and put them into the type.
     The symbols can be found in the symlist that we put them on
     to cause them to be defined.  osyms contains the old value
     of that symlist; everything up to there was defined by us.  */
  /* Note that we preserve the order of the parameters, so
     that in something like "enum {FOO, LAST_THING=FOO}" we print
     FOO, not LAST_THING.  */
  for (syms = *symlist, n = 0; syms; syms = syms->next)
    {
      int j = 0;
      if (syms == osyms)
	j = o_nsyms;
      for (; j < syms->nsyms; j++, n++)
	{
	  struct symbol *xsym = syms->symbol[j];
	  TYPE_FIELD_NAME (type, n) = SYMBOL_NAME (xsym);
	  TYPE_FIELD_TYPE (type, n) = SYMBOL_TYPE (xsym);
	  TYPE_FIELD_BITPOS (type, n) = n;
	  TYPE_FIELD_BITSIZE (type, n) = 0;
	}
      if (syms == osyms)
	break;
    }
  return type;
}

/* Read in and internalize a structure definition.  */

static struct type *
hpread_read_struct_type (hp_type, dn_bufp, objfile)
     dnttpointer hp_type;
     union dnttentry *dn_bufp;
     struct objfile *objfile;
{
  struct nextfield
    {
      struct nextfield *next;
      struct field field;
    };

  struct type *type;
  struct nextfield *list = 0;
  struct nextfield *new;
  int n, nfields = 0;
  dnttpointer field;
  union dnttentry *fieldp;

  /* Is it something we've already dealt with?  */
  type = hpread_alloc_type (hp_type, objfile);
  if ((TYPE_CODE (type) == TYPE_CODE_STRUCT) ||
      (TYPE_CODE (type) == TYPE_CODE_UNION))
      return type;

  /* Get the basic type correct.  */
  if (dn_bufp->dblock.kind == DNTT_TYPE_STRUCT)
    {
      TYPE_CODE (type) = TYPE_CODE_STRUCT;
      TYPE_LENGTH (type) = dn_bufp->dstruct.bitlength / 8;
    }
  else if (dn_bufp->dblock.kind == DNTT_TYPE_UNION)
    {
      TYPE_CODE (type) = TYPE_CODE_UNION;
      TYPE_LENGTH (type) = dn_bufp->dunion.bitlength / 8;
    }
  else
    return type;


  TYPE_FLAGS (type) &= ~TYPE_FLAG_STUB;

  /* Read in and internalize all the fields.  */
  field = dn_bufp->dstruct.firstfield;
  while (field.word != DNTTNIL && field.dnttp.extension)
    {
      fieldp = hpread_get_lntt (field.dnttp.index, objfile);

      /* Get space to record the next field's data.  */
      new = (struct nextfield *) alloca (sizeof (struct nextfield));
      new->next = list;
      list = new;

      list->field.name = VT (objfile) + fieldp->dfield.name;
      list->field.bitpos = fieldp->dfield.bitoffset;
      if (fieldp->dfield.bitlength % 8)
	list->field.bitsize = fieldp->dfield.bitlength;
      else
	list->field.bitsize = 0;
      nfields++;
      field = fieldp->dfield.nextfield;
      list->field.type = hpread_type_lookup (fieldp->dfield.type, objfile);
    }

  TYPE_NFIELDS (type) = nfields;
  TYPE_FIELDS (type) = (struct field *)
    obstack_alloc (&objfile->type_obstack, sizeof (struct field) * nfields);

  /* Copy the saved-up fields into the field vector.  */
  for (n = nfields; list; list = list->next)
    {
      n -= 1;
      TYPE_FIELD (type, n) = list->field;
    }
  return type;
}

/* Read in and internalize a set debug symbol.  */

static struct type *
hpread_read_set_type (hp_type, dn_bufp, objfile)
     dnttpointer hp_type;
     union dnttentry *dn_bufp;
     struct objfile *objfile;
{
  struct type *type;

  /* See if it's something we've already deal with.  */
  type = hpread_alloc_type (hp_type, objfile);
  if (TYPE_CODE (type) == TYPE_CODE_SET)
    return type;

  /* Nope.  Fill in the appropriate fields.  */
  TYPE_CODE (type) = TYPE_CODE_SET;
  TYPE_LENGTH (type) = dn_bufp->dset.bitlength / 8;
  TYPE_NFIELDS (type) = 0;
  TYPE_TARGET_TYPE (type) = hpread_type_lookup (dn_bufp->dset.subtype,
						objfile);
  return type;
}

/* Read in and internalize an array debug symbol.  */

static struct type *
hpread_read_array_type (hp_type, dn_bufp, objfile)
     dnttpointer hp_type;
     union dnttentry *dn_bufp;
     struct objfile *objfile;
{
  struct type *type;
  union dnttentry save;
  save = *dn_bufp;

  /* Why no check here?  Because it kept us from properly determining
     the size of the array!  */
  type = hpread_alloc_type (hp_type, objfile);

  TYPE_CODE (type) = TYPE_CODE_ARRAY;

  /* values are not normalized.  */
  if (!((dn_bufp->darray.arrayisbytes && dn_bufp->darray.elemisbytes)
	|| (!dn_bufp->darray.arrayisbytes && !dn_bufp->darray.elemisbytes)))
    abort ();
  else if (dn_bufp->darray.arraylength == 0x7fffffff)
    {
      /* The HP debug format represents char foo[]; as an array with
	 length 0x7fffffff.  Internally GDB wants to represent this
	 as an array of length zero.  */
     TYPE_LENGTH (type) = 0;
    }
  else
    TYPE_LENGTH (type) = dn_bufp->darray.arraylength / 8;

  TYPE_NFIELDS (type) = 1;
  TYPE_TARGET_TYPE (type) = hpread_type_lookup (dn_bufp->darray.elemtype,
						objfile);
  dn_bufp = &save;
  TYPE_FIELDS (type) = (struct field *)
    obstack_alloc (&objfile->type_obstack, sizeof (struct field));
  TYPE_FIELD_TYPE (type, 0) = hpread_type_lookup (dn_bufp->darray.indextype,
						  objfile);
  return type;
}

/* Read in and internalize a subrange debug symbol.  */
static struct type *
hpread_read_subrange_type (hp_type, dn_bufp, objfile)
     dnttpointer hp_type;
     union dnttentry *dn_bufp;
     struct objfile *objfile;
{
  struct type *type;

  /* Is it something we've already dealt with.  */
  type = hpread_alloc_type (hp_type, objfile);
  if (TYPE_CODE (type) == TYPE_CODE_RANGE)
    return type;

  /* Nope, internalize it.  */
  TYPE_CODE (type) = TYPE_CODE_RANGE;
  TYPE_LENGTH (type) = dn_bufp->dsubr.bitlength / 8;
  TYPE_NFIELDS (type) = 2;
  TYPE_FIELDS (type)
    = (struct field *) obstack_alloc (&objfile->type_obstack,
				      2 * sizeof (struct field));

  if (dn_bufp->dsubr.dyn_low)
    TYPE_FIELD_BITPOS (type, 0) = 0;
  else
    TYPE_FIELD_BITPOS (type, 0) = dn_bufp->dsubr.lowbound;

  if (dn_bufp->dsubr.dyn_high)
    TYPE_FIELD_BITPOS (type, 1) = -1;
  else
    TYPE_FIELD_BITPOS (type, 1) = dn_bufp->dsubr.highbound;
  TYPE_TARGET_TYPE (type) = hpread_type_lookup (dn_bufp->dsubr.subtype,
						objfile);
  return type;
}

static struct type *
hpread_type_lookup (hp_type, objfile)
     dnttpointer hp_type;
     struct objfile *objfile;
{
  union dnttentry *dn_bufp;

  /* First see if it's a simple builtin type.  */
  if (hp_type.dntti.immediate)
    return lookup_fundamental_type (objfile, hpread_type_translate (hp_type));

  /* Not a builtin type.  We'll have to read it in.  */
  if (hp_type.dnttp.index < LNTT_SYMCOUNT (objfile))
    dn_bufp = hpread_get_lntt (hp_type.dnttp.index, objfile);
  else
    return lookup_fundamental_type (objfile, FT_VOID);

  switch (dn_bufp->dblock.kind)
    {
    case DNTT_TYPE_SRCFILE:
    case DNTT_TYPE_MODULE:
    case DNTT_TYPE_FUNCTION:
    case DNTT_TYPE_ENTRY:
    case DNTT_TYPE_BEGIN:
    case DNTT_TYPE_END:
    case DNTT_TYPE_IMPORT:
    case DNTT_TYPE_LABEL:
    case DNTT_TYPE_WITH:
    case DNTT_TYPE_COMMON:
    case DNTT_TYPE_FPARAM:
    case DNTT_TYPE_SVAR:
    case DNTT_TYPE_DVAR:
    case DNTT_TYPE_CONST:
      /* Opps.  Something went very wrong.  */
      return lookup_fundamental_type (objfile, FT_VOID);

    case DNTT_TYPE_TYPEDEF:
      {
	struct type *structtype = hpread_type_lookup (dn_bufp->dtype.type,
						      objfile);
	char *suffix;
	suffix = VT (objfile) + dn_bufp->dtype.name;

	TYPE_CPLUS_SPECIFIC (structtype)
	  = (struct cplus_struct_type *) &cplus_struct_default;
 	TYPE_NAME (structtype) = suffix;
	return structtype;
      }

    case DNTT_TYPE_TAGDEF:
      {
	/* Just a little different from above.  We have to tack on
	   an identifier of some kind (struct, union, enum, etc).  */
	struct type *structtype = hpread_type_lookup (dn_bufp->dtype.type,
						      objfile);
	char *prefix, *suffix;
	suffix = VT (objfile) + dn_bufp->dtype.name;

	/* Lookup the next type in the list.  It should be a structure,
	   union, or enum type.  We will need to attach that to our name.  */
	if (dn_bufp->dtype.type.dnttp.index < LNTT_SYMCOUNT (objfile))
	  dn_bufp = hpread_get_lntt (dn_bufp->dtype.type.dnttp.index, objfile);
	else
	  abort ();

	if (dn_bufp->dblock.kind == DNTT_TYPE_STRUCT)
	  prefix = "struct ";
	else if (dn_bufp->dblock.kind == DNTT_TYPE_UNION)
	  prefix = "union ";
	else
	  prefix = "enum ";

	/* Build the correct name.  */
	structtype->name
	  = (char *) obstack_alloc (&objfile->type_obstack,
				    strlen (prefix) + strlen (suffix) + 1);
	TYPE_NAME (structtype) = strcpy (TYPE_NAME (structtype), prefix);
	TYPE_NAME (structtype) = strcat (TYPE_NAME (structtype), suffix);
	TYPE_TAG_NAME (structtype) = suffix;

	TYPE_CPLUS_SPECIFIC (structtype)
	  = (struct cplus_struct_type *) &cplus_struct_default;

	return structtype;
      }
    case DNTT_TYPE_POINTER:
      return lookup_pointer_type (hpread_type_lookup (dn_bufp->dptr.pointsto,
						      objfile));
    case DNTT_TYPE_ENUM:
      return hpread_read_enum_type (hp_type, dn_bufp, objfile);
    case DNTT_TYPE_MEMENUM:
      return lookup_fundamental_type (objfile, FT_VOID);
    case DNTT_TYPE_SET:
      return hpread_read_set_type (hp_type, dn_bufp, objfile);
    case DNTT_TYPE_SUBRANGE:
      return hpread_read_subrange_type (hp_type, dn_bufp, objfile);
    case DNTT_TYPE_ARRAY:
      return hpread_read_array_type (hp_type, dn_bufp, objfile);
    case DNTT_TYPE_STRUCT:
    case DNTT_TYPE_UNION:
      return hpread_read_struct_type (hp_type, dn_bufp, objfile);
    case DNTT_TYPE_FIELD:
      return hpread_type_lookup (dn_bufp->dfield.type, objfile);
    case DNTT_TYPE_VARIANT:
    case DNTT_TYPE_FILE:
      return lookup_fundamental_type (objfile, FT_VOID);
    case DNTT_TYPE_FUNCTYPE:
      return lookup_function_type (hpread_type_lookup (dn_bufp->dfunctype.retval,
						       objfile));
    case DNTT_TYPE_COBSTRUCT:
    case DNTT_TYPE_XREF:
    case DNTT_TYPE_SA:
    case DNTT_TYPE_MACRO:
    default:
      return lookup_fundamental_type (objfile, FT_VOID);
    }
}

static sltpointer
hpread_record_lines (subfile, s_idx, e_idx, objfile, offset)
     struct subfile *subfile;
     sltpointer s_idx, e_idx;
     struct objfile *objfile;
     CORE_ADDR offset;
{
  union sltentry *sl_bufp;

  while (s_idx <= e_idx)
    {
      sl_bufp = hpread_get_slt (s_idx, objfile);
      /* Only record "normal" entries in the SLT.  */
      if (sl_bufp->snorm.sltdesc == SLT_NORMAL
	  || sl_bufp->snorm.sltdesc == SLT_EXIT)
	record_line (subfile, sl_bufp->snorm.line,
		     sl_bufp->snorm.address + offset);
      s_idx++;
    }
  return e_idx;
}

/* Internalize one native debug symbol.  */

static void
hpread_process_one_debug_symbol (dn_bufp, name, section_offsets, objfile,
				 text_offset, text_size, filename, index)
     union dnttentry *dn_bufp;
     char *name;
     struct section_offsets *section_offsets;
     struct objfile *objfile;
     CORE_ADDR text_offset;
     int text_size;
     char *filename;
     int index;
{
  unsigned long desc;
  int type;
  CORE_ADDR valu;
  int offset = ANOFFSET (section_offsets, SECT_OFF_TEXT);
  union dnttentry *dn_temp;
  dnttpointer hp_type;
  struct symbol *sym;
  struct context_stack *new;

  /* Allocate one GDB debug symbol and fill in some default values.  */
  sym = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
					 sizeof (struct symbol));
  memset (sym, 0, sizeof (struct symbol));
  SYMBOL_NAME (sym) = name;
  SYMBOL_LANGUAGE (sym) = language_auto;
  SYMBOL_INIT_DEMANGLED_NAME (sym, &objfile->symbol_obstack);
  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
  SYMBOL_LINE (sym) = 0;
  SYMBOL_VALUE (sym) = 0;
  SYMBOL_CLASS (sym) = LOC_TYPEDEF;

  hp_type.dnttp.extension = 1;
  hp_type.dnttp.immediate = 0;
  hp_type.dnttp.global = 0;
  hp_type.dnttp.index = index;

  type = dn_bufp->dblock.kind;

  switch (type)
    {
    case DNTT_TYPE_SRCFILE:
      /* This type of symbol indicates from which source file or include file
         the following data comes. If there are no modules it also may
         indicate the start of a new source file, in which case we must
         finish the symbol table of the previous source file
         (if any) and start accumulating a new symbol table.  */

      valu = text_offset;
      if (!last_source_file)
	{
	  start_symtab (name, NULL, valu);
	  SL_INDEX (objfile) = dn_bufp->dsfile.address;
	}
      else
	{
	  SL_INDEX (objfile) = hpread_record_lines (current_subfile,
						    SL_INDEX (objfile),
						    dn_bufp->dsfile.address,
						    objfile, offset);
	}
      start_subfile (name, NULL);
      break;
      
    case DNTT_TYPE_MODULE:
      /* No need to do anything with these DNTT_TYPE_MODULE symbols anymore.  */
      break;

    case DNTT_TYPE_FUNCTION:
    case DNTT_TYPE_ENTRY:
      /* A function or secondary entry point.  */
      valu = dn_bufp->dfunc.lowaddr + offset;
      SL_INDEX (objfile) = hpread_record_lines (current_subfile,
						SL_INDEX (objfile),
						dn_bufp->dfunc.address,
						objfile, offset);
      
      WITHIN_FUNCTION (objfile) = 1;
      CURRENT_FUNCTION_VALUE (objfile) = valu;

      /* Stack must be empty now.  */
      if (context_stack_depth != 0)
	complain (&lbrac_unmatched_complaint, (char *) symnum);
      new = push_context (0, valu);

      SYMBOL_CLASS (sym) = LOC_BLOCK;
      SYMBOL_TYPE (sym) = hpread_read_function_type (hp_type, dn_bufp, objfile);
      if (dn_bufp->dfunc.global)
	add_symbol_to_list (sym, &global_symbols);
      else
	add_symbol_to_list (sym, &file_symbols);
      new->name = sym;

      /* Search forward to the next scope beginning.  */
      while (dn_bufp->dblock.kind != DNTT_TYPE_BEGIN)
	{
	  dn_bufp = hpread_get_lntt (++index, objfile);
	  if (dn_bufp->dblock.extension)
	    continue;
	}
      SL_INDEX (objfile) = hpread_record_lines (current_subfile,
						SL_INDEX (objfile),
						dn_bufp->dbegin.address,
						objfile, offset);
      SYMBOL_LINE (sym) = hpread_get_line (dn_bufp->dbegin.address, objfile);
      record_line (current_subfile, SYMBOL_LINE (sym), valu);
      break;

    case DNTT_TYPE_BEGIN:
      /* Begin a new scope.  */
      SL_INDEX (objfile) = hpread_record_lines (current_subfile,
						SL_INDEX (objfile),
						dn_bufp->dbegin.address,
						objfile, offset);
      valu = hpread_get_location (dn_bufp->dbegin.address, objfile);
      valu += offset;		/* Relocate for dynamic loading */
      desc = hpread_get_depth (dn_bufp->dbegin.address, objfile);
      new = push_context (desc, valu);
      break;

    case DNTT_TYPE_END:
      /* End a scope.  */
      SL_INDEX (objfile) = hpread_record_lines (current_subfile,
						SL_INDEX (objfile),
						dn_bufp->dend.address + 1,
						objfile, offset);
      switch (dn_bufp->dend.endkind)
	{
	case DNTT_TYPE_MODULE:
	  /* Ending a module ends the symbol table for that module.  */
	  valu = text_offset + text_size + offset;
	  (void) end_symtab (valu, objfile, 0);
	  break;

	case DNTT_TYPE_FUNCTION:
	  /* Ending a function, well, ends the function's scope.  */
	  dn_temp = hpread_get_lntt (dn_bufp->dend.beginscope.dnttp.index,
				     objfile);
	  valu = dn_temp->dfunc.hiaddr + offset;
	  new = pop_context ();
	  /* Make a block for the local symbols within.  */
	  finish_block (new->name, &local_symbols, new->old_blocks,
			new->start_addr, valu, objfile);
	  WITHIN_FUNCTION (objfile) = 0;
	  break;
	case DNTT_TYPE_BEGIN:
	  /* Just ending a local scope.  */
	  valu = hpread_get_location (dn_bufp->dend.address, objfile);
	  /* Why in the hell is this needed?  */
	  valu += offset + 9;	/* Relocate for dynamic loading */
	  new = pop_context ();
	  desc = dn_bufp->dend.beginscope.dnttp.index;
	  if (desc != new->depth)
	    complain (&lbrac_mismatch_complaint, (char *) symnum);
	  /* Make a block for the local symbols within.  */
	  finish_block (new->name, &local_symbols, new->old_blocks,
			new->start_addr, valu, objfile);
	  local_symbols = new->locals;
	  break;
	}
      break;
    case DNTT_TYPE_LABEL:
      SYMBOL_NAMESPACE (sym) = LABEL_NAMESPACE;
      break;
    case DNTT_TYPE_FPARAM:
      /* Function parameters.  */
      if (dn_bufp->dfparam.regparam)
	SYMBOL_CLASS (sym) = LOC_REGISTER;
      else
	SYMBOL_CLASS (sym) = LOC_LOCAL;
      if (dn_bufp->dfparam.copyparam)
	{
	  SYMBOL_VALUE (sym) = dn_bufp->dfparam.location;
#ifdef HPREAD_ADJUST_STACK_ADDRESS
	  SYMBOL_VALUE (sym)
	    += HPREAD_ADJUST_STACK_ADDRESS (CURRENT_FUNCTION_VALUE (objfile));
#endif
	}
      else
	SYMBOL_VALUE (sym) = dn_bufp->dfparam.location;
      SYMBOL_TYPE (sym) = hpread_type_lookup (dn_bufp->dfparam.type, objfile);
      add_symbol_to_list (sym, &local_symbols);
      break;
    case DNTT_TYPE_SVAR:
      /* Static variables.  */
      SYMBOL_CLASS (sym) = LOC_STATIC;
      SYMBOL_VALUE_ADDRESS (sym) = dn_bufp->dsvar.location;
      SYMBOL_TYPE (sym) = hpread_type_lookup (dn_bufp->dsvar.type, objfile);
      if (dn_bufp->dsvar.global)
	add_symbol_to_list (sym, &global_symbols);
      else if (WITHIN_FUNCTION (objfile))
	add_symbol_to_list (sym, &local_symbols);
      else
	add_symbol_to_list (sym, &file_symbols);
      break;
    case DNTT_TYPE_DVAR:
      /* Dynamic variables.  */
      if (dn_bufp->ddvar.regvar)
	SYMBOL_CLASS (sym) = LOC_REGISTER;
      else
	SYMBOL_CLASS (sym) = LOC_LOCAL;
      SYMBOL_VALUE (sym) = dn_bufp->ddvar.location;
#ifdef HPREAD_ADJUST_STACK_ADDRESS
      SYMBOL_VALUE (sym)
	+= HPREAD_ADJUST_STACK_ADDRESS (CURRENT_FUNCTION_VALUE (objfile));
#endif
      SYMBOL_TYPE (sym) = hpread_type_lookup (dn_bufp->ddvar.type, objfile);
      if (dn_bufp->ddvar.global)
	add_symbol_to_list (sym, &global_symbols);
      else if (WITHIN_FUNCTION (objfile))
	add_symbol_to_list (sym, &local_symbols);
      else
	add_symbol_to_list (sym, &file_symbols);
      break;
    case DNTT_TYPE_CONST:
      /* A constant (pascal?).  */
      SYMBOL_CLASS (sym) = LOC_CONST;
      SYMBOL_VALUE (sym) = dn_bufp->dconst.location;
      SYMBOL_TYPE (sym) = hpread_type_lookup (dn_bufp->dconst.type, objfile);
      if (dn_bufp->dconst.global)
	add_symbol_to_list (sym, &global_symbols);
      else if (WITHIN_FUNCTION (objfile))
	add_symbol_to_list (sym, &local_symbols);
      else
	add_symbol_to_list (sym, &file_symbols);
      break;
    case DNTT_TYPE_TYPEDEF:
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
      SYMBOL_TYPE (sym) = hpread_type_lookup (dn_bufp->dtype.type, objfile);
      if (dn_bufp->dtype.global)
	add_symbol_to_list (sym, &global_symbols);
      else if (WITHIN_FUNCTION (objfile))
	add_symbol_to_list (sym, &local_symbols);
      else
	add_symbol_to_list (sym, &file_symbols);
      break;
    case DNTT_TYPE_TAGDEF:
      SYMBOL_NAMESPACE (sym) = STRUCT_NAMESPACE;
      SYMBOL_TYPE (sym) = hpread_type_lookup (dn_bufp->dtype.type, objfile);
      TYPE_NAME (sym->type) = SYMBOL_NAME (sym);
      TYPE_TAG_NAME (sym->type) = SYMBOL_NAME (sym);
      if (dn_bufp->dtype.global)
	add_symbol_to_list (sym, &global_symbols);
      else if (WITHIN_FUNCTION (objfile))
	add_symbol_to_list (sym, &local_symbols);
      else
	add_symbol_to_list (sym, &file_symbols);
      break;
    case DNTT_TYPE_POINTER:
      SYMBOL_TYPE (sym) = lookup_pointer_type (hpread_type_lookup
					       (dn_bufp->dptr.pointsto,
						objfile));
      add_symbol_to_list (sym, &file_symbols);
      break;
    case DNTT_TYPE_ENUM:
      SYMBOL_NAMESPACE (sym) = STRUCT_NAMESPACE;
      SYMBOL_TYPE (sym) = hpread_read_enum_type (hp_type, dn_bufp, objfile);
      add_symbol_to_list (sym, &file_symbols);
      break;
    case DNTT_TYPE_MEMENUM:
      break;
    case DNTT_TYPE_SET:
      SYMBOL_TYPE (sym) = hpread_read_set_type (hp_type, dn_bufp, objfile);
      add_symbol_to_list (sym, &file_symbols);
      break;
    case DNTT_TYPE_SUBRANGE:
      SYMBOL_TYPE (sym) = hpread_read_subrange_type (hp_type, dn_bufp,
						     objfile);
      add_symbol_to_list (sym, &file_symbols);
      break;
    case DNTT_TYPE_ARRAY:
      SYMBOL_TYPE (sym) = hpread_read_array_type (hp_type, dn_bufp, objfile);
      add_symbol_to_list (sym, &file_symbols);
      break;
    case DNTT_TYPE_STRUCT:
    case DNTT_TYPE_UNION:
      SYMBOL_NAMESPACE (sym) = STRUCT_NAMESPACE;
      SYMBOL_TYPE (sym) = hpread_read_struct_type (hp_type, dn_bufp, objfile);
      add_symbol_to_list (sym, &file_symbols);
      break;
    default:
      break;
    }
}
