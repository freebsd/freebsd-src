/* index.h -- declarations for index.c.
   $Id: index.h,v 1.2 2004/04/11 17:56:47 karl Exp $

   Copyright (C) 1998, 99 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef INDEX_H
#define INDEX_H

#include "makeinfo.h"
#include "cmds.h"

/* User commands are only new indices.  (Macros are handled separately.)  */
extern COMMAND **user_command_array;
extern int user_command_array_len;

/* An index element... */
typedef struct index_elt
{
  struct index_elt *next;
  char *entry;                  /* The index entry itself, after expansion. */
  char *entry_text;             /* The original, non-expanded entry text. */
  char *node;                   /* The node from whence it came. */
  char *section;                /* Current section number we are in, */
  char *section_name;           /* ... and its title.  */
  int code;                     /* Nonzero means add `@code{...}' when
                                   printing this element. */
  int defining_line;            /* Line number where this entry was written. */
  int output_line;              /* And line number where it is in the output. */
  char *defining_file;          /* Source file for defining_line. */
  char *output_file;            /* Output file for output_line. */
  int entry_number;             /* Entry number.  */
} INDEX_ELT;


/* A list of short-names for each index.
   There are two indices into the the_indices array.
   * read_index is the index that points to the list of index
     entries that we will find if we ask for the list of entries for
     this name.
   * write_index is the index that points to the list of index entries
     that we will add new entries to.

   Initially, read_index and write_index are the same, but the
   @syncodeindex and @synindex commands can change the list we add
   entries to.

   For example, after the commands
     @cindex foo
     @defindex ii
     @synindex cp ii
     @cindex bar

   the cp index will contain the entry `foo', and the new ii
   index will contain the entry `bar'.  This is consistent with the
   way texinfo.tex handles the same situation.

   In addition, for each index, it is remembered whether that index is
   a code index or not.  Code indices have @code{} inserted around the
   first word when they are printed with printindex. */
typedef struct
{
  char *name;
  int read_index;   /* index entries for `name' */
  int write_index;  /* store index entries here, @synindex can change it */
  int code;
} INDEX_ALIST;

extern INDEX_ALIST **name_index_alist;

/* Initialize all indices.  */
extern void init_indices (void);

extern int defined_indices;
extern int printing_index;
extern int index_counter;

INDEX_ELT *index_list (char *name);

#endif /* !INDEX_H */
