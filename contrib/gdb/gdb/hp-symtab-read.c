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

/* Common include for hp-symtab-read.c and hp-psymtab-read.c.
 * Note this has nested includes for a bunch of stuff.
 */
#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "hpread.h"
#include "demangle.h"
#include "complaints.h"




static struct complaint  hpread_unhandled_end_common_complaint =
{
  "unhandled symbol in hp-symtab-read.c: DNTT_TYPE_COMMON/DNTT_TYPE_END.\n", 0, 0
};

static struct complaint  hpread_unhandled_type_complaint =
{
  "hpread_type_translate: unhandled type code.", 0, 0
};

static struct complaint  hpread_struct_complaint =
{
  "hpread_read_struct_type: expected SVAR type...", 0, 0
};

static struct complaint hpread_array_complaint =
{
  "error in hpread_array_type.", 0, 0
};

static struct complaint  hpread_type_lookup_complaint =
{
  "error in hpread_type_lookup().", 0, 0
};


static struct complaint  hpread_unexpected_end_complaint =
{
  "internal error in hp-symtab-read.c: Unexpected DNTT_TYPE_END kind.", 0, 0
};

static struct complaint hpread_tagdef_complaint  =
{
  "error processing class tagdef", 0, 0
};

static struct complaint  hpread_unhandled_common_complaint =
{
  "unhandled symbol in hp-symtab-read.c: DNTT_TYPE_COMMON.", 0, 0
};

static struct complaint  hpread_unhandled_blockdata_complaint =
{
  "unhandled symbol in hp-symtab-read.c: DNTT_TYPE_BLOCKDATA.", 0, 0
};


/* Forward procedure declarations */

static unsigned long hpread_get_scope_start
  PARAMS ((sltpointer, struct objfile *));

static unsigned long hpread_get_line
  PARAMS ((sltpointer, struct objfile *));

static CORE_ADDR hpread_get_location
  PARAMS ((sltpointer, struct objfile *));

static void hpread_psymtab_to_symtab_1 
  PARAMS ((struct partial_symtab *));

void hpread_psymtab_to_symtab 
  PARAMS ((struct partial_symtab *));

static struct symtab *hpread_expand_symtab
  PARAMS ((struct objfile *, int, int, CORE_ADDR, int,
	   struct section_offsets *, char *));

static int hpread_type_translate 
  PARAMS ((dnttpointer));

static struct type **hpread_lookup_type
  PARAMS ((dnttpointer, struct objfile *));

static struct type *hpread_alloc_type
  PARAMS ((dnttpointer, struct objfile *));

static struct type *hpread_read_enum_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *));

static struct type *hpread_read_function_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *, int));

static struct type *hpread_read_doc_function_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *, int));

static struct type *hpread_read_struct_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *));

static struct type *hpread_get_nth_template_arg
  PARAMS ((struct objfile *, int));

static struct type * hpread_read_templ_arg_type 
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *, char *));

static struct type *hpread_read_set_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *));

static struct type * hpread_read_array_type 
  PARAMS ((dnttpointer, union dnttentry *dn_bufp, struct objfile *objfile));

static struct type *hpread_read_subrange_type
  PARAMS ((dnttpointer, union dnttentry *, struct objfile *));

static struct type * hpread_type_lookup
  PARAMS ((dnttpointer, struct objfile *));

static sltpointer hpread_record_lines
  PARAMS ((struct subfile *, sltpointer, sltpointer,
	   struct objfile *, CORE_ADDR));

static void hpread_process_one_debug_symbol
  PARAMS ((union dnttentry *, char *, struct section_offsets *,
	   struct objfile *, CORE_ADDR, int, char *, int, int * ));

static int hpread_get_scope_depth
  PARAMS ((union dnttentry *, struct objfile *, int));

static void fix_static_member_physnames 
  PARAMS ((struct type *, char *, struct objfile *));

static void fixup_class_method_type
  PARAMS ((struct type *, struct type *, struct objfile *));

static void hpread_adjust_bitoffsets PARAMS ((struct type *, int));

static dnttpointer hpread_get_next_skip_over_anon_unions
  PARAMS ((int, dnttpointer, union dnttentry **, struct objfile *));

/* Global to indicate presence of HP-compiled objects,
   in particular, SOM executable file with SOM debug info 
   Defined in symtab.c, used in hppa-tdep.c. */ 
extern int hp_som_som_object_present;

/* Static used to indicate a class type that requires a
   fix-up of one of its method types */
static struct type * fixup_class = NULL;

/* Static used to indicate the method type that is to be
   used to fix-up the type for fixup_class */
static struct type * fixup_method = NULL;



/* Get the nesting depth for the source line identified by INDEX.  */

static unsigned long
hpread_get_scope_start (index, objfile)
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

/* Find the code address associated with a given sltpointer */

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
                   (sl_bufp->snorm.sltdesc != SLT_NORMAL_OFFSET) &&
		   (sl_bufp->snorm.sltdesc != SLT_EXIT)); i++)
	sl_bufp = hpread_get_slt (index - i, objfile);
      if (sl_bufp->snorm.sltdesc == SLT_NORMAL_OFFSET)
        return sl_bufp->snormoff.address;
      else
        return sl_bufp->snorm.address;
    }

  /* find next normal sltentry and get address */
  for (i = 0; ((sl_bufp->snorm.sltdesc != SLT_NORMAL) &&
	       (sl_bufp->snorm.sltdesc != SLT_NORMAL_OFFSET) &&
               (sl_bufp->snorm.sltdesc != SLT_EXIT)); i++)
    sl_bufp = hpread_get_slt (index + i, objfile);
      if (sl_bufp->snorm.sltdesc == SLT_NORMAL_OFFSET)
        return sl_bufp->snormoff.address;
      else
        return sl_bufp->snorm.address;
}


/* Return 1 if an HP debug symbol of type KIND has a name associated with
 * it, else return 0. (This function is not currently used, but I'll
 * leave it here in case it proves useful later on. - RT).
 */

int
hpread_has_name (kind)
     enum dntt_entry_type kind;
{
  switch (kind)
    {
    case DNTT_TYPE_SRCFILE:
    case DNTT_TYPE_MODULE:
    case DNTT_TYPE_FUNCTION:
    case DNTT_TYPE_DOC_FUNCTION:
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
    case DNTT_TYPE_BLOCKDATA:
    case DNTT_TYPE_MEMFUNC:
    case DNTT_TYPE_DOC_MEMFUNC:
      return 1;

    case DNTT_TYPE_BEGIN:
    case DNTT_TYPE_END:
    case DNTT_TYPE_POINTER:
    case DNTT_TYPE_ENUM:
    case DNTT_TYPE_SET:
    case DNTT_TYPE_ARRAY:
    case DNTT_TYPE_STRUCT:
    case DNTT_TYPE_UNION:
    case DNTT_TYPE_VARIANT:
    case DNTT_TYPE_FILE:
    case DNTT_TYPE_FUNCTYPE:
    case DNTT_TYPE_SUBRANGE:
    case DNTT_TYPE_WITH:
    case DNTT_TYPE_COMMON:
    case DNTT_TYPE_COBSTRUCT:
    case DNTT_TYPE_XREF:
    case DNTT_TYPE_MACRO:
    case DNTT_TYPE_CLASS_SCOPE:
    case DNTT_TYPE_REFERENCE:
    case DNTT_TYPE_PTRMEM:
    case DNTT_TYPE_PTRMEMFUNC:
    case DNTT_TYPE_CLASS:
    case DNTT_TYPE_GENFIELD:
    case DNTT_TYPE_VFUNC:
    case DNTT_TYPE_MEMACCESS:
    case DNTT_TYPE_INHERITANCE:
    case DNTT_TYPE_FRIEND_CLASS:
    case DNTT_TYPE_FRIEND_FUNC:
    case DNTT_TYPE_MODIFIER: 
    case DNTT_TYPE_OBJECT_ID:
    case DNTT_TYPE_TEMPLATE:
    case DNTT_TYPE_TEMPLATE_ARG:
    case DNTT_TYPE_FUNC_TEMPLATE:
    case DNTT_TYPE_LINK:
    /* DNTT_TYPE_DYN_ARRAY_DESC ? */
    /* DNTT_TYPE_DESC_SUBRANGE ? */
    /* DNTT_TYPE_BEGIN_EXT ? */
    /* DNTT_TYPE_INLN ? */
    /* DNTT_TYPE_INLN_LIST ? */
    /* DNTT_TYPE_ALIAS ? */
    default:
      return 0;
    }
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
	    fputs_filtered (" ", gdb_stdout);
	    wrap_here ("");
	    fputs_filtered ("and ", gdb_stdout);
	    wrap_here ("");
	    printf_filtered ("%s...", pst->dependencies[i]->filename);
	    wrap_here ("");	/* Flush output */
	    gdb_flush (gdb_stdout);
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

void
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

  /* elz: setting the flag to indicate that the code of the target
     was compiled using an HP compiler (aCC, cc) 
     the processing_acc_compilation variable is declared in the 
     file buildsym.h, the HP_COMPILED_TARGET is defined to be equal
     to 3 in the file tm_hppa.h*/

  processing_gcc_compilation = 0;

  if (LDSYMLEN (pst) || pst->number_of_dependencies)
    {
      /* Print the message now, before reading the string table,
         to avoid disconcerting pauses.  */
      if (info_verbose)
	{
	  printf_filtered ("Reading in symbols for %s...", pst->filename);
	  gdb_flush (gdb_stdout);
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
  char            *namestring;
  union dnttentry *dn_bufp;
  unsigned         max_symnum;
  int              at_module_boundary = 0;
                   /* 1 => at end, -1 => at beginning */ 

  int sym_index = sym_offset / sizeof (struct dntt_type_block);

  current_objfile = objfile;
  subfile_stack = 0;

  last_source_file = 0;

  /* Demangling style -- if EDG style already set, don't change it,
     as HP style causes some problems with the KAI EDG compiler */ 
  if (current_demangling_style != edg_demangling) {
    /* Otherwise, ensure that we are using HP style demangling */ 
    set_demangling_style (HP_DEMANGLING_STYLE_STRING);
  }

  dn_bufp = hpread_get_lntt (sym_index, objfile);
  if (!((dn_bufp->dblock.kind == (unsigned char) DNTT_TYPE_SRCFILE) ||
	(dn_bufp->dblock.kind == (unsigned char) DNTT_TYPE_MODULE)))
    {
      start_symtab ("globals", NULL, 0);
      record_debugformat ("HP");
    }

  /* The psymtab builder (hp-psymtab-read.c) is the one that
   * determined the "sym_size" argument (i.e. how many DNTT symbols
   * are in this symtab), which we use to compute "max_symnum"
   * (point in DNTT to which we read). 
   *
   * Perhaps this should be changed so that 
   * process_one_debug_symbol() "knows" when
   * to stop reading (based on reading from the MODULE to the matching
   * END), and take out this reliance on a #-syms being passed in...
   * (I'm worried about the reliability of this number). But I'll
   * leave it as-is, for now. - RT
   *
   * The change above has been made. I've left the "for" loop control
   * in to prepare for backing this out again. -JB
   */
  max_symnum = sym_size / sizeof (struct dntt_type_block);
           /* No reason to multiply on pst side and divide on sym side... FIXME */

  /* Read in and process each debug symbol within the specified range.
   */
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
				       filename, symnum + sym_index,
                                      &at_module_boundary
                                       );
                                       
      /* OLD COMMENTS: This routine is only called for psts.  All psts
       * correspond to MODULES.  If we ever do lazy-reading of globals
       * from the LNTT, then there will be a pst which ends when the
       * LNTT ends, and not at an END MODULE entry.  Then we'll have
       * to re-visit this break.  

       if( at_end_of_module )
           break;

       */

      /* We no longer break out of the loop when we reach the end of a
         module. The reason is that with CTTI, the compiler can generate
         function symbols (for template function instantiations) which are not
         in any module; typically they show up beyond a module's end, and
         before the next module's start.  We include them in the current
         module.  However, we still don't trust the MAX_SYMNUM value from
         the psymtab, so we break out if we enter a new module. */

      if (at_module_boundary == -1)
        break;
    }

  current_objfile = NULL;
  hp_som_som_object_present = 1; /* Indicate we've processed an HP SOM SOM file */
  
  return end_symtab (text_offset + text_size, objfile, 0);
}




/* Convert basic types from HP debug format into GDB internal format.  */

static int
hpread_type_translate (typep)
     dnttpointer typep;
{
  if (!typep.dntti.immediate) {
    error ("error in hpread_type_translate\n.");
    return;
  }

  switch (typep.dntti.type)
    {
    case HP_TYPE_BOOLEAN:
    case HP_TYPE_BOOLEAN_S300_COMPAT:
    case HP_TYPE_BOOLEAN_VAX_COMPAT:
      return FT_BOOLEAN;
    case HP_TYPE_CHAR:                /* C signed char, C++ plain char */
 
    case HP_TYPE_WIDE_CHAR:
      return FT_CHAR;
    case HP_TYPE_INT:
      if (typep.dntti.bitlength <= 8)
	return FT_SIGNED_CHAR;         /* C++ signed char */
      if (typep.dntti.bitlength <= 16)
	return FT_SHORT;
      if (typep.dntti.bitlength <= 32)
	return FT_INTEGER;
      return FT_LONG_LONG;
    case HP_TYPE_LONG:
      if (typep.dntti.bitlength <= 8)
	return FT_SIGNED_CHAR;           /* C++ signed char. */ 
      return FT_LONG;
    case HP_TYPE_UNSIGNED_LONG:
      if (typep.dntti.bitlength <= 8)
	return FT_UNSIGNED_CHAR;        /* C/C++ unsigned char */
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
    case HP_TYPE_VOID:
      return FT_VOID;
    case HP_TYPE_STRING200:
    case HP_TYPE_LONGSTRING200:
    case HP_TYPE_FTN_STRING_SPEC:
    case HP_TYPE_MOD_STRING_SPEC:
    case HP_TYPE_MOD_STRING_3000:
    case HP_TYPE_FTN_STRING_S300_COMPAT:
    case HP_TYPE_FTN_STRING_VAX_COMPAT:
      return FT_STRING;
    case HP_TYPE_TEMPLATE_ARG:
      return FT_TEMPLATE_ARG;
    case HP_TYPE_TEXT:
    case HP_TYPE_FLABEL:
    case HP_TYPE_PACKED_DECIMAL:
    case HP_TYPE_ANYPOINTER:  
    case HP_TYPE_GLOBAL_ANYPOINTER:
    case HP_TYPE_LOCAL_ANYPOINTER: 
    default:
      warning ("hpread_type_translate: unhandled type code.\n");
      return FT_VOID;
    }
}

/* Given a position in the DNTT, return a pointer to the 
 * already-built "struct type" (if any), for the type defined 
 * at that position.
 */

static struct type **
hpread_lookup_type (hp_type, objfile)
     dnttpointer hp_type;
     struct objfile *objfile;
{
  unsigned old_len;
  int index = hp_type.dnttp.index;
  int size_changed = 0;

  /* The immediate flag indicates this doesn't actually point to
   * a type DNTT.
   */
  if (hp_type.dntti.immediate)
    return NULL;

  /* For each objfile, we maintain a "type vector".
   * This an array of "struct type *"'s with one pointer per DNTT index.
   * Given a DNTT index, we look in this array to see if we have
   * already processed this DNTT and if it is a type definition.
   * If so, then we can locate a pointer to the already-built
   * "struct type", and not build it again.
   * 
   * The need for this arises because our DNTT-walking code wanders
   * around. In particular, it will encounter the same type multiple
   * times (once for each object of that type). We don't want to 
   * built multiple "struct type"'s for the same thing.
   *
   * Having said this, I should point out that this type-vector is
   * an expensive way to keep track of this. If most DNTT entries are 
   * 3 words, the type-vector will be 1/3 the size of the DNTT itself.
   * Alternative solutions:
   * - Keep a compressed or hashed table. Less memory, but more expensive
   *   to search and update.
   * - (Suggested by JB): Overwrite the DNTT entry itself
   *   with the info. Create a new type code "ALREADY_BUILT", and modify
   *   the DNTT to have that type code and point to the already-built entry.
   * -RT
   */

