/* Read os9/os9k symbol tables and convert to internal format, for GDB.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1996
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This module provides three functions: os9k_symfile_init,
   which initializes to read a symbol file; os9k_new_init, which 
   discards existing cached information when all symbols are being
   discarded; and os9k_symfile_read, which reads a symbol table
   from a file.

   os9k_symfile_read only does the minimum work necessary for letting the
   user "name" things symbolically; it does not read the entire symtab.
   Instead, it reads the external and static symbols and puts them in partial
   symbol tables.  When more extensive information is requested of a
   file, the corresponding partial symbol table is mutated into a full
   fledged symbol table by going back and reading the symbols
   for real.  os9k_psymtab_to_symtab() is the function that does this */

#include "defs.h"
#include "gdb_string.h"
#include <stdio.h>

#if defined(USG) || defined(__CYGNUSCLIB__)
#include <sys/types.h>
#include <fcntl.h>
#endif

#include "obstack.h"
#include <sys/param.h>
#ifndef	NO_SYS_FILE
#include <sys/file.h>
#endif
#include "gdb_stat.h"
#include <ctype.h>
#include "symtab.h"
#include "breakpoint.h"
#include "command.h"
#include "target.h"
#include "gdbcore.h"		/* for bfd stuff */
#include "libaout.h"	 	/* FIXME Secret internal BFD stuff for a.out */
#include "symfile.h"
#include "objfiles.h"
#include "buildsym.h"
#include "gdb-stabs.h"
#include "demangle.h"
#include "language.h"		/* Needed inside partial-stab.h */
#include "complaints.h"
#include "os9k.h"
#include "stabsread.h"

/* Each partial symbol table entry contains a pointer to private data for the
   read_symtab() function to use when expanding a partial symbol table entry
   to a full symbol table entry.

   For dbxread this structure contains the offset within the file symbol table
   of first local symbol for this file, and count of the section
   of the symbol table devoted to this file's symbols (actually, the section
   bracketed may contain more than just this file's symbols).  It also contains
   further information needed to locate the symbols if they are in an ELF file.

   If ldsymcnt is 0, the only reason for this thing's existence is the
   dependency list.  Nothing else will happen when it is read in.  */

#define LDSYMOFF(p) (((struct symloc *)((p)->read_symtab_private))->ldsymoff)
#define LDSYMCNT(p) (((struct symloc *)((p)->read_symtab_private))->ldsymnum)

struct symloc {
  int ldsymoff;
  int ldsymnum;
};

/* Remember what we deduced to be the source language of this psymtab. */
static enum language psymtab_language = language_unknown;

/* keep partial symbol table file nested depth */
static int psymfile_depth = 0;

/* keep symbol table file nested depth */
static int symfile_depth = 0;

/* Nonzero means give verbose info on gdb action.  From main.c.  */
extern int info_verbose;

extern int previous_stab_code;

/* Name of last function encountered.  Used in Solaris to approximate
   object file boundaries.  */
static char *last_function_name;

/* Complaints about the symbols we have encountered.  */
extern struct complaint lbrac_complaint;

extern struct complaint unknown_symtype_complaint;

extern struct complaint unknown_symchar_complaint;

extern struct complaint lbrac_rbrac_complaint;

extern struct complaint repeated_header_complaint;

extern struct complaint repeated_header_name_complaint;

#if 0
static struct complaint lbrac_unmatched_complaint =
  {"unmatched Increment Block Entry before symtab pos %d", 0, 0};

static struct complaint lbrac_mismatch_complaint =
  {"IBE/IDE symbol mismatch at symtab pos %d", 0, 0};
#endif

/* Local function prototypes */
static void
os9k_read_ofile_symtab PARAMS ((struct partial_symtab *));

static void
os9k_psymtab_to_symtab PARAMS ((struct partial_symtab *));

static void
os9k_psymtab_to_symtab_1 PARAMS ((struct partial_symtab *));

static void
read_os9k_psymtab PARAMS ((struct section_offsets *, struct objfile *,
                         CORE_ADDR, int));

static int
fill_sym PARAMS ((FILE *, bfd *));

static void
os9k_symfile_init PARAMS ((struct objfile *));

static void
os9k_new_init PARAMS ((struct objfile *));

static void
os9k_symfile_read PARAMS ((struct objfile *, struct section_offsets *, int));

static void
os9k_symfile_finish PARAMS ((struct objfile *));

static void
os9k_process_one_symbol PARAMS ((int, int, CORE_ADDR, char *, 
     struct section_offsets *, struct objfile *));

static struct partial_symtab *
os9k_start_psymtab PARAMS ((struct objfile *, struct section_offsets *, char *,
                       CORE_ADDR, int, int, struct partial_symbol **,
                       struct partial_symbol **));

static struct partial_symtab *
os9k_end_psymtab PARAMS ((struct partial_symtab *, char **, int, int, CORE_ADDR,
                     struct partial_symtab **, int));

static void
record_minimal_symbol PARAMS ((char *, CORE_ADDR, int, struct objfile *, 
	             struct section_offsets *));

#define HANDLE_RBRAC(val) \
  if ((val) > pst->texthigh) pst->texthigh = (val);

#define SWAP_STBHDR(hdrp, abfd) \
  { \
    (hdrp)->fmtno = bfd_get_16(abfd, (unsigned char *)&(hdrp)->fmtno); \
    (hdrp)->crc = bfd_get_32(abfd, (unsigned char *)&(hdrp)->crc); \
    (hdrp)->offset = bfd_get_32(abfd, (unsigned char *)&(hdrp)->offset); \
    (hdrp)->nsym = bfd_get_32(abfd, (unsigned char *)&(hdrp)->nsym); \
  }
#define SWAP_STBSYM(symp, abfd) \
  { \
    (symp)->value = bfd_get_32(abfd, (unsigned char *)&(symp)->value); \
    (symp)->type = bfd_get_16(abfd, (unsigned char *)&(symp)->type); \
    (symp)->stroff = bfd_get_32(abfd, (unsigned char *)&(symp)->stroff); \
  }
#define N_DATA 0
#define N_BSS 1
#define N_RDATA 2
#define N_IDATA 3
#define N_TEXT 4
#define N_ABS 6

static void
record_minimal_symbol (name, address, type, objfile, section_offsets)
     char *name;
     CORE_ADDR address;
     int type;
     struct objfile *objfile;
     struct section_offsets *section_offsets;
{
  enum minimal_symbol_type ms_type;

