/* symbols.h -

   Copyright (C) 1987, 1990, 1992 Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */
/*
 * $FreeBSD: src/gnu/usr.bin/as/symbols.h,v 1.6 1999/08/27 23:34:22 peter Exp $
 */


extern struct obstack	notes; /* eg FixS live here. */

extern struct obstack cond_obstack; /* this is where we track .ifdef/.endif
				       (if we do that at all).  */

extern unsigned int local_bss_counter; /* Zeroed before a pass. */
/* Only used by .lcomm directive. */

extern symbolS *symbol_rootP;	/* all the symbol nodes */
extern symbolS *symbol_lastP;	/* last struct symbol we made, or NULL */

extern symbolS abs_symbol;

extern symbolS *dot_text_symbol;
extern symbolS *dot_data_symbol;
extern symbolS *dot_bss_symbol;

#if __STDC__ == 1

char *decode_local_label_name(char *s);
char *local_label_name(int n, int augend);
symbolS *symbol_find(char *name);
symbolS *symbol_find_base(char *name, int strip_underscore);
symbolS *symbol_find_or_make(char *name);
symbolS *symbol_make(char *name);
symbolS *symbol_new(char *name, segT segment, long value, fragS *frag);
void colon(char *sym_name);
void local_colon(int n);
void symbol_begin(void);
void symbol_table_insert(symbolS *symbolP);
void verify_symbol_chain(symbolS *rootP, symbolS *lastP);

#else /* not __STDC__ */

char *decode_local_label_name();
char *local_label_name();
symbolS *symbol_find();
symbolS *symbol_find_base();
symbolS *symbol_find_or_make();
symbolS *symbol_make();
symbolS *symbol_new();
void colon();
void local_colon();
void symbol_begin();
void symbol_table_insert();
void verify_symbol_chain();

#endif /* not __STDC__ */


/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of symbols.h */