  if (index < LNTT_SYMCOUNT (objfile))
    {
      if (index >= TYPE_VECTOR_LENGTH (objfile))
	{
	  old_len = TYPE_VECTOR_LENGTH (objfile);

          /* See if we need to allocate a type-vector. */
	  if (old_len == 0)
	    {
              TYPE_VECTOR_LENGTH(objfile) = LNTT_SYMCOUNT (objfile) + GNTT_SYMCOUNT (objfile);
	      TYPE_VECTOR (objfile) = (struct type **)
		xmmalloc (objfile->md, TYPE_VECTOR_LENGTH (objfile) * sizeof (struct type *));
	      memset (&TYPE_VECTOR (objfile)[old_len], 0,
		        (TYPE_VECTOR_LENGTH (objfile) - old_len) *
		        sizeof (struct type *));
	    }

          /* See if we need to resize type-vector. With my change to
           * initially allocate a correct-size type-vector, this code
           * should no longer trigger.
           */
	  while (index >= TYPE_VECTOR_LENGTH (objfile)) {
	    TYPE_VECTOR_LENGTH (objfile) *= 2;
            size_changed = 1;
          }
          if (size_changed) {
	    TYPE_VECTOR (objfile) = (struct type **)
	      xmrealloc (objfile -> md,
                         (char *) TYPE_VECTOR (objfile),
                         (TYPE_VECTOR_LENGTH (objfile) * sizeof (struct type *)));

	      memset (&TYPE_VECTOR (objfile)[old_len], 0,
		        (TYPE_VECTOR_LENGTH (objfile) - old_len) *
		        sizeof (struct type *));
          } 

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
  if (*type_addr == 0) {
    *type_addr = alloc_type (objfile);

    /* A hack - if we really are a C++ class symbol, then this default
     * will get overriden later on.
     */
    TYPE_CPLUS_SPECIFIC (*type_addr)
      = (struct cplus_struct_type *) &cplus_struct_default;
  }

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
  struct pending *local_list = NULL;
  int o_nsyms, nsyms = 0;
  dnttpointer mem;
  union dnttentry *memp;
  char *name;
  long n;
  struct symbol *sym;

  /* Allocate a GDB type. If we've already read in this enum type,
   * it'll return the already built GDB type, so stop here.
   * (Note: I added this check, to conform with what's done for 
   *  struct, union, class.
   *  I assume this is OK. - RT)
   */
  type = hpread_alloc_type (hp_type, objfile);
  if (TYPE_CODE (type) == TYPE_CODE_ENUM)
    return type;

  /* HP C supports "sized enums", where a specifier such as "short" or
     "char" can be used to get enums of different sizes. So don't assume
     an enum is always 4 bytes long. pai/1997-08-21 */
  TYPE_LENGTH (type) = dn_bufp->denum.bitlength / 8;

  symlist = &file_symbols;
  osyms = *symlist;
  o_nsyms = osyms ? osyms->nsyms : 0;

  /* Get a name for each member and add it to our list of members.  
   * The list of "mem" SOM records we are walking should all be
   * SOM type DNTT_TYPE_MEMENUM (not checked).
   */
  mem = dn_bufp->denum.firstmem;
  while (mem.word && mem.word != DNTTNIL)
    {
      memp = hpread_get_lntt (mem.dnttp.index, objfile);

      name = VT (objfile) + memp->dmember.name;
      sym = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
					     sizeof (struct symbol));
      memset (sym, 0, sizeof (struct symbol));
      SYMBOL_NAME (sym) = obsavestring (name, strlen (name), 
					&objfile->symbol_obstack);
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
hpread_read_function_type (hp_type, dn_bufp, objfile, newblock)
     dnttpointer hp_type;
     union dnttentry *dn_bufp;
     struct objfile *objfile;
     int newblock;
{
  struct type *type, *type1;
  struct pending *syms;
  struct pending *local_list = NULL;
  int nsyms = 0;
  dnttpointer param;
  union dnttentry *paramp;
  char *name;
  long n;
  struct symbol *sym;
  int record_args = 1;

  /* See if we've already read in this type.  */
  type = hpread_alloc_type (hp_type, objfile);
  if (TYPE_CODE (type) == TYPE_CODE_FUNC)
    {
      record_args = 0; /* already read in, don't modify type */ 
    }
  else
    {
      /* Nope, so read it in and store it away.  */
      if (dn_bufp->dblock.kind == DNTT_TYPE_FUNCTION ||
          dn_bufp->dblock.kind == DNTT_TYPE_MEMFUNC)
        type1 = lookup_function_type (hpread_type_lookup (dn_bufp->dfunc.retval,
                                                          objfile));
      else if (dn_bufp->dblock.kind == DNTT_TYPE_FUNCTYPE)
        type1 = lookup_function_type (hpread_type_lookup (dn_bufp->dfunctype.retval,
                                                          objfile));
      else /* expect DNTT_TYPE_FUNC_TEMPLATE */
        type1 = lookup_function_type (hpread_type_lookup (dn_bufp->dfunc_template.retval,
                                                          objfile));
      memcpy ((char *) type, (char *) type1, sizeof (struct type));
      
      /* Mark it -- in the middle of processing */ 
      TYPE_FLAGS (type) |= TYPE_FLAG_INCOMPLETE;
    }

  /* Now examine each parameter noting its type, location, and a
     wealth of other information.  */
  if (dn_bufp->dblock.kind == DNTT_TYPE_FUNCTION ||
      dn_bufp->dblock.kind == DNTT_TYPE_MEMFUNC)
    param = dn_bufp->dfunc.firstparam;
  else if (dn_bufp->dblock.kind == DNTT_TYPE_FUNCTYPE)
    param = dn_bufp->dfunctype.firstparam;
  else /* expect DNTT_TYPE_FUNC_TEMPLATE */
    param = dn_bufp->dfunc_template.firstparam;
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
      SYMBOL_NAME (sym) = obsavestring (name, strlen (name),
					&objfile->symbol_obstack);

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
          /* ??rehrauer: This appears to be broken w/r/t to passing
             C values of type float and struct.  Perhaps this ought
             to be highighted as a special case, but for now, just
             allowing these to be LOC_ARGs seems to work fine.
             */
#if 0
	  SYMBOL_CLASS (sym) = LOC_REGPARM_ADDR;
#endif
	}
      else
	SYMBOL_VALUE (sym) = paramp->dfparam.location;

      /* Get its type.  */
      SYMBOL_TYPE (sym) = hpread_type_lookup (paramp->dfparam.type, objfile);
      /* Add it to the symbol list.  */
      /* Note 1 (RT) At the moment, add_symbol_to_list() is also being
       * called on FPARAM symbols from the process_one_debug_symbol()
       * level... so parameters are getting added twice! (this shows
       * up in the symbol dump you get from "maint print symbols ...").
       * Note 2 (RT) I took out the processing of FPARAM from the 
       * process_one_debug_symbol() level, so at the moment parameters are only
       * being processed here. This seems to have no ill effect.
       */
      /* Note 3 (pai/1997-08-11) I removed the add_symbol_to_list() which put
         each fparam on the local_symbols list from here.  Now we use the
         local_list to which fparams are added below, and set the param_symbols
         global to point to that at the end of this routine. */ 
      /* elz: I added this new list of symbols which is local to the function.
         this list is the one which is actually used to build the type for the
         function rather than the gloabal list pointed to by symlist.
         Using a global list to keep track of the parameters is wrong, because 
         this function is called recursively if one parameter happend to be
         a function itself with more parameters in it. Adding parameters to the
         same global symbol list would not work!      
         Actually it did work in case of cc compiled programs where you do 
         not check the parameter lists of the arguments. */ 
      add_symbol_to_list (sym, &local_list);

    }

  /* If type was read in earlier, don't bother with modifying
     the type struct */ 
  if (!record_args)
    goto finish;
  
  /* Note how many parameters we found.  */
  TYPE_NFIELDS (type) = nsyms;
  TYPE_FIELDS (type) = (struct field *)
    obstack_alloc (&objfile->type_obstack,
		   sizeof (struct field) * nsyms);

  /* Find the symbols for the parameters and 
     use them to fill parameter-type information into the function-type.
     The parameter symbols can be found in the local_list that we just put them on. */
  /* Note that we preserve the order of the parameters, so
     that in something like "enum {FOO, LAST_THING=FOO}" we print
     FOO, not LAST_THING.  */
  
  /* get the parameters types from the local list not the global list
     so that the type can be correctly constructed for functions which
     have function as parameters */
  for (syms = local_list, n = 0; syms; syms = syms->next)
    {
      int j = 0;
      for (j=0; j < syms->nsyms; j++, n++)
	{
	  struct symbol *xsym = syms->symbol[j];
	  TYPE_FIELD_NAME (type, n) = SYMBOL_NAME (xsym);
	  TYPE_FIELD_TYPE (type, n) = SYMBOL_TYPE (xsym);
	  TYPE_FIELD_BITPOS (type, n) = n;
	  TYPE_FIELD_BITSIZE (type, n) = 0;
	}
    }
  /* Mark it as having been processed */ 
  TYPE_FLAGS (type) &= ~(TYPE_FLAG_INCOMPLETE);

  /* Check whether we need to fix-up a class type with this function's type */
  if (fixup_class && (fixup_method == type))
    {
      fixup_class_method_type (fixup_class, fixup_method, objfile);
      fixup_class = NULL;
      fixup_method = NULL;
    }

  /* Set the param list of this level of the context stack
     to our local list.  Do this only if this function was
     called for creating a new block, and not if it was called
     simply to get the function type. This prevents recursive
     invocations from trashing param_symbols. */
finish:  
  if (newblock)
    param_symbols = local_list;
  
   return type;
}


/* Read and internalize a native DOC function debug symbol.  */
/* This is almost identical to hpread_read_function_type(), except
 * for references to dn_bufp->ddocfunc instead of db_bufp->dfunc.
 * Since debug information for DOC functions is more likely to be
 * volatile, please leave it this way.
 */
static struct type *
hpread_read_doc_function_type (hp_type, dn_bufp, objfile, newblock)
     dnttpointer hp_type;
     union dnttentry *dn_bufp;
     struct objfile *objfile;
     int newblock;
{
  struct type *type, *type1;
  struct pending *syms;
  struct pending *local_list = NULL;
  int nsyms = 0;
  dnttpointer param;
  union dnttentry *paramp;
  char *name;
  long n;
  struct symbol *sym;
  int record_args = 1;

  /* See if we've already read in this type.  */
  type = hpread_alloc_type (hp_type, objfile);
  if (TYPE_CODE (type) == TYPE_CODE_FUNC)
    {
      record_args = 0; /* already read in, don't modify type */ 
    }
  else
    {
      /* Nope, so read it in and store it away.  */
      if (dn_bufp->dblock.kind == DNTT_TYPE_DOC_FUNCTION ||
          dn_bufp->dblock.kind == DNTT_TYPE_DOC_MEMFUNC)
        type1 = lookup_function_type (hpread_type_lookup (dn_bufp->ddocfunc.retval,
                                                          objfile));
      memcpy ((char *) type, (char *) type1, sizeof (struct type));
      
      /* Mark it -- in the middle of processing */ 
      TYPE_FLAGS (type) |= TYPE_FLAG_INCOMPLETE;
    }

  /* Now examine each parameter noting its type, location, and a
     wealth of other information.  */
  if (dn_bufp->dblock.kind == DNTT_TYPE_DOC_FUNCTION ||
      dn_bufp->dblock.kind == DNTT_TYPE_DOC_MEMFUNC)
    param = dn_bufp->ddocfunc.firstparam;
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
          /* ??rehrauer: This appears to be broken w/r/t to passing
             C values of type float and struct.  Perhaps this ought
             to be highighted as a special case, but for now, just
             allowing these to be LOC_ARGs seems to work fine.
             */
#if 0
	  SYMBOL_CLASS (sym) = LOC_REGPARM_ADDR;
#endif
	}
      else
	SYMBOL_VALUE (sym) = paramp->dfparam.location;

      /* Get its type.  */
      SYMBOL_TYPE (sym) = hpread_type_lookup (paramp->dfparam.type, objfile);
      /* Add it to the symbol list.  */
      /* Note 1 (RT) At the moment, add_symbol_to_list() is also being
       * called on FPARAM symbols from the process_one_debug_symbol()
       * level... so parameters are getting added twice! (this shows
       * up in the symbol dump you get from "maint print symbols ...").
       * Note 2 (RT) I took out the processing of FPARAM from the 
       * process_one_debug_symbol() level, so at the moment parameters are only
       * being processed here. This seems to have no ill effect.
       */
      /* Note 3 (pai/1997-08-11) I removed the add_symbol_to_list() which put
         each fparam on the local_symbols list from here.  Now we use the
         local_list to which fparams are added below, and set the param_symbols
         global to point to that at the end of this routine. */ 
         
      /* elz: I added this new list of symbols which is local to the function.
         this list is the one which is actually used to build the type for the
         function rather than the gloabal list pointed to by symlist.
         Using a global list to keep track of the parameters is wrong, because 
         this function is called recursively if one parameter happend to be
         a function itself with more parameters in it. Adding parameters to the
         same global symbol list would not work!      
         Actually it did work in case of cc compiled programs where you do not check the
         parameter lists of the arguments.  */
      add_symbol_to_list (sym, &local_list);
    }

  /* If type was read in earlier, don't bother with modifying
     the type struct */ 
  if (!record_args)
    goto finish;
  
  /* Note how many parameters we found.  */
  TYPE_NFIELDS (type) = nsyms;
  TYPE_FIELDS (type) = (struct field *)
    obstack_alloc (&objfile->type_obstack,
                   sizeof (struct field) * nsyms);

  /* Find the symbols for the parameters and 
     use them to fill parameter-type information into the function-type.
     The parameter symbols can be found in the local_list that we just put them on. */
  /* Note that we preserve the order of the parameters, so
     that in something like "enum {FOO, LAST_THING=FOO}" we print
     FOO, not LAST_THING.  */
  
  /* get the parameters types from the local list not the global list
     so that the type can be correctly constructed for functions which
     have function as parameters
  */
  for (syms = local_list, n = 0; syms; syms = syms->next)
    {
      int j = 0;
      for (j = 0; j < syms->nsyms; j++, n++)
        {
          struct symbol *xsym = syms->symbol[j];
          TYPE_FIELD_NAME (type, n) = SYMBOL_NAME (xsym);
          TYPE_FIELD_TYPE (type, n) = SYMBOL_TYPE (xsym);
          TYPE_FIELD_BITPOS (type, n) = n;
          TYPE_FIELD_BITSIZE (type, n) = 0;
        }
    }
  
  /* Mark it as having been processed */ 
  TYPE_FLAGS (type) &= ~(TYPE_FLAG_INCOMPLETE);

  /* Check whether we need to fix-up a class type with this function's type */
  if (fixup_class && (fixup_method == type))
    {
      fixup_class_method_type (fixup_class, fixup_method, objfile);
      fixup_class = NULL;
      fixup_method = NULL;
    }

  /* Set the param list of this level of the context stack
     to our local list.  Do this only if this function was
     called for creating a new block, and not if it was called
     simply to get the function type. This prevents recursive
     invocations from trashing param_symbols. */
finish:  
  if (newblock)
    param_symbols = local_list;
  
  return type;
}



/* A file-level variable which keeps track of the current-template
 * being processed. Set in hpread_read_struct_type() while processing
 * a template type. Referred to in hpread_get_nth_templ_arg().
 * Yes, this is a kludge, but it arises from the kludge that already
 * exists in symtab.h, namely the fact that they encode
 * "template argument n" with fundamental type FT_TEMPLATE_ARG and
 * bitlength n. This means that deep in processing fundamental types
 * I need to ask the question "what template am I in the middle of?".
 * The alternative to stuffing a global would be to pass an argument
 * down the chain of calls just for this purpose.
 * 
 * There may be problems handling nested templates... tough.
 */
static struct type * current_template = NULL;

/* Read in and internalize a structure definition.  
 * This same routine is called for struct, union, and class types.
 * Also called for templates, since they build a very similar
 * type entry as for class types.
 */

