/* deffile.h - header for .DEF file parser
   Copyright 1998, 1999, 2000 Free Software Foundation, Inc.
   Written by DJ Delorie dj@cygnus.com

   This file is part of GLD, the Gnu Linker.

   GLD is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GLD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GLD; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef DEFFILE_H
#define DEFFILE_H

#include "ansidecl.h"

/* DEF storage definitions.  Note that any ordinal may be zero, and
   any pointer may be NULL, if not defined by the DEF file.  */

typedef struct def_file_section {
  char *name;			/* always set */
  char *class;			/* may be NULL */
  char flag_read, flag_write, flag_execute, flag_shared;
} def_file_section;

typedef struct def_file_export {
  char *name;			/* always set */
  char *internal_name;		/* always set, may == name */
  int ordinal;			/* -1 if not specified */
  int hint;
  char flag_private, flag_constant, flag_noname, flag_data;
} def_file_export;

typedef struct def_file_module {
  struct def_file_module *next;
  void *user_data;
  char name[1];			/* extended via malloc */
} def_file_module;

typedef struct def_file_import {
  char *internal_name;		/* always set */
  def_file_module *module;	/* always set */
  char *name;			/* may be NULL; either this or ordinal will be set */
  int ordinal;			/* may be -1 */
} def_file_import;

typedef struct def_file {
  /* from the NAME or LIBRARY command */
  char *name;
  int is_dll;			/* -1 if NAME/LIBRARY not given */
  bfd_vma base_address;		/* (bfd_vma)(-1) if unspecified */

  /* from the DESCRIPTION command */
  char *description;

  /* from the STACK/HEAP command, -1 if unspecified */
  int stack_reserve, stack_commit;
  int heap_reserve, heap_commit;

  /* from the SECTION/SEGMENT commands */
  int num_section_defs;
  def_file_section *section_defs;

  /* from the EXPORTS commands */
  int num_exports;
  def_file_export *exports;

  /* used by imports for module names */
  def_file_module *modules;

  /* from the IMPORTS commands */
  int num_imports;
  def_file_import *imports;

  /* from the VERSION command, -1 if not specified */
  int version_major, version_minor;
} def_file;

extern def_file *def_file_empty PARAMS ((void));

/* add_to may be NULL.  If not, this .def is appended to it */
extern def_file *def_file_parse PARAMS ((const char *_filename,
					 def_file * _add_to));

extern void def_file_free PARAMS ((def_file * _def));

extern def_file_export *def_file_add_export PARAMS ((def_file * _def,
						     const char *_name,
						 const char *_internal_name,
						     int _ordinal));

extern def_file_import *def_file_add_import PARAMS ((def_file * _def,
						     const char *_name,
						     const char *_from,
						     int _ordinal,
					       const char *_imported_name));

extern void def_file_add_directive PARAMS ((def_file * _def,
					    const char *param,
					    int len));

#ifdef DEF_FILE_PRINT
extern void def_file_print PARAMS ((FILE * _file, def_file * _def));
#endif

#endif /* DEFFILE_H */
