/* Read hp debug symbols and convert to internal format, for GDB.
   Copyright 1993, 1996, 1998, 1999 Free Software Foundation, Inc.

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

/* Common include file for hp_symtab_read.c and hp_psymtab_read.c.
   This has nested includes of a bunch of stuff. */
#include "hpread.h"
#include "demangle.h"

/* To generate dumping code, uncomment this define.  The dumping
   itself is controlled by routine-local statics called "dumping". */
/* #define DUMPING         1 */

/* To use the quick look-up tables, uncomment this define. */
#define QUICK_LOOK_UP      1

/* To call PXDB to process un-processed files, uncomment this define. */
#define USE_PXDB           1

/* Forward procedure declarations */

void hpread_symfile_init
  PARAMS ((struct objfile *));

void
do_pxdb PARAMS ((bfd *));

void hpread_build_psymtabs
  PARAMS ((struct objfile *, struct section_offsets *, int));

void hpread_symfile_finish
  PARAMS ((struct objfile *));

static union dnttentry *hpread_get_gntt
  PARAMS ((int, struct objfile *));

static unsigned long hpread_get_textlow
  PARAMS ((int, int, struct objfile *, int));

static struct partial_symtab *hpread_start_psymtab
  PARAMS ((struct objfile *, struct section_offsets *, char *, CORE_ADDR, int,
	   struct partial_symbol **, struct partial_symbol **));

static struct partial_symtab *hpread_end_psymtab
  PARAMS ((struct partial_symtab *, char **, int, int, CORE_ADDR,
	   struct partial_symtab **, int));

/* End of forward routine declarations */

#ifdef USE_PXDB

/* NOTE use of system files!  May not be portable. */

#define PXDB_SVR4 "/opt/langtools/bin/pxdb"
#define PXDB_BSD  "/usr/bin/pxdb"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* check for the existance of a file, given its full pathname */
int
file_exists (filename)
     char *filename;
{
  if (filename)
    return (access (filename, F_OK) == 0);
  return 0;
}


/* Translate from the "hp_language" enumeration in hp-symtab.h
   used in the debug info to gdb's generic enumeration in defs.h. */
static enum language
trans_lang (in_lang)
     enum hp_language in_lang;
{
  if (in_lang == HP_LANGUAGE_C)
    return language_c;

  else if (in_lang == HP_LANGUAGE_CPLUSPLUS)
    return language_cplus;

  else if (in_lang == HP_LANGUAGE_F77)
    return language_fortran;

  else
    return language_unknown;
}

static char main_string[] = "main";

/* Call PXDB to process our file.

   Approach copied from DDE's "dbgk_run_pxdb".  Note: we
   don't check for BSD location of pxdb, nor for existance
   of pxdb itself, etc.

   NOTE: uses system function and string functions directly.

   Return value: 1 if ok, 0 if not */
int
hpread_call_pxdb (file_name)
     char *file_name;
{
  char *p;
  int status;
  int retval;

  if (file_exists (PXDB_SVR4))
    {
      p = malloc (strlen (PXDB_SVR4) + strlen (file_name) + 2);
      strcpy (p, PXDB_SVR4);
      strcat (p, " ");
      strcat (p, file_name);

      warning ("File not processed by pxdb--about to process now.\n");
      status = system (p);

      retval = (status == 0);
    }
  else
    {
      warning ("pxdb not found at standard location: /opt/langtools/bin\ngdb will not be able to debug %s.\nPlease install pxdb at the above location and then restart gdb.\nYou can also run pxdb on %s with the command\n\"pxdb %s\" and then restart gdb.", file_name, file_name, file_name);

      retval = 0;
    }
  return retval;
}				/* hpread_call_pxdb */


/* Return 1 if the file turns out to need pre-processing
   by PXDB, and we have thus called PXDB to do this processing
   and the file therefore needs to be re-loaded.  Otherwise
   return 0. */
int
hpread_pxdb_needed (sym_bfd)
     bfd *sym_bfd;
{
  asection *pinfo_section, *debug_section, *header_section;
  unsigned int do_pxdb;
  char *buf;
  bfd_size_type header_section_size;

  unsigned long tmp;
  unsigned int pxdbed;

  header_section = bfd_get_section_by_name (sym_bfd, "$HEADER$");
  if (!header_section)
    {
      return 0;			/* No header at all, can't recover... */
    }

  debug_section = bfd_get_section_by_name (sym_bfd, "$DEBUG$");
  pinfo_section = bfd_get_section_by_name (sym_bfd, "$PINFO$");

  if (pinfo_section && !debug_section)
    {
      /* Debug info with DOC, has different header format. 
         this only happens if the file was pxdbed and compiled optimized
         otherwise the PINFO section is not there. */
      header_section_size = bfd_section_size (objfile->obfd, header_section);

      if (header_section_size == (bfd_size_type) sizeof (DOC_info_PXDB_header))
	{
	  buf = alloca (sizeof (DOC_info_PXDB_header));

	  if (!bfd_get_section_contents (sym_bfd,
					 header_section,
					 buf, 0,
					 header_section_size))
	    error ("bfd_get_section_contents\n");

	  tmp = bfd_get_32 (sym_bfd, (bfd_byte *) (buf + sizeof (int) * 4));
	  pxdbed = (tmp >> 31) & 0x1;

	  if (!pxdbed)
	    error ("file debug header info invalid\n");
	  do_pxdb = 0;
	}

      else
	error ("invalid $HEADER$ size in executable \n");
    }

  else
    {

      /* this can be three different cases:
         1. pxdbed and not doc
         - DEBUG and HEADER sections are there
         - header is PXDB_header type
         - pxdbed flag is set to 1

         2. not pxdbed and doc
         - DEBUG and HEADER  sections are there
         - header is DOC_info_header type
         - pxdbed flag is set to 0

         3. not pxdbed and not doc
         - DEBUG and HEADER sections are there
         - header is XDB_header type
         - pxdbed flag is set to 0

         NOTE: the pxdbed flag is meaningful also in the not
         already pxdb processed version of the header,
         because in case on non-already processed by pxdb files
         that same bit in the header would be always zero.
         Why? Because the bit is the leftmost bit of a word
         which contains a 'length' which is always a positive value
         so that bit is never set to 1 (otherwise it would be negative)

         Given the above, we have two choices : either we ignore the
         size of the header itself and just look at the pxdbed field,
         or we check the size and then we (for safety and paranoia related
         issues) check the bit.
         The first solution is used by DDE, the second by PXDB itself.
         I am using the second one here, because I already wrote it,
         and it is the end of a long day.
         Also, using the first approach would still involve size issues
         because we need to read in the contents of the header section, and
         give the correct amount of stuff we want to read to the
         get_bfd_section_contents function.  */

      /* decide which case depending on the size of the header section.
         The size is as defined in hp-symtab.h  */

      header_section_size = bfd_section_size (objfile->obfd, header_section);

      if (header_section_size == (bfd_size_type) sizeof (PXDB_header))	/* pxdb and not doc */
	{

	  buf = alloca (sizeof (PXDB_header));
	  if (!bfd_get_section_contents (sym_bfd,
					 header_section,
					 buf, 0,
					 header_section_size))
	    error ("bfd_get_section_contents\n");

	  tmp = bfd_get_32 (sym_bfd, (bfd_byte *) (buf + sizeof (int) * 3));
	  pxdbed = (tmp >> 31) & 0x1;

	  if (pxdbed)
	    do_pxdb = 0;
	  else
	    error ("file debug header invalid\n");
	}
      else			/*not pxdbed and doc OR not pxdbed and non doc */
	do_pxdb = 1;
    }

  if (do_pxdb)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}				/* hpread_pxdb_needed */

#endif

/* Check whether the file needs to be preprocessed by pxdb. 
   If so, call pxdb. */

void 
do_pxdb (sym_bfd)
     bfd *sym_bfd;
{
  /* The following code is HP-specific.  The "right" way of
     doing this is unknown, but we bet would involve a target-
     specific pre-file-load check using a generic mechanism. */

  /* This code will not be executed if the file is not in SOM
     format (i.e. if compiled with gcc) */
    if (hpread_pxdb_needed (sym_bfd)) 
      {
	/*This file has not been pre-processed. Preprocess now */
	  
	if (hpread_call_pxdb (sym_bfd->filename))
	  {
	    /* The call above has changed the on-disk file, 
               we can close the file anyway, because the
	       symbols will be reread in when the target is run */
	    bfd_close (sym_bfd); 
	  }
      }
}



