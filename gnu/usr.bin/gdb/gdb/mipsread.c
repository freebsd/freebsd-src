/* Read a symbol table in MIPS' format (Third-Eye).
   Copyright 1986, 1987, 1989, 1990, 1991, 1992, 1993 Free Software
   Foundation, Inc.
   Contributed by Alessandro Forin (af@cs.cmu.edu) at CMU.  Major work
   by Per Bothner, John Gilmore and Ian Lance Taylor at Cygnus Support.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* This module provides three functions: mipscoff_symfile_init,
   which initializes to read a symbol file; mipscoff_new_init, which
   discards existing cached information when all symbols are being
   discarded; and mipscoff_symfile_read, which reads a symbol table
   from a file.

   mipscoff_symfile_read only does the minimum work necessary for letting the
   user "name" things symbolically; it does not read the entire symtab.
   Instead, it reads the external and static symbols and puts them in partial
   symbol tables.  When more extensive information is requested of a
   file, the corresponding partial symbol table is mutated into a full
   fledged symbol table by going back and reading the symbols
   for real.  mipscoff_psymtab_to_symtab() is called indirectly through
   a pointer in the psymtab to do this.

   ECOFF symbol tables are mostly written in the byte order of the
   target machine.  However, one section of the table (the auxiliary
   symbol information) is written in the host byte order.  There is a
   bit in the other symbol info which describes which host byte order
   was used.  ECOFF thereby takes the trophy from Intel `b.out' for
   the most brain-dead adaptation of a file format to byte order.

   This module can read all four of the known byte-order combinations,
   on any type of host.  */

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "obstack.h"
#include "buildsym.h"
#include "stabsread.h"
#include "complaints.h"

/* These are needed if the tm.h file does not contain the necessary
   mips specific definitions.  */

#ifndef MIPS_EFI_SYMBOL_NAME
#define MIPS_EFI_SYMBOL_NAME "__GDB_EFI_INFO__"
#include "coff/sym.h"
#include "coff/symconst.h"
typedef struct mips_extra_func_info {
        long    numargs;
        PDR     pdr;
} *mips_extra_func_info_t;
#ifndef RA_REGNUM
#define RA_REGNUM 0
#endif
#endif

#ifdef USG
#include <sys/types.h>
#endif

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <string.h>

#include "gdb-stabs.h"

#include "bfd.h"

#include "coff/internal.h"
#include "coff/ecoff.h"		/* COFF-like aspects of ecoff files */

/* FIXME: coff/internal.h and aout/aout64.h both define N_ABS.  We
   want the definition from aout/aout64.h.  */
#undef	N_ABS

#include "libaout.h"		/* Private BFD a.out information.  */
#include "aout/aout64.h"
#include "aout/stab_gnu.h"	/* STABS information */

/* FIXME: libcoff.h and libaout.h both define a couple of macros.  We
   don't use them.  */
#undef exec_hdr
#undef obj_sym_filepos

#include "libcoff.h"		/* Private BFD COFF information.  */
#include "libecoff.h"		/* Private BFD ECOFF information.  */

#include "expression.h"
#include "language.h"		/* Needed inside partial-stab.h */

/* Provide a default mapping from a ecoff register number to a gdb REGNUM.  */
#ifndef ECOFF_REG_TO_REGNUM
#define ECOFF_REG_TO_REGNUM(num) (num)
#endif

/* Information is passed among various mipsread routines for accessing
   symbol files.  A pointer to this structure is kept in the sym_private
   field of the objfile struct.  */

struct ecoff_symfile_info {
  struct mips_pending **pending_list;
};
#define ECOFF_SYMFILE_INFO(o)	((struct ecoff_symfile_info *)((o)->sym_private))
#define ECOFF_PENDING_LIST(o)	(ECOFF_SYMFILE_INFO(o)->pending_list)
 

/* Each partial symbol table entry contains a pointer to private data
   for the read_symtab() function to use when expanding a partial
   symbol table entry to a full symbol table entry.

   For mipsread this structure contains the index of the FDR that this
   psymtab represents and a pointer to the BFD that the psymtab was
   created from.  */

#define PST_PRIVATE(p) ((struct symloc *)(p)->read_symtab_private)
#define FDR_IDX(p) (PST_PRIVATE(p)->fdr_idx)
#define CUR_BFD(p) (PST_PRIVATE(p)->cur_bfd)

struct symloc
{
  int fdr_idx;
  bfd *cur_bfd;
  EXTR *extern_tab;		/* Pointer to external symbols for this file. */
  int extern_count;		/* Size of extern_tab. */
  enum language pst_language;
};

/* Things we import explicitly from other modules */

extern int info_verbose;

/* Various complaints about symbol reading that don't abort the process */

struct complaint bad_file_number_complaint =
{"bad file number %d", 0, 0};

struct complaint index_complaint =
{"bad aux index at symbol %s", 0, 0};

struct complaint aux_index_complaint =
{"bad proc end in aux found from symbol %s", 0, 0};

struct complaint block_index_complaint =
{"bad aux index at block symbol %s", 0, 0};

struct complaint unknown_ext_complaint =
{"unknown external symbol %s", 0, 0};

struct complaint unknown_sym_complaint =
{"unknown local symbol %s", 0, 0};

struct complaint unknown_st_complaint =
{"with type %d", 0, 0};

struct complaint block_overflow_complaint =
{"block containing %s overfilled", 0, 0};

struct complaint basic_type_complaint =
{"cannot map MIPS basic type 0x%x for %s", 0, 0};

struct complaint unknown_type_qual_complaint =
{"unknown type qualifier 0x%x", 0, 0};

struct complaint array_index_type_complaint =
{"illegal array index type for %s, assuming int", 0, 0};

struct complaint bad_tag_guess_complaint =
{"guessed tag type of %s incorrectly", 0, 0};

struct complaint block_member_complaint =
{"declaration block contains unhandled symbol type %d", 0, 0};

struct complaint stEnd_complaint =
{"stEnd with storage class %d not handled", 0, 0};

struct complaint unknown_mips_symtype_complaint =
{"unknown symbol type 0x%x", 0, 0};

struct complaint stab_unknown_complaint =
{"unknown stabs symbol %s", 0, 0};

struct complaint pdr_for_nonsymbol_complaint =
{"PDR for %s, but no symbol", 0, 0};

struct complaint pdr_static_symbol_complaint =
{"can't handle PDR for static proc at 0x%lx", 0, 0};

struct complaint bad_setjmp_pdr_complaint =
{"fixing bad setjmp PDR from libc", 0, 0};

struct complaint bad_fbitfield_complaint =
{"can't handle TIR fBitfield for %s", 0, 0};

struct complaint bad_continued_complaint =
{"illegal TIR continued for %s", 0, 0};

struct complaint bad_rfd_entry_complaint =
{"bad rfd entry for %s: file %d, index %d", 0, 0};

struct complaint unexpected_type_code_complaint =
{"unexpected type code for %s", 0, 0};

struct complaint unable_to_cross_ref_complaint =
{"unable to cross ref btTypedef for %s", 0, 0};

struct complaint illegal_forward_tq0_complaint =
{"illegal tq0 in forward typedef for %s", 0, 0};

struct complaint illegal_forward_bt_complaint =
{"illegal bt %d in forward typedef for %s", 0, 0};

struct complaint bad_linetable_guess_complaint =
{"guessed size of linetable for %s incorrectly", 0, 0};

/* Macros and extra defs */

/* Puns: hard to find whether -g was used and how */

#define MIN_GLEVEL GLEVEL_0
#define compare_glevel(a,b)					\
	(((a) == GLEVEL_3) ? ((b) < GLEVEL_3) :			\
	 ((b) == GLEVEL_3) ? -1 : (int)((b) - (a)))

/* Things that really are local to this module */

/* Remember what we deduced to be the source language of this psymtab. */

static enum language psymtab_language = language_unknown;

/* Current BFD.  */

static bfd *cur_bfd;

/* Pointer to current file decriptor record, and its index */

static FDR *cur_fdr;
static int cur_fd;

/* Index of current symbol */

static int cur_sdx;

/* Note how much "debuggable" this image is.  We would like
   to see at least one FDR with full symbols */

static max_gdbinfo;
static max_glevel;

/* When examining .o files, report on undefined symbols */

static int n_undef_symbols, n_undef_labels, n_undef_vars, n_undef_procs;

/* Pseudo symbol to use when putting stabs into the symbol table.  */

static char stabs_symbol[] = STABS_SYMBOL;

/* Extra builtin types */

struct type *builtin_type_complex;
struct type *builtin_type_double_complex;
struct type *builtin_type_fixed_dec;
struct type *builtin_type_float_dec;
struct type *builtin_type_string;

/* Forward declarations */

static void
read_mips_symtab PARAMS ((struct objfile *, struct section_offsets *));

static void
read_the_mips_symtab PARAMS ((bfd *));

static int
upgrade_type PARAMS ((int, struct type **, int, union aux_ext *, int, char *));

static void
parse_partial_symbols PARAMS ((struct objfile *,
			       struct section_offsets *));

static int
cross_ref PARAMS ((int, union aux_ext *, struct type **, enum type_code,
		   char **, int, char *));

static void
fixup_sigtramp PARAMS ((void));

static struct symbol *
new_symbol PARAMS ((char *));

static struct type *
new_type PARAMS ((char *));

static struct block *
new_block PARAMS ((int));

static struct symtab *
new_symtab PARAMS ((char *, int, int, struct objfile *));

static struct linetable *
new_linetable PARAMS ((int));

static struct blockvector *
new_bvect PARAMS ((int));

static int
parse_symbol PARAMS ((SYMR *, union aux_ext *, char *, int));

static struct type *
parse_type PARAMS ((int, union aux_ext *, unsigned int, int *, int, char *));

static struct symbol *
mylookup_symbol PARAMS ((char *, struct block *, enum namespace,
			 enum address_class));

static struct block *
shrink_block PARAMS ((struct block *, struct symtab *));

static PTR
xzalloc PARAMS ((unsigned int));

static void
sort_blocks PARAMS ((struct symtab *));

static int
compare_blocks PARAMS ((const void *, const void *));

static struct partial_symtab *
new_psymtab PARAMS ((char *, struct objfile *));

static void
psymtab_to_symtab_1 PARAMS ((struct partial_symtab *, char *));

static void
add_block PARAMS ((struct block *, struct symtab *));

static void
add_symbol PARAMS ((struct symbol *, struct block *));

static int
add_line PARAMS ((struct linetable *, int, CORE_ADDR, int));

static struct linetable *
shrink_linetable PARAMS ((struct linetable *));

static char *
mips_next_symbol_text PARAMS ((void));

/* Things we export to other modules */

/* Address bounds for the signal trampoline in inferior, if any */
/* FIXME:  Nothing really seems to use this.  Why is it here? */

CORE_ADDR sigtramp_address, sigtramp_end;

static void
mipscoff_new_init (ignore)
     struct objfile *ignore;
{
  sigtramp_address = 0;
  stabsread_new_init ();
  buildsym_new_init ();
}

static void
mipscoff_symfile_init (objfile)
     struct objfile *objfile;
{
  if (objfile->sym_private != NULL)
    {
      mfree (objfile->md, objfile->sym_private);
    }
  objfile->sym_private = (PTR)
    xmmalloc (objfile->md, sizeof (struct ecoff_symfile_info));
}

static void
mipscoff_symfile_read (objfile, section_offsets, mainline)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     int mainline;
{
  struct cleanup * back_to;

  init_minimal_symbol_collection ();
  back_to = make_cleanup (discard_minimal_symbols, 0);

  /* Now that the executable file is positioned at symbol table,
     process it and define symbols accordingly.  */

  read_mips_symtab (objfile, section_offsets);

  /* Install any minimal symbols that have been collected as the current
     minimal symbols for this objfile. */

  install_minimal_symbols (objfile);

  do_cleanups (back_to);
}

/* Perform any local cleanups required when we are done with a particular
   objfile.  I.E, we are in the process of discarding all symbol information
   for an objfile, freeing up all memory held for it, and unlinking the
   objfile struct from the global list of known objfiles. */

static void
mipscoff_symfile_finish (objfile)
     struct objfile *objfile;
{
  if (objfile->sym_private != NULL)
    {
      mfree (objfile->md, objfile->sym_private);
    }

  cur_bfd = 0;
}

/* Allocate zeroed memory */

static PTR
xzalloc (size)
     unsigned int size;
{
  PTR p = xmalloc (size);

  memset (p, 0, size);
  return p;
}

/* Exported procedure: Builds a symtab from the PST partial one.
   Restores the environment in effect when PST was created, delegates
   most of the work to an ancillary procedure, and sorts
   and reorders the symtab list at the end */

static void
mipscoff_psymtab_to_symtab (pst)
     struct partial_symtab *pst;
{

  if (!pst)
    return;

  if (info_verbose)
    {
      printf_filtered ("Reading in symbols for %s...", pst->filename);
      fflush (stdout);
    }

  next_symbol_text_func = mips_next_symbol_text;

  psymtab_to_symtab_1 (pst, pst->filename);

  /* Match with global symbols.  This only needs to be done once,
     after all of the symtabs and dependencies have been read in.   */
  scan_file_globals (pst->objfile);

  if (info_verbose)
    printf_filtered ("done.\n");
}

/* Exported procedure: Is PC in the signal trampoline code */

int
in_sigtramp (pc, ignore)
     CORE_ADDR pc;
     char *ignore;		/* function name */
{
  if (sigtramp_address == 0)
    fixup_sigtramp ();
  return (pc >= sigtramp_address && pc < sigtramp_end);
}

/* File-level interface functions */

/* Read the symtab information from file ABFD into memory.  */

static void
read_the_mips_symtab (abfd)
     bfd *abfd;
{
  if (ecoff_slurp_symbolic_info (abfd) == false)
    error ("Error reading symbol table: %s", bfd_errmsg (bfd_error));
}

/* Find a file descriptor given its index RF relative to a file CF */

static FDR *
get_rfd (cf, rf)
     int cf, rf;
{
  FDR *fdrs;
  register FDR *f;
  RFDT rfd;

  fdrs = ecoff_data (cur_bfd)->fdr;
  f = fdrs + cf;
  /* Object files do not have the RFD table, all refs are absolute */
  if (f->rfdBase == 0)
    return fdrs + rf;
  (*ecoff_backend (cur_bfd)->swap_rfd_in)
    (cur_bfd,
     ((char *) ecoff_data (cur_bfd)->external_rfd
      + (f->rfdBase + rf) * ecoff_backend (cur_bfd)->external_rfd_size),
     &rfd);
  return fdrs + rfd;
}

/* Return a safer print NAME for a file descriptor */

static char *
fdr_name (f)
     FDR *f;
{
  if (f->rss == -1)
    return "<stripped file>";
  if (f->rss == 0)
    return "<NFY>";
  return ecoff_data (cur_bfd)->ss + f->issBase + f->rss;
}


/* Read in and parse the symtab of the file OBJFILE.  Symbols from
   different sections are relocated via the SECTION_OFFSETS.  */

