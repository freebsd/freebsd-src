/* Read coff symbol tables and convert to internal format, for GDB.
   Contributed by David D. Johnson, Brown University (ddj@cs.brown.edu).
   Copyright 1987, 1988, 1989, 1990, 1991, 1992, 1993
   Free Software Foundation, Inc.

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

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "breakpoint.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "buildsym.h"
#include "gdb-stabs.h"
#include "complaints.h"
#include <obstack.h>

#include <string.h>

#include <time.h> /* For time_t in libbfd.h.  */
#include <sys/types.h> /* For time_t, if not in time.h.  */
#include "libbfd.h"		/* FIXME secret internal data from BFD */
#include "coff/internal.h"	/* Internal format of COFF symbols in BFD */
#include "libcoff.h"		/* FIXME secret internal data from BFD */

struct coff_symfile_info {
  file_ptr min_lineno_offset;		/* Where in file lowest line#s are */
  file_ptr max_lineno_offset;		/* 1+last byte of line#s in file */

  asection *stabsect;		/* Section pointer for .stab section */
  asection *stabstrsect;		/* Section pointer for .stab section */
  asection *stabindexsect;	/* Section pointer for .stab.index section */
  char *stabstrdata;
};

/* Translate an external name string into a user-visible name.  */
#define	EXTERNAL_NAME(string, abfd) \
	(string[0] == bfd_get_symbol_leading_char(abfd)? string+1: string)

/* To be an sdb debug type, type must have at least a basic or primary
   derived type.  Using this rather than checking against T_NULL is
   said to prevent core dumps if we try to operate on Michael Bloom
   dbx-in-coff file.  */

#define SDB_TYPE(type) (BTYPE(type) | (type & N_TMASK))

/*
 * Convert from an sdb register number to an internal gdb register number.
 * This should be defined in tm.h, if REGISTER_NAMES is not set up
 * to map one to one onto the sdb register numbers.
 */
#ifndef SDB_REG_TO_REGNUM
# define SDB_REG_TO_REGNUM(value)     (value)
#endif

/* Core address of start and end of text of current source file.
   This comes from a ".text" symbol where x_nlinno > 0.  */

static CORE_ADDR cur_src_start_addr;
static CORE_ADDR cur_src_end_addr;

/* Core address of the end of the first object file.  */
static CORE_ADDR first_object_file_end;

/* The addresses of the symbol table stream and number of symbols
   of the object file we are reading (as copied into core).  */

static FILE *nlist_stream_global;
static int nlist_nsyms_global;

/* Vector of line number information.  */

static struct linetable *line_vector;

/* Index of next entry to go in line_vector_index.  */

static int line_vector_index;

/* Last line number recorded in the line vector.  */

static int prev_line_number;

/* Number of elements allocated for line_vector currently.  */

static int line_vector_length;

/* Pointers to scratch storage, used for reading raw symbols and auxents.  */

static char *temp_sym;
static char *temp_aux;

/* Local variables that hold the shift and mask values for the
   COFF file that we are currently reading.  These come back to us
   from BFD, and are referenced by their macro names, as well as
   internally to the BTYPE, ISPTR, ISFCN, ISARY, ISTAG, and DECREF
   macros from ../internalcoff.h .  */

static unsigned	local_n_btmask;
static unsigned	local_n_btshft;
static unsigned	local_n_tmask;
static unsigned	local_n_tshift;

#define	N_BTMASK	local_n_btmask
#define	N_BTSHFT	local_n_btshft
#define	N_TMASK		local_n_tmask
#define	N_TSHIFT	local_n_tshift
 
/* Local variables that hold the sizes in the file of various COFF structures.
   (We only need to know this to read them from the file -- BFD will then
   translate the data in them, into `internal_xxx' structs in the right
   byte order, alignment, etc.)  */

static unsigned	local_linesz;
static unsigned	local_symesz;
static unsigned	local_auxesz;


/* Chain of typedefs of pointers to empty struct/union types.
   They are chained thru the SYMBOL_VALUE_CHAIN.  */

static struct symbol *opaque_type_chain[HASHSIZE];

#if 0
/* The type of the function we are currently reading in.  This is
   used by define_symbol to record the type of arguments to a function. */

struct type *in_function_type;
#endif

struct pending_block *pending_blocks;

/* Complaints about various problems in the file being read  */

struct complaint ef_complaint = 
  {"Unmatched .ef symbol(s) ignored starting at symnum %d", 0, 0};

struct complaint bf_no_aux_complaint =
  {"`.bf' symbol %d has no aux entry", 0, 0};

struct complaint ef_no_aux_complaint =
  {"`.ef' symbol %d has no aux entry", 0, 0};

struct complaint lineno_complaint =
  {"Line number pointer %d lower than start of line numbers", 0, 0};

struct complaint unexpected_type_complaint =
  {"Unexpected type for symbol %s", 0, 0};

struct complaint bad_sclass_complaint =
  {"Bad n_sclass for symbol %s", 0, 0};

struct complaint misordered_blocks_complaint =
  {"Blocks out of order at address %x", 0, 0};

struct complaint tagndx_bad_complaint =
  {"Symbol table entry for %s has bad tagndx value", 0, 0};

struct complaint eb_complaint = 
  {"Mismatched .eb symbol ignored starting at symnum %d", 0, 0};

/* Simplified internal version of coff symbol table information */

struct coff_symbol {
  char *c_name;
  int c_symnum;		/* symbol number of this entry */
  int c_naux;		/* 0 if syment only, 1 if syment + auxent, etc */
  long c_value;
  int c_sclass;
  int c_secnum;
  unsigned int c_type;
};

static struct type *
coff_read_struct_type PARAMS ((int, int, int));

static struct type *
decode_base_type PARAMS ((struct coff_symbol *, unsigned int,
			  union internal_auxent *));

static struct type *
decode_type PARAMS ((struct coff_symbol *, unsigned int,
		     union internal_auxent *));

static struct type *
decode_function_type PARAMS ((struct coff_symbol *, unsigned int,
			      union internal_auxent *));

static struct type *
coff_read_enum_type PARAMS ((int, int, int));

static struct symbol *
process_coff_symbol PARAMS ((struct coff_symbol *, union internal_auxent *,
			     struct objfile *));

static void
patch_opaque_types PARAMS ((struct symtab *));

static void
patch_type PARAMS ((struct type *, struct type *));

static void
enter_linenos PARAMS ((long, int, int));

static void
free_linetab PARAMS ((void));

static int
init_lineno PARAMS ((int, long, int));

static char *
getfilename PARAMS ((union internal_auxent *));

static char *
getsymname PARAMS ((struct internal_syment *));

static void
free_stringtab PARAMS ((void));

static int
init_stringtab PARAMS ((int, long));

static void
read_one_sym PARAMS ((struct coff_symbol *, struct internal_syment *,
		      union internal_auxent *));

static void
read_coff_symtab PARAMS ((long, int, struct objfile *));

static void
find_linenos PARAMS ((bfd *, sec_ptr, PTR));

static void
coff_symfile_init PARAMS ((struct objfile *));

static void
coff_new_init PARAMS ((struct objfile *));

static void
coff_symfile_read PARAMS ((struct objfile *, struct section_offsets *, int));

static void
coff_symfile_finish PARAMS ((struct objfile *));

static void
record_minimal_symbol PARAMS ((char *, CORE_ADDR, enum minimal_symbol_type));

static void
coff_end_symtab PARAMS ((struct objfile *));

static void
complete_symtab PARAMS ((char *, CORE_ADDR, unsigned int));

static void
coff_start_symtab PARAMS ((void));

static void
coff_record_line PARAMS ((int, CORE_ADDR));

static struct type *
coff_alloc_type PARAMS ((int));

static struct type **
coff_lookup_type PARAMS ((int));


static void
coff_locate_sections PARAMS ((bfd *, asection *, PTR));

/* We are called once per section from coff_symfile_read.  We
   need to examine each section we are passed, check to see
   if it is something we are interested in processing, and
   if so, stash away some access information for the section.

   FIXME: The section names should not be hardwired strings (what
   should they be?  I don't think most object file formats have enough
   section flags to specify what kind of debug section it is
   -kingdon).  */