  switch (type)
    {
    case N_TEXT:
	  ms_type = mst_text;
	  address += ANOFFSET(section_offsets, SECT_OFF_TEXT);
	  break;
    case N_DATA:
	  ms_type = mst_data;
	  break;
    case N_BSS:
          ms_type = mst_bss;
	  break;
    case N_RDATA:
	  ms_type = mst_bss;
	  break;
    case N_IDATA:	
	  ms_type = mst_data;
	  break;
    case N_ABS:
	  ms_type = mst_abs;
	  break;
    default:
          ms_type = mst_unknown; break;
  }

  prim_record_minimal_symbol
    (obsavestring (name, strlen(name), &objfile->symbol_obstack),
     address, ms_type, objfile);
}

/* read and process .stb file and store in minimal symbol table */
typedef char mhhdr[80];
struct stbhdr {
	mhhdr comhdr;
	char * name;
	short fmtno;
	int crc;
	int offset;
	int nsym;
	char *pad;
};
struct stbsymbol {
	int value;
	short type;
	int stroff;
};
#define STBSYMSIZE 10

static void
read_minimal_symbols(objfile, section_offsets)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
{
FILE *fp;
bfd *abfd;
struct stbhdr hdr;
struct stbsymbol sym;
int ch, i, j, off;
char buf[64], buf1[128];
		
  fp = objfile->auxf1;
  if (fp == NULL) return;
  abfd = objfile->obfd;
  fread(&hdr.comhdr[0], sizeof(mhhdr), 1, fp);
  i = 0;
  ch = getc(fp);
  while (ch != -1) {
    buf[i] = (char)ch;
    i++;
    if (ch == 0) break;
    ch = getc(fp);
  };
  if (i%2) ch=getc(fp);
  hdr.name = &buf[0];

  fread(&hdr.fmtno, sizeof(hdr.fmtno), 1, fp);
  fread(&hdr.crc, sizeof(hdr.crc), 1, fp);
  fread(&hdr.offset, sizeof(hdr.offset), 1, fp);
  fread(&hdr.nsym, sizeof(hdr.nsym), 1, fp);
  SWAP_STBHDR(&hdr, abfd);      
	
  /* read symbols */
  init_minimal_symbol_collection();
  off = hdr.offset;
  for (i = hdr.nsym; i > 0; i--) {
    fseek(fp, (long)off, 0);
    fread(&sym.value, sizeof(sym.value), 1, fp);
    fread(&sym.type, sizeof(sym.type), 1, fp);
    fread(&sym.stroff, sizeof(sym.stroff), 1, fp);
    SWAP_STBSYM (&sym, abfd);
    fseek(fp, (long)sym.stroff, 0);
    j = 0;
    ch = getc(fp);
    while (ch != -1) {
	buf1[j] = (char)ch;
	j++;
	if (ch == 0) break;
	ch = getc(fp);
    };
    record_minimal_symbol(buf1, sym.value, sym.type&7, objfile, section_offsets);
    off += STBSYMSIZE;
  };
  install_minimal_symbols (objfile);
  return;
}

/* Scan and build partial symbols for a symbol file.
   We have been initialized by a call to os9k_symfile_init, which 
   put all the relevant info into a "struct os9k_symfile_info",
   hung off the objfile structure.

   SECTION_OFFSETS contains offsets relative to which the symbols in the
   various sections are (depending where the sections were actually loaded).
   MAINLINE is true if we are reading the main symbol
   table (as opposed to a shared lib or dynamically loaded file).  */

static void
os9k_symfile_read (objfile, section_offsets, mainline)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     int mainline;	/* FIXME comments above */
{
  bfd *sym_bfd;
  struct cleanup *back_to;

  sym_bfd = objfile->obfd;
  /* If we are reinitializing, or if we have never loaded syms yet, init */
  if (mainline || objfile->global_psymbols.size == 0 || 
    objfile->static_psymbols.size == 0)
    init_psymbol_list (objfile, DBX_SYMCOUNT (objfile));

  pending_blocks = 0;
  back_to = make_cleanup (really_free_pendings, 0);

  make_cleanup (discard_minimal_symbols, 0);
  read_minimal_symbols (objfile, section_offsets);

  /* Now that the symbol table data of the executable file are all in core,
     process them and define symbols accordingly.  */
  read_os9k_psymtab (section_offsets, objfile,
		     DBX_TEXT_ADDR (objfile),
		     DBX_TEXT_SIZE (objfile));

  do_cleanups (back_to);
}

/* Initialize anything that needs initializing when a completely new
   symbol file is specified (not just adding some symbols from another
   file, e.g. a shared library).  */

static void
os9k_new_init (ignore)
     struct objfile *ignore;
{
  stabsread_new_init ();
  buildsym_new_init ();
  psymfile_depth = 0;
/*
  init_header_files ();
*/
}

/* os9k_symfile_init ()
   It is passed a struct objfile which contains, among other things,
   the BFD for the file whose symbols are being read, and a slot for a pointer
   to "private data" which we fill with goodies.

   Since BFD doesn't know how to read debug symbols in a format-independent
   way (and may never do so...), we have to do it ourselves.  We will never
   be called unless this is an a.out (or very similar) file. 
   FIXME, there should be a cleaner peephole into the BFD environment here.  */

static void
os9k_symfile_init (objfile)
     struct objfile *objfile;
{
  bfd *sym_bfd = objfile->obfd;
  char *name = bfd_get_filename (sym_bfd);
  char dbgname[512], stbname[512];
  FILE *symfile = 0;
  FILE *minfile = 0;
  asection *text_sect;

  strcpy(dbgname, name);
  strcat(dbgname, ".dbg");
  strcpy(stbname, name);
  strcat(stbname, ".stb");

  if ((symfile = fopen(dbgname, "r")) == NULL) {
    warning("Symbol file %s not found", dbgname);
  }
  objfile->auxf2 = symfile;

  if ((minfile = fopen(stbname, "r")) == NULL) {
    warning("Symbol file %s not found", stbname);
  }
  objfile->auxf1 = minfile;

  /* Allocate struct to keep track of the symfile */
  objfile->sym_stab_info = (PTR)
    xmmalloc (objfile -> md, sizeof (struct dbx_symfile_info));
  DBX_SYMFILE_INFO (objfile)->stab_section_info = NULL;

  text_sect = bfd_get_section_by_name (sym_bfd, ".text");
  if (!text_sect)
    error ("Can't find .text section in file");
  DBX_TEXT_ADDR (objfile) = bfd_section_vma (sym_bfd, text_sect);
  DBX_TEXT_SIZE (objfile) = bfd_section_size (sym_bfd, text_sect);

