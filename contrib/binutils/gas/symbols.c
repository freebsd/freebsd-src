/* symbols.c -symbol table-
   Copyright 1987, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002
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

/* #define DEBUG_SYMS / * to debug symbol list maintenance.  */

#include "as.h"

#include "safe-ctype.h"
#include "obstack.h"		/* For "symbols.h" */
#include "subsegs.h"

#include "struc-symbol.h"

/* This is non-zero if symbols are case sensitive, which is the
   default.  */
int symbols_case_sensitive = 1;

#ifndef WORKING_DOT_WORD
extern int new_broken_words;
#endif

/* symbol-name => struct symbol pointer */
static struct hash_control *sy_hash;

/* Table of local symbols.  */
static struct hash_control *local_hash;

/* Below are commented in "symbols.h".  */
symbolS *symbol_rootP;
symbolS *symbol_lastP;
symbolS abs_symbol;

#ifdef DEBUG_SYMS
#define debug_verify_symchain verify_symbol_chain
#else
#define debug_verify_symchain(root, last) ((void) 0)
#endif

#define DOLLAR_LABEL_CHAR	'\001'
#define LOCAL_LABEL_CHAR	'\002'

struct obstack notes;

static char *save_symbol_name PARAMS ((const char *));
static void fb_label_init PARAMS ((void));
static long dollar_label_instance PARAMS ((long));
static long fb_label_instance PARAMS ((long));

static void print_binary PARAMS ((FILE *, const char *, expressionS *));

/* Return a pointer to a new symbol.  Die if we can't make a new
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

  /* Link to end of symbol chain.  */
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

/* Save a symbol name on a permanent obstack, and convert it according
   to the object file format.  */

static char *
save_symbol_name (name)
     const char *name;
{
  unsigned int name_length;
  char *ret;

  name_length = strlen (name) + 1;	/* +1 for \0.  */
  obstack_grow (&notes, name, name_length);
  ret = obstack_finish (&notes);

#ifdef STRIP_UNDERSCORE
  if (ret[0] == '_')
    ++ret;
#endif

#ifdef tc_canonicalize_symbol_name
  ret = tc_canonicalize_symbol_name (ret);
#endif

  if (! symbols_case_sensitive)
    {
      char *s;

      for (s = ret; *s != '\0'; s++)
	*s = TOUPPER (*s);
    }

  return ret;
}

symbolS *
symbol_create (name, segment, valu, frag)
     const char *name;		/* It is copied, the caller can destroy/modify.  */
     segT segment;		/* Segment identifier (SEG_<something>).  */
     valueT valu;		/* Symbol value.  */
     fragS *frag;		/* Associated fragment.  */
{
  char *preserved_copy_of_name;
  symbolS *symbolP;

  preserved_copy_of_name = save_symbol_name (name);

  symbolP = (symbolS *) obstack_alloc (&notes, sizeof (symbolS));

  /* symbol must be born in some fixed state.  This seems as good as any.  */
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

#ifdef BFD_ASSEMBLER

/* Local symbol support.  If we can get away with it, we keep only a
   small amount of information for local symbols.  */

static struct local_symbol *local_symbol_make PARAMS ((const char *, segT,
						       valueT, fragS *));
static symbolS *local_symbol_convert PARAMS ((struct local_symbol *));

/* Used for statistics.  */

static unsigned long local_symbol_count;
static unsigned long local_symbol_conversion_count;

/* This macro is called with a symbol argument passed by reference.
   It returns whether this is a local symbol.  If necessary, it
   changes its argument to the real symbol.  */

#define LOCAL_SYMBOL_CHECK(s)						\
  (s->bsym == NULL							\
   ? (local_symbol_converted_p ((struct local_symbol *) s)		\
      ? (s = local_symbol_get_real_symbol ((struct local_symbol *) s),	\
	 0)								\
      : 1)								\
   : 0)

/* Create a local symbol and insert it into the local hash table.  */

static struct local_symbol *
local_symbol_make (name, section, value, frag)
     const char *name;
     segT section;
     valueT value;
     fragS *frag;
{
  char *name_copy;
  struct local_symbol *ret;

  ++local_symbol_count;

  name_copy = save_symbol_name (name);

  ret = (struct local_symbol *) obstack_alloc (&notes, sizeof *ret);
  ret->lsy_marker = NULL;
  ret->lsy_name = name_copy;
  ret->lsy_section = section;
  local_symbol_set_frag (ret, frag);
  ret->lsy_value = value;

  hash_jam (local_hash, name_copy, (PTR) ret);

  return ret;
}

/* Convert a local symbol into a real symbol.  Note that we do not
   reclaim the space used by the local symbol.  */

static symbolS *
local_symbol_convert (locsym)
     struct local_symbol *locsym;
{
  symbolS *ret;

  assert (locsym->lsy_marker == NULL);
  if (local_symbol_converted_p (locsym))
    return local_symbol_get_real_symbol (locsym);

  ++local_symbol_conversion_count;

  ret = symbol_new (locsym->lsy_name, locsym->lsy_section, locsym->lsy_value,
		    local_symbol_get_frag (locsym));

  if (local_symbol_resolved_p (locsym))
    ret->sy_resolved = 1;

  /* Local symbols are always either defined or used.  */
  ret->sy_used = 1;

#ifdef TC_LOCAL_SYMFIELD_CONVERT
  TC_LOCAL_SYMFIELD_CONVERT (locsym, ret);
#endif

  symbol_table_insert (ret);

  local_symbol_mark_converted (locsym);
  local_symbol_set_real_symbol (locsym, ret);

  hash_jam (local_hash, locsym->lsy_name, NULL);

  return ret;
}

#else /* ! BFD_ASSEMBLER */

#define LOCAL_SYMBOL_CHECK(s) 0
#define local_symbol_convert(s) ((symbolS *) s)

#endif /* ! BFD_ASSEMBLER */

/* We have just seen "<name>:".
   Creates a struct symbol unless it already exists.

   Gripes if we are redefining a symbol incompatibly (and ignores it).  */

symbolS *
colon (sym_name)		/* Just seen "x:" - rattle symbols & frags.  */
     const char *sym_name;	/* Symbol name, as a cannonical string.  */
     /* We copy this string: OK to alter later.  */
{
  register symbolS *symbolP;	/* Symbol we are working with.  */

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

      /* We want to store the pointer to where to insert the jump
	 table in the fr_opcode of the rs_broken_word frag.  This
	 requires a little hackery.  */
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
      /* Now check for undefined symbols.  */
      if (LOCAL_SYMBOL_CHECK (symbolP))
	{
#ifdef BFD_ASSEMBLER
	  struct local_symbol *locsym = (struct local_symbol *) symbolP;

	  if (locsym->lsy_section != undefined_section
	      && (local_symbol_get_frag (locsym) != frag_now
		  || locsym->lsy_section != now_seg
		  || locsym->lsy_value != frag_now_fix ()))
	    {
	      as_bad (_("symbol `%s' is already defined"), sym_name);
	      return symbolP;
	    }

	  locsym->lsy_section = now_seg;
	  local_symbol_set_frag (locsym, frag_now);
	  locsym->lsy_value = frag_now_fix ();
#endif
	}
      else if (!S_IS_DEFINED (symbolP) || S_IS_COMMON (symbolP))
	{
	  if (S_GET_VALUE (symbolP) == 0)
	    {
	      symbolP->sy_frag = frag_now;
#ifdef OBJ_VMS
	      S_SET_OTHER (symbolP, const_flag);
#endif
	      S_SET_VALUE (symbolP, (valueT) frag_now_fix ());
	      S_SET_SEGMENT (symbolP, now_seg);
#ifdef N_UNDF
	      know (N_UNDF == 0);
#endif /* if we have one, it better be zero.  */

	    }
	  else
	    {
	      /* There are still several cases to check:

		 A .comm/.lcomm symbol being redefined as initialized
		 data is OK

		 A .comm/.lcomm symbol being redefined with a larger
		 size is also OK

		 This only used to be allowed on VMS gas, but Sun cc
		 on the sparc also depends on it.  */

	      if (((!S_IS_DEBUG (symbolP)
		    && (!S_IS_DEFINED (symbolP) || S_IS_COMMON (symbolP))
		    && S_IS_EXTERNAL (symbolP))
		   || S_GET_SEGMENT (symbolP) == bss_section)
		  && (now_seg == data_section
		      || now_seg == S_GET_SEGMENT (symbolP)))
		{
		  /* Select which of the 2 cases this is.  */
		  if (now_seg != data_section)
		    {
		      /* New .comm for prev .comm symbol.

			 If the new size is larger we just change its
			 value.  If the new size is smaller, we ignore
			 this symbol.  */
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
		      S_SET_OTHER (symbolP, const_flag);
#endif
		      S_SET_VALUE (symbolP, (valueT) frag_now_fix ());
		      S_SET_SEGMENT (symbolP, now_seg);	/* Keep N_EXT bit.  */
		    }
		}
	      else
		{
#if (!defined (OBJ_AOUT) && !defined (OBJ_MAYBE_AOUT) \
     && !defined (OBJ_BOUT) && !defined (OBJ_MAYBE_BOUT))
		  static const char *od_buf = "";
#else
		  char od_buf[100];
		  od_buf[0] = '\0';
#ifdef BFD_ASSEMBLER
		  if (OUTPUT_FLAVOR == bfd_target_aout_flavour)
#endif
		    sprintf (od_buf, "%d.%d.",
			     S_GET_OTHER (symbolP),
			     S_GET_DESC (symbolP));
#endif
		  as_bad (_("symbol `%s' is already defined as \"%s\"/%s%ld"),
			    sym_name,
			    segment_name (S_GET_SEGMENT (symbolP)),
			    od_buf,
			    (long) S_GET_VALUE (symbolP));
		}
	    }			/* if the undefined symbol has no value  */
	}
      else
	{
	  /* Don't blow up if the definition is the same.  */
	  if (!(frag_now == symbolP->sy_frag
		&& S_GET_VALUE (symbolP) == frag_now_fix ()
		&& S_GET_SEGMENT (symbolP) == now_seg))
	    as_bad (_("symbol `%s' is already defined"), sym_name);
	}

    }
