/* Read AIX xcoff symbol tables and convert to internal format, for GDB.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
   	     Free Software Foundation, Inc.
   Derived from coffread.c, dbxread.c, and a lot of hacking.
   Contributed by IBM Corporation.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* RS/6000 and PowerPC only:
   Needs xcoff_add_toc_to_loadinfo and xcoff_init_loadinfo in
   rs6000-tdep.c from target.
   However, if you define FAKING_RS6000, then this code will link with
   any target.  */

#include "defs.h"
#include "bfd.h"

#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include "gdb_string.h"

#include <sys/param.h>
#ifndef	NO_SYS_FILE
#include <sys/file.h>
#endif
#include "gdb_stat.h"

#include "coff/internal.h"
#include "libcoff.h"		/* FIXME, internal data from BFD */
#include "coff/rs6000.h"

#include "symtab.h"
#include "gdbtypes.h"
#include "symfile.h"
#include "objfiles.h"
#include "buildsym.h"
#include "stabsread.h"
#include "expression.h"
#include "language.h"		/* Needed inside partial-stab.h */
#include "complaints.h"

#include "gdb-stabs.h"

/* For interface with stabsread.c.  */
#include "aout/stab_gnu.h"

/* For interface with partial-stab.h.  */
#define N_UNDF	0	/* Undefined symbol */
#undef N_ABS
#define N_ABS 2
#define N_TEXT 	4	/* Text sym -- defined at offset in text seg */
#define N_DATA 	6	/* Data sym -- defined at offset in data seg */
#define N_BSS 	8	/* BSS  sym -- defined at offset in zero'd seg */
#define	N_COMM	0x12	/* Common symbol (visible after shared lib dynlink) */
#define N_FN	0x1f	/* File name of .o file */
#define	N_FN_SEQ 0x0C	/* N_FN from Sequent compilers (sigh) */
/* Note: N_EXT can only be usefully OR-ed with N_UNDF, N_ABS, N_TEXT,
   N_DATA, or N_BSS.  When the low-order bit of other types is set,
   (e.g. N_WARNING versus N_FN), they are two different types.  */
#define N_EXT 	1	/* External symbol (as opposed to local-to-this-file) */
#define N_INDR 0x0a

/* The following symbols refer to set elements.
   All the N_SET[ATDB] symbols with the same name form one set.
   Space is allocated for the set in the text section, and each set
   elements value is stored into one word of the space.
   The first word of the space is the length of the set (number of elements).

   The address of the set is made into an N_SETV symbol
   whose name is the same as the name of the set.
   This symbol acts like a N_DATA global symbol
   in that it can satisfy undefined external references.  */

/* These appear as input to LD, in a .o file.  */
#define	N_SETA	0x14		/* Absolute set element symbol */
#define	N_SETT	0x16		/* Text set element symbol */
#define	N_SETD	0x18		/* Data set element symbol */
#define	N_SETB	0x1A		/* Bss set element symbol */

/* This is output from LD.  */
#define N_SETV	0x1C		/* Pointer to set vector in data area.  */

/* We put a pointer to this structure in the read_symtab_private field
   of the psymtab.  */

struct symloc {

  /* First symbol number for this file.  */

  int first_symnum;

  /* Number of symbols in the section of the symbol table devoted to
     this file's symbols (actually, the section bracketed may contain
     more than just this file's symbols).  If numsyms is 0, the only
     reason for this thing's existence is the dependency list.  Nothing
     else will happen when it is read in.  */

  int numsyms;

  /* Position of the start of the line number information for this psymtab.  */
  unsigned int lineno_off;
};

/* Remember what we deduced to be the source language of this psymtab. */

static enum language psymtab_language = language_unknown;


/* Simplified internal version of coff symbol table information */

struct coff_symbol {
  char *c_name;
  int c_symnum;		/* symbol number of this entry */
  int c_naux;		/* 0 if syment only, 1 if syment + auxent */
  long c_value;
  unsigned char c_sclass;
  int c_secnum;
  unsigned int c_type;
};

/* last function's saved coff symbol `cs' */

static struct coff_symbol fcn_cs_saved;

static bfd *symfile_bfd;

/* Core address of start and end of text of current source file.
   This is calculated from the first function seen after a C_FILE
   symbol. */


static CORE_ADDR cur_src_end_addr;

/* Core address of the end of the first object file.  */

static CORE_ADDR first_object_file_end;

/* initial symbol-table-debug-string vector length */

#define	INITIAL_STABVECTOR_LENGTH	40

/* Nonzero if within a function (so symbols should be local,
   if nothing says specifically).  */

int within_function;

/* Size of a COFF symbol.  I think it is always 18, so I'm not sure
   there is any reason not to just use a #define, but might as well
   ask BFD for the size and store it here, I guess.  */

static unsigned	local_symesz;

struct coff_symfile_info {
  file_ptr min_lineno_offset;		/* Where in file lowest line#s are */
  file_ptr max_lineno_offset;		/* 1+last byte of line#s in file */

  /* Pointer to the string table.  */
  char *strtbl;

  /* Pointer to debug section.  */
  char *debugsec;

  /* Pointer to the a.out symbol table.  */
  char *symtbl;

  /* Number of symbols in symtbl.  */
  int symtbl_num_syms;
};

static struct complaint storclass_complaint =
  {"Unexpected storage class: %d", 0, 0};

static struct complaint bf_notfound_complaint =
  {"line numbers off, `.bf' symbol not found", 0, 0};

static struct complaint ef_complaint = 
  {"Mismatched .ef symbol ignored starting at symnum %d", 0, 0};

static struct complaint eb_complaint = 
  {"Mismatched .eb symbol ignored starting at symnum %d", 0, 0};

static void
enter_line_range PARAMS ((struct subfile *, unsigned, unsigned,
			  CORE_ADDR, CORE_ADDR, unsigned *));

static void
init_stringtab PARAMS ((bfd *, file_ptr, struct objfile *));

static void
xcoff_symfile_init PARAMS ((struct objfile *));

static void
xcoff_new_init PARAMS ((struct objfile *));

static void
xcoff_symfile_finish PARAMS ((struct objfile *));

static struct section_offsets *
xcoff_symfile_offsets PARAMS ((struct objfile *, CORE_ADDR));

static void
find_linenos PARAMS ((bfd *, sec_ptr, PTR));

static char *
coff_getfilename PARAMS ((union internal_auxent *, struct objfile *));

static void
read_symbol PARAMS ((struct internal_syment *, int));

static int
read_symbol_lineno PARAMS ((int));

static int
read_symbol_nvalue PARAMS ((int));

static struct symbol *
process_xcoff_symbol PARAMS ((struct coff_symbol *, struct objfile *));

static void
read_xcoff_symtab PARAMS ((struct partial_symtab *));

static void
add_stab_to_list PARAMS ((char *, struct pending_stabs **));


/* Translate from a COFF section number (target_index) to a SECT_OFF_*
   code.  */
static int secnum_to_section PARAMS ((int, struct objfile *));

struct find_targ_sec_arg {
  int targ_index;
  int *resultp;
};

static void find_targ_sec PARAMS ((bfd *, asection *, void *));

static void find_targ_sec (abfd, sect, obj)
     bfd *abfd;
     asection *sect;
     PTR obj;
{
  struct find_targ_sec_arg *args = (struct find_targ_sec_arg *)obj;
  if (sect->target_index == args->targ_index)
    {
      /* This is the section.  Figure out what SECT_OFF_* code it is.  */
      if (bfd_get_section_flags (abfd, sect) & SEC_CODE)
	*args->resultp = SECT_OFF_TEXT;
      else if (bfd_get_section_flags (abfd, sect) & SEC_LOAD)
	*args->resultp = SECT_OFF_DATA;
      else
	*args->resultp = SECT_OFF_BSS;
    }
}

/* Return the section number (SECT_OFF_*) that CS points to.  */
static int
secnum_to_section (secnum, objfile)
     int secnum;
     struct objfile *objfile;
{
  int off = SECT_OFF_TEXT;
  struct find_targ_sec_arg args;
  args.targ_index = secnum;
  args.resultp = &off;
  bfd_map_over_sections (objfile->obfd, find_targ_sec, &args);
  return off;
}

/* add a given stab string into given stab vector. */

static void
add_stab_to_list (stabname, stabvector)
char *stabname;
struct pending_stabs **stabvector;
{
  if ( *stabvector == NULL) {
    *stabvector = (struct pending_stabs *)
	xmalloc (sizeof (struct pending_stabs) + 
			INITIAL_STABVECTOR_LENGTH * sizeof (char*));
    (*stabvector)->count = 0;
    (*stabvector)->length = INITIAL_STABVECTOR_LENGTH;
  }
  else if ((*stabvector)->count >= (*stabvector)->length) {
    (*stabvector)->length += INITIAL_STABVECTOR_LENGTH;
    *stabvector = (struct pending_stabs *)
	xrealloc ((char *) *stabvector, sizeof (struct pending_stabs) + 
	(*stabvector)->length * sizeof (char*));
  }
  (*stabvector)->stab [(*stabvector)->count++] = stabname;
}

/* Linenos are processed on a file-by-file basis.

   Two reasons:

    1) xlc (IBM's native c compiler) postpones static function code
       emission to the end of a compilation unit. This way it can
       determine if those functions (statics) are needed or not, and
       can do some garbage collection (I think). This makes line
       numbers and corresponding addresses unordered, and we end up
       with a line table like:
       

		lineno	addr
        foo()	  10	0x100
		  20	0x200
		  30	0x300

	foo3()	  70	0x400
		  80	0x500
		  90	0x600

	static foo2()
		  40	0x700
		  50	0x800
		  60	0x900		

	and that breaks gdb's binary search on line numbers, if the
	above table is not sorted on line numbers. And that sort
	should be on function based, since gcc can emit line numbers
	like:
	
		10	0x100	- for the init/test part of a for stmt.
		20	0x200
		30	0x300
		10	0x400	- for the increment part of a for stmt.

	arrange_linetable() will do this sorting.		

     2)	aix symbol table might look like:

		c_file		// beginning of a new file
		.bi		// beginning of include file
		.ei		// end of include file
		.bi
		.ei

	basically, .bi/.ei pairs do not necessarily encapsulate
	their scope. They need to be recorded, and processed later
	on when we come the end of the compilation unit.
	Include table (inclTable) and process_linenos() handle
	that.  */

/* compare line table entry addresses. */