  DBX_SYMBOL_SIZE (objfile) = 0;     /* variable size symbol */
  DBX_SYMCOUNT (objfile) =  0;  /* used to be bfd_get_symcount(sym_bfd) */
  DBX_SYMTAB_OFFSET (objfile) = 0;  /* used to be SYMBOL_TABLE_OFFSET */
}

/* Perform any local cleanups required when we are done with a particular
   objfile.  I.E, we are in the process of discarding all symbol information
   for an objfile, freeing up all memory held for it, and unlinking the
   objfile struct from the global list of known objfiles. */

static void
os9k_symfile_finish (objfile)
     struct objfile *objfile;
{
  if (objfile->sym_stab_info != NULL)
    {
      mfree (objfile -> md, objfile->sym_stab_info);
    }
/*
  free_header_files ();
*/
}


struct st_dbghdr {
  int sync;
  short rev;
  int crc;
  short os;
  short cpu;
};
#define SYNC 		(int)0xefbefeca

#define SWAP_DBGHDR(hdrp, abfd) \
  { \
    (hdrp)->sync = bfd_get_32(abfd, (unsigned char *)&(hdrp)->sync); \
    (hdrp)->rev = bfd_get_16(abfd, (unsigned char *)&(hdrp)->rev); \
    (hdrp)->crc = bfd_get_32(abfd, (unsigned char *)&(hdrp)->crc); \
    (hdrp)->os = bfd_get_16(abfd, (unsigned char *)&(hdrp)->os); \
    (hdrp)->cpu = bfd_get_16(abfd, (unsigned char *)&(hdrp)->cpu); \
  }

#define N_SYM_CMPLR     0
#define N_SYM_SLINE     1
#define N_SYM_SYM       2
#define N_SYM_LBRAC     3
#define N_SYM_RBRAC     4
#define N_SYM_SE        5

struct internal_symstruct {
  short n_type;
  short n_desc;
  long n_value;
  char * n_strx;
};
static struct internal_symstruct symbol;
static struct internal_symstruct *symbuf = &symbol;
static char strbuf[4096];
static struct st_dbghdr dbghdr;
static short cmplrid;

#define VER_PRE_ULTRAC	((short)4)
#define VER_ULTRAC	((short)5)

static int
fill_sym (dbg_file, abfd)
     FILE *dbg_file;
     bfd *abfd;
{
short si, nmask;
long li;
int ii;
char *p;

  int nbytes = fread(&si, sizeof(si), 1, dbg_file);
  if (nbytes == 0)
    return 0;
  if (nbytes < 0)
    perror_with_name ("reading .dbg file.");
  symbuf->n_desc = 0;
  symbuf->n_value = 0;
  symbuf->n_strx = NULL;
  symbuf->n_type = bfd_get_16 (abfd, (unsigned char *)&si);
  symbuf->n_type = 0xf & symbuf->n_type;
  switch (symbuf->n_type)
    {
    case N_SYM_CMPLR:
      fread(&si, sizeof(si), 1, dbg_file);
      symbuf->n_desc = bfd_get_16(abfd, (unsigned char *)&si);
      cmplrid = symbuf->n_desc & 0xff;
      break;
    case N_SYM_SLINE:
      fread(&li, sizeof(li), 1, dbg_file);
      symbuf->n_value = bfd_get_32(abfd, (unsigned char *)&li);
      fread(&li, sizeof(li), 1, dbg_file);
      li = bfd_get_32(abfd, (unsigned char *)&li);
      symbuf->n_strx = (char *)(li >> 12);
      symbuf->n_desc = li & 0xfff;
      break;
    case N_SYM_SYM:
      fread(&li, sizeof(li), 1, dbg_file);
      symbuf->n_value = bfd_get_32(abfd, (unsigned char *)&li);
      si = 0;
      do {
	ii = getc(dbg_file);
	strbuf[si++] = (char) ii;
      } while (ii != 0 || si % 2 != 0);
      symbuf->n_strx = strbuf;
      p = (char *) strchr (strbuf, ':');
      if (!p) break;
      if ((p[1] == 'F' || p[1] == 'f') && cmplrid == VER_PRE_ULTRAC)
	{
	  fread(&si, sizeof(si), 1, dbg_file);
	  nmask = bfd_get_16(abfd, (unsigned char *)&si);
	  for (ii=0; ii<nmask; ii++)
	    fread(&si, sizeof(si), 1, dbg_file);
	}
      break;
    case N_SYM_LBRAC:
      fread(&li, sizeof(li), 1, dbg_file);
      symbuf->n_value = bfd_get_32(abfd, (unsigned char *)&li);
      break;
    case N_SYM_RBRAC:
      fread(&li, sizeof(li), 1, dbg_file);
      symbuf->n_value = bfd_get_32(abfd, (unsigned char *)&li);
      break;
    case N_SYM_SE:
      break;
    }
    return 1;
}

/* Given pointers to an a.out symbol table in core containing dbx
   style data, setup partial_symtab's describing each source file for
   which debugging information is available.
   SYMFILE_NAME is the name of the file we are reading from
   and SECTION_OFFSETS is the set of offsets for the various sections
   of the file (a set of zeros if the mainline program).  */