static void
coff_locate_sections (ignore_abfd, sectp, csip)
     bfd *ignore_abfd;
     asection *sectp;
     PTR csip;
{
  register struct coff_symfile_info *csi;

  csi = (struct coff_symfile_info *) csip;
  if (STREQ (sectp->name, ".stab"))
    {
      csi->stabsect = sectp;
    }
  else if (STREQ (sectp->name, ".stabstr"))
    {
      csi->stabstrsect = sectp;
    }
  else if (STREQ (sectp->name, ".stab.index"))
    {
      csi->stabindexsect = sectp;
    }
}

/* Look up a coff type-number index.  Return the address of the slot
   where the type for that index is stored.
   The type-number is in INDEX. 

   This can be used for finding the type associated with that index
   or for associating a new type with the index.  */

static struct type **
coff_lookup_type (index)
     register int index;
{
  if (index >= type_vector_length)
    {
      int old_vector_length = type_vector_length;

      type_vector_length *= 2;
      if (index /* is still */ >= type_vector_length) {
	type_vector_length = index * 2;
      }
      type_vector = (struct type **)
	xrealloc ((char *) type_vector,
		  type_vector_length * sizeof (struct type *));
      memset (&type_vector[old_vector_length], 0,
	     (type_vector_length - old_vector_length) * sizeof(struct type *));
    }
  return &type_vector[index];
}

/* Make sure there is a type allocated for type number index
   and return the type object.
   This can create an empty (zeroed) type object.  */

static struct type *
coff_alloc_type (index)
     int index;
{
  register struct type **type_addr = coff_lookup_type (index);
  register struct type *type = *type_addr;

  /* If we are referring to a type not known at all yet,
     allocate an empty type for it.
     We will fill it in later if we find out how.  */
  if (type == NULL)
    {
      type = alloc_type (current_objfile);
      *type_addr = type;
    }
  return type;
}

/* Record a line number entry for line LINE at address PC.
   FIXME:  Use record_line instead.  */

static void
coff_record_line (line, pc)
     int line;
     CORE_ADDR pc;
{
  struct linetable_entry *e;
  /* Make sure line vector is big enough.  */

  if (line_vector_index + 2 >= line_vector_length)
    {
      line_vector_length *= 2;
      line_vector = (struct linetable *)
	xrealloc ((char *) line_vector, sizeof (struct linetable)
		  + (line_vector_length
		     * sizeof (struct linetable_entry)));
    }

  e = line_vector->item + line_vector_index++;
  e->line = line; e->pc = pc;
}

/* Start a new symtab for a new source file.
   This is called when a COFF ".file" symbol is seen;
   it indicates the start of data for one original source file.  */

static void
coff_start_symtab ()
{
  start_symtab (
		/* We fill in the filename later.  start_symtab
		   puts this pointer into last_source file and in
		   coff_end_symtab we assume we can free() it.
		   FIXME: leaks memory.  */
		savestring ("", 0),
		/* We never know the directory name for COFF.  */
		NULL,
		/* The start address is irrelevant, since we set
		   last_source_start_addr in coff_end_symtab.  */
		0);

  /* Initialize the source file line number information for this file.  */

  if (line_vector)		/* Unlikely, but maybe possible? */
    free ((PTR)line_vector);
  line_vector_index = 0;
  line_vector_length = 1000;
  prev_line_number = -2;	/* Force first line number to be explicit */
  line_vector = (struct linetable *)
    xmalloc (sizeof (struct linetable)
	     + line_vector_length * sizeof (struct linetable_entry));
}

/* Save the vital information from when starting to read a file,
   for use when closing off the current file.
   NAME is the file name the symbols came from, START_ADDR is the first
   text address for the file, and SIZE is the number of bytes of text.  */

static void
complete_symtab (name, start_addr, size)
    char *name;
    CORE_ADDR start_addr;
    unsigned int size;
{
  last_source_file = savestring (name, strlen (name));
  cur_src_start_addr = start_addr;
  cur_src_end_addr = start_addr + size;

  if (current_objfile -> ei.entry_point >= cur_src_start_addr &&
      current_objfile -> ei.entry_point <  cur_src_end_addr)
    {
      current_objfile -> ei.entry_file_lowpc = cur_src_start_addr;
      current_objfile -> ei.entry_file_highpc = cur_src_end_addr;
    }
}

/* Finish the symbol definitions for one main source file,
   close off all the lexical contexts for that file
   (creating struct block's for them), then make the
   struct symtab for that file and put it in the list of all such. */

static void
coff_end_symtab (objfile)
     struct objfile *objfile;
{
  struct symtab *symtab;

  last_source_start_addr = cur_src_start_addr;

  /* For no good reason, this file stores the number of entries in a
     separate variable instead of in line_vector->nitems.  Fix it.  */
  if (line_vector)
    line_vector->nitems = line_vector_index;

  /* For COFF, we only have one subfile, so we can just look at
     subfiles and not worry about there being other elements in the
     chain.  We fill in various fields now because we didn't know them
     before (or because doing it now is simply an artifact of how this
     file used to be written).  */
  subfiles->line_vector = line_vector;
  subfiles->name = last_source_file;

  /* sort_pending is needed for amdcoff, at least.
     sort_linevec is needed for the SCO compiler.  */
  symtab = end_symtab (cur_src_end_addr, 1, 1, objfile, 0);

  if (symtab != NULL)
    free_named_symtabs (symtab->filename);

  /* Reinitialize for beginning of new file. */
  line_vector = 0;
  line_vector_length = -1;
  last_source_file = NULL;
}

static void
record_minimal_symbol (name, address, type)
     char *name;
     CORE_ADDR address;
     enum minimal_symbol_type type;
{
  /* We don't want TDESC entry points in the minimal symbol table */
  if (name[0] == '@') return;

  prim_record_minimal_symbol (savestring (name, strlen (name)), address, type);
}

/* coff_symfile_init ()
   is the coff-specific initialization routine for reading symbols.
   It is passed a struct objfile which contains, among other things,
   the BFD for the file whose symbols are being read, and a slot for
   a pointer to "private data" which we fill with cookies and other
   treats for coff_symfile_read ().

   We will only be called if this is a COFF or COFF-like file.
   BFD handles figuring out the format of the file, and code in symtab.c
   uses BFD's determination to vector to us.

   The ultimate result is a new symtab (or, FIXME, eventually a psymtab).  */

static int text_bfd_scnum;

static void
coff_symfile_init (objfile)
     struct objfile *objfile;
{
  asection	*section, *strsection;
  bfd *abfd = objfile->obfd;

  /* Allocate struct to keep track of stab reading. */
  objfile->sym_stab_info = (PTR)
    xmmalloc (objfile -> md, sizeof (struct dbx_symfile_info));

  memset ((PTR) objfile->sym_stab_info, 0, sizeof (struct dbx_symfile_info));

  /* Allocate struct to keep track of the symfile */
  objfile -> sym_private = xmmalloc (objfile -> md,
				     sizeof (struct coff_symfile_info));

  memset (objfile->sym_private, 0, sizeof (struct coff_symfile_info));

  init_entry_point_info (objfile);

  /* Save the section number for the text section */
  section = bfd_get_section_by_name (abfd, ".text");
  if (section)
    text_bfd_scnum = section->index;
  else
    text_bfd_scnum = -1;
}

/* This function is called for every section; it finds the outer limits
   of the line table (minimum and maximum file offset) so that the
   mainline code can read the whole thing for efficiency.  */

/* ARGSUSED */
static void
find_linenos (abfd, asect, vpinfo)
     bfd *abfd;
     sec_ptr asect;
     PTR vpinfo;
{
  struct coff_symfile_info *info;
  int size, count;
  file_ptr offset, maxoff;

/* WARNING WILL ROBINSON!  ACCESSING BFD-PRIVATE DATA HERE!  FIXME!  */
  count = asect->lineno_count;
/* End of warning */

  if (count == 0)
    return;
  size = count * local_linesz;

  info = (struct coff_symfile_info *)vpinfo;
/* WARNING WILL ROBINSON!  ACCESSING BFD-PRIVATE DATA HERE!  FIXME!  */
  offset = asect->line_filepos;
/* End of warning */

  if (offset < info->min_lineno_offset || info->min_lineno_offset == 0)
    info->min_lineno_offset = offset;

  maxoff = offset + size;
  if (maxoff > info->max_lineno_offset)
    info->max_lineno_offset = maxoff;
}