static int
compare_lte (lte1, lte2)
     struct linetable_entry *lte1, *lte2;
{
  return lte1->pc - lte2->pc;
}

/* Give a line table with function entries are marked, arrange its functions
   in assending order and strip off function entry markers and return it in
   a newly created table. If the old one is good enough, return the old one. */
/* FIXME: I think all this stuff can be replaced by just passing
   sort_linevec = 1 to end_symtab.  */

static struct linetable *
arrange_linetable (oldLineTb)
     struct linetable *oldLineTb;			/* old linetable */
{
  int ii, jj, 
      newline, 					/* new line count */
      function_count;				/* # of functions */

  struct linetable_entry *fentry;		/* function entry vector */
  int fentry_size;				/* # of function entries */
  struct linetable *newLineTb;			/* new line table */

#define NUM_OF_FUNCTIONS 20

  fentry_size = NUM_OF_FUNCTIONS;
  fentry = (struct linetable_entry*)
    xmalloc (fentry_size * sizeof (struct linetable_entry));

  for (function_count=0, ii=0; ii <oldLineTb->nitems; ++ii) {

    if (oldLineTb->item[ii].line == 0) {	/* function entry found. */

      if (function_count >= fentry_size) {	/* make sure you have room. */
	fentry_size *= 2;
	fentry = (struct linetable_entry*) 
	  xrealloc (fentry, fentry_size * sizeof (struct linetable_entry));
      }
      fentry[function_count].line = ii;
      fentry[function_count].pc = oldLineTb->item[ii].pc;
      ++function_count;
    }
  }

  if (function_count == 0) {
    free (fentry);
    return oldLineTb;
  }
  else if (function_count > 1)
    qsort (fentry, function_count, sizeof(struct linetable_entry), compare_lte);

  /* allocate a new line table. */
  newLineTb = (struct linetable *)
    xmalloc
      (sizeof (struct linetable) + 
       (oldLineTb->nitems - function_count) * sizeof (struct linetable_entry));

  /* if line table does not start with a function beginning, copy up until
     a function begin. */

  newline = 0;
  if (oldLineTb->item[0].line != 0)
    for (newline=0; 
	newline < oldLineTb->nitems && oldLineTb->item[newline].line; ++newline)
      newLineTb->item[newline] = oldLineTb->item[newline];

  /* Now copy function lines one by one. */

  for (ii=0; ii < function_count; ++ii) {
    for (jj = fentry[ii].line + 1;
	         jj < oldLineTb->nitems && oldLineTb->item[jj].line != 0; 
							 ++jj, ++newline)
      newLineTb->item[newline] = oldLineTb->item[jj];
  }
  free (fentry);
  newLineTb->nitems = oldLineTb->nitems - function_count;
  return newLineTb;  
}     

/* include file support: C_BINCL/C_EINCL pairs will be kept in the 
   following `IncludeChain'. At the end of each symtab (end_symtab),
   we will determine if we should create additional symtab's to
   represent if (the include files. */


typedef struct _inclTable {
  char		*name;				/* include filename */

  /* Offsets to the line table.  end points to the last entry which is
     part of this include file.  */
  int		begin, end;
  
  struct subfile *subfile;
  unsigned	funStartLine;			/* start line # of its function */
} InclTable;

#define	INITIAL_INCLUDE_TABLE_LENGTH	20
static InclTable  *inclTable;			/* global include table */
static int	  inclIndx;			/* last entry to table */
static int	  inclLength;			/* table length */
static int	  inclDepth;			/* nested include depth */

static void allocate_include_entry PARAMS ((void));

static void
record_include_begin (cs)
struct coff_symbol *cs;
{
  if (inclDepth)
    {
      /* In xcoff, we assume include files cannot be nested (not in .c files
	 of course, but in corresponding .s files.).  */

      /* This can happen with old versions of GCC.
	 GCC 2.3.3-930426 does not exhibit this on a test case which
	 a user said produced the message for him.  */
      static struct complaint msg = {"Nested C_BINCL symbols", 0, 0};
      complain (&msg);
    }
  ++inclDepth;

  allocate_include_entry ();

  inclTable [inclIndx].name  = cs->c_name;
  inclTable [inclIndx].begin = cs->c_value;
}

static void
record_include_end (cs)
struct coff_symbol *cs;
{
  InclTable *pTbl;  

  if (inclDepth == 0)
    {
      static struct complaint msg = {"Mismatched C_BINCL/C_EINCL pair", 0, 0};
      complain (&msg);
    }

  allocate_include_entry ();

  pTbl = &inclTable [inclIndx];
  pTbl->end = cs->c_value;

  --inclDepth;
  ++inclIndx;
}

static void
allocate_include_entry ()
{
  if (inclTable == NULL)
    {
      inclTable = (InclTable *) 
	xmalloc (sizeof (InclTable) * INITIAL_INCLUDE_TABLE_LENGTH);
      memset (inclTable,
	      '\0', sizeof (InclTable) * INITIAL_INCLUDE_TABLE_LENGTH);
      inclLength = INITIAL_INCLUDE_TABLE_LENGTH;
      inclIndx = 0;
    }
  else if (inclIndx >= inclLength)
    {
      inclLength += INITIAL_INCLUDE_TABLE_LENGTH;
      inclTable = (InclTable *) 
	xrealloc (inclTable, sizeof (InclTable) * inclLength);
      memset (inclTable + inclLength - INITIAL_INCLUDE_TABLE_LENGTH, 
	      '\0', sizeof (InclTable)*INITIAL_INCLUDE_TABLE_LENGTH);
    }
}

/* Global variable to pass the psymtab down to all the routines involved
   in psymtab to symtab processing.  */
static struct partial_symtab *this_symtab_psymtab;

/* given the start and end addresses of a compilation unit (or a csect,
   at times) process its lines and create appropriate line vectors. */

static void
process_linenos (start, end)
     CORE_ADDR start, end;
{
  int offset, ii;
  file_ptr max_offset =
    ((struct coff_symfile_info *)this_symtab_psymtab->objfile->sym_private)
      ->max_lineno_offset;

  /* subfile structure for the main compilation unit.  */
  struct subfile main_subfile;

  /* In the main source file, any time we see a function entry, we
     reset this variable to function's absolute starting line number.
     All the following line numbers in the function are relative to
     this, and we record absolute line numbers in record_line().  */

  unsigned int main_source_baseline = 0;

  unsigned *firstLine;

  offset =
    ((struct symloc *)this_symtab_psymtab->read_symtab_private)->lineno_off;
  if (offset == 0)
    goto return_after_cleanup;

  memset (&main_subfile, '\0', sizeof (main_subfile));

  if (inclIndx == 0)
    /* All source lines were in the main source file. None in include files. */

    enter_line_range (&main_subfile, offset, 0, start, end, 
    						&main_source_baseline);

  else
    {
      /* There was source with line numbers in include files.  */
      main_source_baseline = 0;
      for (ii=0; ii < inclIndx; ++ii)
	{
	  struct subfile *tmpSubfile;

	  /* If there is main file source before include file, enter it.  */
	  if (offset < inclTable[ii].begin)
	    {
	      enter_line_range
		(&main_subfile, offset, inclTable[ii].begin - LINESZ,
		 start, 0, &main_source_baseline);
	    }

	  /* Have a new subfile for the include file.  */

	  tmpSubfile = inclTable[ii].subfile =
	    (struct subfile *) xmalloc (sizeof (struct subfile));

	  memset (tmpSubfile, '\0', sizeof (struct subfile));
	  firstLine = &(inclTable[ii].funStartLine);

	  /* Enter include file's lines now.  */
	  enter_line_range (tmpSubfile, inclTable[ii].begin, 
			    inclTable[ii].end, start, 0, firstLine);

	  if (offset <= inclTable[ii].end)
	    offset = inclTable[ii].end + LINESZ;
	}

      /* All the include files' line have been processed at this point.  Now,
	 enter remaining lines of the main file, if any left.  */
      if (offset < max_offset + 1 - LINESZ)
	{
	  enter_line_range (&main_subfile, offset, 0, start, end, 
			    &main_source_baseline);
	}
    }

  /* Process main file's line numbers.  */
  if (main_subfile.line_vector)
    {
      struct linetable *lineTb, *lv;

      lv = main_subfile.line_vector;

      /* Line numbers are not necessarily ordered. xlc compilation will
	 put static function to the end. */

      lineTb = arrange_linetable (lv);
      if (lv == lineTb)
	{
	  current_subfile->line_vector = (struct linetable *)
	    xrealloc (lv, (sizeof (struct linetable)
			   + lv->nitems * sizeof (struct linetable_entry)));
	}
      else
	{
	  free (lv);
	  current_subfile->line_vector = lineTb;
	}

      current_subfile->line_vector_length = 
	current_subfile->line_vector->nitems;
    }

  /* Now, process included files' line numbers.  */

  for (ii=0; ii < inclIndx; ++ii)
    {
      if ((inclTable[ii].subfile)->line_vector) /* Useless if!!! FIXMEmgo */
	{
	  struct linetable *lineTb, *lv;

	  lv = (inclTable[ii].subfile)->line_vector;

	  /* Line numbers are not necessarily ordered. xlc compilation will
	     put static function to the end. */

	  lineTb = arrange_linetable (lv);

	  push_subfile ();

	  /* For the same include file, we might want to have more than one
	     subfile.  This happens if we have something like:

  		......
	        #include "foo.h"
		......
	 	#include "foo.h"
		......

	     while foo.h including code in it. (stupid but possible)
	     Since start_subfile() looks at the name and uses an
	     existing one if finds, we need to provide a fake name and
	     fool it.  */

#if 0
	  start_subfile (inclTable[ii].name, (char*)0);
#else
	  {
	    /* Pick a fake name that will produce the same results as this
	       one when passed to deduce_language_from_filename.  Kludge on
	       top of kludge.  */
	    char *fakename = strrchr (inclTable[ii].name, '.');
	    if (fakename == NULL)
	      fakename = " ?";
	    start_subfile (fakename, (char*)0);
	    free (current_subfile->name);
	  }
	  current_subfile->name = strdup (inclTable[ii].name);
#endif

	  if (lv == lineTb)
	    {
	      current_subfile->line_vector =
		(struct linetable *) xrealloc
		  (lv, (sizeof (struct linetable)
			+ lv->nitems * sizeof (struct linetable_entry)));

	    }
	  else
	    {
	      free (lv);
	      current_subfile->line_vector = lineTb;
	    }

	  current_subfile->line_vector_length = 
	    current_subfile->line_vector->nitems;
	  start_subfile (pop_subfile (), (char*)0);
	}
    }

 return_after_cleanup:

  /* We don't want to keep alloc/free'ing the global include file table.  */
  inclIndx = 0;

  /* Start with a fresh subfile structure for the next file.  */
  memset (&main_subfile, '\0', sizeof (struct subfile));
}

