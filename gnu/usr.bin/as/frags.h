/* frags.h - Header file for the frag concept.

   Copyright (C) 1987, 1992 Free Software Foundation, Inc.

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
 * $FreeBSD$
 */


extern struct obstack	frags;
/* Frags ONLY live in this obstack. */
/* We use obstack_next_free() macro */
/* so please don't put any other objects */
/* on this stack! */

/*
 * A macro to speed up appending exactly 1 char
 * to current frag.
 */
/* JF changed < 1 to <= 1 to avoid a race conditon */
#define FRAG_APPEND_1_CHAR(datum)	\
{					\
					    if (obstack_room( &frags ) <= 1) {\
										  frag_wane (frag_now);	\
										      frag_new (0);		\
										  }				\
										      obstack_1grow( &frags, datum );	\
										  }


#if __STDC__ == 1

char *frag_more(int nchars);
void frag_align(int alignment, int fill_character);
void frag_new(int old_frags_var_max_size);
void frag_wane(fragS *fragP);

char *frag_variant(relax_stateT type,
		   int max_chars,
		   int var,
		   relax_substateT subtype,
		   symbolS *symbol,
		   long offset,
		   char *opcode,
		   int pcrel_adjust,
		   int bsr);

char *frag_var(relax_stateT type,
	       int max_chars,
	       int var,
	       relax_substateT subtype,
	       symbolS *symbol,
	       long offset,
	       char *opcode);

#else /* not __STDC__ */

char *frag_more();
char *frag_var();
char *frag_variant();
void frag_align();
void frag_new();
void frag_wane();

#endif /* not __STDC__ */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of frags.h */