static struct type *
hpread_read_struct_type (hp_type, dn_bufp, objfile)
     dnttpointer hp_type;
     union dnttentry *dn_bufp;
     struct objfile *objfile;
{
  /* The data members get linked together into a list of struct nextfield's */
  struct nextfield
    {
      struct nextfield *next;
      struct field field;
      unsigned char attributes; /* store visibility and virtuality info */
#     define ATTR_VIRTUAL 1      
#     define ATTR_PRIVATE 2      
#     define ATTR_PROTECT 3      
    };


  /* The methods get linked together into a list of struct next_fn_field's */
  struct next_fn_field
    {
      struct next_fn_field *next;
      struct fn_fieldlist field;
      struct fn_field fn_field;
      int num_fn_fields;
     };

  /* The template args get linked together into a list of struct next_template's */
  struct next_template
    {
      struct next_template *next;
      struct template_arg arg;
    };

  /* The template instantiations get linked together into a list of these... */
  struct next_instantiation 
    {
      struct next_instantiation * next;
      struct type * t;
    };

  struct type *type;
  struct type *baseclass;
  struct type *memtype;
  struct nextfield *list = 0, *tmp_list = 0;
  struct next_fn_field *fn_list = 0;
  struct next_fn_field *fn_p;
  struct next_template *t_new, *t_list = 0;
  struct nextfield *new;
  struct next_fn_field *fn_new;
  struct next_instantiation *i_new, *i_list = 0;
  int n, nfields = 0, n_fn_fields = 0, n_fn_fields_total = 0;
  int n_base_classes = 0, n_templ_args = 0;
  int ninstantiations = 0;
  dnttpointer field, fn_field, parent;
  union dnttentry *fieldp, *fn_fieldp, *parentp;
  int i;
  int static_member = 0;
  int const_member = 0;
  int volatile_member = 0;
  unsigned long vtbl_offset;
  int need_bitvectors = 0;
  char * method_name = NULL;
  char * method_alias = NULL;
  

  /* Is it something we've already dealt with?  */
  type = hpread_alloc_type (hp_type, objfile);
  if ((TYPE_CODE (type) == TYPE_CODE_STRUCT) ||
      (TYPE_CODE (type) == TYPE_CODE_UNION) ||
      (TYPE_CODE (type) == TYPE_CODE_CLASS) ||
      (TYPE_CODE (type) == TYPE_CODE_TEMPLATE))
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
  else if (dn_bufp->dblock.kind == DNTT_TYPE_CLASS)
    {
      TYPE_CODE (type) = TYPE_CODE_CLASS; 
      TYPE_LENGTH (type) = dn_bufp->dclass.bitlength / 8;

      /* Overrides the TYPE_CPLUS_SPECIFIC(type) with allocated memory
       * rather than &cplus_struct_default.
       */
      allocate_cplus_struct_type(type);

      /* Fill in declared-type.
       * (The C++ compiler will emit TYPE_CODE_CLASS 
       * for all 3 of "class", "struct"
       * "union", and we have to look at the "class_decl" field if we
       * want to know how it was really declared)
       */
      /* (0==class, 1==union, 2==struct) */
      TYPE_DECLARED_TYPE(type) = dn_bufp->dclass.class_decl;
    }
  else if (dn_bufp->dblock.kind == DNTT_TYPE_TEMPLATE)
    {
      /* Get the basic type correct.  */
      TYPE_CODE (type) = TYPE_CODE_TEMPLATE; 
      allocate_cplus_struct_type(type);
      TYPE_DECLARED_TYPE(type) = DECLARED_TYPE_TEMPLATE;
    }
  else
    return type;


  TYPE_FLAGS (type) &= ~TYPE_FLAG_STUB;

  /* For classes, read the parent list.
   * Question (RT): Do we need to do this for templates also?
   */
  if (dn_bufp->dblock.kind == DNTT_TYPE_CLASS) {

    /* First read the parent-list (classes from which we derive fields) */
    parent = dn_bufp->dclass.parentlist;
    while (parent.word && parent.word != DNTTNIL) {
      parentp = hpread_get_lntt (parent.dnttp.index, objfile);

      /* "parentp" should point to a DNTT_TYPE_INHERITANCE record */

      /* Get space to record the next field/data-member. */
      new = (struct nextfield *) alloca (sizeof (struct nextfield));
      new->next = list;
      list = new;

      FIELD_BITSIZE (list->field) = 0;

      /* The "classname" field is actually a DNTT pointer to the base class */
      baseclass = hpread_type_lookup (parentp->dinheritance.classname, 
  					     objfile);
      FIELD_TYPE (list->field) = baseclass;

      list->field.name = type_name_no_tag(FIELD_TYPE (list->field));

      list->attributes = 0;

      /* Check for virtuality of base, and set the
       * offset of the base subobject within the object.
       * (Offset set to -1 for virtual bases (for now).)
       */
      if (parentp->dinheritance.Virtual)
        {
	  B_SET(&(list->attributes), ATTR_VIRTUAL);
          parentp->dinheritance.offset = -1;
        }
      else
        FIELD_BITPOS (list->field) = parentp->dinheritance.offset;

      /* Check visibility */
      switch (parentp->dinheritance.visibility)
        {
          case 1:
            B_SET(&(list->attributes), ATTR_PROTECT);
            break;
          case 2:
            B_SET(&(list->attributes), ATTR_PRIVATE);
            break;
        }
      
      n_base_classes++;
      nfields++;

      parent = parentp->dinheritance.next;
    }
  }

  /* For templates, read the template argument list.
   * This must be done before processing the member list, because
   * the member list may refer back to this. E.g.:
   *   template <class T1, class T2> class q2 {
   *     public:
   *     T1 a;
   *     T2 b;
   *   };
   * We need to read the argument list "T1", "T2" first.
   */
  if (dn_bufp->dblock.kind == DNTT_TYPE_TEMPLATE) {
    /* Kludge alert: This stuffs a global "current_template" which
     * is referred to by hpread_get_nth_templ_arg(). The global
     * is cleared at the end of this routine.
     */
    current_template = type;

    /* Read in the argument list */
    field = dn_bufp->dtemplate.arglist;
    while (field.word && field.word != DNTTNIL) {
      /* Get this template argument*/
      fieldp = hpread_get_lntt (field.dnttp.index, objfile);
      if (fieldp->dblock.kind != DNTT_TYPE_TEMPLATE_ARG)
        {
          warning ("Invalid debug info: Template argument entry is of wrong kind");
          break;
        }
      /* Bump the count */ 
      n_templ_args++;
      /* Allocate and fill in a struct next_template */
      t_new = (struct next_template *) alloca (sizeof (struct next_template));
      t_new->next = t_list;
      t_list = t_new;
      t_list->arg.name = VT (objfile) + fieldp->dtempl_arg.name;
      t_list->arg.type = hpread_read_templ_arg_type(field, fieldp, 
                                                    objfile, t_list->arg.name);
      /* Walk to the next template argument */
      field = fieldp->dtempl_arg.nextarg;
    }
  }

  TYPE_NTEMPLATE_ARGS(type) = n_templ_args;

  if (n_templ_args > 0)
    TYPE_TEMPLATE_ARGS(type) = (struct template_arg *)
      obstack_alloc (&objfile->type_obstack, sizeof (struct template_arg) * n_templ_args);
  for (n = n_templ_args; t_list; t_list = t_list->next)
    {
      n -= 1;
      TYPE_TEMPLATE_ARG(type, n) = t_list->arg;
    }

  /* Next read in and internalize all the fields/members.  */
  if (dn_bufp->dblock.kind == DNTT_TYPE_STRUCT)
    field = dn_bufp->dstruct.firstfield;
  else if (dn_bufp->dblock.kind == DNTT_TYPE_UNION)
    field = dn_bufp->dunion.firstfield;
  else if (dn_bufp->dblock.kind == DNTT_TYPE_CLASS)
    field = dn_bufp->dclass.memberlist;
  else if (dn_bufp->dblock.kind == DNTT_TYPE_TEMPLATE)
    field = dn_bufp->dtemplate.memberlist;
  else
    field.word = DNTTNIL;

