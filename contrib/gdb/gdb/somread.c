/* Read HP PA/Risc object files for GDB.
   Copyright 1991, 1992, 1996 Free Software Foundation, Inc.
   Written by Fred Fish at Cygnus Support.

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

#include "defs.h"
#include "bfd.h"
#include <syms.h>
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "buildsym.h"
#include "stabsread.h"
#include "gdb-stabs.h"
#include "complaints.h"
#include "gdb_string.h"
#include "demangle.h"
#include "som.h"
#include "libhppa.h"

/* Various things we might complain about... */

static void
som_symfile_init PARAMS ((struct objfile *));

static void
som_new_init PARAMS ((struct objfile *));

static void
som_symfile_read PARAMS ((struct objfile *, struct section_offsets *, int));

static void
som_symfile_finish PARAMS ((struct objfile *));

static void
som_symtab_read PARAMS ((bfd *, struct objfile *,
			 struct section_offsets *));

static struct section_offsets *
som_symfile_offsets PARAMS ((struct objfile *, CORE_ADDR));

/* FIXME: These should really be in a common header somewhere */

extern void
hpread_build_psymtabs PARAMS ((struct objfile *, struct section_offsets *, int));

extern void
hpread_symfile_finish PARAMS ((struct objfile *));

extern void
hpread_symfile_init PARAMS ((struct objfile *));

extern void
do_pxdb PARAMS ((bfd *));

/*

LOCAL FUNCTION

	som_symtab_read -- read the symbol table of a SOM file

SYNOPSIS

	void som_symtab_read (bfd *abfd, struct objfile *objfile,
			      struct section_offsets *section_offsets)

DESCRIPTION

	Given an open bfd, a base address to relocate symbols to, and a
	flag that specifies whether or not this bfd is for an executable
	or not (may be shared library for example), add all the global
	function and data symbols to the minimal symbol table.
*/

static void
som_symtab_read (abfd, objfile, section_offsets)
     bfd *abfd;
     struct objfile *objfile;
     struct section_offsets *section_offsets;
{
  unsigned int number_of_symbols;
  int val, dynamic;
  char *stringtab;
  asection *shlib_info;
  struct symbol_dictionary_record *buf, *bufp, *endbufp;
  char *symname;
  CONST int symsize = sizeof (struct symbol_dictionary_record);
  CORE_ADDR text_offset, data_offset;


  text_offset = ANOFFSET (section_offsets, 0);
  data_offset = ANOFFSET (section_offsets, 1);

  number_of_symbols = bfd_get_symcount (abfd);

  buf = alloca (symsize * number_of_symbols);
  bfd_seek (abfd, obj_som_sym_filepos (abfd), SEEK_SET);
  val = bfd_read (buf, symsize * number_of_symbols, 1, abfd);
  if (val != symsize * number_of_symbols)
    error ("Couldn't read symbol dictionary!");

  stringtab = alloca (obj_som_stringtab_size (abfd));
  bfd_seek (abfd, obj_som_str_filepos (abfd), SEEK_SET);
  val = bfd_read (stringtab, obj_som_stringtab_size (abfd), 1, abfd);
  if (val != obj_som_stringtab_size (abfd))
    error ("Can't read in HP string table.");

  /* We need to determine if objfile is a dynamic executable (so we
     can do the right thing for ST_ENTRY vs ST_CODE symbols).

     There's nothing in the header which easily allows us to do
     this.  The only reliable way I know of is to check for the
     existance of a $SHLIB_INFO$ section with a non-zero size.  */
  /* The code below is not a reliable way to check whether an
   * executable is dynamic, so I commented it out - RT
   * shlib_info = bfd_get_section_by_name (objfile->obfd, "$SHLIB_INFO$");
   * if (shlib_info)
   *   dynamic = (bfd_section_size (objfile->obfd, shlib_info) != 0);
   * else
   *   dynamic = 0;
   */
  /* I replaced the code with a simple check for text offset not being
   * zero. Still not 100% reliable, but a more reliable way of asking
   * "is this a dynamic executable?" than the above. RT
   */
  dynamic = (text_offset != 0);