static void
read_os9k_psymtab (section_offsets, objfile, text_addr, text_size)
     struct section_offsets *section_offsets;
     struct objfile *objfile;
     CORE_ADDR text_addr;
     int text_size;
{
  register struct internal_symstruct *bufp = 0;	/* =0 avoids gcc -Wall glitch*/
  register char *namestring;
  int past_first_source_file = 0;
  CORE_ADDR last_o_file_start = 0;
#if 0
  struct cleanup *back_to;
#endif
  bfd *abfd;
  FILE *fp;

  /* End of the text segment of the executable file.  */
  static CORE_ADDR end_of_text_addr;

  /* Current partial symtab */
  static struct partial_symtab *pst = 0;

  /* List of current psymtab's include files */
  char **psymtab_include_list;
  int includes_allocated;
  int includes_used;

  /* Index within current psymtab dependency list */
  struct partial_symtab **dependency_list;
  int dependencies_used, dependencies_allocated;

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

#ifdef END_OF_TEXT_DEFAULT
  end_of_text_addr = END_OF_TEXT_DEFAULT;
#else
  end_of_text_addr = text_addr + section_offsets->offsets[SECT_OFF_TEXT]
			       + text_size;	/* Relocate */
#endif

  abfd = objfile->obfd;
  fp = objfile->auxf2; 
  if (!fp) return;
		
  fread(&dbghdr.sync, sizeof(dbghdr.sync), 1, fp);
  fread(&dbghdr.rev, sizeof(dbghdr.rev), 1, fp);
  fread(&dbghdr.crc, sizeof(dbghdr.crc), 1, fp);
  fread(&dbghdr.os, sizeof(dbghdr.os), 1, fp);
  fread(&dbghdr.cpu, sizeof(dbghdr.cpu), 1, fp);
  SWAP_DBGHDR(&dbghdr, abfd);      

  symnum = 0;
  while(1)
    {
    int ret;
    long cursymoffset;

      /* Get the symbol for this run and pull out some info */
      QUIT;	/* allow this to be interruptable */
      cursymoffset = ftell(objfile->auxf2);
      ret = fill_sym(objfile->auxf2, abfd);
      if (ret <= 0) break;
      else symnum++;
      bufp = symbuf;

      /* Special case to speed up readin. */
      if (bufp->n_type == (short)N_SYM_SLINE) continue;

#define CUR_SYMBOL_VALUE bufp->n_value
      /* partial-stab.h */

      switch (bufp->n_type)
	{
	char *p;

	case N_SYM_CMPLR:
	  continue;

	case N_SYM_SE:
          CUR_SYMBOL_VALUE += ANOFFSET(section_offsets, SECT_OFF_TEXT);
	  if (psymfile_depth == 1 && pst)
	    {
	      os9k_end_psymtab (pst, psymtab_include_list, includes_used,
		       symnum, CUR_SYMBOL_VALUE,
		       dependency_list, dependencies_used);
	      pst = (struct partial_symtab *) 0;
	      includes_used = 0;
	      dependencies_used = 0;
	    }
  	  psymfile_depth--;
	  continue;

	case N_SYM_SYM:		/* Typedef or automatic variable. */
	  namestring = bufp->n_strx;
          p = (char *) strchr (namestring, ':');
          if (!p)
            continue;           /* Not a debugging symbol.   */

	  /* Main processing section for debugging symbols which
	     the initial read through the symbol tables needs to worry
	     about.  If we reach this point, the symbol which we are
	     considering is definitely one we are interested in.
	     p must also contain the (valid) index into the namestring
	     which indicates the debugging type symbol.  */

	  switch (p[1])
	    {
	    case 'S' :
	      {
		unsigned long valu;
            	enum language tmp_language;
		char *str, *p;
		int n;
		
		valu = CUR_SYMBOL_VALUE;
		if (valu)
		  valu += ANOFFSET (section_offsets, SECT_OFF_TEXT);
		past_first_source_file = 1;

                p = strchr(namestring, ':');
                if (p) n = p-namestring;
                else n = strlen(namestring);
                str = alloca(n+1);
                strncpy(str, namestring, n);
                str[n] = '\0';

		if (psymfile_depth == 0) {
		  if (!pst)
		    pst = os9k_start_psymtab (objfile, section_offsets,
				 str, valu,
				 cursymoffset,
				 symnum-1,
				 objfile -> global_psymbols.next,
				 objfile -> static_psymbols.next);
		} else { /* this is a include file */
            	  tmp_language = deduce_language_from_filename (str);
            	  if (tmp_language != language_unknown
                	&& (tmp_language != language_c
                    	|| psymtab_language != language_cplus))
              		psymtab_language = tmp_language;

/*
		  if (pst && STREQ (str, pst->filename))
		    continue;
		  {
		    register int i;
		    for (i = 0; i < includes_used; i++)
		      if (STREQ (str, psymtab_include_list[i]))
			{
			  i = -1; 
			  break;
			}
		    if (i == -1)
		      continue;
		  }
*/

                  psymtab_include_list[includes_used++] = str;
                  if (includes_used >= includes_allocated)
                    {
                      char **orig = psymtab_include_list;

                      psymtab_include_list = (char **)
                	  alloca ((includes_allocated *= 2) * sizeof (char *));
                      memcpy ((PTR)psymtab_include_list, (PTR)orig,
                          includes_used * sizeof (char *));
              	    }

		}
  		psymfile_depth++;
		continue;
	      }

	    case 'v':
	      add_psymbol_to_list (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_STATIC,
				   &objfile->static_psymbols,
				   0, CUR_SYMBOL_VALUE,
				   psymtab_language, objfile);
	      continue;
	    case 'V':
	      add_psymbol_to_list (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_STATIC,
				   &objfile->global_psymbols,
				   0, CUR_SYMBOL_VALUE,
				   psymtab_language, objfile);
	      continue;

	    case 'T':
	      if (p != namestring)	/* a name is there, not just :T... */
		{
		  add_psymbol_to_list (namestring, p - namestring,
				       STRUCT_NAMESPACE, LOC_TYPEDEF,
				       &objfile->static_psymbols,
				       CUR_SYMBOL_VALUE, 0,
				       psymtab_language, objfile);
		  if (p[2] == 't')
		    {
		      /* Also a typedef with the same name.  */
		      add_psymbol_to_list (namestring, p - namestring,
					   VAR_NAMESPACE, LOC_TYPEDEF,
					   &objfile->static_psymbols,
					   CUR_SYMBOL_VALUE, 0, psymtab_language,
					   objfile);
		      p += 1;
		    }
		  /* The semantics of C++ state that "struct foo { ... }"
		     also defines a typedef for "foo".  Unfortuantely, cfront
		     never makes the typedef when translating from C++ to C.
		     We make the typedef here so that "ptype foo" works as
		     expected for cfront translated code.  */
		  else if (psymtab_language == language_cplus)
		   {
		      /* Also a typedef with the same name.  */
		      add_psymbol_to_list (namestring, p - namestring,
					   VAR_NAMESPACE, LOC_TYPEDEF,
					   &objfile->static_psymbols,
					   CUR_SYMBOL_VALUE, 0, psymtab_language,
					   objfile);
		   }
		}
	      goto check_enum;
	    case 't':
	      if (p != namestring)	/* a name is there, not just :T... */
		{
		  add_psymbol_to_list (namestring, p - namestring,
				       VAR_NAMESPACE, LOC_TYPEDEF,
				       &objfile->static_psymbols,
				       CUR_SYMBOL_VALUE, 0,
				       psymtab_language, objfile);
		}
	    check_enum:
	      /* If this is an enumerated type, we need to
		 add all the enum constants to the partial symbol
		 table.  This does not cover enums without names, e.g.
		 "enum {a, b} c;" in C, but fortunately those are
		 rare.  There is no way for GDB to find those from the
		 enum type without spending too much time on it.  Thus
		 to solve this problem, the compiler needs to put out the
		 enum in a nameless type.  GCC2 does this.  */

	      /* We are looking for something of the form
		 <name> ":" ("t" | "T") [<number> "="] "e" <size>
		 {<constant> ":" <value> ","} ";".  */

	      /* Skip over the colon and the 't' or 'T'.  */
	      p += 2;
	      /* This type may be given a number.  Also, numbers can come
		 in pairs like (0,26).  Skip over it.  */
	      while ((*p >= '0' && *p <= '9')
		     || *p == '(' || *p == ',' || *p == ')'
		     || *p == '=')
		p++;

	      if (*p++ == 'e')
		{
		  /* We have found an enumerated type. skip size */
	          while (*p >= '0' && *p <= '9') p++;
		  /* According to comments in read_enum_type
		     a comma could end it instead of a semicolon.
		     I don't know where that happens.
		     Accept either.  */
		  while (*p && *p != ';' && *p != ',')
		    {
		      char *q;

		      /* Check for and handle cretinous dbx symbol name
			 continuation! 
		      if (*p == '\\')
			p = next_symbol_text (objfile);
		      */

		      /* Point to the character after the name
			 of the enum constant.  */
		      for (q = p; *q && *q != ':'; q++)
			;
		      /* Note that the value doesn't matter for
			 enum constants in psymtabs, just in symtabs.  */
		      add_psymbol_to_list (p, q - p,
					   VAR_NAMESPACE, LOC_CONST,
					   &objfile->static_psymbols, 0,
					   0, psymtab_language, objfile);
		      /* Point past the name.  */
		      p = q;
		      /* Skip over the value.  */
		      while (*p && *p != ',')
			p++;
		      /* Advance past the comma.  */
		      if (*p)
			p++;
		    }
		}
	      continue;
	    case 'c':
	      /* Constant, e.g. from "const" in Pascal.  */
	      add_psymbol_to_list (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_CONST,
				   &objfile->static_psymbols, CUR_SYMBOL_VALUE,
				   0, psymtab_language, objfile);
	      continue;

	    case 'f':
	      CUR_SYMBOL_VALUE += ANOFFSET(section_offsets, SECT_OFF_TEXT);
              if (pst && pst->textlow == 0)
                pst->textlow = CUR_SYMBOL_VALUE;

	      add_psymbol_to_list (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_BLOCK,
				   &objfile->static_psymbols, CUR_SYMBOL_VALUE,
				   0, psymtab_language, objfile);
	      continue;

	    case 'F':
	      CUR_SYMBOL_VALUE += ANOFFSET(section_offsets, SECT_OFF_TEXT);
              if (pst && pst->textlow == 0)
                pst->textlow = CUR_SYMBOL_VALUE;

	      add_psymbol_to_list (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_BLOCK,
				   &objfile->global_psymbols, CUR_SYMBOL_VALUE,
				   0, psymtab_language, objfile);
	      continue;

	    case 'p':
	    case 'l':
            case 's':
	      continue;

	    case ':':
	      /* It is a C++ nested symbol.  We don't need to record it
		 (I don't think); if we try to look up foo::bar::baz,
		 then symbols for the symtab containing foo should get
		 read in, I think.  */
	      /* Someone says sun cc puts out symbols like
		 /foo/baz/maclib::/usr/local/bin/maclib,
		 which would get here with a symbol type of ':'.  */
	      continue;

	    default:
	      /* Unexpected symbol descriptor.  The second and subsequent stabs
		 of a continued stab can show up here.  The question is
		 whether they ever can mimic a normal stab--it would be
		 nice if not, since we certainly don't want to spend the
		 time searching to the end of every string looking for
		 a backslash.  */

	      complain (&unknown_symchar_complaint, p[1]);
	      continue;
	    }

	case N_SYM_RBRAC:
	  CUR_SYMBOL_VALUE += ANOFFSET(section_offsets, SECT_OFF_TEXT);
#ifdef HANDLE_RBRAC
	  HANDLE_RBRAC(CUR_SYMBOL_VALUE);
	  continue;
#endif
	case N_SYM_LBRAC:
	  continue;

	default:
	  /* If we haven't found it yet, ignore it.  It's probably some
	     new type we don't know about yet.  */
	  complain (&unknown_symtype_complaint,
		    local_hex_string ((unsigned long) bufp->n_type));
	  continue;
	}
    }

  DBX_SYMCOUNT (objfile) = symnum;

  /* If there's stuff to be cleaned up, clean it up.  */
  if (DBX_SYMCOUNT (objfile) > 0
/*FIXME, does this have a bug at start address 0? */
      && last_o_file_start
      && objfile -> ei.entry_point < bufp->n_value
      && objfile -> ei.entry_point >= last_o_file_start)
    {
      objfile -> ei.entry_file_lowpc = last_o_file_start;
      objfile -> ei.entry_file_highpc = bufp->n_value;
    }

  if (pst)
    {
      os9k_end_psymtab (pst, psymtab_include_list, includes_used,
		   symnum, end_of_text_addr,
		   dependency_list, dependencies_used);
    }
/*
  do_cleanups (back_to);
*/
}