/* The BFD for this file -- only good while we're actively reading
   symbols into a psymtab or a symtab.  */

static bfd *symfile_bfd;

/* Read a symbol file, after initialization by coff_symfile_init.  */
/* FIXME!  Addr and Mainline are not used yet -- this will not work for
   shared libraries or add_file!  */

/* ARGSUSED */
static void
coff_symfile_read (objfile, section_offsets, mainline)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     int mainline;
{
  struct coff_symfile_info *info;
  struct dbx_symfile_info *dbxinfo;
  bfd *abfd = objfile->obfd;
  coff_data_type *cdata = coff_data (abfd);
  char *name = bfd_get_filename (abfd);
  int desc;
  register int val;
  int num_symbols;
  int symtab_offset;
  int stringtab_offset;
  struct cleanup *back_to;
  int stabsize, stabstrsize;

  info = (struct coff_symfile_info *) objfile -> sym_private;
  dbxinfo = (struct dbx_symfile_info *) objfile->sym_stab_info;
  symfile_bfd = abfd;			/* Kludge for swap routines */

/* WARNING WILL ROBINSON!  ACCESSING BFD-PRIVATE DATA HERE!  FIXME!  */
   desc = fileno ((FILE *)(abfd->iostream));	/* File descriptor */
   num_symbols = bfd_get_symcount (abfd);	/* How many syms */
   symtab_offset = cdata->sym_filepos;		/* Symbol table file offset */
   stringtab_offset = symtab_offset +		/* String table file offset */
		      num_symbols * cdata->local_symesz;

  /* Set a few file-statics that give us specific information about
     the particular COFF file format we're reading.  */
  local_linesz   = cdata->local_linesz;
  local_n_btmask = cdata->local_n_btmask;
  local_n_btshft = cdata->local_n_btshft;
  local_n_tmask  = cdata->local_n_tmask;
  local_n_tshift = cdata->local_n_tshift;
  local_linesz   = cdata->local_linesz;
  local_symesz   = cdata->local_symesz;
  local_auxesz   = cdata->local_auxesz;

  /* Allocate space for raw symbol and aux entries, based on their
     space requirements as reported by BFD.  */
  temp_sym = (char *) xmalloc
	 (cdata->local_symesz + cdata->local_auxesz);
  temp_aux = temp_sym + cdata->local_symesz;
  back_to = make_cleanup (free_current_contents, &temp_sym);
/* End of warning */

  /* Read the line number table, all at once.  */
  info->min_lineno_offset = 0;
  info->max_lineno_offset = 0;
  bfd_map_over_sections (abfd, find_linenos, (PTR) info);

  make_cleanup (free_linetab, 0);
  val = init_lineno (desc, info->min_lineno_offset, 
		     info->max_lineno_offset - info->min_lineno_offset);
  if (val < 0)
    error ("\"%s\": error reading line numbers\n", name);

  /* Now read the string table, all at once.  */

  make_cleanup (free_stringtab, 0);
  val = init_stringtab (desc, stringtab_offset);
  if (val < 0)
    error ("\"%s\": can't get string table", name);

  init_minimal_symbol_collection ();
  make_cleanup (discard_minimal_symbols, 0);

  /* Now that the executable file is positioned at symbol table,
     process it and define symbols accordingly.  */

  read_coff_symtab ((long)symtab_offset, num_symbols, objfile);

  /* Sort symbols alphabetically within each block.  */

  sort_all_symtab_syms ();

  /* Install any minimal symbols that have been collected as the current
     minimal symbols for this objfile. */

  install_minimal_symbols (objfile);

  bfd_map_over_sections (abfd, coff_locate_sections, (PTR) info);

  if (info->stabsect)
    {
      /* FIXME: dubious.  Why can't we use something normal like
	 bfd_get_section_contents?  */
      fseek ((FILE *) abfd->iostream, abfd->where, 0);

      stabsize = bfd_section_size (abfd, info->stabsect);
      stabstrsize = bfd_section_size (abfd, info->stabstrsect);

      coffstab_build_psymtabs (objfile,
			       section_offsets,
			       mainline,
			       info->stabsect->filepos, stabsize,
			       info->stabstrsect->filepos, stabstrsize);
    }

  do_cleanups (back_to);
}

static void
coff_new_init (ignore)
     struct objfile *ignore;
{
}

/* Perform any local cleanups required when we are done with a particular
   objfile.  I.E, we are in the process of discarding all symbol information
   for an objfile, freeing up all memory held for it, and unlinking the
   objfile struct from the global list of known objfiles. */

static void
coff_symfile_finish (objfile)
     struct objfile *objfile;
{
  if (objfile -> sym_private != NULL)
    {
      mfree (objfile -> md, objfile -> sym_private);
    }
}


/* Given pointers to a symbol table in coff style exec file,
   analyze them and create struct symtab's describing the symbols.
   NSYMS is the number of symbols in the symbol table.
   We read them one at a time using read_one_sym ().  */

static void
read_coff_symtab (symtab_offset, nsyms, objfile)
     long symtab_offset;
     int nsyms;
     struct objfile *objfile;
{
  FILE *stream; 
  register struct context_stack *new;
  struct coff_symbol coff_symbol;
  register struct coff_symbol *cs = &coff_symbol;
  static struct internal_syment main_sym;
  static union internal_auxent main_aux;
  struct coff_symbol fcn_cs_saved;
  static struct internal_syment fcn_sym_saved;
  static union internal_auxent fcn_aux_saved;
  struct symtab *s;
  
  /* A .file is open.  */
  int in_source_file = 0;
  int num_object_files = 0;
  int next_file_symnum = -1;

  /* Name of the current file.  */
  char *filestring = "";
  int depth = 0;
  int fcn_first_line = 0;
  int fcn_last_line = 0;
  int fcn_start_addr = 0;
  long fcn_line_ptr = 0;
  int val;

  stream = bfd_cache_lookup(objfile->obfd);
  if (!stream)
   perror_with_name(objfile->name);

  /* Work around a stdio bug in SunOS4.1.1 (this makes me nervous....
     it's hard to know I've really worked around it.  The fix should be
     harmless, anyway).  The symptom of the bug is that the first
     fread (in read_one_sym), will (in my example) actually get data
     from file offset 268, when the fseek was to 264 (and ftell shows
     264).  This causes all hell to break loose.  I was unable to
     reproduce this on a short test program which operated on the same
     file, performing (I think) the same sequence of operations.

     It stopped happening when I put in this rewind().

     FIXME: Find out if this has been reported to Sun, whether it has
     been fixed in a later release, etc.  */

  rewind (stream);

  /* Position to read the symbol table. */
  val = fseek (stream, (long)symtab_offset, 0);
  if (val < 0)
    perror_with_name (objfile->name);

  current_objfile = objfile;
  nlist_stream_global = stream;
  nlist_nsyms_global = nsyms;
  last_source_file = NULL;
  memset (opaque_type_chain, 0, sizeof opaque_type_chain);

  if (type_vector)			/* Get rid of previous one */
    free ((PTR)type_vector);
  type_vector_length = 160;
  type_vector = (struct type **)
		xmalloc (type_vector_length * sizeof (struct type *));
  memset (type_vector, 0, type_vector_length * sizeof (struct type *));

  coff_start_symtab ();