#ifdef BFD_ASSEMBLER
  else if (! flag_keep_locals && bfd_is_local_label_name (stdoutput, sym_name))
    {
      symbolP = (symbolS *) local_symbol_make (sym_name, now_seg,
					       (valueT) frag_now_fix (),
					       frag_now);
    }
#endif /* BFD_ASSEMBLER */
  else
    {
      symbolP = symbol_new (sym_name, now_seg, (valueT) frag_now_fix (),
			    frag_now);
#ifdef OBJ_VMS
      S_SET_OTHER (symbolP, const_flag);
#endif /* OBJ_VMS */

      symbol_table_insert (symbolP);
    }

  if (mri_common_symbol != NULL)
    {
      /* This symbol is actually being defined within an MRI common
         section.  This requires special handling.  */
      if (LOCAL_SYMBOL_CHECK (symbolP))
	symbolP = local_symbol_convert ((struct local_symbol *) symbolP);
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

/* Die if we can't insert the symbol.  */

void
symbol_table_insert (symbolP)
     symbolS *symbolP;
{
  register const char *error_string;

  know (symbolP);
  know (S_GET_NAME (symbolP));

  if (LOCAL_SYMBOL_CHECK (symbolP))
    {
      error_string = hash_jam (local_hash, S_GET_NAME (symbolP),
			       (PTR) symbolP);
      if (error_string != NULL)
	as_fatal (_("inserting \"%s\" into symbol table failed: %s"),
		  S_GET_NAME (symbolP), error_string);
      return;
    }

  if ((error_string = hash_jam (sy_hash, S_GET_NAME (symbolP), (PTR) symbolP)))
    {
      as_fatal (_("inserting \"%s\" into symbol table failed: %s"),
		S_GET_NAME (symbolP), error_string);
    }				/* on error  */
}

/* If a symbol name does not exist, create it as undefined, and insert
   it into the symbol table.  Return a pointer to it.  */

symbolS *
symbol_find_or_make (name)
     const char *name;
{
  register symbolS *symbolP;

  symbolP = symbol_find (name);

  if (symbolP == NULL)
    {
#ifdef BFD_ASSEMBLER
      if (! flag_keep_locals && bfd_is_local_label_name (stdoutput, name))
	{
	  symbolP = md_undefined_symbol ((char *) name);
	  if (symbolP != NULL)
	    return symbolP;

	  symbolP = (symbolS *) local_symbol_make (name, undefined_section,
						   (valueT) 0,
						   &zero_address_frag);
	  return symbolP;
	}
#endif

      symbolP = symbol_make (name);

      symbol_table_insert (symbolP);
    }				/* if symbol wasn't found */

  return (symbolP);
}

symbolS *
symbol_make (name)
     const char *name;
{
  symbolS *symbolP;

  /* Let the machine description default it, e.g. for register names.  */
  symbolP = md_undefined_symbol ((char *) name);

  if (!symbolP)
    symbolP = symbol_new (name, undefined_section, (valueT) 0, &zero_address_frag);

  return (symbolP);
}

/* Implement symbol table lookup.
   In:	A symbol's name as a string: '\0' can't be part of a symbol name.
   Out:	NULL if the name was not in the symbol table, else the address
   of a struct symbol associated with that name.  */

symbolS *
symbol_find (name)
     const char *name;
{
#ifdef STRIP_UNDERSCORE
  return (symbol_find_base (name, 1));
#else /* STRIP_UNDERSCORE */
  return (symbol_find_base (name, 0));
#endif /* STRIP_UNDERSCORE */
}

symbolS *
symbol_find_exact (name)
     const char *name;
{
#ifdef BFD_ASSEMBLER
  {
    struct local_symbol *locsym;

    locsym = (struct local_symbol *) hash_find (local_hash, name);
    if (locsym != NULL)
      return (symbolS *) locsym;
  }
#endif

  return ((symbolS *) hash_find (sy_hash, name));
}

symbolS *
symbol_find_base (name, strip_underscore)
     const char *name;
     int strip_underscore;
{
  if (strip_underscore && *name == '_')
    name++;

#ifdef tc_canonicalize_symbol_name
  {
    char *copy;
    size_t len = strlen (name) + 1;

    copy = (char *) alloca (len);
    memcpy (copy, name, len);
    name = tc_canonicalize_symbol_name (copy);
  }
#endif

  if (! symbols_case_sensitive)
    {
      char *copy;
      const char *orig;
      unsigned char c;

      orig = name;
      name = copy = (char *) alloca (strlen (name) + 1);

      while ((c = *orig++) != '\0')
	{
	  *copy++ = TOUPPER (c);
	}
      *copy = '\0';
    }

  return symbol_find_exact (name);
}

/* Once upon a time, symbols were kept in a singly linked list.  At
   least coff needs to be able to rearrange them from time to time, for
   which a doubly linked list is much more convenient.  Loic did these
   as macros which seemed dangerous to me so they're now functions.
   xoxorich.  */

/* Link symbol ADDME after symbol TARGET in the chain.  */

void
symbol_append (addme, target, rootPP, lastPP)
     symbolS *addme;
     symbolS *target;
     symbolS **rootPP;
     symbolS **lastPP;
{
  if (LOCAL_SYMBOL_CHECK (addme))
    abort ();
  if (target != NULL && LOCAL_SYMBOL_CHECK (target))
    abort ();

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
    }				/* if the list is empty  */

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
    }				/* if we have a next  */

  addme->sy_next = target->sy_next;
  target->sy_next = addme;