void
aix_process_linenos ()
{
  /* process line numbers and enter them into line vector */
  process_linenos (last_source_start_addr, cur_src_end_addr);
}


/* Enter a given range of lines into the line vector.
   can be called in the following two ways:
     enter_line_range (subfile, beginoffset, endoffset, startaddr, 0, firstLine)  or
     enter_line_range (subfile, beginoffset, 0, startaddr, endaddr, firstLine)

   endoffset points to the last line table entry that we should pay
   attention to.  */

static void
enter_line_range (subfile, beginoffset, endoffset, startaddr, endaddr,
		  firstLine)
     struct subfile *subfile;
     unsigned   beginoffset, endoffset;	/* offsets to line table */
     CORE_ADDR  startaddr, endaddr;
     unsigned   *firstLine;
{
  unsigned int curoffset;
  CORE_ADDR addr;
  struct external_lineno ext_lnno;
  struct internal_lineno int_lnno;
  unsigned int limit_offset;
  bfd *abfd;

  if (endoffset == 0 && startaddr == 0 && endaddr == 0)
    return;
  curoffset = beginoffset;
  limit_offset =
    ((struct coff_symfile_info *)this_symtab_psymtab->objfile->sym_private)
      ->max_lineno_offset;

  if (endoffset != 0)
    {
      if (endoffset >= limit_offset)
	{
	  static struct complaint msg =
	    {"Bad line table offset in C_EINCL directive", 0, 0};
	  complain (&msg);
	  return;
	}
      limit_offset = endoffset;
    }
  else
    limit_offset -= 1;
  abfd = this_symtab_psymtab->objfile->obfd;

  while (curoffset <= limit_offset)
    {
      bfd_seek (abfd, curoffset, SEEK_SET);
      bfd_read (&ext_lnno, sizeof (struct external_lineno), 1, abfd);
      bfd_coff_swap_lineno_in (abfd, &ext_lnno, &int_lnno);

      /* Find the address this line represents.  */
      addr = (int_lnno.l_lnno
	      ? int_lnno.l_addr.l_paddr
	      : read_symbol_nvalue (int_lnno.l_addr.l_symndx));
      addr += ANOFFSET (this_symtab_psymtab->objfile->section_offsets,
			SECT_OFF_TEXT);

      if (addr < startaddr || (endaddr && addr >= endaddr))
	return;

      if (int_lnno.l_lnno == 0)
	{
	  *firstLine = read_symbol_lineno (int_lnno.l_addr.l_symndx);
	  record_line (subfile, 0, addr);
	  --(*firstLine);
	}
      else
	record_line (subfile, *firstLine + int_lnno.l_lnno, addr);
      curoffset += LINESZ;
    }
}


/* Save the vital information for use when closing off the current file.
   NAME is the file name the symbols came from, START_ADDR is the first
   text address for the file, and SIZE is the number of bytes of text.  */

#define complete_symtab(name, start_addr) {	\
  last_source_file = savestring (name, strlen (name));	\
  last_source_start_addr = start_addr;			\
}


/* Refill the symbol table input buffer
   and set the variables that control fetching entries from it.
   Reports an error if no data available.
   This function can read past the end of the symbol table
   (into the string table) but this does no harm.  */

/* Reading symbol table has to be fast! Keep the followings as macros, rather
   than functions. */

#define	RECORD_MINIMAL_SYMBOL(NAME, ADDR, TYPE, SECTION, OBJFILE) \
{						\
  char *namestr;				\
  namestr = (NAME); \
  if (namestr[0] == '.') ++namestr; \
  prim_record_minimal_symbol_and_info (namestr, (ADDR), (TYPE), \
				       (char *)NULL, (SECTION), (OBJFILE)); \
  misc_func_recorded = 1;					\
}


/* xcoff has static blocks marked in `.bs', `.es' pairs. They cannot be
   nested. At any given time, a symbol can only be in one static block.
   This is the base address of current static block, zero if non exists. */
   
static int static_block_base = 0;

/* Section number for the current static block.  */

static int static_block_section = -1;

/* true if space for symbol name has been allocated. */

static int symname_alloced = 0;

/* Next symbol to read.  Pointer into raw seething symbol table.  */

static char *raw_symbol;

/* This is the function which stabsread.c calls to get symbol
   continuations.  */
static char *
xcoff_next_symbol_text (objfile)
     struct objfile *objfile;
{
  struct internal_syment symbol;
  static struct complaint msg =
    {"Unexpected symbol continuation", 0, 0};
  char *retval;
  /* FIXME: is this the same as the passed arg? */
  objfile = this_symtab_psymtab->objfile;

  bfd_coff_swap_sym_in (objfile->obfd, raw_symbol, &symbol);
  if (symbol.n_zeroes)
    {
      complain (&msg);

      /* Return something which points to '\0' and hope the symbol reading
	 code does something reasonable.  */
      retval = "";
    }
  else if (symbol.n_sclass & 0x80)
    {
      retval =
	((struct coff_symfile_info *)objfile->sym_private)->debugsec
	  + symbol.n_offset;
      raw_symbol +=
	coff_data (objfile->obfd)->local_symesz;
      ++symnum;
    }
  else
    {
      complain (&msg);

      /* Return something which points to '\0' and hope the symbol reading
	 code does something reasonable.  */
      retval = "";
    }
  return retval;
}

/* Read symbols for a given partial symbol table.  */

static void
read_xcoff_symtab (pst)
     struct partial_symtab *pst;
{
  struct objfile *objfile = pst->objfile;
  bfd *abfd = objfile->obfd;
  char *raw_auxptr;		/* Pointer to first raw aux entry for sym */
  char *strtbl = ((struct coff_symfile_info *)objfile->sym_private)->strtbl;
  char *debugsec =
    ((struct coff_symfile_info *)objfile->sym_private)->debugsec;

  struct internal_syment symbol[1];
  union internal_auxent main_aux;
  struct coff_symbol cs[1];
  CORE_ADDR file_start_addr = 0;
  CORE_ADDR file_end_addr = 0;

  int next_file_symnum = -1;
  unsigned int max_symnum;
  int just_started = 1;
  int depth = 0;
  int fcn_start_addr = 0;

  struct coff_symbol fcn_stab_saved;

  /* fcn_cs_saved is global because process_xcoff_symbol needs it. */
  union internal_auxent fcn_aux_saved;
  struct context_stack *new;

  char *filestring = " _start_ ";	/* Name of the current file. */

  char *last_csect_name;		/* last seen csect's name and value */
  CORE_ADDR last_csect_val;
  int last_csect_sec;

  this_symtab_psymtab = pst;

  /* Get the appropriate COFF "constants" related to the file we're
     handling. */
  local_symesz = coff_data (abfd)->local_symesz;

  last_source_file = NULL;
  last_csect_name = 0;
  last_csect_val = 0;

  start_stabs ();
  start_symtab (filestring, (char *)NULL, file_start_addr);
  symnum = ((struct symloc *)pst->read_symtab_private)->first_symnum;
  max_symnum =
    symnum + ((struct symloc *)pst->read_symtab_private)->numsyms;
  first_object_file_end = 0;

  raw_symbol =
    ((struct coff_symfile_info *) objfile->sym_private)->symtbl
      + symnum * local_symesz;

