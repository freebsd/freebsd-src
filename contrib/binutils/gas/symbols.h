/* symbols.h -
   Copyright (C) 1987, 90, 92, 93, 94, 95, 1997 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

extern struct obstack notes;	/* eg FixS live here. */

extern struct obstack cond_obstack;	/* this is where we track .ifdef/.endif
				       (if we do that at all).  */

extern symbolS *symbol_rootP;	/* all the symbol nodes */
extern symbolS *symbol_lastP;	/* last struct symbol we made, or NULL */

extern symbolS abs_symbol;

extern int symbol_table_frozen;

/* This is non-zero if symbols are case sensitive, which is the
   default.  */
extern int symbols_case_sensitive;

char *decode_local_label_name PARAMS ((char *s));
symbolS *symbol_find PARAMS ((CONST char *name));
symbolS *symbol_find_base PARAMS ((CONST char *name, int strip_underscore));
symbolS *symbol_find_or_make PARAMS ((const char *name));
symbolS *symbol_make PARAMS ((CONST char *name));
symbolS *symbol_new PARAMS ((CONST char *name, segT segment, valueT value,
			     fragS * frag));
symbolS *symbol_create PARAMS ((CONST char *name, segT segment, valueT value,
				fragS * frag));
symbolS *colon PARAMS ((const char *sym_name));
void local_colon PARAMS ((int n));
void symbol_begin PARAMS ((void));
void symbol_print_statistics PARAMS ((FILE *));
void symbol_table_insert PARAMS ((symbolS * symbolP));
valueT resolve_symbol_value PARAMS ((symbolS *, int));

void print_symbol_value PARAMS ((symbolS *));
void print_expr PARAMS ((expressionS *));
void print_expr_1 PARAMS ((FILE *, expressionS *));
void print_symbol_value_1 PARAMS ((FILE *, symbolS *));

int dollar_label_defined PARAMS ((long l));
void dollar_label_clear PARAMS ((void));
void define_dollar_label PARAMS ((long l));
char *dollar_label_name PARAMS ((long l, int augend));

void fb_label_instance_inc PARAMS ((long label));
char *fb_label_name PARAMS ((long n, long augend));

extern void copy_symbol_attributes PARAMS ((symbolS *, symbolS *));

/* Get and set the values of symbols.  These used to be macros.  */
extern valueT S_GET_VALUE PARAMS ((symbolS *));
extern void S_SET_VALUE PARAMS ((symbolS *, valueT));

#ifdef BFD_ASSEMBLER
extern int S_IS_EXTERNAL PARAMS ((symbolS *));
extern int S_IS_WEAK PARAMS ((symbolS *));
extern int S_IS_COMMON PARAMS ((symbolS *));
extern int S_IS_DEFINED PARAMS ((symbolS *));
extern int S_IS_DEBUG PARAMS ((symbolS *));
extern int S_IS_LOCAL PARAMS ((symbolS *));
extern int S_IS_EXTERN PARAMS ((symbolS *));
extern int S_IS_STABD PARAMS ((symbolS *));
extern CONST char *S_GET_NAME PARAMS ((symbolS *));
extern segT S_GET_SEGMENT PARAMS ((symbolS *));
extern void S_SET_SEGMENT PARAMS ((symbolS *, segT));
extern void S_SET_EXTERNAL PARAMS ((symbolS *));
extern void S_SET_NAME PARAMS ((symbolS *, char *));
extern void S_CLEAR_EXTERNAL PARAMS ((symbolS *));
extern void S_SET_WEAK PARAMS ((symbolS *));
#endif

/* end of symbols.h */