#ifdef QUICK_LOOK_UP

/* Code to handle quick lookup-tables follows. */


/* Some useful macros */
#define VALID_FILE(i)   ((i) < pxdb_header_p->fd_entries)
#define VALID_MODULE(i) ((i) < pxdb_header_p->md_entries)
#define VALID_PROC(i)   ((i) < pxdb_header_p->pd_entries)
#define VALID_CLASS(i)  ((i) < pxdb_header_p->cd_entries)

#define FILE_START(i)    (qFD[i].adrStart)
#define MODULE_START(i) (qMD[i].adrStart)
#define PROC_START(i)    (qPD[i].adrStart)

#define FILE_END(i)   (qFD[i].adrEnd)
#define MODULE_END(i) (qMD[i].adrEnd)
#define PROC_END(i)   (qPD[i].adrEnd)

#define FILE_ISYM(i)   (qFD[i].isym)
#define MODULE_ISYM(i) (qMD[i].isym)
#define PROC_ISYM(i)   (qPD[i].isym)

#define VALID_CURR_FILE    (curr_fd < pxdb_header_p->fd_entries)
#define VALID_CURR_MODULE  (curr_md < pxdb_header_p->md_entries)
#define VALID_CURR_PROC    (curr_pd < pxdb_header_p->pd_entries)
#define VALID_CURR_CLASS   (curr_cd < pxdb_header_p->cd_entries)

#define CURR_FILE_START     (qFD[curr_fd].adrStart)
#define CURR_MODULE_START   (qMD[curr_md].adrStart)
#define CURR_PROC_START     (qPD[curr_pd].adrStart)

#define CURR_FILE_END    (qFD[curr_fd].adrEnd)
#define CURR_MODULE_END  (qMD[curr_md].adrEnd)
#define CURR_PROC_END    (qPD[curr_pd].adrEnd)

#define CURR_FILE_ISYM    (qFD[curr_fd].isym)
#define CURR_MODULE_ISYM  (qMD[curr_md].isym)
#define CURR_PROC_ISYM    (qPD[curr_pd].isym)

#define TELL_OBJFILE                                      \
            do {                                          \
               if( !told_objfile ) {                      \
                   told_objfile = 1;                      \
                   warning ("\nIn object file \"%s\":\n", \
                            objfile->name);               \
               }                                          \
            } while (0)



/* Keeping track of the start/end symbol table (LNTT) indices of
   psymtabs created so far */

typedef struct
  {
    int start;
    int end;
  }
pst_syms_struct;

static pst_syms_struct *pst_syms_array = 0;

static pst_syms_count = 0;
static pst_syms_size = 0;

/* used by the TELL_OBJFILE macro */
static boolean told_objfile = 0;

/* Set up psymtab symbol index stuff */
static void
init_pst_syms ()
{
  pst_syms_count = 0;
  pst_syms_size = 20;
  pst_syms_array = (pst_syms_struct *) xmalloc (20 * sizeof (pst_syms_struct));
}

/* Clean up psymtab symbol index stuff */
static void
clear_pst_syms ()
{
  pst_syms_count = 0;
  pst_syms_size = 0;
  free (pst_syms_array);
  pst_syms_array = 0;
}

/* Add information about latest psymtab to symbol index table */
static void
record_pst_syms (start_sym, end_sym)
     int start_sym;
     int end_sym;
{
  if (++pst_syms_count > pst_syms_size)
    {
      pst_syms_array = (pst_syms_struct *) xrealloc (pst_syms_array,
			      2 * pst_syms_size * sizeof (pst_syms_struct));
      pst_syms_size *= 2;
    }
  pst_syms_array[pst_syms_count - 1].start = start_sym;
  pst_syms_array[pst_syms_count - 1].end = end_sym;
}

/* Find a suitable symbol table index which can serve as the upper
   bound of a psymtab that starts at INDEX

   This scans backwards in the psymtab symbol index table to find a
   "hole" in which the given index can fit.  This is a heuristic!!
   We don't search the entire table to check for multiple holes,
   we don't care about overlaps, etc. 

   Return 0 => not found */
static int
find_next_pst_start (index)
     int index;
{
  int i;

  for (i = pst_syms_count - 1; i >= 0; i--)
    if (pst_syms_array[i].end <= index)
      return (i == pst_syms_count - 1) ? 0 : pst_syms_array[i + 1].start - 1;

  if (pst_syms_array[0].start > index)
    return pst_syms_array[0].start - 1;

  return 0;
}



/* Utility functions to find the ending symbol index for a psymtab */

/* Find the next file entry that begins beyond INDEX, and return
   its starting symbol index - 1.
   QFD is the file table, CURR_FD is the file entry from where to start,
   PXDB_HEADER_P as in hpread_quick_traverse (to allow macros to work).

   Return 0 => not found */
static int
find_next_file_isym (index, qFD, curr_fd, pxdb_header_p)
     int index;
     quick_file_entry *qFD;
     int curr_fd;
     PXDB_header_ptr pxdb_header_p;
{
  while (VALID_CURR_FILE)
    {
      if (CURR_FILE_ISYM >= index)
	return CURR_FILE_ISYM - 1;
      curr_fd++;
    }
  return 0;
}

/* Find the next procedure entry that begins beyond INDEX, and return
   its starting symbol index - 1.
   QPD is the procedure table, CURR_PD is the proc entry from where to start,
   PXDB_HEADER_P as in hpread_quick_traverse (to allow macros to work).

   Return 0 => not found */
static int
find_next_proc_isym (index, qPD, curr_pd, pxdb_header_p)
     int index;
     quick_procedure_entry *qPD;
     int curr_pd;
     PXDB_header_ptr pxdb_header_p;
{
  while (VALID_CURR_PROC)
    {
      if (CURR_PROC_ISYM >= index)
	return CURR_PROC_ISYM - 1;
      curr_pd++;
    }
  return 0;
}

/* Find the next module entry that begins beyond INDEX, and return
   its starting symbol index - 1.
   QMD is the module table, CURR_MD is the modue entry from where to start,
   PXDB_HEADER_P as in hpread_quick_traverse (to allow macros to work).

   Return 0 => not found */
static int
find_next_module_isym (index, qMD, curr_md, pxdb_header_p)
     int index;
     quick_module_entry *qMD;
     int curr_md;
     PXDB_header_ptr pxdb_header_p;
{
  while (VALID_CURR_MODULE)
    {
      if (CURR_MODULE_ISYM >= index)
	return CURR_MODULE_ISYM - 1;
      curr_md++;
    }
  return 0;
}

/* Scan and record partial symbols for all functions starting from index
   pointed to by CURR_PD_P, and between code addresses START_ADR and END_ADR.
   Other parameters are explained in comments below. */

/* This used to be inline in hpread_quick_traverse, but now that we do essentially the
   same thing for two different cases (modules and module-less files), it's better
   organized in a separate routine, although it does take lots of arguments. pai/1997-10-08 */

static int
scan_procs (curr_pd_p, qPD, max_procs, start_adr, end_adr, pst, vt_bits, objfile, section_offsets)
     int *curr_pd_p;		/* pointer to current proc index */
     quick_procedure_entry *qPD;	/* the procedure quick lookup table */
     int max_procs;		/* number of entries in proc. table */
     CORE_ADDR start_adr;	/* beginning of code range for current psymtab */
     CORE_ADDR end_adr;		/* end of code range for current psymtab */
     struct partial_symtab *pst;	/* current psymtab */
     char *vt_bits;		/* strings table of SOM debug space */
     struct objfile *objfile;	/* current object file */
     struct section_offsets *section_offsets;	/* not really used for HP-UX currently */
{
  union dnttentry *dn_bufp;
  int symbol_count = 0;		/* Total number of symbols in this psymtab */
  int curr_pd = *curr_pd_p;	/* Convenience variable -- avoid dereferencing pointer all the time */

#ifdef DUMPING
  /* Turn this on for lots of debugging information in this routine */
  static int dumping = 0;
#endif

#ifdef DUMPING
  if (dumping)
    {
      printf ("Scan_procs called, addresses %x to %x, proc %x\n", start_adr, end_adr, curr_pd);
    }
#endif