#ifdef SYMBOLS_NEED_BACKPOINTERS
  addme->sy_previous = target;
#endif /* SYMBOLS_NEED_BACKPOINTERS */

  debug_verify_symchain (symbol_rootP, symbol_lastP);
}

/* Set the chain pointers of SYMBOL to null.  */

void
symbol_clear_list_pointers (symbolP)
     symbolS *symbolP;
{
  if (LOCAL_SYMBOL_CHECK (symbolP))
    abort ();
  symbolP->sy_next = NULL;
#ifdef SYMBOLS_NEED_BACKPOINTERS
  symbolP->sy_previous = NULL;
#endif
}

#ifdef SYMBOLS_NEED_BACKPOINTERS
/* Remove SYMBOLP from the list.  */

void
symbol_remove (symbolP, rootPP, lastPP)
     symbolS *symbolP;
     symbolS **rootPP;
     symbolS **lastPP;
{
  if (LOCAL_SYMBOL_CHECK (symbolP))
    abort ();

  if (symbolP == *rootPP)
    {
      *rootPP = symbolP->sy_next;
    }				/* if it was the root  */

  if (symbolP == *lastPP)
    {
      *lastPP = symbolP->sy_previous;
    }				/* if it was the tail  */

  if (symbolP->sy_next != NULL)
    {
      symbolP->sy_next->sy_previous = symbolP->sy_previous;
    }				/* if not last  */

  if (symbolP->sy_previous != NULL)
    {
      symbolP->sy_previous->sy_next = symbolP->sy_next;
    }				/* if not first  */

  debug_verify_symchain (*rootPP, *lastPP);
}

/* Link symbol ADDME before symbol TARGET in the chain.  */

