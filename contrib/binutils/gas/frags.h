/* frags.h - Header file for the frag concept.
   Copyright (C) 1987, 92, 93, 94, 95, 1997 Free Software Foundation, Inc.

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

#ifdef ANSI_PROTOTYPES
struct obstack;
#endif

#if 0
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
#else
extern void frag_append_1_char PARAMS ((int));
#define FRAG_APPEND_1_CHAR(X) frag_append_1_char (X)
#endif


void frag_init PARAMS ((void));
fragS *frag_alloc PARAMS ((struct obstack *));
void frag_grow PARAMS ((unsigned int nchars));
char *frag_more PARAMS ((int nchars));
void frag_align PARAMS ((int alignment, int fill_character, int max));
void frag_align_pattern PARAMS ((int alignment,
				 const char *fill_pattern,
				 int n_fill,
				 int max));
void frag_new PARAMS ((int old_frags_var_max_size));
void frag_wane PARAMS ((fragS * fragP));

char *frag_variant PARAMS ((relax_stateT type,
			    int max_chars,
			    int var,
			    relax_substateT subtype,
			    symbolS * symbol,
			    offsetT offset,
			    char *opcode));

char *frag_var PARAMS ((relax_stateT type,
			int max_chars,
			int var,
			relax_substateT subtype,
			symbolS * symbol,
			offsetT offset,
			char *opcode));

/* end of frags.h */