  endbufp = buf + number_of_symbols;
  for (bufp = buf; bufp < endbufp; ++bufp)
    {
      enum minimal_symbol_type ms_type;

      QUIT;

      switch (bufp->symbol_scope)
	{
	case SS_UNIVERSAL:
	case SS_EXTERNAL:
	  switch (bufp->symbol_type)
	    {
	    case ST_SYM_EXT:
	    case ST_ARG_EXT:
	      continue;

	    case ST_CODE:
	    case ST_PRI_PROG:
	    case ST_SEC_PROG:
	    case ST_MILLICODE:
	      symname = bufp->name.n_strx + stringtab;
	      ms_type = mst_text;
	      bufp->symbol_value += text_offset;
#ifdef SMASH_TEXT_ADDRESS
	      SMASH_TEXT_ADDRESS (bufp->symbol_value);
#endif
	      break;

	    case ST_ENTRY:
	      symname = bufp->name.n_strx + stringtab;
	      /* For a dynamic executable, ST_ENTRY symbols are
		 the stubs, while the ST_CODE symbol is the real
		 function.  */
	      if (dynamic)
		ms_type = mst_solib_trampoline;
	      else
		ms_type = mst_text;
	      bufp->symbol_value += text_offset;
#ifdef SMASH_TEXT_ADDRESS
	      SMASH_TEXT_ADDRESS (bufp->symbol_value);
#endif
	      break;

	    case ST_STUB:
	      symname = bufp->name.n_strx + stringtab;
	      ms_type = mst_solib_trampoline;
	      bufp->symbol_value += text_offset;
#ifdef SMASH_TEXT_ADDRESS
	      SMASH_TEXT_ADDRESS (bufp->symbol_value);
#endif
	      break;

	    case ST_DATA:
	      symname = bufp->name.n_strx + stringtab;
	      bufp->symbol_value += data_offset;
	      ms_type = mst_data;
	      break;
	    default:
	      continue;
	    }
	  break;

#if 0
	  /* SS_GLOBAL and SS_LOCAL are two names for the same thing (!).  */
	case SS_GLOBAL:
#endif
	case SS_LOCAL:
	  switch (bufp->symbol_type)
	    {
	    case ST_SYM_EXT:
	    case ST_ARG_EXT:
	      continue;

	    case ST_CODE:
	      symname = bufp->name.n_strx + stringtab;
	      ms_type = mst_file_text;
	      bufp->symbol_value += text_offset;
#ifdef SMASH_TEXT_ADDRESS
	      SMASH_TEXT_ADDRESS (bufp->symbol_value);
#endif

	    check_strange_names:
	      /* Utah GCC 2.5, FSF GCC 2.6 and later generate correct local
		 label prefixes for stabs, constant data, etc.  So we need
		 only filter out L$ symbols which are left in due to
		 limitations in how GAS generates SOM relocations.

		 When linking in the HPUX C-library the HP linker has
		 the nasty habit of placing section symbols from the literal
		 subspaces in the middle of the program's text.  Filter
		 those out as best we can.  Check for first and last character
		 being '$'. 

		 And finally, the newer HP compilers emit crud like $PIC_foo$N
		 in some circumstance (PIC code I guess).  It's also claimed
		 that they emit D$ symbols too.  What stupidity.  */
	      if ((symname[0] == 'L' && symname[1] == '$')
		  || (symname[0] == '$' && symname[strlen(symname) - 1] == '$')
		  || (symname[0] == 'D' && symname[1] == '$')
		  || (strncmp (symname, "$PIC", 4) == 0))
		continue;
	      break;

	    case ST_PRI_PROG:
	    case ST_SEC_PROG:
	    case ST_MILLICODE:
	      symname = bufp->name.n_strx + stringtab;
	      ms_type = mst_file_text;
	      bufp->symbol_value += text_offset;
#ifdef SMASH_TEXT_ADDRESS
	      SMASH_TEXT_ADDRESS (bufp->symbol_value);
#endif
	      break;

	    case ST_ENTRY:
	      symname = bufp->name.n_strx + stringtab;
	      /* For a dynamic executable, ST_ENTRY symbols are
		 the stubs, while the ST_CODE symbol is the real
		 function.  */
	      if (dynamic)
		ms_type = mst_solib_trampoline;
	      else
		ms_type = mst_file_text;
	      bufp->symbol_value += text_offset;
#ifdef SMASH_TEXT_ADDRESS
	      SMASH_TEXT_ADDRESS (bufp->symbol_value);
#endif
	      break;

	    case ST_STUB:
	      symname = bufp->name.n_strx + stringtab;
	      ms_type = mst_solib_trampoline;
	      bufp->symbol_value += text_offset;
#ifdef SMASH_TEXT_ADDRESS
	      SMASH_TEXT_ADDRESS (bufp->symbol_value);
#endif
	      break;


	    case ST_DATA:
	      symname = bufp->name.n_strx + stringtab;
	      bufp->symbol_value += data_offset;
	      ms_type = mst_file_data;
	      goto check_strange_names;

	    default:
	      continue;
	    }
	  break;

	/* This can happen for common symbols when -E is passed to the
	   final link.  No idea _why_ that would make the linker force
	   common symbols to have an SS_UNSAT scope, but it does.

	   This also happens for weak symbols, but their type is
	   ST_DATA.  */
	case SS_UNSAT:
	  switch (bufp->symbol_type)
	    {
	      case ST_STORAGE:
	      case ST_DATA:
		symname = bufp->name.n_strx + stringtab;
		bufp->symbol_value += data_offset;
		ms_type = mst_data;
		break;

	      default:
		continue;
	    }
	  break;

	default:
	  continue;
	}

      if (bufp->name.n_strx > obj_som_stringtab_size (abfd))
	error ("Invalid symbol data; bad HP string table offset: %d",
	       bufp->name.n_strx);

      prim_record_minimal_symbol (symname, bufp->symbol_value, ms_type, 
				  objfile);
    }
}