  while ((CURR_PROC_START <= end_adr) && (curr_pd < max_procs))
    {

      char *rtn_name;		/* mangled name */
      char *rtn_dem_name;	/* qualified demangled name */
      char *class_name;
      int class;

      if ((trans_lang ((enum hp_language) qPD[curr_pd].language) == language_cplus) &&
	  vt_bits[(long) qPD[curr_pd].sbAlias])		/* not a null string */
	{
	  /* Get mangled name for the procedure, and demangle it */
	  rtn_name = &vt_bits[(long) qPD[curr_pd].sbAlias];
	  rtn_dem_name = cplus_demangle (rtn_name, DMGL_ANSI | DMGL_PARAMS);
	}
      else
	{
	  rtn_name = &vt_bits[(long) qPD[curr_pd].sbProc];
	  rtn_dem_name = NULL;
	}

      /* Hack to get around HP C/C++ compilers' insistence on providing
         "_MAIN_" as an alternate name for "main" */
      if ((strcmp (rtn_name, "_MAIN_") == 0) &&
	  (strcmp (&vt_bits[(long) qPD[curr_pd].sbProc], "main") == 0))
	rtn_dem_name = rtn_name = main_string;

#ifdef DUMPING
      if (dumping)
	{
	  printf ("..add %s (demangled %s), index %x to this psymtab\n", rtn_name, rtn_dem_name, curr_pd);
	}
#endif

      /* Check for module-spanning routines. */
      if (CURR_PROC_END > end_adr)
	{
	  TELL_OBJFILE;
	  warning ("Procedure \"%s\" [0x%x] spans file or module boundaries.", rtn_name, curr_pd);
	}

      /* Add this routine symbol to the list in the objfile. 
         Unfortunately we have to go to the LNTT to determine the
         correct list to put it on. An alternative (which the
         code used to do) would be to not check and always throw
         it on the "static" list. But if we go that route, then
         symbol_lookup() needs to be tweaked a bit to account
         for the fact that the function might not be found on
         the correct list in the psymtab. - RT */
      dn_bufp = hpread_get_lntt (qPD[curr_pd].isym, objfile);
      if (dn_bufp->dfunc.global)
	add_psymbol_with_dem_name_to_list (rtn_name,
					   strlen (rtn_name),
					   rtn_dem_name,
					   strlen (rtn_dem_name),
					   VAR_NAMESPACE,
					   LOC_BLOCK,	/* "I am a routine"        */
					   &objfile->global_psymbols,
					   (qPD[curr_pd].adrStart + /* Starting address of rtn */
					    ANOFFSET (section_offsets, SECT_OFF_TEXT)),
					   0,	/* core addr?? */
					   trans_lang ((enum hp_language) qPD[curr_pd].language),
					   objfile);
      else
	add_psymbol_with_dem_name_to_list (rtn_name,
					   strlen (rtn_name),
					   rtn_dem_name,
					   strlen (rtn_dem_name),
					   VAR_NAMESPACE,
					   LOC_BLOCK,	/* "I am a routine"        */
					   &objfile->static_psymbols,
					   (qPD[curr_pd].adrStart +  /* Starting address of rtn */
					    ANOFFSET (section_offsets, SECT_OFF_TEXT)),
					   0,	/* core addr?? */
					   trans_lang ((enum hp_language) qPD[curr_pd].language),
					   objfile);

      symbol_count++;
      *curr_pd_p = ++curr_pd;	/* bump up count & reflect in caller */
    }				/* loop over procedures */

#ifdef DUMPING
  if (dumping)
    {
      if (symbol_count == 0)
	printf ("Scan_procs: no symbols found!\n");
    }
#endif

  return symbol_count;
}


/* Traverse the quick look-up tables, building a set of psymtabs.

   This constructs a psymtab for modules and files in the quick lookup
   tables.

   Mostly, modules correspond to compilation units, so we try to
   create psymtabs that correspond to modules; however, in some cases
   a file can result in a compiled object which does not have a module
   entry for it, so in such cases we create a psymtab for the file.  */