  while (symnum < max_symnum)
    {

      QUIT;			/* make this command interruptable.  */

      /* READ_ONE_SYMBOL (symbol, cs, symname_alloced); */
      /* read one symbol into `cs' structure. After processing the
	 whole symbol table, only string table will be kept in memory,
	 symbol table and debug section of xcoff will be freed. Thus
	 we can mark symbols with names in string table as
	 `alloced'. */
      {
	int ii;

	/* Swap and align the symbol into a reasonable C structure.  */
	bfd_coff_swap_sym_in (abfd, raw_symbol, symbol);

	cs->c_symnum = symnum;
	cs->c_naux = symbol->n_numaux;
	if (symbol->n_zeroes)
	  {
	    symname_alloced = 0;
	    /* We must use the original, unswapped, name here so the name field
	       pointed to by cs->c_name will persist throughout xcoffread.  If
	       we use the new field, it gets overwritten for each symbol.  */
	    cs->c_name = ((struct external_syment *)raw_symbol)->e.e_name;
	    /* If it's exactly E_SYMNMLEN characters long it isn't
	       '\0'-terminated.  */
	    if (cs->c_name[E_SYMNMLEN - 1] != '\0')
	      {
		char *p;
		p = obstack_alloc (&objfile->symbol_obstack, E_SYMNMLEN + 1);
		strncpy (p, cs->c_name, E_SYMNMLEN);
		p[E_SYMNMLEN] = '\0';
		cs->c_name = p;
		symname_alloced = 1;
	      }
	  }
	else if (symbol->n_sclass & 0x80)
	  {
	    cs->c_name = debugsec + symbol->n_offset;
	    symname_alloced = 0;
	  }
	else
	  {
	    /* in string table */
	    cs->c_name = strtbl + (int)symbol->n_offset;
	    symname_alloced = 1;
	  }
	cs->c_value = symbol->n_value;
	cs->c_sclass = symbol->n_sclass;
	cs->c_secnum = symbol->n_scnum;
	cs->c_type = (unsigned)symbol->n_type;

	raw_symbol += coff_data (abfd)->local_symesz;
	++symnum;

	/* Save addr of first aux entry.  */
	raw_auxptr = raw_symbol;

	/* Skip all the auxents associated with this symbol.  */
	for (ii = symbol->n_numaux; ii; --ii)
	  {
	    raw_symbol += coff_data (abfd)->local_auxesz;
	    ++symnum;
	  }
      }

      /* if symbol name starts with ".$" or "$", ignore it. */
      if (cs->c_name[0] == '$'
	  || (cs->c_name[1] == '$' && cs->c_name[0] == '.'))
	continue;

      if (cs->c_symnum == next_file_symnum && cs->c_sclass != C_FILE)
	{
	  if (last_source_file)
	    {
	      pst->symtab =
		end_symtab (cur_src_end_addr, objfile, SECT_OFF_TEXT);
	      end_stabs ();
	    }

	  start_stabs ();
	  start_symtab ("_globals_", (char *)NULL, (CORE_ADDR)0);
	  cur_src_end_addr = first_object_file_end;
	  /* done with all files, everything from here on is globals */
	}

      /* if explicitly specified as a function, treat is as one. */
      if (ISFCN(cs->c_type) && cs->c_sclass != C_TPDEF)
	{
	  bfd_coff_swap_aux_in (abfd, raw_auxptr, cs->c_type, cs->c_sclass,
				0, cs->c_naux, &main_aux);
	  goto function_entry_point;
	}

      if ((cs->c_sclass == C_EXT || cs->c_sclass == C_HIDEXT)
	  && cs->c_naux == 1)
	{
	  /* Dealing with a symbol with a csect entry.  */

#define	CSECT(PP) ((PP)->x_csect)
#define	CSECT_LEN(PP) (CSECT(PP).x_scnlen.l)
#define	CSECT_ALIGN(PP) (SMTYP_ALIGN(CSECT(PP).x_smtyp))
#define	CSECT_SMTYP(PP) (SMTYP_SMTYP(CSECT(PP).x_smtyp))
#define	CSECT_SCLAS(PP) (CSECT(PP).x_smclas)

	  /* Convert the auxent to something we can access.  */
	  bfd_coff_swap_aux_in (abfd, raw_auxptr, cs->c_type, cs->c_sclass,
				0, cs->c_naux, &main_aux);

	  switch (CSECT_SMTYP (&main_aux))
	    {

	    case XTY_ER:
	      /* Ignore all external references.  */
	      continue;

	    case XTY_SD:
	      /* A section description.  */
	      {
		switch (CSECT_SCLAS (&main_aux))
		  {

		  case XMC_PR:
		    {

		      /* A program csect is seen.  We have to allocate one
			 symbol table for each program csect.  Normally gdb
			 prefers one symtab for each source file.  In case
			 of AIX, one source file might include more than one
			 [PR] csect, and they don't have to be adjacent in
			 terms of the space they occupy in memory. Thus, one
			 single source file might get fragmented in the
			 memory and gdb's file start and end address
			 approach does not work!  GCC (and I think xlc) seem
			 to put all the code in the unnamed program csect.  */

		      if (last_csect_name)
			{
			  complete_symtab (filestring, file_start_addr);
			  cur_src_end_addr = file_end_addr;
			  end_symtab (file_end_addr, objfile, SECT_OFF_TEXT);
			  end_stabs ();
			  start_stabs ();
			  /* Give all csects for this source file the same
			     name.  */
			  start_symtab (filestring, NULL, (CORE_ADDR)0);
			}

		      /* If this is the very first csect seen,
			 basically `__start'. */
		      if (just_started)
			{
			  first_object_file_end
			    = cs->c_value + CSECT_LEN (&main_aux);
			  just_started = 0;
			}

		      file_start_addr =
			cs->c_value + ANOFFSET (objfile->section_offsets,
						SECT_OFF_TEXT);
		      file_end_addr = file_start_addr + CSECT_LEN (&main_aux);

		      if (cs->c_name && cs->c_name[0] == '.')
			{
			  last_csect_name = cs->c_name;
			  last_csect_val = cs->c_value;
			  last_csect_sec = secnum_to_section (cs->c_secnum, objfile);
			}
		    }
		    continue;

		    /* All other symbols are put into the minimal symbol
		       table only.  */

		  case XMC_RW:
		    continue;

		  case XMC_TC0:
		    continue;

		  case XMC_TC:
		    continue;

		  default:
		    /* Ignore the symbol.  */
		    continue;
		  }
	      }
	      break;

	    case XTY_LD:

	      switch (CSECT_SCLAS (&main_aux))
		{
		case XMC_PR:
		  /* a function entry point. */
		function_entry_point:

		  fcn_start_addr = cs->c_value;

		  /* save the function header info, which will be used
		     when `.bf' is seen. */
		  fcn_cs_saved = *cs;
		  fcn_aux_saved = main_aux;
		  continue;

		case XMC_GL:
		  /* shared library function trampoline code entry point. */
		  continue;

		case XMC_DS:
		  /* The symbols often have the same names as debug symbols for
		     functions, and confuse lookup_symbol.  */
		  continue;

		default:
		  /* xlc puts each variable in a separate csect, so we get
		     an XTY_SD for each variable.  But gcc puts several
		     variables in a csect, so that each variable only gets
		     an XTY_LD. This will typically be XMC_RW; I suspect
		     XMC_RO and XMC_BS might be possible too.
		     These variables are put in the minimal symbol table
		     only.  */
		  continue;
		}
	      break;

	    case XTY_CM:
	      /* Common symbols are put into the minimal symbol table only.  */
	      continue;

	    default:
	      break;
	    }
	}

      switch (cs->c_sclass)
	{

	case C_FILE:

	  /* c_value field contains symnum of next .file entry in table
	     or symnum of first global after last .file. */

	  next_file_symnum = cs->c_value;

	  /* Complete symbol table for last object file containing
	     debugging information. */

	  /* Whether or not there was a csect in the previous file, we
	     have to call `end_stabs' and `start_stabs' to reset
	     type_vector, line_vector, etc. structures.  */

	  complete_symtab (filestring, file_start_addr);
	  cur_src_end_addr = file_end_addr;
	  end_symtab (file_end_addr, objfile, SECT_OFF_TEXT);
	  end_stabs ();

	  /* XCOFF, according to the AIX 3.2 documentation, puts the filename
	     in cs->c_name.  But xlc 1.3.0.2 has decided to do things the
	     standard COFF way and put it in the auxent.  We use the auxent if
	     the symbol is ".file" and an auxent exists, otherwise use the symbol
	     itself.  Simple enough.  */
	  if (!strcmp (cs->c_name, ".file") && cs->c_naux > 0)
	    {
	      bfd_coff_swap_aux_in (abfd, raw_auxptr, cs->c_type, cs->c_sclass,
				    0, cs->c_naux, &main_aux);
	      filestring = coff_getfilename (&main_aux, objfile);
	    }
	  else
	    filestring = cs->c_name;

	  start_stabs ();
	  start_symtab (filestring, (char *)NULL, (CORE_ADDR)0);
	  last_csect_name = 0;

	  /* reset file start and end addresses. A compilation unit with no text
	     (only data) should have zero file boundaries. */
	  file_start_addr = file_end_addr = 0;
	  break;

	case C_FUN:
	  fcn_stab_saved = *cs;
	  break;

	case C_FCN:
	  if (STREQ (cs->c_name, ".bf"))
	    {
	      CORE_ADDR off = ANOFFSET (objfile->section_offsets,
					SECT_OFF_TEXT);
	      bfd_coff_swap_aux_in (abfd, raw_auxptr, cs->c_type, cs->c_sclass,
				    0, cs->c_naux, &main_aux);

	      within_function = 1;

	      new = push_context (0, fcn_start_addr + off);

	      new->name = define_symbol 
		(fcn_cs_saved.c_value + off,
		 fcn_stab_saved.c_name, 0, 0, objfile);
	      if (new->name != NULL)
		SYMBOL_SECTION (new->name) = SECT_OFF_TEXT;
	    }
	  else if (STREQ (cs->c_name, ".ef"))
	    {

	      bfd_coff_swap_aux_in (abfd, raw_auxptr, cs->c_type, cs->c_sclass,
				    0, cs->c_naux, &main_aux);

	      /* The value of .ef is the address of epilogue code;
		 not useful for gdb.  */
	      /* { main_aux.x_sym.x_misc.x_lnsz.x_lnno
		 contains number of lines to '}' */

	      if (context_stack_depth <= 0)
		{		/* We attempted to pop an empty context stack */
		  complain (&ef_complaint, cs->c_symnum);
		  within_function = 0;
		  break;
		}
	      new = pop_context ();
	      /* Stack must be empty now.  */
	      if (context_stack_depth > 0 || new == NULL)
		{
		  complain (&ef_complaint, cs->c_symnum);
		  within_function = 0;
		  break;
		}

	      finish_block (new->name, &local_symbols, new->old_blocks,
			    new->start_addr,
			    (fcn_cs_saved.c_value
			     + fcn_aux_saved.x_sym.x_misc.x_fsize
			     + ANOFFSET (objfile->section_offsets,
					 SECT_OFF_TEXT)),
			    objfile);
	      within_function = 0;
	    }
	  break;

	case C_BSTAT:
	  /* Begin static block.  */
	  {
	    struct internal_syment symbol;

	    read_symbol (&symbol, cs->c_value);
	    static_block_base = symbol.n_value;
	    static_block_section =
	      secnum_to_section (symbol.n_scnum, objfile);
	  }
	  break;

	case C_ESTAT:
	  /* End of static block.  */
	  static_block_base = 0;
	  static_block_section = -1;
	  break;

	case C_ARG:
	case C_REGPARM:
	case C_REG:
	case C_TPDEF:
	case C_STRTAG:
	case C_UNTAG:
	case C_ENTAG:
	  {
	    static struct complaint msg =
	      {"Unrecognized storage class %d.", 0, 0};
	    complain (&msg, cs->c_sclass);
	  }
	  break;

	case C_LABEL:
	case C_NULL:
	  /* Ignore these.  */
	  break;

	case C_HIDEXT:
	case C_STAT:
	  break;

	case C_BINCL:
	  /* beginning of include file */
	  /* In xlc output, C_BINCL/C_EINCL pair doesn't show up in sorted
	     order. Thus, when wee see them, we might not know enough info
	     to process them. Thus, we'll be saving them into a table 
	     (inclTable) and postpone their processing. */

	  record_include_begin (cs);
	  break;

	case C_EINCL:
	  /* End of include file.  */
	  /* See the comment after case C_BINCL.  */
	  record_include_end (cs);
	  break;

	case C_BLOCK:
	  if (STREQ (cs->c_name, ".bb"))
	    {
	      depth++;
	      new = push_context (depth,
				  (cs->c_value
				   + ANOFFSET (objfile->section_offsets,
					       SECT_OFF_TEXT)));
	    }
	  else if (STREQ (cs->c_name, ".eb"))
	    {
	      if (context_stack_depth <= 0)
		{		/* We attempted to pop an empty context stack */
		  complain (&eb_complaint, cs->c_symnum);
		  break;
		}
	      new = pop_context ();
	      if (depth-- != new->depth)
		{
		  complain (&eb_complaint, cs->c_symnum);
		  break;
		}
	      if (local_symbols && context_stack_depth > 0)
		{
		  /* Make a block for the local symbols within.  */
		  finish_block (new->name, &local_symbols, new->old_blocks,
				new->start_addr,
				(cs->c_value
				 + ANOFFSET (objfile->section_offsets,
					     SECT_OFF_TEXT)),
				objfile);
		}
	      local_symbols = new->locals;
	    }
	  break;

	default:
	  process_xcoff_symbol (cs, objfile);
	  break;
	}
    }