/* Scan and build partial symbols for a symbol file.
   We have been initialized by a call to som_symfile_init, which 
   currently does nothing.

   SECTION_OFFSETS is a set of offsets to apply to relocate the symbols
   in each section.  This is ignored, as it isn't needed for SOM.

   MAINLINE is true if we are reading the main symbol
   table (as opposed to a shared lib or dynamically loaded file).

   This function only does the minimum work necessary for letting the
   user "name" things symbolically; it does not read the entire symtab.
   Instead, it reads the external and static symbols and puts them in partial
   symbol tables.  When more extensive information is requested of a
   file, the corresponding partial symbol table is mutated into a full
   fledged symbol table by going back and reading the symbols
   for real.

   We look for sections with specific names, to tell us what debug
   format to look for:  FIXME!!!

   somstab_build_psymtabs() handles STABS symbols.

   Note that SOM files have a "minimal" symbol table, which is vaguely
   reminiscent of a COFF symbol table, but has only the minimal information
   necessary for linking.  We process this also, and use the information to
   build gdb's minimal symbol table.  This gives us some minimal debugging
   capability even for files compiled without -g.  */

static void
som_symfile_read (objfile, section_offsets, mainline)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     int mainline;
{
  bfd *abfd = objfile->obfd;
  struct cleanup *back_to;

  do_pxdb (symfile_bfd_open (objfile->name));

  init_minimal_symbol_collection ();
  back_to = make_cleanup ((make_cleanup_func) discard_minimal_symbols, 0);

  /* Read in the import list and the export list.  Currently
     the export list isn't used; the import list is used in
     hp-symtab-read.c to handle static vars declared in other
     shared libraries. */
  init_import_symbols (objfile);
#if 0   /* Export symbols not used today 1997-08-05 */ 
  init_export_symbols (objfile);
#else
  objfile->export_list = NULL;
  objfile->export_list_size = 0;
#endif

  /* Process the normal SOM symbol table first. 
     This reads in the DNTT and string table, but doesn't
     actually scan the DNTT. It does scan the linker symbol
     table and thus build up a "minimal symbol table". */
 
  som_symtab_read (abfd, objfile, section_offsets);

  /* Now read information from the stabs debug sections.
     This is a no-op for SOM.
     Perhaps it is intended for some kind of mixed STABS/SOM
     situation? */    
  stabsect_build_psymtabs (objfile, section_offsets, mainline,
			   "$GDB_SYMBOLS$", "$GDB_STRINGS$", "$TEXT$");

  /* Now read the native debug information. 
     This builds the psymtab. This used to be done via a scan of
     the DNTT, but is now done via the PXDB-built quick-lookup tables
     together with a scan of the GNTT. See hp-psymtab-read.c. */
  hpread_build_psymtabs (objfile, section_offsets, mainline);

  /* Install any minimal symbols that have been collected as the current
     minimal symbols for this objfile. 
     Further symbol-reading is done incrementally, file-by-file,
     in a step known as "psymtab-to-symtab" expansion. hp-symtab-read.c
     contains the code to do the actual DNTT scanning and symtab building. */
  install_minimal_symbols (objfile);

  /* Force hppa-tdep.c to re-read the unwind descriptors.  */
  objfile->obj_private = NULL;
  do_cleanups (back_to);
}