  while (field.word && field.word != DNTTNIL)
    {
      fieldp = hpread_get_lntt (field.dnttp.index, objfile);

      /* At this point "fieldp" may point to either a DNTT_TYPE_FIELD
       * or a DNTT_TYPE_GENFIELD record. 
       */
      vtbl_offset = 0;
      static_member = 0;
      const_member = 0;
      volatile_member = 0;
       
      if (fieldp->dblock.kind == DNTT_TYPE_GENFIELD) {

        /* The type will be GENFIELD if the field is a method or
         * a static member (or some other cases -- see below)
         */

        /* Follow a link to get to the record for the field. */
        fn_field = fieldp->dgenfield.field;
        fn_fieldp = hpread_get_lntt(fn_field.dnttp.index, objfile);

        /* Virtual funcs are indicated by a VFUNC which points to the
         * real entry
         */
        if (fn_fieldp->dblock.kind == DNTT_TYPE_VFUNC) {
          vtbl_offset = fn_fieldp->dvfunc.vtbl_offset;
          fn_field = fn_fieldp->dvfunc.funcptr;
          fn_fieldp = hpread_get_lntt(fn_field.dnttp.index, objfile);
        }

        /* A function's entry may be preceded by a modifier which
         * labels it static/constant/volatile.
         */
        if (fn_fieldp->dblock.kind == DNTT_TYPE_MODIFIER) {
          static_member = fn_fieldp->dmodifier.m_static;
          const_member = fn_fieldp->dmodifier.m_const;
          volatile_member = fn_fieldp->dmodifier.m_volatile;
          fn_field = fn_fieldp->dmodifier.type;
          fn_fieldp = hpread_get_lntt(fn_field.dnttp.index, objfile);
        }

        /* Check whether we have a method */
        if ((fn_fieldp->dblock.kind == DNTT_TYPE_MEMFUNC) ||
            (fn_fieldp->dblock.kind == DNTT_TYPE_FUNCTION) ||
            (fn_fieldp->dblock.kind == DNTT_TYPE_DOC_MEMFUNC) ||
            (fn_fieldp->dblock.kind == DNTT_TYPE_DOC_FUNCTION)) {
          /* Method found */

          short ix = 0;

          /* Look up function type of method */
	  memtype = hpread_type_lookup (fn_field, objfile);

          /* Methods can be seen before classes in the SOM records.
             If we are processing this class because it's a parameter of a
             method, at this point the method's type is actually incomplete;
             we'll have to fix it up later; mark the class for this. */ 
             
          if (TYPE_INCOMPLETE (memtype))
            {
              TYPE_FLAGS (type) |= TYPE_FLAG_INCOMPLETE;
              if (fixup_class)
                warning ("Two classes to fix up for method??  Type information may be incorrect for some classes.");
              if (fixup_method)
                warning ("Two methods to be fixed up at once?? Type information may be incorrect for some classes.");
              fixup_class = type;     /* remember this class has to be fixed up */
              fixup_method = memtype; /* remember the method type to be used in fixup */ 
            }

          /* HP aCC generates operator names without the "operator" keyword, and
             generates null strings as names for operators that are 
             user-defined type conversions to basic types (e.g. operator int ()).
             So try to reconstruct name as best as possible. */

          method_name = (char *) (VT (objfile) + fn_fieldp->dfunc.name);
          method_alias = (char *) (VT (objfile) + fn_fieldp->dfunc.alias);

          if (!method_name ||                               /* no name */ 
              !*method_name ||                              /* or null name */ 
              cplus_mangle_opname (method_name, DMGL_ANSI)) /* or name is an operator like "<" */ 
            {
              char * tmp_name = cplus_demangle (method_alias, DMGL_ANSI);
              char * op_string = strstr (tmp_name, "operator");
              method_name = xmalloc (strlen (op_string) + 1); /* don't overwrite VT! */ 
              strcpy (method_name, op_string);
            }
                        
          /* First check if a method of the same name has already been seen. */
          fn_p = fn_list;
          while (fn_p)
            {
              if (STREQ (fn_p->field.name, method_name))
                break;
              fn_p = fn_p->next;
            }

          /* If no such method was found, allocate a new entry in the list */ 
          if (!fn_p)
            {
              /* Get space to record this member function */
              /* Note: alloca used; this will disappear on routine exit */
              fn_new = (struct next_fn_field *) alloca (sizeof (struct next_fn_field));
              fn_new->next = fn_list;
              fn_list = fn_new;

              /* Fill in the fields of the struct nextfield */
          
              /* Record the (unmangled) method name */
              fn_list->field.name = method_name;
              /* Initial space for overloaded methods */
              /* Note: xmalloc is used; this will persist after this routine exits */ 
              fn_list->field.fn_fields = (struct fn_field *) xmalloc (5 * (sizeof (struct fn_field)));
              fn_list->field.length = 1; /* Init # of overloaded instances */ 
              fn_list->num_fn_fields = 5; /* # of entries for which space allocated */
              fn_p = fn_list;
              ix = 0; /* array index for fn_field */ 
              /* Bump the total count of the distinctly named methods */
              n_fn_fields++;
            }
          else /* Another overloaded instance of an already seen method name */
            {
              if (++(fn_p->field.length) > fn_p->num_fn_fields)
                {
                  /* Increase space allocated for overloaded instances */ 
                  fn_p->field.fn_fields
                    = (struct fn_field *) xrealloc (fn_p->field.fn_fields,
                                                     (fn_p->num_fn_fields + 5) * sizeof (struct fn_field));
                  fn_p->num_fn_fields += 5;
                }
              ix = fn_p->field.length -1;  /* array index for fn_field */
            }
          
          /* "physname" is intended to be the name of this overloaded instance. */
          if ((fn_fieldp->dfunc.language == HP_LANGUAGE_CPLUSPLUS) && 
	      method_alias && 
              *method_alias) /* not a null string */
            fn_p->field.fn_fields[ix].physname = method_alias;
	  else
            fn_p->field.fn_fields[ix].physname = method_name;
          /* What's expected here is the function type */
   /* But mark it as NULL if the method was incompletely processed
             We'll fix this up later when the method is fully processed */
          if (TYPE_INCOMPLETE (memtype))
            {
              fn_p->field.fn_fields[ix].type = NULL;
              fn_p->field.fn_fields[ix].args = NULL;
            }
          else
            {
              fn_p->field.fn_fields[ix].type = memtype;

              /* The argument list */
              fn_p->field.fn_fields[ix].type->type_specific.arg_types =
                (struct type **) obstack_alloc(&objfile->type_obstack,
                                               sizeof(struct type *) * (memtype->nfields + 1));
              for (i = 0; i < memtype->nfields; i++) 
                fn_p->field.fn_fields[ix].type->type_specific.arg_types[i] = memtype->fields[i].type;
              /* void termination */ 
              fn_p->field.fn_fields[ix].type->type_specific.arg_types[memtype->nfields] = builtin_type_void;

              /* pai: It's not clear why this args field has to be set.  Perhaps
               * it should be eliminated entirely. */
              fn_p->field.fn_fields[ix].args =
                (struct type **) obstack_alloc(&objfile->type_obstack,
                                               sizeof(struct type *) * (memtype->nfields + 1));
              for (i = 0; i < memtype->nfields; i++)
                fn_p->field.fn_fields[ix].args[i] = memtype->fields[i].type;
              /* null-terminated, unlike arg_types above e*/ 
              fn_p->field.fn_fields[ix].args[memtype->nfields] = NULL;
            }
          /* For virtual functions, fill in the voffset field with the
           * virtual table offset. (This is just copied over from the
           * SOM record; not sure if it is what GDB expects here...).
           * But if the function is a static method, set it to 1.
           * 
           * Note that we have to add 1 because 1 indicates a static
           * method, and 0 indicates a non-static, non-virtual method */

          if (static_member)
            fn_p->field.fn_fields[ix].voffset = VOFFSET_STATIC;
          else
            fn_p->field.fn_fields[ix].voffset = vtbl_offset ? vtbl_offset + 1 : 0;

          /* Also fill in the fcontext field with the current
           * class. (The latter isn't quite right: should be the baseclass
           * that defines the virtual function... Note we do have
           * a variable "baseclass" that we could stuff into the fcontext
           * field, but "baseclass" isn't necessarily right either,
           * since the virtual function could have been defined more
           * than one level up).
           */

          if (vtbl_offset != 0)
            fn_p->field.fn_fields[ix].fcontext = type;
          else
            fn_p->field.fn_fields[ix].fcontext = NULL;

          /* Other random fields pertaining to this method */
          fn_p->field.fn_fields[ix].is_const = const_member;
          fn_p->field.fn_fields[ix].is_volatile = volatile_member; /* ?? */ 
          switch (fieldp->dgenfield.visibility) {
            case 1:
              fn_p->field.fn_fields[ix].is_protected = 1;
              fn_p->field.fn_fields[ix].is_private = 0;
              break;
            case 2:
              fn_p->field.fn_fields[ix].is_protected = 0;
              fn_p->field.fn_fields[ix].is_private = 1;
              break;
            default: /* public */
              fn_p->field.fn_fields[ix].is_protected = 0;
              fn_p->field.fn_fields[ix].is_private = 0;
          }
          fn_p->field.fn_fields[ix].is_stub = 0;

          /* HP aCC emits both MEMFUNC and FUNCTION entries for a method;
             if the class points to the FUNCTION, there is usually separate
             code for the method; but if we have a MEMFUNC, the method has
             been inlined (and there is usually no FUNCTION entry)
             FIXME Not sure if this test is accurate. pai/1997-08-22 */ 
          if ((fn_fieldp->dblock.kind == DNTT_TYPE_MEMFUNC) ||
              (fn_fieldp->dblock.kind == DNTT_TYPE_DOC_MEMFUNC))
            fn_p->field.fn_fields[ix].is_inlined = 1;
          else
            fn_p->field.fn_fields[ix].is_inlined = 0;
          
          fn_p->field.fn_fields[ix].dummy = 0;
          
          /* Bump the total count of the member functions */
          n_fn_fields_total++;

        } else if (fn_fieldp->dblock.kind == DNTT_TYPE_SVAR) {
          /* This case is for static data members of classes */ 
          
          /* pai:: FIXME -- check that "staticmem" bit is set */

          /* Get space to record this static member */
          new = (struct nextfield *) alloca (sizeof (struct nextfield));
          new->next = list;
          list = new;
          
          list->field.name = VT (objfile) + fn_fieldp->dsvar.name;
          FIELD_BITSIZE (list->field) = -1; /* indicates static member */
          SET_FIELD_PHYSNAME (list->field, 0);  /* initialize to empty */
          memtype = hpread_type_lookup (fn_fieldp->dsvar.type, objfile);
          
          FIELD_TYPE (list->field) = memtype;
          list->attributes = 0;
          switch (fieldp->dgenfield.visibility) {
            case 1:
              B_SET(&(list->attributes), ATTR_PROTECT);
              break;
            case 2:
              B_SET(&(list->attributes), ATTR_PRIVATE);
              break;
          }
          nfields++;
        }

        else if (fn_fieldp->dblock.kind == DNTT_TYPE_FIELD)
          {
            /* FIELDs follow GENFIELDs for fields of anonymous unions.
               Code below is replicated from the case for FIELDs further
               below, except that fieldp is replaced by fn_fieldp */ 
            if (!fn_fieldp->dfield.a_union)
              warning ("Debug info inconsistent: FIELD of anonymous union doesn't have a_union bit set");
            /* Get space to record the next field/data-member. */
            new = (struct nextfield *) alloca (sizeof (struct nextfield));
            new->next = list;
            list = new;

            list->field.name = VT (objfile) + fn_fieldp->dfield.name;
            FIELD_BITPOS (list->field) = fn_fieldp->dfield.bitoffset;
            if (fn_fieldp->dfield.bitlength % 8)
              list->field.bitsize = fn_fieldp->dfield.bitlength;
            else
              list->field.bitsize = 0;

            memtype = hpread_type_lookup (fn_fieldp->dfield.type, objfile);
            list->field.type = memtype;
            list->attributes = 0;
            switch (fn_fieldp->dfield.visibility) {
              case 1:
                B_SET(&(list->attributes), ATTR_PROTECT);
                break;
              case 2:
                B_SET(&(list->attributes), ATTR_PRIVATE);
                break;
            }
            nfields++;
          }
        else if (fn_fieldp->dblock.kind == DNTT_TYPE_SVAR)
          {
            /* Field of anonymous union; union is not inside a class */
            if (!fn_fieldp->dsvar.a_union)
              warning ("Debug info inconsistent: SVAR field in anonymous union doesn't have a_union bit set");
            /* Get space to record the next field/data-member. */
            new = (struct nextfield *) alloca (sizeof (struct nextfield));
            new->next = list;
            list = new;

            list->field.name = VT (objfile) + fn_fieldp->dsvar.name;
            FIELD_BITPOS (list->field) = 0;  /* FIXME is this always true? */
            FIELD_BITSIZE (list->field) = 0; /* use length from type */
            memtype = hpread_type_lookup (fn_fieldp->dsvar.type, objfile);
            list->field.type = memtype;
            list->attributes = 0;
            /* No info to set visibility -- always public */ 
            nfields++;
          }
        else if (fn_fieldp->dblock.kind == DNTT_TYPE_DVAR)
          {
            /* Field of anonymous union; union is not inside a class */
            if (!fn_fieldp->ddvar.a_union)
              warning ("Debug info inconsistent: DVAR field in anonymous union doesn't have a_union bit set");
            /* Get space to record the next field/data-member. */
            new = (struct nextfield *) alloca (sizeof (struct nextfield));
            new->next = list;
            list = new;

            list->field.name = VT (objfile) + fn_fieldp->ddvar.name;
            FIELD_BITPOS (list->field) = 0;  /* FIXME is this always true? */
            FIELD_BITSIZE (list->field) = 0; /* use length from type */ 
            memtype = hpread_type_lookup (fn_fieldp->ddvar.type, objfile);
            list->field.type = memtype;
            list->attributes = 0;
            /* No info to set visibility -- always public */ 
            nfields++;
          }
        else { /* Not a method, nor a static data member, nor an anon union field */

          /* This case is for miscellaneous type entries (local enums,
             local function templates, etc.) that can be present
             inside a class. */

          /* Enums -- will be handled by other code that takes care
             of DNTT_TYPE_ENUM; here we see only DNTT_TYPE_MEMENUM so
             it's not clear we could have handled them here at all. */
          /* FUNC_TEMPLATE: is handled by other code (??). */
          /* MEMACCESS: modified access for inherited member. Not
             sure what to do with this, ignoriing it at present. */ 

          /* What other entries can appear following a GENFIELD which
             we do not handle above?  (MODIFIER, VFUNC handled above.) */

          if ((fn_fieldp->dblock.kind != DNTT_TYPE_MEMACCESS) &&
              (fn_fieldp->dblock.kind != DNTT_TYPE_MEMENUM) &&
              (fn_fieldp->dblock.kind != DNTT_TYPE_FUNC_TEMPLATE))
            warning ("Internal error: Unexpected debug record kind %d found following DNTT_GENFIELD",
                     fn_fieldp->dblock.kind);
        }
        /* walk to the next FIELD or GENFIELD */
        field = fieldp->dgenfield.nextfield;

      } 
      else if (fieldp->dblock.kind == DNTT_TYPE_FIELD) {
        
        /* Ordinary structure/union/class field */
        struct type * anon_union_type;

        /* Get space to record the next field/data-member. */
	new = (struct nextfield *) alloca (sizeof (struct nextfield));
        new->next = list;
        list = new;
        
        list->field.name = VT (objfile) + fieldp->dfield.name;


        /* A FIELD by itself (without a GENFIELD) can also be a static member */
        if (fieldp->dfield.staticmem)
          {
	    FIELD_BITPOS (list->field) = -1;
	    FIELD_BITSIZE (list->field) = 0;
          }
        else /* Non-static data member */ 
          {
	    FIELD_BITPOS (list->field) = fieldp->dfield.bitoffset;
	    if (fieldp->dfield.bitlength % 8)
	      FIELD_BITSIZE (list->field) = fieldp->dfield.bitlength;
	    else
	      FIELD_BITSIZE (list->field) = 0;
          }

        memtype = hpread_type_lookup (fieldp->dfield.type, objfile);
        FIELD_TYPE (list->field) = memtype;
        list->attributes = 0;
        switch (fieldp->dfield.visibility) {
          case 1:
            B_SET(&(list->attributes), ATTR_PROTECT);
            break;
          case 2:
            B_SET(&(list->attributes), ATTR_PRIVATE);
            break;
        }
        nfields++;


        /* Note 1: First, we have to check if the current field is an anonymous
           union. If it is, then *its* fields are threaded along in the
           nextfield chain. :-( This was supposed to help debuggers, but is
           really just a nuisance since we deal with anonymous unions anyway by
           checking that the name is null.  So anyway, we skip over the fields
           of the anonymous union. pai/1997-08-22 */
        /* Note 2: In addition, the bitoffsets for the fields of the anon union
           are relative to the enclosing struct, *NOT* relative to the anon
           union!  This is an even bigger nuisance -- we have to go in and munge
           the anon union's type information appropriately. pai/1997-08-22 */

        /* Both tasks noted above are done by a separate function.  This takes us
           to the next FIELD or GENFIELD, skipping anon unions, and recursively
           processing intermediate types. */ 
        field = hpread_get_next_skip_over_anon_unions (1, field, &fieldp, objfile);

      } else {
        /* neither field nor genfield ?? is this possible?? */
        /* pai:: FIXME walk to the next -- how? */
        warning ("Internal error: unexpected DNTT kind %d encountered as field of struct");
        warning ("Skipping remaining fields of struct");
        break; /* get out of loop of fields */ 
      }
    }

  /* If it's a template, read in the instantiation list */
  if (dn_bufp->dblock.kind == DNTT_TYPE_TEMPLATE) {
    ninstantiations = 0;
    field = dn_bufp->dtemplate.expansions;
    while (field.word && field.word != DNTTNIL) {
      fieldp = hpread_get_lntt (field.dnttp.index, objfile);
 
      /* The expansions or nextexp should point to a tagdef */
      if (fieldp->dblock.kind != DNTT_TYPE_TAGDEF) 
        break;

      i_new = (struct next_instantiation *) alloca (sizeof (struct next_instantiation)); 
      i_new->next = i_list;
      i_list = i_new;
      i_list->t = hpread_type_lookup (field, objfile);
      ninstantiations++;

      /* And the "type" field of that should point to a class */
      field = fieldp->dtag.type;
      fieldp = hpread_get_lntt (field.dnttp.index, objfile);
      if (fieldp->dblock.kind != DNTT_TYPE_CLASS) 
        break;

      /* Get the next expansion */
      field = fieldp->dclass.nextexp;
    }
  }
  TYPE_NINSTANTIATIONS(type) = ninstantiations;
  if (ninstantiations > 0) 
    TYPE_INSTANTIATIONS(type) = (struct type **)
      obstack_alloc (&objfile->type_obstack, sizeof (struct type *) * ninstantiations);
  for (n = ninstantiations; i_list; i_list = i_list->next)
    {
      n -= 1;
      TYPE_INSTANTIATION(type, n) = i_list->t;
    }


  /* Copy the field-list to GDB's symbol table */
  TYPE_NFIELDS (type) = nfields;
  TYPE_N_BASECLASSES (type) = n_base_classes;
  TYPE_FIELDS (type) = (struct field *)
    obstack_alloc (&objfile->type_obstack, sizeof (struct field) * nfields);
  /* Copy the saved-up fields into the field vector.  */
  for (n = nfields, tmp_list = list; tmp_list; tmp_list = tmp_list->next)
    {
      n -= 1;
      TYPE_FIELD (type, n) = tmp_list->field;
    }

  /* Copy the "function-field-list" (i.e., the list of member
   * functions in the class) to GDB's symbol table 
   */
  TYPE_NFN_FIELDS (type) = n_fn_fields;
  TYPE_NFN_FIELDS_TOTAL (type) = n_fn_fields_total;
  TYPE_FN_FIELDLISTS(type) = (struct fn_fieldlist *)
    obstack_alloc (&objfile->type_obstack, sizeof (struct fn_fieldlist) * n_fn_fields);
  for (n = n_fn_fields; fn_list; fn_list = fn_list->next)
    {
      n -= 1;
      TYPE_FN_FIELDLIST(type, n) = fn_list->field;
    }

  /* pai:: FIXME -- perhaps each bitvector should be created individually */
  for (n = nfields, tmp_list = list; tmp_list; tmp_list = tmp_list->next)
    {
      n -= 1;
      if (tmp_list->attributes)
        {
          need_bitvectors = 1;
          break;
        }
    }

  if (need_bitvectors)
    {
      /* pai:: this step probably redundant */ 
      ALLOCATE_CPLUS_STRUCT_TYPE (type);

      TYPE_FIELD_VIRTUAL_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_VIRTUAL_BITS (type), nfields);

      TYPE_FIELD_PRIVATE_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_PRIVATE_BITS (type), nfields);
      
      TYPE_FIELD_PROTECTED_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_PROTECTED_BITS (type), nfields);

      /* this field vector isn't actually used with HP aCC */
      TYPE_FIELD_IGNORE_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_IGNORE_BITS (type), nfields);

      while (nfields-- > 0)
        {
          if (B_TST(&(list->attributes),ATTR_VIRTUAL))
            SET_TYPE_FIELD_VIRTUAL (type, nfields);
          if (B_TST(&(list->attributes),ATTR_PRIVATE))
            SET_TYPE_FIELD_PRIVATE (type, nfields);
          if (B_TST(&(list->attributes),ATTR_PROTECT))
            SET_TYPE_FIELD_PROTECTED (type, nfields);

          list = list->next;
        }
    }
  else
    {
      TYPE_FIELD_VIRTUAL_BITS(type) = NULL;
      TYPE_FIELD_PROTECTED_BITS(type) = NULL;
      TYPE_FIELD_PRIVATE_BITS(type) = NULL;
    }
  
  if (has_vtable(type))
    {
      /* Allocate space for class runtime information */
      TYPE_RUNTIME_PTR(type) = (struct runtime_info *) xmalloc (sizeof(struct runtime_info));
      /* Set flag for vtable */
      TYPE_VTABLE(type) = 1;
      /* The first non-virtual base class with a vtable. */
      TYPE_PRIMARY_BASE(type) = primary_base_class(type);
      /* The virtual base list. */
      TYPE_VIRTUAL_BASE_LIST(type) = virtual_base_list(type);
    }
  else
    TYPE_RUNTIME_PTR(type) = NULL;

  /* If this is a local type (C++ - declared inside a function), record file name & line # */
  if (hpread_get_scope_depth (dn_bufp, objfile, 1 /* no need for real depth */))
    {
      TYPE_LOCALTYPE_PTR (type) = (struct local_type_info *) xmalloc (sizeof (struct local_type_info));
      TYPE_LOCALTYPE_FILE (type) = (char *) xmalloc (strlen (current_subfile->name) + 1);
      strcpy (TYPE_LOCALTYPE_FILE (type), current_subfile->name);
      if (current_subfile->line_vector && (current_subfile->line_vector->nitems > 0))
        TYPE_LOCALTYPE_LINE (type) = current_subfile->line_vector->item[current_subfile->line_vector->nitems - 1].line;
      else
        TYPE_LOCALTYPE_LINE (type) = 0;
    }
  else
    TYPE_LOCALTYPE_PTR (type) = NULL;

  /* Clear the global saying what template we are in the middle of processing */
  current_template = NULL;

  return type;
}

/* Adjust the physnames for each static member of a struct
   or class type to be something like "A::x"; then various
   other pieces of code that do a lookup_symbol on the phyname
   work correctly.
   TYPE is a pointer to the struct/class type
   NAME is a char * (string) which is the class/struct name
   Void return */ 

static void
fix_static_member_physnames (type, class_name, objfile)
  struct type * type;
  char * class_name;
  struct objfile * objfile;
{
  int i;