  symnum = 0;
  while (symnum < nsyms)
    {
      QUIT;			/* Make this command interruptable.  */
      read_one_sym (cs, &main_sym, &main_aux);

#ifdef SEM
      temp_sem_val = cs->c_name[0] << 24 | cs->c_name[1] << 16 |
                     cs->c_name[2] << 8 | cs->c_name[3];
      if (int_sem_val == temp_sem_val)
        last_coffsem = (int) strtol (cs->c_name+4, (char **) NULL, 10);
#endif

      if (cs->c_symnum == next_file_symnum && cs->c_sclass != C_FILE)
	{
	  if (last_source_file)
	    coff_end_symtab (objfile);

	  coff_start_symtab ();
	  complete_symtab ("_globals_", 0, first_object_file_end);
	  /* done with all files, everything from here on out is globals */
	}

      /* Special case for file with type declarations only, no text.  */
      if (!last_source_file && SDB_TYPE (cs->c_type)
	  && cs->c_secnum == N_DEBUG)
	complete_symtab (filestring, 0, 0);

      /* Typedefs should not be treated as symbol definitions.  */
      if (ISFCN (cs->c_type) && cs->c_sclass != C_TPDEF)
	{
	  /* Record all functions -- external and static -- in minsyms. */
	  record_minimal_symbol (cs->c_name, cs->c_value, mst_text);

	  fcn_line_ptr = main_aux.x_sym.x_fcnary.x_fcn.x_lnnoptr;
	  fcn_start_addr = cs->c_value;
	  fcn_cs_saved = *cs;
	  fcn_sym_saved = main_sym;
	  fcn_aux_saved = main_aux;
	  continue;
	}

      switch (cs->c_sclass)
	{
	  case C_EFCN:
	  case C_EXTDEF:
	  case C_ULABEL:
	  case C_USTATIC:
	  case C_LINE:
	  case C_ALIAS:
	  case C_HIDDEN:
	    complain (&bad_sclass_complaint, cs->c_name);
	    break;

	  case C_FILE:
	    /*
	     * c_value field contains symnum of next .file entry in table
	     * or symnum of first global after last .file.
	     */
	    next_file_symnum = cs->c_value;
	    if (cs->c_naux > 0)
	      filestring = getfilename (&main_aux);
	    else
	      filestring = "";

	    /*
	     * Complete symbol table for last object file
	     * containing debugging information.
	     */
	    if (last_source_file)
	      {
		coff_end_symtab (objfile);
		coff_start_symtab ();
	      }
	    in_source_file = 1;
	    break;

          case C_STAT:
	    if (cs->c_name[0] == '.') {
		    if (STREQ (cs->c_name, ".text")) {
			    /* FIXME:  don't wire in ".text" as section name
				       or symbol name! */
			    if (++num_object_files == 1) {
				    /* last address of startup file */
				    first_object_file_end = cs->c_value +
					    main_aux.x_scn.x_scnlen;
			    }
			    /* Check for in_source_file deals with case of
			       a file with debugging symbols
			       followed by a later file with no symbols.  */
			    if (in_source_file)
			      complete_symtab (filestring, cs->c_value,
					       main_aux.x_scn.x_scnlen);
			    in_source_file = 0;
		    }
		    /* flush rest of '.' symbols */
		    break;
	    }
	    else if (!SDB_TYPE (cs->c_type)
		     && cs->c_name[0] == 'L'
		     && (strncmp (cs->c_name, "LI%", 3) == 0
			 || strncmp (cs->c_name, "LF%", 3) == 0
			 || strncmp (cs->c_name,"LC%",3) == 0
			 || strncmp (cs->c_name,"LP%",3) == 0
			 || strncmp (cs->c_name,"LPB%",4) == 0
			 || strncmp (cs->c_name,"LBB%",4) == 0
			 || strncmp (cs->c_name,"LBE%",4) == 0
			 || strncmp (cs->c_name,"LPBX%",5) == 0))
	      /* At least on a 3b1, gcc generates swbeg and string labels
		 that look like this.  Ignore them.  */
	      break;
	    /* fall in for static symbols that don't start with '.' */
	  case C_EXT:
	    /* Record external symbols in minsyms if we don't have debug
	       info for them.  FIXME, this is probably the wrong thing
	       to do.  Why don't we record them even if we do have
	       debug symbol info?  What really belongs in the minsyms
	       anyway?  Fred!??  */
	    if (!SDB_TYPE (cs->c_type)) {
		/* FIXME: This is BOGUS Will Robinson! 
	 	Coff should provide the SEC_CODE flag for executable sections,
	 	then if we could look up sections by section number we
  	 	could see if the flags indicate SEC_CODE.  If so, then
	 	record this symbol as a function in the minimal symbol table.
		But why are absolute syms recorded as functions, anyway?  */
		    if (cs->c_secnum <= text_bfd_scnum+1) {/* text or abs */
			    record_minimal_symbol (cs->c_name, cs->c_value,
						   mst_text);
			    break;
		    } else {
			    record_minimal_symbol (cs->c_name, cs->c_value,
						   mst_data);
			    break;
		    }
	    }
	    process_coff_symbol (cs, &main_aux, objfile);
	    break;

	  case C_FCN:
	    if (STREQ (cs->c_name, ".bf"))
	      {
		within_function = 1;

		/* value contains address of first non-init type code */
		/* main_aux.x_sym.x_misc.x_lnsz.x_lnno
			    contains line number of '{' } */
		if (cs->c_naux != 1)
		  complain (&bf_no_aux_complaint, cs->c_symnum);
		fcn_first_line = main_aux.x_sym.x_misc.x_lnsz.x_lnno;

		/* Might want to check that locals are 0 and
		   context_stack_depth is zero, and complain if not.  */

		depth = 0;
		new = push_context (depth, fcn_start_addr);
		fcn_cs_saved.c_name = getsymname (&fcn_sym_saved);
		new->name = process_coff_symbol (&fcn_cs_saved,
						 &fcn_aux_saved, objfile);
	      }
	    else if (STREQ (cs->c_name, ".ef"))
	      {
		/* the value of .ef is the address of epilogue code;
		   not useful for gdb.  */
		/* { main_aux.x_sym.x_misc.x_lnsz.x_lnno
			    contains number of lines to '}' */
		new = pop_context ();
		/* Stack must be empty now.  */
		if (context_stack_depth > 0 || new == NULL)
		  {
		    complain (&ef_complaint, cs->c_symnum);
		    within_function = 0;
		    break;
		  }
		if (cs->c_naux != 1)
		  {
		    complain (&ef_no_aux_complaint, cs->c_symnum);
		    fcn_last_line = 0x7FFFFFFF;
		  }
		else
		  {
		    fcn_last_line = main_aux.x_sym.x_misc.x_lnsz.x_lnno;
		  }
		enter_linenos (fcn_line_ptr, fcn_first_line, fcn_last_line);

		finish_block (new->name, &local_symbols, new->old_blocks,
			      new->start_addr,
#if defined (FUNCTION_EPILOGUE_SIZE)
			      /* This macro should be defined only on
				 machines where the
				 fcn_aux_saved.x_sym.x_misc.x_fsize
				 field is always zero.
				 So use the .bf record information that
				 points to the epilogue and add the size
				 of the epilogue.  */
			      cs->c_value + FUNCTION_EPILOGUE_SIZE,
#else
			      fcn_cs_saved.c_value +
			          fcn_aux_saved.x_sym.x_misc.x_fsize,
#endif
			      objfile
			      );
		within_function = 0;
	      }
	    break;

	  case C_BLOCK:
	    if (STREQ (cs->c_name, ".bb"))
	      {
		push_context (++depth, cs->c_value);
	      }
	    else if (STREQ (cs->c_name, ".eb"))
	      {
		new = pop_context ();
		if (depth-- != new->depth)
		  {
		    complain (&eb_complaint, symnum);
		    break;
		  }
		if (local_symbols && context_stack_depth > 0)
		  {
		    /* Make a block for the local symbols within.  */
		    finish_block (0, &local_symbols, new->old_blocks,
				  new->start_addr, cs->c_value, objfile);
		  }
		/* Now pop locals of block just finished.  */
		local_symbols = new->locals;
	      }
	    break;

	  default:
	    process_coff_symbol (cs, &main_aux, objfile);
	    break;
	}
    }

  if (last_source_file)
    coff_end_symtab (objfile);

  /* Patch up any opaque types (references to types that are not defined
     in the file where they are referenced, e.g. "struct foo *bar").  */
  ALL_OBJFILE_SYMTABS (objfile, s)
    patch_opaque_types (s);

  current_objfile = NULL;
}

/* Routines for reading headers and symbols from executable.  */

#ifdef FIXME
/* Move these XXXMAGIC symbol defns into BFD!  */

/* Read COFF file header, check magic number,
   and return number of symbols. */