void
symbol_insert (addme, target, rootPP, lastPP)
     symbolS *addme;
     symbolS *target;
     symbolS **rootPP;
     symbolS **lastPP ATTRIBUTE_UNUSED;
{
  if (LOCAL_SYMBOL_CHECK (addme))
    abort ();
  if (LOCAL_SYMBOL_CHECK (target))
    abort ();

  if (target->sy_previous != NULL)
    {
      target->sy_previous->sy_next = addme;
    }
  else
    {
      know (*rootPP == target);
      *rootPP = addme;
    }				/* if not first  */

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
#ifdef BFD_ASSEMBLER
      assert (symbolP->bsym != NULL);
#endif
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
resolve_symbol_value (symp)
     symbolS *symp;
{
  int resolved;
  valueT final_val = 0;
  segT final_seg;

#ifdef BFD_ASSEMBLER
  if (LOCAL_SYMBOL_CHECK (symp))
    {
      struct local_symbol *locsym = (struct local_symbol *) symp;

      final_val = locsym->lsy_value;
      if (local_symbol_resolved_p (locsym))
	return final_val;

      final_val += local_symbol_get_frag (locsym)->fr_address / OCTETS_PER_BYTE;

      if (finalize_syms)
	{
	  locsym->lsy_value = final_val;
	  local_symbol_mark_resolved (locsym);
	}

      return final_val;
    }
#endif

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
      if (finalize_syms)
	as_bad (_("symbol definition loop encountered at `%s'"),
		S_GET_NAME (symp));
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
	  final_val += symp->sy_frag->fr_address / OCTETS_PER_BYTE;
	  if (final_seg == expr_section)
	    final_seg = absolute_section;
	  resolved = 1;
	  break;

	case O_symbol:
	case O_symbol_rva:
	  left = resolve_symbol_value (add_symbol);
	  seg_left = S_GET_SEGMENT (add_symbol);
	  if (finalize_syms)
	    symp->sy_value.X_op_symbol = NULL;

	do_symbol:
	  if (symp->sy_mri_common)
	    {
	      /* This is a symbol inside an MRI common section.  The
		 relocation routines are going to handle it specially.
		 Don't change the value.  */
	      resolved = symbol_resolved_p (add_symbol);
	      break;
	    }

	  if (finalize_syms && final_val == 0)
	    {
	      if (LOCAL_SYMBOL_CHECK (add_symbol))
		add_symbol = local_symbol_convert ((struct local_symbol *)
						   add_symbol);
	      copy_symbol_attributes (symp, add_symbol);
	    }

	  /* If we have equated this symbol to an undefined or common
	     symbol, keep X_op set to O_symbol, and don't change
	     X_add_number.  This permits the routine which writes out
	     relocation to detect this case, and convert the
	     relocation to be against the symbol to which this symbol
	     is equated.  */
	  if (! S_IS_DEFINED (add_symbol) || S_IS_COMMON (add_symbol))
	    {
	      if (finalize_syms)
		{
		  symp->sy_value.X_op = O_symbol;
		  symp->sy_value.X_add_symbol = add_symbol;
		  symp->sy_value.X_add_number = final_val;
		  /* Use X_op_symbol as a flag.  */
		  symp->sy_value.X_op_symbol = add_symbol;
		  final_seg = seg_left;
		}
	      final_val = 0;
	      resolved = symbol_resolved_p (add_symbol);
	      symp->sy_resolving = 0;
	      goto exit_dont_set_value;
	    }
	  else if (finalize_syms && final_seg == expr_section
		   && seg_left != expr_section)
	    {
	      /* If the symbol is an expression symbol, do similarly
		 as for undefined and common syms above.  Handles
		 "sym +/- expr" where "expr" cannot be evaluated
		 immediately, and we want relocations to be against
		 "sym", eg. because it is weak.  */
	      symp->sy_value.X_op = O_symbol;
	      symp->sy_value.X_add_symbol = add_symbol;
	      symp->sy_value.X_add_number = final_val;
	      symp->sy_value.X_op_symbol = add_symbol;
	      final_seg = seg_left;
	      final_val += symp->sy_frag->fr_address + left;
	      resolved = symbol_resolved_p (add_symbol);
	      symp->sy_resolving = 0;
	      goto exit_dont_set_value;
	    }
	  else
	    {
	      final_val += symp->sy_frag->fr_address + left;
	      if (final_seg == expr_section || final_seg == undefined_section)
		final_seg = seg_left;
	    }

	  resolved = symbol_resolved_p (add_symbol);
	  break;

	case O_uminus:
	case O_bit_not:
	case O_logical_not:
	  left = resolve_symbol_value (add_symbol);
	  seg_left = S_GET_SEGMENT (add_symbol);

	  if (op == O_uminus)
	    left = -left;
	  else if (op == O_logical_not)
	    left = !left;
	  else
	    left = ~left;

	  final_val += left + symp->sy_frag->fr_address;
	  if (final_seg == expr_section || final_seg == undefined_section)
	    final_seg = seg_left;

	  resolved = symbol_resolved_p (add_symbol);
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
	  left = resolve_symbol_value (add_symbol);
	  right = resolve_symbol_value (op_symbol);
	  seg_left = S_GET_SEGMENT (add_symbol);
	  seg_right = S_GET_SEGMENT (op_symbol);

	  /* Simplify addition or subtraction of a constant by folding the
	     constant into X_add_number.  */
	  if (op == O_add)
	    {
	      if (seg_right == absolute_section)
		{
		  final_val += right;
		  goto do_symbol;
		}
	      else if (seg_left == absolute_section)
		{
		  final_val += left;
		  add_symbol = op_symbol;
		  left = right;
		  seg_left = seg_right;
		  goto do_symbol;
		}
	    }
	  else if (op == O_subtract)
	    {
	      if (seg_right == absolute_section)
		{
		  final_val -= right;
		  goto do_symbol;
		}
	    }

	  /* Equality and non-equality tests are permitted on anything.
	     Subtraction, and other comparison operators are permitted if
	     both operands are in the same section.  Otherwise, both
	     operands must be absolute.  We already handled the case of
	     addition or subtraction of a constant above.  This will
	     probably need to be changed for an object file format which
	     supports arbitrary expressions, such as IEEE-695.

	     Don't emit messages unless we're finalizing the symbol value,
	     otherwise we may get the same message multiple times.  */
	  if ((op == O_eq || op == O_ne)
	      || ((op == O_subtract
		   || op == O_lt || op == O_le || op == O_ge || op == O_gt)
		  && seg_left == seg_right
		  && (seg_left != undefined_section
		      || add_symbol == op_symbol))
	      || (seg_left == absolute_section
		  && seg_right == absolute_section))
	    {
	      if (final_seg == expr_section || final_seg == undefined_section)
		final_seg = absolute_section;
	    }
	  else if (finalize_syms)
	    {
	      char *file;
	      unsigned int line;

	      if (expr_symbol_where (symp, &file, &line))
		{
		  if (seg_left == undefined_section)
		    as_bad_where (file, line,
				  _("undefined symbol `%s' in operation"),
				  S_GET_NAME (symp->sy_value.X_add_symbol));
		  if (seg_right == undefined_section)
		    as_bad_where (file, line,
				  _("undefined symbol `%s' in operation"),
				  S_GET_NAME (symp->sy_value.X_op_symbol));
		  if (seg_left != undefined_section
		      && seg_right != undefined_section)
		    as_bad_where (file, line,
				  _("invalid section for operation"));
		}
	      else
		{
		  if (seg_left == undefined_section)
		    as_bad (_("undefined symbol `%s' in operation setting `%s'"),
			    S_GET_NAME (symp->sy_value.X_add_symbol),
			    S_GET_NAME (symp));
		  if (seg_right == undefined_section)
		    as_bad (_("undefined symbol `%s' in operation setting `%s'"),
			    S_GET_NAME (symp->sy_value.X_op_symbol),
			    S_GET_NAME (symp));
		  if (seg_left != undefined_section
		      && seg_right != undefined_section)
		    as_bad (_("invalid section for operation setting `%s'"),
			    S_GET_NAME (symp));
		}
	      /* Prevent the error propagating.  */
	      if (final_seg == expr_section || final_seg == undefined_section)
		final_seg = absolute_section;
	    }

	  /* Check for division by zero.  */
	  if ((op == O_divide || op == O_modulus) && right == 0)
	    {
	      /* If seg_right is not absolute_section, then we've
		 already issued a warning about using a bad symbol.  */
	      if (seg_right == absolute_section && finalize_syms)
		{
		  char *file;
		  unsigned int line;

		  if (expr_symbol_where (symp, &file, &line))
		    as_bad_where (file, line, _("division by zero"));
		  else
		    as_bad (_("division by zero when setting `%s'"),
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
	    case O_eq:
	    case O_ne:
	      left = (left == right && seg_left == seg_right
		      && (seg_left != undefined_section
			  || add_symbol == op_symbol)
		      ? ~ (offsetT) 0 : 0);
	      if (symp->sy_value.X_op == O_ne)
		left = ~left;
	      break;
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
	    {
	      if (seg_left == undefined_section
		  || seg_right == undefined_section)
		final_seg = undefined_section;
	      else if (seg_left == absolute_section)
		final_seg = seg_right;
	      else
		final_seg = seg_left;
	    }
	  resolved = (symbol_resolved_p (add_symbol)
		      && symbol_resolved_p (op_symbol));
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

  if (finalize_syms)
    S_SET_VALUE (symp, final_val);

exit_dont_set_value:
  /* Always set the segment, even if not finalizing the value.
     The segment is used to determine whether a symbol is defined.  */
#if defined (OBJ_AOUT) && ! defined (BFD_ASSEMBLER)
  /* The old a.out backend does not handle S_SET_SEGMENT correctly
     for a stab symbol, so we use this bad hack.  */
  if (final_seg != S_GET_SEGMENT (symp))
#endif
    S_SET_SEGMENT (symp, final_seg);

  /* Don't worry if we can't resolve an expr_section symbol.  */
  if (finalize_syms)
    {
      if (resolved)
	symp->sy_resolved = 1;
      else if (S_GET_SEGMENT (symp) != expr_section)
	{
	  as_bad (_("can't resolve value for symbol `%s'"),
		  S_GET_NAME (symp));
	  symp->sy_resolved = 1;
	}
    }

  return final_val;
}

#ifdef BFD_ASSEMBLER

static void resolve_local_symbol PARAMS ((const char *, PTR));

/* A static function passed to hash_traverse.  */

static void
resolve_local_symbol (key, value)
     const char *key ATTRIBUTE_UNUSED;
     PTR value;
{
  if (value != NULL)
    resolve_symbol_value (value);
}

#endif

/* Resolve all local symbols.  */

void
resolve_local_symbol_values ()
{
#ifdef BFD_ASSEMBLER
  hash_traverse (local_hash, resolve_local_symbol);
#endif
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

  /* If we get here, label isn't defined.  */
  return 0;
}

static long
dollar_label_instance (label)
     long label;
{
  long *i;

  know ((dollar_labels != NULL) || (dollar_label_count == 0));

  for (i = dollar_labels; i < dollar_labels + dollar_label_count; ++i)
    if (*i == label)
      return (dollar_label_instances[i - dollar_labels]);

  /* If we get here, we haven't seen the label before.
     Therefore its instance count is zero.  */
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

  /* If we get to here, we don't have label listed yet.  */

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
    }				/* if we needed to grow  */

  dollar_labels[dollar_label_count] = label;
  dollar_label_instances[dollar_label_count] = 1;
  dollar_label_defines[dollar_label_count] = 1;
  ++dollar_label_count;
}

/* Caller must copy returned name: we re-use the area for the next name.

   The mth occurence of label n: is turned into the symbol "Ln^Am"
   where n is the label number and m is the instance number. "L" makes
   it a label discarded unless debugging and "^A"('\1') ensures no
   ordinary symbol SHOULD get the same name as a local label
   symbol. The first "4:" is "L4^A1" - the m numbers begin at 1.

   fb labels get the same treatment, except that ^B is used in place
   of ^A.  */

char *				/* Return local label name.  */
dollar_label_name (n, augend)
     register long n;		/* we just saw "n$:" : n a number.  */
     register int augend;	/* 0 for current instance, 1 for new instance.  */
{
  long i;
  /* Returned to caller, then copied.  Used for created names ("4f").  */
  static char symbol_name_build[24];
  register char *p;
  register char *q;
  char symbol_name_temporary[20];	/* Build up a number, BACKWARDS.  */

  know (n >= 0);
  know (augend == 0 || augend == 1);
  p = symbol_name_build;
#ifdef LOCAL_LABEL_PREFIX
  *p++ = LOCAL_LABEL_PREFIX;
#endif
  *p++ = 'L';

  /* Next code just does sprintf( {}, "%d", n);  */
  /* Label number.  */
  q = symbol_name_temporary;
  for (*q++ = 0, i = n; i; ++q)
    {
      *q = i % 10 + '0';
      i /= 10;
    }
  while ((*p = *--q) != '\0')
    ++p;

  *p++ = DOLLAR_LABEL_CHAR;		/* ^A  */

  /* Instance number.  */
  q = symbol_name_temporary;
  for (*q++ = 0, i = dollar_label_instance (n) + augend; i; ++q)
    {
      *q = i % 10 + '0';
      i /= 10;
    }
  while ((*p++ = *--q) != '\0');;

  /* The label, as a '\0' ended string, starts at symbol_name_build.  */
  return symbol_name_build;
}

/* Sombody else's idea of local labels. They are made by "n:" where n
   is any decimal digit. Refer to them with
    "nb" for previous (backward) n:
   or "nf" for next (forward) n:.

   We do a little better and let n be any number, not just a single digit, but
   since the other guy's assembler only does ten, we treat the first ten
   specially.

   Like someone else's assembler, we have one set of local label counters for
   entire assembly, not one set per (sub)segment like in most assemblers. This
   implies that one can refer to a label in another segment, and indeed some
   crufty compilers have done just that.

   Since there could be a LOT of these things, treat them as a sparse
   array.  */

#define FB_LABEL_SPECIAL (10)

static long fb_low_counter[FB_LABEL_SPECIAL];
static long *fb_labels;
static long *fb_label_instances;
static long fb_label_count;
static long fb_label_max;

/* This must be more than FB_LABEL_SPECIAL.  */
#define FB_LABEL_BUMP_BY (FB_LABEL_SPECIAL + 6)

static void
fb_label_init ()
{
  memset ((void *) fb_low_counter, '\0', sizeof (fb_low_counter));
}

/* Add one to the instance number of this fb label.  */

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
	    }			/* if we find it  */
	}			/* for each existing label  */
    }

  /* If we get to here, we don't have label listed yet.  */

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
    }				/* if we needed to grow  */

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
	    }			/* if we find it  */
	}			/* for each existing label  */
    }

  /* We didn't find the label, so this must be a reference to the
     first instance.  */
  return 0;
}

