/* symbols.h -
   Copyright (C) 1987, 90, 92, 93, 94, 95, 97, 99, 2000
   Free Software Foundation, Inc.

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
void resolve_local_symbol_values PARAMS ((void));

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
extern int S_IS_FUNCTION PARAMS ((symbolS *));
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

    int added;			/* nasty thing happend yet? */
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

void symbol_clear_list_pointers PARAMS ((symbolS * symbolP));

#ifdef SYMBOLS_NEED_BACKPOINTERS

void symbol_insert PARAMS ((symbolS * addme, symbolS * target,
			    symbolS ** rootP, symbolS ** lastP));
void symbol_remove PARAMS ((symbolS * symbolP, symbolS ** rootP,
			    symbolS ** lastP));

extern symbolS *symbol_previous PARAMS ((symbolS *));

#endif /* SYMBOLS_NEED_BACKPOINTERS */

void verify_symbol_chain PARAMS ((symbolS * rootP, symbolS * lastP));
void verify_symbol_chain_2 PARAMS ((symbolS * symP));

void symbol_append PARAMS ((symbolS * addme, symbolS * target,
			    symbolS ** rootP, symbolS ** lastP));

extern symbolS *symbol_next PARAMS ((symbolS *));

extern expressionS *symbol_get_value_expression PARAMS ((symbolS *));
extern void symbol_set_value_expression PARAMS ((symbolS *,
						 const expressionS *));
extern void symbol_set_frag PARAMS ((symbolS *, fragS *));
extern fragS *symbol_get_frag PARAMS ((symbolS *));
extern void symbol_mark_used PARAMS ((symbolS *));
extern void symbol_clear_used PARAMS ((symbolS *));
extern int symbol_used_p PARAMS ((symbolS *));
extern void symbol_mark_used_in_reloc PARAMS ((symbolS *));
extern void symbol_clear_used_in_reloc PARAMS ((symbolS *));
extern int symbol_used_in_reloc_p PARAMS ((symbolS *));
extern void symbol_mark_mri_common PARAMS ((symbolS *));
extern void symbol_clear_mri_common PARAMS ((symbolS *));
extern int symbol_mri_common_p PARAMS ((symbolS *));
extern void symbol_mark_written PARAMS ((symbolS *));
extern void symbol_clear_written PARAMS ((symbolS *));
extern int symbol_written_p PARAMS ((symbolS *));
extern void symbol_mark_resolved PARAMS ((symbolS *));
extern int symbol_resolved_p PARAMS ((symbolS *));
extern int symbol_section_p PARAMS ((symbolS *));
extern int symbol_equated_p PARAMS ((symbolS *));
extern int symbol_constant_p PARAMS ((symbolS *));

#ifdef BFD_ASSEMBLER
extern asymbol *symbol_get_bfdsym PARAMS ((symbolS *));
extern void symbol_set_bfdsym PARAMS ((symbolS *, asymbol *));
#endif

#ifdef OBJ_SYMFIELD_TYPE
OBJ_SYMFIELD_TYPE *symbol_get_obj PARAMS ((symbolS *));
void symbol_set_obj PARAMS ((symbolS *, OBJ_SYMFIELD_TYPE *));
#endif

#ifdef TC_SYMFIELD_TYPE
TC_SYMFIELD_TYPE *symbol_get_tc PARAMS ((symbolS *));
void symbol_set_tc PARAMS ((symbolS *, TC_SYMFIELD_TYPE *));
#endif
