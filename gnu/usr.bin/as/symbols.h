/* symbols.h -
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

extern struct obstack	notes; /* eg FixS live here. */

#define symbol_table_lookup(name) ((symbolS *)(symbol_find (name)))

extern unsigned int local_bss_counter; /* Zeroed before a pass. */
				/* Only used by .lcomm directive. */


extern symbolS * symbol_rootP;	/* all the symbol nodes */
extern symbolS * symbol_lastP;	/* last struct symbol we made, or NULL */

extern symbolS	abs_symbol;

symbolS *	symbol_find();
void		symbol_begin();
char *		local_label_name();
void		local_colon();
symbolS *	symbol_new();
void		colon();
void		symbol_table_insert();
symbolS *	symbol_find_or_make();

/* end: symbols.h */