/* Caller must copy returned name: we re-use the area for the next name.

   The mth occurence of label n: is turned into the symbol "Ln^Bm"
   where n is the label number and m is the instance number. "L" makes
   it a label discarded unless debugging and "^B"('\2') ensures no
   ordinary symbol SHOULD get the same name as a local label
   symbol. The first "4:" is "L4^B1" - the m numbers begin at 1.

   dollar labels get the same treatment, except that ^A is used in
   place of ^B.  */

char *				/* Return local label name.  */
fb_label_name (n, augend)
     long n;			/* We just saw "n:", "nf" or "nb" : n a number.  */
     long augend;		/* 0 for nb, 1 for n:, nf.  */
{
  long i;
  /* Returned to caller, then copied.  Used for created names ("4f").  */
  static char symbol_name_build[24];
  register char *p;
  register char *q;
  char symbol_name_temporary[20];	/* Build up a number, BACKWARDS.  */

  know (n >= 0);
  know (augend == 0 || augend == 1);
  p = symbol_name_build;
#ifdef LOCAL_LABEL_PREFIX
  *p++ = LOCAL_LABEL_PREFIX;
#endif
  *p++ = 'L';

  /* Next code just does sprintf( {}, "%d", n);  */
  /* Label number.  */
  q = symbol_name_temporary;
  for (*q++ = 0, i = n; i; ++q)
    {
      *q = i % 10 + '0';
      i /= 10;
    }
  while ((*p = *--q) != '\0')
    ++p;

  *p++ = LOCAL_LABEL_CHAR;		/* ^B  */

  /* Instance number.  */
  q = symbol_name_temporary;
  for (*q++ = 0, i = fb_label_instance (n) + augend; i; ++q)
    {
      *q = i % 10 + '0';
      i /= 10;
    }
  while ((*p++ = *--q) != '\0');;

  /* The label, as a '\0' ended string, starts at symbol_name_build.  */
  return (symbol_name_build);
}