  /* We fix the member names only for classes or structs */ 
  if (TYPE_CODE (type) != TYPE_CODE_STRUCT)
    return;

  for (i=0; i < TYPE_NFIELDS (type); i++)
    if (TYPE_FIELD_STATIC (type, i))
      {
        if (TYPE_FIELD_STATIC_PHYSNAME (type, i))
          return;  /* physnames are already set */

        SET_FIELD_PHYSNAME (type->fields[i], 
                 obstack_alloc (&objfile->type_obstack,
                                  strlen (class_name) + strlen (TYPE_FIELD_NAME (type, i)) + 3));
        strcpy (TYPE_FIELD_STATIC_PHYSNAME (type, i), class_name);
        strcat (TYPE_FIELD_STATIC_PHYSNAME (type, i), "::");
        strcat (TYPE_FIELD_STATIC_PHYSNAME (type, i), TYPE_FIELD_NAME (type, i));
      }
}

/* Fix-up the type structure for a CLASS so that the type entry
 * for a method (previously marked with a null type in hpread_read_struct_type()
 * is set correctly to METHOD.
 * OBJFILE is as for other such functions. 
 * Void return. */ 
 
static void
fixup_class_method_type (class, method, objfile)
  struct type * class;
  struct type * method;
  struct objfile * objfile;
{
  int i, j, k;

  if (!class || !method || !objfile)
    return;

  /* Only for types that have methods */
  if ((TYPE_CODE (class) != TYPE_CODE_CLASS) &&
      (TYPE_CODE (class) != TYPE_CODE_UNION))
    return;

  /* Loop over all methods and find the one marked with a NULL type */ 
  for (i = 0; i < TYPE_NFN_FIELDS (class); i++)
    for (j = 0; j < TYPE_FN_FIELDLIST_LENGTH (class, i); j++)
      if (TYPE_FN_FIELD_TYPE (TYPE_FN_FIELDLIST1 (class, i), j) == NULL)
        {
          /* Set the method type */ 
          TYPE_FN_FIELD_TYPE (TYPE_FN_FIELDLIST1 (class, i), j) = method;
          /* The argument list */
          (TYPE_FN_FIELD_TYPE (TYPE_FN_FIELDLIST1 (class, i), j))->type_specific.arg_types
            = (struct type **) obstack_alloc(&objfile->type_obstack,
                                             sizeof(struct type *) * (method->nfields + 1));
          for (k = 0; k < method->nfields; k++) 
          (TYPE_FN_FIELD_TYPE (TYPE_FN_FIELDLIST1 (class, i), j))->type_specific.arg_types[k] = method->fields[k].type;
          /* void termination */ 
          (TYPE_FN_FIELD_TYPE (TYPE_FN_FIELDLIST1 (class, i), j))->type_specific.arg_types[method->nfields] = builtin_type_void;
          
          /* pai: It's not clear why this args field has to be set.  Perhaps
           * it should be eliminated entirely. */
          (TYPE_FN_FIELD (TYPE_FN_FIELDLIST1 (class, i), j)).args
            = (struct type **) obstack_alloc(&objfile->type_obstack,
                                             sizeof(struct type *) * (method->nfields + 1));
          for (k = 0; k < method->nfields; k++)
            (TYPE_FN_FIELD (TYPE_FN_FIELDLIST1 (class, i), j)).args[k] = method->fields[k].type;
          /* null-terminated, unlike arg_types above */ 
          (TYPE_FN_FIELD (TYPE_FN_FIELDLIST1 (class, i), j)).args[method->nfields] = NULL;

          /* Break out of both loops -- only one method to fix up in a class */ 
          goto finish;
        }

finish:
  TYPE_FLAGS (class) &= ~TYPE_FLAG_INCOMPLETE;  
}


/* If we're in the middle of processing a template, get a pointer
 * to the Nth template argument.
 * An example may make this clearer:
 *   template <class T1, class T2> class q2 {
 *     public:
 *     T1 a;
 *     T2 b;
 *   };
 * The type for "a" will be "first template arg" and
 * the type for "b" will be "second template arg".
 * We need to look these up in order to fill in "a" and "b"'s type.
 * This is called from hpread_type_lookup().
 */
static struct type * 
hpread_get_nth_template_arg(objfile, n)
     struct objfile *objfile;
     int n;
{
   if (current_template != NULL)
     return TYPE_TEMPLATE_ARG(current_template, n).type;
   else
     return lookup_fundamental_type (objfile, FT_TEMPLATE_ARG);
}

/* Read in and internalize a TEMPL_ARG (template arg) symbol.  */