static void
read_mips_symtab (objfile, section_offsets)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
{
  cur_bfd = objfile->obfd;

  read_the_mips_symtab (objfile->obfd);

  parse_partial_symbols (objfile, section_offsets);

#if 0
  /* Check to make sure file was compiled with -g.  If not, warn the
     user of this limitation.  */
  if (compare_glevel (max_glevel, GLEVEL_2) < 0)
    {
      if (max_gdbinfo == 0)
	printf ("\n%s not compiled with -g, debugging support is limited.\n",
		 objfile->name);
      printf ("You should compile with -g2 or -g3 for best debugging support.\n");
      fflush (stdout);
    }
#endif
}

/* Local utilities */

/* Map of FDR indexes to partial symtabs */

struct pst_map
{
  struct partial_symtab *pst;	/* the psymtab proper */
  long n_globals;		/* exported globals (external symbols) */
  long globals_offset;		/* cumulative */
};


/* Utility stack, used to nest procedures and blocks properly.
   It is a doubly linked list, to avoid too many alloc/free.
   Since we might need it quite a few times it is NOT deallocated
   after use. */

static struct parse_stack
{
  struct parse_stack *next, *prev;
  struct symtab *cur_st;	/* Current symtab. */
  struct block *cur_block;	/* Block in it. */
  int blocktype;		/* What are we parsing. */
  int maxsyms;			/* Max symbols in this block. */
  struct type *cur_type;	/* Type we parse fields for. */
  int cur_field;		/* Field number in cur_type. */
  CORE_ADDR procadr;		/* Start addres of this procedure */
  int numargs;			/* Its argument count */
}

 *top_stack;			/* Top stack ptr */


/* Enter a new lexical context */

static void
push_parse_stack ()
{
  struct parse_stack *new;

  /* Reuse frames if possible */
  if (top_stack && top_stack->prev)
    new = top_stack->prev;
  else
    new = (struct parse_stack *) xzalloc (sizeof (struct parse_stack));
  /* Initialize new frame with previous content */
  if (top_stack)
    {
      register struct parse_stack *prev = new->prev;

      *new = *top_stack;
      top_stack->prev = new;
      new->prev = prev;
      new->next = top_stack;
    }
  top_stack = new;
}

/* Exit a lexical context */

static void
pop_parse_stack ()
{
  if (!top_stack)
    return;
  if (top_stack->next)
    top_stack = top_stack->next;
}


/* Cross-references might be to things we haven't looked at
   yet, e.g. type references.  To avoid too many type
   duplications we keep a quick fixup table, an array
   of lists of references indexed by file descriptor */

struct mips_pending
{
  struct mips_pending *next;	/* link */
  char *s;			/* the unswapped symbol */
  struct type *t;		/* its partial type descriptor */
};


/* Check whether we already saw symbol SH in file FH */

static struct mips_pending *
is_pending_symbol (fh, sh)
     FDR *fh;
     char *sh;
{
  int f_idx = fh - ecoff_data (cur_bfd)->fdr;
  register struct mips_pending *p;
  struct mips_pending **pending_list = ECOFF_PENDING_LIST (current_objfile);

  /* Linear search is ok, list is typically no more than 10 deep */
  for (p = pending_list[f_idx]; p; p = p->next)
    if (p->s == sh)
      break;
  return p;
}

/* Add a new symbol SH of type T */

static void
add_pending (fh, sh, t)
     FDR *fh;
     char *sh;
     struct type *t;
{
  int f_idx = fh - ecoff_data (cur_bfd)->fdr;
  struct mips_pending *p = is_pending_symbol (fh, sh);

  /* Make sure we do not make duplicates */
  if (!p)
    {
      struct mips_pending **pending_list = ECOFF_PENDING_LIST (current_objfile);

      p = ((struct mips_pending *)
	   obstack_alloc (&current_objfile->psymbol_obstack,
			  sizeof (struct mips_pending)));
      p->s = sh;
      p->t = t;
      p->next = pending_list[f_idx];
      pending_list[f_idx] = p;
    }
}


/* Parsing Routines proper. */

/* Parse a single symbol. Mostly just make up a GDB symbol for it.
   For blocks, procedures and types we open a new lexical context.
   This is basically just a big switch on the symbol's type.  Argument
   AX is the base pointer of aux symbols for this file (fh->iauxBase).
   EXT_SH points to the unswapped symbol, which is needed for struct,
   union, etc., types; it is NULL for an EXTR.  BIGEND says whether
   aux symbols are big-endian or little-endian.  Return count of
   SYMR's handled (normally one).  */

static int
parse_symbol (sh, ax, ext_sh, bigend)
     SYMR *sh;
     union aux_ext *ax;
     char *ext_sh;
     int bigend;
{
  const bfd_size_type external_sym_size
    = ecoff_backend (cur_bfd)->external_sym_size;
  void (* const swap_sym_in) PARAMS ((bfd *, PTR, SYMR *)) =
    ecoff_backend (cur_bfd)->swap_sym_in;
  char *name;
  struct symbol *s;
  struct block *b;
  struct mips_pending *pend;
  struct type *t;
  struct field *f;
  int count = 1;
  enum address_class class;
  TIR tir;
  long svalue = sh->value;
  int bitsize;

  if (ext_sh == (char *) NULL)
    name = ecoff_data (cur_bfd)->ssext + sh->iss;
  else
    name = ecoff_data (cur_bfd)->ss + cur_fdr->issBase + sh->iss;

  switch (sh->st)
    {
    case stNil:
      break;

    case stGlobal:		/* external symbol, goes into global block */
      class = LOC_STATIC;
      b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (top_stack->cur_st),
			     GLOBAL_BLOCK);
      s = new_symbol (name);
      SYMBOL_VALUE_ADDRESS (s) = (CORE_ADDR) sh->value;
      goto data;

    case stStatic:		/* static data, goes into current block. */
      class = LOC_STATIC;
      b = top_stack->cur_block;
      s = new_symbol (name);
      if (sh->sc == scCommon)
	{
	  /* It is a FORTRAN common block.  At least for SGI Fortran the
	     address is not in the symbol; we need to fix it later in
	     scan_file_globals.  */
	  int bucket = hashname (SYMBOL_NAME (s));
	  SYMBOL_VALUE_CHAIN (s) = global_sym_chain[bucket];
	  global_sym_chain[bucket] = s;
	}
      else
	SYMBOL_VALUE_ADDRESS (s) = (CORE_ADDR) sh->value;
      goto data;

    case stLocal:		/* local variable, goes into current block */
      if (sh->sc == scRegister)
	{
	  class = LOC_REGISTER;
	  svalue = ECOFF_REG_TO_REGNUM (svalue);
	}
      else
	class = LOC_LOCAL;
      b = top_stack->cur_block;
      s = new_symbol (name);
      SYMBOL_VALUE (s) = svalue;

    data:			/* Common code for symbols describing data */
      SYMBOL_NAMESPACE (s) = VAR_NAMESPACE;
      SYMBOL_CLASS (s) = class;
      add_symbol (s, b);

      /* Type could be missing in a number of cases */
      if (sh->sc == scUndefined || sh->sc == scNil ||
	  sh->index == 0xfffff)
	SYMBOL_TYPE (s) = builtin_type_int;	/* undefined? */
      else
	SYMBOL_TYPE (s) = parse_type (cur_fd, ax, sh->index, 0, bigend, name);
      /* Value of a data symbol is its memory address */
      break;

    case stParam:		/* arg to procedure, goes into current block */
      max_gdbinfo++;
      top_stack->numargs++;

      /* Special GNU C++ name.  */
      if (name[0] == CPLUS_MARKER && name[1] == 't' && name[2] == 0)
	name = "this";		/* FIXME, not alloc'd in obstack */
      s = new_symbol (name);

      SYMBOL_NAMESPACE (s) = VAR_NAMESPACE;
      switch (sh->sc)
	{
	case scRegister:
	  /* Pass by value in register.  */
	  SYMBOL_CLASS(s) = LOC_REGPARM;
	  svalue = ECOFF_REG_TO_REGNUM (svalue);
	  break;
	case scVar:
	  /* Pass by reference on stack.  */
	  SYMBOL_CLASS(s) = LOC_REF_ARG;
	  break;
	case scVarRegister:
	  /* Pass by reference in register.  */
	  SYMBOL_CLASS(s) = LOC_REGPARM_ADDR;
	  svalue = ECOFF_REG_TO_REGNUM (svalue);
	  break;
	default:
	  /* Pass by value on stack.  */
	  SYMBOL_CLASS(s) = LOC_ARG;
	  break;
	}
      SYMBOL_VALUE (s) = svalue;
      SYMBOL_TYPE (s) = parse_type (cur_fd, ax, sh->index, 0, bigend, name);
      add_symbol (s, top_stack->cur_block);
#if 0
      /* FIXME:  This has not been tested.  See dbxread.c */
      /* Add the type of this parameter to the function/procedure
		   type of this block. */
      add_param_to_type (&top_stack->cur_block->function->type, s);
#endif
      break;

    case stLabel:		/* label, goes into current block */
      s = new_symbol (name);
      SYMBOL_NAMESPACE (s) = VAR_NAMESPACE;	/* so that it can be used */
      SYMBOL_CLASS (s) = LOC_LABEL;	/* but not misused */
      SYMBOL_VALUE_ADDRESS (s) = (CORE_ADDR) sh->value;
      SYMBOL_TYPE (s) = builtin_type_int;
      add_symbol (s, top_stack->cur_block);
      break;

    case stProc:		/* Procedure, usually goes into global block */
    case stStaticProc:		/* Static procedure, goes into current block */
      s = new_symbol (name);
      SYMBOL_NAMESPACE (s) = VAR_NAMESPACE;
      SYMBOL_CLASS (s) = LOC_BLOCK;
      /* Type of the return value */
      if (sh->sc == scUndefined || sh->sc == scNil)
	t = builtin_type_int;
      else
	t = parse_type (cur_fd, ax, sh->index + 1, 0, bigend, name);
      b = top_stack->cur_block;
      if (sh->st == stProc)
	{
	  struct blockvector *bv = BLOCKVECTOR (top_stack->cur_st);
	  /* The next test should normally be true,
		       but provides a hook for nested functions
		       (which we don't want to make global). */
	  if (b == BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK))
	    b = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	}
      add_symbol (s, b);

      /* Make a type for the procedure itself */
#if 0
      /* FIXME:  This has not been tested yet!  See dbxread.c */
      /* Generate a template for the type of this function.  The
	 types of the arguments will be added as we read the symbol
	 table. */
      memcpy (lookup_function_type (t), SYMBOL_TYPE (s), sizeof (struct type));
#else
      SYMBOL_TYPE (s) = lookup_function_type (t);