/* Decode name that may have been generated by foo_label_name() above.
   If the name wasn't generated by foo_label_name(), then return it
   unaltered.  This is used for error messages.  */

char *
decode_local_label_name (s)
     char *s;
{
  char *p;
  char *symbol_decode;
  int label_number;
  int instance_number;
  char *type;
  const char *message_format;
  int index = 0;

#ifdef LOCAL_LABEL_PREFIX
  if (s[index] == LOCAL_LABEL_PREFIX)
    ++index;
#endif

  if (s[index] != 'L')
    return s;

  for (label_number = 0, p = s + index + 1; ISDIGIT (*p); ++p)
    label_number = (10 * label_number) + *p - '0';

  if (*p == DOLLAR_LABEL_CHAR)
    type = "dollar";
  else if (*p == LOCAL_LABEL_CHAR)
    type = "fb";
  else
    return s;

  for (instance_number = 0, p++; ISDIGIT (*p); ++p)
    instance_number = (10 * instance_number) + *p - '0';

  message_format = _("\"%d\" (instance number %d of a %s label)");
  symbol_decode = obstack_alloc (&notes, strlen (message_format) + 30);
  sprintf (symbol_decode, message_format, label_number, instance_number, type);

  return symbol_decode;
}

/* Get the value of a symbol.  */

