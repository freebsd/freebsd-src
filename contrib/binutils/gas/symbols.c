/* symbols.c -symbol table-
   Copyright (C) 1987, 90, 91, 92, 93, 94, 95, 96, 97, 1998
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

/* #define DEBUG_SYMS / * to debug symbol list maintenance */

#include <ctype.h>

#include "as.h"

#include "obstack.h"		/* For "symbols.h" */
#include "subsegs.h"

/* This is non-zero if symbols are case sensitive, which is the
   default.  */
int symbols_case_sensitive = 1;

#ifndef WORKING_DOT_WORD
extern int new_broken_words;
#endif

/* symbol-name => struct symbol pointer */
static struct hash_control *sy_hash;

/* Below are commented in "symbols.h". */
symbolS *symbol_rootP;
symbolS *symbol_lastP;
symbolS abs_symbol;

#ifdef DEBUG_SYMS
#define debug_verify_symchain verify_symbol_chain
#else
#define debug_verify_symchain(root, last) ((void) 0)
#endif

struct obstack notes;

static void fb_label_init PARAMS ((void));
static long dollar_label_instance PARAMS ((long));
static long fb_label_instance PARAMS ((long));

/* symbol_new()
  
   Return a pointer to a new symbol.  Die if we can't make a new
   symbol.  Fill in the symbol's values.  Add symbol to end of symbol
   chain.
 
   This function should be called in the general case of creating a
   symbol.  However, if the output file symbol table has already been
   set, and you are certain that this symbol won't be wanted in the
   output file, you can call symbol_create.  */

symbolS *
symbol_new (name, segment, valu, frag)
     const char *name;
     segT segment;
     valueT valu;
     fragS *frag;
{
  symbolS *symbolP = symbol_create (name, segment, valu, frag);

  /*
   * Link to end of symbol chain.
   */
#ifdef BFD_ASSEMBLER
  {
    extern int symbol_table_frozen;
    if (symbol_table_frozen)
      abort ();
  }
#endif
  symbol_append (symbolP, symbol_lastP, &symbol_rootP, &symbol_lastP);

  return symbolP;
}