read_file_hdr (chan, file_hdr)
    int chan;
    FILHDR *file_hdr;
{
  lseek (chan, 0L, 0);
  if (myread (chan, (char *)file_hdr, FILHSZ) < 0)
    return -1;

  switch (file_hdr->f_magic)
    {
#ifdef MC68MAGIC
    case MC68MAGIC:
#endif
#ifdef NS32GMAGIC
      case NS32GMAGIC:
      case NS32SMAGIC:
#endif
#ifdef I386MAGIC
    case I386MAGIC:
#endif
#ifdef CLIPPERMAGIC
    case CLIPPERMAGIC:
#endif
#if defined (MC68KWRMAGIC) \
  && (!defined (MC68MAGIC) || MC68KWRMAGIC != MC68MAGIC)
    case MC68KWRMAGIC:
#endif
#ifdef MC68KROMAGIC
    case MC68KROMAGIC:
    case MC68KPGMAGIC:
#endif
#ifdef MC88DGMAGIC
    case MC88DGMAGIC:
#endif      
#ifdef MC88MAGIC
    case MC88MAGIC:
#endif      
#ifdef I960ROMAGIC
    case I960ROMAGIC:		/* Intel 960 */
#endif
#ifdef I960RWMAGIC
    case I960RWMAGIC:		/* Intel 960 */
#endif
	return file_hdr->f_nsyms;

      default:
#ifdef BADMAG
	if (BADMAG(file_hdr))
	  return -1;
	else
	  return file_hdr->f_nsyms;
#else
	return -1;
#endif
    }
}
#endif

/* Read the next symbol, swap it, and return it in both internal_syment
   form, and coff_symbol form.  Also return its first auxent, if any,
   in internal_auxent form, and skip any other auxents.  */

static void
read_one_sym (cs, sym, aux)
    register struct coff_symbol *cs;
    register struct internal_syment *sym;
    register union internal_auxent *aux;
{
  int i;

  cs->c_symnum = symnum;
  fread (temp_sym, local_symesz, 1, nlist_stream_global);
  bfd_coff_swap_sym_in (symfile_bfd, temp_sym, (char *)sym);
  cs->c_naux = sym->n_numaux & 0xff;
  if (cs->c_naux >= 1)
    {
    fread (temp_aux, local_auxesz, 1, nlist_stream_global);
    bfd_coff_swap_aux_in (symfile_bfd, temp_aux, sym->n_type, sym->n_sclass,
			  (char *)aux);
    /* If more than one aux entry, read past it (only the first aux
       is important). */
    for (i = 1; i < cs->c_naux; i++)
      fread (temp_aux, local_auxesz, 1, nlist_stream_global);
    }
  cs->c_name = getsymname (sym);
  cs->c_value = sym->n_value;
  cs->c_sclass = (sym->n_sclass & 0xff);
  cs->c_secnum = sym->n_scnum;
  cs->c_type = (unsigned) sym->n_type;
  if (!SDB_TYPE (cs->c_type))
    cs->c_type = 0;

  symnum += 1 + cs->c_naux;
}

/* Support for string table handling */

static char *stringtab = NULL;

static int
init_stringtab (chan, offset)
    int chan;
    long offset;
{
  long length;
  int val;
  unsigned char lengthbuf[4];

  free_stringtab ();

  if (lseek (chan, offset, 0) < 0)
    return -1;

  val = myread (chan, (char *)lengthbuf, sizeof lengthbuf);
  length = bfd_h_get_32 (symfile_bfd, lengthbuf);

  /* If no string table is needed, then the file may end immediately
     after the symbols.  Just return with `stringtab' set to null. */
  if (val != sizeof lengthbuf || length < sizeof lengthbuf)
    return 0;

  stringtab = (char *) xmalloc (length);
  memcpy (stringtab, &length, sizeof length);
  if (length == sizeof length)		/* Empty table -- just the count */
    return 0;

  val = myread (chan, stringtab + sizeof lengthbuf, length - sizeof lengthbuf);
  if (val != length - sizeof lengthbuf || stringtab[length - 1] != '\0')
    return -1;

  return 0;
}

static void
free_stringtab ()
{
  if (stringtab)
    free (stringtab);
  stringtab = NULL;
}

static char *
getsymname (symbol_entry)
    struct internal_syment *symbol_entry;
{
  static char buffer[SYMNMLEN+1];
  char *result;

  if (symbol_entry->_n._n_n._n_zeroes == 0)
    {
      result = stringtab + symbol_entry->_n._n_n._n_offset;
    }
  else
    {
      strncpy (buffer, symbol_entry->_n._n_name, SYMNMLEN);
      buffer[SYMNMLEN] = '\0';
      result = buffer;
    }
  return result;
}

/* Extract the file name from the aux entry of a C_FILE symbol.  Return
   only the last component of the name.  Result is in static storage and
   is only good for temporary use.  */

static char *
getfilename (aux_entry)
    union internal_auxent *aux_entry;
{
  static char buffer[BUFSIZ];
  register char *temp;
  char *result;

  if (aux_entry->x_file.x_n.x_zeroes == 0)
    strcpy (buffer, stringtab + aux_entry->x_file.x_n.x_offset);
  else
    {
      strncpy (buffer, aux_entry->x_file.x_fname, FILNMLEN);
      buffer[FILNMLEN] = '\0';
    }
  result = buffer;
  if ((temp = strrchr (result, '/')) != NULL)
    result = temp + 1;
  return (result);
}

/* Support for line number handling */
static char *linetab = NULL;
static long linetab_offset;
static unsigned long linetab_size;

/* Read in all the line numbers for fast lookups later.  Leave them in
   external (unswapped) format in memory; we'll swap them as we enter
   them into GDB's data structures.  */
 
static int
init_lineno (chan, offset, size)
    int chan;
    long offset;
    int size;
{
  int val;

  linetab_offset = offset;
  linetab_size = size;

  free_linetab();

  if (size == 0)
    return 0;

  if (lseek (chan, offset, 0) < 0)
    return -1;
  
  /* Allocate the desired table, plus a sentinel */
  linetab = (char *) xmalloc (size + local_linesz);

  val = myread (chan, linetab, size);
  if (val != size)
    return -1;

  /* Terminate it with an all-zero sentinel record */
  memset (linetab + size, 0, local_linesz);

  return 0;
}

static void
free_linetab ()
{
  if (linetab)
    free (linetab);
  linetab = NULL;
}

#if !defined (L_LNNO32)
#define L_LNNO32(lp) ((lp)->l_lnno)
#endif

static void
enter_linenos (file_offset, first_line, last_line)
    long file_offset;
    register int first_line;
    register int last_line;
{
  register char *rawptr;
  struct internal_lineno lptr;

  if (file_offset < linetab_offset)
    {
      complain (&lineno_complaint, file_offset);
      if (file_offset > linetab_size)	/* Too big to be an offset? */
	return;
      file_offset += linetab_offset;  /* Try reading at that linetab offset */
    }
  
  rawptr = &linetab[file_offset - linetab_offset];

  /* skip first line entry for each function */
  rawptr += local_linesz;
  /* line numbers start at one for the first line of the function */
  first_line--;

  for (;;) {
    bfd_coff_swap_lineno_in (symfile_bfd, rawptr, &lptr);
    rawptr += local_linesz;
    /* The next function, or the sentinel, will have L_LNNO32 zero; we exit. */
    if (L_LNNO32 (&lptr) && L_LNNO32 (&lptr) <= last_line)
      coff_record_line (first_line + L_LNNO32 (&lptr), lptr.l_addr.l_paddr);
    else
      break;
  } 
}

static void
patch_type (type, real_type)
    struct type *type;
    struct type *real_type;
{
  register struct type *target = TYPE_TARGET_TYPE (type);
  register struct type *real_target = TYPE_TARGET_TYPE (real_type);
  int field_size = TYPE_NFIELDS (real_target) * sizeof (struct field);

  TYPE_LENGTH (target) = TYPE_LENGTH (real_target);
  TYPE_NFIELDS (target) = TYPE_NFIELDS (real_target);
  TYPE_FIELDS (target) = (struct field *) TYPE_ALLOC (target, field_size);

  memcpy (TYPE_FIELDS (target), TYPE_FIELDS (real_target), field_size);

  if (TYPE_NAME (real_target))
    {
      if (TYPE_NAME (target))
	free (TYPE_NAME (target));
      TYPE_NAME (target) = concat (TYPE_NAME (real_target), NULL);
    }
}

/* Patch up all appropriate typedef symbols in the opaque_type_chains
   so that they can be used to print out opaque data structures properly.  */

static void
patch_opaque_types (s)
     struct symtab *s;
{
  register struct block *b;
  register int i;
  register struct symbol *real_sym;
  