static struct type *
hpread_read_templ_arg_type (hp_type, dn_bufp, objfile, name)
     dnttpointer hp_type;
     union dnttentry *dn_bufp;
     struct objfile *objfile;
     char * name;
{
  struct type *type;

  /* See if it's something we've already deal with.  */
  type = hpread_alloc_type (hp_type, objfile);
  if (TYPE_CODE (type) == TYPE_CODE_TEMPLATE_ARG)
    return type;

  /* Nope.  Fill in the appropriate fields.  */
  TYPE_CODE (type) = TYPE_CODE_TEMPLATE_ARG;
  TYPE_LENGTH (type) = 0;
  TYPE_NFIELDS (type) = 0;
  TYPE_NAME (type) = name;
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
  
  /* Allocate an array type symbol.
   * Why no check for already-read here, like in the other
   * hpread_read_xxx_type routines?  Because it kept us 
   * from properly determining the size of the array!  
   */
  type = hpread_alloc_type (hp_type, objfile);

  TYPE_CODE (type) = TYPE_CODE_ARRAY;

  /* Although the hp-symtab.h does not *require* this to be the case,
   * GDB is assuming that "arrayisbytes" and "elemisbytes" be consistent.
   * I.e., express both array-length and element-length in bits,
   * or express both array-length and element-length in bytes.
   */
  if (!((dn_bufp->darray.arrayisbytes && dn_bufp->darray.elemisbytes) ||
	(!dn_bufp->darray.arrayisbytes && !dn_bufp->darray.elemisbytes))) {
    warning ("error in hpread_array_type.\n");
    return;
  } else if (dn_bufp->darray.arraylength == 0x7fffffff) {
    /* The HP debug format represents char foo[]; as an array with
     * length 0x7fffffff.  Internally GDB wants to represent this
     *	as an array of length zero.  
     */
    TYPE_LENGTH (type) = 0;
  } else if (dn_bufp->darray.arrayisbytes)
    TYPE_LENGTH (type) = dn_bufp->darray.arraylength;
  else /* arraylength is in bits */
    TYPE_LENGTH (type) = dn_bufp->darray.arraylength / 8;

  TYPE_TARGET_TYPE (type) = hpread_type_lookup (dn_bufp->darray.elemtype,
						objfile);

  /* The one "field" is used to store the subscript type */
  /* Since C and C++ multi-dimensional arrays are simply represented
   * as: array of array of ..., we only need one subscript-type
   * per array. This subscript type is typically a subrange of integer.
   * If this gets extended to support languages like Pascal, then
   * we need to fix this to represent multi-dimensional arrays properly.
   */
  TYPE_NFIELDS (type) = 1;
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

/* struct type * hpread_type_lookup(hp_type, objfile)
 *   Arguments:
 *     hp_type: A pointer into the DNTT specifying what type we
 *              are about to "look up"., or else [for fundamental types
 *              like int, float, ...] an "immediate" structure describing
 *              the type.
 *     objfile: ?
 *   Return value: A pointer to a "struct type" (representation of a
 *                 type in GDB's internal symbol table - see gdbtypes.h)
 *   Routine description:
 *     There are a variety of places when scanning the DNTT when we
 *     need to interpret a "type" field. The simplest and most basic 
 *     example is when we're processing the symbol table record
 *     for a data symbol (a SVAR or DVAR record). That has
 *     a "type" field specifying the type of the data symbol. That
 *     "type" field is either an "immediate" type specification (for the
 *     fundamental types) or a DNTT pointer (for more complicated types). 
 *     For the more complicated types, we may or may not have already
 *     processed the pointed-to type. (Multiple data symbols can of course
 *     share the same type).
 *     The job of hpread_type_lookup() is to process this "type" field.
 *     Most of the real work is done in subroutines. Here we interpret
 *     the immediate flag. If not immediate, chase the DNTT pointer to
 *     find our way to the SOM record describing the type, switch on
 *     the SOM kind, and then call an appropriate subroutine depending
 *     on what kind of type we are constructing. (e.g., an array type,
 *     a struct/class type, etc).
 */
static struct type *
hpread_type_lookup (hp_type, objfile)
     dnttpointer hp_type;
     struct objfile *objfile;
{
  union dnttentry *dn_bufp;
  struct type * tmp_type;

  /* First see if it's a simple builtin type.  */
  if (hp_type.dntti.immediate)
    /* If this is a template argument, the argument number is
     * encoded in the bitlength. All other cases, just return
     * GDB's representation of this fundamental type.
     */
    if (hp_type.dntti.type == HP_TYPE_TEMPLATE_ARG)
      return hpread_get_nth_template_arg(objfile, hp_type.dntti.bitlength);
    else
      return lookup_fundamental_type (objfile, hpread_type_translate (hp_type));

  /* Not a builtin type.  We'll have to read it in.  */
  if (hp_type.dnttp.index < LNTT_SYMCOUNT (objfile))
    dn_bufp = hpread_get_lntt (hp_type.dnttp.index, objfile);
  else
    /* This is a fancy way of returning NULL */
    return lookup_fundamental_type (objfile, FT_VOID);

  switch (dn_bufp->dblock.kind)
    {
    case DNTT_TYPE_SRCFILE:
    case DNTT_TYPE_MODULE:
    case DNTT_TYPE_ENTRY:
    case DNTT_TYPE_BEGIN:
    case DNTT_TYPE_END:
    case DNTT_TYPE_IMPORT:
    case DNTT_TYPE_LABEL:
    case DNTT_TYPE_FPARAM:
    case DNTT_TYPE_SVAR:
    case DNTT_TYPE_DVAR:
    case DNTT_TYPE_CONST:
    case DNTT_TYPE_MEMENUM:
    case DNTT_TYPE_VARIANT:
    case DNTT_TYPE_FILE:
    case DNTT_TYPE_WITH:
    case DNTT_TYPE_COMMON:
    case DNTT_TYPE_COBSTRUCT:
    case DNTT_TYPE_XREF:
    case DNTT_TYPE_SA:
    case DNTT_TYPE_MACRO:
    case DNTT_TYPE_BLOCKDATA:
    case DNTT_TYPE_CLASS_SCOPE:
    case DNTT_TYPE_MEMACCESS:
    case DNTT_TYPE_INHERITANCE:
    case DNTT_TYPE_OBJECT_ID:
    case DNTT_TYPE_FRIEND_CLASS:
    case DNTT_TYPE_FRIEND_FUNC:
      /* These are not types - something went wrong.  */
      /* This is a fancy way of returning NULL */
      return lookup_fundamental_type (objfile, FT_VOID);

    case DNTT_TYPE_FUNCTION:
      /* We wind up here when dealing with class member functions 
       * (called from hpread_read_struct_type(), i.e. when processing
       * the class definition itself).
       */
      return hpread_read_function_type (hp_type, dn_bufp, objfile, 0);

    case DNTT_TYPE_DOC_FUNCTION:
      return hpread_read_doc_function_type (hp_type, dn_bufp, objfile, 0);

    case DNTT_TYPE_TYPEDEF:
      {
        /* A typedef - chase it down by making a recursive call */
	struct type *structtype = hpread_type_lookup (dn_bufp->dtype.type,
						      objfile);

        /* The following came from the base hpread.c that we inherited.
         * It is WRONG so I have commented it out. - RT
         *...

	char *suffix;
	suffix = VT (objfile) + dn_bufp->dtype.name;
 	TYPE_NAME (structtype) = suffix;

         * ... further explanation ....
         *
         * What we have here is a typedef pointing to a typedef.
         * E.g.,
         * typedef int foo;
         * typedef foo fum;
         *
         * What we desire to build is (these are pictures
         * of "struct type"'s): 
         *
         *  +---------+     +----------+     +------------+
         *  | typedef |     | typedef  |     | fund. type |
         *  |     type| ->  |      type| ->  |            |
         *  | "fum"   |     | "foo"    |     | "int"      |
         *  +---------+     +----------+     +------------+
         *
         * What this commented-out code is doing is smashing the
         * name of pointed-to-type to be the same as the pointed-from
         * type. So we wind up with something like:
         *
         *  +---------+     +----------+     +------------+
         *  | typedef |     | typedef  |     | fund. type |
         *  |     type| ->  |      type| ->  |            |
         *  | "fum"   |     | "fum"    |     | "fum"      |
         *  +---------+     +----------+     +------------+
         * 
         */

	return structtype;
      }

    case DNTT_TYPE_TAGDEF:
	{
	/* Just a little different from above.  We have to tack on
	 * an identifier of some kind (struct, union, enum, class, etc).  
         */
	struct type *structtype = hpread_type_lookup (dn_bufp->dtype.type,
						      objfile);
	char *prefix, *suffix;
	suffix = VT (objfile) + dn_bufp->dtype.name;

	/* Lookup the next type in the list.  It should be a structure,
	 * union, class, enum, or template type.  
         * We will need to attach that to our name.  
         */
	if (dn_bufp->dtype.type.dnttp.index < LNTT_SYMCOUNT (objfile))
	  dn_bufp = hpread_get_lntt (dn_bufp->dtype.type.dnttp.index, objfile);
	else {
          complain (&hpread_type_lookup_complaint);
          return;
        }

	if (dn_bufp->dblock.kind == DNTT_TYPE_STRUCT) {
	  prefix = "struct ";
	} else if (dn_bufp->dblock.kind == DNTT_TYPE_UNION) {
	  prefix = "union ";
	} else if (dn_bufp->dblock.kind == DNTT_TYPE_CLASS) { 
          /* Further field for CLASS saying how it was really declared */
          /* 0==class, 1==union, 2==struct */
          if (dn_bufp->dclass.class_decl == 0) 
	    prefix = "class ";
          else if (dn_bufp->dclass.class_decl == 1) 
	    prefix = "union ";
          else if (dn_bufp->dclass.class_decl == 2) 
	    prefix = "struct ";
          else
            prefix = "";
	} else if (dn_bufp->dblock.kind == DNTT_TYPE_ENUM) {
	  prefix = "enum ";
	} else if (dn_bufp->dblock.kind == DNTT_TYPE_TEMPLATE) {
	  prefix = "template ";
        } else {
          prefix = "";
        }

	/* Build the correct name.  */
	structtype->name
	  = (char *) obstack_alloc (&objfile->type_obstack,
				    strlen (prefix) + strlen (suffix) + 1);
	TYPE_NAME (structtype) = strcpy (TYPE_NAME (structtype), prefix);
	TYPE_NAME (structtype) = strcat (TYPE_NAME (structtype), suffix);
	TYPE_TAG_NAME (structtype) = suffix;

        /* For classes/structs, we have to set the static member "physnames"
           to point to strings like "Class::Member" */ 
        if (TYPE_CODE (structtype) == TYPE_CODE_STRUCT)
          fix_static_member_physnames (structtype, suffix, objfile);

	return structtype;
      }

    case DNTT_TYPE_POINTER:
      /* Pointer type - call a routine in gdbtypes.c that constructs
       * the appropriate GDB type.
       */
      return make_pointer_type (
          hpread_type_lookup (dn_bufp->dptr.pointsto, 
                              objfile), 
          NULL);

    case DNTT_TYPE_REFERENCE:
      /* C++ reference type - call a routine in gdbtypes.c that constructs
       * the appropriate GDB type.
       */
      return make_reference_type (
                 hpread_type_lookup (dn_bufp->dreference.pointsto, 
                                     objfile), 
                 NULL);
   
    case DNTT_TYPE_ENUM:
      return hpread_read_enum_type (hp_type, dn_bufp, objfile);
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

    case DNTT_TYPE_FUNCTYPE:
      /* Here we want to read the function SOMs and return a 
       * type for it. We get here, for instance, when processing
       * pointer-to-function type.
       */
      return hpread_read_function_type (hp_type, dn_bufp, objfile, 0);

    case DNTT_TYPE_PTRMEM:
      /* Declares a C++ pointer-to-data-member type. 
       * The "pointsto" field defines the class,
       * while the "memtype" field defines the pointed-to-type.
       */
      {
      struct type * ptrmemtype;
      struct type * class_type;
      struct type * memtype;
      memtype = hpread_type_lookup (dn_bufp->dptrmem.memtype, 
                                    objfile), 
      class_type = hpread_type_lookup (dn_bufp->dptrmem.pointsto, 
                                       objfile), 
      ptrmemtype = alloc_type(objfile);
      smash_to_member_type(ptrmemtype, class_type, memtype); 
      return make_pointer_type(ptrmemtype, NULL);
      }
      break;

    case DNTT_TYPE_PTRMEMFUNC:
      /* Defines a C++ pointer-to-function-member type. 
       * The "pointsto" field defines the class,
       * while the "memtype" field defines the pointed-to-type.
       */
      {
      struct type * ptrmemtype;
      struct type * class_type;
      struct type * functype;
      struct type * retvaltype;
      int nargs;
      int i;
      struct type ** args_type;
      class_type = hpread_type_lookup (dn_bufp->dptrmem.pointsto, 
                                       objfile); 
      functype = hpread_type_lookup (dn_bufp->dptrmem.memtype, 
                                     objfile); 
      retvaltype = TYPE_TARGET_TYPE (functype);
      nargs =   TYPE_NFIELDS (functype);
      args_type = (struct type **) xmalloc ((nargs+1) * sizeof (struct type *));
      for (i = 0; i < nargs; i++) {
        args_type[i] = TYPE_FIELD_TYPE (functype, i); 
      }
      args_type[nargs] = NULL;
      ptrmemtype = alloc_type(objfile);
      smash_to_method_type(ptrmemtype, class_type, retvaltype, args_type); 
      return make_pointer_type(ptrmemtype, NULL);
      }
      break;

    case DNTT_TYPE_CLASS: 
      return hpread_read_struct_type (hp_type, dn_bufp, objfile);

    case DNTT_TYPE_GENFIELD:
      /* Chase pointer from GENFIELD to FIELD, and make recursive
       * call on that.
       */
      return hpread_type_lookup (dn_bufp->dgenfield.field, objfile);

    case DNTT_TYPE_VFUNC:
      /* C++ virtual function.
       * We get here in the course of processing a class type which
       * contains virtual functions. Just go through another level
       * of indirection to get to the pointed-to function SOM.
       */
      return hpread_type_lookup (dn_bufp->dvfunc.funcptr, objfile);

    case DNTT_TYPE_MODIFIER:
      /* Check the modifiers and then just make a recursive call on
       * the "type" pointed to by the modifier DNTT.
       * 
       * pai:: FIXME -- do we ever want to handle "m_duplicate" and
       * "m_void" modifiers?  Is static_flag really needed here?
       * (m_static used for methods of classes, elsewhere).
       */
      tmp_type = make_cv_type (dn_bufp->dmodifier.m_const, 
                               dn_bufp->dmodifier.m_volatile,
                               hpread_type_lookup (dn_bufp->dmodifier.type, objfile),
                               0);
      return tmp_type;
      

    case DNTT_TYPE_MEMFUNC:
      /* Member function. Treat like a function.
       * I think we get here in the course of processing a 
       * pointer-to-member-function type...
       */
      return hpread_read_function_type (hp_type, dn_bufp, objfile, 0);

    case DNTT_TYPE_DOC_MEMFUNC:
      return hpread_read_doc_function_type (hp_type, dn_bufp, objfile, 0);

    case DNTT_TYPE_TEMPLATE:
      /* Template - sort of the header for a template definition,
       * which like a class, points to a member list and also points
       * to a TEMPLATE_ARG list of type-arguments.
       */
      return hpread_read_struct_type (hp_type, dn_bufp, objfile);

    case DNTT_TYPE_TEMPLATE_ARG:
      {
      char * name;
      /* The TEMPLATE record points to an argument list of
       * TEMPLATE_ARG records, each of which describes one
       * of the type-arguments. 
       */
      name =  VT (objfile) + dn_bufp->dtempl_arg.name;
      return hpread_read_templ_arg_type (hp_type, dn_bufp, objfile, name);
      }

    case DNTT_TYPE_FUNC_TEMPLATE:
      /* We wind up here when processing a TEMPLATE type, 
       * if the template has member function(s).
       * Treat it like a FUNCTION.
       */
      return hpread_read_function_type (hp_type, dn_bufp, objfile, 0);

    case DNTT_TYPE_LINK:
      /* The LINK record is used to link up templates with instantiations.
       * There is no type associated with the LINK record per se.
       */
      return lookup_fundamental_type (objfile, FT_VOID);

    /* Also not yet handled... */
    /* case DNTT_TYPE_DYN_ARRAY_DESC: */
    /* case DNTT_TYPE_DESC_SUBRANGE: */
    /* case DNTT_TYPE_BEGIN_EXT: */
    /* case DNTT_TYPE_INLN: */
    /* case DNTT_TYPE_INLN_LIST: */
    /* case DNTT_TYPE_ALIAS: */
    default:
      /* A fancy way of returning NULL */
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
      else if (sl_bufp->snorm.sltdesc == SLT_NORMAL_OFFSET)
	record_line (subfile, sl_bufp->snormoff.line,
		     sl_bufp->snormoff.address + offset);
      s_idx++;
    }
  return e_idx;
}

/* Given a function "f" which is a member of a class, find
 * the classname that it is a member of. Used to construct
 * the name (e.g., "c::f") which GDB will put in the
 * "demangled name" field of the function's symbol.
 * Called from hpread_process_one_debug_symbol()
 * If "f" is not a member function, return NULL.
 */
char * class_of (functype)
struct type * functype;
{
  struct type * first_param_type;
  char * first_param_name;
  struct type * pointed_to_type;
  char * class_name;

  /* Check that the function has a first argument "this",
   * and that "this" is a pointer to a class. If not,
   * functype is not a member function, so return NULL.
   */
  if (TYPE_NFIELDS(functype) == 0)
    return NULL;
  first_param_name = TYPE_FIELD_NAME (functype, 0);
  if (first_param_name == NULL)
    return NULL; /* paranoia */
  if (strcmp(first_param_name, "this"))
    return NULL;
  first_param_type = TYPE_FIELD_TYPE (functype, 0);
  if (first_param_type == NULL)
    return NULL; /* paranoia */
  if (TYPE_CODE(first_param_type) != TYPE_CODE_PTR)
    return NULL;

  /* Get the thing that "this" points to, check that
   * it's a class, and get its class name.
   */
  pointed_to_type = TYPE_TARGET_TYPE(first_param_type);
  if (pointed_to_type == NULL) 
    return NULL; /* paranoia */
  if (TYPE_CODE(pointed_to_type) != TYPE_CODE_CLASS)
    return NULL;
  class_name = TYPE_NAME(pointed_to_type);
  if (class_name == NULL)
    return NULL; /* paranoia */

  /* The class name may be of the form "class c", in which case
   * we want to strip off the leading "class ".
   */
  if (strncmp(class_name, "class ", 6) == 0)
    class_name += 6;

  return class_name;
}

/* Internalize one native debug symbol. 
 * Called in a loop from hpread_expand_symtab(). 
 * Arguments:
 *   dn_bufp: 
 *   name: 
 *   section_offsets:
 *   objfile:
 *   text_offset: 
 *   text_size: 
 *   filename: 
 *   index: 		Index of this symbol
 *   at_module_boundary_p Pointer to boolean flag to control caller's loop.
 */

static void
hpread_process_one_debug_symbol (dn_bufp, name, section_offsets, objfile,
				 text_offset, text_size, filename,
                                 index, at_module_boundary_p
                                 )
     union dnttentry *dn_bufp;
     char *name;
     struct section_offsets *section_offsets;
     struct objfile *objfile;
     CORE_ADDR text_offset;
     int       text_size;
     char     *filename;
     int       index;
     int      *at_module_boundary_p;
{
  unsigned long desc;
  int type;
  CORE_ADDR valu;
  int offset = ANOFFSET (section_offsets, SECT_OFF_TEXT);
  int data_offset = ANOFFSET (section_offsets, SECT_OFF_DATA);
  union dnttentry *dn_temp;
  dnttpointer hp_type;
  struct symbol *sym;
  struct context_stack *new;
  char * class_scope_name;
  extern int is_in_import_list (); /* in somread.c */ 
  
  /* Allocate one GDB debug symbol and fill in some default values. */
  sym = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
					 sizeof (struct symbol));
  memset (sym, 0, sizeof (struct symbol));
  SYMBOL_NAME (sym) = obsavestring (name, strlen (name), &objfile->symbol_obstack);
  SYMBOL_LANGUAGE (sym) = language_auto;
  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
  SYMBOL_LINE (sym) = 0;
  SYMBOL_VALUE (sym) = 0;
  SYMBOL_CLASS (sym) = LOC_TYPEDEF;

  /* Just a trick in case the SOM debug symbol is a type definition.
   * There are routines that are set up to build a GDB type symbol, given
   * a SOM dnttpointer. So we set up a dummy SOM dnttpointer "hp_type".
   * This allows us to call those same routines.
   */
  hp_type.dnttp.extension = 1;
  hp_type.dnttp.immediate = 0;
  hp_type.dnttp.global = 0;
  hp_type.dnttp.index = index;

  /* This "type" is the type of SOM record.
   * Switch on SOM type.
   */
  type = dn_bufp->dblock.kind;
  switch (type)
    {
    case DNTT_TYPE_SRCFILE:
      /* This type of symbol indicates from which source file or
       * include file any following data comes. It may indicate:
       *
       * o   The start of an entirely new source file (and thus
       *     a new module)
       *
       * o   The start of a different source file due to #include
       *
       * o   The end of an include file and the return to the original
       *     file. Thus if "foo.c" includes "bar.h", we see first
       *     a SRCFILE for foo.c, then one for bar.h, and then one for
       *     foo.c again.
       *
       * If it indicates the start of a new module then we must
       * finish the symbol table of the previous module 
       * (if any) and start accumulating a new symbol table.  
       */

      valu = text_offset;
      if (!last_source_file ) {
          /*
           * A note on "last_source_file": this is a char* pointing
           * to the actual file name.  "start_symtab" sets it,
           * "end_symtab" clears it.
           *
           * So if "last_source_file" is NULL, then either this is
           * the first record we are looking at, or a previous call
           * to "end_symtab()" was made to close out the previous
           * module.  Since we're now quitting the scan loop when we
           * see a MODULE END record, we should never get here, except
           * in the case that we're not using the quick look-up tables
           * and have to use the old system as a fall-back.
           */
	  start_symtab (name, NULL, valu);
	  record_debugformat ("HP");
	  SL_INDEX (objfile) = dn_bufp->dsfile.address;
      }

      else {
          /* Either a new include file, or a SRCFILE record
           * saying we are back in the main source (or out of
           * a nested include file) again.
           */
	  SL_INDEX (objfile) = hpread_record_lines (current_subfile,
						    SL_INDEX (objfile),
						    dn_bufp->dsfile.address,
						    objfile, offset);
      }

      /* A note on "start_subfile".  This routine will check
       * the name we pass it and look for an existing subfile
       * of that name.  There's thus only one sub-file for the
       * actual source (e.g. for "foo.c" in foo.c), despite the
       * fact that we'll see lots of SRCFILE entries for foo.c
       * inside foo.c.
       */
      start_subfile (name, NULL);
      break;
      
    case DNTT_TYPE_MODULE:
      /*
       * We no longer ignore DNTT_TYPE_MODULE symbols.  The module 
       * represents the meaningful semantic structure of a compilation
       * unit.  We expect to start the psymtab-to-symtab expansion
       * looking at a MODULE entry, and to end it at the corresponding
       * END MODULE entry.
       *
       *--Begin outdated comments
       * 
       * This record signifies the start of a new source module
       * In C/C++ there is no explicit "module" construct in the language,
       * but each compilation unit is implicitly a module and they
       * do emit the DNTT_TYPE_MODULE records.
       * The end of the module is marked by a matching DNTT_TYPE_END record.
       *
       * The reason GDB gets away with ignoring the DNTT_TYPE_MODULE record 
       * is it notices the DNTT_TYPE_END record for the previous 
       * module (see comments under DNTT_TYPE_END case), and then treats
       * the next DNTT_TYPE_SRCFILE record as if it were the module-start record.
       * (i.e., it makes a start_symtab() call).
       * This scheme seems a little convoluted, but I'll leave it 
       * alone on the principle "if it ain't broke don't fix
       * it". (RT).
       *
       *-- End outdated comments
       */

      valu = text_offset;
      if (!last_source_file )
	{
          /* Start of a new module. We know this because "last_source_file"
           * is NULL, which can only happen the first time or if we just 
           * made a call to end_symtab() to close out the previous module.
           */
	  start_symtab (name, NULL, valu);
	  SL_INDEX (objfile) = dn_bufp->dmodule.address;
	}
      else
	{
          /* This really shouldn't happen if we're using the quick
           * look-up tables, as it would mean we'd scanned past an
           * END MODULE entry.  But if we're not using the tables,
           * we started the module on the SRCFILE entry, so it's ok.
           * For now, accept this.
           */
     /* warning( "Error expanding psymtab, missed module end, found entry for %s",
      *           name );
      */
          *at_module_boundary_p = -1;
	}

      start_subfile (name, NULL);
      break;

    case DNTT_TYPE_FUNCTION:
    case DNTT_TYPE_ENTRY:
      /* A function or secondary entry point.  */
      valu = dn_bufp->dfunc.lowaddr + offset;

      /* Record lines up to this point. */
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

      /* Built a type for the function. This includes processing
       * the symbol records for the function parameters.
       */
      SYMBOL_CLASS (sym) = LOC_BLOCK;
      SYMBOL_TYPE (sym) = hpread_read_function_type (hp_type, dn_bufp, objfile, 1);

      /* The "SYMBOL_NAME" field is expected to be the mangled name
       * (if any), which we get from the "alias" field of the SOM record
       * if that exists.
       */
      if ((dn_bufp->dfunc.language == HP_LANGUAGE_CPLUSPLUS) && 
          dn_bufp->dfunc.alias && /* has an alias */ 
          *(char *)(VT (objfile) + dn_bufp->dfunc.alias)) /* not a null string */ 
        SYMBOL_NAME (sym) = VT (objfile) + dn_bufp->dfunc.alias;
      else
        SYMBOL_NAME (sym) = VT (objfile) + dn_bufp->dfunc.name;

      /* Special hack to get around HP compilers' insistence on
       * reporting "main" as "_MAIN_" for C/C++ */
      if ((strcmp (SYMBOL_NAME (sym), "_MAIN_") == 0) &&
          (strcmp (VT (objfile) + dn_bufp->dfunc.name, "main") == 0))
        SYMBOL_NAME (sym) = VT (objfile) + dn_bufp->dfunc.name;
      
      /* The SYMBOL_CPLUS_DEMANGLED_NAME field is expected to
       * be the demangled name.
       */
      if (dn_bufp->dfunc.language == HP_LANGUAGE_CPLUSPLUS)
	{
        /* SYMBOL_INIT_DEMANGLED_NAME is a macro which winds up
         * calling the demangler in libiberty (cplus_demangle()) to
         * do the job. This generally does the job, even though
         * it's intended for the GNU compiler and not the aCC compiler
         * Note that SYMBOL_INIT_DEMANGLED_NAME calls the
         * demangler with arguments DMGL_PARAMS | DMGL_ANSI.
         * Generally, we don't want params when we display
         * a demangled name, but when I took out the DMGL_PARAMS,
         * some things broke, so I'm leaving it in here, and
         * working around the issue in stack.c. - RT
         */
        SYMBOL_INIT_DEMANGLED_NAME (sym, &objfile->symbol_obstack);
        if ((SYMBOL_NAME (sym) == VT (objfile) + dn_bufp->dfunc.alias) &&
           (!SYMBOL_CPLUS_DEMANGLED_NAME(sym))) {

          /* Well, the symbol name is mangled, but the
           * demangler in libiberty failed so the demangled
           * field is still NULL. Try to
           * do the job ourselves based on the "name" field
           * in the SOM record. A complication here is that
           * the name field contains only the function name
           * (like "f"), whereas we want the class qualification
           * (as in "c::f"). Try to reconstruct that.
           */          
          char * basename;
          char * classname;
          char * dem_name;
          basename = VT (objfile) + dn_bufp->dfunc.name;
          classname = class_of(SYMBOL_TYPE(sym));
          if (classname) {
            dem_name = xmalloc(strlen(basename)+strlen(classname)+3);
            strcpy(dem_name, classname);
            strcat(dem_name, "::");
            strcat(dem_name, basename);
            SYMBOL_CPLUS_DEMANGLED_NAME(sym) = dem_name;
            SYMBOL_LANGUAGE (sym) = language_cplus;
          }
        }
      }
 
      /* Add the function symbol to the list of symbols in this blockvector */
      if (dn_bufp->dfunc.global)
	add_symbol_to_list (sym, &global_symbols);
      else
	add_symbol_to_list (sym, &file_symbols);
      new->name = sym;

      /* Search forward to the next BEGIN and also read
       * in the line info up to that point. 
       * Not sure why this is needed.
       * In HP FORTRAN this code is harmful since there   
       * may not be a BEGIN after the FUNCTION.
       * So I made it C/C++ specific. - RT
       */
      if (dn_bufp->dfunc.language == HP_LANGUAGE_C ||
          dn_bufp->dfunc.language == HP_LANGUAGE_CPLUSPLUS) {
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
      }
      record_line (current_subfile, SYMBOL_LINE (sym), valu);
      break;

    case DNTT_TYPE_DOC_FUNCTION:
      valu = dn_bufp->ddocfunc.lowaddr + offset;

      /* Record lines up to this point. */
      SL_INDEX (objfile) = hpread_record_lines (current_subfile,
						SL_INDEX (objfile),
						dn_bufp->ddocfunc.address,
						objfile, offset);
      
      WITHIN_FUNCTION (objfile) = 1;
      CURRENT_FUNCTION_VALUE (objfile) = valu;
      /* Stack must be empty now.  */
      if (context_stack_depth != 0)
	complain (&lbrac_unmatched_complaint, (char *) symnum);
      new = push_context (0, valu);

      /* Built a type for the function. This includes processing
       * the symbol records for the function parameters.
       */
      SYMBOL_CLASS (sym) = LOC_BLOCK;
      SYMBOL_TYPE (sym) = hpread_read_doc_function_type (hp_type, dn_bufp, objfile, 1);

      /* The "SYMBOL_NAME" field is expected to be the mangled name
       * (if any), which we get from the "alias" field of the SOM record
       * if that exists.
       */
      if ((dn_bufp->ddocfunc.language == HP_LANGUAGE_CPLUSPLUS) && 
          dn_bufp->ddocfunc.alias && /* has an alias */ 
          *(char *)(VT (objfile) + dn_bufp->ddocfunc.alias)) /* not a null string */ 
        SYMBOL_NAME (sym) = VT (objfile) + dn_bufp->ddocfunc.alias;
      else
        SYMBOL_NAME (sym) = VT (objfile) + dn_bufp->ddocfunc.name;

      /* Special hack to get around HP compilers' insistence on
       * reporting "main" as "_MAIN_" for C/C++ */
      if ((strcmp (SYMBOL_NAME (sym), "_MAIN_") == 0) &&
          (strcmp (VT (objfile) + dn_bufp->ddocfunc.name, "main") == 0))
        SYMBOL_NAME (sym) = VT (objfile) + dn_bufp->ddocfunc.name;
      
      if (dn_bufp->ddocfunc.language == HP_LANGUAGE_CPLUSPLUS) {

        /* SYMBOL_INIT_DEMANGLED_NAME is a macro which winds up
         * calling the demangler in libiberty (cplus_demangle()) to
         * do the job. This generally does the job, even though
         * it's intended for the GNU compiler and not the aCC compiler
         * Note that SYMBOL_INIT_DEMANGLED_NAME calls the
         * demangler with arguments DMGL_PARAMS | DMGL_ANSI.
         * Generally, we don't want params when we display
         * a demangled name, but when I took out the DMGL_PARAMS,
         * some things broke, so I'm leaving it in here, and
         * working around the issue in stack.c. - RT 
         */
        SYMBOL_INIT_DEMANGLED_NAME (sym, &objfile->symbol_obstack);

        if ((SYMBOL_NAME (sym) == VT (objfile) + dn_bufp->ddocfunc.alias) &&
           (!SYMBOL_CPLUS_DEMANGLED_NAME(sym))) {

          /* Well, the symbol name is mangled, but the
           * demangler in libiberty failed so the demangled
           * field is still NULL. Try to
           * do the job ourselves based on the "name" field
           * in the SOM record. A complication here is that
           * the name field contains only the function name
           * (like "f"), whereas we want the class qualification
           * (as in "c::f"). Try to reconstruct that.
           */          
          char * basename;
          char * classname;
          char * dem_name;
          basename = VT (objfile) + dn_bufp->ddocfunc.name;
          classname = class_of(SYMBOL_TYPE(sym));
          if (classname) {
            dem_name = xmalloc(strlen(basename)+strlen(classname)+3);
            strcpy(dem_name, classname);
            strcat(dem_name, "::");
            strcat(dem_name, basename);
            SYMBOL_CPLUS_DEMANGLED_NAME(sym) = dem_name;
            SYMBOL_LANGUAGE (sym) = language_cplus;
          }
        }
      }

      /* Add the function symbol to the list of symbols in this blockvector */
      if (dn_bufp->ddocfunc.global)
	add_symbol_to_list (sym, &global_symbols);
      else
	add_symbol_to_list (sym, &file_symbols);
      new->name = sym;

      /* Search forward to the next BEGIN and also read
       * in the line info up to that point. 
       * Not sure why this is needed.
       * In HP FORTRAN this code is harmful since there   
       * may not be a BEGIN after the FUNCTION.
       * So I made it C/C++ specific. - RT
       */
      if (dn_bufp->ddocfunc.language == HP_LANGUAGE_C ||
          dn_bufp->ddocfunc.language == HP_LANGUAGE_CPLUSPLUS) {
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
      }
      record_line (current_subfile, SYMBOL_LINE (sym), valu);
      break;

    case DNTT_TYPE_BEGIN:
      /* Begin a new scope. */
      if (context_stack_depth == 1 /* this means we're at function level */ &&
          context_stack[0].name != NULL /* this means it's a function */ &&
          context_stack[0].depth == 0 /* this means it's the first BEGIN 
                                         we've seen after the FUNCTION */ 
         )
      {
        /* This is the first BEGIN after a FUNCTION.
         * We ignore this one, since HP compilers always insert
         * at least one BEGIN, i.e. it's:
         * 
         *     FUNCTION
         *     argument symbols
         *     BEGIN
         *     local symbols
         *        (possibly nested BEGIN ... END's if there are inner { } blocks)
         *     END
         *     END
         *
         * By ignoring this first BEGIN, the local symbols get treated
         * as belonging to the function scope, and "print func::local_sym"
         * works (which is what we want).
         */

        /* All we do here is increase the depth count associated with
         * the FUNCTION entry in the context stack. This ensures that
         * the next BEGIN we see (if any), representing a real nested { }
         * block, will get processed.
         */ 

         context_stack[0].depth++;

      } else { 

        /* Record lines up to this SLT pointer. */
        SL_INDEX (objfile) = hpread_record_lines (current_subfile,
                      			 	  SL_INDEX (objfile),
						  dn_bufp->dbegin.address,
						  objfile, offset);
        /* Calculate start address of new scope */
        valu = hpread_get_location (dn_bufp->dbegin.address, objfile);
        valu += offset;		/* Relocate for dynamic loading */
        /* We use the scope start DNTT index as nesting depth identifier! */
        desc = hpread_get_scope_start (dn_bufp->dbegin.address, objfile);
        new = push_context (desc, valu);
      }
      break;

    case DNTT_TYPE_END:
      /* End a scope.  */

      /* Valid end kinds are:
       *  MODULE
       *  FUNCTION
       *  WITH
       *  COMMON
       *  BEGIN
       *  CLASS_SCOPE
       */

      SL_INDEX (objfile) = hpread_record_lines (current_subfile,
						SL_INDEX (objfile),
						dn_bufp->dend.address,
						objfile, offset);
      switch (dn_bufp->dend.endkind)
	{
	case DNTT_TYPE_MODULE:
	  /* Ending a module ends the symbol table for that module.  
           * Calling end_symtab() has the side effect of clearing the
           * last_source_file pointer, which in turn signals 
           * process_one_debug_symbol() to treat the next DNTT_TYPE_SRCFILE
           * record as a module-begin.
           */
	  valu = text_offset + text_size + offset;

          /* Tell our caller that we're done with expanding the
           * debug information for a module.
           */
          *at_module_boundary_p = 1;

          /* Don't do this, as our caller will do it!
           *
           *	  (void) end_symtab (valu, objfile, 0);
           */
	  break;

	case DNTT_TYPE_FUNCTION:
	  /* Ending a function, well, ends the function's scope.  */
	  dn_temp = hpread_get_lntt (dn_bufp->dend.beginscope.dnttp.index,
				     objfile);
	  valu = dn_temp->dfunc.hiaddr + offset;
          /* Insert func params into local list */ 
          merge_symbol_lists (&param_symbols, &local_symbols);
	  new = pop_context ();
	  /* Make a block for the local symbols within.  */
	  finish_block (new->name, &local_symbols, new->old_blocks,
			new->start_addr, valu, objfile);
	  WITHIN_FUNCTION (objfile) = 0; /* This may have to change for Pascal */ 
          local_symbols = new->locals;
          param_symbols = new->params;
	  break;

	case DNTT_TYPE_BEGIN:
          if (context_stack_depth == 1 &&
              context_stack[0].name != NULL &&
              context_stack[0].depth == 1)         
          {
            /* This is the END corresponding to the
             * BEGIN which we ignored - see DNTT_TYPE_BEGIN case above.
             */
            context_stack[0].depth--;
          } else {
	    /* Ending a local scope.  */
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
	    param_symbols = new->params;
          }
	  break;

        case DNTT_TYPE_WITH:
          /* Since we ignore the DNTT_TYPE_WITH that starts the scope,
           * we can ignore the DNTT_TYPE_END that ends it.
           */
	  break;

        case DNTT_TYPE_COMMON:
          /* End a FORTRAN common block. We don't currently handle these */
          complain (&hpread_unhandled_end_common_complaint);
	  break;

        case DNTT_TYPE_CLASS_SCOPE: 

         /* pai: FIXME Not handling nested classes for now -- must
           * maintain a stack */
          class_scope_name = NULL;

#if 0
          /* End a class scope */
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
	  param_symbols = new->params;
#endif
	  break;

        default:
          complain (&hpread_unexpected_end_complaint);
          break;
	}
      break;

    /* DNTT_TYPE_IMPORT is not handled */

    case DNTT_TYPE_LABEL:
      SYMBOL_NAMESPACE (sym) = LABEL_NAMESPACE;
      break;

    case DNTT_TYPE_FPARAM:
      /* Function parameters.  */
      /* Note 1: This code was present in the 4.16 sources, and then
         removed, because fparams are handled in
         hpread_read_function_type().  However, while fparam symbols
         are indeed handled twice, this code here cannot be removed
         because then they don't get added to the local symbol list of
         the function's code block, which leads to a failure to look
         up locals, "this"-relative member names, etc.  So I've put
         this code back in. pai/1997-07-21 */
      /* Note 2: To fix a defect, we stopped adding FPARAMS to local_symbols
         in hpread_read_function_type(), so FPARAMS had to be handled
         here.  I changed the location to be the appropriate argument
         kinds rather than LOC_LOCAL. pai/1997-08-08 */
      /* Note 3: Well, the fix in Note 2 above broke argument printing
         in traceback frames, and further it makes assumptions about the
         order of the FPARAM entries from HP compilers (cc and aCC in particular
         generate them in reverse orders -- fixing one breaks for the other).
         So I've added code in hpread_read_function_type() to add fparams
         to a param_symbols list for the current context level.  These are
         then merged into local_symbols when a function end is reached.
         pai/1997-08-11 */ 
         
      break; /* do nothing; handled in hpread_read_function_type() */ 
      
#if 0 /* Old code */ 
      if (dn_bufp->dfparam.regparam)
        SYMBOL_CLASS (sym) = LOC_REGISTER;
      else if (dn_bufp->dfparam.indirect)
	SYMBOL_CLASS (sym) = LOC_REF_ARG;
      else
	SYMBOL_CLASS (sym) = LOC_ARG;
      SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
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
      add_symbol_to_list (sym, &fparam_symbols);
      break;
#endif

    case DNTT_TYPE_SVAR:
      /* Static variables.  */
      SYMBOL_CLASS (sym) = LOC_STATIC; 

      /* Note: There is a case that arises with globals in shared
       * libraries where we need to set the address to LOC_INDIRECT.
       * This case is if you have a global "g" in one library, and
       * it is referenced "extern <type> g;" in another library.
       * If we're processing the symbols for the referencing library,
       * we'll see a global "g", but in this case the address given
       * in the symbol table contains a pointer to the real "g".
       * We use the storage class LOC_INDIRECT to indicate this. RT
       */
      if (is_in_import_list (SYMBOL_NAME(sym), objfile))
        SYMBOL_CLASS (sym) = LOC_INDIRECT;

      SYMBOL_VALUE_ADDRESS (sym) = dn_bufp->dsvar.location + data_offset;
      SYMBOL_TYPE (sym) = hpread_type_lookup (dn_bufp->dsvar.type, objfile);

      if (dn_bufp->dsvar.global)
	add_symbol_to_list (sym, &global_symbols);
        
      else if (WITHIN_FUNCTION (objfile))
	add_symbol_to_list (sym, &local_symbols);
        
      else
	add_symbol_to_list (sym, &file_symbols);

      if (dn_bufp->dsvar.thread_specific)
        {
          /* Thread-local variable.
           */
          SYMBOL_CLASS (sym)   = LOC_THREAD_LOCAL_STATIC;
          SYMBOL_BASEREG (sym) = CR27_REGNUM;

          if( objfile->flags & OBJF_SHARED ) {
              /*
               * This variable is not only thread local but
               * in a shared library.
               *
               * Alas, the shared lib structures are private
               * to "somsolib.c".  But C lets us point to one.
               */
              struct so_list *so;

              if( objfile->obj_private == NULL )
                  error( "Internal error in reading shared library information." );

              so = ((obj_private_data_t *)(objfile->obj_private))->so_info;
              if( so == NULL )
                  error( "Internal error in reading shared library information." );

              /* Thread-locals in shared libraries do NOT have the
               * standard offset ("data_offset"), so we re-calculate
               * where to look for this variable, using a call-back
               * to interpret the private shared-library data.
               */
              SYMBOL_VALUE_ADDRESS(sym) = dn_bufp->dsvar.location +
                                             so_lib_thread_start_addr( so );
          }
        }
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
      /* A typedef. We do want to process these, since a name is
       * added to the namespace for the typedef'ed name.
       */
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
      {
      int global = dn_bufp->dtag.global;
      /* Structure, union, enum, template, or class tag definition */
      /* We do want to process these, since a name is
       * added to the namespace for the tag name (and if C++ class,
       * for the typename also).
       */
      SYMBOL_NAMESPACE (sym) = STRUCT_NAMESPACE;

      /* The tag contains in its "type" field a pointer to the
       * DNTT_TYPE_STRUCT, DNTT_TYPE_UNION, DNTT_TYPE_ENUM, 
       * DNTT_TYPE_CLASS or DNTT_TYPE_TEMPLATE
       * record that actually defines the type.
       */
      SYMBOL_TYPE (sym) = hpread_type_lookup (dn_bufp->dtype.type, objfile);
      TYPE_NAME (sym->type) = SYMBOL_NAME (sym);
      TYPE_TAG_NAME (sym->type) = SYMBOL_NAME (sym);
      if (dn_bufp->dtag.global)
	add_symbol_to_list (sym, &global_symbols);
      else if (WITHIN_FUNCTION (objfile))
	add_symbol_to_list (sym, &local_symbols);
      else
	add_symbol_to_list (sym, &file_symbols);

      /* If this is a C++ class, then we additionally 
       * need to define a typedef for the
       * class type. E.g., so that the name "c" becomes visible as
       * a type name when the user says "class c { ... }".
       * In order to figure this out, we need to chase down the "type"
       * field to get to the DNTT_TYPE_CLASS record. 
       *
       * We also add the typename for ENUM. Though this isn't
       * strictly correct, it is necessary because of the debug info
       * generated by the aCC compiler, in which we cannot
       * distinguish between:
       *   enum e { ... };
       * and
       *   typedef enum { ... } e;
       * I.e., the compiler emits the same debug info for the above
       * two cases, in both cases "e" appearing as a tagdef.
       * Therefore go ahead and generate the typename so that
       * "ptype e" will work in the above cases.
       *
       * We also add the typename for TEMPLATE, so as to allow "ptype t"
       * when "t" is a template name. 
       */
      if (dn_bufp->dtype.type.dnttp.index < LNTT_SYMCOUNT (objfile))
        dn_bufp = hpread_get_lntt (dn_bufp->dtag.type.dnttp.index, objfile);
      else {
        complain (&hpread_tagdef_complaint);
        return;
      }
      if (dn_bufp->dblock.kind == DNTT_TYPE_CLASS ||
          dn_bufp->dblock.kind == DNTT_TYPE_ENUM ||
          dn_bufp->dblock.kind == DNTT_TYPE_TEMPLATE) {
        struct symbol *newsym;
        
        newsym = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
	             				  sizeof (struct symbol));
        memset (newsym, 0, sizeof (struct symbol));
        SYMBOL_NAME (newsym) = name;
        SYMBOL_LANGUAGE (newsym) = language_auto;
        SYMBOL_NAMESPACE (newsym) = VAR_NAMESPACE;
        SYMBOL_LINE (newsym) = 0;
        SYMBOL_VALUE (newsym) = 0;
        SYMBOL_CLASS (newsym) = LOC_TYPEDEF;
        SYMBOL_TYPE (newsym) = sym->type;
        if (global)
  	  add_symbol_to_list (newsym, &global_symbols);
        else if (WITHIN_FUNCTION (objfile))
  	  add_symbol_to_list (newsym, &local_symbols);
        else
  	  add_symbol_to_list (newsym, &file_symbols);
      }
      }
      break;

    case DNTT_TYPE_POINTER:
      /* Declares a pointer type. Should not be necessary to do anything
       * with the type at this level; these are processed
       * at the hpread_type_lookup() level. 
       */
      break;

    case DNTT_TYPE_ENUM:
      /* Declares an enum type. Should not be necessary to do anything
       * with the type at this level; these are processed
       * at the hpread_type_lookup() level. 
       */
      break;

    case DNTT_TYPE_MEMENUM:
      /* Member of enum */
      /* Ignored at this level, but hpread_read_enum_type() will take
       * care of walking the list of enumeration members.
       */
      break;

    case DNTT_TYPE_SET:
      /* Declares a set type. Should not be necessary to do anything
       * with the type at this level; these are processed
       * at the hpread_type_lookup() level. 
       */
      break;

    case DNTT_TYPE_SUBRANGE:
      /* Declares a subrange type. Should not be necessary to do anything
       * with the type at this level; these are processed
       * at the hpread_type_lookup() level. 
       */
      break;

    case DNTT_TYPE_ARRAY:
      /* Declares an array type. Should not be necessary to do anything
       * with the type at this level; these are processed
       * at the hpread_type_lookup() level. 
       */
      break;

    case DNTT_TYPE_STRUCT:
    case DNTT_TYPE_UNION:
      /* Declares an struct/union type. 
       * Should not be necessary to do anything
       * with the type at this level; these are processed
       * at the hpread_type_lookup() level. 
       */
      break;

    case DNTT_TYPE_FIELD:
      /* Structure/union/class field */
      /* Ignored at this level, but hpread_read_struct_type() will take
       * care of walking the list of structure/union/class members.
       */
      break;

    /* DNTT_TYPE_VARIANT is not handled by GDB */

    /* DNTT_TYPE_FILE is not handled by GDB */

    case DNTT_TYPE_FUNCTYPE:
      /* Function type */
      /* Ignored at this level, handled within hpread_type_lookup() */
      break;

    case DNTT_TYPE_WITH:
      /* This is emitted within methods to indicate "with <class>" 
       * scoping rules (i.e., indicate that the class data members
       * are directly visible).
       * However, since GDB already infers this by looking at the
       * "this" argument, interpreting the DNTT_TYPE_WITH 
       * symbol record is unnecessary.
       */
      break;

    case DNTT_TYPE_COMMON:
      /* FORTRAN common. Not yet handled. */
      complain (&hpread_unhandled_common_complaint);
      break;

    /* DNTT_TYPE_COBSTRUCT is not handled by GDB.  */
    /* DNTT_TYPE_XREF is not handled by GDB.  */
    /* DNTT_TYPE_SA is not handled by GDB.  */
    /* DNTT_TYPE_MACRO is not handled by GDB */

    case DNTT_TYPE_BLOCKDATA:
      /* Not sure what this is - part of FORTRAN support maybe? 
       * Anyway, not yet handled.
       */
      complain (&hpread_unhandled_blockdata_complaint);
      break;

    case DNTT_TYPE_CLASS_SCOPE:



      /* The compiler brackets member functions with a CLASS_SCOPE/END
       * pair of records, presumably to put them in a different scope
       * from the module scope where they are normally defined.
       * E.g., in the situation:
       *   void f() { ... }
       *   void c::f() { ...}
       * The member function "c::f" will be bracketed by a CLASS_SCOPE/END.
       * This causes "break f" at the module level to pick the
       * the file-level function f(), not the member function
       * (which needs to be referenced via "break c::f"). 
       * 
       * Here we record the class name to generate the demangled names of
       * member functions later.
       *
       * FIXME Not being used now for anything -- cplus_demangle seems
       * enough for getting the class-qualified names of functions. We
       * may need this for handling nested classes and types.  */

      /* pai: FIXME Not handling nested classes for now -- need to
       * maintain a stack */

      dn_temp = hpread_get_lntt (dn_bufp->dclass_scope.type.dnttp.index, objfile);
      if (dn_temp->dblock.kind == DNTT_TYPE_TAGDEF)
        class_scope_name = VT (objfile) + dn_temp->dtag.name;
      else
        class_scope_name = NULL;

#if 0

      /* Begin a new scope.  */
      SL_INDEX (objfile) = hpread_record_lines (current_subfile,
						SL_INDEX (objfile),
						dn_bufp->dclass_scope.address,
						objfile, offset);
      valu = hpread_get_location (dn_bufp->dclass_scope.address, objfile);
      valu += offset;		/* Relocate for dynamic loading */
      desc = hpread_get_scope_start (dn_bufp->dclass_scope.address, objfile);
      /* We use the scope start DNTT index as the nesting depth identifier! */
      new = push_context (desc, valu);
#endif
      break;

    case DNTT_TYPE_REFERENCE:
      /* Declares a C++ reference type. Should not be necessary to do anything
       * with the type at this level; these are processed
       * at the hpread_type_lookup() level.
       */
      break;

    case DNTT_TYPE_PTRMEM:
      /* Declares a C++ pointer-to-data-member type. This does not
       * need to be handled at this level; being a type description it
       * is instead handled at the hpread_type_lookup() level.
       */
      break;

    case DNTT_TYPE_PTRMEMFUNC:
      /* Declares a C++ pointer-to-function-member type. This does not
       * need to be handled at this level; being a type description it
       * is instead handled at the hpread_type_lookup() level.
       */
      break;

    case DNTT_TYPE_CLASS:
      /* Declares a class type. 
       * Should not be necessary to do anything
       * with the type at this level; these are processed
       * at the hpread_type_lookup() level. 
       */
      break;

    case DNTT_TYPE_GENFIELD:
      /* I believe this is used for class member functions */
      /* Ignored at this level, but hpread_read_struct_type() will take
       * care of walking the list of class members.
       */
      break;

    case DNTT_TYPE_VFUNC:
      /* Virtual function */
      /* This does not have to be handled at this level; handled in
       * the course of processing class symbols.
       */
      break;

    case DNTT_TYPE_MEMACCESS:
      /* DDE ignores this symbol table record.
       * It has something to do with "modified access" to class members.
       * I'll assume we can safely ignore it too.
       */
      break;

    case DNTT_TYPE_INHERITANCE:
      /* These don't have to be handled here, since they are handled
       * within hpread_read_struct_type() in the process of constructing
       * a class type.
       */
      break;

    case DNTT_TYPE_FRIEND_CLASS:
    case DNTT_TYPE_FRIEND_FUNC:
      /* These can safely be ignored, as GDB doesn't need this
       * info. DDE only uses it in "describe". We may later want
       * to extend GDB's "ptype" to give this info, but for now
       * it seems safe enough to ignore it.
       */  
      break;

    case DNTT_TYPE_MODIFIER:
      /* Intended to supply "modified access" to a type */
      /* From the way DDE handles this, it looks like it always
       * modifies a type. Therefore it is safe to ignore it at this
       * level, and handle it in hpread_type_lookup().
       */
      break;

    case DNTT_TYPE_OBJECT_ID:
      /* Just ignore this - that's all DDE does */
      break;

    case DNTT_TYPE_MEMFUNC:
      /* Member function */
      /* This does not have to be handled at this level; handled in
       * the course of processing class symbols.
       */
      break;

    case DNTT_TYPE_DOC_MEMFUNC:
      /* Member function */
      /* This does not have to be handled at this level; handled in
       * the course of processing class symbols.
       */
      break;

    case DNTT_TYPE_TEMPLATE:
      /* Template - sort of the header for a template definition,
       * which like a class, points to a member list and also points
       * to a TEMPLATE_ARG list of type-arguments.
       * We do not need to process TEMPLATE records at this level though.
       */
      break;

    case DNTT_TYPE_TEMPLATE_ARG:
      /* The TEMPLATE record points to an argument list of
       * TEMPLATE_ARG records, each of which describes one
       * of the type-arguments.
       * We do not need to process TEMPLATE_ARG records at this level though.
       */
      break;

    case DNTT_TYPE_FUNC_TEMPLATE:
      /* This will get emitted for member functions of templates.
       * But we don't need to process this record at this level though,
       * we will process it in the course of processing a TEMPLATE
       * record.
       */
      break;

    case DNTT_TYPE_LINK:
      /* The LINK record is used to link up templates with instantiations. */
      /* It is not clear why this is needed, and furthermore aCC does
       * not appear to generate this, so I think we can safely ignore it. - RT
       */
      break;

    /* DNTT_TYPE_DYN_ARRAY_DESC is not handled by GDB */
    /* DNTT_TYPE_DESC_SUBRANGE is not handled by GDB */
    /* DNTT_TYPE_BEGIN_EXT is not handled by GDB */
    /* DNTT_TYPE_INLN is not handled by GDB */
    /* DNTT_TYPE_INLN_LIST is not handled by GDB */
    /* DNTT_TYPE_ALIAS is not handled by GDB */

    default:
      break;
    }
}