symbolS *
symbol_create (name, segment, valu, frag)
     const char *name;		/* It is copied, the caller can destroy/modify */
     segT segment;		/* Segment identifier (SEG_<something>) */
     valueT valu;		/* Symbol value */
     fragS *frag;		/* Associated fragment */
{
  unsigned int name_length;
  char *preserved_copy_of_name;
  symbolS *symbolP;

  name_length = strlen (name) + 1;	/* +1 for \0 */
  obstack_grow (&notes, name, name_length);
  preserved_copy_of_name = obstack_finish (&notes);
#ifdef STRIP_UNDERSCORE
  if (preserved_copy_of_name[0] == '_')
    preserved_copy_of_name++;
#endif

#ifdef tc_canonicalize_symbol_name
  preserved_copy_of_name =
    tc_canonicalize_symbol_name (preserved_copy_of_name);
#endif

  if (! symbols_case_sensitive)
    {
      unsigned char *s;

      for (s = (unsigned char *) preserved_copy_of_name; *s != '\0'; s++)
	if (islower (*s))
	  *s = toupper (*s);
    }

  symbolP = (symbolS *) obstack_alloc (&notes, sizeof (symbolS));

  /* symbol must be born in some fixed state.  This seems as good as any. */
  memset (symbolP, 0, sizeof (symbolS));

#ifdef BFD_ASSEMBLER
  symbolP->bsym = bfd_make_empty_symbol (stdoutput);
  if (symbolP->bsym == NULL)
    as_perror ("%s", "bfd_make_empty_symbol");
  symbolP->bsym->udata.p = (PTR) symbolP;
#endif
  S_SET_NAME (symbolP, preserved_copy_of_name);

  S_SET_SEGMENT (symbolP, segment);
  S_SET_VALUE (symbolP, valu);
  symbol_clear_list_pointers (symbolP);

  symbolP->sy_frag = frag;
#ifndef BFD_ASSEMBLER
  symbolP->sy_number = ~0;
  symbolP->sy_name_offset = (unsigned int) ~0;
#endif

  obj_symbol_new_hook (symbolP);

#ifdef tc_symbol_new_hook
  tc_symbol_new_hook (symbolP);
#endif

  return symbolP;
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
symbolS *
colon (sym_name)		/* just seen "x:" - rattle symbols & frags */
     const char *sym_name;	/* symbol name, as a cannonical string */
     /* We copy this string: OK to alter later. */
{
  register symbolS *symbolP;	/* symbol we are working with */

  /* Sun local labels go out of scope whenever a non-local symbol is
     defined.  */
  if (LOCAL_LABELS_DOLLAR)
    {
      int local;

#ifdef BFD_ASSEMBLER
      local = bfd_is_local_label_name (stdoutput, sym_name);
#else
      local = LOCAL_LABEL (sym_name);
#endif

      if (! local)
	dollar_label_clear ();
    }

#ifndef WORKING_DOT_WORD
  if (new_broken_words)
    {
      struct broken_word *a;
      int possible_bytes;
      fragS *frag_tmp;
      char *frag_opcode;

      extern const int md_short_jump_size;
      extern const int md_long_jump_size;
      possible_bytes = (md_short_jump_size
			+ new_broken_words * md_long_jump_size);

      frag_tmp = frag_now;
      frag_opcode = frag_var (rs_broken_word,
			      possible_bytes,
			      possible_bytes,
			      (relax_substateT) 0,
			      (symbolS *) broken_words,
			      (offsetT) 0,
			      NULL);

      /* We want to store the pointer to where to insert the jump table in the
	 fr_opcode of the rs_broken_word frag.  This requires a little
	 hackery.  */
      while (frag_tmp
	     && (frag_tmp->fr_type != rs_broken_word
		 || frag_tmp->fr_opcode))
	frag_tmp = frag_tmp->fr_next;
      know (frag_tmp);
      frag_tmp->fr_opcode = frag_opcode;
      new_broken_words = 0;

      for (a = broken_words; a && a->dispfrag == 0; a = a->next_broken_word)
	a->dispfrag = frag_tmp;
    }
#endif /* WORKING_DOT_WORD */

  if ((symbolP = symbol_find (sym_name)) != 0)
    {
#ifdef RESOLVE_SYMBOL_REDEFINITION
      if (RESOLVE_SYMBOL_REDEFINITION (symbolP))
	return symbolP;
#endif
      /*
       *	Now check for undefined symbols
       */
      if (!S_IS_DEFINED (symbolP) || S_IS_COMMON (symbolP))
	{
	  if (S_GET_VALUE (symbolP) == 0)
	    {
	      symbolP->sy_frag = frag_now;
#ifdef OBJ_VMS
	      S_SET_OTHER(symbolP, const_flag);
#endif
	      S_SET_VALUE (symbolP, (valueT) frag_now_fix ());
	      S_SET_SEGMENT (symbolP, now_seg);
#ifdef N_UNDF
	      know (N_UNDF == 0);
#endif /* if we have one, it better be zero. */

	    }
	  else
	    {
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

	      if (((!S_IS_DEBUG (symbolP)
		    && (!S_IS_DEFINED (symbolP) || S_IS_COMMON (symbolP))
		    && S_IS_EXTERNAL (symbolP))
		   || S_GET_SEGMENT (symbolP) == bss_section)
		  && (now_seg == data_section
		      || now_seg == S_GET_SEGMENT (symbolP)))
		{
		  /*
		   *	Select which of the 2 cases this is
		   */
		  if (now_seg != data_section)
		    {
		      /*
		       *   New .comm for prev .comm symbol.
		       *	If the new size is larger we just
		       *	change its value.  If the new size
		       *	is smaller, we ignore this symbol
		       */
		      if (S_GET_VALUE (symbolP)
			  < ((unsigned) frag_now_fix ()))
			{
			  S_SET_VALUE (symbolP, (valueT) frag_now_fix ());
			}
		    }
		  else
		    {
		      /* It is a .comm/.lcomm being converted to initialized
			 data.  */
		      symbolP->sy_frag = frag_now;
#ifdef OBJ_VMS
		      S_SET_OTHER(symbolP, const_flag);
#endif
		      S_SET_VALUE (symbolP, (valueT) frag_now_fix ());
		      S_SET_SEGMENT (symbolP, now_seg);	/* keep N_EXT bit */
		    }
		}
	      else
		{
#if defined (S_GET_OTHER) && defined (S_GET_DESC)
		  as_fatal ("Symbol \"%s\" is already defined as \"%s\"/%d.%d.%ld.",
			    sym_name,
			    segment_name (S_GET_SEGMENT (symbolP)),
			    S_GET_OTHER (symbolP), S_GET_DESC (symbolP),
			    (long) S_GET_VALUE (symbolP));
#else
		  as_fatal ("Symbol \"%s\" is already defined as \"%s\"/%ld.",
			    sym_name,
			    segment_name (S_GET_SEGMENT (symbolP)),
			    (long) S_GET_VALUE (symbolP));
#endif
		}
	    }			/* if the undefined symbol has no value */
	}
      else
	{
	  /* Don't blow up if the definition is the same */
	  if (!(frag_now == symbolP->sy_frag
		&& S_GET_VALUE (symbolP) == frag_now_fix ()
		&& S_GET_SEGMENT (symbolP) == now_seg))
	    as_fatal ("Symbol %s already defined.", sym_name);
	}			/* if this symbol is not yet defined */

    }
  else
    {
      symbolP = symbol_new (sym_name, now_seg, (valueT) frag_now_fix (),
			    frag_now);
#ifdef OBJ_VMS
      S_SET_OTHER (symbolP, const_flag);
#endif /* OBJ_VMS */

      symbol_table_insert (symbolP);
    }				/* if we have seen this symbol before */

  if (mri_common_symbol != NULL)
    {
      /* This symbol is actually being defined within an MRI common
         section.  This requires special handling.  */
      symbolP->sy_value.X_op = O_symbol;
      symbolP->sy_value.X_add_symbol = mri_common_symbol;
      symbolP->sy_value.X_add_number = S_GET_VALUE (mri_common_symbol);
      symbolP->sy_frag = &zero_address_frag;
      S_SET_SEGMENT (symbolP, expr_section);
      symbolP->sy_mri_common = 1;
    }

#ifdef tc_frob_label
  tc_frob_label (symbolP);
#endif
#ifdef obj_frob_label
  obj_frob_label (symbolP);
#endif

  return symbolP;
}


/*
 *			symbol_table_insert()
 *
 * Die if we can't insert the symbol.
 *
 */

void 
symbol_table_insert (symbolP)
     symbolS *symbolP;
{
  register const char *error_string;

  know (symbolP);
  know (S_GET_NAME (symbolP));

  if ((error_string = hash_jam (sy_hash, S_GET_NAME (symbolP), (PTR) symbolP)))
    {
      as_fatal ("Inserting \"%s\" into symbol table failed: %s",
		S_GET_NAME (symbolP), error_string);
    }				/* on error */
}				/* symbol_table_insert() */

/*
 *			symbol_find_or_make()
 *
 * If a symbol name does not exist, create it as undefined, and insert
 * it into the symbol table. Return a pointer to it.
 */
symbolS *
symbol_find_or_make (name)
     const char *name;
{
  register symbolS *symbolP;

  symbolP = symbol_find (name);

  if (symbolP == NULL)
    {
      symbolP = symbol_make (name);

      symbol_table_insert (symbolP);
    }				/* if symbol wasn't found */

  return (symbolP);
}				/* symbol_find_or_make() */

symbolS *
symbol_make (name)
     CONST char *name;
{
  symbolS *symbolP;

  /* Let the machine description default it, e.g. for register names. */
  symbolP = md_undefined_symbol ((char *) name);

  if (!symbolP)
    symbolP = symbol_new (name, undefined_section, (valueT) 0, &zero_address_frag);

  return (symbolP);
}				/* symbol_make() */

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
     CONST char *name;
{
#ifdef STRIP_UNDERSCORE
  return (symbol_find_base (name, 1));
#else /* STRIP_UNDERSCORE */
  return (symbol_find_base (name, 0));
#endif /* STRIP_UNDERSCORE */
}				/* symbol_find() */

symbolS *
symbol_find_base (name, strip_underscore)
     CONST char *name;
     int strip_underscore;
{
  if (strip_underscore && *name == '_')
    name++;

#ifdef tc_canonicalize_symbol_name
  {
    char *copy;

    copy = (char *) alloca (strlen (name) + 1);
    strcpy (copy, name);
    name = tc_canonicalize_symbol_name (copy);
  }
#endif

  if (! symbols_case_sensitive)
    {
      unsigned char *copy;

      copy = (unsigned char *) alloca (strlen (name) + 1);
      name = (const char *) copy;
      for (; *copy != '\0'; copy++)
	if (islower (*copy))
	  *copy = toupper (*copy);
    }

  return ((symbolS *) hash_find (sy_hash, name));
}

/*
 * Once upon a time, symbols were kept in a singly linked list.  At
 * least coff needs to be able to rearrange them from time to time, for
 * which a doubly linked list is much more convenient.  Loic did these
 * as macros which seemed dangerous to me so they're now functions.
 * xoxorich.
 */

/* Link symbol ADDME after symbol TARGET in the chain. */
void 
symbol_append (addme, target, rootPP, lastPP)
     symbolS *addme;
     symbolS *target;
     symbolS **rootPP;
     symbolS **lastPP;
{
  if (target == NULL)
    {
      know (*rootPP == NULL);
      know (*lastPP == NULL);
      addme->sy_next = NULL;
#ifdef SYMBOLS_NEED_BACKPOINTERS
      addme->sy_previous = NULL;
#endif
      *rootPP = addme;
      *lastPP = addme;
      return;
    }				/* if the list is empty */

  if (target->sy_next != NULL)
    {
#ifdef SYMBOLS_NEED_BACKPOINTERS
      target->sy_next->sy_previous = addme;
#endif /* SYMBOLS_NEED_BACKPOINTERS */
    }
  else
    {
      know (*lastPP == target);
      *lastPP = addme;
    }				/* if we have a next */

  addme->sy_next = target->sy_next;
  target->sy_next = addme;

#ifdef SYMBOLS_NEED_BACKPOINTERS
  addme->sy_previous = target;
#endif /* SYMBOLS_NEED_BACKPOINTERS */

  debug_verify_symchain (symbol_rootP, symbol_lastP);
}

/* Set the chain pointers of SYMBOL to null. */
void 
symbol_clear_list_pointers (symbolP)
     symbolS *symbolP;
{
  symbolP->sy_next = NULL;
#ifdef SYMBOLS_NEED_BACKPOINTERS
  symbolP->sy_previous = NULL;
#endif
}

#ifdef SYMBOLS_NEED_BACKPOINTERS
/* Remove SYMBOLP from the list. */
void 
symbol_remove (symbolP, rootPP, lastPP)
     symbolS *symbolP;
     symbolS **rootPP;
     symbolS **lastPP;
{
  if (symbolP == *rootPP)
    {
      *rootPP = symbolP->sy_next;
    }				/* if it was the root */

  if (symbolP == *lastPP)
    {
      *lastPP = symbolP->sy_previous;
    }				/* if it was the tail */

  if (symbolP->sy_next != NULL)
    {
      symbolP->sy_next->sy_previous = symbolP->sy_previous;
    }				/* if not last */

  if (symbolP->sy_previous != NULL)
    {
      symbolP->sy_previous->sy_next = symbolP->sy_next;
    }				/* if not first */

  debug_verify_symchain (*rootPP, *lastPP);
}

/* Link symbol ADDME before symbol TARGET in the chain. */
void 
symbol_insert (addme, target, rootPP, lastPP)
     symbolS *addme;
     symbolS *target;
     symbolS **rootPP;
     symbolS **lastPP;
{
  if (target->sy_previous != NULL)
    {
      target->sy_previous->sy_next = addme;
    }
  else
    {
      know (*rootPP == target);
      *rootPP = addme;
    }				/* if not first */

  addme->sy_previous = target->sy_previous;
  target->sy_previous = addme;
  addme->sy_next = target;

  debug_verify_symchain (*rootPP, *lastPP);
}

#endif /* SYMBOLS_NEED_BACKPOINTERS */

void 
verify_symbol_chain (rootP, lastP)
     symbolS *rootP;
     symbolS *lastP;
{
  symbolS *symbolP = rootP;

  if (symbolP == NULL)
    return;

  for (; symbol_next (symbolP) != NULL; symbolP = symbol_next (symbolP))
    {
#ifdef SYMBOLS_NEED_BACKPOINTERS
      assert (symbolP->sy_next->sy_previous == symbolP);
#else
      /* Walk the list anyways, to make sure pointers are still good.  */
      ;
#endif /* SYMBOLS_NEED_BACKPOINTERS */
    }

  assert (lastP == symbolP);
}

void
verify_symbol_chain_2 (sym)
     symbolS *sym;
{
  symbolS *p = sym, *n = sym;
#ifdef SYMBOLS_NEED_BACKPOINTERS
  while (symbol_previous (p))
    p = symbol_previous (p);
#endif
  while (symbol_next (n))
    n = symbol_next (n);
  verify_symbol_chain (p, n);
}

/* Resolve the value of a symbol.  This is called during the final
   pass over the symbol table to resolve any symbols with complex
   values.  */

valueT
resolve_symbol_value (symp, finalize)
     symbolS *symp;
     int finalize;
{
  int resolved;
  valueT final_val;
  segT final_seg;

  if (symp->sy_resolved)
    {
      if (symp->sy_value.X_op == O_constant)
	return (valueT) symp->sy_value.X_add_number;
      else
	return 0;
    }

  resolved = 0;
  final_seg = S_GET_SEGMENT (symp);

  if (symp->sy_resolving)
    {
      if (finalize)
	as_bad ("Symbol definition loop encountered at %s", S_GET_NAME (symp));
      final_val = 0;
      resolved = 1;
    }
  else
    {
      symbolS *add_symbol, *op_symbol;
      offsetT left, right;
      segT seg_left, seg_right;
      operatorT op;

      symp->sy_resolving = 1;

      /* Help out with CSE.  */
      add_symbol = symp->sy_value.X_add_symbol;
      op_symbol = symp->sy_value.X_op_symbol;
      final_val = symp->sy_value.X_add_number;
      op = symp->sy_value.X_op;

      switch (op)
	{
	default:
	  BAD_CASE (op);
	  break;

	case O_absent:
	  final_val = 0;
	  /* Fall through.  */

	case O_constant:
	  final_val += symp->sy_frag->fr_address;
	  if (final_seg == expr_section)
	    final_seg = absolute_section;
	  resolved = 1;
	  break;

	case O_symbol:
	case O_symbol_rva:
	  left = resolve_symbol_value (add_symbol, finalize);
	do_symbol:

	  if (symp->sy_mri_common)
	    {
	      /* This is a symbol inside an MRI common section.  The
                 relocation routines are going to handle it specially.
                 Don't change the value.  */
	      resolved = add_symbol->sy_resolved;
	      break;
	    }

	  if (finalize && final_val == 0)
	    copy_symbol_attributes (symp, add_symbol);

	  /* If we have equated this symbol to an undefined symbol, we
             keep X_op set to O_symbol, and we don't change
             X_add_number.  This permits the routine which writes out
             relocation to detect this case, and convert the
             relocation to be against the symbol to which this symbol
             is equated.  */
	  if (! S_IS_DEFINED (add_symbol) || S_IS_COMMON (add_symbol))
	    {
	      if (finalize)
		{
		  symp->sy_value.X_op = O_symbol;
		  S_SET_SEGMENT (symp, S_GET_SEGMENT (add_symbol));
		  symp->sy_value.X_add_number = final_val;
		}
	      final_val = 0;
	      resolved = add_symbol->sy_resolved;
	      goto exit_dont_set_value;
	    }
	  else
	    {
	      final_val += symp->sy_frag->fr_address + left;
	      if (final_seg == expr_section || final_seg == undefined_section)
		final_seg = S_GET_SEGMENT (add_symbol);
	    }

	  resolved = add_symbol->sy_resolved;
	  break;

	case O_uminus:
	case O_bit_not:
	case O_logical_not:
	  left = resolve_symbol_value (add_symbol, finalize);

	  if (op == O_uminus)
	    left = -left;
	  else if (op == O_logical_not)
	    left = !left;
	  else
	    left = ~left;

	  final_val += left + symp->sy_frag->fr_address;
	  if (final_seg == expr_section || final_seg == undefined_section)
	    final_seg = absolute_section;

	  resolved = add_symbol->sy_resolved;
	  break;

	case O_multiply:
	case O_divide:
	case O_modulus:
	case O_left_shift:
	case O_right_shift:
	case O_bit_inclusive_or:
	case O_bit_or_not:
	case O_bit_exclusive_or:
	case O_bit_and:
	case O_add:
	case O_subtract:
	case O_eq:
	case O_ne:
	case O_lt:
	case O_le:
	case O_ge:
	case O_gt:
	case O_logical_and:
	case O_logical_or:
	  left = resolve_symbol_value (add_symbol, finalize);
	  right = resolve_symbol_value (op_symbol, finalize);
	  seg_left = S_GET_SEGMENT (add_symbol);
	  seg_right = S_GET_SEGMENT (op_symbol);

	  /* Simplify addition or subtraction of a constant by folding the
	     constant into X_add_number.  */
	  if (op == O_add || op == O_subtract)
	    {
	      if (seg_right == absolute_section)
		{
		  if (op == O_add)
		    final_val += right;
		  else
		    final_val -= right;
		  op = O_symbol;
		  op_symbol = NULL;
		  goto do_symbol;
		}
	      else if (seg_left == absolute_section && op == O_add)
		{
		  op = O_symbol;
		  final_val += left;
		  add_symbol = op_symbol;
		  left = right;
		  op_symbol = NULL;
		  goto do_symbol;
		}
	    }

	  /* Subtraction is permitted if both operands are in the same
	     section.  Otherwise, both operands must be absolute.  We
	     already handled the case of addition or subtraction of a
	     constant above.  This will probably need to be changed
	     for an object file format which supports arbitrary
	     expressions, such as IEEE-695.  */
	  /* Don't emit messages unless we're finalizing the symbol value,
	     otherwise we may get the same message multiple times.  */
	  if ((seg_left != absolute_section || seg_right != absolute_section)
	      && (op != O_subtract || seg_left != seg_right)
	      && finalize)
	    {
	      char *file;
	      unsigned int line;

	      if (expr_symbol_where (symp, &file, &line))
		{
		  if (seg_left == undefined_section)
		    as_bad_where (file, line,
				  "undefined symbol %s in operation",
				  S_GET_NAME (symp->sy_value.X_add_symbol));
		  if (seg_right == undefined_section)
		    as_bad_where (file, line,
				  "undefined symbol %s in operation",
				  S_GET_NAME (symp->sy_value.X_op_symbol));
		  if (seg_left != undefined_section
		      && seg_right != undefined_section)
		    as_bad_where (file, line, "invalid section for operation");
		}
	      else
		{
		  if (seg_left == undefined_section)
		    as_bad ("undefined symbol %s in operation setting %s",
			    S_GET_NAME (symp->sy_value.X_add_symbol),
			    S_GET_NAME (symp));
		  if (seg_right == undefined_section)
		    as_bad ("undefined symbol %s in operation setting %s",
			    S_GET_NAME (symp->sy_value.X_op_symbol),
			    S_GET_NAME (symp));
		  if (seg_left != undefined_section
		      && seg_right != undefined_section)
		    as_bad ("invalid section for operation setting %s",
			    S_GET_NAME (symp));
		}
	    }

	  /* Check for division by zero.  */
	  if ((op == O_divide || op == O_modulus) && right == 0)
	    {
	      /* If seg_right is not absolute_section, then we've
                 already issued a warning about using a bad symbol.  */
	      if (seg_right == absolute_section && finalize)
		{
		  char *file;
		  unsigned int line;

		  if (expr_symbol_where (symp, &file, &line))
		    as_bad_where (file, line, "division by zero");
		  else
		    as_bad ("division by zero when setting %s",
			    S_GET_NAME (symp));
		}

	      right = 1;
	    }

	  switch (symp->sy_value.X_op)
	    {
	    case O_multiply:		left *= right; break;
	    case O_divide:		left /= right; break;
	    case O_modulus:		left %= right; break;
	    case O_left_shift:		left <<= right; break;
	    case O_right_shift:		left >>= right; break;
	    case O_bit_inclusive_or:	left |= right; break;
	    case O_bit_or_not:		left |= ~right; break;
	    case O_bit_exclusive_or:	left ^= right; break;
	    case O_bit_and:		left &= right; break;
	    case O_add:			left += right; break;
	    case O_subtract:		left -= right; break;
	    case O_eq:	left = left == right ? ~ (offsetT) 0 : 0; break;
	    case O_ne:	left = left != right ? ~ (offsetT) 0 : 0; break;
	    case O_lt:	left = left <  right ? ~ (offsetT) 0 : 0; break;
	    case O_le:	left = left <= right ? ~ (offsetT) 0 : 0; break;
	    case O_ge:	left = left >= right ? ~ (offsetT) 0 : 0; break;
	    case O_gt:	left = left >  right ? ~ (offsetT) 0 : 0; break;
	    case O_logical_and:	left = left && right; break;
	    case O_logical_or:	left = left || right; break;
	    default:		abort ();
	    }

	  final_val += symp->sy_frag->fr_address + left;
	  if (final_seg == expr_section || final_seg == undefined_section)
	    final_seg = absolute_section;
	  resolved = (add_symbol->sy_resolved && op_symbol->sy_resolved);
   	  break;

	case O_register:
	case O_big:
	case O_illegal:
	  /* Give an error (below) if not in expr_section.  We don't
	     want to worry about expr_section symbols, because they
	     are fictional (they are created as part of expression
	     resolution), and any problems may not actually mean
	     anything.  */
	  break;
	}

      symp->sy_resolving = 0;
    }

  if (finalize)
    {
      S_SET_VALUE (symp, final_val);

#if defined (OBJ_AOUT) && ! defined (BFD_ASSEMBLER)
      /* The old a.out backend does not handle S_SET_SEGMENT correctly
         for a stab symbol, so we use this bad hack.  */
      if (final_seg != S_GET_SEGMENT (symp))
#endif
	S_SET_SEGMENT (symp, final_seg);
    }

exit_dont_set_value:
  /* Don't worry if we can't resolve an expr_section symbol.  */
  if (finalize)
    {
      if (resolved)
	symp->sy_resolved = 1;
      else if (S_GET_SEGMENT (symp) != expr_section)
	{
	  as_bad ("can't resolve value for symbol \"%s\"", S_GET_NAME (symp));
	  symp->sy_resolved = 1;
	}
    }

  return final_val;
}

/* Dollar labels look like a number followed by a dollar sign.  Eg, "42$".
   They are *really* local.  That is, they go out of scope whenever we see a
   label that isn't local.  Also, like fb labels, there can be multiple
   instances of a dollar label.  Therefor, we name encode each instance with
   the instance number, keep a list of defined symbols separate from the real
   symbol table, and we treat these buggers as a sparse array.  */

static long *dollar_labels;
static long *dollar_label_instances;
static char *dollar_label_defines;
static unsigned long dollar_label_count;
static unsigned long dollar_label_max;

int 
dollar_label_defined (label)
     long label;
{
  long *i;

  know ((dollar_labels != NULL) || (dollar_label_count == 0));

  for (i = dollar_labels; i < dollar_labels + dollar_label_count; ++i)
    if (*i == label)
      return dollar_label_defines[i - dollar_labels];

  /* if we get here, label isn't defined */
  return 0;
}				/* dollar_label_defined() */

static long
dollar_label_instance (label)
     long label;
{
  long *i;

  know ((dollar_labels != NULL) || (dollar_label_count == 0));

  for (i = dollar_labels; i < dollar_labels + dollar_label_count; ++i)
    if (*i == label)
      return (dollar_label_instances[i - dollar_labels]);

  /* If we get here, we haven't seen the label before, therefore its instance
     count is zero.  */
  return 0;
}

void 
dollar_label_clear ()
{
  memset (dollar_label_defines, '\0', (unsigned int) dollar_label_count);
}

#define DOLLAR_LABEL_BUMP_BY 10

void 
define_dollar_label (label)
     long label;
{
  long *i;

  for (i = dollar_labels; i < dollar_labels + dollar_label_count; ++i)
    if (*i == label)
      {
	++dollar_label_instances[i - dollar_labels];
	dollar_label_defines[i - dollar_labels] = 1;
	return;
      }

  /* if we get to here, we don't have label listed yet. */

  if (dollar_labels == NULL)
    {
      dollar_labels = (long *) xmalloc (DOLLAR_LABEL_BUMP_BY * sizeof (long));
      dollar_label_instances = (long *) xmalloc (DOLLAR_LABEL_BUMP_BY * sizeof (long));
      dollar_label_defines = xmalloc (DOLLAR_LABEL_BUMP_BY);
      dollar_label_max = DOLLAR_LABEL_BUMP_BY;
      dollar_label_count = 0;
    }
  else if (dollar_label_count == dollar_label_max)
    {
      dollar_label_max += DOLLAR_LABEL_BUMP_BY;
      dollar_labels = (long *) xrealloc ((char *) dollar_labels,
					 dollar_label_max * sizeof (long));
      dollar_label_instances = (long *) xrealloc ((char *) dollar_label_instances,
					  dollar_label_max * sizeof (long));
      dollar_label_defines = xrealloc (dollar_label_defines, dollar_label_max);
    }				/* if we needed to grow */

  dollar_labels[dollar_label_count] = label;
  dollar_label_instances[dollar_label_count] = 1;
  dollar_label_defines[dollar_label_count] = 1;
  ++dollar_label_count;
}

/*
 *			dollar_label_name()
 *
 * Caller must copy returned name: we re-use the area for the next name.
 *
 * The mth occurence of label n: is turned into the symbol "Ln^Am"
 * where n is the label number and m is the instance number. "L" makes
 * it a label discarded unless debugging and "^A"('\1') ensures no
 * ordinary symbol SHOULD get the same name as a local label
 * symbol. The first "4:" is "L4^A1" - the m numbers begin at 1.
 *
 * fb labels get the same treatment, except that ^B is used in place of ^A.
 */

char *				/* Return local label name. */
dollar_label_name (n, augend)
     register long n;		/* we just saw "n$:" : n a number */
     register int augend;	/* 0 for current instance, 1 for new instance */
{
  long i;
  /* Returned to caller, then copied.  used for created names ("4f") */
  static char symbol_name_build[24];
  register char *p;
  register char *q;
  char symbol_name_temporary[20];	/* build up a number, BACKWARDS */

  know (n >= 0);
  know (augend == 0 || augend == 1);
  p = symbol_name_build;
  *p++ = 'L';

  /* Next code just does sprintf( {}, "%d", n); */
  /* label number */
  q = symbol_name_temporary;
  for (*q++ = 0, i = n; i; ++q)
    {
      *q = i % 10 + '0';
      i /= 10;
    }
  while ((*p = *--q) != '\0')
    ++p;

  *p++ = 1;			/* ^A */

  /* instance number */
  q = symbol_name_temporary;
  for (*q++ = 0, i = dollar_label_instance (n) + augend; i; ++q)
    {
      *q = i % 10 + '0';
      i /= 10;
    }
  while ((*p++ = *--q) != '\0');;

  /* The label, as a '\0' ended string, starts at symbol_name_build. */
  return symbol_name_build;
}

/*
 * Sombody else's idea of local labels. They are made by "n:" where n
 * is any decimal digit. Refer to them with
 *  "nb" for previous (backward) n:
 *  or "nf" for next (forward) n:.
 *
 * We do a little better and let n be any number, not just a single digit, but
 * since the other guy's assembler only does ten, we treat the first ten
 * specially.
 *
 * Like someone else's assembler, we have one set of local label counters for
 * entire assembly, not one set per (sub)segment like in most assemblers. This
 * implies that one can refer to a label in another segment, and indeed some
 * crufty compilers have done just that.
 *
 * Since there could be a LOT of these things, treat them as a sparse array.
 */

#define FB_LABEL_SPECIAL (10)

static long fb_low_counter[FB_LABEL_SPECIAL];
static long *fb_labels;
static long *fb_label_instances;
static long fb_label_count;
static long fb_label_max;

/* this must be more than FB_LABEL_SPECIAL */
#define FB_LABEL_BUMP_BY (FB_LABEL_SPECIAL + 6)

static void 
fb_label_init ()
{
  memset ((void *) fb_low_counter, '\0', sizeof (fb_low_counter));
}				/* fb_label_init() */

/* add one to the instance number of this fb label */
void 
fb_label_instance_inc (label)
     long label;
{
  long *i;

  if (label < FB_LABEL_SPECIAL)
    {
      ++fb_low_counter[label];
      return;
    }

  if (fb_labels != NULL)
    {
      for (i = fb_labels + FB_LABEL_SPECIAL;
	   i < fb_labels + fb_label_count; ++i)
	{
	  if (*i == label)
	    {
	      ++fb_label_instances[i - fb_labels];
	      return;
	    }			/* if we find it */
	}			/* for each existing label */
    }

  /* if we get to here, we don't have label listed yet. */

  if (fb_labels == NULL)
    {
      fb_labels = (long *) xmalloc (FB_LABEL_BUMP_BY * sizeof (long));
      fb_label_instances = (long *) xmalloc (FB_LABEL_BUMP_BY * sizeof (long));
      fb_label_max = FB_LABEL_BUMP_BY;
      fb_label_count = FB_LABEL_SPECIAL;

    }
  else if (fb_label_count == fb_label_max)
    {
      fb_label_max += FB_LABEL_BUMP_BY;
      fb_labels = (long *) xrealloc ((char *) fb_labels,
				     fb_label_max * sizeof (long));
      fb_label_instances = (long *) xrealloc ((char *) fb_label_instances,
					      fb_label_max * sizeof (long));
    }				/* if we needed to grow */

  fb_labels[fb_label_count] = label;
  fb_label_instances[fb_label_count] = 1;
  ++fb_label_count;
}

static long 
fb_label_instance (label)
     long label;
{
  long *i;

  if (label < FB_LABEL_SPECIAL)
    {
      return (fb_low_counter[label]);
    }

  if (fb_labels != NULL)
    {
      for (i = fb_labels + FB_LABEL_SPECIAL;
	   i < fb_labels + fb_label_count; ++i)
	{
	  if (*i == label)
	    {
	      return (fb_label_instances[i - fb_labels]);
	    }			/* if we find it */
	}			/* for each existing label */
    }

  /* We didn't find the label, so this must be a reference to the
     first instance.  */
  return 0;
}

/*
 *			fb_label_name()
 *
 * Caller must copy returned name: we re-use the area for the next name.
 *
 * The mth occurence of label n: is turned into the symbol "Ln^Bm"
 * where n is the label number and m is the instance number. "L" makes
 * it a label discarded unless debugging and "^B"('\2') ensures no
 * ordinary symbol SHOULD get the same name as a local label
 * symbol. The first "4:" is "L4^B1" - the m numbers begin at 1.
 *
 * dollar labels get the same treatment, except that ^A is used in place of ^B. */

char *				/* Return local label name. */
fb_label_name (n, augend)
     long n;			/* we just saw "n:", "nf" or "nb" : n a number */
     long augend;		/* 0 for nb, 1 for n:, nf */
{
  long i;
  /* Returned to caller, then copied.  used for created names ("4f") */
  static char symbol_name_build[24];
  register char *p;
  register char *q;
  char symbol_name_temporary[20];	/* build up a number, BACKWARDS */

  know (n >= 0);
  know (augend == 0 || augend == 1);
  p = symbol_name_build;
  *p++ = 'L';

  /* Next code just does sprintf( {}, "%d", n); */
  /* label number */
  q = symbol_name_temporary;
  for (*q++ = 0, i = n; i; ++q)
    {
      *q = i % 10 + '0';
      i /= 10;
    }
  while ((*p = *--q) != '\0')
    ++p;

  *p++ = 2;			/* ^B */

  /* instance number */
  q = symbol_name_temporary;
  for (*q++ = 0, i = fb_label_instance (n) + augend; i; ++q)
    {
      *q = i % 10 + '0';
      i /= 10;
    }
  while ((*p++ = *--q) != '\0');;

  /* The label, as a '\0' ended string, starts at symbol_name_build. */
  return (symbol_name_build);
}				/* fb_label_name() */

/*
 * decode name that may have been generated by foo_label_name() above.  If
 * the name wasn't generated by foo_label_name(), then return it unaltered.
 * This is used for error messages.
 */

char *
decode_local_label_name (s)
     char *s;
{
  char *p;
  char *symbol_decode;
  int label_number;
  int instance_number;
  char *type;
  const char *message_format = "\"%d\" (instance number %d of a %s label)";

  if (s[0] != 'L')
    return s;

  for (label_number = 0, p = s + 1; isdigit ((unsigned char) *p); ++p)
    label_number = (10 * label_number) + *p - '0';

  if (*p == 1)
    type = "dollar";
  else if (*p == 2)
    type = "fb";
  else
    return s;

  for (instance_number = 0, p++; isdigit ((unsigned char) *p); ++p)
    instance_number = (10 * instance_number) + *p - '0';

  symbol_decode = obstack_alloc (&notes, strlen (message_format) + 30);
  sprintf (symbol_decode, message_format, label_number, instance_number, type);

  return symbol_decode;
}

/* Get the value of a symbol.  */

valueT
S_GET_VALUE (s)
     symbolS *s;
{
  if (!s->sy_resolved && s->sy_value.X_op != O_constant)
    resolve_symbol_value (s, 1);
  if (s->sy_value.X_op != O_constant)
    {
      static symbolS *recur;

      /* FIXME: In non BFD assemblers, S_IS_DEFINED and S_IS_COMMON
         may call S_GET_VALUE.  We use a static symbol to avoid the
         immediate recursion.  */
      if (recur == s)
	return (valueT) s->sy_value.X_add_number;
      recur = s;
      if (! s->sy_resolved
	  || s->sy_value.X_op != O_symbol
	  || (S_IS_DEFINED (s) && ! S_IS_COMMON (s)))
	as_bad ("Attempt to get value of unresolved symbol %s",
		S_GET_NAME (s));
      recur = NULL;
    }
  return (valueT) s->sy_value.X_add_number;
}

/* Set the value of a symbol.  */

void
S_SET_VALUE (s, val)
     symbolS *s;
     valueT val;
{
  s->sy_value.X_op = O_constant;
  s->sy_value.X_add_number = (offsetT) val;
  s->sy_value.X_unsigned = 0;
}

void
copy_symbol_attributes (dest, src)
     symbolS *dest, *src;
{
#ifdef BFD_ASSEMBLER
  /* In an expression, transfer the settings of these flags.
     The user can override later, of course.  */
#define COPIED_SYMFLAGS	(BSF_FUNCTION | BSF_OBJECT)
  dest->bsym->flags |= src->bsym->flags & COPIED_SYMFLAGS;
#endif

#ifdef OBJ_COPY_SYMBOL_ATTRIBUTES
  OBJ_COPY_SYMBOL_ATTRIBUTES (dest, src);
#endif
}

#ifdef BFD_ASSEMBLER

int
S_IS_EXTERNAL (s)
     symbolS *s;
{
  flagword flags = s->bsym->flags;

  /* sanity check */
  if ((flags & BSF_LOCAL) && (flags & BSF_GLOBAL))
    abort ();

  return (flags & BSF_GLOBAL) != 0;
}

int
S_IS_WEAK (s)
     symbolS *s;
{
  return (s->bsym->flags & BSF_WEAK) != 0;
}

int
S_IS_COMMON (s)
     symbolS *s;
{
  return bfd_is_com_section (s->bsym->section);
}

int
S_IS_DEFINED (s)
     symbolS *s;
{
  return s->bsym->section != undefined_section;
}

int
S_IS_DEBUG (s)
     symbolS *s;
{
  if (s->bsym->flags & BSF_DEBUGGING)
    return 1;
  return 0;
}

int
S_IS_LOCAL (s)
     symbolS *s;
{
  flagword flags = s->bsym->flags;
  const char *name;

  /* sanity check */
  if ((flags & BSF_LOCAL) && (flags & BSF_GLOBAL))
    abort ();

  if (bfd_get_section (s->bsym) == reg_section)
    return 1;

  if (flag_strip_local_absolute
      && (flags & BSF_GLOBAL) == 0
      && bfd_get_section (s->bsym) == absolute_section)
    return 1;

  name = S_GET_NAME (s);
  return (name != NULL
	  && ! S_IS_DEBUG (s)
	  && (strchr (name, '\001')
	      || strchr (name, '\002')
	      || (! flag_keep_locals
		  && (bfd_is_local_label (stdoutput, s->bsym)
		      || (flag_mri
			  && name[0] == '?'
			  && name[1] == '?')))));
}

int
S_IS_EXTERN (s)
     symbolS *s;
{
  return S_IS_EXTERNAL (s);
}

int
S_IS_STABD (s)
     symbolS *s;
{
  return S_GET_NAME (s) == 0;
}

CONST char *
S_GET_NAME (s)
     symbolS *s;
{
  return s->bsym->name;
}

segT
S_GET_SEGMENT (s)
     symbolS *s;
{
  return s->bsym->section;
}

void
S_SET_SEGMENT (s, seg)
     symbolS *s;
     segT seg;
{
  /* Don't reassign section symbols.  The direct reason is to prevent seg
     faults assigning back to const global symbols such as *ABS*, but it
     shouldn't happen anyway.  */

  if (s->bsym->flags & BSF_SECTION_SYM)
    {
      if (s->bsym->section != seg)
	abort();
    }
  else
    s->bsym->section = seg;
}

void
S_SET_EXTERNAL (s)
     symbolS *s;
{
  if ((s->bsym->flags & BSF_WEAK) != 0)
    {
      /* Let .weak override .global.  */
      return;
    }
  s->bsym->flags |= BSF_GLOBAL;
  s->bsym->flags &= ~(BSF_LOCAL|BSF_WEAK);
}

void
S_CLEAR_EXTERNAL (s)
     symbolS *s;
{
  if ((s->bsym->flags & BSF_WEAK) != 0)
    {
      /* Let .weak override.  */
      return;
    }
  s->bsym->flags |= BSF_LOCAL;
  s->bsym->flags &= ~(BSF_GLOBAL|BSF_WEAK);
}

void
S_SET_WEAK (s)
     symbolS *s;
{
  s->bsym->flags |= BSF_WEAK;
  s->bsym->flags &= ~(BSF_GLOBAL|BSF_LOCAL);
}

void
S_SET_NAME (s, name)
     symbolS *s;
     char *name;
{
  s->bsym->name = name;
}
#endif /* BFD_ASSEMBLER */

void
symbol_begin ()
{
  symbol_lastP = NULL;
  symbol_rootP = NULL;		/* In case we have 0 symbols (!!) */
  sy_hash = hash_new ();

  memset ((char *) (&abs_symbol), '\0', sizeof (abs_symbol));
#ifdef BFD_ASSEMBLER
#if defined (EMIT_SECTION_SYMBOLS) || !defined (RELOC_REQUIRES_SYMBOL)
  abs_symbol.bsym = bfd_abs_section.symbol;
#endif
#else
  /* Can't initialise a union. Sigh. */
  S_SET_SEGMENT (&abs_symbol, absolute_section);
#endif
  abs_symbol.sy_value.X_op = O_constant;
  abs_symbol.sy_frag = &zero_address_frag;

  if (LOCAL_LABELS_FB)
    fb_label_init ();
}


int indent_level;

#if 0

static void
indent ()
{
  printf ("%*s", indent_level * 4, "");
}

#endif

void
print_symbol_value_1 (file, sym)
     FILE *file;
     symbolS *sym;
{
  const char *name = S_GET_NAME (sym);
  if (!name || !name[0])
    name = "(unnamed)";
  fprintf (file, "sym %lx %s", (unsigned long) sym, name);
  if (sym->sy_frag != &zero_address_frag)
    fprintf (file, " frag %lx", (long) sym->sy_frag);
  if (sym->written)
    fprintf (file, " written");
  if (sym->sy_resolved)
    fprintf (file, " resolved");
  else if (sym->sy_resolving)
    fprintf (file, " resolving");
  if (sym->sy_used_in_reloc)
    fprintf (file, " used-in-reloc");
  if (sym->sy_used)
    fprintf (file, " used");
  if (S_IS_LOCAL (sym))
    fprintf (file, " local");
  if (S_IS_EXTERN (sym))
    fprintf (file, " extern");
  if (S_IS_DEBUG (sym))
    fprintf (file, " debug");
  if (S_IS_DEFINED (sym))
    fprintf (file, " defined");
  fprintf (file, " %s", segment_name (S_GET_SEGMENT (sym)));
  if (sym->sy_resolved)
    {
      segT s = S_GET_SEGMENT (sym);

      if (s != undefined_section
          && s != expr_section)
	fprintf (file, " %lx", (long) S_GET_VALUE (sym));
    }
  else if (indent_level < 8 && S_GET_SEGMENT (sym) != undefined_section)
    {
      indent_level++;
      fprintf (file, "\n%*s<", indent_level * 4, "");
      print_expr_1 (file, &sym->sy_value);
      fprintf (file, ">");
      indent_level--;
    }
  fflush (file);
}

void
print_symbol_value (sym)
     symbolS *sym;
{
  indent_level = 0;
  print_symbol_value_1 (stderr, sym);
  fprintf (stderr, "\n");
}

void
print_expr_1 (file, exp)
     FILE *file;
     expressionS *exp;
{
  fprintf (file, "expr %lx ", (long) exp);
  switch (exp->X_op)
    {
    case O_illegal:
      fprintf (file, "illegal");
      break;
    case O_absent:
      fprintf (file, "absent");
      break;
    case O_constant:
      fprintf (file, "constant %lx", (long) exp->X_add_number);
      break;
    case O_symbol:
      indent_level++;
      fprintf (file, "symbol\n%*s<", indent_level * 4, "");
      print_symbol_value_1 (file, exp->X_add_symbol);
      fprintf (file, ">");
    maybe_print_addnum:
      if (exp->X_add_number)
	fprintf (file, "\n%*s%lx", indent_level * 4, "",
		 (long) exp->X_add_number);
      indent_level--;
      break;
    case O_register:
      fprintf (file, "register #%d", (int) exp->X_add_number);
      break;
    case O_big:
      fprintf (file, "big");
      break;
    case O_uminus:
      fprintf (file, "uminus -<");
      indent_level++;
      print_symbol_value_1 (file, exp->X_add_symbol);
      fprintf (file, ">");
      goto maybe_print_addnum;
    case O_bit_not:
      fprintf (file, "bit_not");
      break;
    case O_multiply:
      fprintf (file, "multiply");
      break;
    case O_divide:
      fprintf (file, "divide");
      break;
    case O_modulus:
      fprintf (file, "modulus");
      break;
    case O_left_shift:
      fprintf (file, "lshift");
      break;
    case O_right_shift:
      fprintf (file, "rshift");
      break;
    case O_bit_inclusive_or:
      fprintf (file, "bit_ior");
      break;
    case O_bit_exclusive_or:
      fprintf (file, "bit_xor");
      break;
    case O_bit_and:
      fprintf (file, "bit_and");
      break;
    case O_eq:
      fprintf (file, "eq");
      break;
    case O_ne:
      fprintf (file, "ne");
      break;
    case O_lt:
      fprintf (file, "lt");
      break;
    case O_le:
      fprintf (file, "le");
      break;
    case O_ge:
      fprintf (file, "ge");
      break;
    case O_gt:
      fprintf (file, "gt");
      break;
    case O_logical_and:
      fprintf (file, "logical_and");
      break;
    case O_logical_or:
      fprintf (file, "logical_or");
      break;
    case O_add:
      indent_level++;
      fprintf (file, "add\n%*s<", indent_level * 4, "");
      print_symbol_value_1 (file, exp->X_add_symbol);
      fprintf (file, ">\n%*s<", indent_level * 4, "");
      print_symbol_value_1 (file, exp->X_op_symbol);
      fprintf (file, ">");
      goto maybe_print_addnum;
    case O_subtract:
      indent_level++;
      fprintf (file, "subtract\n%*s<", indent_level * 4, "");
      print_symbol_value_1 (file, exp->X_add_symbol);
      fprintf (file, ">\n%*s<", indent_level * 4, "");
      print_symbol_value_1 (file, exp->X_op_symbol);
      fprintf (file, ">");
      goto maybe_print_addnum;
    default:
      fprintf (file, "{unknown opcode %d}", (int) exp->X_op);
      break;
    }
  fflush (stdout);
}

void
print_expr (exp)
     expressionS *exp;
{
  print_expr_1 (stderr, exp);
  fprintf (stderr, "\n");
}

void
symbol_print_statistics (file)
     FILE *file;
{
  hash_print_statistics (file, "symbol table", sy_hash);
}

/* end of symbols.c */
