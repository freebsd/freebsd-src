/* symbols.c -symbol table-

   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.

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

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#include "as.h"

#include "obstack.h"		/* For "symbols.h" */
#include "subsegs.h"

#ifndef WORKING_DOT_WORD
extern int new_broken_words;
#endif

static
    struct hash_control *
    sy_hash;			/* symbol-name => struct symbol pointer */

/* Below are commented in "symbols.h". */
unsigned int local_bss_counter;
symbolS * symbol_rootP;
symbolS * symbol_lastP;
symbolS	abs_symbol;

symbolS*		dot_text_symbol;
symbolS*		dot_data_symbol;
symbolS*		dot_bss_symbol;

struct obstack	notes;

/*
 * Un*x idea of local labels. They are made by "n:" where n
 * is any decimal digit. Refer to them with
 *  "nb" for previous (backward) n:
 *  or "nf" for next (forward) n:.
 *
 * Like Un*x AS, we have one set of local label counters for entire assembly,
 * not one set per (sub)segment like in most assemblers. This implies that
 * one can refer to a label in another segment, and indeed some crufty
 * compilers have done just that.
 *
 * I document the symbol names here to save duplicating words elsewhere.
 * The mth occurence of label n: is turned into the symbol "Ln^Am" where
 * n is a digit and m is a decimal number. "L" makes it a label discarded
 * unless debugging and "^A"('\1') ensures no ordinary symbol SHOULD get the
 * same name as a local label symbol. The first "4:" is "L4^A1" - the m
 * numbers begin at 1.
 */

typedef short unsigned int
    local_label_countT;

static local_label_countT
    local_label_counter[10];

static				/* Returned to caller, then copied. */
    char symbol_name_build[12];	/* used for created names ("4f") */

#ifdef LOCAL_LABELS_DOLLAR
int local_label_defined[10];
#endif


void
    symbol_begin()
{
	symbol_lastP = NULL;
	symbol_rootP = NULL;		/* In case we have 0 symbols (!!) */
	sy_hash = hash_new();
	memset((char *)(& abs_symbol), '\0', sizeof(abs_symbol));
	S_SET_SEGMENT(&abs_symbol, SEG_ABSOLUTE);	/* Can't initialise a union. Sigh. */
	memset((char *)(local_label_counter), '\0', sizeof(local_label_counter) );
	local_bss_counter = 0;
}

/*
 *			local_label_name()
 *
 * Caller must copy returned name: we re-use the area for the next name.
 */

char *				/* Return local label name. */
    local_label_name(n, augend)
register int n;	/* we just saw "n:", "nf" or "nb" : n a digit */
register int augend; /* 0 for nb, 1 for n:, nf */
{
	register char *	p;
	register char *	q;
	char symbol_name_temporary[10]; /* build up a number, BACKWARDS */

	know( n >= 0 );
	know( augend == 0 || augend == 1 );
	p = symbol_name_build;
	* p ++ = 1;			/* ^A */
	* p ++ = 'L';
	* p ++ = n + '0';		/* Make into ASCII */
	n = local_label_counter[ n ] + augend;
	/* version number of this local label */
	/*
	 * Next code just does sprintf( {}, "%d", n);
	 * It is more elegant to do the next part recursively, but a procedure
	 * call for each digit emitted is considered too costly.
	 */
	q = symbol_name_temporary;
	for (*q++=0; n; q++)		/* emits NOTHING if n starts as 0 */
	    {
		    know(n>0);		/* We expect n > 0 always */
		    *q = n % 10 + '0';
		    n /= 10;
	    }
	while (( * p ++ = * -- q ) != '\0') ;;

	/* The label, as a '\0' ended string, starts at symbol_name_build. */
	return(symbol_name_build);
} /* local_label_name() */


void local_colon (n)
int n;	/* just saw "n:" */
{
	local_label_counter[n] ++;
#ifdef LOCAL_LABELS_DOLLAR
	local_label_defined[n]=1;
#endif
	colon (local_label_name (n, 0));
}

