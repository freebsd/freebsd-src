/* macro.h -- declarations for macro.c.
   $Id: macro.h,v 1.5 1999/07/15 00:00:46 karl Exp $

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   */

#ifndef MACRO_H
#define MACRO_H

extern FILE *macro_expansion_output_stream;
extern char *macro_expansion_filename;
extern int me_executing_string;
extern int only_macro_expansion;

/* Here is a structure used to remember input text strings and offsets
   within them. */
typedef struct {
  char *pointer;                /* Pointer to the input text. */
  int offset;                   /* Offset of the last character output. */
} ITEXT;

/* Macro definitions for user-defined commands. */
typedef struct {
  char *name;                   /* Name of the macro. */
  char **arglist;               /* Args to replace when executing. */
  char *body;                   /* Macro body. */
  char *source_file;            /* File where this macro is defined. */
  int source_lineno;            /* Line number within FILENAME. */
  int inhibited;                /* Nonzero means make find_macro () fail. */
  int flags;                    /* ME_RECURSE, ME_QUOTE_ARG, etc. */
} MACRO_DEF;

/* flags for MACRO_DEF */
#define ME_RECURSE      0x01
#define ME_QUOTE_ARG    0x02

extern void execute_macro ();
extern MACRO_DEF *find_macro ();
extern char *expand_macro ();

extern ITEXT *remember_itext ();
extern void forget_itext ();
extern void maybe_write_itext ();
extern void write_region_to_macro_output ();
extern void append_to_expansion_output ();
extern void me_append_before_this_command ();
extern void me_execute_string ();

extern char *alias_expand ();
extern int enclosure_command ();
extern void enclosure_expand ();

/* The @commands.  */
extern void cm_macro (), cm_rmacro (), cm_unmacro ();
extern void cm_alias (), cm_definfoenclose ();

#endif /* not MACRO_H */
