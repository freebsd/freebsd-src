/* frags.h - Header file for the frag concept.
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
      

/* end: frags.h */