#endif

      /* Create and enter a new lexical context */
      b = new_block (top_stack->maxsyms);
      SYMBOL_BLOCK_VALUE (s) = b;
      BLOCK_FUNCTION (b) = s;
      BLOCK_START (b) = BLOCK_END (b) = sh->value;
      BLOCK_SUPERBLOCK (b) = top_stack->cur_block;
      add_block (b, top_stack->cur_st);

      /* Not if we only have partial info */
      if (sh->sc == scUndefined || sh->sc == scNil)
	break;

      push_parse_stack ();
      top_stack->cur_block = b;
      top_stack->blocktype = sh->st;
      top_stack->cur_type = SYMBOL_TYPE (s);
      top_stack->cur_field = -1;
      top_stack->procadr = sh->value;
      top_stack->numargs = 0;
      break;

      /* Beginning of code for structure, union, and enum definitions.
	       They all share a common set of local variables, defined here.  */
      {
	enum type_code type_code;
	char *ext_tsym;
	int nfields;
	long max_value;
	struct field *f;

    case stStruct:		/* Start a block defining a struct type */
	type_code = TYPE_CODE_STRUCT;
	goto structured_common;

    case stUnion:		/* Start a block defining a union type */
	type_code = TYPE_CODE_UNION;
	goto structured_common;

    case stEnum:		/* Start a block defining an enum type */
	type_code = TYPE_CODE_ENUM;
	goto structured_common;

    case stBlock:		/* Either a lexical block, or some type */
	if (sh->sc != scInfo && sh->sc != scCommon)
	  goto case_stBlock_code;	/* Lexical block */

	type_code = TYPE_CODE_UNDEF;	/* We have a type.  */

	/* Common code for handling struct, union, enum, and/or as-yet-
	   unknown-type blocks of info about structured data.  `type_code'
	   has been set to the proper TYPE_CODE, if we know it.  */
      structured_common:
	push_parse_stack ();
	top_stack->blocktype = stBlock;

	/* First count the number of fields and the highest value. */
	nfields = 0;
	max_value = 0;
	for (ext_tsym = ext_sh + external_sym_size;
	     ;
	     ext_tsym += external_sym_size)
	  {
	    SYMR tsym;

	    (*swap_sym_in) (cur_bfd, ext_tsym, &tsym);

	    switch (tsym.st)
	      {
	      case stEnd:
		goto end_of_fields;

	      case stMember:
		if (nfields == 0 && type_code == TYPE_CODE_UNDEF)
		  /* If the type of the member is Nil (or Void),
		     without qualifiers, assume the tag is an
		     enumeration. */
		  if (tsym.index == indexNil)
		    type_code = TYPE_CODE_ENUM;
		  else
		    {
		      ecoff_swap_tir_in (bigend,
					 &ax[tsym.index].a_ti,
					 &tir);
		      if ((tir.bt == btNil || tir.bt == btVoid)
			  && tir.tq0 == tqNil)
			type_code = TYPE_CODE_ENUM;
		    }
		nfields++;
		if (tsym.value > max_value)
		  max_value = tsym.value;
		break;

	      case stBlock:
	      case stUnion:
	      case stEnum:
	      case stStruct:
		{
#if 0
		  /* This is a no-op; is it trying to tell us something
		     we should be checking?  */
		  if (tsym.sc == scVariant);	/*UNIMPLEMENTED*/
#endif
		  if (tsym.index != 0)
		    {
		      /* This is something like a struct within a
			 struct.  Skip over the fields of the inner
			 struct.  The -1 is because the for loop will
			 increment ext_tsym.  */
		      ext_tsym = ((char *) ecoff_data (cur_bfd)->external_sym
				  + ((cur_fdr->isymBase + tsym.index - 1)
				     * external_sym_size));
		    }
		}
		break;

	      case stTypedef:
		/* mips cc puts out a typedef for struct x if it is not yet
		   defined when it encounters
		   struct y { struct x *xp; };
		   Just ignore it. */
		break;

	      default:
		complain (&block_member_complaint, tsym.st);
	      }
	  }
      end_of_fields:;

	/* In an stBlock, there is no way to distinguish structs,
	   unions, and enums at this point.  This is a bug in the
	   original design (that has been fixed with the recent
	   addition of the stStruct, stUnion, and stEnum symbol
	   types.)  The way you can tell is if/when you see a variable
	   or field of that type.  In that case the variable's type
	   (in the AUX table) says if the type is struct, union, or
	   enum, and points back to the stBlock here.  So you can
	   patch the tag kind up later - but only if there actually is
	   a variable or field of that type.

	   So until we know for sure, we will guess at this point.
	   The heuristic is:
	   If the first member has index==indexNil or a void type,
	   assume we have an enumeration.
	   Otherwise, if there is more than one member, and all
	   the members have offset 0, assume we have a union.
	   Otherwise, assume we have a struct.

	   The heuristic could guess wrong in the case of of an
	   enumeration with no members or a union with one (or zero)
	   members, or when all except the last field of a struct have
	   width zero.  These are uncommon and/or illegal situations,
	   and in any case guessing wrong probably doesn't matter
	   much.

	   But if we later do find out we were wrong, we fixup the tag
	   kind.  Members of an enumeration must be handled
	   differently from struct/union fields, and that is harder to
	   patch up, but luckily we shouldn't need to.  (If there are
	   any enumeration members, we can tell for sure it's an enum
	   here.) */

	if (type_code == TYPE_CODE_UNDEF)
	  if (nfields > 1 && max_value == 0)
	    type_code = TYPE_CODE_UNION;
	  else
	    type_code = TYPE_CODE_STRUCT;

	/* Create a new type or use the pending type.  */
	pend = is_pending_symbol (cur_fdr, ext_sh);
	if (pend == (struct mips_pending *) NULL)
	  {
	    t = new_type (NULL);
	    add_pending (cur_fdr, ext_sh, t);
	  }
	else
	  t = pend->t;

	/* Alpha cc unnamed structs do not get a tag name.  */
	if (sh->iss == 0)
	  TYPE_TAG_NAME (t) = NULL;
	else
	  TYPE_TAG_NAME (t) = obconcat (&current_objfile->symbol_obstack,
					"", "", name);

	TYPE_CODE (t) = type_code;
	TYPE_LENGTH (t) = sh->value;
	TYPE_NFIELDS (t) = nfields;
	TYPE_FIELDS (t) = f = ((struct field *)
			       TYPE_ALLOC (t,
					   nfields * sizeof (struct field)));

	if (type_code == TYPE_CODE_ENUM)
	  {
	    /* This is a non-empty enum. */
	    for (ext_tsym = ext_sh + external_sym_size;
		 ;
		 ext_tsym += external_sym_size)
	      {
		SYMR tsym;
		struct symbol *enum_sym;

		(*swap_sym_in) (cur_bfd, ext_tsym, &tsym);

		if (tsym.st != stMember)
		  break;

		f->bitpos = tsym.value;
		f->type = t;
		f->name = (ecoff_data (cur_bfd)->ss
			   + cur_fdr->issBase
			   + tsym.iss);
		f->bitsize = 0;

		enum_sym = ((struct symbol *)
			    obstack_alloc (&current_objfile->symbol_obstack,
					   sizeof (struct symbol)));
		memset ((PTR) enum_sym, 0, sizeof (struct symbol));
		SYMBOL_NAME (enum_sym) = f->name;
		SYMBOL_CLASS (enum_sym) = LOC_CONST;
		SYMBOL_TYPE (enum_sym) = t;
		SYMBOL_NAMESPACE (enum_sym) = VAR_NAMESPACE;
		SYMBOL_VALUE (enum_sym) = tsym.value;
		add_symbol (enum_sym, top_stack->cur_block);

		/* Skip the stMembers that we've handled. */
		count++;
		f++;
	      }
	  }
	/* make this the current type */
	top_stack->cur_type = t;
	top_stack->cur_field = 0;

	/* Do not create a symbol for alpha cc unnamed structs.  */
	if (sh->iss == 0)
	  break;
	s = new_symbol (name);
	SYMBOL_NAMESPACE (s) = STRUCT_NAMESPACE;
	SYMBOL_CLASS (s) = LOC_TYPEDEF;
	SYMBOL_VALUE (s) = 0;
	SYMBOL_TYPE (s) = t;

	/* gcc puts out an empty struct for an opaque struct definitions.  */
	if (TYPE_NFIELDS (t) == 0)
	  {
	    TYPE_FLAGS (t) |= TYPE_FLAG_STUB;
	    SYMBOL_NAMESPACE (s) = VAR_NAMESPACE;
	  }
	add_symbol (s, top_stack->cur_block);
	break;

	/* End of local variables shared by struct, union, enum, and
	   block (as yet unknown struct/union/enum) processing.  */
      }

    case_stBlock_code:
      /* beginnning of (code) block. Value of symbol
	 is the displacement from procedure start */
      push_parse_stack ();
      top_stack->blocktype = stBlock;
      b = new_block (top_stack->maxsyms);
      BLOCK_START (b) = sh->value + top_stack->procadr;
      BLOCK_SUPERBLOCK (b) = top_stack->cur_block;
      top_stack->cur_block = b;
      add_block (b, top_stack->cur_st);
      break;

    case stEnd:		/* end (of anything) */
      if (sh->sc == scInfo || sh->sc == scCommon)
	{
	  /* Finished with type */
	  top_stack->cur_type = 0;
	}
      else if (sh->sc == scText &&
	       (top_stack->blocktype == stProc ||
		top_stack->blocktype == stStaticProc))
	{
	  /* Finished with procedure */
	  struct blockvector *bv = BLOCKVECTOR (top_stack->cur_st);
	  struct mips_extra_func_info *e;
	  struct block *b;
	  int i;

	  BLOCK_END (top_stack->cur_block) += sh->value;	/* size */

	  /* Make up special symbol to contain procedure specific info */
	  s = new_symbol (MIPS_EFI_SYMBOL_NAME);
	  SYMBOL_NAMESPACE (s) = LABEL_NAMESPACE;
	  SYMBOL_CLASS (s) = LOC_CONST;
	  SYMBOL_TYPE (s) = builtin_type_void;
	  e = ((struct mips_extra_func_info *)
	       obstack_alloc (&current_objfile->symbol_obstack,
			      sizeof (struct mips_extra_func_info)));
	  SYMBOL_VALUE (s) = (long) e;
	  e->numargs = top_stack->numargs;
	  add_symbol (s, top_stack->cur_block);

	  /* Reallocate symbols, saving memory */
	  b = shrink_block (top_stack->cur_block, top_stack->cur_st);

	  /* f77 emits proc-level with address bounds==[0,0],
	     So look for such child blocks, and patch them.  */
	  for (i = 0; i < BLOCKVECTOR_NBLOCKS (bv); i++)
	    {
	      struct block *b_bad = BLOCKVECTOR_BLOCK (bv, i);
	      if (BLOCK_SUPERBLOCK (b_bad) == b
		  && BLOCK_START (b_bad) == top_stack->procadr
		  && BLOCK_END (b_bad) == top_stack->procadr)
		{
		  BLOCK_START (b_bad) = BLOCK_START (b);
		  BLOCK_END (b_bad) = BLOCK_END (b);
		}
	    }
	}
      else if (sh->sc == scText && top_stack->blocktype == stBlock)
	{
	  /* End of (code) block. The value of the symbol is the
	     displacement from the procedure`s start address of the
	     end of this block. */
	  BLOCK_END (top_stack->cur_block) = sh->value + top_stack->procadr;
	  shrink_block (top_stack->cur_block, top_stack->cur_st);
	}
      else if (sh->sc == scText && top_stack->blocktype == stFile)
	{
	  /* End of file.  Pop parse stack and ignore.  Higher
	     level code deals with this.  */
	  ;
	}
      else
	complain (&stEnd_complaint, sh->sc);

      pop_parse_stack ();	/* restore previous lexical context */
      break;

    case stMember:		/* member of struct or union */
      f = &TYPE_FIELDS (top_stack->cur_type)[top_stack->cur_field++];
      f->name = name;
      f->bitpos = sh->value;
      bitsize = 0;
      f->type = parse_type (cur_fd, ax, sh->index, &bitsize, bigend, name);
      f->bitsize = bitsize;
      break;

    case stTypedef:		/* type definition */
      /* Typedefs for forward declarations and opaque structs from alpha cc
	 are handled by cross_ref, skip them.  */
      if (sh->iss == 0)
	break;

      /* Parse the type or use the pending type.  */
      pend = is_pending_symbol (cur_fdr, ext_sh);
      if (pend == (struct mips_pending *) NULL)
	{
	  t = parse_type (cur_fd, ax, sh->index, (int *)NULL, bigend, name);
	  add_pending (cur_fdr, ext_sh, t);
	}
      else
	t = pend->t;

      /* mips cc puts out a typedef with the name of the struct for forward
	 declarations. These should not go into the symbol table and
	 TYPE_NAME should not be set for them.
	 They can't be distinguished from an intentional typedef to
	 the same name however:
	 x.h:
		struct x { int ix; int jx; };
		struct xx;
	 x.c:
		typedef struct x x;
		struct xx {int ixx; int jxx; };
	 generates a cross referencing stTypedef for x and xx.
	 The user visible effect of this is that the type of a pointer
	 to struct foo sometimes is given as `foo *' instead of `struct foo *'.
	 The problem is fixed with alpha cc.  */

      s = new_symbol (name);
      SYMBOL_NAMESPACE (s) = VAR_NAMESPACE;
      SYMBOL_CLASS (s) = LOC_TYPEDEF;
      SYMBOL_BLOCK_VALUE (s) = top_stack->cur_block;
      SYMBOL_TYPE (s) = t;
      add_symbol (s, top_stack->cur_block);

      /* Incomplete definitions of structs should not get a name.  */
      if (TYPE_NAME (SYMBOL_TYPE (s)) == NULL
	  && (TYPE_NFIELDS (SYMBOL_TYPE (s)) != 0
              || (TYPE_CODE (SYMBOL_TYPE (s)) != TYPE_CODE_STRUCT
		  && TYPE_CODE (SYMBOL_TYPE (s)) != TYPE_CODE_UNION)))
	{
	  if (TYPE_CODE (SYMBOL_TYPE (s)) == TYPE_CODE_PTR
	      || TYPE_CODE (SYMBOL_TYPE (s)) == TYPE_CODE_FUNC)
	    {
	      /* If we are giving a name to a type such as "pointer to
		 foo" or "function returning foo", we better not set
		 the TYPE_NAME.  If the program contains "typedef char
		 *caddr_t;", we don't want all variables of type char
		 * to print as caddr_t.  This is not just a
		 consequence of GDB's type management; CC and GCC (at
		 least through version 2.4) both output variables of
		 either type char * or caddr_t with the type
		 refering to the stTypedef symbol for caddr_t.  If a future
		 compiler cleans this up it GDB is not ready for it
		 yet, but if it becomes ready we somehow need to
		 disable this check (without breaking the PCC/GCC2.4
		 case).

		 Sigh.

		 Fortunately, this check seems not to be necessary
		 for anything except pointers or functions.  */
	    }
	  else
	    TYPE_NAME (SYMBOL_TYPE (s)) = SYMBOL_NAME (s);
	}
      break;

    case stFile:		/* file name */
      push_parse_stack ();
      top_stack->blocktype = sh->st;
      break;

      /* I`ve never seen these for C */
    case stRegReloc:
      break;			/* register relocation */
    case stForward:
      break;			/* forwarding address */
    case stConstant:
      break;			/* constant */
    default:
      complain (&unknown_mips_symtype_complaint, sh->st);
      break;
    }

  return count;
}

/* Parse the type information provided in the raw AX entries for
   the symbol SH. Return the bitfield size in BS, in case.
   We must byte-swap the AX entries before we use them; BIGEND says whether
   they are big-endian or little-endian (from fh->fBigendian).  */