/*
 *			symbol_new()
 *
 * Return a pointer to a new symbol.
 * Die if we can't make a new symbol.
 * Fill in the symbol's values.
 * Add symbol to end of symbol chain.
 *
 *
 * Please always call this to create a new symbol.
 *
 * Changes since 1985: Symbol names may not contain '\0'. Sigh.
 * 2nd argument is now a SEG rather than a TYPE.  The mapping between
 * segments and types is mostly encapsulated herein (actually, we inherit it
 * from macros in struc-symbol.h).
 */

symbolS *symbol_new(name, segment, value, frag)
char *name;			/* It is copied, the caller can destroy/modify */
segT segment;			/* Segment identifier (SEG_<something>) */
long value;			/* Symbol value */
fragS *frag;			/* Associated fragment */
{
	unsigned int name_length;
	char *preserved_copy_of_name;
	symbolS *symbolP;

	name_length = strlen(name) + 1; /* +1 for \0 */
	obstack_grow(&notes, name, name_length);
	preserved_copy_of_name = obstack_finish(&notes);
	symbolP = (symbolS *) obstack_alloc(&notes, sizeof(symbolS));

	/* symbol must be born in some fixed state.  This seems as good as any. */
	memset(symbolP, 0, sizeof(symbolS));

#ifdef STRIP_UNDERSCORE
	S_SET_NAME(symbolP, (*preserved_copy_of_name == '_'
			     ? preserved_copy_of_name + 1
			     : preserved_copy_of_name));
#else /* STRIP_UNDERSCORE */
	S_SET_NAME(symbolP, preserved_copy_of_name);
#endif /* STRIP_UNDERSCORE */

	S_SET_SEGMENT(symbolP, segment);
	S_SET_VALUE(symbolP, value);
	/*	symbol_clear_list_pointers(symbolP); uneeded if symbol is born zeroed. */

	symbolP->sy_frag = frag;
	/* krm: uneeded if symbol is born zeroed.
	   symbolP->sy_forward = NULL; */ /* JF */
	symbolP->sy_number = ~0;
	symbolP->sy_name_offset = ~0;

	/*
	 * Link to end of symbol chain.
	 */
	symbol_append(symbolP, symbol_lastP, &symbol_rootP, &symbol_lastP);

	obj_symbol_new_hook(symbolP);

#ifdef DEBUG
	/*	verify_symbol_chain(symbol_rootP, symbol_lastP); */
#endif /* DEBUG */

	return(symbolP);
} /* symbol_new() */


/*
 *			colon()
 *
 * We have just seen "<name>:".
 * Creates a struct symbol unless it already exists.
 *
 * Gripes if we are redefining a symbol incompatibly (and ignores it).
 *
 */