/* Initialize anything that needs initializing when a completely new symbol
   file is specified (not just adding some symbols from another file, e.g. a
   shared library).

   We reinitialize buildsym, since we may be reading stabs from a SOM file.  */

static void
som_new_init (ignore)
     struct objfile *ignore;
{
  stabsread_new_init ();
  buildsym_new_init ();
}

/* Perform any local cleanups required when we are done with a particular
   objfile.  I.E, we are in the process of discarding all symbol information
   for an objfile, freeing up all memory held for it, and unlinking the
   objfile struct from the global list of known objfiles. */

static void
som_symfile_finish (objfile)
     struct objfile *objfile;
{
  if (objfile -> sym_stab_info != NULL)
    {
      mfree (objfile -> md, objfile -> sym_stab_info);
    }
  hpread_symfile_finish (objfile);
}

/* SOM specific initialization routine for reading symbols.  */

static void
som_symfile_init (objfile)
     struct objfile *objfile;
{
  /* SOM objects may be reordered, so set OBJF_REORDERED.  If we
     find this causes a significant slowdown in gdb then we could
     set it in the debug symbol readers only when necessary.  */
  objfile->flags |= OBJF_REORDERED;
  hpread_symfile_init (objfile);
}

/* SOM specific parsing routine for section offsets.

   Plain and simple for now.  */

static struct section_offsets *
som_symfile_offsets (objfile, addr)
     struct objfile *objfile;
     CORE_ADDR addr;
{
  struct section_offsets *section_offsets;
  int i;

  objfile->num_sections = SECT_OFF_MAX;
  section_offsets = (struct section_offsets *)
    obstack_alloc (&objfile -> psymbol_obstack, SIZEOF_SECTION_OFFSETS);

  /* First see if we're a shared library.  If so, get the section
     offsets from the library, else get them from addr.  */
  if (!som_solib_section_offsets (objfile, section_offsets))
    {
      for (i = 0; i < SECT_OFF_MAX; i++)
	ANOFFSET (section_offsets, i) = addr;
    }

  return section_offsets;
}



/* Check if a given symbol NAME is in the import list
   of OBJFILE.
   1 => true, 0 => false
   This is used in hp_symtab_read.c to deal with static variables
   that are defined in a different shared library than the one
   whose symbols are being processed. */ 

int is_in_import_list (name, objfile)
  char * name;
  struct objfile * objfile;
{
  register int i;

  if (!objfile ||
      !name ||
      !*name)
    return 0;