  /* Go through the per-file symbols only */
  b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK);
  for (i = BLOCK_NSYMS (b) - 1; i >= 0; i--)
    {
      /* Find completed typedefs to use to fix opaque ones.
	 Remove syms from the chain when their types are stored,
	 but search the whole chain, as there may be several syms
	 from different files with the same name.  */
      real_sym = BLOCK_SYM (b, i);
      if (SYMBOL_CLASS (real_sym) == LOC_TYPEDEF &&
	  SYMBOL_NAMESPACE (real_sym) == VAR_NAMESPACE &&
	  TYPE_CODE (SYMBOL_TYPE (real_sym)) == TYPE_CODE_PTR &&
	  TYPE_LENGTH (TYPE_TARGET_TYPE (SYMBOL_TYPE (real_sym))) != 0)
	{
	  register char *name = SYMBOL_NAME (real_sym);
	  register int hash = hashname (name);
	  register struct symbol *sym, *prev;
	  
	  prev = 0;
	  for (sym = opaque_type_chain[hash]; sym;)
	    {
	      if (name[0] == SYMBOL_NAME (sym)[0] &&
		  STREQ (name + 1, SYMBOL_NAME (sym) + 1))
		{
		  if (prev)
		    {
		      SYMBOL_VALUE_CHAIN (prev) = SYMBOL_VALUE_CHAIN (sym);
		    }
		  else
		    {
		      opaque_type_chain[hash] = SYMBOL_VALUE_CHAIN (sym);
		    }
		  
		  patch_type (SYMBOL_TYPE (sym), SYMBOL_TYPE (real_sym));
		  
		  if (prev)
		    {
		      sym = SYMBOL_VALUE_CHAIN (prev);
		    }
		  else
		    {
		      sym = opaque_type_chain[hash];
		    }
		}
	      else
		{
		  prev = sym;
		  sym = SYMBOL_VALUE_CHAIN (sym);
		}
	    }
	}
    }
}

static struct symbol *
process_coff_symbol (cs, aux, objfile)
     register struct coff_symbol *cs;
     register union internal_auxent *aux;
     struct objfile *objfile;
{
  register struct symbol *sym
    = (struct symbol *) obstack_alloc (&objfile->symbol_obstack,
				       sizeof (struct symbol));
  char *name;
  struct type *temptype;

  memset (sym, 0, sizeof (struct symbol));
  name = cs->c_name;
  name = EXTERNAL_NAME (name, objfile->obfd);
  SYMBOL_NAME (sym) = obstack_copy0 (&objfile->symbol_obstack, name,
				     strlen (name));

  /* default assumptions */
  SYMBOL_VALUE (sym) = cs->c_value;
  SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;

  if (ISFCN (cs->c_type))
    {
#if 0
       /* FIXME:  This has NOT been tested.  The DBX version has.. */
       /* Generate a template for the type of this function.  The 
	  types of the arguments will be added as we read the symbol 
	  table. */
       struct type *new = (struct type *)
		    obstack_alloc (&objfile->symbol_obstack, sizeof (struct type));
       
       memcpy (new, lookup_function_type (decode_function_type (cs, cs->c_type, aux)),
		      sizeof(struct type));
       SYMBOL_TYPE (sym) = new;
       in_function_type = SYMBOL_TYPE(sym);
#else
       SYMBOL_TYPE(sym) = 
	 lookup_function_type (decode_function_type (cs, cs->c_type, aux));
#endif

      SYMBOL_CLASS (sym) = LOC_BLOCK;
      if (cs->c_sclass == C_STAT)
	add_symbol_to_list (sym, &file_symbols);
      else if (cs->c_sclass == C_EXT)
	add_symbol_to_list (sym, &global_symbols);
    }
  else
    {
      SYMBOL_TYPE (sym) = decode_type (cs, cs->c_type, aux);
      switch (cs->c_sclass)
	{
	  case C_NULL:
	    break;

	  case C_AUTO:
	    SYMBOL_CLASS (sym) = LOC_LOCAL;
	    add_symbol_to_list (sym, &local_symbols);
	    break;

	  case C_EXT:
	    SYMBOL_CLASS (sym) = LOC_STATIC;
	    SYMBOL_VALUE_ADDRESS (sym) = (CORE_ADDR) cs->c_value;
	    add_symbol_to_list (sym, &global_symbols);
	    break;

	  case C_STAT:
	    SYMBOL_CLASS (sym) = LOC_STATIC;
	    SYMBOL_VALUE_ADDRESS (sym) = (CORE_ADDR) cs->c_value;
	    if (within_function) {
	      /* Static symbol of local scope */
	      add_symbol_to_list (sym, &local_symbols);
	    }
	    else {
	      /* Static symbol at top level of file */
	      add_symbol_to_list (sym, &file_symbols);
	    }
	    break;

#ifdef C_GLBLREG		/* AMD coff */
	  case C_GLBLREG:
#endif
	  case C_REG:
	    SYMBOL_CLASS (sym) = LOC_REGISTER;
	    SYMBOL_VALUE (sym) = SDB_REG_TO_REGNUM(cs->c_value);
	    add_symbol_to_list (sym, &local_symbols);
	    break;

	  case C_LABEL:
	    break;

	  case C_ARG:
	    SYMBOL_CLASS (sym) = LOC_ARG;
#if 0
	    /* FIXME:  This has not been tested. */
	    /* Add parameter to function.  */
	    add_param_to_type(&in_function_type,sym);
#endif
	    add_symbol_to_list (sym, &local_symbols);
#if !defined (BELIEVE_PCC_PROMOTION) && (TARGET_BYTE_ORDER == BIG_ENDIAN)
	    /* If PCC says a parameter is a short or a char,
	       aligned on an int boundary, realign it to the "little end"
	       of the int.  */
	    temptype = lookup_fundamental_type (current_objfile, FT_INTEGER);
	    if (TYPE_LENGTH (SYMBOL_TYPE (sym)) < TYPE_LENGTH (temptype)
		&& TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_INT
		&& 0 == SYMBOL_VALUE (sym) % TYPE_LENGTH (temptype))
		{
		    SYMBOL_VALUE (sym) += TYPE_LENGTH (temptype)
				        - TYPE_LENGTH (SYMBOL_TYPE (sym));
		}
#endif
	    break;

	  case C_REGPARM:
	    SYMBOL_CLASS (sym) = LOC_REGPARM;
	    SYMBOL_VALUE (sym) = SDB_REG_TO_REGNUM(cs->c_value);
	    add_symbol_to_list (sym, &local_symbols);
#if !defined (BELIEVE_PCC_PROMOTION)
	/* FIXME:  This should retain the current type, since it's just
	   a register value.  gnu@adobe, 26Feb93 */
	    /* If PCC says a parameter is a short or a char,
	       it is really an int.  */
	    temptype = lookup_fundamental_type (current_objfile, FT_INTEGER);
	    if (TYPE_LENGTH (SYMBOL_TYPE (sym)) < TYPE_LENGTH (temptype)
		&& TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_INT)
		{
		    SYMBOL_TYPE (sym) = TYPE_UNSIGNED (SYMBOL_TYPE (sym))
			? lookup_fundamental_type (current_objfile,
						   FT_UNSIGNED_INTEGER)
			    : temptype;
		}
#endif
	    break;
	    
	  case C_TPDEF:
	    SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	    SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;

	    /* If type has no name, give it one */
	    if (TYPE_NAME (SYMBOL_TYPE (sym)) == 0)
	      TYPE_NAME (SYMBOL_TYPE (sym)) = concat (SYMBOL_NAME (sym), NULL);

	    /* Keep track of any type which points to empty structured type,
		so it can be filled from a definition from another file.  A
		simple forward reference (TYPE_CODE_UNDEF) is not an
		empty structured type, though; the forward references
		work themselves out via the magic of coff_lookup_type.  */
	    if (TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_PTR &&
		TYPE_LENGTH (TYPE_TARGET_TYPE (SYMBOL_TYPE (sym))) == 0 &&
		TYPE_CODE   (TYPE_TARGET_TYPE (SYMBOL_TYPE (sym))) !=
						TYPE_CODE_UNDEF)
	      {
		register int i = hashname (SYMBOL_NAME (sym));

		SYMBOL_VALUE_CHAIN (sym) = opaque_type_chain[i];
		opaque_type_chain[i] = sym;
	      }
	    add_symbol_to_list (sym, &file_symbols);
	    break;

	  case C_STRTAG:
	  case C_UNTAG:
	  case C_ENTAG:
	    SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	    SYMBOL_NAMESPACE (sym) = STRUCT_NAMESPACE;

            /* Some compilers try to be helpful by inventing "fake"
               names for anonymous enums, structures, and unions, like
               "~0fake" or ".0fake".  Thanks, but no thanks... */
	    if (TYPE_TAG_NAME (SYMBOL_TYPE (sym)) == 0)
	      if (SYMBOL_NAME(sym) != NULL
		  && *SYMBOL_NAME(sym) != '~'
		  && *SYMBOL_NAME(sym) != '.')
		TYPE_TAG_NAME (SYMBOL_TYPE (sym)) =
		  concat (SYMBOL_NAME (sym), NULL);

	    add_symbol_to_list (sym, &file_symbols);
	    break;

	  default:
	    break;
	}
    }
  return sym;
}