/* Get nesting depth for a DNTT entry.
 * DN_BUFP points to a DNTT entry.
 * OBJFILE is the object file.
 * REPORT_NESTED is a flag; if 0, real nesting depth is
 * reported, if it is 1, the function simply returns a 
 * non-zero value if the nesting depth is anything > 0.
 * 
 * Return value is an integer.  0 => not a local type / name
 * positive return => type or name is local to some 
 * block or function.
 */ 


/* elz: ATTENTION: FIXME: NOTE: WARNING!!!!
   this function now returns 0 right away. It was taking too much time
   at start up. Now, though, the local types are not handled correctly.
*/


static int
hpread_get_scope_depth (dn_bufp, objfile, report_nested)
  union dnttentry * dn_bufp;
  struct objfile * objfile;
  int  report_nested;
{
  register int index;
  register union dnttentry * dn_tmp;
  register short depth = 0;
/****************************/
  return 0;
/****************************/

  index = (((char *) dn_bufp) - LNTT (objfile)) / (sizeof (struct dntt_type_block));
  
  while (--index >= 0)
    {
      dn_tmp = hpread_get_lntt (index, objfile);
      switch (dn_tmp->dblock.kind)
        {
          case DNTT_TYPE_MODULE:
            return depth;
          case DNTT_TYPE_END:
            /* index is signed int; dnttp.index is 29-bit unsigned int! */
            index = (int) dn_tmp->dend.beginscope.dnttp.index;
            break;
          case DNTT_TYPE_BEGIN:
          case DNTT_TYPE_FUNCTION:
          case DNTT_TYPE_DOC_FUNCTION:
          case DNTT_TYPE_WITH:   
          case DNTT_TYPE_COMMON: 
          case DNTT_TYPE_CLASS_SCOPE:
            depth++;
            if (report_nested)
              return 1;
            break;
          default:
            break;
        }
    }
  return depth;
}