static struct type *
parse_type (fd, ax, aux_index, bs, bigend, sym_name)
     int fd;
     union aux_ext *ax;
     unsigned int aux_index;
     int *bs;
     int bigend;
     char *sym_name;
{
  /* Null entries in this map are treated specially */
  static struct type **map_bt[] =
  {
    &builtin_type_void,		/* btNil */
    0,				/* btAdr */
    &builtin_type_char,		/* btChar */
    &builtin_type_unsigned_char,/* btUChar */
    &builtin_type_short,	/* btShort */
    &builtin_type_unsigned_short,	/* btUShort */
    &builtin_type_int,		/* btInt */
    &builtin_type_unsigned_int,	/* btUInt */
    &builtin_type_long,		/* btLong */
    &builtin_type_unsigned_long,/* btULong */
    &builtin_type_float,	/* btFloat */
    &builtin_type_double,	/* btDouble */
    0,				/* btStruct */
    0,				/* btUnion */
    0,				/* btEnum */
    0,				/* btTypedef */
    0,				/* btRange */
    0,				/* btSet */
    &builtin_type_complex,	/* btComplex */
    &builtin_type_double_complex,	/* btDComplex */
    0,				/* btIndirect */
    &builtin_type_fixed_dec,	/* btFixedDec */
    &builtin_type_float_dec,	/* btFloatDec */
    &builtin_type_string,	/* btString */
    0,				/* btBit */
    0,				/* btPicture */
    &builtin_type_void,		/* btVoid */
    0,				/* DEC C++:  Pointer to member */
    0,				/* DEC C++:  Virtual function table */
    0,				/* DEC C++:  Class (Record) */
    &builtin_type_long,		/* btLong64  */
    &builtin_type_unsigned_long, /* btULong64 */
    &builtin_type_long_long,	/* btLongLong64  */
    &builtin_type_unsigned_long_long, /* btULongLong64 */
    &builtin_type_unsigned_long, /* btAdr64 */
    &builtin_type_long,		/* btInt64  */
    &builtin_type_unsigned_long, /* btUInt64 */
  };

  TIR t[1];
  struct type *tp = 0;
  enum type_code type_code = TYPE_CODE_UNDEF;

  /* Handle corrupt aux indices.  */
  if (aux_index >= (ecoff_data (cur_bfd)->fdr + fd)->caux)
    {
      complain (&index_complaint, sym_name);
      return builtin_type_int;
    }
  ax += aux_index;

  /* Use aux as a type information record, map its basic type.  */
  ecoff_swap_tir_in (bigend, &ax->a_ti, t);
  if (t->bt >= (sizeof (map_bt) / sizeof (*map_bt)))
    {
      complain (&basic_type_complaint, t->bt, sym_name);
      return builtin_type_int;
    }
  if (map_bt[t->bt])
    {
      tp = *map_bt[t->bt];
    }
  else
    {
      tp = NULL;
      /* Cannot use builtin types -- build our own */
      switch (t->bt)
	{
	case btAdr:
	  tp = lookup_pointer_type (builtin_type_void);
	  break;
	case btStruct:
	  type_code = TYPE_CODE_STRUCT;
	  break;
	case btUnion:
	  type_code = TYPE_CODE_UNION;
	  break;
	case btEnum:
	  type_code = TYPE_CODE_ENUM;
	  break;
	case btRange:
	  type_code = TYPE_CODE_RANGE;
	  break;
	case btSet:
	  type_code = TYPE_CODE_SET;
	  break;
	case btTypedef:
	  /* alpha cc uses this for typedefs. The true type will be
	     obtained by crossreferencing below.  */
	  type_code = TYPE_CODE_ERROR;
	  break;
	default:
	  complain (&basic_type_complaint, t->bt, sym_name);
	  return builtin_type_int;
	}
    }

  /* Move on to next aux */
  ax++;

  if (t->fBitfield)
    {
      /* Inhibit core dumps with some cfront generated objects that
	 corrupt the TIR.  */
      if (bs == (int *)NULL)
	{
	  complain (&bad_fbitfield_complaint, sym_name);
	  return builtin_type_int;
	}
      *bs = AUX_GET_WIDTH (bigend, ax);
      ax++;
    }

  /* All these types really point to some (common) MIPS type
     definition, and only the type-qualifiers fully identify
     them.  We'll make the same effort at sharing. */
  if (t->bt == btStruct ||
      t->bt == btUnion ||
      t->bt == btEnum ||

      /* btSet (I think) implies that the name is a tag name, not a typedef
	 name.  This apparently is a MIPS extension for C sets.  */
      t->bt == btSet)
    {
      char *name;

      /* Try to cross reference this type, build new type on failure.  */
      ax += cross_ref (fd, ax, &tp, type_code, &name, bigend, sym_name);
      if (tp == (struct type *) NULL)
	tp = init_type (type_code, 0, 0, (char *) NULL, current_objfile);

      /* Make sure that TYPE_CODE(tp) has an expected type code.
	 Any type may be returned from cross_ref if file indirect entries
	 are corrupted.  */
      if (TYPE_CODE (tp) != TYPE_CODE_STRUCT
	  && TYPE_CODE (tp) != TYPE_CODE_UNION
	  && TYPE_CODE (tp) != TYPE_CODE_ENUM)
	{
	  complain (&unexpected_type_code_complaint, sym_name);
	}
      else
	{

	  /* Usually, TYPE_CODE(tp) is already type_code.  The main
	     exception is if we guessed wrong re struct/union/enum.
	     But for struct vs. union a wrong guess is harmless, so
	     don't complain().  */
	  if ((TYPE_CODE (tp) == TYPE_CODE_ENUM
	       && type_code != TYPE_CODE_ENUM)
	      || (TYPE_CODE (tp) != TYPE_CODE_ENUM
		  && type_code == TYPE_CODE_ENUM))
	    {
	      complain (&bad_tag_guess_complaint, sym_name);
	    }

	  if (TYPE_CODE (tp) != type_code)
	    {
	      TYPE_CODE (tp) = type_code;
	    }

	  /* Do not set the tag name if it is a compiler generated tag name
	      (.Fxx or .xxfake or empty) for unnamed struct/union/enums.  */
	  if (name[0] == '.' || name[0] == '\0')
	    TYPE_TAG_NAME (tp) = NULL;
	  else if (TYPE_TAG_NAME (tp) == NULL
		   || !STREQ (TYPE_TAG_NAME (tp), name))
	    TYPE_TAG_NAME (tp) = obsavestring (name, strlen (name),
					       &current_objfile->type_obstack);
	}
    }

  /* All these types really point to some (common) MIPS type
     definition, and only the type-qualifiers fully identify
     them.  We'll make the same effort at sharing.
     FIXME: btIndirect cannot happen here as it is handled by the
     switch t->bt above.  And we are not doing any guessing on range types.  */
  if (t->bt == btIndirect ||
      t->bt == btRange)
    {
      char *name;

      /* Try to cross reference this type, build new type on failure.  */
      ax += cross_ref (fd, ax, &tp, type_code, &name, bigend, sym_name);
      if (tp == (struct type *) NULL)
	tp = init_type (type_code, 0, 0, (char *) NULL, current_objfile);

      /* Make sure that TYPE_CODE(tp) has an expected type code.
	 Any type may be returned from cross_ref if file indirect entries
	 are corrupted.  */
      if (TYPE_CODE (tp) != TYPE_CODE_RANGE)
	{
	  complain (&unexpected_type_code_complaint, sym_name);
	}
      else
	{
	  /* Usually, TYPE_CODE(tp) is already type_code.  The main
	     exception is if we guessed wrong re struct/union/enum. */
	  if (TYPE_CODE (tp) != type_code)
	    {
	      complain (&bad_tag_guess_complaint, sym_name);
	      TYPE_CODE (tp) = type_code;
	    }
	  if (TYPE_NAME (tp) == NULL || !STREQ (TYPE_NAME (tp), name))
	    TYPE_NAME (tp) = obsavestring (name, strlen (name),
					   &current_objfile->type_obstack);
	}
    }
  if (t->bt == btTypedef)
    {
      char *name;

      /* Try to cross reference this type, it should succeed.  */
      ax += cross_ref (fd, ax, &tp, type_code, &name, bigend, sym_name);
      if (tp == (struct type *) NULL)
	{
	  complain (&unable_to_cross_ref_complaint, sym_name);
	  tp = builtin_type_int;
	}
    }

  /* Deal with range types */
  if (t->bt == btRange)
    {
      TYPE_NFIELDS (tp) = 2;
      TYPE_FIELDS (tp) = ((struct field *)
			  TYPE_ALLOC (tp, 2 * sizeof (struct field)));
      TYPE_FIELD_NAME (tp, 0) = obsavestring ("Low", strlen ("Low"),
					      &current_objfile->type_obstack);
      TYPE_FIELD_BITPOS (tp, 0) = AUX_GET_DNLOW (bigend, ax);
      ax++;
      TYPE_FIELD_NAME (tp, 1) = obsavestring ("High", strlen ("High"),
					      &current_objfile->type_obstack);
      TYPE_FIELD_BITPOS (tp, 1) = AUX_GET_DNHIGH (bigend, ax);
      ax++;
    }

  /* Parse all the type qualifiers now. If there are more
     than 6 the game will continue in the next aux */

  while (1)
    {
#define PARSE_TQ(tq) \
      if (t->tq != tqNil) \
	ax += upgrade_type(fd, &tp, t->tq, ax, bigend, sym_name); \
      else \
	break;

      PARSE_TQ (tq0);
      PARSE_TQ (tq1);
      PARSE_TQ (tq2);
      PARSE_TQ (tq3);
      PARSE_TQ (tq4);
      PARSE_TQ (tq5);
#undef	PARSE_TQ

      /* mips cc 2.x and gcc never put out continued aux entries.  */
      if (!t->continued)
	break;

      ecoff_swap_tir_in (bigend, &ax->a_ti, t);
      ax++;
    }

  /* Complain for illegal continuations due to corrupt aux entries.  */
  if (t->continued)
    complain (&bad_continued_complaint, sym_name);
 
  return tp;
}

/* Make up a complex type from a basic one.  Type is passed by
   reference in TPP and side-effected as necessary. The type
   qualifier TQ says how to handle the aux symbols at AX for
   the symbol SX we are currently analyzing.  BIGEND says whether
   aux symbols are big-endian or little-endian.
   Returns the number of aux symbols we parsed. */

static int
upgrade_type (fd, tpp, tq, ax, bigend, sym_name)
     int fd;
     struct type **tpp;
     int tq;
     union aux_ext *ax;
     int bigend;
     char *sym_name;
{
  int off;
  struct type *t;

  /* Used in array processing */
  int rf, id;
  FDR *fh;
  struct type *range;
  struct type *indx;
  int lower, upper;
  RNDXR rndx;

  switch (tq)
    {
    case tqPtr:
      t = lookup_pointer_type (*tpp);
      *tpp = t;
      return 0;

    case tqProc:
      t = lookup_function_type (*tpp);
      *tpp = t;
      return 0;

    case tqArray:
      off = 0;

      /* Determine and record the domain type (type of index) */
      ecoff_swap_rndx_in (bigend, &ax->a_rndx, &rndx);
      id = rndx.index;
      rf = rndx.rfd;
      if (rf == 0xfff)
	{
	  ax++;
	  rf = AUX_GET_ISYM (bigend, ax);
	  off++;
	}
      fh = get_rfd (fd, rf);

      indx = parse_type (fd,
			 ecoff_data (cur_bfd)->external_aux + fh->iauxBase,
			 id, (int *) NULL, bigend, sym_name);

      /* The bounds type should be an integer type, but might be anything
	 else due to corrupt aux entries.  */
      if (TYPE_CODE (indx) != TYPE_CODE_INT)
	{
	  complain (&array_index_type_complaint, sym_name);
	  indx = builtin_type_int;
	}

      /* Get the bounds, and create the array type.  */
      ax++;
      lower = AUX_GET_DNLOW (bigend, ax);
      ax++;
      upper = AUX_GET_DNHIGH (bigend, ax);
      ax++;
      rf = AUX_GET_WIDTH (bigend, ax);	/* bit size of array element */

      range = create_range_type ((struct type *) NULL, indx,
				 lower, upper);

      t = create_array_type ((struct type *) NULL, *tpp, range);

      /* We used to fill in the supplied array element bitsize
	 here if the TYPE_LENGTH of the target type was zero.
	 This happens for a `pointer to an array of anonymous structs',
	 but in this case the array element bitsize is also zero,
	 so nothing is gained.
	 And we used to check the TYPE_LENGTH of the target type against
	 the supplied array element bitsize.
	 gcc causes a mismatch for `pointer to array of object',
	 since the sdb directives it uses do not have a way of
	 specifying the bitsize, but it does no harm (the
	 TYPE_LENGTH should be correct) and we should be able to
	 ignore the erroneous bitsize from the auxiliary entry safely.
	 dbx seems to ignore it too.  */

      *tpp = t;
      return 4 + off;

    case tqVol:
      /* Volatile -- currently ignored */
      return 0;

    case tqConst:
      /* Const -- currently ignored */
      return 0;

    default:
      complain (&unknown_type_qual_complaint, tq);
      return 0;
    }
}


/* Parse a procedure descriptor record PR.  Note that the procedure is
   parsed _after_ the local symbols, now we just insert the extra
   information we need into a MIPS_EFI_SYMBOL_NAME symbol that has
   already been placed in the procedure's main block.  Note also that
   images that have been partially stripped (ld -x) have been deprived
   of local symbols, and we have to cope with them here.  FIRST_OFF is
   the offset of the first procedure for this FDR; we adjust the
   address by this amount, but I don't know why.  SEARCH_SYMTAB is the symtab
   to look for the function which contains the MIPS_EFI_SYMBOL_NAME symbol
   in question, or NULL to use top_stack->cur_block.  */

static void parse_procedure PARAMS ((PDR *, struct symtab *, unsigned long));

static void
parse_procedure (pr, search_symtab, first_off)
     PDR *pr;
     struct symtab *search_symtab;
     unsigned long first_off;
{
  struct symbol *s, *i;
  struct block *b;
  struct mips_extra_func_info *e;
  char *sh_name;

  /* Simple rule to find files linked "-x" */
  if (cur_fdr->rss == -1)
    {
      if (pr->isym == -1)
	{
	  /* Static procedure at address pr->adr.  Sigh. */
	  complain (&pdr_static_symbol_complaint, (unsigned long) pr->adr);
	  return;
	}
      else
	{
	  /* external */
	  EXTR she;
	  
	  (*ecoff_backend (cur_bfd)->swap_ext_in)
	    (cur_bfd,
	     ((char *) ecoff_data (cur_bfd)->external_ext
	      + pr->isym * ecoff_backend (cur_bfd)->external_ext_size),
	     &she);
	  sh_name = ecoff_data (cur_bfd)->ssext + she.asym.iss;
	}
    }
  else
    {
      /* Full symbols */
      SYMR sh;

      (*ecoff_backend (cur_bfd)->swap_sym_in)
	(cur_bfd,
	 ((char *) ecoff_data (cur_bfd)->external_sym
	  + ((cur_fdr->isymBase + pr->isym)
	     * ecoff_backend (cur_bfd)->external_sym_size)),
	 &sh);
      sh_name = ecoff_data (cur_bfd)->ss + cur_fdr->issBase + sh.iss;
    }

  if (search_symtab != NULL)
    {
#if 0
      /* This loses both in the case mentioned (want a static, find a global),
	 but also if we are looking up a non-mangled name which happens to
	 match the name of a mangled function.  */
      /* We have to save the cur_fdr across the call to lookup_symbol.
	 If the pdr is for a static function and if a global function with
	 the same name exists, lookup_symbol will eventually read in the symtab
	 for the global function and clobber cur_fdr.  */
      FDR *save_cur_fdr = cur_fdr;
      s = lookup_symbol (sh_name, NULL, VAR_NAMESPACE, 0, NULL);
      cur_fdr = save_cur_fdr;
#else
      s = mylookup_symbol
	(sh_name,
	 BLOCKVECTOR_BLOCK (BLOCKVECTOR (search_symtab), STATIC_BLOCK),
	 VAR_NAMESPACE,
	 LOC_BLOCK);
#endif
    }
  else
    s = mylookup_symbol (sh_name, top_stack->cur_block,
			 VAR_NAMESPACE, LOC_BLOCK);

  if (s != 0)
    {
      b = SYMBOL_BLOCK_VALUE (s);
    }
  else
    {
      complain (&pdr_for_nonsymbol_complaint, sh_name);
#if 1
      return;
#else
/* FIXME -- delete.  We can't do symbol allocation now; it's all done.  */
      s = new_symbol (sh_name);
      SYMBOL_NAMESPACE (s) = VAR_NAMESPACE;
      SYMBOL_CLASS (s) = LOC_BLOCK;
      /* Donno its type, hope int is ok */
      SYMBOL_TYPE (s) = lookup_function_type (builtin_type_int);
      add_symbol (s, top_stack->cur_block);
      /* Wont have symbols for this one */
      b = new_block (2);
      SYMBOL_BLOCK_VALUE (s) = b;
      BLOCK_FUNCTION (b) = s;
      BLOCK_START (b) = pr->adr;
      /* BOUND used to be the end of procedure's text, but the
	 argument is no longer passed in.  */
      BLOCK_END (b) = bound;
      BLOCK_SUPERBLOCK (b) = top_stack->cur_block;
      add_block (b, top_stack->cur_st);
#endif
    }

  i = mylookup_symbol (MIPS_EFI_SYMBOL_NAME, b, LABEL_NAMESPACE, LOC_CONST);

  if (i)
    {
      e = (struct mips_extra_func_info *) SYMBOL_VALUE (i);
      e->pdr = *pr;
      e->pdr.isym = (long) s;
      e->pdr.adr += cur_fdr->adr - first_off;

      /* Correct incorrect setjmp procedure descriptor from the library
	 to make backtrace through setjmp work.  */
      if (e->pdr.pcreg == 0 && STREQ (sh_name, "setjmp"))
	{
	  complain (&bad_setjmp_pdr_complaint, 0);
	  e->pdr.pcreg = RA_REGNUM;
	  e->pdr.regmask = 0x80000000;
	  e->pdr.regoffset = -4;
	}

      /* Fake PC_REGNUM for alpha __sigtramp so that read_next_frame_reg
	 will use the saved user pc from the sigcontext.  */
      if (STREQ (sh_name, "__sigtramp"))
	e->pdr.pcreg = PC_REGNUM;
    }
}

/* Parse the external symbol ES. Just call parse_symbol() after
   making sure we know where the aux are for it. For procedures,
   parsing of the PDRs has already provided all the needed
   information, we only parse them if SKIP_PROCEDURES is false,
   and only if this causes no symbol duplication.
   BIGEND says whether aux entries are big-endian or little-endian.

   This routine clobbers top_stack->cur_block and ->cur_st. */