valueT
S_GET_VALUE (s)
     symbolS *s;
{
#ifdef BFD_ASSEMBLER
  if (LOCAL_SYMBOL_CHECK (s))
    return resolve_symbol_value (s);
#endif

  if (!s->sy_resolved)
    {
      valueT val = resolve_symbol_value (s);
      if (!finalize_syms)
	return val;
    }
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
	as_bad (_("attempt to get value of unresolved symbol `%s'"),
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
#ifdef BFD_ASSEMBLER
  if (LOCAL_SYMBOL_CHECK (s))
    {
      ((struct local_symbol *) s)->lsy_value = val;
      return;
    }
#endif

  s->sy_value.X_op = O_constant;
  s->sy_value.X_add_number = (offsetT) val;
  s->sy_value.X_unsigned = 0;
}

void
copy_symbol_attributes (dest, src)
     symbolS *dest, *src;
{
  if (LOCAL_SYMBOL_CHECK (dest))
    dest = local_symbol_convert ((struct local_symbol *) dest);
  if (LOCAL_SYMBOL_CHECK (src))
    src = local_symbol_convert ((struct local_symbol *) src);

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
S_IS_FUNCTION (s)
     symbolS *s;
{
  flagword flags;

  if (LOCAL_SYMBOL_CHECK (s))
    return 0;

  flags = s->bsym->flags;

  return (flags & BSF_FUNCTION) != 0;
}

int
S_IS_EXTERNAL (s)
     symbolS *s;
{
  flagword flags;

  if (LOCAL_SYMBOL_CHECK (s))
    return 0;

  flags = s->bsym->flags;

  /* Sanity check.  */
  if ((flags & BSF_LOCAL) && (flags & BSF_GLOBAL))
    abort ();

  return (flags & BSF_GLOBAL) != 0;
}

int
S_IS_WEAK (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 0;
  return (s->bsym->flags & BSF_WEAK) != 0;
}

int
S_IS_COMMON (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 0;
  return bfd_is_com_section (s->bsym->section);
}

int
S_IS_DEFINED (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return ((struct local_symbol *) s)->lsy_section != undefined_section;
  return s->bsym->section != undefined_section;
}

int
S_IS_DEBUG (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 0;
  if (s->bsym->flags & BSF_DEBUGGING)
    return 1;
  return 0;
}

int
S_IS_LOCAL (s)
     symbolS *s;
{
  flagword flags;
  const char *name;

  if (LOCAL_SYMBOL_CHECK (s))
    return 1;

  flags = s->bsym->flags;

  /* Sanity check.  */
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
	  && (strchr (name, DOLLAR_LABEL_CHAR)
	      || strchr (name, LOCAL_LABEL_CHAR)
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

const char *
S_GET_NAME (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return ((struct local_symbol *) s)->lsy_name;
  return s->bsym->name;
}

segT
S_GET_SEGMENT (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return ((struct local_symbol *) s)->lsy_section;
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

  if (LOCAL_SYMBOL_CHECK (s))
    {
      if (seg == reg_section)
	s = local_symbol_convert ((struct local_symbol *) s);
      else
	{
	  ((struct local_symbol *) s)->lsy_section = seg;
	  return;
	}
    }

  if (s->bsym->flags & BSF_SECTION_SYM)
    {
      if (s->bsym->section != seg)
	abort ();
    }
  else
    s->bsym->section = seg;
}

void
S_SET_EXTERNAL (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  if ((s->bsym->flags & BSF_WEAK) != 0)
    {
      /* Let .weak override .global.  */
      return;
    }
  if (s->bsym->flags & BSF_SECTION_SYM)
    {
      char * file;
      unsigned int line;

      /* Do not reassign section symbols.  */
      as_where (& file, & line);
      as_warn_where (file, line,
		     _("section symbols are already global"));
      return;
    }
  s->bsym->flags |= BSF_GLOBAL;
  s->bsym->flags &= ~(BSF_LOCAL | BSF_WEAK);
}

void
S_CLEAR_EXTERNAL (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return;
  if ((s->bsym->flags & BSF_WEAK) != 0)
    {
      /* Let .weak override.  */
      return;
    }
  s->bsym->flags |= BSF_LOCAL;
  s->bsym->flags &= ~(BSF_GLOBAL | BSF_WEAK);
}

void
S_SET_WEAK (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  s->bsym->flags |= BSF_WEAK;
  s->bsym->flags &= ~(BSF_GLOBAL | BSF_LOCAL);
}

void
S_SET_NAME (s, name)
     symbolS *s;
     char *name;
{
  if (LOCAL_SYMBOL_CHECK (s))
    {
      ((struct local_symbol *) s)->lsy_name = name;
      return;
    }
  s->bsym->name = name;
}
#endif /* BFD_ASSEMBLER */

#ifdef SYMBOLS_NEED_BACKPOINTERS

/* Return the previous symbol in a chain.  */

symbolS *
symbol_previous (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    abort ();
  return s->sy_previous;
}

#endif /* SYMBOLS_NEED_BACKPOINTERS */

/* Return the next symbol in a chain.  */

symbolS *
symbol_next (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    abort ();
  return s->sy_next;
}

/* Return a pointer to the value of a symbol as an expression.  */

expressionS *
symbol_get_value_expression (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  return &s->sy_value;
}

/* Set the value of a symbol to an expression.  */

void
symbol_set_value_expression (s, exp)
     symbolS *s;
     const expressionS *exp;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  s->sy_value = *exp;
}

/* Set the frag of a symbol.  */

void
symbol_set_frag (s, f)
     symbolS *s;
     fragS *f;
{
#ifdef BFD_ASSEMBLER
  if (LOCAL_SYMBOL_CHECK (s))
    {
      local_symbol_set_frag ((struct local_symbol *) s, f);
      return;
    }
#endif
  s->sy_frag = f;
}

/* Return the frag of a symbol.  */

fragS *
symbol_get_frag (s)
     symbolS *s;
{
#ifdef BFD_ASSEMBLER
  if (LOCAL_SYMBOL_CHECK (s))
    return local_symbol_get_frag ((struct local_symbol *) s);
#endif
  return s->sy_frag;
}

/* Mark a symbol as having been used.  */

void
symbol_mark_used (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return;
  s->sy_used = 1;
}

/* Clear the mark of whether a symbol has been used.  */

void
symbol_clear_used (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  s->sy_used = 0;
}

/* Return whether a symbol has been used.  */

int
symbol_used_p (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 1;
  return s->sy_used;
}

/* Mark a symbol as having been used in a reloc.  */

void
symbol_mark_used_in_reloc (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  s->sy_used_in_reloc = 1;
}

/* Clear the mark of whether a symbol has been used in a reloc.  */

void
symbol_clear_used_in_reloc (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return;
  s->sy_used_in_reloc = 0;
}

/* Return whether a symbol has been used in a reloc.  */

int
symbol_used_in_reloc_p (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 0;
  return s->sy_used_in_reloc;
}

/* Mark a symbol as an MRI common symbol.  */

void
symbol_mark_mri_common (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  s->sy_mri_common = 1;
}

/* Clear the mark of whether a symbol is an MRI common symbol.  */

void
symbol_clear_mri_common (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return;
  s->sy_mri_common = 0;
}

/* Return whether a symbol is an MRI common symbol.  */

int
symbol_mri_common_p (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 0;
  return s->sy_mri_common;
}

/* Mark a symbol as having been written.  */

void
symbol_mark_written (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return;
  s->written = 1;
}

/* Clear the mark of whether a symbol has been written.  */

void
symbol_clear_written (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return;
  s->written = 0;
}

/* Return whether a symbol has been written.  */

int
symbol_written_p (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 0;
  return s->written;
}

/* Mark a symbol has having been resolved.  */

void
symbol_mark_resolved (s)
     symbolS *s;
{
#ifdef BFD_ASSEMBLER
  if (LOCAL_SYMBOL_CHECK (s))
    {
      local_symbol_mark_resolved ((struct local_symbol *) s);
      return;
    }
#endif
  s->sy_resolved = 1;
}

/* Return whether a symbol has been resolved.  */

int
symbol_resolved_p (s)
     symbolS *s;
{
#ifdef BFD_ASSEMBLER
  if (LOCAL_SYMBOL_CHECK (s))
    return local_symbol_resolved_p ((struct local_symbol *) s);
#endif
  return s->sy_resolved;
}

/* Return whether a symbol is a section symbol.  */

int
symbol_section_p (s)
     symbolS *s ATTRIBUTE_UNUSED;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 0;
#ifdef BFD_ASSEMBLER
  return (s->bsym->flags & BSF_SECTION_SYM) != 0;
#else
  /* FIXME.  */
  return 0;
#endif
}

/* Return whether a symbol is equated to another symbol.  */

int
symbol_equated_p (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 0;
  return s->sy_value.X_op == O_symbol;
}

/* Return whether a symbol is equated to another symbol, and should be
   treated specially when writing out relocs.  */

int
symbol_equated_reloc_p (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 0;
  /* X_op_symbol, normally not used for O_symbol, is set by
     resolve_symbol_value to flag expression syms that have been
     equated.  */
  return (s->sy_value.X_op == O_symbol
	  && ((s->sy_resolved && s->sy_value.X_op_symbol != NULL)
	      || ! S_IS_DEFINED (s)
	      || S_IS_COMMON (s)));
}

/* Return whether a symbol has a constant value.  */

int
symbol_constant_p (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    return 1;
  return s->sy_value.X_op == O_constant;
}

#ifdef BFD_ASSEMBLER

/* Return the BFD symbol for a symbol.  */

asymbol *
symbol_get_bfdsym (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  return s->bsym;
}

/* Set the BFD symbol for a symbol.  */

void
symbol_set_bfdsym (s, bsym)
     symbolS *s;
     asymbol *bsym;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  s->bsym = bsym;
}

#endif /* BFD_ASSEMBLER */

#ifdef OBJ_SYMFIELD_TYPE

/* Get a pointer to the object format information for a symbol.  */

OBJ_SYMFIELD_TYPE *
symbol_get_obj (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  return &s->sy_obj;
}

/* Set the object format information for a symbol.  */

void
symbol_set_obj (s, o)
     symbolS *s;
     OBJ_SYMFIELD_TYPE *o;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  s->sy_obj = *o;
}