void colon(sym_name)		/* just seen "x:" - rattle symbols & frags */
register char *  sym_name; /* symbol name, as a cannonical string */
/* We copy this string: OK to alter later. */
{
	register symbolS * symbolP; /* symbol we are working with */

#ifdef LOCAL_LABELS_DOLLAR
	/* Sun local labels go out of scope whenever a non-local symbol is defined.  */

	if (*sym_name != 'L')
	    memset((void *) local_label_defined, '\0', sizeof(local_label_defined));
#endif

#ifndef WORKING_DOT_WORD
	if (new_broken_words) {
		struct broken_word *a;
		int possible_bytes;
		fragS *frag_tmp;
		char *frag_opcode;

		extern const md_short_jump_size;
		extern const md_long_jump_size;
		possible_bytes=md_short_jump_size + new_broken_words * md_long_jump_size;

		frag_tmp=frag_now;
		frag_opcode=frag_var(rs_broken_word,
				     possible_bytes,
				     possible_bytes,
				     (relax_substateT) 0,
				     (symbolS *) broken_words,
				     0L,
				     NULL);

		/* We want to store the pointer to where to insert the jump table in the
		   fr_opcode of the rs_broken_word frag.  This requires a little hackery */
		while (frag_tmp && (frag_tmp->fr_type != rs_broken_word || frag_tmp->fr_opcode))
		    frag_tmp=frag_tmp->fr_next;
		know(frag_tmp);
		frag_tmp->fr_opcode=frag_opcode;
		new_broken_words = 0;

		for (a=broken_words;a && a->dispfrag == 0;a=a->next_broken_word)
		    a->dispfrag=frag_tmp;
	}
#endif
	if ((symbolP = symbol_find(sym_name)) != 0) {
#ifdef	OBJ_VMS
		/*
		 *	If the new symbol is .comm AND it has a size of zero,
		 *	we ignore it (i.e. the old symbol overrides it)
		 */
		if ((SEGMENT_TO_SYMBOL_TYPE((int) now_seg) == (N_UNDF | N_EXT)) &&
		    ((obstack_next_free(& frags) - frag_now->fr_literal) == 0))
		    return;
		/*
		 *	If the old symbol is .comm and it has a size of zero,
		 *	we override it with the new symbol value.
		 */
	  if (S_IS_EXTERNAL(symbolP) &&  S_IS_DEFINED(symbolP)
		    && (S_GET_VALUE(symbolP) == 0)) {
			symbolP->sy_frag  = frag_now;
			S_GET_OTHER(symbolP) = const_flag;
			S_SET_VALUE(symbolP, obstack_next_free(& frags) - frag_now->fr_literal);
			symbolP->sy_symbol.n_type |=
			  SEGMENT_TO_SYMBOL_TYPE((int) now_seg); /* keep N_EXT bit */
			return;
		}
#endif	/* OBJ_VMS */
		/*
		 *	Now check for undefined symbols
		 */
		if (!S_IS_DEFINED(symbolP)) {
			if (S_GET_VALUE(symbolP) == 0) {
				symbolP->sy_frag  = frag_now;
#ifdef OBJ_VMS
			  S_GET_OTHER(symbolP) = const_flag;
#endif
				S_SET_VALUE(symbolP, obstack_next_free(&frags) - frag_now->fr_literal);
				S_SET_SEGMENT(symbolP, now_seg);
#ifdef N_UNDF
				know(N_UNDF == 0);
#endif /* if we have one, it better be zero. */

			} else {
				/*
				 *	There are still several cases to check:
				 *		A .comm/.lcomm symbol being redefined as
				 *			initialized data is OK
				 *		A .comm/.lcomm symbol being redefined with
				 *			a larger size is also OK
				 *
				 * This only used to be allowed on VMS gas, but Sun cc
				 * on the sparc also depends on it.
				 */
				/*			  char New_Type = SEGMENT_TO_SYMBOL_TYPE((int) now_seg); */

				if (((!S_IS_DEBUG(symbolP) && !S_IS_DEFINED(symbolP) && S_IS_EXTERNAL(symbolP))
				     || (S_GET_SEGMENT(symbolP) == SEG_BSS))
				    && ((now_seg == SEG_DATA)
					|| (now_seg == S_GET_SEGMENT(symbolP)))) {
					/*
					 *	Select which of the 2 cases this is
					 */
					if (now_seg != SEG_DATA) {
						/*
						 *   New .comm for prev .comm symbol.
						 *	If the new size is larger we just
						 *	change its value.  If the new size
						 *	is smaller, we ignore this symbol
						 */
						if (S_GET_VALUE(symbolP)
						    < ((unsigned) (obstack_next_free(& frags) - frag_now->fr_literal))) {
							S_SET_VALUE(symbolP,
								    obstack_next_free(& frags) -
								    frag_now->fr_literal);
						}
					} else {
						/*
						 *	It is a .comm/.lcomm being converted
						 *	to initialized data.
						 */
						symbolP->sy_frag  = frag_now;
#ifdef OBJ_VMS
					  S_GET_OTHER(symbolP) = const_flag;
#endif /* OBJ_VMS */
						S_SET_VALUE(symbolP, obstack_next_free(& frags) - frag_now->fr_literal);
						S_SET_SEGMENT(symbolP, now_seg); /* keep N_EXT bit */
					}
				} else {
#ifdef OBJ_COFF
					as_fatal("Symbol \"%s\" is already defined as \"%s\"/%d.",
						 sym_name,
						 segment_name(S_GET_SEGMENT(symbolP)),
						 S_GET_VALUE(symbolP));
#else /* OBJ_COFF */
					as_fatal("Symbol \"%s\" is already defined as \"%s\"/%d.%d.%d.",
						 sym_name,
						 segment_name(S_GET_SEGMENT(symbolP)),
						 S_GET_OTHER(symbolP), S_GET_DESC(symbolP),
						 S_GET_VALUE(symbolP));
#endif /* OBJ_COFF */
				}
			} /* if the undefined symbol has no value */
		} else
		    {
			    /* Don't blow up if the definition is the same */
			    if (!(frag_now == symbolP->sy_frag
				  && S_GET_VALUE(symbolP) == obstack_next_free(&frags) - frag_now->fr_literal
				  && S_GET_SEGMENT(symbolP) == now_seg) )
				as_fatal("Symbol %s already defined.", sym_name);
		    } /* if this symbol is not yet defined */

	} else {
		symbolP = symbol_new(sym_name,
				     now_seg,
				     (valueT)(obstack_next_free(&frags)-frag_now->fr_literal),
				     frag_now);
#ifdef OBJ_VMS
		S_SET_OTHER(symbolP, const_flag);
#endif /* OBJ_VMS */

		symbol_table_insert(symbolP);
	} /* if we have seen this symbol before */

	return;
} /* colon() */


