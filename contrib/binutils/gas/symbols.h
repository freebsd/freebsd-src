/* symbols.h -
   Copyright 1987, 1990, 1992, 1993, 1994, 1995, 1997, 1999, 2000, 2001,
   2002, 2003 Free Software Foundation, Inc.

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

#ifdef BFD_ASSEMBLER
/* The BFD code wants to walk the list in both directions.  */
#undef  SYMBOLS_NEED_BACKPOINTERS
#define SYMBOLS_NEED_BACKPOINTERS
#endif

#ifndef BFD_ASSEMBLER
/* The non-BFD code expects to be able to manipulate the symbol fields
   directly.  */
#include "struc-symbol.h"
#endif

extern struct obstack notes;	/* eg FixS live here.  */

extern struct obstack cond_obstack;	/* this is where we track .ifdef/.endif
				       (if we do that at all).  */

extern symbolS *symbol_rootP;	/* all the symbol nodes */
extern symbolS *symbol_lastP;	/* last struct symbol we made, or NULL */

extern symbolS abs_symbol;

extern int symbol_table_frozen;

/* This is non-zero if symbols are case sensitive, which is the
   default.  */
extern int symbols_case_sensitive;

char *decode_local_label_name (char *s);
symbolS *symbol_find (const char *name);
symbolS *symbol_find_exact (const char *name);
symbolS *symbol_find_base (const char *name, int strip_underscore);
symbolS *symbol_find_or_make (const char *name);
symbolS *symbol_make (const char *name);
symbolS *symbol_new (const char *name, segT segment, valueT value,
		     fragS * frag);
symbolS *symbol_create (const char *name, segT segment, valueT value,
			fragS * frag);
struct local_symbol *local_symbol_make (const char *name, segT section,
					valueT value, fragS * frag);
symbolS *symbol_temp_new (segT, valueT, fragS *);
symbolS *symbol_temp_new_now (void);
symbolS *symbol_temp_make (void);

symbolS *colon (const char *sym_name);
void local_colon (int n);
void symbol_begin (void);
void symbol_print_statistics (FILE *);
void symbol_table_insert (symbolS * symbolP);
valueT resolve_symbol_value (symbolS *);
void resolve_local_symbol_values (void);

void print_symbol_value (symbolS *);
void print_expr (expressionS *);
void print_expr_1 (FILE *, expressionS *);
void print_symbol_value_1 (FILE *, symbolS *);

int dollar_label_defined (long l);
void dollar_label_clear (void);
void define_dollar_label (long l);
char *dollar_label_name (long l, int augend);

void fb_label_instance_inc (long label);
char *fb_label_name (long n, long augend);

extern void copy_symbol_attributes (symbolS *, symbolS *);

/* Get and set the values of symbols.  These used to be macros.  */
extern valueT S_GET_VALUE (symbolS *);
extern void S_SET_VALUE (symbolS *, valueT);

#ifdef BFD_ASSEMBLER
extern int S_IS_FUNCTION (symbolS *);
extern int S_IS_EXTERNAL (symbolS *);
extern int S_IS_WEAK (symbolS *);
extern int S_IS_COMMON (symbolS *);
extern int S_IS_DEFINED (symbolS *);
extern int S_FORCE_RELOC (symbolS *, int);
extern int S_IS_DEBUG (symbolS *);
extern int S_IS_LOCAL (symbolS *);
extern int S_IS_EXTERN (symbolS *);
extern int S_IS_STABD (symbolS *);
extern const char *S_GET_NAME (symbolS *);
extern segT S_GET_SEGMENT (symbolS *);
extern void S_SET_SEGMENT (symbolS *, segT);
extern void S_SET_EXTERNAL (symbolS *);
extern void S_SET_NAME (symbolS *, char *);
extern void S_CLEAR_EXTERNAL (symbolS *);
extern void S_SET_WEAK (symbolS *);
extern void S_SET_THREAD_LOCAL (symbolS *);
#endif

