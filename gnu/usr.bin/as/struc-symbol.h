/* struct_symbol.h - Internal symbol structure
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
   
   oYou should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */
/*
 * $Id: struc-symbol.h,v 1.2 1993/11/03 00:52:18 paul Exp $
 */


#ifndef __struc_symbol_h__
#define __struc_symbol_h__

struct symbol			/* our version of an nlist node */
{
	obj_symbol_type sy_symbol;	/* what we write in .o file (if permitted) */
	unsigned long sy_name_offset;	/* 4-origin position of sy_name in symbols */
	/* part of object file. */
	/* 0 for (nameless) .stabd symbols. */
	/* Not used until write_object_file() time. */
	long	sy_number;	/* 24 bit symbol number. */
	/* Symbol numbers start at 0 and are */
	/* unsigned. */
	struct symbol *sy_next;	/* forward chain, or NULL */
#ifdef SYMBOLS_NEED_BACKPOINTERS
	struct symbol *sy_previous;	/* backward chain, or NULL */
#endif /* SYMBOLS_NEED_BACKPOINTERS */
	struct frag *sy_frag;	/* NULL or -> frag this symbol attaches to. */
	struct symbol *sy_forward;	/* value is really that of this other symbol */
	/* We will probably want to add a sy_segment here soon. */

#ifdef PIC
	/* Force symbol into symbol table, even if local */
	int	sy_forceout;
#endif
	/* Size of symbol as given by the .size directive */
	void 	*sy_sizexp;	/* (expressionS *) */

	/* Auxiliary type information as given by the .type directive */
	int	sy_aux;
#define AUX_OBJECT	1
#define AUX_FUNC	2
};

typedef struct symbol symbolS;

#ifdef PIC
symbolS *GOT_symbol;		/* Pre-defined "__GLOBAL_OFFSET_TABLE" */
int	got_referenced;
#endif

typedef unsigned valueT;	/* The type of n_value. Helps casting. */

#ifndef WORKING_DOT_WORD
struct broken_word {
	struct broken_word *next_broken_word;/* One of these strucs per .word x-y */
	fragS	*frag;		/* Which frag its in */
	char	*word_goes_here;/* Where in the frag it is */
	fragS	*dispfrag;	/* where to add the break */
	symbolS	*add;		/* symbol_x */
	symbolS	*sub;		/* - symbol_y */
	long	addnum;		/* + addnum */
	int	added;		/* nasty thing happend yet? */
	/* 1: added and has a long-jump */
	/* 2: added but uses someone elses long-jump */
	struct broken_word *use_jump; /* points to broken_word with a similar
					 long-jump */
};
extern struct broken_word *broken_words;
#endif /* ndef WORKING_DOT_WORD */

#define	SEGMENT_TO_SYMBOL_TYPE(seg)  (seg_N_TYPE[(int) (seg)])
extern const short seg_N_TYPE[]; /* subseg.c */

#define	N_REGISTER	30	/* Fake N_TYPE value for SEG_REGISTER */

#ifdef SYMBOLS_NEED_BACKPOINTERS
#if __STDC__ == 1

void symbol_clear_list_pointers(symbolS *symbolP);
void symbol_insert(symbolS *addme, symbolS *target, symbolS **rootP, symbolS **lastP);
void symbol_remove(symbolS *symbolP, symbolS **rootP, symbolS **lastP);
void verify_symbol_chain(symbolS *rootP, symbolS *lastP);

#else /* not __STDC__ */

void symbol_clear_list_pointers();
void symbol_insert();
void symbol_remove();
void verify_symbol_chain();

#endif /* not __STDC__ */

#define symbol_previous(s) ((s)->sy_previous)

#else /* SYMBOLS_NEED_BACKPOINTERS */

#define symbol_clear_list_pointers(clearme) {clearme->sy_next = NULL;}

#endif /* SYMBOLS_NEED_BACKPOINTERS */

#if __STDC__ == 1
void symbol_append(symbolS *addme, symbolS *target, symbolS **rootP, symbolS **lastP);
#else /* not __STDC__ */
void symbol_append();
#endif /* not __STDC__ */

#define symbol_next(s)	((s)->sy_next)

#endif /* __struc_symbol_h__ */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of struc-symbol.h */
