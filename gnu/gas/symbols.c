/* symbols.c -symbol table-
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


#include "as.h"
#include "hash.h"
#include "obstack.h"		/* For "symbols.h" */
#include "struc-symbol.h"
#include "symbols.h"
#include "frags.h"

#ifndef WORKING_DOT_WORD
extern int new_broken_words;
#endif
#ifdef VMS
extern char const_flag;
#endif

static
struct hash_control *
sy_hash;			/* symbol-name => struct symbol pointer */

				/* Below are commented in "symbols.h". */
unsigned int local_bss_counter;
symbolS * symbol_rootP;
symbolS * symbol_lastP;
symbolS	abs_symbol;
struct obstack	notes;



symbolS * symbol_find();	/* Keep C compiler happy. */

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

#ifdef SUN_ASM_SYNTAX
int local_label_defined[10];
#endif


void
symbol_begin()
{
  symbol_lastP = NULL;
  symbol_rootP = NULL;		/* In case we have 0 symbols (!!) */
  sy_hash = hash_new();
  bzero ((char *)(& abs_symbol), sizeof(abs_symbol));
  abs_symbol . sy_type = N_ABS;	/* Can't initialise a union. Sigh. */
  bzero ((char *)(local_label_counter), sizeof(local_label_counter) );
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
  * p ++ = 'L';
  * p ++ = n + '0';		/* Make into ASCII */
  * p ++ = 1;			/* ^A */
  n = local_label_counter [ n ] + augend;
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
  while ( * p ++ = * -- q )
    {
    }
  /* The label, as a '\0' ended string, starts at symbol_name_build. */
  return (symbol_name_build);
}