  for (i=0; i < objfile->import_list_size; i++)
    if (objfile->import_list[i] && STREQ (name, objfile->import_list[i]))
        return 1;
  return 0;
}


/* Read in and initialize the SOM import list which is present
   for all executables and shared libraries.  The import list
   consists of the symbols that are referenced in OBJFILE but
   not defined there.  (Variables that are imported are dealt
   with as "loc_indirect" vars.)
   Return value = number of import symbols read in. */
int
init_import_symbols (objfile)
  struct objfile * objfile;
{
  unsigned int import_list;
  unsigned int import_list_size;
  unsigned int string_table;
  unsigned int string_table_size;
  char * string_buffer;
  register int i;
  register int j;
  register int k;
  asection * text_section;      /* section handle */ 
  unsigned int dl_header[12];   /* SOM executable header */ 

  /* A struct for an entry in the SOM import list */
  typedef struct {
    int name;              /* index into the string table */ 
    short dont_care1;      /* we don't use this */ 
    unsigned char type;    /* 0 = NULL, 2 = Data, 3 = Code, 7 = Storage, 13 = Plabel */ 
    unsigned int reserved2 : 8; /* not used */ 
  } SomImportEntry;

  /* We read 100 entries in at a time from the disk file. */ 
# define SOM_READ_IMPORTS_NUM         100
# define SOM_READ_IMPORTS_CHUNK_SIZE  (sizeof (SomImportEntry) * SOM_READ_IMPORTS_NUM)
  SomImportEntry buffer[SOM_READ_IMPORTS_NUM];
  
  /* Initialize in case we error out */
  objfile->import_list = NULL;
  objfile->import_list_size = 0;

#if 0  /* DEBUGGING */
  printf ("Processing import list for %s\n", objfile->name);
#endif

  /* It doesn't work, for some reason, to read in space $TEXT$;
     the subspace $SHLIB_INFO$ has to be used.  Some BFD quirk? pai/1997-08-05 */ 
  text_section = bfd_get_section_by_name (objfile->obfd, "$SHLIB_INFO$");
  if (!text_section)
    return 0;
  /* Get the SOM executable header */ 
  bfd_get_section_contents (objfile->obfd, text_section, dl_header, 0, 12 * sizeof (int));

  /* Check header version number for 10.x HP-UX */
  /* Currently we deal only with 10.x systems; on 9.x the version # is 89060912.
     FIXME: Change for future HP-UX releases and mods to the SOM executable format */ 
  if (dl_header[0] != 93092112)
    return 0;
  
  import_list = dl_header[4];     
  import_list_size = dl_header[5];
  if (!import_list_size)
    return 0;
  string_table = dl_header[10];     
  string_table_size = dl_header[11];
  if (!string_table_size)
    return 0;

  /* Suck in SOM string table */ 
  string_buffer = (char *) xmalloc (string_table_size);
  bfd_get_section_contents (objfile->obfd, text_section, string_buffer,
                            string_table, string_table_size);

  /* Allocate import list in the psymbol obstack; this has nothing
     to do with psymbols, just a matter of convenience.  We want the
     import list to be freed when the objfile is deallocated */ 
  objfile->import_list
    = (ImportEntry *) obstack_alloc (&objfile->psymbol_obstack,
                                      import_list_size * sizeof (ImportEntry));

  /* Read in the import entries, a bunch at a time */ 
  for (j=0, k=0;
       j < (import_list_size / SOM_READ_IMPORTS_NUM);
       j++)
    {
      bfd_get_section_contents (objfile->obfd, text_section, buffer,
                                import_list + j * SOM_READ_IMPORTS_CHUNK_SIZE,
                                SOM_READ_IMPORTS_CHUNK_SIZE);
      for (i=0; i < SOM_READ_IMPORTS_NUM; i++, k++)
        {
          if (buffer[i].type != (unsigned char) 0) 
            {
              objfile->import_list[k]
                = (char *) obstack_alloc (&objfile->psymbol_obstack, strlen (string_buffer + buffer[i].name) + 1);
              strcpy (objfile->import_list[k], string_buffer + buffer[i].name);
              /* Some day we might want to record the type and other information too */ 
            }
          else /* null type */ 
            objfile->import_list[k] = NULL;
          
#if 0 /* DEBUGGING */
          printf ("Import String %d:%d (%d), type %d is %s\n", j, i, k,
                  (int) buffer[i].type, objfile->import_list[k]);
#endif
        }
    }