/* Adjust the bitoffsets for all fields of an anonymous union of
   type TYPE by negative BITS.  This handles HP aCC's hideous habit
   of giving members of anonymous unions bit offsets relative to the
   enclosing structure instead of relative to the union itself. */  

static void
hpread_adjust_bitoffsets (type, bits)
  struct type * type;
  int bits;
{
  register int i;

  /* This is done only for unions; caller had better check that
     it is an anonymous one. */ 
  if (TYPE_CODE (type) != TYPE_CODE_UNION)
    return;

  /* Adjust each field; since this is a union, there are no base
     classes. Also no static membes.  Also, no need for recursion as
     the members of this union if themeselves structs or unions, have
     the correct bitoffsets; if an anonymous union is a member of this
     anonymous union, the code in hpread_read_struct_type() will
     adjust for that. */

  for (i = 0; i < TYPE_NFIELDS (type); i++)
    TYPE_FIELD_BITPOS (type, i) -= bits;
}

/* Because of quirks in HP compilers' treatment of anonymous unions inside
   classes, we have to chase through a chain of threaded FIELD entries.
   If we encounter an anonymous union in the chain, we must recursively skip over
   that too.

   This function does a "next" in the chain of FIELD entries, but transparently
   skips over anonymous unions' fields (recursively).
   
   Inputs are the number of times to do "next" at the top level, the dnttpointer
   (FIELD) and entry pointer (FIELDP) for the dntt record corresponding to it,
   and the ubiquitous objfile parameter. (Note: FIELDP is a **.)  Return value
   is a dnttpointer for the new field after all the skipped ones */

static dnttpointer
hpread_get_next_skip_over_anon_unions (skip_fields, field, fieldp, objfile)
  int skip_fields;
  dnttpointer field;
  union dnttentry ** fieldp;
  struct objfile * objfile;
{
  struct type * anon_type;
  register int i;
  int bitoffset;
  char * name;

  for (i=0; i < skip_fields; i++)
    {
      /* Get type of item we're looking at now; recursively processes the types
         of these intermediate items we skip over, so they aren't lost. */
      anon_type = hpread_type_lookup ((*fieldp)->dfield.type, objfile);
      anon_type = CHECK_TYPEDEF (anon_type);
      bitoffset = (*fieldp)->dfield.bitoffset;
      name = VT (objfile) + (*fieldp)->dfield.name;
      /* First skip over one item to avoid stack death on recursion */ 
      field = (*fieldp)->dfield.nextfield;
      *fieldp = hpread_get_lntt (field.dnttp.index, objfile);
      /* Do we have another anonymous union? If so, adjust the bitoffsets
         of its members and skip over its members. */
      if ((TYPE_CODE (anon_type) == TYPE_CODE_UNION) &&
          (!name || STREQ (name, "")))
        {
          hpread_adjust_bitoffsets (anon_type, bitoffset);
          field = hpread_get_next_skip_over_anon_unions (TYPE_NFIELDS (anon_type), field, fieldp, objfile);
        }
    }
  return field;
}