  if (last_source_file)
    {
      struct symtab *s;

      complete_symtab (filestring, file_start_addr);
      cur_src_end_addr = file_end_addr;
      s = end_symtab (file_end_addr, objfile, SECT_OFF_TEXT);
      /* When reading symbols for the last C_FILE of the objfile, try
         to make sure that we set pst->symtab to the symtab for the
         file, not to the _globals_ symtab.  I'm not sure whether this
         actually works right or when/if it comes up.  */
      if (pst->symtab == NULL)
	pst->symtab = s;
      end_stabs ();
    }
}

#define	SYMBOL_DUP(SYMBOL1, SYMBOL2)	\
  (SYMBOL2) = (struct symbol *)		\
  	obstack_alloc (&objfile->symbol_obstack, sizeof (struct symbol)); \
  *(SYMBOL2) = *(SYMBOL1);
  
 
#define	SYMNAME_ALLOC(NAME, ALLOCED)	\
  (ALLOCED) ? (NAME) : obstack_copy0 (&objfile->symbol_obstack, (NAME), strlen (NAME));


static struct type *func_symbol_type;
static struct type *var_symbol_type;

/* process one xcoff symbol. */

static struct symbol *
process_xcoff_symbol (cs, objfile)
  register struct coff_symbol *cs;
  struct objfile *objfile;
{
  struct symbol onesymbol;
  register struct symbol *sym = &onesymbol;
  struct symbol *sym2 = NULL;
  char *name, *pp;

  int sec;
  CORE_ADDR off;

  if (cs->c_secnum < 0)
    {
      /* The value is a register number, offset within a frame, etc.,
	 and does not get relocated.  */
      off = 0;
      sec = -1;
    }
  else
    {
      sec = secnum_to_section (cs->c_secnum, objfile);
      off = ANOFFSET (objfile->section_offsets, sec);
    }

  name = cs->c_name;
  if (name[0] == '.')
    ++name;

  memset (sym, '\0', sizeof (struct symbol));

  /* default assumptions */
  SYMBOL_VALUE (sym) = cs->c_value + off;
  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
  SYMBOL_SECTION (sym) = secnum_to_section (cs->c_secnum, objfile);

  if (ISFCN (cs->c_type))
    {
      /* At this point, we don't know the type of the function.  This
	 will be patched with the type from its stab entry later on in
	 patch_block_stabs (), unless the file was compiled without -g.  */

      SYMBOL_NAME (sym) = SYMNAME_ALLOC (name, symname_alloced);
      SYMBOL_TYPE (sym) = func_symbol_type;

      SYMBOL_CLASS (sym) = LOC_BLOCK;
      SYMBOL_DUP (sym, sym2);

      if (cs->c_sclass == C_EXT)
	add_symbol_to_list (sym2, &global_symbols);
      else if (cs->c_sclass == C_HIDEXT || cs->c_sclass == C_STAT)
	add_symbol_to_list (sym2, &file_symbols);
    }
  else
    {
      /* In case we can't figure out the type, provide default. */
      SYMBOL_TYPE (sym) = var_symbol_type;

      switch (cs->c_sclass)
	{
#if 0
	/* The values of functions and global symbols are now resolved
	   via the global_sym_chain in stabsread.c.  */
	case C_FUN:
	  if (fcn_cs_saved.c_sclass == C_EXT)
	    add_stab_to_list (name, &global_stabs);
	  else
	    add_stab_to_list (name, &file_stabs);
	  break;

	case C_GSYM:
	  add_stab_to_list (name, &global_stabs);
	  break;
#endif

	case C_BCOMM:
	  common_block_start (cs->c_name, objfile);
	  break;

	case C_ECOMM:
	  common_block_end (objfile);
	  break;

	default:
	  complain (&storclass_complaint, cs->c_sclass);
	  /* FALLTHROUGH */

	case C_DECL:
	case C_PSYM:
	case C_RPSYM:
	case C_ECOML:
	case C_LSYM:
	case C_RSYM:
	case C_GSYM:

	  {
	    sym = define_symbol (cs->c_value + off, cs->c_name, 0, 0, objfile);
	    if (sym != NULL)
	      {
		SYMBOL_SECTION (sym) = sec;
	      }
	    return sym;
	  }

	case C_STSYM:

	  /* For xlc (not GCC), the 'V' symbol descriptor is used for
	     all statics and we need to distinguish file-scope versus
	     function-scope using within_function.  We do this by
	     changing the string we pass to define_symbol to use 'S'
	     where we need to, which is not necessarily super-clean,
	     but seems workable enough.  */

	  if (*name == ':' || (pp = (char *) strchr(name, ':')) == NULL)
	    return NULL;

	  ++pp;
	  if (*pp == 'V' && !within_function)
	    *pp = 'S';
	  sym = define_symbol ((cs->c_value
				+ ANOFFSET (objfile->section_offsets,
					    static_block_section)),
			       cs->c_name, 0, 0, objfile);
	  if (sym != NULL)
	    {
	      SYMBOL_VALUE (sym) += static_block_base;
	      SYMBOL_SECTION (sym) = static_block_section;
	    }
	  return sym;

	}
    }
  return sym2;
}

/* Extract the file name from the aux entry of a C_FILE symbol.  Return
   only the last component of the name.  Result is in static storage and
   is only good for temporary use.  */

static char *
coff_getfilename (aux_entry, objfile)
     union internal_auxent *aux_entry;
     struct objfile *objfile;
{
  static char buffer[BUFSIZ];
  register char *temp;
  char *result;

  if (aux_entry->x_file.x_n.x_zeroes == 0)
    strcpy (buffer,
	    ((struct coff_symfile_info *)objfile->sym_private)->strtbl
	    + aux_entry->x_file.x_n.x_offset);
  else
    {
      strncpy (buffer, aux_entry->x_file.x_fname, FILNMLEN);
      buffer[FILNMLEN] = '\0';
    }
  result = buffer;

  /* FIXME: We should not be throwing away the information about what
     directory.  It should go into dirname of the symtab, or some such
     place.  */
  if ((temp = strrchr (result, '/')) != NULL)
    result = temp + 1;
  return (result);
}

/* Set *SYMBOL to symbol number symno in symtbl.  */
static void
read_symbol (symbol, symno)
     struct internal_syment *symbol;
     int symno;
{
  int nsyms =
    ((struct coff_symfile_info *)this_symtab_psymtab->objfile->sym_private)
      ->symtbl_num_syms;
  char *stbl = 
    ((struct coff_symfile_info *)this_symtab_psymtab->objfile->sym_private)
      ->symtbl;
  if (symno < 0 || symno >= nsyms)
    {
      static struct complaint msg =
	{"Invalid symbol offset", 0, 0};
      complain (&msg);
      symbol->n_value = 0;
      symbol->n_scnum = -1;
      return;
    }
  bfd_coff_swap_sym_in (this_symtab_psymtab->objfile->obfd,
			stbl + (symno*local_symesz),
			symbol);
}
  
/* Get value corresponding to symbol number symno in symtbl.  */

static int
read_symbol_nvalue (symno)
     int symno;
{
  struct internal_syment symbol[1];

  read_symbol (symbol, symno);
  return symbol->n_value;  
}


/* Find the address of the function corresponding to symno, where
   symno is the symbol pointed to by the linetable.  */

static int
read_symbol_lineno (symno)
     int symno;
{
  int nsyms =
    ((struct coff_symfile_info *)this_symtab_psymtab->objfile->sym_private)
      ->symtbl_num_syms;
  char *stbl = 
    ((struct coff_symfile_info *)this_symtab_psymtab->objfile->sym_private)
      ->symtbl;
  struct internal_syment symbol[1];
  union internal_auxent main_aux[1];

  if (symno < 0)
    {
      complain (&bf_notfound_complaint);
      return 0;
    }

  /* Note that just searching for a short distance (e.g. 50 symbols)
     is not enough, at least in the following case.

     .extern foo
     [many .stabx entries]
     [a few functions, referring to foo]
     .globl foo
     .bf

     What happens here is that the assembler moves the .stabx entries
     to right before the ".bf" for foo, but the symbol for "foo" is before
     all the stabx entries.  See PR gdb/2222.  */

  /* Maintaining a table of .bf entries might be preferable to this search.
     If I understand things correctly it would need to be done only for
     the duration of a single psymtab to symtab conversion.  */
  while (symno < nsyms)
    {
      bfd_coff_swap_sym_in (symfile_bfd,
			    stbl + (symno * local_symesz), symbol);
      if (symbol->n_sclass == C_FCN && STREQ (symbol->n_name, ".bf"))
	goto gotit;
      symno += symbol->n_numaux + 1;
    }

  complain (&bf_notfound_complaint);
  return 0;

gotit:
  /* take aux entry and return its lineno */
  symno++;
  bfd_coff_swap_aux_in (this_symtab_psymtab->objfile->obfd,
			stbl + symno * local_symesz,
			symbol->n_type, symbol->n_sclass,
			0, symbol->n_numaux, main_aux);

  return main_aux->x_sym.x_misc.x_lnsz.x_lnno;
}

/* Support for line number handling */

/* This function is called for every section; it finds the outer limits
 * of the line table (minimum and maximum file offset) so that the
 * mainline code can read the whole thing for efficiency.
 */
static void
find_linenos (abfd, asect, vpinfo)
     bfd *abfd;
     sec_ptr asect;
     PTR vpinfo; 
{
  struct coff_symfile_info *info;
  int size, count;
  file_ptr offset, maxoff;

  count = asect->lineno_count;

  if (!STREQ (asect->name, ".text") || count == 0)
    return;

  size = count * coff_data (abfd)->local_linesz;
  info = (struct coff_symfile_info *)vpinfo;
  offset = asect->line_filepos;
  maxoff = offset + size;

  if (offset < info->min_lineno_offset || info->min_lineno_offset == 0)
    info->min_lineno_offset = offset;

  if (maxoff > info->max_lineno_offset)
    info->max_lineno_offset = maxoff;
}

static void xcoff_psymtab_to_symtab_1 PARAMS ((struct partial_symtab *));