/* Decode a coff type specifier;
   return the type that is meant.  */

static
struct type *
decode_type (cs, c_type, aux)
     register struct coff_symbol *cs;
     unsigned int c_type;
     register union internal_auxent *aux;
{
  register struct type *type = 0;
  unsigned int new_c_type;

  if (c_type & ~N_BTMASK)
    {
      new_c_type = DECREF (c_type);
      if (ISPTR (c_type))
	{
	  type = decode_type (cs, new_c_type, aux);
	  type = lookup_pointer_type (type);
	}
      else if (ISFCN (c_type))
	{
	  type = decode_type (cs, new_c_type, aux);
	  type = lookup_function_type (type);
	}
      else if (ISARY (c_type))
	{
	  int i, n;
	  register unsigned short *dim;
	  struct type *base_type, *index_type, *range_type;

	  /* Define an array type.  */
	  /* auxent refers to array, not base type */
	  if (aux->x_sym.x_tagndx.l == 0)
	    cs->c_naux = 0;

	  /* shift the indices down */
	  dim = &aux->x_sym.x_fcnary.x_ary.x_dimen[0];
	  i = 1;
	  n = dim[0];
	  for (i = 0; *dim && i < DIMNUM - 1; i++, dim++)
	    *dim = *(dim + 1);
	  *dim = 0;

	  base_type = decode_type (cs, new_c_type, aux);
	  index_type = lookup_fundamental_type (current_objfile, FT_INTEGER);
	  range_type =
	    create_range_type ((struct type *) NULL, index_type, 0, n - 1);
	  type =
	    create_array_type ((struct type *) NULL, base_type, range_type);
	}
      return type;
    }

  /* Reference to existing type.  This only occurs with the
     struct, union, and enum types.  EPI a29k coff
     fakes us out by producing aux entries with a nonzero
     x_tagndx for definitions of structs, unions, and enums, so we
     have to check the c_sclass field.  SCO 3.2v4 cc gets confused
     with pointers to pointers to defined structs, and generates
     negative x_tagndx fields.  */
  if (cs->c_naux > 0 && aux->x_sym.x_tagndx.l != 0)
    {
      if (cs->c_sclass != C_STRTAG
	  && cs->c_sclass != C_UNTAG
	  && cs->c_sclass != C_ENTAG
	  && aux->x_sym.x_tagndx.l >= 0)
	{
	  type = coff_alloc_type (aux->x_sym.x_tagndx.l);
	  return type;
	} else {
	  complain (&tagndx_bad_complaint, cs->c_name);
	  /* And fall through to decode_base_type... */
	}
    }

  return decode_base_type (cs, BTYPE (c_type), aux);
}

/* Decode a coff type specifier for function definition;
   return the type that the function returns.  */

static
struct type *
decode_function_type (cs, c_type, aux)
     register struct coff_symbol *cs;
     unsigned int c_type;
     register union internal_auxent *aux;
{
  if (aux->x_sym.x_tagndx.l == 0)
    cs->c_naux = 0;	/* auxent refers to function, not base type */

  return decode_type (cs, DECREF (c_type), aux);
}

/* basic C types */

static
struct type *
decode_base_type (cs, c_type, aux)
     register struct coff_symbol *cs;
     unsigned int c_type;
     register union internal_auxent *aux;
{
  struct type *type;

  switch (c_type)
    {
      case T_NULL:
        /* shows up with "void (*foo)();" structure members */
	return lookup_fundamental_type (current_objfile, FT_VOID);

#if 0
/* DGUX actually defines both T_ARG and T_VOID to the same value.  */
#ifdef T_ARG
      case T_ARG:
	/* Shows up in DGUX, I think.  Not sure where.  */
	return lookup_fundamental_type (current_objfile, FT_VOID);	/* shouldn't show up here */
#endif
#endif /* 0 */

#ifdef T_VOID
      case T_VOID:
	/* Intel 960 COFF has this symbol and meaning.  */
	return lookup_fundamental_type (current_objfile, FT_VOID);
#endif

      case T_CHAR:
	return lookup_fundamental_type (current_objfile, FT_CHAR);

      case T_SHORT:
	return lookup_fundamental_type (current_objfile, FT_SHORT);

      case T_INT:
	return lookup_fundamental_type (current_objfile, FT_INTEGER);

      case T_LONG:
	return lookup_fundamental_type (current_objfile, FT_LONG);

      case T_FLOAT:
	return lookup_fundamental_type (current_objfile, FT_FLOAT);

      case T_DOUBLE:
	return lookup_fundamental_type (current_objfile, FT_DBL_PREC_FLOAT);

      case T_STRUCT:
	if (cs->c_naux != 1)
	  {
	    /* anonymous structure type */
	    type = coff_alloc_type (cs->c_symnum);
	    TYPE_CODE (type) = TYPE_CODE_STRUCT;
	    TYPE_NAME (type) = NULL;
	    /* This used to set the tag to "<opaque>".  But I think setting it
	       to NULL is right, and the printing code can print it as
	       "struct {...}".  */
	    TYPE_TAG_NAME (type) = NULL;
	    INIT_CPLUS_SPECIFIC(type);
	    TYPE_LENGTH (type) = 0;
	    TYPE_FIELDS (type) = 0;
	    TYPE_NFIELDS (type) = 0;
	  }
	else
	  {
	    type = coff_read_struct_type (cs->c_symnum,
				    aux->x_sym.x_misc.x_lnsz.x_size,
				    aux->x_sym.x_fcnary.x_fcn.x_endndx.l);
	  }
	return type;

      case T_UNION:
	if (cs->c_naux != 1)
	  {
	    /* anonymous union type */
	    type = coff_alloc_type (cs->c_symnum);
	    TYPE_NAME (type) = NULL;
	    /* This used to set the tag to "<opaque>".  But I think setting it
	       to NULL is right, and the printing code can print it as
	       "union {...}".  */
	    TYPE_TAG_NAME (type) = NULL;
	    INIT_CPLUS_SPECIFIC(type);
	    TYPE_LENGTH (type) = 0;
	    TYPE_FIELDS (type) = 0;
	    TYPE_NFIELDS (type) = 0;
	  }
	else
	  {
	    type = coff_read_struct_type (cs->c_symnum,
				    aux->x_sym.x_misc.x_lnsz.x_size,
				    aux->x_sym.x_fcnary.x_fcn.x_endndx.l);
	  }
	TYPE_CODE (type) = TYPE_CODE_UNION;
	return type;

      case T_ENUM:
	if (cs->c_naux != 1)
	  {
	    /* anonymous enum type */
	    type = coff_alloc_type (cs->c_symnum);
	    TYPE_CODE (type) = TYPE_CODE_ENUM;
	    TYPE_NAME (type) = NULL;
	    /* This used to set the tag to "<opaque>".  But I think setting it
	       to NULL is right, and the printing code can print it as
	       "enum {...}".  */
	    TYPE_TAG_NAME (type) = NULL;
	    TYPE_LENGTH (type) = 0;
	    TYPE_FIELDS (type) = 0;
	    TYPE_NFIELDS(type) = 0;
	  }
	else
	  {
	    type = coff_read_enum_type (cs->c_symnum,
					aux->x_sym.x_misc.x_lnsz.x_size,
					aux->x_sym.x_fcnary.x_fcn.x_endndx.l);
	  }
	return type;

      case T_MOE:
	/* shouldn't show up here */
	break;

      case T_UCHAR:
	return lookup_fundamental_type (current_objfile, FT_UNSIGNED_CHAR);

      case T_USHORT:
	return lookup_fundamental_type (current_objfile, FT_UNSIGNED_SHORT);

      case T_UINT:
	return lookup_fundamental_type (current_objfile, FT_UNSIGNED_INTEGER);

      case T_ULONG:
	return lookup_fundamental_type (current_objfile, FT_UNSIGNED_LONG);
    }
  complain (&unexpected_type_complaint, cs->c_name);
  return lookup_fundamental_type (current_objfile, FT_VOID);
}