int
hpread_quick_traverse (objfile,	section_offsets, gntt_bits, vt_bits, pxdb_header_p)	
     struct objfile *objfile;        /* The object file descriptor */
     struct section_offsets *section_offsets; /* ?? Null for HP */
     char *gntt_bits;                /* GNTT entries, loaded in from the file */
     char *vt_bits;                  /* VT (string) entries ditto. */
     PXDB_header_ptr pxdb_header_p;  /* Pointer to pxdb header ditto */
{
  struct partial_symtab *pst;

  char *addr;

  quick_procedure_entry *qPD;
  quick_file_entry *qFD;
  quick_module_entry *qMD;
  quick_class_entry *qCD;

  int idx;
  int i;
  CORE_ADDR start_adr;		/* current psymtab's starting code addr   */
  CORE_ADDR end_adr;		/* current psymtab's ending code addr     */
  CORE_ADDR next_mod_adr;	/* next module's starting code addr    */
  int curr_pd;			/* current procedure */
  int curr_fd;			/* current file      */
  int curr_md;			/* current module    */
  int start_sym;		/* current psymtab's starting symbol index */
  int end_sym;			/* current psymtab's ending symbol index   */
  int max_LNTT_sym_index;
  int syms_in_pst;
  B_TYPE *class_entered;

  struct partial_symbol **global_syms;	/* We'll be filling in the "global"   */
  struct partial_symbol **static_syms;	/* and "static" tables in the objfile
                                           as we go, so we need a pair of     
                                           current pointers. */

#ifdef DUMPING
  /* Turn this on for lots of debugging information in this routine.
     You get a blow-by-blow account of quick lookup table reading */
  static int dumping = 0;
#endif

  pst = (struct partial_symtab *) 0;

  /* Clear out some globals */
  init_pst_syms ();
  told_objfile = 0;

  /* Demangling style -- if EDG style already set, don't change it,
     as HP style causes some problems with the KAI EDG compiler */
  if (current_demangling_style != edg_demangling)
    {
      /* Otherwise, ensure that we are using HP style demangling */
      set_demangling_style (HP_DEMANGLING_STYLE_STRING);
    }

  /* First we need to find the starting points of the quick
     look-up tables in the GNTT. */

  addr = gntt_bits;

  qPD = (quick_procedure_entry_ptr) addr;
  addr += pxdb_header_p->pd_entries * sizeof (quick_procedure_entry);

#ifdef DUMPING
  if (dumping)
    {
      printf ("\n Printing routines as we see them\n");
      for (i = 0; VALID_PROC (i); i++)
	{
	  idx = (long) qPD[i].sbProc;
	  printf ("%s %x..%x\n", &vt_bits[idx],
		  (int) PROC_START (i),
		  (int) PROC_END (i));
	}
    }
#endif

  qFD = (quick_file_entry_ptr) addr;
  addr += pxdb_header_p->fd_entries * sizeof (quick_file_entry);

#ifdef DUMPING
  if (dumping)
    {
      printf ("\n Printing files as we see them\n");
      for (i = 0; VALID_FILE (i); i++)
	{
	  idx = (long) qFD[i].sbFile;
	  printf ("%s %x..%x\n", &vt_bits[idx],
		  (int) FILE_START (i),
		  (int) FILE_END (i));
	}
    }
#endif

  qMD = (quick_module_entry_ptr) addr;
  addr += pxdb_header_p->md_entries * sizeof (quick_module_entry);

#ifdef DUMPING
  if (dumping)
    {
      printf ("\n Printing modules as we see them\n");
      for (i = 0; i < pxdb_header_p->md_entries; i++)
	{
	  idx = (long) qMD[i].sbMod;
	  printf ("%s\n", &vt_bits[idx]);
	}
    }
#endif

  qCD = (quick_class_entry_ptr) addr;
  addr += pxdb_header_p->cd_entries * sizeof (quick_class_entry);

#ifdef DUMPING
  if (dumping)
    {
      printf ("\n Printing classes as we see them\n");
      for (i = 0; VALID_CLASS (i); i++)
	{
	  idx = (long) qCD[i].sbClass;
	  printf ("%s\n", &vt_bits[idx]);
	}

      printf ("\n Done with dump, on to build!\n");
    }
#endif

  /* We need this index only while hp-symtab-read.c expects
     a byte offset to the end of the LNTT entries for a given
     psymtab.  Thus the need for it should go away someday.

     When it goes away, then we won't have any need to load the
     LNTT from the objfile at psymtab-time, and start-up will be
     faster.  To make that work, we'll need some way to create
     a null pst for the "globals" pseudo-module. */
  max_LNTT_sym_index = LNTT_SYMCOUNT (objfile);

  /* Scan the module descriptors and make a psymtab for each.

     We know the MDs, FDs and the PDs are in order by starting
     address.  We use that fact to traverse all three arrays in
     parallel, knowing when the next PD is in a new file
     and we need to create a new psymtab. */
  curr_pd = 0;			/* Current procedure entry */
  curr_fd = 0;			/* Current file entry */
  curr_md = 0;			/* Current module entry */

  start_adr = 0;		/* Current psymtab code range */
  end_adr = 0;

  start_sym = 0;		/* Current psymtab symbol range */
  end_sym = 0;

  syms_in_pst = 0;		/* Symbol count for psymtab */

  /* Psts actually just have pointers into the objfile's
     symbol table, not their own symbol tables. */
  global_syms = objfile->global_psymbols.list;
  static_syms = objfile->static_psymbols.list;


  /* First skip over pseudo-entries with address 0.  These represent inlined
     routines and abstract (uninstantiated) template routines.
     FIXME: These should be read in and available -- even if we can't set
     breakpoints, etc., there's some information that can be presented
     to the user. pai/1997-10-08  */

  while (VALID_CURR_PROC && (CURR_PROC_START == 0))
    curr_pd++;

  /* Loop over files, modules, and procedures in code address order. Each
     time we enter an iteration of this loop, curr_pd points to the first
     unprocessed procedure, curr_fd points to the first unprocessed file, and
     curr_md to the first unprocessed module.  Each iteration of this loop
     updates these as required -- any or all of them may be bumpd up
     each time around.  When we exit this loop, we are done with all files
     and modules in the tables -- there may still be some procedures, however.

     Note: This code used to loop only over module entries, under the assumption
     that files can occur via inclusions and are thus unreliable, while a
     compiled object always corresponds to a module.  With CTTI in the HP aCC
     compiler, it turns out that compiled objects may have only files and no
     modules; so we have to loop over files and modules, creating psymtabs for
     either as appropriate.  Unfortunately there are some problems (notably:
     1. the lack of "SRC_FILE_END" entries in the LNTT, 2. the lack of pointers
     to the ending symbol indices of a module or a file) which make it quite hard
     to do this correctly.  Currently it uses a bunch of heuristics to start and
     end psymtabs; they seem to work well with most objects generated by aCC, but
     who knows when that will change...   */

  while (VALID_CURR_FILE || VALID_CURR_MODULE)
    {

      char *mod_name_string;
      char *full_name_string;

      /* First check for modules like "version.c", which have no code
         in them but still have qMD entries.  They also have no qFD or
         qPD entries.  Their start address is -1 and their end address
         is 0.  */
      if (VALID_CURR_MODULE && (CURR_MODULE_START == -1) && (CURR_MODULE_END == 0))
	{

	  mod_name_string = &vt_bits[(long) qMD[curr_md].sbMod];

#ifdef DUMPING
	  if (dumping)
	    printf ("Module with data only %s\n", mod_name_string);
#endif

	  /* We'll skip the rest (it makes error-checking easier), and
	     just make an empty pst.  Right now empty psts are not put
	     in the pst chain, so all this is for naught, but later it
	     might help.  */

	  pst = hpread_start_psymtab (objfile,
				      section_offsets,	        /* ?? */
				      mod_name_string,
				      CURR_MODULE_START,	/* Low text address: bogus! */
				      (CURR_MODULE_ISYM * sizeof (struct dntt_type_block)),
				                                /* ldsymoff */
				      global_syms,
				      static_syms);

	  pst = hpread_end_psymtab (pst,
				    NULL,	/* psymtab_include_list */
				    0,		/* includes_used        */
				    end_sym * sizeof (struct dntt_type_block),
				                /* byte index in LNTT of end 
				                   = capping symbol offset  
				                   = LDSYMOFF of nextfile */
				    0,	        /* text high            */
				    NULL,	/* dependency_list      */
				    0);	        /* dependencies_used    */

	  global_syms = objfile->global_psymbols.next;
	  static_syms = objfile->static_psymbols.next;

	  curr_md++;
	}
      else if (VALID_CURR_MODULE &&
	       ((CURR_MODULE_START == 0) || (CURR_MODULE_START == -1) ||
		(CURR_MODULE_END == 0) || (CURR_MODULE_END == -1)))
	{
	  TELL_OBJFILE;
	  warning ("Module \"%s\" [0x%x] has non-standard addresses.  It starts at 0x%x, ends at 0x%x, and will be skipped.",
		   mod_name_string, curr_md, start_adr, end_adr);
	  /* On to next module */
	  curr_md++;
	}
      else
	{
	  /* First check if we are looking at a file with code in it
	     that does not overlap the current module's code range */

	  if (VALID_CURR_FILE ? (VALID_CURR_MODULE ? (CURR_FILE_END < CURR_MODULE_START) : 1) : 0)
	    {

	      /* Looking at file not corresponding to any module,
	         create a psymtab for it */
	      full_name_string = &vt_bits[(long) qFD[curr_fd].sbFile];
	      start_adr = CURR_FILE_START;
	      end_adr = CURR_FILE_END;
	      start_sym = CURR_FILE_ISYM;

	      /* Check if there are any procedures not handled until now, that
	         begin before the start address of this file, and if so, adjust
	         this module's start address to include them.  This handles routines that
	         are in between file or module ranges for some reason (probably
	         indicates a compiler bug */

	      if (CURR_PROC_START < start_adr)
		{
		  TELL_OBJFILE;
		  warning ("Found procedure \"%s\" [0x%x] that is not in any file or module.",
			   &vt_bits[(long) qPD[curr_pd].sbProc], curr_pd);
		  start_adr = CURR_PROC_START;
		  if (CURR_PROC_ISYM < start_sym)
		    start_sym = CURR_PROC_ISYM;
		}

	      /* Sometimes (compiler bug -- COBOL) the module end address is higher
	         than the start address of the next module, so check for that and
	         adjust accordingly */

	      if (VALID_FILE (curr_fd + 1) && (FILE_START (curr_fd + 1) <= end_adr))
		{
		  TELL_OBJFILE;
		  warning ("File \"%s\" [0x%x] has ending address after starting address of next file; adjusting ending address down.",
			   full_name_string, curr_fd);
		  end_adr = FILE_START (curr_fd + 1) - 1;	/* Is -4 (or -8 for 64-bit) better? */
		}
	      if (VALID_MODULE (curr_md) && (CURR_MODULE_START <= end_adr))
		{
		  TELL_OBJFILE;
		  warning ("File \"%s\" [0x%x] has ending address after starting address of next module; adjusting ending address down.",
			   full_name_string, curr_fd);
		  end_adr = CURR_MODULE_START - 1;	/* Is -4 (or -8 for 64-bit) better? */
		}


#ifdef DUMPING
	      if (dumping)
		{
		  printf ("Make new psymtab for file %s (%x to %x).\n",
			  full_name_string, start_adr, end_adr);
		}
#endif
	      /* Create the basic psymtab, connecting it in the list
	         for this objfile and pointing its symbol entries
	         to the current end of the symbol areas in the objfile.

	         The "ldsymoff" parameter is the byte offset in the LNTT
	         of the first symbol in this file.  Some day we should
	         turn this into an index (fix in hp-symtab-read.c as well).
	         And it's not even the right byte offset, as we're using
	         the size of a union! FIXME!  */
	      pst = hpread_start_psymtab (objfile,
					  section_offsets,	/* ?? */
					  full_name_string,
					  start_adr,	        /* Low text address */
					  (start_sym * sizeof (struct dntt_type_block)),
					                        /* ldsymoff */
					  global_syms,
					  static_syms);

	      /* Set up to only enter each class referenced in this module once.  */
	      class_entered = malloc (B_BYTES (pxdb_header_p->cd_entries));
	      B_CLRALL (class_entered, pxdb_header_p->cd_entries);

	      /* Scan the procedure descriptors for procedures in the current
	         file, based on the starting addresses. */

	      syms_in_pst = scan_procs (&curr_pd, qPD, pxdb_header_p->pd_entries,
					start_adr, end_adr,
					pst, vt_bits, objfile, section_offsets);

	      /* Get ending symbol offset */

	      end_sym = 0;
	      /* First check for starting index before previous psymtab */
	      if (pst_syms_count && start_sym < pst_syms_array[pst_syms_count - 1].end)
		{
		  end_sym = find_next_pst_start (start_sym);
		}
	      /* Look for next start index of a file or module, or procedure */
	      if (!end_sym)
		{
		  int next_file_isym = find_next_file_isym (start_sym, qFD, curr_fd + 1, pxdb_header_p);
		  int next_module_isym = find_next_module_isym (start_sym, qMD, curr_md, pxdb_header_p);
		  int next_proc_isym = find_next_proc_isym (start_sym, qPD, curr_pd, pxdb_header_p);

		  if (next_file_isym && next_module_isym)
		    {
		      /* pick lower of next file or module start index */
		      end_sym = min (next_file_isym, next_module_isym);
		    }
		  else
		    {
		      /* one of them is zero, pick the other */
		      end_sym = max (next_file_isym, next_module_isym);
		    }

		  /* As a precaution, check next procedure index too */
		  if (!end_sym)
		    end_sym = next_proc_isym;
		  else
		    end_sym = min (end_sym, next_proc_isym);
		}

	      /* Couldn't find procedure, file, or module, use globals as default */
	      if (!end_sym)
		end_sym = pxdb_header_p->globals;

#ifdef DUMPING
	      if (dumping)
		{
		  printf ("File psymtab indices: %x to %x\n", start_sym, end_sym);
		}
#endif

	      pst = hpread_end_psymtab (pst,
					NULL,	/* psymtab_include_list */
					0,	/* includes_used        */
					end_sym * sizeof (struct dntt_type_block),
					        /* byte index in LNTT of end 
						   = capping symbol offset   
						   = LDSYMOFF of nextfile */
					end_adr,	/* text high */
					NULL,	/* dependency_list */
					0);	/* dependencies_used */

	      record_pst_syms (start_sym, end_sym);

	      if (NULL == pst)
		warning ("No symbols in psymtab for file \"%s\" [0x%x].", full_name_string, curr_fd);

#ifdef DUMPING
	      if (dumping)
		{
		  printf ("Made new psymtab for file %s (%x to %x), sym %x to %x.\n",
			  full_name_string, start_adr, end_adr, CURR_FILE_ISYM, end_sym);
		}
#endif
	      /* Prepare for the next psymtab. */
	      global_syms = objfile->global_psymbols.next;
	      static_syms = objfile->static_psymbols.next;
	      free (class_entered);

	      curr_fd++;
	    }			/* Psymtab for file */
	  else
	    {
	      /* We have a module for which we create a psymtab */

	      mod_name_string = &vt_bits[(long) qMD[curr_md].sbMod];

	      /* We will include the code ranges of any files that happen to
	         overlap with this module */

	      /* So, first pick the lower of the file's and module's start addresses */
	      start_adr = CURR_MODULE_START;
	      if (VALID_CURR_FILE)
		{
		  if (CURR_FILE_START < CURR_MODULE_START)
		    {
		      TELL_OBJFILE;
		      warning ("File \"%s\" [0x%x] crosses beginning of module \"%s\".",
			       &vt_bits[(long) qFD[curr_fd].sbFile],
			       curr_fd, mod_name_string);

		      start_adr = CURR_FILE_START;
		    }
		}

	      /* Also pick the lower of the file's and the module's start symbol indices */
	      start_sym = CURR_MODULE_ISYM;
	      if (VALID_CURR_FILE && (CURR_FILE_ISYM < CURR_MODULE_ISYM))
		start_sym = CURR_FILE_ISYM;

	      /* For the end address, we scan through the files till we find one
	         that overlaps the current module but ends beyond it; if no such file exists we
	         simply use the module's start address.  
	         (Note, if file entries themselves overlap
	         we take the longest overlapping extension beyond the end of the module...)
	         We assume that modules never overlap. */

	      end_adr = CURR_MODULE_END;

	      if (VALID_CURR_FILE)
		{
		  while (VALID_CURR_FILE && (CURR_FILE_START < end_adr))
		    {

#ifdef DUMPING
		      if (dumping)
			printf ("Maybe skipping file %s which overlaps with module %s\n",
				&vt_bits[(long) qFD[curr_fd].sbFile], mod_name_string);
#endif
		      if (CURR_FILE_END > end_adr)
			{
			  TELL_OBJFILE;
			  warning ("File \"%s\" [0x%x] crosses end of module \"%s\".",
				   &vt_bits[(long) qFD[curr_fd].sbFile],
				   curr_fd, mod_name_string);
			  end_adr = CURR_FILE_END;
			}
		      curr_fd++;
		    }
		  curr_fd--;	/* back up after going too far */
		}

	      /* Sometimes (compiler bug -- COBOL) the module end address is higher
	         than the start address of the next module, so check for that and
	         adjust accordingly */

	      if (VALID_MODULE (curr_md + 1) && (MODULE_START (curr_md + 1) <= end_adr))
		{
		  TELL_OBJFILE;
		  warning ("Module \"%s\" [0x%x] has ending address after starting address of next module; adjusting ending address down.",
			   mod_name_string, curr_md);
		  end_adr = MODULE_START (curr_md + 1) - 1;	/* Is -4 (or -8 for 64-bit) better? */
		}
	      if (VALID_FILE (curr_fd + 1) && (FILE_START (curr_fd + 1) <= end_adr))
		{
		  TELL_OBJFILE;
		  warning ("Module \"%s\" [0x%x] has ending address after starting address of next file; adjusting ending address down.",
			   mod_name_string, curr_md);
		  end_adr = FILE_START (curr_fd + 1) - 1;	/* Is -4 (or -8 for 64-bit) better? */
		}

	      /* Use one file to get the full name for the module.  This
	         situation can arise if there is executable code in a #include
	         file.  Each file with code in it gets a qFD.  Files which don't
	         contribute code don't get a qFD, even if they include files
	         which do, e.g.: 

	         body.c:                    rtn.h:
	         int x;                     int main() {
	         #include "rtn.h"               return x;
	         }

	         There will a qFD for "rtn.h",and a qMD for "body.c",
	         but no qMD for "rtn.h" or qFD for "body.c"!

	         We pick the name of the last file to overlap with this
	         module.  C convention is to put include files first.  In a
	         perfect world, we could check names and use the file whose full
	         path name ends with the module name. */

	      if (VALID_CURR_FILE)
		full_name_string = &vt_bits[(long) qFD[curr_fd].sbFile];
	      else
		full_name_string = mod_name_string;

	      /* Check if there are any procedures not handled until now, that
	         begin before the start address we have now, and if so, adjust
	         this psymtab's start address to include them.  This handles routines that
	         are in between file or module ranges for some reason (probably
	         indicates a compiler bug */

	      if (CURR_PROC_START < start_adr)
		{
		  TELL_OBJFILE;
		  warning ("Found procedure \"%s\" [0x%x] that is not in any file or module.",
			   &vt_bits[(long) qPD[curr_pd].sbProc], curr_pd);
		  start_adr = CURR_PROC_START;
		  if (CURR_PROC_ISYM < start_sym)
		    start_sym = CURR_PROC_ISYM;
		}

#ifdef DUMPING
	      if (dumping)
		{
		  printf ("Make new psymtab for module %s (%x to %x), using file %s\n",
		     mod_name_string, start_adr, end_adr, full_name_string);
		}
#endif
	      /* Create the basic psymtab, connecting it in the list
	         for this objfile and pointing its symbol entries
	         to the current end of the symbol areas in the objfile.

	         The "ldsymoff" parameter is the byte offset in the LNTT
	         of the first symbol in this file.  Some day we should
	         turn this into an index (fix in hp-symtab-read.c as well).
	         And it's not even the right byte offset, as we're using
	         the size of a union! FIXME!  */
	      pst = hpread_start_psymtab (objfile,
					  section_offsets,	/* ?? */
					  full_name_string,
					  start_adr,	/* Low text address */
					  (start_sym * sizeof (struct dntt_type_block)),
					                /* ldsymoff */
					  global_syms,
					  static_syms);

	      /* Set up to only enter each class referenced in this module once.  */
	      class_entered = malloc (B_BYTES (pxdb_header_p->cd_entries));
	      B_CLRALL (class_entered, pxdb_header_p->cd_entries);

	      /* Scan the procedure descriptors for procedures in the current
	         module, based on the starting addresses. */

	      syms_in_pst = scan_procs (&curr_pd, qPD, pxdb_header_p->pd_entries,
					start_adr, end_adr,
					pst, vt_bits, objfile, section_offsets);

	      /* Get ending symbol offset */

	      end_sym = 0;
	      /* First check for starting index before previous psymtab */
	      if (pst_syms_count && start_sym < pst_syms_array[pst_syms_count - 1].end)
		{
		  end_sym = find_next_pst_start (start_sym);
		}
	      /* Look for next start index of a file or module, or procedure */
	      if (!end_sym)
		{
		  int next_file_isym = find_next_file_isym (start_sym, qFD, curr_fd + 1, pxdb_header_p);
		  int next_module_isym = find_next_module_isym (start_sym, qMD, curr_md + 1, pxdb_header_p);
		  int next_proc_isym = find_next_proc_isym (start_sym, qPD, curr_pd, pxdb_header_p);

		  if (next_file_isym && next_module_isym)
		    {
		      /* pick lower of next file or module start index */
		      end_sym = min (next_file_isym, next_module_isym);
		    }
		  else
		    {
		      /* one of them is zero, pick the other */
		      end_sym = max (next_file_isym, next_module_isym);
		    }

		  /* As a precaution, check next procedure index too */
		  if (!end_sym)
		    end_sym = next_proc_isym;
		  else
		    end_sym = min (end_sym, next_proc_isym);
		}

	      /* Couldn't find procedure, file, or module, use globals as default */
	      if (!end_sym)
		end_sym = pxdb_header_p->globals;

#ifdef DUMPING
	      if (dumping)
		{
		  printf ("Module psymtab indices: %x to %x\n", start_sym, end_sym);
		}
#endif

	      pst = hpread_end_psymtab (pst,
					NULL,	/* psymtab_include_list */
					0,	/* includes_used        */
					end_sym * sizeof (struct dntt_type_block),
					        /* byte index in LNTT of end 
						   = capping symbol offset   
						   = LDSYMOFF of nextfile */
					end_adr,	/* text high */
					NULL,	/* dependency_list      */
					0);	/* dependencies_used    */

	      record_pst_syms (start_sym, end_sym);

	      if (NULL == pst)
		warning ("No symbols in psymtab for module \"%s\" [0x%x].", mod_name_string, curr_md);

#ifdef DUMPING
	      if (dumping)
		{
		  printf ("Made new psymtab for module %s (%x to %x), sym %x to %x.\n",
			  mod_name_string, start_adr, end_adr, CURR_MODULE_ISYM, end_sym);
		}
#endif

	      /* Prepare for the next psymtab. */
	      global_syms = objfile->global_psymbols.next;
	      static_syms = objfile->static_psymbols.next;
	      free (class_entered);

	      curr_md++;
	      curr_fd++;
	    }			/* psymtab for module */
	}			/* psymtab for non-bogus file or module */
    }				/* End of while loop over all files & modules */

  /* There may be some routines after all files and modules -- these will get
     inserted in a separate new module of their own */
  if (VALID_CURR_PROC)
    {
      start_adr = CURR_PROC_START;
      end_adr = qPD[pxdb_header_p->pd_entries - 1].adrEnd;
      TELL_OBJFILE;
      warning ("Found functions beyond end of all files and modules [0x%x].", curr_pd);
#ifdef DUMPING
      if (dumping)
	{
	  printf ("Orphan functions at end, PD %d and beyond (%x to %x)\n",
		  curr_pd, start_adr, end_adr);
	}
#endif
      pst = hpread_start_psymtab (objfile,
				  section_offsets,	/* ?? */
				  "orphans",
				  start_adr,	/* Low text address */
				  (CURR_PROC_ISYM * sizeof (struct dntt_type_block)),
				                /* ldsymoff */
				  global_syms,
				  static_syms);

      scan_procs (&curr_pd, qPD, pxdb_header_p->pd_entries,
		  start_adr, end_adr,
		  pst, vt_bits, objfile, section_offsets);

      pst = hpread_end_psymtab (pst,
				NULL,	/* psymtab_include_list */
				0,	/* includes_used */
				pxdb_header_p->globals * sizeof (struct dntt_type_block),
				        /* byte index in LNTT of end 
					   = capping symbol offset   
					   = LDSYMOFF of nextfile */
				end_adr,	/* text high  */
				NULL,	/* dependency_list */
				0);	/* dependencies_used */
    }


#ifdef NEVER_NEVER
  /* Now build psts for non-module things (in the tail of
     the LNTT, after the last END MODULE entry).

     If null psts were kept on the chain, this would be
     a solution.  FIXME */
  pst = hpread_start_psymtab (objfile,
			      section_offsets,
			      "globals",
			      0,
			      (pxdb_header_p->globals
			       * sizeof (struct dntt_type_block)),
			      objfile->global_psymbols.next,
			      objfile->static_psymbols.next);
  hpread_end_psymtab (pst,
		      NULL, 0,
		      (max_LNTT_sym_index * sizeof (struct dntt_type_block)),
		      0,
		      NULL, 0);
#endif

  clear_pst_syms ();

  return 1;

}				/* End of hpread_quick_traverse. */