static void
parse_external (es, skip_procedures, bigend)
     EXTR *es;
     int skip_procedures;
     int bigend;
{
  union aux_ext *ax;

  if (es->ifd != ifdNil)
    {
      cur_fd = es->ifd;
      cur_fdr = ecoff_data (cur_bfd)->fdr + cur_fd;
      ax = ecoff_data (cur_bfd)->external_aux + cur_fdr->iauxBase;
    }
  else
    {
      cur_fdr = ecoff_data (cur_bfd)->fdr;
      ax = 0;
    }

  /* Reading .o files */
  if (es->asym.sc == scUndefined || es->asym.sc == scNil)
    {
      char *what;
      switch (es->asym.st)
	{
	case stNil:
	  /* These are generated for static symbols in .o files,
	     ignore them.  */
	  return;
	case stStaticProc:
	case stProc:
	  what = "procedure";
	  n_undef_procs++;
	  break;
	case stGlobal:
	  what = "variable";
	  n_undef_vars++;
	  break;
	case stLabel:
	  what = "label";
	  n_undef_labels++;
	  break;
	default:
	  what = "symbol";
	  break;
	}
      n_undef_symbols++;
      /* FIXME:  Turn this into a complaint? */
      if (info_verbose)
	printf_filtered ("Warning: %s `%s' is undefined (in %s)\n",
			 what,
			 ecoff_data (cur_bfd)->ssext + es->asym.iss,
			 fdr_name (cur_fdr));
      return;
    }

  switch (es->asym.st)
    {
    case stProc:
      /* If we have full symbols we do not need more */
      if (skip_procedures)
	return;
      if (mylookup_symbol (ecoff_data (cur_bfd)->ssext + es->asym.iss,
			   top_stack->cur_block,
			   VAR_NAMESPACE, LOC_BLOCK))
	break;
      /* fall through */
    case stGlobal:
    case stLabel:
      /* Note that the case of a symbol with indexNil must be handled
	 anyways by parse_symbol().  */
      parse_symbol (&es->asym, ax, (char *) NULL, bigend);
      break;
    default:
      break;
    }
}

/* Parse the line number info for file descriptor FH into
   GDB's linetable LT.  MIPS' encoding requires a little bit
   of magic to get things out.  Note also that MIPS' line
   numbers can go back and forth, apparently we can live
   with that and do not need to reorder our linetables */

static void
parse_lines (fh, pr, lt, maxlines)
     FDR *fh;
     PDR *pr;
     struct linetable *lt;
     int maxlines;
{
  unsigned char *base;
  int j, k;
  int delta, count, lineno = 0;
  unsigned long first_off = pr->adr;

  if (fh->cbLine == 0)
    return;

  base = ecoff_data (cur_bfd)->line + fh->cbLineOffset;

  /* Scan by procedure descriptors */
  k = 0;
  for (j = 0; j < fh->cpd; j++, pr++)
    {
      long l;
      unsigned long adr;
      unsigned char *halt;

      /* No code for this one */
      if (pr->iline == ilineNil ||
	  pr->lnLow == -1 || pr->lnHigh == -1)
	continue;

      /* Determine start and end address of compressed line bytes for
	 this procedure.  */
      base = ecoff_data (cur_bfd)->line + fh->cbLineOffset;
      if (j != (fh->cpd - 1))
 	halt = base + pr[1].cbLineOffset;
      else
 	halt = base + fh->cbLine;
      base += pr->cbLineOffset;

      adr = fh->adr + pr->adr - first_off;
      l = adr >> 2;		/* in words */
      for (lineno = pr->lnLow; base < halt; )
	{
	  count = *base & 0x0f;
	  delta = *base++ >> 4;
	  if (delta >= 8)
	    delta -= 16;
	  if (delta == -8)
	    {
	      delta = (base[0] << 8) | base[1];
	      if (delta >= 0x8000)
		delta -= 0x10000;
	      base += 2;
	    }
	  lineno += delta;	/* first delta is 0 */

	  /* Complain if the line table overflows. Could happen
	     with corrupt binaries.  */
	  if (lt->nitems >= maxlines)
	    {
	      complain (&bad_linetable_guess_complaint, fdr_name (fh));
	      break;
	    }
	  k = add_line (lt, lineno, l, k);
	  l += count + 1;
	}
    }
}

/* Master parsing procedure for first-pass reading of file symbols
   into a partial_symtab.  */

static void
parse_partial_symbols (objfile, section_offsets)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
{
  const struct ecoff_backend_data * const backend = ecoff_backend (cur_bfd);
  const bfd_size_type external_sym_size = backend->external_sym_size;
  const bfd_size_type external_rfd_size = backend->external_rfd_size;
  const bfd_size_type external_ext_size = backend->external_ext_size;
  void (* const swap_ext_in) PARAMS ((bfd *, PTR, EXTR *))
    = backend->swap_ext_in;
  void (* const swap_sym_in) PARAMS ((bfd *, PTR, SYMR *))
    = backend->swap_sym_in;
  void (* const swap_rfd_in) PARAMS ((bfd *, PTR, RFDT *))
    = backend->swap_rfd_in;
  int f_idx, s_idx;
  HDRR *hdr = &ecoff_data (cur_bfd)->symbolic_header;
  /* Running pointers */
  FDR *fh;
  char *ext_out;
  char *ext_out_end;
  EXTR *ext_block;
  register EXTR *ext_in;
  EXTR *ext_in_end;
  SYMR sh;
  struct partial_symtab *pst;

  int past_first_source_file = 0;

  /* List of current psymtab's include files */
  char **psymtab_include_list;
  int includes_allocated;
  int includes_used;
  EXTR *extern_tab;
  struct pst_map *fdr_to_pst;
  /* Index within current psymtab dependency list */
  struct partial_symtab **dependency_list;
  int dependencies_used, dependencies_allocated;
  struct cleanup *old_chain;
  char *name;
  enum language prev_language;

  extern_tab = (EXTR *) obstack_alloc (&objfile->psymbol_obstack,
				       sizeof (EXTR) * hdr->iextMax);

  includes_allocated = 30;
  includes_used = 0;
  psymtab_include_list = (char **) alloca (includes_allocated *
					   sizeof (char *));
  next_symbol_text_func = mips_next_symbol_text;

  dependencies_allocated = 30;
  dependencies_used = 0;
  dependency_list =
    (struct partial_symtab **) alloca (dependencies_allocated *
				       sizeof (struct partial_symtab *));

  last_source_file = NULL;

  /*
   * Big plan:
   *
   * Only parse the Local and External symbols, and the Relative FDR.
   * Fixup enough of the loader symtab to be able to use it.
   * Allocate space only for the file's portions we need to
   * look at. (XXX)
   */

  max_gdbinfo = 0;
  max_glevel = MIN_GLEVEL;

  /* Allocate the map FDR -> PST.
     Minor hack: -O3 images might claim some global data belongs
     to FDR -1. We`ll go along with that */
  fdr_to_pst = (struct pst_map *) xzalloc ((hdr->ifdMax + 1) * sizeof *fdr_to_pst);
  old_chain = make_cleanup (free, fdr_to_pst);
  fdr_to_pst++;
  {
    struct partial_symtab *pst = new_psymtab ("", objfile);
    fdr_to_pst[-1].pst = pst;
    FDR_IDX (pst) = -1;
  }

  /* Allocate the global pending list.  */
  ECOFF_PENDING_LIST (objfile) = 
    ((struct mips_pending **)
     obstack_alloc (&objfile->psymbol_obstack,
		    hdr->ifdMax * sizeof (struct mips_pending *)));
  memset ((PTR) ECOFF_PENDING_LIST (objfile), 0,
	  hdr->ifdMax * sizeof (struct mips_pending *));

  /* Pass 0 over external syms: swap them in.  */
  ext_block = (EXTR *) xmalloc (hdr->iextMax * sizeof (EXTR));
  make_cleanup (free, ext_block);

  ext_out = (char *) ecoff_data (cur_bfd)->external_ext;
  ext_out_end = ext_out + hdr->iextMax * external_ext_size;
  ext_in = ext_block;
  for (; ext_out < ext_out_end; ext_out += external_ext_size, ext_in++)
    (*swap_ext_in) (cur_bfd, ext_out, ext_in);

  /* Pass 1 over external syms: Presize and partition the list */
  ext_in = ext_block;
  ext_in_end = ext_in + hdr->iextMax;
  for (; ext_in < ext_in_end; ext_in++)
    fdr_to_pst[ext_in->ifd].n_globals++;

  /* Pass 1.5 over files:  partition out global symbol space */
  s_idx = 0;
  for (f_idx = -1; f_idx < hdr->ifdMax; f_idx++)
    {
      fdr_to_pst[f_idx].globals_offset = s_idx;
      s_idx += fdr_to_pst[f_idx].n_globals;
      fdr_to_pst[f_idx].n_globals = 0;
    }

  /* Pass 2 over external syms: fill in external symbols */
  ext_in = ext_block;
  ext_in_end = ext_in + hdr->iextMax;
  for (; ext_in < ext_in_end; ext_in++)
    {
      enum minimal_symbol_type ms_type = mst_text;

      extern_tab[fdr_to_pst[ext_in->ifd].globals_offset
		 + fdr_to_pst[ext_in->ifd].n_globals++] = *ext_in;

      if (ext_in->asym.sc == scUndefined || ext_in->asym.sc == scNil)
	continue;

      name = ecoff_data (cur_bfd)->ssext + ext_in->asym.iss;
      switch (ext_in->asym.st)
	{
	case stProc:
	  break;
	case stStaticProc:
	  ms_type = mst_file_text;
	  break;
	case stGlobal:
          if (ext_in->asym.sc == scData
	      || ext_in->asym.sc == scSData
	      || ext_in->asym.sc == scRData)
	    ms_type = mst_data;
	  else
	    ms_type = mst_bss;
	  break;
	case stLabel:
          if (ext_in->asym.sc == scAbs)
	    ms_type = mst_abs;
          else if (ext_in->asym.sc == scText)
	    ms_type = mst_text;
          else if (ext_in->asym.sc == scData
		   || ext_in->asym.sc == scSData
		   || ext_in->asym.sc == scRData)
	    ms_type = mst_data;
	  else
	    ms_type = mst_bss;
	  break;
	case stLocal:
	  /* The alpha has the section start addresses in stLocal symbols
	     whose name starts with a `.'. Skip those but complain for all
	     other stLocal symbols.  */
	  if (name[0] == '.')
	    continue;
	  /* Fall through.  */
	default:
	  ms_type = mst_unknown;
	  complain (&unknown_ext_complaint, name);
	}
      prim_record_minimal_symbol (name, ext_in->asym.value, ms_type);
    }

  /* Pass 3 over files, over local syms: fill in static symbols */
  for (f_idx = 0; f_idx < hdr->ifdMax; f_idx++)
    {
      struct partial_symtab *save_pst;
      EXTR *ext_ptr;

      cur_fdr = fh = ecoff_data (cur_bfd)->fdr + f_idx;

      if (fh->csym == 0)
	{
	  fdr_to_pst[f_idx].pst = NULL;
	  continue;
	}
      pst = start_psymtab_common (objfile, section_offsets,
				  fdr_name (fh),
				  fh->cpd ? fh->adr : 0,
				  objfile->global_psymbols.next,
				  objfile->static_psymbols.next);
      pst->read_symtab_private = ((char *)
				  obstack_alloc (&objfile->psymbol_obstack,
						 sizeof (struct symloc)));
      memset ((PTR) pst->read_symtab_private, 0, sizeof (struct symloc));

      save_pst = pst;
      FDR_IDX (pst) = f_idx;
      CUR_BFD (pst) = cur_bfd;

      /* The way to turn this into a symtab is to call... */
      pst->read_symtab = mipscoff_psymtab_to_symtab;

      /* Set up language for the pst.
         The language from the FDR is used if it is unambigious (e.g. cfront
	 with native cc and g++ will set the language to C).
	 Otherwise we have to deduce the language from the filename.
	 Native ecoff has every header file in a separate FDR, so
	 deduce_language_from_filename will return language_unknown for
	 a header file, which is not what we want.
	 But the FDRs for the header files are after the FDR for the source
	 file, so we can assign the language of the source file to the
	 following header files. Then we save the language in the private
	 pst data so that we can reuse it when building symtabs.  */
      prev_language = psymtab_language;

      switch (fh->lang)
	{
	case langCplusplusV2:
	  psymtab_language = language_cplus;
	  break;
	default:
	  psymtab_language = deduce_language_from_filename (fdr_name (fh));
	  break;
	}
      if (psymtab_language == language_unknown)
	psymtab_language = prev_language;
      PST_PRIVATE (pst)->pst_language = psymtab_language;

      pst->texthigh = pst->textlow;

      /* For stabs-in-ecoff files, the second symbol must be @stab.
	 This symbol is emitted by mips-tfile to signal that the
	 current object file uses encapsulated stabs instead of mips
	 ecoff for local symbols.  (It is the second symbol because
	 the first symbol is the stFile used to signal the start of a
	 file). */
      processing_gcc_compilation = 0;
      if (fh->csym >= 2)
	{
	  (*swap_sym_in) (cur_bfd,
			  ((char *) ecoff_data (cur_bfd)->external_sym
			   + (fh->isymBase + 1) * external_sym_size),
			  &sh);
	  if (STREQ (ecoff_data (cur_bfd)->ss + fh->issBase + sh.iss,
		     stabs_symbol))
	    processing_gcc_compilation = 2;
	}

      if (processing_gcc_compilation != 0)
	{
	  for (cur_sdx = 2; cur_sdx < fh->csym; cur_sdx++)
	    {
	      int type_code;
	      char *namestring;

	      (*swap_sym_in) (cur_bfd,
			      ((char *) ecoff_data (cur_bfd)->external_sym
			       + (fh->isymBase + cur_sdx) * external_sym_size),
			      &sh);
	      type_code = ECOFF_UNMARK_STAB (sh.index);
	      if (!ECOFF_IS_STAB (&sh))
		{
		  if (sh.st == stProc || sh.st == stStaticProc)
		    {
		      long procaddr = sh.value;
		      long isym;


		      isym = AUX_GET_ISYM (fh->fBigendian,
					   (ecoff_data (cur_bfd)->external_aux
					    + fh->iauxBase
					    + sh.index));
		      (*swap_sym_in) (cur_bfd,
				      (((char *)
					ecoff_data (cur_bfd)->external_sym)
				       + ((fh->isymBase + isym - 1)
					  * external_sym_size)),
				      &sh);
		      if (sh.st == stEnd)
			{
			  long high = procaddr + sh.value;
			  if (high > pst->texthigh)
			    pst->texthigh = high;
			}
		    }
		  continue;
		}
#define SET_NAMESTRING() \
  namestring = ecoff_data (cur_bfd)->ss + fh->issBase + sh.iss
#define CUR_SYMBOL_TYPE type_code
#define CUR_SYMBOL_VALUE sh.value
#define START_PSYMTAB(ofile,secoff,fname,low,symoff,global_syms,static_syms)\
  pst = save_pst
#define END_PSYMTAB(pst,ilist,ninc,c_off,c_text,dep_list,n_deps) (void)0
#define HANDLE_RBRAC(val) \
  if ((val) > save_pst->texthigh) save_pst->texthigh = (val);
#include "partial-stab.h"
	    }
	}
      else
	{
	  for (cur_sdx = 0; cur_sdx < fh->csym;)
	    {
	      char *name;
	      enum address_class class;

	      (*swap_sym_in) (cur_bfd,
			      ((char *) ecoff_data (cur_bfd)->external_sym
			       + ((fh->isymBase + cur_sdx)
				  * external_sym_size)),
			      &sh);

	      if (ECOFF_IS_STAB (&sh))
		{
		  cur_sdx++;
		  continue;
		}

	      /* Non absolute static symbols go into the minimal table.  */
	      if (sh.sc == scUndefined || sh.sc == scNil
		  || (sh.index == indexNil
		      && (sh.st != stStatic || sh.sc == scAbs)))
		{
		  /* FIXME, premature? */
		  cur_sdx++;
		  continue;
		}

	      name = ecoff_data (cur_bfd)->ss + fh->issBase + sh.iss;

	      switch (sh.st)
		{
		  long high;
		  long procaddr;
		  int new_sdx;

		case stStaticProc:	/* Function */
		  /* I believe this is used only for file-local functions.
		     The comment in symconst.h ("load time only static procs")
		     isn't particularly clear on this point.  */
		  prim_record_minimal_symbol (name, sh.value, mst_file_text);
		  /* FALLTHROUGH */

		case stProc:	/* Asm labels apparently */
		  ADD_PSYMBOL_TO_LIST (name, strlen (name),
				       VAR_NAMESPACE, LOC_BLOCK,
				       objfile->static_psymbols, sh.value,
				       psymtab_language, objfile);
		  /* Skip over procedure to next one. */
		  if (sh.index >= hdr->iauxMax)
		    {
		      /* Should not happen, but does when cross-compiling
			   with the MIPS compiler.  FIXME -- pull later.  */
		      complain (&index_complaint, name);
		      new_sdx = cur_sdx + 1;	/* Don't skip at all */
		    }
		  else
		    new_sdx = AUX_GET_ISYM (fh->fBigendian,
					    (ecoff_data (cur_bfd)->external_aux
					     + fh->iauxBase
					     + sh.index));
		  procaddr = sh.value;

		  if (new_sdx <= cur_sdx)
		    {
		      /* This should not happen either... FIXME.  */
		      complain (&aux_index_complaint, name);
		      new_sdx = cur_sdx + 1;	/* Don't skip backward */
		    }

		  cur_sdx = new_sdx;
		  (*swap_sym_in) (cur_bfd,
				  ((char *) ecoff_data (cur_bfd)->external_sym
				   + ((fh->isymBase + cur_sdx - 1)
				      * external_sym_size)),
				  &sh);
		  if (sh.st != stEnd)
		    continue;
		  high = procaddr + sh.value;
		  if (high > pst->texthigh)
		    pst->texthigh = high;
		  continue;

		case stStatic:	/* Variable */
		  if (sh.sc == scData || sh.sc == scSData || sh.sc == scRData)
		    prim_record_minimal_symbol (name, sh.value, mst_file_data);
		  else
		    prim_record_minimal_symbol (name, sh.value, mst_file_bss);
		  class = LOC_STATIC;
		  break;

		case stTypedef:/* Typedef */
		  class = LOC_TYPEDEF;
		  break;

		case stConstant:	/* Constant decl */
		  class = LOC_CONST;
		  break;

		case stUnion:
		case stStruct:
		case stEnum:
		case stBlock:	/* { }, str, un, enum*/
		  if (sh.sc == scInfo || sh.sc == scCommon)
		    {
		      ADD_PSYMBOL_TO_LIST (name, strlen (name),
					   STRUCT_NAMESPACE, LOC_TYPEDEF,
					   objfile->static_psymbols,
					   sh.value,
					   psymtab_language, objfile);
		    }
		  /* Skip over the block */
		  new_sdx = sh.index;
		  if (new_sdx <= cur_sdx)
		    {
		      /* This happens with the Ultrix kernel. */
		      complain (&block_index_complaint, name);
		      new_sdx = cur_sdx + 1;	/* Don't skip backward */
		    }
		  cur_sdx = new_sdx;
		  continue;

		case stFile:	/* File headers */
		case stLabel:	/* Labels */
		case stEnd:	/* Ends of files */
		  goto skip;

		case stLocal:	/* Local variables */
		  /* Normally these are skipped because we skip over
		     all blocks we see.  However, these can occur
		     as visible symbols in a .h file that contains code. */
		  goto skip;

		default:
		  /* Both complaints are valid:  one gives symbol name,
		     the other the offending symbol type.  */
		  complain (&unknown_sym_complaint, name);
		  complain (&unknown_st_complaint, sh.st);
		  cur_sdx++;
		  continue;
		}
	      /* Use this gdb symbol */
	      ADD_PSYMBOL_TO_LIST (name, strlen (name),
				   VAR_NAMESPACE, class,
				   objfile->static_psymbols, sh.value,
				   psymtab_language, objfile);
	    skip:
	      cur_sdx++;	/* Go to next file symbol */
	    }

	  /* Now do enter the external symbols. */
	  ext_ptr = &extern_tab[fdr_to_pst[f_idx].globals_offset];
	  cur_sdx = fdr_to_pst[f_idx].n_globals;
	  PST_PRIVATE (save_pst)->extern_count = cur_sdx;
	  PST_PRIVATE (save_pst)->extern_tab = ext_ptr;
	  for (; --cur_sdx >= 0; ext_ptr++)
	    {
	      enum address_class class;
	      SYMR *psh;
	      char *name;

	      if (ext_ptr->ifd != f_idx)
		abort ();
	      psh = &ext_ptr->asym;

	      /* Do not add undefined symbols to the partial symbol table.  */
	      if (psh->sc == scUndefined || psh->sc == scNil)
		continue;

	      switch (psh->st)
		{
		case stNil:
		  /* These are generated for static symbols in .o files,
		     ignore them.  */
		  continue;
		case stProc:
		case stStaticProc:
		  class = LOC_BLOCK;
		  break;
		case stLabel:
		  class = LOC_LABEL;
		  break;
		default:
		  complain (&unknown_ext_complaint,
			    ecoff_data (cur_bfd)->ssext + psh->iss);
		  /* Fall through, pretend it's global.  */
		case stGlobal:
		  class = LOC_STATIC;
		  break;
		}
	      name = ecoff_data (cur_bfd)->ssext + psh->iss;
	      ADD_PSYMBOL_ADDR_TO_LIST (name, strlen (name),
				        VAR_NAMESPACE, class,
				        objfile->global_psymbols, (CORE_ADDR) psh->value,
				        psymtab_language, objfile);
	    }
	}

      /* Link pst to FDR. end_psymtab returns NULL if the psymtab was
	 empty and put on the free list.  */
      fdr_to_pst[f_idx].pst = end_psymtab (save_pst,
					   psymtab_include_list, includes_used,
					   -1, save_pst->texthigh,
					   dependency_list, dependencies_used);
      if (objfile->ei.entry_point >= save_pst->textlow &&
	  objfile->ei.entry_point < save_pst->texthigh)
	{
	  objfile->ei.entry_file_lowpc = save_pst->textlow;
	  objfile->ei.entry_file_highpc = save_pst->texthigh;
	}
    }

  /* Now scan the FDRs for dependencies */
  for (f_idx = 0; f_idx < hdr->ifdMax; f_idx++)
    {
      fh = f_idx + ecoff_data (cur_bfd)->fdr;
      pst = fdr_to_pst[f_idx].pst;

      if (pst == (struct partial_symtab *)NULL)
	continue;

      /* This should catch stabs-in-ecoff. */
      if (fh->crfd <= 1)
	continue;

      /* Skip the first file indirect entry as it is a self dependency
	 for source files or a reverse .h -> .c dependency for header files.  */
      pst->number_of_dependencies = 0;
      pst->dependencies =
	((struct partial_symtab **)
	 obstack_alloc (&objfile->psymbol_obstack,
			((fh->crfd - 1)
			 * sizeof (struct partial_symtab *))));
      for (s_idx = 1; s_idx < fh->crfd; s_idx++)
	{
	  RFDT rh;

	  (*swap_rfd_in) (cur_bfd,
			  ((char *) ecoff_data (cur_bfd)->external_rfd
			   + (fh->rfdBase + s_idx) * external_rfd_size),
			  &rh);
	  if (rh < 0 || rh >= hdr->ifdMax)
	    {
	      complain (&bad_file_number_complaint, rh);
	      continue;
	    }

	  /* Skip self dependencies of header files.  */
	  if (rh == f_idx)
	    continue;

	  /* Do not add to dependeny list if psymtab was empty.  */
	  if (fdr_to_pst[rh].pst == (struct partial_symtab *)NULL)
	    continue;
	  pst->dependencies[pst->number_of_dependencies++] = fdr_to_pst[rh].pst;
	}
    }
  do_cleanups (old_chain);
}