/*
 *			symbol_table_insert()
 *
 * Die if we can't insert the symbol.
 *
 */

void symbol_table_insert(symbolP)
symbolS *symbolP;
{
	register char *error_string;

	know(symbolP);
	know(S_GET_NAME(symbolP));

	if (*(error_string = hash_jam(sy_hash, S_GET_NAME(symbolP), (char *)symbolP))) {
		as_fatal("Inserting \"%s\" into symbol table failed: %s",
			 S_GET_NAME(symbolP), error_string);
	} /* on error */
} /* symbol_table_insert() */

/*
 *			symbol_find_or_make()
 *
 * If a symbol name does not exist, create it as undefined, and insert
 * it into the symbol table. Return a pointer to it.
 */
symbolS *symbol_find_or_make(name)
char *name;
{
	register symbolS *symbolP;

	symbolP = symbol_find(name);

	if (symbolP == NULL) {
		symbolP = symbol_make(name);

		symbol_table_insert(symbolP);
	} /* if symbol wasn't found */

	return(symbolP);
} /* symbol_find_or_make() */

symbolS *symbol_make(name)
char *name;
{
	symbolS *symbolP;

	/* Let the machine description default it, e.g. for register names. */
	symbolP = md_undefined_symbol(name);

	if (!symbolP) {
		symbolP = symbol_new(name,
				     SEG_UNKNOWN,
				     0,
				     &zero_address_frag);
	} /* if md didn't build us a symbol */

	return(symbolP);
} /* symbol_make() */

/*
 *			symbol_find()
 *
 * Implement symbol table lookup.
 * In:	A symbol's name as a string: '\0' can't be part of a symbol name.
 * Out:	NULL if the name was not in the symbol table, else the address
 *	of a struct symbol associated with that name.
 */

symbolS *symbol_find(name)
char *name;
{
#ifdef STRIP_UNDERSCORE
	return(symbol_find_base(name, 1));
#else /* STRIP_UNDERSCORE */
	return(symbol_find_base(name, 0));
#endif /* STRIP_UNDERSCORE */
} /* symbol_find() */

symbolS *symbol_find_base(name, strip_underscore)
char *name;
int strip_underscore;
{
	if (strip_underscore && *name == '_') name++;
	return ( (symbolS *) hash_find( sy_hash, name ));
}

/*
 * Once upon a time, symbols were kept in a singly linked list.  At
 * least coff needs to be able to rearrange them from time to time, for
 * which a doubly linked list is much more convenient.  Loic did these
 * as macros which seemed dangerous to me so they're now functions.
 * xoxorich.
 */

/* Link symbol ADDME after symbol TARGET in the chain. */
void symbol_append(addme, target, rootPP, lastPP)
symbolS *addme;
symbolS *target;
symbolS **rootPP;
symbolS **lastPP;
{
	if (target == NULL) {
		know(*rootPP == NULL);
		know(*lastPP == NULL);
		*rootPP = addme;
		*lastPP = addme;
		return;
	} /* if the list is empty */

	if (target->sy_next != NULL) {
#ifdef SYMBOLS_NEED_BACKPOINTERS
		target->sy_next->sy_previous = addme;
#endif /* SYMBOLS_NEED_BACKPOINTERS */
	} else {
		know(*lastPP == target);
		*lastPP = addme;
	} /* if we have a next */

	addme->sy_next = target->sy_next;
	target->sy_next = addme;

#ifdef SYMBOLS_NEED_BACKPOINTERS
	addme->sy_previous = target;
#endif /* SYMBOLS_NEED_BACKPOINTERS */

#ifdef DEBUG
	/*	verify_symbol_chain(*rootPP, *lastPP); */
#endif /* DEBUG */

	return;
} /* symbol_append() */