/* Get appropriate header, based on pxdb type. 
   Return value: 1 if ok, 0 if not */
int
hpread_get_header (objfile, pxdb_header_p)
     struct objfile *objfile;
     PXDB_header_ptr pxdb_header_p;
{
  asection *pinfo_section, *debug_section, *header_section;

#ifdef DUMPING
  /* Turn on for debugging information */
  static int dumping = 0;
#endif

  header_section = bfd_get_section_by_name (objfile->obfd, "$HEADER$");
  if (!header_section)
    {
      /* We don't have either PINFO or DEBUG sections.  But
         stuff like "libc.sl" has no debug info.  There's no
         need to warn the user of this, as it may be ok. The
         caller will figure it out and issue any needed
         messages. */
#ifdef DUMPING
      if (dumping)
	printf ("==No debug info at all for %s.\n", objfile->name);
#endif

      return 0;
    }

  /* We would like either a $DEBUG$ or $PINFO$ section.
     Once we know which, we can understand the header
     data (which we have defined to suit the more common
     $DEBUG$ case). */
  debug_section = bfd_get_section_by_name (objfile->obfd, "$DEBUG$");
  pinfo_section = bfd_get_section_by_name (objfile->obfd, "$PINFO$");
  if (debug_section)
    {
      /* The expected case: normal pxdb header. */
      bfd_get_section_contents (objfile->obfd, header_section,
				pxdb_header_p, 0, sizeof (PXDB_header));

      if (!pxdb_header_p->pxdbed)
	{
	  /* This shouldn't happen if we check in "symfile.c". */
	  return 0;
	}			/* DEBUG section */
    }

  else if (pinfo_section)
    {
      /* The DOC case; we need to translate this into a
         regular header. */
      DOC_info_PXDB_header doc_header;

#ifdef DUMPING
      if (dumping)
	{
	  printf ("==OOps, PINFO, let's try to handle this, %s.\n", objfile->name);
	}
#endif

      bfd_get_section_contents (objfile->obfd,
				header_section,
				&doc_header, 0,
				sizeof (DOC_info_PXDB_header));

      if (!doc_header.pxdbed)
	{
	  /* This shouldn't happen if we check in "symfile.c". */
	  warning ("File \"%s\" not processed by pxdb!", objfile->name);
	  return 0;
	}

      /* Copy relevent fields to standard header passed in. */
      pxdb_header_p->pd_entries = doc_header.pd_entries;
      pxdb_header_p->fd_entries = doc_header.fd_entries;
      pxdb_header_p->md_entries = doc_header.md_entries;
      pxdb_header_p->pxdbed = doc_header.pxdbed;
      pxdb_header_p->bighdr = doc_header.bighdr;
      pxdb_header_p->sa_header = doc_header.sa_header;
      pxdb_header_p->inlined = doc_header.inlined;
      pxdb_header_p->globals = doc_header.globals;
      pxdb_header_p->time = doc_header.time;
      pxdb_header_p->pg_entries = doc_header.pg_entries;
      pxdb_header_p->functions = doc_header.functions;
      pxdb_header_p->files = doc_header.files;
      pxdb_header_p->cd_entries = doc_header.cd_entries;
      pxdb_header_p->aa_entries = doc_header.aa_entries;
      pxdb_header_p->oi_entries = doc_header.oi_entries;
      pxdb_header_p->version = doc_header.version;
    }				/* PINFO section */

  else
    {
#ifdef DUMPING
      if (dumping)
	printf ("==No debug info at all for %s.\n", objfile->name);
#endif

      return 0;

    }

  return 1;
}				/* End of hpread_get_header */
#endif /* QUICK_LOOK_UP */