static char *
mips_next_symbol_text ()
{
  SYMR sh;

  cur_sdx++;
  (*ecoff_backend (cur_bfd)->swap_sym_in)
    (cur_bfd,
     ((char *) ecoff_data (cur_bfd)->external_sym
      + ((cur_fdr->isymBase + cur_sdx)
	 * ecoff_backend (cur_bfd)->external_sym_size)),
     &sh);
  return ecoff_data (cur_bfd)->ss + cur_fdr->issBase + sh.iss;
}

/* Ancillary function to psymtab_to_symtab().  Does all the work
   for turning the partial symtab PST into a symtab, recurring
   first on all dependent psymtabs.  The argument FILENAME is
   only passed so we can see in debug stack traces what file
   is being read.

   This function has a split personality, based on whether the
   symbol table contains ordinary ecoff symbols, or stabs-in-ecoff.
   The flow of control and even the memory allocation differs.  FIXME.  */

static void
psymtab_to_symtab_1 (pst, filename)
     struct partial_symtab *pst;
     char *filename;
{
  const bfd_size_type external_sym_size
    = ecoff_backend (cur_bfd)->external_sym_size;
  const bfd_size_type external_pdr_size
    = ecoff_backend (cur_bfd)->external_pdr_size;
  void (* const swap_sym_in) PARAMS ((bfd *, PTR, SYMR *))
    = ecoff_backend (cur_bfd)->swap_sym_in;
  void (* const swap_pdr_in) PARAMS ((bfd *, PTR, PDR *))
    = ecoff_backend (cur_bfd)->swap_pdr_in;
  int i;
  struct symtab *st;
  FDR *fh;
  struct linetable *lines;

  if (pst->readin)
    return;
  pst->readin = 1;

  /* Read in all partial symbtabs on which this one is dependent.
     NOTE that we do have circular dependencies, sigh.  We solved
     that by setting pst->readin before this point.  */

  for (i = 0; i < pst->number_of_dependencies; i++)
    if (!pst->dependencies[i]->readin)
      {
	/* Inform about additional files to be read in.  */
	if (info_verbose)
	  {
	    fputs_filtered (" ", stdout);
	    wrap_here ("");
	    fputs_filtered ("and ", stdout);
	    wrap_here ("");
	    printf_filtered ("%s...",
			     pst->dependencies[i]->filename);
	    wrap_here ("");	/* Flush output */
	    fflush (stdout);
	  }
	/* We only pass the filename for debug purposes */
	psymtab_to_symtab_1 (pst->dependencies[i],
			     pst->dependencies[i]->filename);
      }

  /* Do nothing if this is a dummy psymtab.  */

  if (pst->n_global_syms == 0 && pst->n_static_syms == 0
      && pst->textlow == 0 && pst->texthigh == 0)
    return;

  /* Now read the symbols for this symtab */

  cur_bfd = CUR_BFD (pst);
  current_objfile = pst->objfile;
  cur_fd = FDR_IDX (pst);
  fh = (cur_fd == -1) ? (FDR *) NULL : ecoff_data (cur_bfd)->fdr + cur_fd;
  cur_fdr = fh;

  /* See comment in parse_partial_symbols about the @stabs sentinel. */
  processing_gcc_compilation = 0;
  if (fh != (FDR *) NULL && fh->csym >= 2)
    {
      SYMR sh;

      (*swap_sym_in) (cur_bfd,
		      ((char *) ecoff_data (cur_bfd)->external_sym
		       + (fh->isymBase + 1) * external_sym_size),
		      &sh);
      if (STREQ (ecoff_data (cur_bfd)->ss + fh->issBase + sh.iss,
		 stabs_symbol))
	{
	  /* We indicate that this is a GCC compilation so that certain
	     features will be enabled in stabsread/dbxread.  */
	  processing_gcc_compilation = 2;
	}
    }

  if (processing_gcc_compilation != 0)
    {
      char *pdr_ptr;
      char *pdr_end;
      int first_pdr;
      unsigned long first_off = 0;

      /* This symbol table contains stabs-in-ecoff entries.  */

      /* Parse local symbols first */

      if (fh->csym <= 2)	/* FIXME, this blows psymtab->symtab ptr */
	{
	  current_objfile = NULL;
	  return;
	}
      for (cur_sdx = 2; cur_sdx < fh->csym; cur_sdx++)
	{
	  SYMR sh;
	  char *name;
	  CORE_ADDR valu;

	  (*swap_sym_in) (cur_bfd,
			  ((char *) ecoff_data (cur_bfd)->external_sym
			   + (fh->isymBase + cur_sdx) * external_sym_size),
			  &sh);
	  name = ecoff_data (cur_bfd)->ss + fh->issBase + sh.iss;
	  valu = sh.value;
	  if (ECOFF_IS_STAB (&sh))
	    {
	      int type_code = ECOFF_UNMARK_STAB (sh.index);
	      process_one_symbol (type_code, 0, valu, name,
				  pst->section_offsets, pst->objfile);
	      if (type_code == N_FUN)
		{
		  /* Make up special symbol to contain
		     procedure specific info */
		  struct mips_extra_func_info *e =
		    ((struct mips_extra_func_info *)
		     obstack_alloc (&current_objfile->symbol_obstack,
				    sizeof (struct mips_extra_func_info)));
		  struct symbol *s = new_symbol (MIPS_EFI_SYMBOL_NAME);
		  SYMBOL_NAMESPACE (s) = LABEL_NAMESPACE;
		  SYMBOL_CLASS (s) = LOC_CONST;
		  SYMBOL_TYPE (s) = builtin_type_void;
		  SYMBOL_VALUE (s) = (long) e;
		  add_symbol_to_list (s, &local_symbols);
		}
	    }
	  else if (sh.st == stLabel && sh.index != indexNil)
	    {
	      /* Handle encoded stab line number. */
	      record_line (current_subfile, sh.index, valu);
	    }
	  else if (sh.st == stProc || sh.st == stStaticProc || sh.st == stEnd)
	    /* These are generated by gcc-2.x, do not complain */
	    ;
	  else
	    complain (&stab_unknown_complaint, name);
	}
      st = end_symtab (pst->texthigh, 0, 0, pst->objfile, SECT_OFF_TEXT);
      end_stabs ();

      /* Sort the symbol table now, we are done adding symbols to it.
	 We must do this before parse_procedure calls lookup_symbol.  */
      sort_symtab_syms (st);

      /* This may not be necessary for stabs symtabs.  FIXME.  */
      sort_blocks (st);

      /* Fill in procedure info next.  */
      first_pdr = 1;
      pdr_ptr = ((char *) ecoff_data (cur_bfd)->external_pdr
		 + fh->ipdFirst * external_pdr_size);
      pdr_end = pdr_ptr + fh->cpd * external_pdr_size;
      for (; pdr_ptr < pdr_end; pdr_ptr += external_pdr_size)
	{
	  PDR pr;

	  (*swap_pdr_in) (cur_bfd, pdr_ptr, &pr);
	  if (first_pdr)
	    {
	      first_off = pr.adr;
	      first_pdr = 0;
	    }
	  parse_procedure (&pr, st, first_off);
	}
    }
  else
    {
      /* This symbol table contains ordinary ecoff entries.  */

      /* FIXME:  doesn't use pst->section_offsets.  */

      int f_max;
      int maxlines;
      EXTR *ext_ptr;

      /* How many symbols will we need */
      /* FIXME, this does not count enum values. */
      f_max = pst->n_global_syms + pst->n_static_syms;
      if (fh == 0)
	{
	  maxlines = 0;
	  st = new_symtab ("unknown", f_max, 0, pst->objfile);
	}
      else
	{
	  f_max += fh->csym + fh->cpd;
	  maxlines = 2 * fh->cline;
	  st = new_symtab (pst->filename, 2 * f_max, maxlines, pst->objfile);

	  /* The proper language was already determined when building
	     the psymtab, use it.  */
	  st->language = PST_PRIVATE (pst)->pst_language;
	}

      psymtab_language = st->language;

      lines = LINETABLE (st);

      /* Get a new lexical context */

      push_parse_stack ();
      top_stack->cur_st = st;
      top_stack->cur_block = BLOCKVECTOR_BLOCK (BLOCKVECTOR (st),
						STATIC_BLOCK);
      BLOCK_START (top_stack->cur_block) = fh ? fh->adr : 0;
      BLOCK_END (top_stack->cur_block) = 0;
      top_stack->blocktype = stFile;
      top_stack->maxsyms = 2 * f_max;
      top_stack->cur_type = 0;
      top_stack->procadr = 0;
      top_stack->numargs = 0;

      if (fh)
	{
	  char *sym_ptr;
	  char *sym_end;

	  /* Parse local symbols first */
	  sym_ptr = ((char *) ecoff_data (cur_bfd)->external_sym
		     + fh->isymBase * external_sym_size);
	  sym_end = sym_ptr + fh->csym * external_sym_size;
	  while (sym_ptr < sym_end)
	    {
	      SYMR sh;
	      int c;

	      (*swap_sym_in) (cur_bfd, sym_ptr, &sh);
	      c = parse_symbol (&sh,
				(ecoff_data (cur_bfd)->external_aux
				 + fh->iauxBase),
				sym_ptr, fh->fBigendian);
	      sym_ptr += c * external_sym_size;
	    }

	  /* Linenumbers.  At the end, check if we can save memory.
	     parse_lines has to look ahead an arbitrary number of PDR
	     structures, so we swap them all first.  */
	  if (fh->cpd > 0)
	    {
	      PDR *pr_block;
	      struct cleanup *old_chain;
	      char *pdr_ptr;
	      char *pdr_end;
	      PDR *pdr_in;
	      PDR *pdr_in_end;

	      pr_block = (PDR *) xmalloc (fh->cpd * sizeof (PDR));

	      old_chain = make_cleanup (free, pr_block);

	      pdr_ptr = ((char *) ecoff_data (cur_bfd)->external_pdr
			 + fh->ipdFirst * external_pdr_size);
	      pdr_end = pdr_ptr + fh->cpd * external_pdr_size;
	      pdr_in = pr_block;
	      for (;
		   pdr_ptr < pdr_end;
		   pdr_ptr += external_pdr_size, pdr_in++)
		(*swap_pdr_in) (cur_bfd, pdr_ptr, pdr_in);

	      parse_lines (fh, pr_block, lines, maxlines);
	      if (lines->nitems < fh->cline)
		lines = shrink_linetable (lines);

	      /* Fill in procedure info next.  */
	      pdr_in = pr_block;
	      pdr_in_end = pdr_in + fh->cpd;
	      for (; pdr_in < pdr_in_end; pdr_in++)
		parse_procedure (pdr_in, 0, pr_block->adr);

	      do_cleanups (old_chain);
	    }
	}

      LINETABLE (st) = lines;

      /* .. and our share of externals.
	 XXX use the global list to speed up things here. how?
	 FIXME, Maybe quit once we have found the right number of ext's? */
      top_stack->cur_st = st;
      top_stack->cur_block = BLOCKVECTOR_BLOCK (BLOCKVECTOR (top_stack->cur_st),
						GLOBAL_BLOCK);
      top_stack->blocktype = stFile;
      top_stack->maxsyms = (ecoff_data (cur_bfd)->symbolic_header.isymMax
			    + ecoff_data (cur_bfd)->symbolic_header.ipdMax
			    + ecoff_data (cur_bfd)->symbolic_header.iextMax);

      ext_ptr = PST_PRIVATE (pst)->extern_tab;
      for (i = PST_PRIVATE (pst)->extern_count; --i >= 0; ext_ptr++)
	parse_external (ext_ptr, 1, fh->fBigendian);

      /* If there are undefined symbols, tell the user.
	 The alpha has an undefined symbol for every symbol that is
	 from a shared library, so tell the user only if verbose is on.  */
      if (info_verbose && n_undef_symbols)
	{
	  printf_filtered ("File %s contains %d unresolved references:",
			   st->filename, n_undef_symbols);
	  printf_filtered ("\n\t%4d variables\n\t%4d procedures\n\t%4d labels\n",
			   n_undef_vars, n_undef_procs, n_undef_labels);
	  n_undef_symbols = n_undef_labels = n_undef_vars = n_undef_procs = 0;

	}
      pop_parse_stack ();

      /* Sort the symbol table now, we are done adding symbols to it.*/
      sort_symtab_syms (st);

      sort_blocks (st);
    }

  /* Now link the psymtab and the symtab.  */
  pst->symtab = st;

  current_objfile = NULL;
}

