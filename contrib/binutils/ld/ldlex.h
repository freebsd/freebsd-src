/* ldlex.h -
   Copyright 1991, 1992, 1993, 1994, 1995, 1997, 2000
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
   along with GLD; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef LDLEX_H
#define LDLEX_H

#include <stdio.h>

/* The initial parser states.  */
typedef enum input_enum {
  input_selected,		/* We've set the initial state.  */
  input_script,
  input_mri_script,
  input_version_script,
  input_defsym
} input_type;

extern input_type parser_input;

extern unsigned int lineno;
extern const char *lex_string;

/* In ldlex.l.  */
extern int yylex PARAMS ((void));
extern void lex_push_file PARAMS ((FILE *, const char *));
extern void lex_redirect PARAMS ((const char *));
extern void ldlex_script PARAMS ((void));
extern void ldlex_mri_script PARAMS ((void));
extern void ldlex_version_script PARAMS ((void));
extern void ldlex_version_file PARAMS ((void));
extern void ldlex_defsym PARAMS ((void));
extern void ldlex_expression PARAMS ((void));
extern void ldlex_both PARAMS ((void));
extern void ldlex_command PARAMS ((void));
extern void ldlex_popstate PARAMS ((void));

/* In lexsup.c.  */
extern int lex_input PARAMS ((void));
extern void lex_unput PARAMS ((int));
#ifndef yywrap
extern int yywrap PARAMS ((void));
#endif
extern void parse_args PARAMS ((unsigned, char **));

#endif