void
local_colon (n)
     int		n;	/* just saw "n:" */
{
  local_label_counter [n] ++;
#ifdef SUN_ASM_SYNTAX
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
 */

symbolS *
symbol_new (name, type, other, desc, value, frag)
     char *		name;	/* We copy this: OK to alter your copy. */
     unsigned char	type;	/* As in <a.out.h>. */
     char		other;	/* As in <a.out.h>. */
     short int		desc;	/* As in <a.out.h>. */
     valueT		value;	/* As in <a.out.h>, often an address. */
				/* Often used as offset from frag address. */
     struct frag *	frag;	/* For sy_frag. */
{
  register symbolS *		symbolP;
  register char *		preserved_copy_of_name;
  register unsigned int		name_length;
           char *		p;

  name_length = strlen(name) + 1;
  obstack_grow(&notes,name,name_length);
  p=obstack_finish(&notes);
  /* obstack_1done( &notes, name, name_length, &p ); */
  preserved_copy_of_name = p;
  p=obstack_alloc(&notes,sizeof(struct symbol));
  /* obstack_1blank( &notes, sizeof(struct symbol), &p ); */
  symbolP			= (symbolS *) p;
  symbolP -> sy_name		= preserved_copy_of_name;
  symbolP -> sy_type		= type;
  symbolP -> sy_other		= other;
  symbolP -> sy_desc		= desc;
  symbolP -> sy_value		= value;
  symbolP -> sy_frag		= frag;
  symbolP -> sy_next		= NULL;	/* End of chain. */
  symbolP -> sy_forward		= NULL; /* JF */
#ifdef SUSPECT
  symbolP -> sy_name_offset	= ~ 0; /* Impossible offset catches errors. */
  symbolP -> sy_number		= ~ 0; /* Ditto. */
#endif
  /*
   * Link to end of symbol chain.
   */
  if (symbol_lastP)
    {
      symbol_lastP -> sy_next = symbolP;
    }
  else
    {
      symbol_rootP = symbolP;
    }
  symbol_lastP = symbolP;

  return (symbolP);
}

/*
 *			colon()
 *
 * We have just seen "<name>:".
 * Creates a struct symbol unless it already exists.
 *
 * Gripes if we are redefining a symbol incompatibly (and ignores it).
 *
 */
void
colon (sym_name)		/* just seen "x:" - rattle symbols & frags */
     register char *  sym_name; /* symbol name, as a cannonical string */
				/* We copy this string: OK to alter later. */
{
  register struct symbol * symbolP; /* symbol we are working with */

#ifdef SUN_ASM_SYNTAX
  /* Sun local labes go out of scope whenever a non-local symbol is
     defined.  */

  if(*sym_name !='L')
    bzero((void *)local_label_defined,sizeof(local_label_defined));
#endif

#ifndef WORKING_DOT_WORD
  if(new_broken_words) {
    struct broken_word *a;
    int possible_bytes;
    fragS *frag_tmp;
    char *frag_opcode;
    extern md_short_jump_size;
    extern md_long_jump_size;

    possible_bytes=md_short_jump_size+new_broken_words*md_long_jump_size;
    frag_tmp=frag_now;
    frag_opcode=frag_var(rs_broken_word,possible_bytes,possible_bytes,(relax_substateT)0,(symbolS *)broken_words,(long int)0,(char *)0);

    /* We want to store the pointer to where to insert the jump table in the
       fr_opcode of the rs_broken_word frag.  This requires a little hackery */
    while(frag_tmp && (frag_tmp->fr_type!=rs_broken_word || frag_tmp->fr_opcode))
      frag_tmp=frag_tmp->fr_next;
    know(frag_tmp);
    frag_tmp->fr_opcode=frag_opcode;
    new_broken_words = 0;

    for(a=broken_words;a && a->dispfrag==0;a=a->next_broken_word)
      a->dispfrag=frag_tmp;
  }
#endif
  if (symbolP = symbol_table_lookup( sym_name ))
    {
#ifdef	VMS
      /*
       *	If the new symbol is .comm AND it has a size of zero,
       *	we ignore it (i.e. the old symbol overrides it)
       */
      if ((seg_N_TYPE [(int) now_seg] == (N_UNDF | N_EXT)) &&
	  ((obstack_next_free(& frags) - frag_now -> fr_literal) == 0))
		return;
      /*
       *	If the old symbol is .comm and it has a size of zero,
       *	we override it with the new symbol value.
       */
      if ((symbolP -> sy_type == (N_UNDF | N_EXT)) &&
	  (symbolP->sy_value == 0)) {
	      symbolP -> sy_frag  = frag_now;
	      symbolP -> sy_other = const_flag;
	      symbolP -> sy_value = obstack_next_free(& frags) - frag_now -> fr_literal;
	      symbolP -> sy_type |= seg_N_TYPE [(int) now_seg]; /* keep N_EXT bit */
	      return;
      }
#endif	/* VMS */
      /*
       *	Now check for undefined symbols
       */
      if ((symbolP -> sy_type & N_TYPE) == N_UNDF)
	{
	  if(   symbolP -> sy_other == 0
	     && symbolP -> sy_desc  == 0
	     && symbolP -> sy_value == 0)
	    {
	      symbolP -> sy_frag  = frag_now;
#ifdef VMS
		symbolP -> sy_other = const_flag;
#endif
	      symbolP -> sy_value = obstack_next_free(& frags) - frag_now -> fr_literal;
	      know( N_UNDF == 0 );
	      symbolP -> sy_type |= seg_N_TYPE [(int) now_seg]; /* keep N_EXT bit */
	    }
	  else
	    {
#ifdef	VMS
	      /*
	       *	There are still several cases to check:
	       *		A .comm/.lcomm symbol being redefined as
	       *			initialized data is OK
	       *		A .comm/.lcomm symbol being redefined with
	       *			a larger size is also OK
	       */
	      char New_Type = seg_N_TYPE [(int) now_seg];
	      if (((symbolP->sy_type == (N_UNDF | N_EXT)) ||
		   (symbolP->sy_type == N_BSS)) &&
		  (((New_Type & ~N_EXT) == N_DATA) ||
		   (New_Type == symbolP->sy_type))) {
			/*
			 *	Select which of the 2 cases this is
			 */
			if (New_Type == symbolP->sy_type) {
				/*
				 *	If the new size is larger we just
				 *	change its value.  If the new size
				 *	is smaller, we ignore this symbol
				 */
				if (symbolP->sy_value <
					(obstack_next_free(& frags) -
						frag_now -> fr_literal)) {
				      symbolP -> sy_value = 
					obstack_next_free(& frags) -
						frag_now -> fr_literal;
				}
			} else {
				/*
				 *	It is a .comm/.lcomm being converted
				 *	to initialized data.
				 */
				symbolP -> sy_frag  = frag_now;
				symbolP -> sy_other = const_flag;
				symbolP -> sy_value = obstack_next_free(& frags) - frag_now -> fr_literal;
				symbolP -> sy_type |= seg_N_TYPE [(int) now_seg]; /* keep N_EXT bit */
			}
	      } else {
#endif	/* VMS */
	      as_fatal( "Symbol \"%s\" is already defined as \"%s\"/%d.%d.%d.",
		      sym_name,
		      seg_name [(int) N_TYPE_seg [symbolP -> sy_type & N_TYPE]],
		      symbolP -> sy_other, symbolP -> sy_desc,
		      symbolP -> sy_value);
#ifdef	VMS
		}
#endif	/* VMS */
	    }
	}
      else
	{
	  as_fatal("Symbol %s already defined.",sym_name);
	}
    }
  else
    {
      symbolP = symbol_new (sym_name,
			    (unsigned char)(seg_N_TYPE [(int) now_seg]),
#ifdef VMS
			    const_flag,
#else
			    0,
#endif
			    0,
			    (valueT)(obstack_next_free(&frags)-frag_now->fr_literal),
			    frag_now);
      symbol_table_insert (symbolP);
    }
}


/*
 *			symbol_table_insert()
 *
 * Die if we can't insert the symbol.
 *
 */

void
symbol_table_insert (symbolP)
     struct symbol *	symbolP;
{
  register char *	error_string;

  know( symbolP );
  know( symbolP -> sy_name );
  if ( * (error_string = hash_jam (sy_hash, symbolP -> sy_name, (char *)symbolP)))
    {
      as_fatal( "Inserting \"%s\" into symbol table failed: %s",
	      symbolP -> sy_name, error_string);
    }
}

/*
 *			symbol_find_or_make()
 *
 * If a symbol name does not exist, create it as undefined, and insert
 * it into the symbol table. Return a pointer to it.
 */
symbolS *
symbol_find_or_make (name)
     char *	name;
{
  register symbolS *	symbolP;

  symbolP = symbol_table_lookup (name);
  if (symbolP == NULL)
    {
      symbolP = symbol_new (name, N_UNDF, 0, 0, 0, & zero_address_frag);
      symbol_table_insert (symbolP);
    }
  return (symbolP);
}

/*
 *			symbol_find()
 * 
 * Implement symbol table lookup.
 * In:	A symbol's name as a string: '\0' can't be part of a symbol name.
 * Out:	NULL if the name was not in the symbol table, else the address
 *	of a struct symbol associated with that name.
 */

symbolS *
symbol_find (name)
     char *	name;
{
  return ( (symbolS *) hash_find( sy_hash, name ));
}


/* end: symbols.c */