static void
xcoff_psymtab_to_symtab_1 (pst)
     struct partial_symtab *pst;
{
  struct cleanup *old_chain;
  int i;
  
  if (!pst)
    return;

  if (pst->readin)
    {
      fprintf_unfiltered
	(gdb_stderr, "Psymtab for %s already read in.  Shouldn't happen.\n",
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
	    wrap_here ("");		/* Flush output */
	    gdb_flush (gdb_stdout);
	  }
	xcoff_psymtab_to_symtab_1 (pst->dependencies[i]);
      }

  if (((struct symloc *)pst->read_symtab_private)->numsyms != 0)
    {
      /* Init stuff necessary for reading in symbols.  */
      stabsread_init ();
      buildsym_init ();
      old_chain = make_cleanup (really_free_pendings, 0);

      read_xcoff_symtab (pst);
      sort_symtab_syms (pst->symtab);

      do_cleanups (old_chain);
    }

  pst->readin = 1;
}

static void xcoff_psymtab_to_symtab PARAMS ((struct partial_symtab *));

/* Read in all of the symbols for a given psymtab for real.
   Be verbose about it if the user wants that.  */

static void
xcoff_psymtab_to_symtab (pst)
     struct partial_symtab *pst;
{
  bfd *sym_bfd;

  if (!pst)
    return;

  if (pst->readin)
    {
      fprintf_unfiltered
	(gdb_stderr, "Psymtab for %s already read in.  Shouldn't happen.\n",
	 pst->filename);
      return;
    }

  if (((struct symloc *)pst->read_symtab_private)->numsyms != 0
      || pst->number_of_dependencies)
    {
      /* Print the message now, before reading the string table,
	 to avoid disconcerting pauses.  */
      if (info_verbose)
	{
	  printf_filtered ("Reading in symbols for %s...", pst->filename);
	  gdb_flush (gdb_stdout);
	}

      sym_bfd = pst->objfile->obfd;

      next_symbol_text_func = xcoff_next_symbol_text;

      xcoff_psymtab_to_symtab_1 (pst);

      /* Match with global symbols.  This only needs to be done once,
         after all of the symtabs and dependencies have been read in.   */
      scan_file_globals (pst->objfile);

      /* Finish up the debug error message.  */
      if (info_verbose)
	printf_filtered ("done.\n");
    }
}

static void
xcoff_new_init (objfile)
     struct objfile *objfile;
{
  stabsread_new_init ();
  buildsym_new_init ();
}

/* Do initialization in preparation for reading symbols from OBJFILE.
 
   We will only be called if this is an XCOFF or XCOFF-like file.
   BFD handles figuring out the format of the file, and code in symfile.c
   uses BFD's determination to vector to us.  */

static void
xcoff_symfile_init (objfile)
     struct objfile *objfile;
{
  /* Allocate struct to keep track of the symfile */
  objfile -> sym_private = xmmalloc (objfile -> md,
				     sizeof (struct coff_symfile_info));

  /* XCOFF objects may be reordered, so set OBJF_REORDERED.  If we
     find this causes a significant slowdown in gdb then we could
     set it in the debug symbol readers only when necessary.  */
  objfile->flags |= OBJF_REORDERED;

  init_entry_point_info (objfile);
}

/* Perform any local cleanups required when we are done with a particular
   objfile.  I.E, we are in the process of discarding all symbol information
   for an objfile, freeing up all memory held for it, and unlinking the
   objfile struct from the global list of known objfiles. */

static void
xcoff_symfile_finish (objfile)
     struct objfile *objfile;
{
  if (objfile -> sym_private != NULL)
    {
      mfree (objfile -> md, objfile -> sym_private);
    }

  /* Start with a fresh include table for the next objfile.  */
  if (inclTable)
    {
      free (inclTable);
      inclTable = NULL;
    }
  inclIndx = inclLength = inclDepth = 0;
}


static void
init_stringtab (abfd, offset, objfile)
     bfd *abfd;
     file_ptr offset;
     struct objfile *objfile;
{
  long length;
  int val;
  unsigned char lengthbuf[4];
  char *strtbl;

  ((struct coff_symfile_info *)objfile->sym_private)->strtbl = NULL;

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    error ("cannot seek to string table in %s: %s",
	   bfd_get_filename (abfd), bfd_errmsg (bfd_get_error ()));

  val = bfd_read ((char *)lengthbuf, 1, sizeof lengthbuf, abfd);
  length = bfd_h_get_32 (abfd, lengthbuf);

  /* If no string table is needed, then the file may end immediately
     after the symbols.  Just return with `strtbl' set to NULL.  */

  if (val != sizeof lengthbuf || length < sizeof lengthbuf)
    return;

  /* Allocate string table from symbol_obstack. We will need this table
     as long as we have its symbol table around. */

  strtbl = (char *) obstack_alloc (&objfile->symbol_obstack, length);
  ((struct coff_symfile_info *)objfile->sym_private)->strtbl = strtbl;

  /* Copy length buffer, the first byte is usually zero and is
     used for stabs with a name length of zero.  */
  memcpy (strtbl, lengthbuf, sizeof lengthbuf);
  if (length == sizeof lengthbuf)
    return;

  val = bfd_read (strtbl + sizeof lengthbuf, 1, length - sizeof lengthbuf,
		  abfd);

  if (val != length - sizeof lengthbuf)
    error ("cannot read string table from %s: %s",
	   bfd_get_filename (abfd), bfd_errmsg (bfd_get_error ()));
  if (strtbl[length - 1] != '\0')
    error ("bad symbol file: string table does not end with null character");

  return;
}

/* If we have not yet seen a function for this psymtab, this is 0.  If we
   have seen one, it is the offset in the line numbers of the line numbers
   for the psymtab.  */
static unsigned int first_fun_line_offset;

static struct partial_symtab *xcoff_start_psymtab
  PARAMS ((struct objfile *, struct section_offsets *, char *, int,
	   struct partial_symbol **, struct partial_symbol **));

/* Allocate and partially fill a partial symtab.  It will be
   completely filled at the end of the symbol list.

   SYMFILE_NAME is the name of the symbol-file we are reading from, and ADDR
   is the address relative to which its symbols are (incremental) or 0
   (normal). */

static struct partial_symtab *
xcoff_start_psymtab (objfile, section_offsets,
		     filename, first_symnum, global_syms, static_syms)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     char *filename;
     int first_symnum;
     struct partial_symbol **global_syms;
     struct partial_symbol **static_syms;
{
  struct partial_symtab *result =
    start_psymtab_common (objfile, section_offsets,
			  filename,
			  /* We fill in textlow later.  */
			  0,
			  global_syms, static_syms);

  result->read_symtab_private = (char *)
    obstack_alloc (&objfile -> psymbol_obstack, sizeof (struct symloc));
  ((struct symloc *)result->read_symtab_private)->first_symnum = first_symnum;
  result->read_symtab = xcoff_psymtab_to_symtab;

  /* Deduce the source language from the filename for this psymtab. */
  psymtab_language = deduce_language_from_filename (filename);

  return result;
}

static struct partial_symtab *xcoff_end_psymtab
  PARAMS ((struct partial_symtab *, char **, int, int,
	   struct partial_symtab **, int));

/* Close off the current usage of PST.  
   Returns PST, or NULL if the partial symtab was empty and thrown away.

   CAPPING_SYMBOL_NUMBER is the end of pst (exclusive).

   INCLUDE_LIST, NUM_INCLUDES, DEPENDENCY_LIST, and NUMBER_DEPENDENCIES
   are the information for includes and dependencies.  */