  /* Get the leftovers */ 
  if (k < import_list_size)
    bfd_get_section_contents (objfile->obfd, text_section, buffer,
                              import_list + k * sizeof (SomImportEntry),
                              (import_list_size - k) * sizeof (SomImportEntry));
  for (i=0; k < import_list_size; i++, k++)
    {
      if (buffer[i].type != (unsigned char) 0)
        {
          objfile->import_list[k]
            = (char *) obstack_alloc (&objfile->psymbol_obstack, strlen (string_buffer + buffer[i].name) + 1);
          strcpy (objfile->import_list[k], string_buffer + buffer[i].name);
          /* Some day we might want to record the type and other information too */ 
        }
      else
        objfile->import_list[k] = NULL;
#if 0 /* DEBUGGING */ 
      printf ("Import String F:%d (%d), type %d, is %s\n", i, k,
              (int) buffer[i].type, objfile->import_list[k]);
#endif
    }

  objfile->import_list_size = import_list_size;
  free (string_buffer);
  return import_list_size;
}

/* Read in and initialize the SOM export list which is present
   for all executables and shared libraries.  The import list
   consists of the symbols that are referenced in OBJFILE but
   not defined there.  (Variables that are imported are dealt
   with as "loc_indirect" vars.)
   Return value = number of import symbols read in. */
int
init_export_symbols (objfile)
  struct objfile * objfile;
{
  unsigned int export_list;
  unsigned int export_list_size;
  unsigned int string_table;
  unsigned int string_table_size;
  char * string_buffer;
  register int i;
  register int j;
  register int k;
  asection * text_section;    /* section handle */  
  unsigned int dl_header[12]; /* SOM executable header */ 

  /* A struct for an entry in the SOM export list */
  typedef struct {
    int next;    /* for hash table use -- we don't use this */ 
    int name;    /* index into string table */ 
    int value;   /* offset or plabel */
    int dont_care1;     /* not used */ 
    unsigned char type; /* 0 = NULL, 2 = Data, 3 = Code, 7 = Storage, 13 = Plabel */ 
    char dont_care2;    /* not used */ 
    short dont_care3;   /* not used */ 
  } SomExportEntry;

  /* We read 100 entries in at a time from the disk file. */ 
# define SOM_READ_EXPORTS_NUM         100  
# define SOM_READ_EXPORTS_CHUNK_SIZE  (sizeof (SomExportEntry) * SOM_READ_EXPORTS_NUM)
  SomExportEntry buffer[SOM_READ_EXPORTS_NUM];

  /* Initialize in case we error out */
  objfile->export_list = NULL;
  objfile->export_list_size = 0;

#if 0  /* DEBUGGING */
  printf ("Processing export list for %s\n", objfile->name);
#endif

  /* It doesn't work, for some reason, to read in space $TEXT$;
     the subspace $SHLIB_INFO$ has to be used.  Some BFD quirk? pai/1997-08-05 */ 
  text_section = bfd_get_section_by_name (objfile->obfd, "$SHLIB_INFO$");
  if (!text_section)
    return 0;
  /* Get the SOM executable header */ 
  bfd_get_section_contents (objfile->obfd, text_section, dl_header, 0, 12 * sizeof (int));

  /* Check header version number for 10.x HP-UX */
  /* Currently we deal only with 10.x systems; on 9.x the version # is 89060912.
     FIXME: Change for future HP-UX releases and mods to the SOM executable format */ 
  if (dl_header[0] != 93092112)
    return 0;
  