#ifdef SYMBOLS_NEED_BACKPOINTERS
/* Remove SYMBOLP from the list. */
void symbol_remove(symbolP, rootPP, lastPP)
symbolS *symbolP;
symbolS **rootPP;
symbolS **lastPP;
{
	if (symbolP == *rootPP) {
		*rootPP = symbolP->sy_next;
	} /* if it was the root */

	if (symbolP == *lastPP) {
		*lastPP = symbolP->sy_previous;
	} /* if it was the tail */

	if (symbolP->sy_next != NULL) {
		symbolP->sy_next->sy_previous = symbolP->sy_previous;
	} /* if not last */

	if (symbolP->sy_previous != NULL) {
		symbolP->sy_previous->sy_next = symbolP->sy_next;
	} /* if not first */

#ifdef DEBUG
	verify_symbol_chain(*rootPP, *lastPP);
#endif /* DEBUG */

	return;
} /* symbol_remove() */

/* Set the chain pointers of SYMBOL to null. */
void symbol_clear_list_pointers(symbolP)
symbolS *symbolP;
{
	symbolP->sy_next = NULL;
	symbolP->sy_previous = NULL;
} /* symbol_clear_list_pointers() */

/* Link symbol ADDME before symbol TARGET in the chain. */
void symbol_insert(addme, target, rootPP, lastPP)
symbolS *addme;
symbolS *target;
symbolS **rootPP;
symbolS **lastPP;
{
	if (target->sy_previous != NULL) {
		target->sy_previous->sy_next = addme;
	} else {
		know(*rootPP == target);
		*rootPP = addme;
	} /* if not first */

	addme->sy_previous = target->sy_previous;
	target->sy_previous = addme;
	addme->sy_next = target;

#ifdef DEBUG
	verify_symbol_chain(*rootPP, *lastPP);
#endif /* DEBUG */

	return;
} /* symbol_insert() */
#endif /* SYMBOLS_NEED_BACKPOINTERS */

void verify_symbol_chain(rootP, lastP)
symbolS *rootP;
symbolS *lastP;
{
	symbolS *symbolP = rootP;

	if (symbolP == NULL) {
		return;
	} /* empty chain */

	for ( ; symbol_next(symbolP) != NULL; symbolP = symbol_next(symbolP)) {
#ifdef SYMBOLS_NEED_BACKPOINTERS
		/*$if (symbolP->sy_previous) {
		  know(symbolP->sy_previous->sy_next == symbolP);
		  } else {
		  know(symbolP == rootP);
		  }$*/ /* both directions */
		know(symbolP->sy_next->sy_previous == symbolP);
#else /* SYMBOLS_NEED_BACKPOINTERS */
		;
#endif /* SYMBOLS_NEED_BACKPOINTERS */
	} /* verify pointers */

	know(lastP == symbolP);

	return;
} /* verify_symbol_chain() */


/*
 * decode name that may have been generated by local_label_name() above.  If
 * the name wasn't generated by local_label_name(), then return it unaltered.
 * This is used for error messages.
 */

char *decode_local_label_name(s)
char *s;
{
 	char *symbol_decode;
 	int label_number;
	/*	int label_version; */
 	char *message_format = "\"%d\" (instance number %s of a local label)";

 	if (s[0] != 'L'
 	    || s[2] != 1) {
 		return(s);
 	} /* not a local_label_name() generated name. */

 	label_number = s[1] - '0';

	(void) sprintf(symbol_decode = obstack_alloc(&notes, strlen(s + 3) + strlen(message_format) + 10),
		       message_format, label_number, s + 3);

	return(symbol_decode);
} /* decode_local_label_name() */


/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of symbols.c */