static struct partial_symtab *
xcoff_end_psymtab (pst, include_list, num_includes, capping_symbol_number,
		   dependency_list, number_dependencies)
     struct partial_symtab *pst;
     char **include_list;
     int num_includes;
     int capping_symbol_number;
     struct partial_symtab **dependency_list;
     int number_dependencies;
{
  int i;
  struct objfile *objfile = pst -> objfile;

  if (capping_symbol_number != -1)
    ((struct symloc *)pst->read_symtab_private)->numsyms =
      capping_symbol_number
	- ((struct symloc *)pst->read_symtab_private)->first_symnum;
  ((struct symloc *)pst->read_symtab_private)->lineno_off =
    first_fun_line_offset;
  first_fun_line_offset = 0;
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
      ((struct symloc *)subpst->read_symtab_private)->first_symnum = 0;
      ((struct symloc *)subpst->read_symtab_private)->numsyms = 0;
      subpst->textlow = 0;
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

  /* If there is already a psymtab or symtab for a file of this name,
     remove it.  (If there is a symtab, more drastic things also
     happen.)  This happens in VxWorks.  */
  free_named_symtabs (pst->filename);

  if (num_includes == 0
      && number_dependencies == 0
      && pst->n_global_syms == 0
      && pst->n_static_syms == 0)
    {
      /* Throw away this psymtab, it's empty.  We can't deallocate it, since
	 it is on the obstack, but we can forget to chain it on the list.  */
      /* Empty psymtabs happen as a result of header files which don't have
	 any symbols in them.  There can be a lot of them.  */
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

static void swap_sym PARAMS ((struct internal_syment *,
			      union internal_auxent *, char **, char **,
			      unsigned int *,
			      struct objfile *));

/* Swap raw symbol at *RAW and put the name in *NAME, the symbol in
   *SYMBOL, the first auxent in *AUX.  Advance *RAW and *SYMNUMP over
   the symbol and its auxents.  */

static void
swap_sym (symbol, aux, name, raw, symnump, objfile)
     struct internal_syment *symbol;
     union internal_auxent *aux;
     char **name;
     char **raw;
     unsigned int *symnump;
     struct objfile *objfile;
{
  bfd_coff_swap_sym_in (objfile->obfd, *raw, symbol);
  if (symbol->n_zeroes)
    {
      /* If it's exactly E_SYMNMLEN characters long it isn't
	 '\0'-terminated.  */
      if (symbol->n_name[E_SYMNMLEN - 1] != '\0')
	{
	  /* FIXME: wastes memory for symbols which we don't end up putting
	     into the minimal symbols.  */
	  char *p;
	  p = obstack_alloc (&objfile->psymbol_obstack, E_SYMNMLEN + 1);
	  strncpy (p, symbol->n_name, E_SYMNMLEN);
	  p[E_SYMNMLEN] = '\0';
	  *name = p;
	}
      else
	/* Point to the unswapped name as that persists as long as the
	   objfile does.  */
	*name = ((struct external_syment *)*raw)->e.e_name;
    }
  else if (symbol->n_sclass & 0x80)
    {
      *name = ((struct coff_symfile_info *)objfile->sym_private)->debugsec
	+ symbol->n_offset;
    }
  else
    {
      *name = ((struct coff_symfile_info *)objfile->sym_private)->strtbl
	+ symbol->n_offset;
    }
  ++*symnump;
  *raw += coff_data (objfile->obfd)->local_symesz;
  if (symbol->n_numaux > 0)
    {
      bfd_coff_swap_aux_in (objfile->obfd, *raw, symbol->n_type,
			    symbol->n_sclass, 0, symbol->n_numaux, aux);

      *symnump += symbol->n_numaux;
      *raw += coff_data (objfile->obfd)->local_symesz * symbol->n_numaux;
    }
}

static void
scan_xcoff_symtab (section_offsets, objfile)
     struct section_offsets *section_offsets;
     struct objfile *objfile;
{
  int toc_offset = 0;		/* toc offset value in data section. */
  char *filestring = NULL;

  char *namestring;
  int past_first_source_file = 0;
  bfd *abfd;
  unsigned int nsyms;

  /* Current partial symtab */
  struct partial_symtab *pst;

  /* List of current psymtab's include files */
  char **psymtab_include_list;
  int includes_allocated;
  int includes_used;

  /* Index within current psymtab dependency list */
  struct partial_symtab **dependency_list;
  int dependencies_used, dependencies_allocated;

  char *sraw_symbol;
  struct internal_syment symbol;
  union internal_auxent main_aux;
  unsigned int ssymnum;

  char *last_csect_name = NULL;		/* last seen csect's name and value */
  CORE_ADDR last_csect_val = 0;
  int last_csect_sec = 0;
  int  misc_func_recorded = 0;		/* true if any misc. function */

  pst = (struct partial_symtab *) 0;

  includes_allocated = 30;
  includes_used = 0;
  psymtab_include_list = (char **) alloca (includes_allocated *
					   sizeof (char *));

  dependencies_allocated = 30;
  dependencies_used = 0;
  dependency_list =
    (struct partial_symtab **) alloca (dependencies_allocated *
				       sizeof (struct partial_symtab *));

  last_source_file = NULL;

  abfd = objfile->obfd;

  sraw_symbol = ((struct coff_symfile_info *)objfile->sym_private)->symtbl;
  nsyms = ((struct coff_symfile_info *)objfile->sym_private)->symtbl_num_syms;
  ssymnum = 0;
  while (ssymnum < nsyms)
    {
      int sclass = ((struct external_syment *)sraw_symbol)->e_sclass[0] & 0xff;
      /* This is the type we pass to partial-stab.h.  A less kludgy solution
	 would be to break out partial-stab.h into its various parts--shuffle
	 off the DBXREAD_ONLY stuff to dbxread.c, and make separate
	 pstab-norm.h (for most types), pstab-sol.h (for N_SOL), etc.  */
      int stype;

      QUIT;

      switch (sclass)
	{
	case C_EXT:
	case C_HIDEXT:
	  {
	    /* The CSECT auxent--always the last auxent.  */
	    union internal_auxent csect_aux;
	    unsigned int symnum_before = ssymnum;

	    swap_sym (&symbol, &main_aux, &namestring, &sraw_symbol,
		      &ssymnum, objfile);
	    if (symbol.n_numaux > 1)
	      {
		bfd_coff_swap_aux_in
		  (objfile->obfd,
		   sraw_symbol - coff_data(abfd)->local_symesz,
		   symbol.n_type,
		   symbol.n_sclass,
		   symbol.n_numaux - 1,
		   symbol.n_numaux,
		   &csect_aux);
	      }
	    else
	      csect_aux = main_aux;

	    /* If symbol name starts with ".$" or "$", ignore it.  */
	    if (namestring[0] == '$'
		|| (namestring[0] == '.' && namestring[1] == '$'))
	      break;

	    switch (csect_aux.x_csect.x_smtyp & 0x7)
	      {
	      case XTY_SD:
		switch (csect_aux.x_csect.x_smclas)
		  {
		  case XMC_PR:
		    if (last_csect_name)
		      {
			/* If no misc. function recorded in the last
			   seen csect, enter it as a function. This
			   will take care of functions like strcmp()
			   compiled by xlc.  */

			if (!misc_func_recorded)
			  {
			    RECORD_MINIMAL_SYMBOL
			      (last_csect_name, last_csect_val,
			       mst_text, last_csect_sec,
			       objfile);
			  }

			if (pst != NULL)
			  {
			    /* We have to allocate one psymtab for
			       each program csect, because their text
			       sections need not be adjacent.  */
			    xcoff_end_psymtab
			      (pst, psymtab_include_list,
			       includes_used,
			       symnum_before,
			       dependency_list, dependencies_used);
			    includes_used = 0;
			    dependencies_used = 0;
			    /* Give all psymtabs for this source file the same
			       name.  */
			    pst = xcoff_start_psymtab
			      (objfile, section_offsets,
			       filestring,
			       symnum_before,
			       objfile->global_psymbols.next,
			       objfile->static_psymbols.next);
			  }
		      }
		    if (namestring && namestring[0] == '.')
		      {
			last_csect_name = namestring;
			last_csect_val = symbol.n_value;
			last_csect_sec =
			  secnum_to_section (symbol.n_scnum, objfile);
		      }
		    if (pst != NULL)
		      {
			CORE_ADDR highval =
			  symbol.n_value + csect_aux.x_csect.x_scnlen.l;
			if (highval > pst->texthigh)
			  pst->texthigh = highval;
			if (pst->textlow == 0 || symbol.n_value < pst->textlow)
			  pst->textlow = symbol.n_value;
		      }
		    misc_func_recorded = 0;
		    break;

		  case XMC_RW:
		    /* Data variables are recorded in the minimal symbol
		       table, except for section symbols.  */
		    if (*namestring != '.')
		      prim_record_minimal_symbol_and_info
			(namestring, symbol.n_value,
			 sclass == C_HIDEXT ? mst_file_data : mst_data,
			 NULL, secnum_to_section (symbol.n_scnum, objfile),
			 objfile);
		    break;

		  case XMC_TC0:
		    if (toc_offset)
		      warning ("More than one XMC_TC0 symbol found.");
		    toc_offset = symbol.n_value;
		    break;

		  case XMC_TC:
		    /* These symbols tell us where the TOC entry for a
		       variable is, not the variable itself.  */
		    break;

		  default:
		    break;
		  }
		break;

	      case XTY_LD:
		switch (csect_aux.x_csect.x_smclas)
		  {
		  case XMC_PR:
		    /* A function entry point.  */

		    if (first_fun_line_offset == 0 && symbol.n_numaux > 1)
		      first_fun_line_offset =
			main_aux.x_sym.x_fcnary.x_fcn.x_lnnoptr;
		    RECORD_MINIMAL_SYMBOL
		      (namestring, symbol.n_value,
		       sclass == C_HIDEXT ? mst_file_text : mst_text,
		       secnum_to_section (symbol.n_scnum, objfile),
		       objfile);
		    break;

		  case XMC_GL:
		    /* shared library function trampoline code entry
		       point. */

		    /* record trampoline code entries as
		       mst_solib_trampoline symbol.  When we lookup mst
		       symbols, we will choose mst_text over
		       mst_solib_trampoline. */
		    RECORD_MINIMAL_SYMBOL
		      (namestring, symbol.n_value,
		       mst_solib_trampoline,
		       secnum_to_section (symbol.n_scnum, objfile),
		       objfile);
		    break;

		  case XMC_DS:
		    /* The symbols often have the same names as
		       debug symbols for functions, and confuse
		       lookup_symbol.  */
		    break;

		  default:

		    /* xlc puts each variable in a separate csect,
		       so we get an XTY_SD for each variable.  But
		       gcc puts several variables in a csect, so
		       that each variable only gets an XTY_LD.  We
		       still need to record them.  This will
		       typically be XMC_RW; I suspect XMC_RO and
		       XMC_BS might be possible too.  */
		    if (*namestring != '.')
		      prim_record_minimal_symbol_and_info
			(namestring, symbol.n_value,
			 sclass == C_HIDEXT ? mst_file_data : mst_data,
			 NULL, secnum_to_section (symbol.n_scnum, objfile),
			 objfile);
		    break;
		  }
		break;

	      case XTY_CM:
		switch (csect_aux.x_csect.x_smclas)
		  {
		  case XMC_RW:
		  case XMC_BS:
		    /* Common variables are recorded in the minimal symbol
		       table, except for section symbols.  */
		    if (*namestring != '.')
		      prim_record_minimal_symbol_and_info
			(namestring, symbol.n_value,
			 sclass == C_HIDEXT ? mst_file_bss : mst_bss,
			 NULL, secnum_to_section (symbol.n_scnum, objfile),
			 objfile);
		    break;
		  }
		break;

	      default:
		break;
	      }
	  }
	  break;
	case C_FILE:
	  {
	    unsigned int symnum_before;

	    symnum_before = ssymnum;
	    swap_sym (&symbol, &main_aux, &namestring, &sraw_symbol,
		      &ssymnum, objfile);

	    /* See if the last csect needs to be recorded.  */

	    if (last_csect_name && !misc_func_recorded)
	      {

		/* If no misc. function recorded in the last seen csect, enter
		   it as a function.  This will take care of functions like
		   strcmp() compiled by xlc.  */

		RECORD_MINIMAL_SYMBOL
		  (last_csect_name, last_csect_val,
		   mst_text, last_csect_sec, objfile);
	      }

	    if (pst)
	      {
		xcoff_end_psymtab (pst, psymtab_include_list, includes_used,
				   symnum_before,
				   dependency_list, dependencies_used);
		includes_used = 0;
		dependencies_used = 0;
	      }
	    first_fun_line_offset = 0;

	    /* XCOFF, according to the AIX 3.2 documentation, puts the
	       filename in cs->c_name.  But xlc 1.3.0.2 has decided to
	       do things the standard COFF way and put it in the auxent.
	       We use the auxent if the symbol is ".file" and an auxent
	       exists, otherwise use the symbol itself.  */
	    if (!strcmp (namestring, ".file") && symbol.n_numaux > 0)
	      {
		filestring = coff_getfilename (&main_aux, objfile);
	      }
	    else
	      filestring = namestring;

	    pst = xcoff_start_psymtab (objfile, section_offsets,
				       filestring,
				       symnum_before,
				       objfile->global_psymbols.next,
				       objfile->static_psymbols.next);
	    last_csect_name = NULL;
	  }
	  break;

	default:
	  {
	    static struct complaint msg =
	      {"Storage class %d not recognized during scan", 0, 0};
	    complain (&msg, sclass);
	  }
	  /* FALLTHROUGH */

	  /* C_FCN is .bf and .ef symbols.  I think it is sufficient
	     to handle only the C_FUN and C_EXT.  */
	case C_FCN:

	case C_BSTAT:
	case C_ESTAT:
	case C_ARG:
	case C_REGPARM:
	case C_REG:
	case C_TPDEF:
	case C_STRTAG:
	case C_UNTAG:
	case C_ENTAG:
	case C_LABEL:
	case C_NULL:

	  /* C_EINCL means we are switching back to the main file.  But there
	     is no reason to care; the only thing we want to know about
	     includes is the names of all the included (.h) files.  */
	case C_EINCL:

	case C_BLOCK:

	  /* I don't think C_STAT is used in xcoff; C_HIDEXT appears to be
	     used instead.  */
	case C_STAT:

	  /* I don't think the name of the common block (as opposed to the
	     variables within it) is something which is user visible
	     currently.  */
	case C_BCOMM:
	case C_ECOMM:

	case C_PSYM:
	case C_RPSYM:

	  /* I think we can ignore C_LSYM; types on xcoff seem to use C_DECL
	     so C_LSYM would appear to be only for locals.  */
	case C_LSYM:

	case C_AUTO:
	case C_RSYM:
	  {
	    /* We probably could save a few instructions by assuming that
	       C_LSYM, C_PSYM, etc., never have auxents.  */
	    int naux1 =
	      ((struct external_syment *)sraw_symbol)->e_numaux[0] + 1;
	    ssymnum += naux1;
	    sraw_symbol += sizeof (struct external_syment) * naux1;
	  }
	  break;

	case C_BINCL:
	  stype = N_SOL;
	  goto pstab;

	case C_FUN:
	  /* The value of the C_FUN is not the address of the function (it
	     appears to be the address before linking), but as long as it
	     is smaller than the actual address, then find_pc_partial_function
	     will use the minimal symbols instead.  I hope.  */

	case C_GSYM:
	case C_ECOML:
	case C_DECL:
	case C_STSYM:
	  stype = N_LSYM;
	pstab:;
	  swap_sym (&symbol, &main_aux, &namestring, &sraw_symbol,
		    &ssymnum, objfile);
#define CUR_SYMBOL_TYPE stype
#define CUR_SYMBOL_VALUE symbol.n_value

/* START_PSYMTAB and END_PSYMTAB are never used, because they are only
   called from DBXREAD_ONLY or N_SO code.  Likewise for the symnum
   variable.  */
#define START_PSYMTAB(ofile,secoff,fname,low,symoff,global_syms,static_syms) 0
#define END_PSYMTAB(pst,ilist,ninc,c_off,c_text,dep_list,n_deps)\
  do {} while (0)
/* We have already set the namestring.  */
#define SET_NAMESTRING() /* */

#include "partial-stab.h"
	}
    }

  if (pst)
    {
      xcoff_end_psymtab (pst, psymtab_include_list, includes_used,
			 ssymnum,
			 dependency_list, dependencies_used);
    }

  /* Record the toc offset value of this symbol table into ldinfo structure.
     If no XMC_TC0 is found, toc_offset should be zero. Another place to obtain
     this information would be file auxiliary header. */

#ifndef FAKING_RS6000
  xcoff_add_toc_to_loadinfo (toc_offset);
#endif
}

/* Scan and build partial symbols for a symbol file.
   We have been initialized by a call to dbx_symfile_init, which 
   put all the relevant info into a "struct dbx_symfile_info",
   hung off the objfile structure.

   SECTION_OFFSETS contains offsets relative to which the symbols in the
   various sections are (depending where the sections were actually loaded).
   MAINLINE is true if we are reading the main symbol
   table (as opposed to a shared lib or dynamically loaded file).  */

static void
xcoff_initial_scan (objfile, section_offsets, mainline)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     int mainline;	/* FIXME comments above */
{
  bfd *abfd;
  int val;
  struct cleanup *back_to;
  int num_symbols;			/* # of symbols */
  file_ptr symtab_offset;		/* symbol table and */
  file_ptr stringtab_offset;		/* string table file offsets */
  struct coff_symfile_info *info;
  char *name;
  unsigned int size;

#ifndef FAKING_RS6000
  /* Initialize load info structure. */
  if (mainline)
    xcoff_init_loadinfo ();
#endif

  info = (struct coff_symfile_info *) objfile -> sym_private;
  symfile_bfd = abfd = objfile->obfd;
  name = objfile->name;

  num_symbols = bfd_get_symcount (abfd);	/* # of symbols */
  symtab_offset = obj_sym_filepos (abfd);	/* symbol table file offset */
  stringtab_offset = symtab_offset +
    num_symbols * coff_data(abfd)->local_symesz;

  info->min_lineno_offset = 0;
  info->max_lineno_offset = 0;
  bfd_map_over_sections (abfd, find_linenos, info);

  if (num_symbols > 0)
    {
      /* Read the string table.  */
      init_stringtab (abfd, stringtab_offset, objfile);

      /* Read the .debug section, if present.  */
      {
	sec_ptr secp;
	bfd_size_type length;
	char *debugsec = NULL;

	secp = bfd_get_section_by_name (abfd, ".debug");
	if (secp)
	  {
	    length = bfd_section_size (abfd, secp);
	    if (length)
	      {
		debugsec =
		  (char *) obstack_alloc (&objfile->symbol_obstack, length);

		if (!bfd_get_section_contents (abfd, secp, debugsec,
					       (file_ptr) 0, length))
		  {
		    error ("Error reading .debug section of `%s': %s",
			   name, bfd_errmsg (bfd_get_error ()));
		  }
	      }
	  }
	((struct coff_symfile_info *)objfile->sym_private)->debugsec =
	  debugsec;
      }
    }

  /* Read the symbols.  We keep them in core because we will want to
     access them randomly in read_symbol*.  */
  val = bfd_seek (abfd, symtab_offset, SEEK_SET);
  if (val < 0)
    error ("Error reading symbols from %s: %s",
	   name, bfd_errmsg (bfd_get_error ()));
  size = coff_data (abfd)->local_symesz * num_symbols;
  ((struct coff_symfile_info *)objfile->sym_private)->symtbl =
    obstack_alloc (&objfile->symbol_obstack, size);
  ((struct coff_symfile_info *)objfile->sym_private)->symtbl_num_syms =
    num_symbols;

  val = bfd_read (((struct coff_symfile_info *)objfile->sym_private)->symtbl,
		  size, 1, abfd);
  if (val != size)
    perror_with_name ("reading symbol table");

  /* If we are reinitializing, or if we have never loaded syms yet, init */
  if (mainline
      || objfile->global_psymbols.size == 0
      || objfile->static_psymbols.size == 0)
    /* I'm not sure how how good num_symbols is; the rule of thumb in
       init_psymbol_list was developed for a.out.  On the one hand,
       num_symbols includes auxents.  On the other hand, it doesn't
       include N_SLINE.  */
    init_psymbol_list (objfile, num_symbols);

  pending_blocks = 0;
  back_to = make_cleanup (really_free_pendings, 0);

  init_minimal_symbol_collection ();
  make_cleanup (discard_minimal_symbols, 0);

  /* Now that the symbol table data of the executable file are all in core,
     process them and define symbols accordingly.  */

  scan_xcoff_symtab (section_offsets, objfile);

  /* Install any minimal symbols that have been collected as the current
     minimal symbols for this objfile. */

  install_minimal_symbols (objfile);

  do_cleanups (back_to);
}

static struct section_offsets *
xcoff_symfile_offsets (objfile, addr)
     struct objfile *objfile;
     CORE_ADDR addr;
{
  struct section_offsets *section_offsets;
  int i;

  objfile->num_sections = SECT_OFF_MAX;
  section_offsets = (struct section_offsets *)
    obstack_alloc
      (&objfile -> psymbol_obstack,
       sizeof (struct section_offsets)
       + sizeof (section_offsets->offsets) * objfile->num_sections);

  /* syms_from_objfile kindly subtracts from addr the bfd_section_vma
     of the .text section.  This strikes me as wrong--whether the
     offset to be applied to symbol reading is relative to the start
     address of the section depends on the symbol format.  In any
     event, this whole "addr" concept is pretty broken (it doesn't
     handle any section but .text sensibly), so just ignore the addr
     parameter and use 0.  rs6000-nat.c will set the correct section
     offsets via objfile_relocate.  */
  for (i = 0; i < objfile->num_sections; ++i)
    ANOFFSET (section_offsets, i) = 0;

  return section_offsets;
}

/* Register our ability to parse symbols for xcoff BFD files.  */

static struct sym_fns xcoff_sym_fns =
{

  /* Because the bfd uses coff_flavour, we need to specially kludge
     the flavour.  It is possible that coff and xcoff should be merged as
     they do have fundamental similarities (for example, the extra storage
     classes used for stabs could presumably be recognized in any COFF file).
     However, in addition to obvious things like all the csect hair, there are
     some subtler differences between xcoffread.c and coffread.c, notably
     the fact that coffread.c has no need to read in all the symbols, but
     xcoffread.c reads all the symbols and does in fact randomly access them
     (in C_BSTAT and line number processing).  */

  (enum bfd_flavour)-1,

  xcoff_new_init,	/* sym_new_init: init anything gbl to entire symtab */
  xcoff_symfile_init,	/* sym_init: read initial info, setup for sym_read() */
  xcoff_initial_scan,	/* sym_read: read a symbol file into symtab */
  xcoff_symfile_finish, /* sym_finish: finished with file, cleanup */
  xcoff_symfile_offsets, /* sym_offsets: xlate offsets ext->int form */
  NULL			/* next: pointer to next struct sym_fns */
};

void
_initialize_xcoffread ()
{
  add_symtab_fns(&xcoff_sym_fns);

  func_symbol_type = init_type (TYPE_CODE_FUNC, 1, 0,
				"<function, no debug info>", NULL);
  TYPE_TARGET_TYPE (func_symbol_type) = builtin_type_int;
  var_symbol_type =
    init_type (TYPE_CODE_INT, TARGET_INT_BIT / HOST_CHAR_BIT, 0,
	       "<variable, no debug info>", NULL);
}
