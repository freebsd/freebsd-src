/* struct_symbol.h - Internal symbol structure
   Copyright (C) 1987, 1992, 1993, 1994 Free Software Foundation, Inc.

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
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef __struc_symbol_h__
#define __struc_symbol_h__

#ifdef BFD_ASSEMBLER
/* The BFD code wants to walk the list in both directions.  */
#undef  SYMBOLS_NEED_BACKPOINTERS
#define SYMBOLS_NEED_BACKPOINTERS
#endif

/* our version of an nlist node */
struct symbol
{
#ifndef BFD_ASSEMBLER
  /* The (4-origin) position of sy_name in the symbol table of the object
     file.  This will be 0 for (nameless) .stabd symbols.

     Not used until write_object_file() time. */
  unsigned long sy_name_offset;

  /* What we write in .o file (if permitted).  */
  obj_symbol_type sy_symbol;

  /* The 24 bit symbol number.  Symbol numbers start at 0 and are unsigned. */
  long sy_number;
#else
  /* BFD symbol */
  asymbol *bsym;
#endif

  /* The value of the symbol.  */
  expressionS sy_value;

  /* Forwards and (optionally) backwards chain pointers.  */
  struct symbol *sy_next;
#ifdef SYMBOLS_NEED_BACKPOINTERS
  struct symbol *sy_previous;
#endif /* SYMBOLS_NEED_BACKPOINTERS */

  /* Pointer to the frag this symbol is attached to, if any.
     Otherwise, NULL.  */
  struct frag *sy_frag;

  unsigned int written : 1;
  /* Whether symbol value has been completely resolved (used during
     final pass over symbol table).  */
  unsigned int sy_resolved : 1;
  /* Whether the symbol value is currently being resolved (used to
     detect loops in symbol dependencies).  */
  unsigned int sy_resolving : 1;
  /* Whether the symbol value is used in a reloc.  This is used to
     ensure that symbols used in relocs are written out, even if they
     are local and would otherwise not be.  */
  unsigned int sy_used_in_reloc : 1;

  /* Whether the symbol is used as an operand or in an expression.  
     NOTE:  Not all the backends keep this information accurate;
     backends which use this bit are responsible for setting it when
     a symbol is used in backend routines.  */
  unsigned int sy_used : 1;

  /* This is set if the symbol is defined in an MRI common section.
     We handle such sections as single common symbols, so symbols
     defined within them must be treated specially by the relocation
     routines.  */
  unsigned int sy_mri_common : 1;

#ifdef OBJ_SYMFIELD_TYPE
  OBJ_SYMFIELD_TYPE sy_obj;
#endif

#ifdef TC_SYMFIELD_TYPE
  TC_SYMFIELD_TYPE sy_tc;
#endif

#ifdef TARGET_SYMBOL_FIELDS
  TARGET_SYMBOL_FIELDS
#endif
};

typedef struct symbol symbolS;

#ifndef WORKING_DOT_WORD
struct broken_word
  {
    /* Linked list -- one of these structures per ".word x-y+C"
       expression.  */
    struct broken_word *next_broken_word;
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

#define symbol_previous(s) ((s)->sy_previous)

#endif /* SYMBOLS_NEED_BACKPOINTERS */

void verify_symbol_chain PARAMS ((symbolS * rootP, symbolS * lastP));
void verify_symbol_chain_2 PARAMS ((symbolS * symP));

void symbol_append PARAMS ((symbolS * addme, symbolS * target,
			    symbolS ** rootP, symbolS ** lastP));

#define symbol_next(s)	((s)->sy_next)

#endif /* __struc_symbol_h__ */

/* end of struc-symbol.h */