/* This page contains subroutines of read_type.  */

/* Read the description of a structure (or union type)
   and return an object describing the type.  */

static struct type *
coff_read_struct_type (index, length, lastsym)
     int index;
     int length;
     int lastsym;
{
  struct nextfield
    {
      struct nextfield *next;
      struct field field;
    };

  register struct type *type;
  register struct nextfield *list = 0;
  struct nextfield *new;
  int nfields = 0;
  register int n;
  char *name;
  struct coff_symbol member_sym;
  register struct coff_symbol *ms = &member_sym;
  struct internal_syment sub_sym;
  union internal_auxent sub_aux;
  int done = 0;

  type = coff_alloc_type (index);
  TYPE_CODE (type) = TYPE_CODE_STRUCT;
  INIT_CPLUS_SPECIFIC(type);
  TYPE_LENGTH (type) = length;

  while (!done && symnum < lastsym && symnum < nlist_nsyms_global)
    {
      read_one_sym (ms, &sub_sym, &sub_aux);
      name = ms->c_name;
      name = EXTERNAL_NAME (name, current_objfile->obfd);

      switch (ms->c_sclass)
	{
	  case C_MOS:
	  case C_MOU:

	    /* Get space to record the next field's data.  */
	    new = (struct nextfield *) alloca (sizeof (struct nextfield));
	    new->next = list;
	    list = new;

	    /* Save the data.  */
	    list->field.name = savestring (name, strlen (name));
	    list->field.type = decode_type (ms, ms->c_type, &sub_aux);
	    list->field.bitpos = 8 * ms->c_value;
	    list->field.bitsize = 0;
	    nfields++;
	    break;

	  case C_FIELD:

	    /* Get space to record the next field's data.  */
	    new = (struct nextfield *) alloca (sizeof (struct nextfield));
	    new->next = list;
	    list = new;

	    /* Save the data.  */
	    list->field.name = savestring (name, strlen (name));
	    list->field.type = decode_type (ms, ms->c_type, &sub_aux);
	    list->field.bitpos = ms->c_value;
	    list->field.bitsize = sub_aux.x_sym.x_misc.x_lnsz.x_size;
	    nfields++;
	    break;

	  case C_EOS:
	    done = 1;
	    break;
	}
    }
  /* Now create the vector of fields, and record how big it is.  */

  TYPE_NFIELDS (type) = nfields;
  TYPE_FIELDS (type) = (struct field *)
    TYPE_ALLOC (type, sizeof (struct field) * nfields);

  /* Copy the saved-up fields into the field vector.  */

  for (n = nfields; list; list = list->next)
    TYPE_FIELD (type, --n) = list->field;

  return type;
}

/* Read a definition of an enumeration type,
   and create and return a suitable type object.
   Also defines the symbols that represent the values of the type.  */

/* ARGSUSED */
static struct type *
coff_read_enum_type (index, length, lastsym)
     int index;
     int length;
     int lastsym;
{
  register struct symbol *sym;
  register struct type *type;
  int nsyms = 0;
  int done = 0;
  struct pending **symlist;
  struct coff_symbol member_sym;
  register struct coff_symbol *ms = &member_sym;
  struct internal_syment sub_sym;
  union internal_auxent sub_aux;
  struct pending *osyms, *syms;
  int o_nsyms;
  register int n;
  char *name;

  type = coff_alloc_type (index);
  if (within_function)
    symlist = &local_symbols;
  else
    symlist = &file_symbols;
  osyms = *symlist;
  o_nsyms = osyms ? osyms->nsyms : 0;

  while (!done && symnum < lastsym && symnum < nlist_nsyms_global)
    {
      read_one_sym (ms, &sub_sym, &sub_aux);
      name = ms->c_name;
      name = EXTERNAL_NAME (name, current_objfile->obfd);

      switch (ms->c_sclass)
	{
	  case C_MOE:
	    sym = (struct symbol *) xmalloc (sizeof (struct symbol));
	    memset (sym, 0, sizeof (struct symbol));

	    SYMBOL_NAME (sym) = savestring (name, strlen (name));
	    SYMBOL_CLASS (sym) = LOC_CONST;
	    SYMBOL_NAMESPACE (sym) = VAR_NAMESPACE;
	    SYMBOL_VALUE (sym) = ms->c_value;
	    add_symbol_to_list (sym, symlist);
	    nsyms++;
	    break;

	  case C_EOS:
	    /* Sometimes the linker (on 386/ix 2.0.2 at least) screws
	       up the count of how many symbols to read.  So stop
	       on .eos.  */
	    done = 1;
	    break;
	}
    }

  /* Now fill in the fields of the type-structure.  */

  if (length > 0)
    TYPE_LENGTH (type) =  length;
  else
    TYPE_LENGTH (type) =  TARGET_INT_BIT / TARGET_CHAR_BIT;	/* Assume ints */
  TYPE_CODE (type) = TYPE_CODE_ENUM;
  TYPE_NFIELDS (type) = nsyms;
  TYPE_FIELDS (type) = (struct field *)
    TYPE_ALLOC (type, sizeof (struct field) * nsyms);

  /* Find the symbols for the values and put them into the type.
     The symbols can be found in the symlist that we put them on
     to cause them to be defined.  osyms contains the old value
     of that symlist; everything up to there was defined by us.  */
  /* Note that we preserve the order of the enum constants, so
     that in something like "enum {FOO, LAST_THING=FOO}" we print
     FOO, not LAST_THING.  */

  for (syms = *symlist, n = 0; syms; syms = syms->next)
    {
      int j = 0;
      if (syms == osyms)
	j = o_nsyms;
      for (; j < syms->nsyms; j++,n++)
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

#if 0
  /* This screws up perfectly good C programs with enums.  FIXME.  */
  /* Is this Modula-2's BOOLEAN type?  Flag it as such if so. */
  if(TYPE_NFIELDS(type) == 2 &&
     ((STREQ(TYPE_FIELD_NAME(type,0),"TRUE") &&
       STREQ(TYPE_FIELD_NAME(type,1),"FALSE")) ||
      (STREQ(TYPE_FIELD_NAME(type,1),"TRUE") &&
       STREQ(TYPE_FIELD_NAME(type,0),"FALSE"))))
     TYPE_CODE(type) = TYPE_CODE_BOOL;
#endif
  return type;
}

struct section_offsets *
coff_symfile_offsets (objfile, addr)
     struct objfile *objfile;
     CORE_ADDR addr;
{
  struct section_offsets *section_offsets;
  int i;
 
  section_offsets = (struct section_offsets *)
    obstack_alloc (&objfile -> psymbol_obstack,
		   sizeof (struct section_offsets) +
		          sizeof (section_offsets->offsets) * (SECT_OFF_MAX-1));

  for (i = 0; i < SECT_OFF_MAX; i++)
    ANOFFSET (section_offsets, i) = addr;
  
  return section_offsets;
}

/* Register our ability to parse symbols for coff BFD files */

static struct sym_fns coff_sym_fns =
{
  "coff",		/* sym_name: name or name prefix of BFD target type */
  4,			/* sym_namelen: number of significant sym_name chars */
  coff_new_init,	/* sym_new_init: init anything gbl to entire symtab */
  coff_symfile_init,	/* sym_init: read initial info, setup for sym_read() */
  coff_symfile_read,	/* sym_read: read a symbol file into symtab */
  coff_symfile_finish,	/* sym_finish: finished with file, cleanup */
  coff_symfile_offsets, /* sym_offsets:  xlate external to internal form */
  NULL			/* next: pointer to next struct sym_fns */
};

void
_initialize_coffread ()
{
  add_symtab_fns(&coff_sym_fns);
}