/* Ancillary parsing procedures. */

/* Lookup the type at relative index RN.  Return it in TPP
   if found and in any event come up with its name PNAME.
   BIGEND says whether aux symbols are big-endian or not (from fh->fBigendian).
   Return value says how many aux symbols we ate. */

static int
cross_ref (fd, ax, tpp, type_code, pname, bigend, sym_name)
     int fd;
     union aux_ext *ax;
     struct type **tpp;
     enum type_code type_code;	/* Use to alloc new type if none is found. */
     char **pname;
     int bigend;
     char *sym_name;
{
  RNDXR rn[1];
  unsigned int rf;
  int result = 1;
  FDR *fh;
  char *esh;
  SYMR sh;
  int xref_fd;
  struct mips_pending *pend;

  *tpp = (struct type *)NULL;

  ecoff_swap_rndx_in (bigend, &ax->a_rndx, rn);

  /* Escape index means 'the next one' */
  if (rn->rfd == 0xfff)
    {
      result++;
      rf = AUX_GET_ISYM (bigend, ax + 1);
    }
  else
    {
      rf = rn->rfd;
    }

  /* mips cc uses a rf of -1 for opaque struct definitions.
     Set TYPE_FLAG_STUB for these types so that check_stub_type will
     resolve them if the struct gets defined in another compilation unit.  */
  if (rf == -1)
    {
      *pname = "<undefined>";
      *tpp = init_type (type_code, 0, 0, (char *) NULL, current_objfile);
      TYPE_FLAGS (*tpp) |= TYPE_FLAG_STUB;
      return result;
    }

  /* mips cc uses an escaped rn->index of 0 for struct return types
     of procedures that were compiled without -g. These will always remain
     undefined.  */
  if (rn->rfd == 0xfff && rn->index == 0)
    {
      *pname = "<undefined>";
      return result;
    }

  /* Find the relative file descriptor and the symbol in it.  */
  fh = get_rfd (fd, rf);
  xref_fd = fh - ecoff_data (cur_bfd)->fdr;

  if (rn->index >= fh->csym)
    {
      /* File indirect entry is corrupt.  */
      *pname = "<illegal>";
      complain (&bad_rfd_entry_complaint,
		sym_name, xref_fd, rn->index);
      return result;
    }

  /* If we have processed this symbol then we left a forwarding
     pointer to the type in the pending list.  If not, we`ll put
     it in a list of pending types, to be processed later when
     the file will be.  In any event, we collect the name for the
     type here.  */

  esh = ((char *) ecoff_data (cur_bfd)->external_sym
	 + ((fh->isymBase + rn->index)
	    * ecoff_backend (cur_bfd)->external_sym_size));
  (*ecoff_backend (cur_bfd)->swap_sym_in) (cur_bfd, esh, &sh);

  /* Make sure that this type of cross reference can be handled.  */
  if (sh.sc != scInfo
      || (sh.st != stBlock && sh.st != stTypedef
	  && sh.st != stStruct && sh.st != stUnion
	  && sh.st != stEnum))
    {
      /* File indirect entry is corrupt.  */
      *pname = "<illegal>";
      complain (&bad_rfd_entry_complaint,
		sym_name, xref_fd, rn->index);
      return result;
    }

  *pname = ecoff_data (cur_bfd)->ss + fh->issBase + sh.iss;

  pend = is_pending_symbol (fh, esh);
  if (pend)
    *tpp = pend->t;
  else
    {
      /* We have not yet seen this type.  */

      if (sh.iss == 0 && sh.st == stTypedef)
	{
	  TIR tir;

	  /* alpha cc puts out a stTypedef with a sh.iss of zero for
	     two cases:
	     a) forward declarations of structs/unions/enums which are not
		defined in this compilation unit.
		For these the type will be void. This is a bad design decision
		as cross referencing across compilation units is impossible
		due to the missing name.
	     b) forward declarations of structs/unions/enums which are defined
		later in this file or in another file in the same compilation
		unit.  Simply cross reference those again to get the
		true type.
	     The forward references are not entered in the pending list and
	     in the symbol table.  */

	  ecoff_swap_tir_in (bigend,
			     &(ecoff_data (cur_bfd)->external_aux
			       + fh->iauxBase + sh.index)->a_ti,
			     &tir);
	  if (tir.tq0 != tqNil)
	    complain (&illegal_forward_tq0_complaint, sym_name);
	  switch (tir.bt)
	    {
	    case btVoid:
	      *tpp = init_type (type_code, 0, 0, (char *) NULL,
				current_objfile);
    	      *pname = "<undefined>";
	      break;

	    case btStruct:
	    case btUnion:
	    case btEnum:
	      cross_ref (xref_fd,
			 (ecoff_data (cur_bfd)->external_aux
			  + fh->iauxBase + sh.index + 1),
			 tpp, type_code, pname,
			 fh->fBigendian, sym_name);
	      break;

	    default:
	      complain (&illegal_forward_bt_complaint, tir.bt, sym_name);
	      *tpp = init_type (type_code, 0, 0, (char *) NULL,
				current_objfile);
	      break;
	    }
	  return result;
	}
      else if (sh.st == stTypedef)
	{
	  /* Parse the type for a normal typedef. This might recursively call
	     cross_ref till we get a non typedef'ed type.
	     FIXME: This is not correct behaviour, but gdb currently
	     cannot handle typedefs without type copying. But type copying is
	     impossible as we might have mutual forward references between
	     two files and the copied type would not get filled in when
	     we later parse its definition.   */
	  *tpp = parse_type (xref_fd,
			     ecoff_data (cur_bfd)->external_aux + fh->iauxBase,
			     sh.index,
			     (int *)NULL,
			     fh->fBigendian,
			     (ecoff_data (cur_bfd)->ss
			      + fh->issBase + sh.iss));
	}
      else
	{
	  /* Cross reference to a struct/union/enum which is defined
	     in another file in the same compilation unit but that file
	     has not been parsed yet.
	     Initialize the type only, it will be filled in when
	     it's definition is parsed.  */
	  *tpp = init_type (type_code, 0, 0, (char *) NULL, current_objfile);
	}
      add_pending (fh, esh, *tpp);
    }

  /* We used one auxent normally, two if we got a "next one" rf. */
  return result;
}


/* Quick&dirty lookup procedure, to avoid the MI ones that require
   keeping the symtab sorted */

static struct symbol *
mylookup_symbol (name, block, namespace, class)
     char *name;
     register struct block *block;
     enum namespace namespace;
     enum address_class class;
{
  register int bot, top, inc;
  register struct symbol *sym;

  bot = 0;
  top = BLOCK_NSYMS (block);
  inc = name[0];
  while (bot < top)
    {
      sym = BLOCK_SYM (block, bot);
      if (SYMBOL_NAME (sym)[0] == inc
	  && SYMBOL_NAMESPACE (sym) == namespace
	  && SYMBOL_CLASS (sym) == class
	  && strcmp (SYMBOL_NAME (sym), name) == 0)
	return sym;
      bot++;
    }
  block = BLOCK_SUPERBLOCK (block);
  if (block)
    return mylookup_symbol (name, block, namespace, class);
  return 0;
}


/* Add a new symbol S to a block B.
   Infrequently, we will need to reallocate the block to make it bigger.
   We only detect this case when adding to top_stack->cur_block, since
   that's the only time we know how big the block is.  FIXME.  */

static void
add_symbol (s, b)
     struct symbol *s;
     struct block *b;
{
  int nsyms = BLOCK_NSYMS (b)++;
  struct block *origb;
  struct parse_stack *stackp;

  if (b == top_stack->cur_block &&
      nsyms >= top_stack->maxsyms)
    {
      complain (&block_overflow_complaint, SYMBOL_NAME (s));
      /* In this case shrink_block is actually grow_block, since
		   BLOCK_NSYMS(b) is larger than its current size.  */
      origb = b;
      b = shrink_block (top_stack->cur_block, top_stack->cur_st);

      /* Now run through the stack replacing pointers to the
	 original block.  shrink_block has already done this
	 for the blockvector and BLOCK_FUNCTION.  */
      for (stackp = top_stack; stackp; stackp = stackp->next)
	{
	  if (stackp->cur_block == origb)
	    {
	      stackp->cur_block = b;
	      stackp->maxsyms = BLOCK_NSYMS (b);
	    }
	}
    }
  BLOCK_SYM (b, nsyms) = s;
}

/* Add a new block B to a symtab S */

static void
add_block (b, s)
     struct block *b;
     struct symtab *s;
{
  struct blockvector *bv = BLOCKVECTOR (s);

  bv = (struct blockvector *) xrealloc ((PTR) bv,
					(sizeof (struct blockvector)
					 + BLOCKVECTOR_NBLOCKS (bv)
					 * sizeof (bv->block)));
  if (bv != BLOCKVECTOR (s))
    BLOCKVECTOR (s) = bv;

  BLOCKVECTOR_BLOCK (bv, BLOCKVECTOR_NBLOCKS (bv)++) = b;
}

/* Add a new linenumber entry (LINENO,ADR) to a linevector LT.
   MIPS' linenumber encoding might need more than one byte
   to describe it, LAST is used to detect these continuation lines.

   Combining lines with the same line number seems like a bad idea.
   E.g: There could be a line number entry with the same line number after the
   prologue and GDB should not ignore it (this is a better way to find
   a prologue than mips_skip_prologue).
   But due to the compressed line table format there are line number entries
   for the same line which are needed to bridge the gap to the next
   line number entry. These entries have a bogus address info with them
   and we are unable to tell them from intended duplicate line number
   entries.
   This is another reason why -ggdb debugging format is preferable.  */