#endif /* OBJ_SYMFIELD_TYPE */

#ifdef TC_SYMFIELD_TYPE

/* Get a pointer to the processor information for a symbol.  */

TC_SYMFIELD_TYPE *
symbol_get_tc (s)
     symbolS *s;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  return &s->sy_tc;
}

/* Set the processor information for a symbol.  */

void
symbol_set_tc (s, o)
     symbolS *s;
     TC_SYMFIELD_TYPE *o;
{
  if (LOCAL_SYMBOL_CHECK (s))
    s = local_symbol_convert ((struct local_symbol *) s);
  s->sy_tc = *o;
}

#endif /* TC_SYMFIELD_TYPE */

void
symbol_begin ()
{
  symbol_lastP = NULL;
  symbol_rootP = NULL;		/* In case we have 0 symbols (!!)  */
  sy_hash = hash_new ();
#ifdef BFD_ASSEMBLER
  local_hash = hash_new ();
#endif

  memset ((char *) (&abs_symbol), '\0', sizeof (abs_symbol));
#ifdef BFD_ASSEMBLER
#if defined (EMIT_SECTION_SYMBOLS) || !defined (RELOC_REQUIRES_SYMBOL)
  abs_symbol.bsym = bfd_abs_section.symbol;
#endif
#else
  /* Can't initialise a union. Sigh.  */
  S_SET_SEGMENT (&abs_symbol, absolute_section);
#endif
  abs_symbol.sy_value.X_op = O_constant;
  abs_symbol.sy_frag = &zero_address_frag;

  if (LOCAL_LABELS_FB)
    fb_label_init ();
}

int indent_level;

/* Maximum indent level.
   Available for modification inside a gdb session.  */
int max_indent_level = 8;

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

  if (LOCAL_SYMBOL_CHECK (sym))
    {
#ifdef BFD_ASSEMBLER
      struct local_symbol *locsym = (struct local_symbol *) sym;
      if (local_symbol_get_frag (locsym) != &zero_address_frag
	  && local_symbol_get_frag (locsym) != NULL)
	fprintf (file, " frag %lx", (long) local_symbol_get_frag (locsym));
      if (local_symbol_resolved_p (locsym))
	fprintf (file, " resolved");
      fprintf (file, " local");
#endif
    }
  else
    {
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
    }
  fprintf (file, " %s", segment_name (S_GET_SEGMENT (sym)));
  if (symbol_resolved_p (sym))
    {
      segT s = S_GET_SEGMENT (sym);

      if (s != undefined_section
	  && s != expr_section)
	fprintf (file, " %lx", (long) S_GET_VALUE (sym));
    }
  else if (indent_level < max_indent_level
	   && S_GET_SEGMENT (sym) != undefined_section)
    {
      indent_level++;
      fprintf (file, "\n%*s<", indent_level * 4, "");
#ifdef BFD_ASSEMBLER
      if (LOCAL_SYMBOL_CHECK (sym))
	fprintf (file, "constant %lx",
		 (long) ((struct local_symbol *) sym)->lsy_value);
      else
#endif
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

static void
print_binary (file, name, exp)
     FILE *file;
     const char *name;
     expressionS *exp;
{
  indent_level++;
  fprintf (file, "%s\n%*s<", name, indent_level * 4, "");
  print_symbol_value_1 (file, exp->X_add_symbol);
  fprintf (file, ">\n%*s<", indent_level * 4, "");
  print_symbol_value_1 (file, exp->X_op_symbol);
  fprintf (file, ">");
  indent_level--;
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
      print_binary (file, "multiply", exp);
      break;
    case O_divide:
      print_binary (file, "divide", exp);
      break;
    case O_modulus:
      print_binary (file, "modulus", exp);
      break;
    case O_left_shift:
      print_binary (file, "lshift", exp);
      break;
    case O_right_shift:
      print_binary (file, "rshift", exp);
      break;
    case O_bit_inclusive_or:
      print_binary (file, "bit_ior", exp);
      break;
    case O_bit_exclusive_or:
      print_binary (file, "bit_xor", exp);
      break;
    case O_bit_and:
      print_binary (file, "bit_and", exp);
      break;
    case O_eq:
      print_binary (file, "eq", exp);
      break;
    case O_ne:
      print_binary (file, "ne", exp);
      break;
    case O_lt:
      print_binary (file, "lt", exp);
      break;
    case O_le:
      print_binary (file, "le", exp);
      break;
    case O_ge:
      print_binary (file, "ge", exp);
      break;
    case O_gt:
      print_binary (file, "gt", exp);
      break;
    case O_logical_and:
      print_binary (file, "logical_and", exp);
      break;
    case O_logical_or:
      print_binary (file, "logical_or", exp);
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
#ifdef BFD_ASSEMBLER
  hash_print_statistics (file, "mini local symbol table", local_hash);
  fprintf (file, "%lu mini local symbols created, %lu converted\n",
	   local_symbol_count, local_symbol_conversion_count);
#endif
}