/* Initialization for reading native HP C debug symbols from OBJFILE.

   Its only purpose in life is to set up the symbol reader's private
   per-objfile data structures, and read in the raw contents of the debug
   sections (attaching pointers to the debug info into the private data
   structures).

   Since BFD doesn't know how to read debug symbols in a format-independent
   way (and may never do so...), we have to do it ourselves.  Note we may
   be called on a file without native HP C debugging symbols.

   FIXME, there should be a cleaner peephole into the BFD environment
   here. */
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
     of LNTT symbols.

     FIXME: this could be moved into the psymtab-to-symtab expansion
     code, and save startup time.  At the moment this data is
     still used, though.  We'd need a way to tell hp-symtab-read.c
     whether or not to load the LNTT. */
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

   Note that the minimal table is produced by the linker, and has
   only global routines in it; the psymtab is based on compiler-
   generated debug information and has non-global
   routines in it as well as files and class information.

   We assume hpread_symfile_init has been called to initialize the
   symbol reader's private data structures.

   SECTION_OFFSETS contains offsets relative to which the symbols in the
   various sections are (depending where the sections were actually loaded).
   MAINLINE is true if we are reading the main symbol table (as
   opposed to a shared lib or dynamically loaded file). */
void
hpread_build_psymtabs (objfile, section_offsets, mainline)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     int mainline;
{

#ifdef DUMPING
  /* Turn this on to get debugging output. */
  static int dumping = 0;
#endif

  char *namestring;
  int past_first_source_file = 0;
  struct cleanup *old_chain;

  int hp_symnum, symcount, i;
  int scan_start = 0;

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
  free_pending_blocks ();
  make_cleanup ((make_cleanup_func) really_free_pendings, 0);

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

  old_chain = make_cleanup ((make_cleanup_func) free_objfile, objfile);

  last_source_file = 0;

#ifdef QUICK_LOOK_UP
  {
    /* Begin code for new-style loading of quick look-up tables. */

    /* elz: this checks whether the file has beeen processed by pxdb.
       If not we would like to try to read the psymbols in
       anyway, but it turns out to be not so easy. So this could 
       actually be commented out, but I leave it in, just in case
       we decide to add support for non-pxdb-ed stuff in the future. */
    PXDB_header pxdb_header;
    int found_modules_in_program;

    if (hpread_get_header (objfile, &pxdb_header))
      {
	/* Build a minimal table.  No types, no global variables,
	   no include files.... */
#ifdef DUMPING
	if (dumping)
	  printf ("\nNew method for %s\n", objfile->name);
#endif

	/* elz: quick_traverse returns true if it found
	   some modules in the main source file, other
	   than those in end.c
	   In C and C++, all the files have MODULES entries
	   in the LNTT, and the quick table traverse is all 
	   based on finding these MODULES entries. Without 
	   those it cannot work. 
	   It happens that F77 programs don't have MODULES
	   so the quick traverse gets confused. F90 programs
	   have modules, and the quick method still works.
	   So, if modules (other than those in end.c) are
	   not found we give up on the quick table stuff, 
	   and fall back on the slower method  */
	found_modules_in_program = hpread_quick_traverse (objfile,
							  section_offsets,
							  GNTT (objfile),
							  VT (objfile),
							  &pxdb_header);

	discard_cleanups (old_chain);

		/* Set up to scan the global section of the LNTT.

		   This field is not always correct: if there are
		   no globals, it will point to the last record in
		   the regular LNTT, which is usually an END MODULE.

		   Since it might happen that there could be a file
		   with just one global record, there's no way to
		   tell other than by looking at the record, so that's
		   done below. */
	if (found_modules_in_program)
	  scan_start = pxdb_header.globals;
      }
#ifdef DUMPING
    else
      {
	if (dumping)
	  printf ("\nGoing on to old method for %s\n", objfile->name);
      }
#endif
  }
#endif /* QUICK_LOOK_UP */

    /* Make two passes, one over the GNTT symbols, the other for the
       LNTT symbols.

     JB comment: above isn't true--they only make one pass, over
     the LNTT.  */
  for (i = 0; i < 1; i++)
    {
      int within_function = 0;

      if (i)
	symcount = GNTT_SYMCOUNT (objfile);
      else
	symcount = LNTT_SYMCOUNT (objfile);


      for (hp_symnum = scan_start; hp_symnum < symcount; hp_symnum++)
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
#ifdef QUICK_LOOK_UP
		if (scan_start == hp_symnum
		    && symcount == hp_symnum + 1)
		  {
		    /* If there are NO globals in an executable,
		       PXDB's index to the globals will point to
		       the last record in the file, which 
		       could be this record. (this happened for F77 libraries)
		       ignore it and be done! */
		    continue;
		  }
#endif /* QUICK_LOOK_UP */

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

		valu = hpread_get_textlow (i, hp_symnum, objfile, symcount);
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

	      /* First end the previous psymtab */
	      if (pst)
		{
		  hpread_end_psymtab (pst, psymtab_include_list, includes_used,
				      ((hp_symnum - 1)
				       * sizeof (struct dntt_type_block)),
				      texthigh,
				      dependency_list, dependencies_used);
		  pst = (struct partial_symtab *) 0;
		  includes_used = 0;
		  dependencies_used = 0;
		  have_name = 0;
		}

	      /* Now begin a new module and a new psymtab for it */
	      SET_NAMESTRING (dn_bufp, &namestring, objfile);
	      valu = hpread_get_textlow (i, hp_symnum, objfile, symcount);
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
	      if (dn_bufp->dfunc.global)
		add_psymbol_to_list (namestring, strlen (namestring),
				     VAR_NAMESPACE, LOC_BLOCK,
				     &objfile->global_psymbols, valu,
				     0, language_unknown, objfile);
	      else
		add_psymbol_to_list (namestring, strlen (namestring),
				     VAR_NAMESPACE, LOC_BLOCK,
				     &objfile->static_psymbols, valu,
				     0, language_unknown, objfile);
	      within_function = 1;
	      continue;

	    case DNTT_TYPE_DOC_FUNCTION:
	      valu = dn_bufp->ddocfunc.hiaddr + ANOFFSET (section_offsets,
							  SECT_OFF_TEXT);
	      if (valu > texthigh)
		texthigh = valu;
	      valu = dn_bufp->ddocfunc.lowaddr +
		ANOFFSET (section_offsets, SECT_OFF_TEXT);
	      SET_NAMESTRING (dn_bufp, &namestring, objfile);
	      if (dn_bufp->ddocfunc.global)
		add_psymbol_to_list (namestring, strlen (namestring),
				     VAR_NAMESPACE, LOC_BLOCK,
				     &objfile->global_psymbols, valu,
				     0, language_unknown, objfile);
	      else
		add_psymbol_to_list (namestring, strlen (namestring),
				     VAR_NAMESPACE, LOC_BLOCK,
				     &objfile->static_psymbols, valu,
				     0, language_unknown, objfile);
	      within_function = 1;
	      continue;

	    case DNTT_TYPE_BEGIN:
	    case DNTT_TYPE_END:
	      /* We don't check MODULE end here, because there can be
	         symbols beyond the module end which properly belong to the
	         current psymtab -- so we wait till the next MODULE start */


#ifdef QUICK_LOOK_UP
	      if (scan_start == hp_symnum
		  && symcount == hp_symnum + 1)
		{
		  /* If there are NO globals in an executable,
		     PXDB's index to the globals will point to
		     the last record in the file, which is
		     probably an END MODULE, i.e. this record.
		     ignore it and be done! */
		  continue;
		}
#endif /* QUICK_LOOK_UP */

	      /* Scope block begin/end.  We only care about function
	         and file blocks right now.  */

	      if ((dn_bufp->dend.endkind == DNTT_TYPE_FUNCTION) ||
		  (dn_bufp->dend.endkind == DNTT_TYPE_DOC_FUNCTION))
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

		/* Compute address of the data symbol */
		valu = dn_bufp->dsvar.location;
		/* Relocate in case it's in a shared library */
		if (storage == LOC_STATIC)
		  valu += ANOFFSET (section_offsets, SECT_OFF_DATA);

		/* Luckily, dvar, svar, typedef, and tagdef all
		   have their "global" bit in the same place, so it works
		   (though it's bad programming practice) to reference
		   "dsvar.global" even though we may be looking at
		   any of the above four types. */
		if (dn_bufp->dsvar.global)
		  {
		    add_psymbol_to_list (namestring, strlen (namestring),
					 namespace, storage,
					 &objfile->global_psymbols,
					 valu,
					 0, language_unknown, objfile);
		  }
		else
		  {
		    add_psymbol_to_list (namestring, strlen (namestring),
					 namespace, storage,
					 &objfile->static_psymbols,
					 valu,
					 0, language_unknown, objfile);
		  }

		/* For TAGDEF's, the above code added the tagname to the
		   struct namespace. This will cause tag "t" to be found
		   on a reference of the form "(struct t) x". But for
		   C++ classes, "t" will also be a typename, which we
		   want to find on a reference of the form "ptype t".
		   Therefore, we also add "t" to the var namespace.
		   Do the same for enum's due to the way aCC generates
		   debug info for these (see more extended comment
		   in hp-symtab-read.c).
		   We do the same for templates, so that "ptype t"
		   where "t" is a template also works. */
		if (dn_bufp->dblock.kind == DNTT_TYPE_TAGDEF &&
		  dn_bufp->dtype.type.dnttp.index < LNTT_SYMCOUNT (objfile))
		  {
		    int global = dn_bufp->dtag.global;
		    /* Look ahead to see if it's a C++ class */
		    dn_bufp = hpread_get_lntt (dn_bufp->dtype.type.dnttp.index, objfile);
		    if (dn_bufp->dblock.kind == DNTT_TYPE_CLASS ||
			dn_bufp->dblock.kind == DNTT_TYPE_ENUM ||
			dn_bufp->dblock.kind == DNTT_TYPE_TEMPLATE)
		      {
			if (global)
			  {
			    add_psymbol_to_list (namestring, strlen (namestring),
						 VAR_NAMESPACE, storage,
						 &objfile->global_psymbols,
						 dn_bufp->dsvar.location,
						 0, language_unknown, objfile);
			  }
			else
			  {
			    add_psymbol_to_list (namestring, strlen (namestring),
						 VAR_NAMESPACE, storage,
						 &objfile->static_psymbols,
						 dn_bufp->dsvar.location,
						 0, language_unknown, objfile);
			  }
		      }
		  }
	      }
	      continue;

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
	      if (dn_bufp->dconst.global)
		add_psymbol_to_list (namestring, strlen (namestring),
				     VAR_NAMESPACE, LOC_CONST,
				     &objfile->global_psymbols, 0,
				     0, language_unknown, objfile);
	      else
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

  /* End any pending partial symbol table. */
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

union dnttentry *
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

union sltentry *
hpread_get_slt (index, objfile)
     int index;
     struct objfile *objfile;
{
  return (union sltentry *) &(SLT (objfile)[index * sizeof (union sltentry)]);
}

/* Get the low address associated with some symbol (typically the start
   of a particular source file or module).  Since that information is not
   stored as part of the DNTT_TYPE_MODULE or DNTT_TYPE_SRCFILE symbol we must infer it from
   the existance of DNTT_TYPE_FUNCTION symbols.  */

static unsigned long
hpread_get_textlow (global, index, objfile, symcount)
     int global;
     int index;
     struct objfile *objfile;
     int symcount;
{
  union dnttentry *dn_bufp;
  struct minimal_symbol *msymbol;

  /* Look for a DNTT_TYPE_FUNCTION symbol.  */
  if (index < symcount)		/* symcount is the number of symbols in */
    {				/*   the dbinfo, LNTT table */
      do
	{
	  if (global)
	    dn_bufp = hpread_get_gntt (index++, objfile);
	  else
	    dn_bufp = hpread_get_lntt (index++, objfile);
	}
      while (dn_bufp->dblock.kind != DNTT_TYPE_FUNCTION
	     && dn_bufp->dblock.kind != DNTT_TYPE_DOC_FUNCTION
	     && dn_bufp->dblock.kind != DNTT_TYPE_END
	     && index < symcount);
    }

  /* Avoid going past a DNTT_TYPE_END when looking for a DNTT_TYPE_FUNCTION.  This
     might happen when a sourcefile has no functions.  */
  if (dn_bufp->dblock.kind == DNTT_TYPE_END)
    return 0;

  /* Avoid going past the end of the LNTT file */
  if (index == symcount)
    return 0;

  /* The minimal symbols are typically more accurate for some reason.  */
  if (dn_bufp->dblock.kind == DNTT_TYPE_FUNCTION)
    msymbol = lookup_minimal_symbol (dn_bufp->dfunc.name + VT (objfile), NULL,
				     objfile);
  else				/* must be a DNTT_TYPE_DOC_FUNCTION */
    msymbol = lookup_minimal_symbol (dn_bufp->ddocfunc.name + VT (objfile), NULL,
				     objfile);

  if (msymbol)
    return SYMBOL_VALUE_ADDRESS (msymbol);
  else
    return dn_bufp->dfunc.lowaddr;
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
  int offset = ANOFFSET (section_offsets, SECT_OFF_TEXT);
  extern void hpread_psymtab_to_symtab ();
  struct partial_symtab *result =
  start_psymtab_common (objfile, section_offsets,
			filename, textlow, global_syms, static_syms);

  result->textlow += offset;
  result->read_symtab_private = (char *)
    obstack_alloc (&objfile->psymbol_obstack, sizeof (struct symloc));
  LDSYMOFF (result) = ldsymoff;
  result->read_symtab = hpread_psymtab_to_symtab;

  return result;
}


/* Close off the current usage of PST.  
   Returns PST or NULL if the partial symtab was empty and thrown away.

   capping_symbol_offset  --Byte index in LNTT or GNTT of the
   last symbol processed during the build
   of the previous pst.

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
  struct objfile *objfile = pst->objfile;
  int offset = ANOFFSET (pst->section_offsets, SECT_OFF_TEXT);

#ifdef DUMPING
  /* Turn on to see what kind of a psymtab we've built. */
  static int dumping = 0;
#endif

  if (capping_symbol_offset != -1)
    LDSYMLEN (pst) = capping_symbol_offset - LDSYMOFF (pst);
  else
    LDSYMLEN (pst) = 0;
  pst->texthigh = capping_text + offset;

  pst->n_global_syms =
    objfile->global_psymbols.next - (objfile->global_psymbols.list + pst->globals_offset);
  pst->n_static_syms =
    objfile->static_psymbols.next - (objfile->static_psymbols.list + pst->statics_offset);

#ifdef DUMPING
  if (dumping)
    {
      printf ("\nPst %s, LDSYMOFF %x (%x), LDSYMLEN %x (%x), globals %d, statics %d\n",
	      pst->filename,
	      LDSYMOFF (pst),
	      LDSYMOFF (pst) / sizeof (struct dntt_type_block),
	      LDSYMLEN (pst),
	      LDSYMLEN (pst) / sizeof (struct dntt_type_block),
	      pst->n_global_syms, pst->n_static_syms);
    }
#endif

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
      LDSYMOFF (subpst) =
	LDSYMLEN (subpst) =
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
         it is on the obstack, but we can forget to chain it on the list. 
         Empty psymtabs happen as a result of header files which don't have
         any symbols in them.  There can be a lot of them.  But this check
         is wrong, in that a psymtab with N_SLINE entries but nothing else
         is not empty, but we don't realize that.  Fixing that without slowing
         things down might be tricky.
         It's also wrong if we're using the quick look-up tables, as
         we can get empty psymtabs from modules with no routines in
         them. */

      discard_psymtab (pst);

      /* Indicate that psymtab was thrown away.  */
      pst = (struct partial_symtab *) NULL;

    }
  return pst;
}


/* End of hp-psymtab-read.c */

/* Set indentation to 4 spaces for Emacs; this file is
   mostly non-GNU-ish in its style :-( */
#if 0
***Local Variables:
***c - basic - offset:4
*** End:
#endif