static int
add_line (lt, lineno, adr, last)
     struct linetable *lt;
     int lineno;
     CORE_ADDR adr;
     int last;
{
  if (last == 0)
    last = -2;			/* make sure we record first line */

  if (last == lineno)		/* skip continuation lines */
    return lineno;

  lt->item[lt->nitems].line = lineno;
  lt->item[lt->nitems++].pc = adr << 2;
  return lineno;
}

/* Sorting and reordering procedures */

/* Blocks with a smaller low bound should come first */

static int
compare_blocks (arg1, arg2)
     const PTR arg1;
     const PTR arg2;
{
  register int addr_diff;
  struct block **b1 = (struct block **) arg1;
  struct block **b2 = (struct block **) arg2;

  addr_diff = (BLOCK_START ((*b1))) - (BLOCK_START ((*b2)));
  if (addr_diff == 0)
    return (BLOCK_END ((*b2))) - (BLOCK_END ((*b1)));
  return addr_diff;
}

/* Sort the blocks of a symtab S.
   Reorder the blocks in the blockvector by code-address,
   as required by some MI search routines */

static void
sort_blocks (s)
     struct symtab *s;
{
  struct blockvector *bv = BLOCKVECTOR (s);

  if (BLOCKVECTOR_NBLOCKS (bv) <= 2)
    {
      /* Cosmetic */
      if (BLOCK_END (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK)) == 0)
	BLOCK_START (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK)) = 0;
      if (BLOCK_END (BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK)) == 0)
	BLOCK_START (BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK)) = 0;
      return;
    }
  /*
   * This is very unfortunate: normally all functions are compiled in
   * the order they are found, but if the file is compiled -O3 things
   * are very different.  It would be nice to find a reliable test
   * to detect -O3 images in advance.
   */
  if (BLOCKVECTOR_NBLOCKS (bv) > 3)
    qsort (&BLOCKVECTOR_BLOCK (bv, FIRST_LOCAL_BLOCK),
	   BLOCKVECTOR_NBLOCKS (bv) - FIRST_LOCAL_BLOCK,
	   sizeof (struct block *),
	   compare_blocks);

  {
    register CORE_ADDR high = 0;
    register int i, j = BLOCKVECTOR_NBLOCKS (bv);

    for (i = FIRST_LOCAL_BLOCK; i < j; i++)
      if (high < BLOCK_END (BLOCKVECTOR_BLOCK (bv, i)))
	high = BLOCK_END (BLOCKVECTOR_BLOCK (bv, i));
    BLOCK_END (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK)) = high;
  }

  BLOCK_START (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK)) =
    BLOCK_START (BLOCKVECTOR_BLOCK (bv, FIRST_LOCAL_BLOCK));

  BLOCK_START (BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK)) =
    BLOCK_START (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK));
  BLOCK_END (BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK)) =
    BLOCK_END (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK));
}


/* Constructor/restructor/destructor procedures */

/* Allocate a new symtab for NAME.  Needs an estimate of how many symbols
   MAXSYMS and linenumbers MAXLINES we'll put in it */

static struct symtab *
new_symtab (name, maxsyms, maxlines, objfile)
     char *name;
     int maxsyms;
     int maxlines;
     struct objfile *objfile;
{
  struct symtab *s = allocate_symtab (name, objfile);

  LINETABLE (s) = new_linetable (maxlines);

  /* All symtabs must have at least two blocks */
  BLOCKVECTOR (s) = new_bvect (2);
  BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), GLOBAL_BLOCK) = new_block (maxsyms);
  BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK) = new_block (maxsyms);
  BLOCK_SUPERBLOCK (BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK)) =
    BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), GLOBAL_BLOCK);

  s->free_code = free_linetable;

  return (s);
}

/* Allocate a new partial_symtab NAME */

static struct partial_symtab *
new_psymtab (name, objfile)
     char *name;
     struct objfile *objfile;
{
  struct partial_symtab *psymtab;

  psymtab = allocate_psymtab (name, objfile);

  /* Keep a backpointer to the file's symbols */

  psymtab->read_symtab_private = ((char *)
				  obstack_alloc (&objfile->psymbol_obstack,
						 sizeof (struct symloc)));
  memset ((PTR) psymtab->read_symtab_private, 0, sizeof (struct symloc));
  CUR_BFD (psymtab) = cur_bfd;

  /* The way to turn this into a symtab is to call... */
  psymtab->read_symtab = mipscoff_psymtab_to_symtab;
  return (psymtab);
}


/* Allocate a linetable array of the given SIZE.  Since the struct
   already includes one item, we subtract one when calculating the
   proper size to allocate.  */

static struct linetable *
new_linetable (size)
     int size;
{
  struct linetable *l;

  size = (size - 1) * sizeof (l->item) + sizeof (struct linetable);
  l = (struct linetable *) xmalloc (size);
  l->nitems = 0;
  return l;
}

/* Oops, too big. Shrink it.  This was important with the 2.4 linetables,
   I am not so sure about the 3.4 ones.

   Since the struct linetable already includes one item, we subtract one when
   calculating the proper size to allocate.  */

static struct linetable *
shrink_linetable (lt)
     struct linetable *lt;
{

  return (struct linetable *) xrealloc ((PTR) lt,
					(sizeof (struct linetable)
					 + ((lt->nitems - 1)
					    * sizeof (lt->item))));
}

/* Allocate and zero a new blockvector of NBLOCKS blocks. */

static struct blockvector *
new_bvect (nblocks)
     int nblocks;
{
  struct blockvector *bv;
  int size;

  size = sizeof (struct blockvector) + nblocks * sizeof (struct block *);
  bv = (struct blockvector *) xzalloc (size);

  BLOCKVECTOR_NBLOCKS (bv) = nblocks;

  return bv;
}

/* Allocate and zero a new block of MAXSYMS symbols */

static struct block *
new_block (maxsyms)
     int maxsyms;
{
  int size = sizeof (struct block) + (maxsyms - 1) * sizeof (struct symbol *);

  return (struct block *) xzalloc (size);
}

/* Ooops, too big. Shrink block B in symtab S to its minimal size.
   Shrink_block can also be used by add_symbol to grow a block.  */

static struct block *
shrink_block (b, s)
     struct block *b;
     struct symtab *s;
{
  struct block *new;
  struct blockvector *bv = BLOCKVECTOR (s);
  int i;

  /* Just reallocate it and fix references to the old one */

  new = (struct block *) xrealloc ((PTR) b,
				   (sizeof (struct block)
				    + ((BLOCK_NSYMS (b) - 1)
				       * sizeof (struct symbol *))));

  /* Should chase pointers to old one.  Fortunately, that`s just
	   the block`s function and inferior blocks */
  if (BLOCK_FUNCTION (new) && SYMBOL_BLOCK_VALUE (BLOCK_FUNCTION (new)) == b)
    SYMBOL_BLOCK_VALUE (BLOCK_FUNCTION (new)) = new;
  for (i = 0; i < BLOCKVECTOR_NBLOCKS (bv); i++)
    if (BLOCKVECTOR_BLOCK (bv, i) == b)
      BLOCKVECTOR_BLOCK (bv, i) = new;
    else if (BLOCK_SUPERBLOCK (BLOCKVECTOR_BLOCK (bv, i)) == b)
      BLOCK_SUPERBLOCK (BLOCKVECTOR_BLOCK (bv, i)) = new;
  return new;
}

/* Create a new symbol with printname NAME */

static struct symbol *
new_symbol (name)
     char *name;
{
  struct symbol *s = ((struct symbol *)
		      obstack_alloc (&current_objfile->symbol_obstack,
				     sizeof (struct symbol)));

  memset ((PTR) s, 0, sizeof (*s));
  SYMBOL_NAME (s) = name;
  SYMBOL_LANGUAGE (s) = psymtab_language;
  SYMBOL_INIT_DEMANGLED_NAME (s, &current_objfile->symbol_obstack);
  return s;
}

/* Create a new type with printname NAME */

static struct type *
new_type (name)
     char *name;
{
  struct type *t;

  t = alloc_type (current_objfile);
  TYPE_NAME (t) = name;
  TYPE_CPLUS_SPECIFIC (t) = (struct cplus_struct_type *) &cplus_struct_default;
  return t;
}


/* Things used for calling functions in the inferior.
   These functions are exported to our companion
   mips-tdep.c file and are here because they play
   with the symbol-table explicitly. */

/* Sigtramp: make sure we have all the necessary information
   about the signal trampoline code. Since the official code
   from MIPS does not do so, we make up that information ourselves.
   If they fix the library (unlikely) this code will neutralize itself. */

static void
fixup_sigtramp ()
{
  struct symbol *s;
  struct symtab *st;
  struct block *b, *b0 = NULL;

  sigtramp_address = -1;

  /* We have to handle the following cases here:
     a) The Mips library has a sigtramp label within sigvec.
     b) Irix has a _sigtramp which we want to use, but it also has sigvec.  */
  s = lookup_symbol ("sigvec", 0, VAR_NAMESPACE, 0, NULL);
  if (s != 0)
    {
      b0 = SYMBOL_BLOCK_VALUE (s);
      s = lookup_symbol ("sigtramp", b0, VAR_NAMESPACE, 0, NULL);
    }
  if (s == 0)
    {
      /* No sigvec or no sigtramp inside sigvec, try _sigtramp.  */
      s = lookup_symbol ("_sigtramp", 0, VAR_NAMESPACE, 0, NULL);
    }

  /* But maybe this program uses its own version of sigvec */
  if (s == 0)
    return;

  /* Did we or MIPSco fix the library ? */
  if (SYMBOL_CLASS (s) == LOC_BLOCK)
    {
      sigtramp_address = BLOCK_START (SYMBOL_BLOCK_VALUE (s));
      sigtramp_end = BLOCK_END (SYMBOL_BLOCK_VALUE (s));
      return;
    }

  sigtramp_address = SYMBOL_VALUE (s);
  sigtramp_end = sigtramp_address + 0x88;	/* black magic */

  /* But what symtab does it live in ? */
  st = find_pc_symtab (SYMBOL_VALUE (s));

  /*
   * Ok, there goes the fix: turn it into a procedure, with all the
   * needed info.  Note we make it a nested procedure of sigvec,
   * which is the way the (assembly) code is actually written.
   */
  SYMBOL_NAMESPACE (s) = VAR_NAMESPACE;
  SYMBOL_CLASS (s) = LOC_BLOCK;
  SYMBOL_TYPE (s) = init_type (TYPE_CODE_FUNC, 4, 0, (char *) NULL,
			       st->objfile);
  TYPE_TARGET_TYPE (SYMBOL_TYPE (s)) = builtin_type_void;

  /* Need a block to allocate MIPS_EFI_SYMBOL_NAME in */
  b = new_block (1);
  SYMBOL_BLOCK_VALUE (s) = b;
  BLOCK_START (b) = sigtramp_address;
  BLOCK_END (b) = sigtramp_end;
  BLOCK_FUNCTION (b) = s;
  BLOCK_SUPERBLOCK (b) = BLOCK_SUPERBLOCK (b0);
  add_block (b, st);
  sort_blocks (st);

  /* Make a MIPS_EFI_SYMBOL_NAME entry for it */
  {
    struct mips_extra_func_info *e =
      ((struct mips_extra_func_info *)
       xzalloc (sizeof (struct mips_extra_func_info)));

    e->numargs = 0;		/* the kernel thinks otherwise */
    /* align_longword(sigcontext + SIGFRAME) */
    e->pdr.frameoffset = 0x150;
    e->pdr.framereg = SP_REGNUM;
    /* read_next_frame_reg provides the true pc at the time of signal */
    e->pdr.pcreg = PC_REGNUM;
    e->pdr.regmask = -2;
    e->pdr.regoffset = -(41 * sizeof (int));
    e->pdr.fregmask = -1;
    e->pdr.fregoffset = -(7 * sizeof (int));
    e->pdr.isym = (long) s;
    e->pdr.adr = sigtramp_address;

    current_objfile = st->objfile;	/* Keep new_symbol happy */
    s = new_symbol (MIPS_EFI_SYMBOL_NAME);
    SYMBOL_VALUE (s) = (long) e;
    SYMBOL_NAMESPACE (s) = LABEL_NAMESPACE;
    SYMBOL_CLASS (s) = LOC_CONST;
    SYMBOL_TYPE (s) = builtin_type_void;
    current_objfile = NULL;
  }

  BLOCK_SYM (b, BLOCK_NSYMS (b)++) = s;
}


/* Fake up identical offsets for all sections.  */

struct section_offsets *
mipscoff_symfile_offsets (objfile, addr)
     struct objfile *objfile;
     CORE_ADDR addr;
{
  struct section_offsets *section_offsets;
  int i;

  section_offsets = ((struct section_offsets *)
		     obstack_alloc (&objfile->psymbol_obstack,
				    (sizeof (struct section_offsets)
				     + (sizeof (section_offsets->offsets)
					* (SECT_OFF_MAX - 1)))));

  for (i = 0; i < SECT_OFF_MAX; i++)
    ANOFFSET (section_offsets, i) = addr;

  return section_offsets;
}

/* Initialization */

static struct sym_fns ecoff_sym_fns =
{
  "ecoff",			/* sym_name: name or name prefix of BFD target type */
  5,				/* sym_namelen: number of significant sym_name chars */
  mipscoff_new_init,		/* sym_new_init: init anything gbl to entire symtab */
  mipscoff_symfile_init,	/* sym_init: read initial info, setup for sym_read() */
  mipscoff_symfile_read,	/* sym_read: read a symbol file into symtab */
  mipscoff_symfile_finish,	/* sym_finish: finished with file, cleanup */
  mipscoff_symfile_offsets,	/* sym_offsets: dummy FIXME til implem sym reloc */
  NULL				/* next: pointer to next struct sym_fns */
};


void
_initialize_mipsread ()
{
  add_symtab_fns (&ecoff_sym_fns);

  /* Missing basic types */

  builtin_type_string =
    init_type (TYPE_CODE_STRING,
	       TARGET_CHAR_BIT / TARGET_CHAR_BIT,
	       0, "string",
	       (struct objfile *) NULL);
  builtin_type_complex =
    init_type (TYPE_CODE_FLT,
	       TARGET_COMPLEX_BIT / TARGET_CHAR_BIT,
	       0, "complex",
	       (struct objfile *) NULL);
  builtin_type_double_complex =
    init_type (TYPE_CODE_FLT,
	       TARGET_DOUBLE_COMPLEX_BIT / TARGET_CHAR_BIT,
	       0, "double complex",
	       (struct objfile *) NULL);
  builtin_type_fixed_dec =
    init_type (TYPE_CODE_INT,
	       TARGET_INT_BIT / TARGET_CHAR_BIT,
	       0, "fixed decimal",
	       (struct objfile *) NULL);
  builtin_type_float_dec =
    init_type (TYPE_CODE_FLT,
	       TARGET_DOUBLE_BIT / TARGET_CHAR_BIT,
	       0, "floating decimal",
	       (struct objfile *) NULL);
}
