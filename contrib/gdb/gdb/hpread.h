/* hpread.h
 * Common include file for:
 *   hp_symtab_read.c
 *   hp_psymtab_read.c
 */

/* Copyright 1993, 1996 Free Software Foundation, Inc.

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

extern union sltentry *hpread_get_slt
  PARAMS ((int, struct objfile *));

extern union dnttentry *hpread_get_lntt
  PARAMS ((int, struct objfile *));

int hpread_has_name
  PARAMS ((enum dntt_entry_type));

/* end of hpread.h */