/* Allocate and partially fill a partial symtab.  It will be
   completely filled at the end of the symbol list.

   SYMFILE_NAME is the name of the symbol-file we are reading from, and ADDR
   is the address relative to which its symbols are (incremental) or 0
   (normal). */


static struct partial_symtab *
os9k_start_psymtab (objfile, section_offsets,
	       filename, textlow, ldsymoff,ldsymcnt,  global_syms, static_syms)
     struct objfile *objfile;
     struct section_offsets *section_offsets;
     char *filename;
     CORE_ADDR textlow;
     int ldsymoff;
     int ldsymcnt;
     struct partial_symbol **global_syms;
     struct partial_symbol **static_syms;
{
  struct partial_symtab *result =
      start_psymtab_common(objfile, section_offsets,
			   filename, textlow, global_syms, static_syms);

  result->read_symtab_private = (char *)
    obstack_alloc (&objfile -> psymbol_obstack, sizeof (struct symloc));

  LDSYMOFF(result) = ldsymoff;
  LDSYMCNT(result) = ldsymcnt;
  result->read_symtab = os9k_psymtab_to_symtab;

  /* Deduce the source language from the filename for this psymtab. */
  psymtab_language = deduce_language_from_filename (filename);
  return result;
}

/* Close off the current usage of PST.  
   Returns PST or NULL if the partial symtab was empty and thrown away.
   FIXME:  List variables and peculiarities of same.  */