#ifndef WORKING_DOT_WORD
struct broken_word
  {
    /* Linked list -- one of these structures per ".word x-y+C"
       expression.  */
    struct broken_word *next_broken_word;
    /* Segment and subsegment for broken word.  */
    segT seg;
    subsegT subseg;
    /* Which frag is this broken word in?  */
    fragS *frag;
    /* Where in the frag is it?  */
    char *word_goes_here;
    /* Where to add the break.  */
    fragS *dispfrag;		/* where to add the break */
    /* Operands of expression.  */
    symbolS *add;
    symbolS *sub;
    offsetT addnum;

    int added;			/* nasty thing happened yet? */
    /* 1: added and has a long-jump */
    /* 2: added but uses someone elses long-jump */

    /* Pointer to broken_word with a similar long-jump.  */
    struct broken_word *use_jump;
  };
extern struct broken_word *broken_words;
#endif /* ndef WORKING_DOT_WORD */

/*
 * Current means for getting from symbols to segments and vice verse.
 * This will change for infinite-segments support (e.g. COFF).
 */
extern const segT N_TYPE_seg[];	/* subseg.c */

#define	SEGMENT_TO_SYMBOL_TYPE(seg)  ( seg_N_TYPE [(int) (seg)] )
extern const short seg_N_TYPE[];/* subseg.c */

#define	N_REGISTER	30	/* Fake N_TYPE value for SEG_REGISTER */

void symbol_clear_list_pointers (symbolS * symbolP);

#ifdef SYMBOLS_NEED_BACKPOINTERS

void symbol_insert (symbolS * addme, symbolS * target,
		    symbolS ** rootP, symbolS ** lastP);
void symbol_remove (symbolS * symbolP, symbolS ** rootP,
		    symbolS ** lastP);

extern symbolS *symbol_previous (symbolS *);

#endif /* SYMBOLS_NEED_BACKPOINTERS */

void verify_symbol_chain (symbolS * rootP, symbolS * lastP);
void verify_symbol_chain_2 (symbolS * symP);

void symbol_append (symbolS * addme, symbolS * target,
		    symbolS ** rootP, symbolS ** lastP);

extern symbolS *symbol_next (symbolS *);

extern expressionS *symbol_get_value_expression (symbolS *);
extern void symbol_set_value_expression (symbolS *, const expressionS *);
extern void symbol_set_value_now (symbolS *);
extern void symbol_set_frag (symbolS *, fragS *);
extern fragS *symbol_get_frag (symbolS *);
extern void symbol_mark_used (symbolS *);
extern void symbol_clear_used (symbolS *);
extern int symbol_used_p (symbolS *);
extern void symbol_mark_used_in_reloc (symbolS *);
extern void symbol_clear_used_in_reloc (symbolS *);
extern int symbol_used_in_reloc_p (symbolS *);
extern void symbol_mark_mri_common (symbolS *);
extern void symbol_clear_mri_common (symbolS *);
extern int symbol_mri_common_p (symbolS *);
extern void symbol_mark_written (symbolS *);
extern void symbol_clear_written (symbolS *);
extern int symbol_written_p (symbolS *);
extern void symbol_mark_resolved (symbolS *);
extern int symbol_resolved_p (symbolS *);
extern int symbol_section_p (symbolS *);
extern int symbol_equated_p (symbolS *);
extern int symbol_equated_reloc_p (symbolS *);
extern int symbol_constant_p (symbolS *);

#ifdef BFD_ASSEMBLER
extern asymbol *symbol_get_bfdsym (symbolS *);
extern void symbol_set_bfdsym (symbolS *, asymbol *);
#endif

#ifdef OBJ_SYMFIELD_TYPE
OBJ_SYMFIELD_TYPE *symbol_get_obj (symbolS *);
void symbol_set_obj (symbolS *, OBJ_SYMFIELD_TYPE *);
#endif

#ifdef TC_SYMFIELD_TYPE
TC_SYMFIELD_TYPE *symbol_get_tc (symbolS *);
void symbol_set_tc (symbolS *, TC_SYMFIELD_TYPE *);
#endif