  export_list = dl_header[8];      
  export_list_size = dl_header[9]; 
  if (!export_list_size)
    return 0;
  string_table = dl_header[10];     
  string_table_size = dl_header[11];
  if (!string_table_size)
    return 0;

  /* Suck in SOM string table */ 
  string_buffer = (char *) xmalloc (string_table_size);
  bfd_get_section_contents (objfile->obfd, text_section, string_buffer,
                            string_table, string_table_size);

  /* Allocate export list in the psymbol obstack; this has nothing
     to do with psymbols, just a matter of convenience.  We want the
     export list to be freed when the objfile is deallocated */ 
  objfile->export_list
    = (ExportEntry *)  obstack_alloc (&objfile->psymbol_obstack,
                                      export_list_size * sizeof (ExportEntry));

  /* Read in the export entries, a bunch at a time */ 
  for (j=0, k=0;
       j < (export_list_size / SOM_READ_EXPORTS_NUM);
       j++)
    {
      bfd_get_section_contents (objfile->obfd, text_section, buffer,
                                export_list + j * SOM_READ_EXPORTS_CHUNK_SIZE,
                                SOM_READ_EXPORTS_CHUNK_SIZE);
      for (i=0; i < SOM_READ_EXPORTS_NUM; i++, k++)
        {
          if (buffer[i].type != (unsigned char) 0)
            {
              objfile->export_list[k].name
                = (char *) obstack_alloc (&objfile->psymbol_obstack, strlen (string_buffer + buffer[i].name) + 1);
              strcpy (objfile->export_list[k].name, string_buffer + buffer[i].name);
              objfile->export_list[k].address = buffer[i].value;
              /* Some day we might want to record the type and other information too */ 
            }
          else /* null type */ 
            { 
              objfile->export_list[k].name = NULL;
              objfile->export_list[k].address = 0;
            }
#if 0 /* DEBUGGING */
          printf ("Export String %d:%d (%d), type %d is %s\n", j, i, k,
                  (int) buffer[i].type, objfile->export_list[k].name);
#endif
        }
    }

  /* Get the leftovers */ 
  if (k < export_list_size)
    bfd_get_section_contents (objfile->obfd, text_section, buffer,
                              export_list + k * sizeof (SomExportEntry),
                              (export_list_size - k) * sizeof (SomExportEntry));
  for (i=0; k < export_list_size; i++, k++)
    {
      if (buffer[i].type != (unsigned char) 0)
        {
          objfile->export_list[k].name
            = (char *) obstack_alloc (&objfile->psymbol_obstack, strlen (string_buffer + buffer[i].name) + 1);
          strcpy (objfile->export_list[k].name, string_buffer + buffer[i].name);
          /* Some day we might want to record the type and other information too */ 
          objfile->export_list[k].address = buffer[i].value;
        }
      else
        {
          objfile->export_list[k].name = NULL;
          objfile->export_list[k].address = 0;
        }
#if 0 /* DEBUGGING */ 
      printf ("Export String F:%d (%d), type %d, value %x is %s\n", i, k,
              (int) buffer[i].type, buffer[i].value, objfile->export_list[k].name);
#endif
    }

  objfile->export_list_size = export_list_size;
  free (string_buffer);
  return export_list_size;
}



/* Register that we are able to handle SOM object file formats.  */

static struct sym_fns som_sym_fns =
{
  bfd_target_som_flavour,
  som_new_init,		/* sym_new_init: init anything gbl to entire symtab */
  som_symfile_init,	/* sym_init: read initial info, setup for sym_read() */
  som_symfile_read,	/* sym_read: read a symbol file into symtab */
  som_symfile_finish,	/* sym_finish: finished with file, cleanup */
  som_symfile_offsets,	/* sym_offsets:  Translate ext. to int. relocation */
  NULL			/* next: pointer to next struct sym_fns */
};

void
_initialize_somread ()
{
  add_symtab_fns (&som_sym_fns);
}