static struct partial_symtab *
os9k_end_psymtab (pst, include_list, num_includes, capping_symbol_cnt,
	     capping_text, dependency_list, number_dependencies)
     struct partial_symtab *pst;
     char **include_list;
     int num_includes;
     int capping_symbol_cnt;
     CORE_ADDR capping_text;
     struct partial_symtab **dependency_list;
     int number_dependencies;
/*     struct partial_symbol *capping_global, *capping_static;*/
{
  int i;
  struct partial_symtab *p1;
  struct objfile *objfile = pst -> objfile;

  if (capping_symbol_cnt != -1)
      LDSYMCNT(pst) = capping_symbol_cnt - LDSYMCNT(pst);

  /* Under Solaris, the N_SO symbols always have a value of 0,
     instead of the usual address of the .o file.  Therefore,
     we have to do some tricks to fill in texthigh and textlow.
     The first trick is in partial-stab.h: if we see a static
     or global function, and the textlow for the current pst
     is still 0, then we use that function's address for 
     the textlow of the pst.

     Now, to fill in texthigh, we remember the last function seen
     in the .o file (also in partial-stab.h).  Also, there's a hack in
     bfd/elf.c and gdb/elfread.c to pass the ELF st_size field
     to here via the misc_info field.  Therefore, we can fill in
     a reliable texthigh by taking the address plus size of the
     last function in the file.

     Unfortunately, that does not cover the case where the last function
     in the file is static.  See the paragraph below for more comments
     on this situation.

     Finally, if we have a valid textlow for the current file, we run
     down the partial_symtab_list filling in previous texthighs that
     are still unknown.  */

  if (pst->texthigh == 0 && last_function_name) {
    char *p;
    int n;
    struct minimal_symbol *minsym;

    p = strchr (last_function_name, ':');
    if (p == NULL)
      p = last_function_name;
    n = p - last_function_name;
    p = alloca (n + 1);
    strncpy (p, last_function_name, n);
    p[n] = 0;
    
    minsym = lookup_minimal_symbol (p, NULL, objfile);

    if (minsym) {
      pst->texthigh = SYMBOL_VALUE_ADDRESS(minsym)+(long)MSYMBOL_INFO(minsym);
    } else {
      /* This file ends with a static function, and it's
	 difficult to imagine how hard it would be to track down
	 the elf symbol.  Luckily, most of the time no one will notice,
	 since the next file will likely be compiled with -g, so
	 the code below will copy the first fuction's start address 
	 back to our texthigh variable.  (Also, if this file is the
	 last one in a dynamically linked program, texthigh already
	 has the right value.)  If the next file isn't compiled
	 with -g, then the last function in this file winds up owning
	 all of the text space up to the next -g file, or the end (minus
	 shared libraries).  This only matters for single stepping,
	 and even then it will still work, except that it will single
	 step through all of the covered functions, instead of setting
	 breakpoints around them as it usualy does.  This makes it
	 pretty slow, but at least it doesn't fail.

	 We can fix this with a fairly big change to bfd, but we need
	 to coordinate better with Cygnus if we want to do that.  FIXME.  */
    }
    last_function_name = NULL;
  }

  /* this test will be true if the last .o file is only data */
  if (pst->textlow == 0)
    pst->textlow = pst->texthigh;

  /* If we know our own starting text address, then walk through all other
     psymtabs for this objfile, and if any didn't know their ending text
     address, set it to our starting address.  Take care to not set our
     own ending address to our starting address, nor to set addresses on
     `dependency' files that have both textlow and texthigh zero.  */
  if (pst->textlow) {
    ALL_OBJFILE_PSYMTABS (objfile, p1) {
      if (p1->texthigh == 0  && p1->textlow != 0 && p1 != pst) {
	p1->texthigh = pst->textlow;
	/* if this file has only data, then make textlow match texthigh */
	if (p1->textlow == 0)
	  p1->textlow = p1->texthigh;
      }
    }
  }

  /* End of kludge for patching Solaris textlow and texthigh.  */

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
	LDSYMCNT(subpst) =
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

  /* If there is already a psymtab or symtab for a file of this name, 
     remove it.
     (If there is a symtab, more drastic things also happen.)
     This happens in VxWorks.  */
  free_named_symtabs (pst->filename);

  if (num_includes == 0
   && number_dependencies == 0
   && pst->n_global_syms == 0
   && pst->n_static_syms == 0) {
    /* Throw away this psymtab, it's empty.  We can't deallocate it, since
       it is on the obstack, but we can forget to chain it on the list.  */
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

static void
os9k_psymtab_to_symtab_1 (pst)
     struct partial_symtab *pst;
{
  struct cleanup *old_chain;
  int i;
  
  if (!pst)
    return;

  if (pst->readin)
    {
      fprintf_unfiltered (gdb_stderr, "Psymtab for %s already read in.  Shouldn't happen.\n",
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
	os9k_psymtab_to_symtab_1 (pst->dependencies[i]);
      }

  if (LDSYMCNT(pst))		/* Otherwise it's a dummy */
    {
      /* Init stuff necessary for reading in symbols */
      stabsread_init ();
      buildsym_init ();
      old_chain = make_cleanup (really_free_pendings, 0);

      /* Read in this file's symbols */
      os9k_read_ofile_symtab (pst);
      sort_symtab_syms (pst->symtab);
      do_cleanups (old_chain);
    }

  pst->readin = 1;
}

/* Read in all of the symbols for a given psymtab for real.
   Be verbose about it if the user wants that.  */

static void
os9k_psymtab_to_symtab (pst)
     struct partial_symtab *pst;
{
  bfd *sym_bfd;

  if (!pst)
    return;

  if (pst->readin)
    {
      fprintf_unfiltered (gdb_stderr, "Psymtab for %s already read in.  Shouldn't happen.\n",
	       pst->filename);
      return;
    }

  if (LDSYMCNT(pst) || pst->number_of_dependencies)
    {
      /* Print the message now, before reading the string table,
	 to avoid disconcerting pauses.  */
      if (info_verbose)
	{
	  printf_filtered ("Reading in symbols for %s...", pst->filename);
	  gdb_flush (gdb_stdout);
	}

      sym_bfd = pst->objfile->obfd;
      os9k_psymtab_to_symtab_1 (pst);

      /* Match with global symbols.  This only needs to be done once,
         after all of the symtabs and dependencies have been read in.   */
      scan_file_globals (pst->objfile);

      /* Finish up the debug error message.  */
      if (info_verbose)
	printf_filtered ("done.\n");
    }
}

/* Read in a defined section of a specific object file's symbols. */
static void
os9k_read_ofile_symtab (pst)
     struct partial_symtab *pst;
{
  register struct internal_symstruct *bufp;
  unsigned char type;
  unsigned max_symnum;
  register bfd *abfd;
  struct objfile *objfile;
  int sym_offset;		/* Offset to start of symbols to read */
  CORE_ADDR text_offset;	/* Start of text segment for symbols */
  int text_size;		/* Size of text segment for symbols */
  struct section_offsets *section_offsets;
  FILE *dbg_file;

  objfile = pst->objfile;
  sym_offset = LDSYMOFF(pst);
  max_symnum = LDSYMCNT(pst);
  text_offset = pst->textlow;
  text_size = pst->texthigh - pst->textlow;
  section_offsets = pst->section_offsets;

  current_objfile = objfile;
  subfile_stack = NULL;
  last_source_file = NULL;

  abfd = objfile->obfd;
  dbg_file = objfile->auxf2;

#if 0
  /* It is necessary to actually read one symbol *before* the start
     of this symtab's symbols, because the GCC_COMPILED_FLAG_SYMBOL
     occurs before the N_SO symbol.
     Detecting this in read_dbx_symtab
     would slow down initial readin, so we look for it here instead. */
  if (!processing_acc_compilation && sym_offset >= (int)symbol_size)
    {
      fseek (objefile->auxf2, sym_offset, SEEK_CUR);
      fill_sym(objfile->auxf2, abfd);
      bufp = symbuf;

      processing_gcc_compilation = 0;
      if (bufp->n_type == N_TEXT)
	{
	  if (STREQ (namestring, GCC_COMPILED_FLAG_SYMBOL))
	    processing_gcc_compilation = 1;
	  else if (STREQ (namestring, GCC2_COMPILED_FLAG_SYMBOL))
	    processing_gcc_compilation = 2;
	}

      /* Try to select a C++ demangling based on the compilation unit
	 producer. */

      if (processing_gcc_compilation)
	{
	  if (AUTO_DEMANGLING)
	    {
	      set_demangling_style (GNU_DEMANGLING_STYLE_STRING);
	    }
	}
    }
  else
    {
      /* The N_SO starting this symtab is the first symbol, so we
	 better not check the symbol before it.  I'm not this can
	 happen, but it doesn't hurt to check for it.  */
      bfd_seek (symfile_bfd, sym_offset, SEEK_CUR);
      processing_gcc_compilation = 0;
    }
#endif /* 0 */

  fseek(dbg_file, (long)sym_offset, 0);
/*
  if (bufp->n_type != (unsigned char)N_SYM_SYM)
    error("First symbol in segment of executable not a source symbol");
*/

  for (symnum = 0; symnum < max_symnum; symnum++)
    {
      QUIT;			/* Allow this to be interruptable */
      fill_sym(dbg_file, abfd);
      bufp = symbuf;
      type = bufp->n_type;

      os9k_process_one_symbol ((int)type, (int)bufp->n_desc, 
      (CORE_ADDR)bufp->n_value, bufp->n_strx, section_offsets, objfile);

      /* We skip checking for a new .o or -l file; that should never
         happen in this routine. */
#if 0
      else if (type == N_TEXT)
	{
	  /* I don't think this code will ever be executed, because
	     the GCC_COMPILED_FLAG_SYMBOL usually is right before
	     the N_SO symbol which starts this source file.
	     However, there is no reason not to accept
	     the GCC_COMPILED_FLAG_SYMBOL anywhere.  */

	  if (STREQ (namestring, GCC_COMPILED_FLAG_SYMBOL))
	    processing_gcc_compilation = 1;
	  else if (STREQ (namestring, GCC2_COMPILED_FLAG_SYMBOL))
	    processing_gcc_compilation = 2;

	  if (AUTO_DEMANGLING)
	    {
	      set_demangling_style (GNU_DEMANGLING_STYLE_STRING);
	    }
	}
      else if (type & N_EXT || type == (unsigned char)N_TEXT
	       || type == (unsigned char)N_NBTEXT
	       ) {
	  /* Global symbol: see if we came across a dbx defintion for
	     a corresponding symbol.  If so, store the value.  Remove
	     syms from the chain when their values are stored, but
	     search the whole chain, as there may be several syms from
	     different files with the same name. */
	  /* This is probably not true.  Since the files will be read
	     in one at a time, each reference to a global symbol will
	     be satisfied in each file as it appears. So we skip this
	     section. */
	  ;
        }
#endif /* 0 */
    }

  current_objfile = NULL;

  /* In a Solaris elf file, this variable, which comes from the
     value of the N_SO symbol, will still be 0.  Luckily, text_offset,
     which comes from pst->textlow is correct. */
  if (last_source_start_addr == 0)
    last_source_start_addr = text_offset;
  pst->symtab = end_symtab (text_offset + text_size, objfile, SECT_OFF_TEXT);
  end_stabs ();
}


/* This handles a single symbol from the symbol-file, building symbols
   into a GDB symtab.  It takes these arguments and an implicit argument.

   TYPE is the type field of the ".stab" symbol entry.
   DESC is the desc field of the ".stab" entry.
   VALU is the value field of the ".stab" entry.
   NAME is the symbol name, in our address space.
   SECTION_OFFSETS is a set of amounts by which the sections of this object
          file were relocated when it was loaded into memory.
          All symbols that refer
	  to memory locations need to be offset by these amounts.
   OBJFILE is the object file from which we are reading symbols.
 	       It is used in end_symtab.  */

static void
os9k_process_one_symbol (type, desc, valu, name, section_offsets, objfile)
     int type, desc;
     CORE_ADDR valu;
     char *name;
     struct section_offsets *section_offsets;
     struct objfile *objfile;
{
  register struct context_stack *new;
  /* The stab type used for the definition of the last function.
     N_STSYM or N_GSYM for SunOS4 acc; N_FUN for other compilers.  */
  static int function_stab_type = 0;

#if 0
  /* Something is wrong if we see real data before
     seeing a source file name.  */
  if (last_source_file == NULL && type != (unsigned char)N_SO)
    {
      /* Ignore any symbols which appear before an N_SO symbol.  Currently
	 no one puts symbols there, but we should deal gracefully with the
	 case.  A complain()t might be in order (if !IGNORE_SYMBOL (type)),
	 but this should not be an error ().  */
      return;
    }
#endif /* 0 */

  switch (type)
    {
    case N_SYM_LBRAC:
      /* On most machines, the block addresses are relative to the
	 N_SO, the linker did not relocate them (sigh).  */
      valu += ANOFFSET (section_offsets, SECT_OFF_TEXT); 
      new = push_context (desc, valu);
      break;

    case N_SYM_RBRAC:
      valu += ANOFFSET (section_offsets, SECT_OFF_TEXT); 
      new = pop_context();

#if !defined (OS9K_VARIABLES_INSIDE_BLOCK)
#define OS9K_VARIABLES_INSIDE_BLOCK(desc, gcc_p) 1
#endif

      if (!OS9K_VARIABLES_INSIDE_BLOCK(desc, processing_gcc_compilation))
	local_symbols = new->locals;

      if (context_stack_depth > 1)
	{
	  /* This is not the outermost LBRAC...RBRAC pair in the function,
	     its local symbols preceded it, and are the ones just recovered
	     from the context stack.  Define the block for them (but don't
	     bother if the block contains no symbols.  Should we complain
	     on blocks without symbols?  I can't think of any useful purpose
	     for them).  */
	  if (local_symbols != NULL)
	    {
	      /* Muzzle a compiler bug that makes end < start.  (which
		 compilers?  Is this ever harmful?).  */
	      if (new->start_addr > valu)
		{
		  complain (&lbrac_rbrac_complaint);
		  new->start_addr = valu;
		}
	      /* Make a block for the local symbols within.  */
	      finish_block (0, &local_symbols, new->old_blocks,
			    new->start_addr, valu, objfile);
	    }
	}
      else
	{
	  if (context_stack_depth == 0)
	    {
	      within_function = 0;
	      /* Make a block for the local symbols within.  */
	      finish_block (new->name, &local_symbols, new->old_blocks,
			new->start_addr, valu, objfile);
	    }
	  else
	    {
	      /* attach local_symbols to the end of new->locals */
	      if (!new->locals) new->locals = local_symbols;
	      else {
		struct pending *p;

	        p = new->locals;
	        while (p->next) p = p->next; 
	        p->next = local_symbols;
	      }
	    }
	}

      if (OS9K_VARIABLES_INSIDE_BLOCK(desc, processing_gcc_compilation))
	/* Now pop locals of block just finished.  */
	local_symbols = new->locals;
      break;


    case N_SYM_SLINE:
      /* This type of "symbol" really just records
	 one line-number -- core-address correspondence.
	 Enter it in the line list for this symbol table. */
      /* Relocate for dynamic loading and for ELF acc fn-relative syms.  */
      valu += ANOFFSET (section_offsets, SECT_OFF_TEXT); 
      /* FIXME: loses if sizeof (char *) > sizeof (int) */
      record_line (current_subfile, (int)name, valu);
      break;

    /* The following symbol types need to have the appropriate offset added
       to their value; then we process symbol definitions in the name.  */
    case N_SYM_SYM:

      if (name)
	{
	  char deftype;
	  char *dirn, *n;
	  char *p = strchr (name, ':');
	  if (p == NULL)
	    deftype = '\0';
	  else
	    deftype = p[1];


	  switch (deftype)
	    {
	    case 'S':
      	      valu += ANOFFSET (section_offsets, SECT_OFF_TEXT);
	      n = strrchr(name, '/');
	      if (n != NULL) {
		*n = '\0';
		n++;
		dirn = name;
	      } else {
		n = name;
		dirn = NULL;
	      }
	      *p = '\0';
	      if (symfile_depth++ == 0) {
		if (last_source_file) {
          	  end_symtab (valu, objfile, SECT_OFF_TEXT);
		  end_stabs ();
		}
		start_stabs ();
		os9k_stabs = 1;
		start_symtab (n, dirn, valu);
	      } else {
		push_subfile();
		start_subfile (n, dirn!=NULL ? dirn : current_subfile->dirname);
	      }
	      break;

	    case 'f':
	    case 'F':
              valu += ANOFFSET (section_offsets, SECT_OFF_TEXT);
	      function_stab_type = type;

	      within_function = 1;
	      new = push_context (0, valu);
	      new->name = define_symbol (valu, name, desc, type, objfile);
	      break;

	    case 'V':
	    case 'v':
              valu += ANOFFSET (section_offsets, SECT_OFF_DATA);
	      define_symbol (valu, name, desc, type, objfile);
	      break;

	    default:
	      define_symbol (valu, name, desc, type, objfile);
	      break;
	    }
	}
      break;

    case N_SYM_SE:
	if (--symfile_depth != 0)
	  start_subfile(pop_subfile(), current_subfile->dirname);
      break;

    default:
      complain (&unknown_symtype_complaint,
		local_hex_string((unsigned long) type));
      /* FALLTHROUGH */
      break;

    case N_SYM_CMPLR:
      break;
    }
  previous_stab_code = type;
}

/* Parse the user's idea of an offset for dynamic linking, into our idea
   of how to represent it for fast symbol reading.  */

static struct section_offsets *
os9k_symfile_offsets (objfile, addr)
     struct objfile *objfile;
     CORE_ADDR addr;
{
  struct section_offsets *section_offsets;
  int i;

  objfile->num_sections = SECT_OFF_MAX;
  section_offsets = (struct section_offsets *)
    obstack_alloc (&objfile -> psymbol_obstack,
		   sizeof (struct section_offsets)
		   + sizeof (section_offsets->offsets) * (SECT_OFF_MAX-1));

  for (i = 0; i < SECT_OFF_MAX; i++)
    ANOFFSET (section_offsets, i) = addr;
  
  return section_offsets;
}

static struct sym_fns os9k_sym_fns =
{
  bfd_target_os9k_flavour,
  os9k_new_init,	/* sym_new_init: init anything gbl to entire symtab */
  os9k_symfile_init,	/* sym_init: read initial info, setup for sym_read() */
  os9k_symfile_read,	/* sym_read: read a symbol file into symtab */
  os9k_symfile_finish,	/* sym_finish: finished with file, cleanup */
  os9k_symfile_offsets,	/* sym_offsets: parse user's offsets to internal form*/
  NULL			/* next: pointer to next struct sym_fns */
};

void
_initialize_os9kread ()
{
  add_symtab_fns(&os9k_sym_fns);
}
