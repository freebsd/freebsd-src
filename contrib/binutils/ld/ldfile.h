/* ldfile.h -
   Copyright 1991, 1992, 1993, 1994, 1995, 2000, 2002
   Free Software Foundation, Inc.

   This file is part of GLD, the Gnu Linker.

   GLD is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   GLD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GLD; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef LDFILE_H
#define LDFILE_H

extern const char *ldfile_input_filename;
extern boolean ldfile_assumed_script;
extern unsigned long ldfile_output_machine;
extern enum bfd_architecture ldfile_output_architecture;
extern const char *ldfile_output_machine_name;

/* Structure used to hold the list of directories to search for
   libraries.  */

typedef struct search_dirs {
  /* Next directory on list.  */
  struct search_dirs *next;
  /* Name of directory.  */
  const char *name;
  /* true if this is from the command line.  */
  boolean cmdline;
} search_dirs_type;

extern search_dirs_type *search_head;

extern void ldfile_add_arch PARAMS ((const char *));
extern void ldfile_add_library_path PARAMS ((const char *, boolean cmdline));
extern void ldfile_open_command_file PARAMS ((const char *name));
extern void ldfile_open_file PARAMS ((struct lang_input_statement_struct *));
extern boolean ldfile_try_open_bfd
  PARAMS ((const char *, struct lang_input_statement_struct *));
extern FILE *ldfile_find_command_file
  PARAMS ((const char *name, const char *extend));
extern void ldfile_set_output_arch PARAMS ((const char *));
extern boolean ldfile_open_file_search
  PARAMS ((const char *arch, struct lang_input_statement_struct *,
	   const char *lib, const char *suffix));

#endif
