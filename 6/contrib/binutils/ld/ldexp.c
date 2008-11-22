/* This module handles expression trees.
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002, 2003, 2004
   Free Software Foundation, Inc.
   Written by Steve Chamberlain of Cygnus Support <sac@cygnus.com>.

This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

/* This module is in charge of working out the contents of expressions.

   It has to keep track of the relative/absness of a symbol etc. This
   is done by keeping all values in a struct (an etree_value_type)
   which contains a value, a section to which it is relative and a
   valid bit.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"

#include "ld.h"
#include "ldmain.h"
#include "ldmisc.h"
#include "ldexp.h"
#include <ldgram.h>
#include "ldlang.h"
#include "libiberty.h"
#include "safe-ctype.h"

static etree_value_type exp_fold_tree_no_dot
  (etree_type *, lang_output_section_statement_type *, lang_phase_type);
static bfd_vma align_n
  (bfd_vma, bfd_vma);

struct exp_data_seg exp_data_seg;

/* Print the string representation of the given token.  Surround it
   with spaces if INFIX_P is TRUE.  */

static void
exp_print_token (token_code_type code, int infix_p)
{
  static const struct
  {
    token_code_type code;
    char * name;
  }
  table[] =
  {
    { INT, "int" },
    { NAME, "NAME" },
    { PLUSEQ, "+=" },
    { MINUSEQ, "-=" },
    { MULTEQ, "*=" },
    { DIVEQ, "/=" },
    { LSHIFTEQ, "<<=" },
    { RSHIFTEQ, ">>=" },
    { ANDEQ, "&=" },
    { OREQ, "|=" },
    { OROR, "||" },
    { ANDAND, "&&" },
    { EQ, "==" },
    { NE, "!=" },
    { LE, "<=" },
    { GE, ">=" },
    { LSHIFT, "<<" },
    { RSHIFT, ">>" },
    { ALIGN_K, "ALIGN" },
    { BLOCK, "BLOCK" },
    { QUAD, "QUAD" },
    { SQUAD, "SQUAD" },
    { LONG, "LONG" },
    { SHORT, "SHORT" },
    { BYTE, "BYTE" },
    { SECTIONS, "SECTIONS" },
    { SIZEOF_HEADERS, "SIZEOF_HEADERS" },
    { MEMORY, "MEMORY" },
    { DEFINED, "DEFINED" },
    { TARGET_K, "TARGET" },
    { SEARCH_DIR, "SEARCH_DIR" },
    { MAP, "MAP" },
    { ENTRY, "ENTRY" },
    { NEXT, "NEXT" },
    { SIZEOF, "SIZEOF" },
    { ADDR, "ADDR" },
    { LOADADDR, "LOADADDR" },
    { MAX_K, "MAX_K" },
    { REL, "relocatable" },
    { DATA_SEGMENT_ALIGN, "DATA_SEGMENT_ALIGN" },
    { DATA_SEGMENT_END, "DATA_SEGMENT_END" }
  };
  unsigned int idx;

  for (idx = 0; idx < ARRAY_SIZE (table); idx++)
    if (table[idx].code == code)
      break;

  if (infix_p)
    fputc (' ', config.map_file);

  if (idx < ARRAY_SIZE (table))
    fputs (table[idx].name, config.map_file);
  else if (code < 127)
    fputc (code, config.map_file);
  else
    fprintf (config.map_file, "<code %d>", code);

  if (infix_p)
    fputc (' ', config.map_file);
}

static void
make_abs (etree_value_type *ptr)
{
  asection *s = ptr->section->bfd_section;
  ptr->value += s->vma;
  ptr->section = abs_output_section;
}

static etree_value_type
new_abs (bfd_vma value)
{
  etree_value_type new;
  new.valid_p = TRUE;
  new.section = abs_output_section;
  new.value = value;
  return new;
}

etree_type *
exp_intop (bfd_vma value)
{
  etree_type *new = stat_alloc (sizeof (new->value));
  new->type.node_code = INT;
  new->value.value = value;
  new->value.str = NULL;
  new->type.node_class = etree_value;
  return new;
}

etree_type *
exp_bigintop (bfd_vma value, char *str)
{
  etree_type *new = stat_alloc (sizeof (new->value));
  new->type.node_code = INT;
  new->value.value = value;
  new->value.str = str;
  new->type.node_class = etree_value;
  return new;
}

/* Build an expression representing an unnamed relocatable value.  */

etree_type *
exp_relop (asection *section, bfd_vma value)
{
  etree_type *new = stat_alloc (sizeof (new->rel));
  new->type.node_code = REL;
  new->type.node_class = etree_rel;
  new->rel.section = section;
  new->rel.value = value;
  return new;
}

static etree_value_type
new_rel (bfd_vma value,
	 char *str,
	 lang_output_section_statement_type *section)
{
  etree_value_type new;
  new.valid_p = TRUE;
  new.value = value;
  new.str = str;
  new.section = section;
  return new;
}

static etree_value_type
new_rel_from_section (bfd_vma value,
		      lang_output_section_statement_type *section)
{
  etree_value_type new;
  new.valid_p = TRUE;
  new.value = value;
  new.str = NULL;
  new.section = section;

  new.value -= section->bfd_section->vma;

  return new;
}

static etree_value_type
fold_unary (etree_type *tree,
	    lang_output_section_statement_type *current_section,
	    lang_phase_type allocation_done,
	    bfd_vma dot,
	    bfd_vma *dotp)
{
  etree_value_type result;

  result = exp_fold_tree (tree->unary.child,
			  current_section,
			  allocation_done, dot, dotp);
  if (result.valid_p)
    {
      switch (tree->type.node_code)
	{
	case ALIGN_K:
	  if (allocation_done != lang_first_phase_enum)
	    result = new_rel_from_section (align_n (dot, result.value),
					   current_section);
	  else
	    result.valid_p = FALSE;
	  break;

	case ABSOLUTE:
	  if (allocation_done != lang_first_phase_enum)
	    {
	      result.value += result.section->bfd_section->vma;
	      result.section = abs_output_section;
	    }
	  else
	    result.valid_p = FALSE;
	  break;

	case '~':
	  make_abs (&result);
	  result.value = ~result.value;
	  break;

	case '!':
	  make_abs (&result);
	  result.value = !result.value;
	  break;

	case '-':
	  make_abs (&result);
	  result.value = -result.value;
	  break;

	case NEXT:
	  /* Return next place aligned to value.  */
	  if (allocation_done == lang_allocating_phase_enum)
	    {
	      make_abs (&result);
	      result.value = align_n (dot, result.value);
	    }
	  else
	    result.valid_p = FALSE;
	  break;

	case DATA_SEGMENT_END:
	  if (allocation_done != lang_first_phase_enum
	      && current_section == abs_output_section
	      && (exp_data_seg.phase == exp_dataseg_align_seen
		  || exp_data_seg.phase == exp_dataseg_adjust
		  || allocation_done != lang_allocating_phase_enum))
	    {
	      if (exp_data_seg.phase == exp_dataseg_align_seen)
		{
		  exp_data_seg.phase = exp_dataseg_end_seen;
		  exp_data_seg.end = result.value;
		}
	    }
	  else
	    result.valid_p = FALSE;
	  break;

	default:
	  FAIL ();
	  break;
	}
    }

  return result;
}

static etree_value_type
fold_binary (etree_type *tree,
	     lang_output_section_statement_type *current_section,
	     lang_phase_type allocation_done,
	     bfd_vma dot,
	     bfd_vma *dotp)
{
  etree_value_type result;

  result = exp_fold_tree (tree->binary.lhs, current_section,
			  allocation_done, dot, dotp);
  if (result.valid_p)
    {
      etree_value_type other;

      other = exp_fold_tree (tree->binary.rhs,
			     current_section,
			     allocation_done, dot, dotp);
      if (other.valid_p)
	{
	  /* If the values are from different sections, or this is an
	     absolute expression, make both the source arguments
	     absolute.  However, adding or subtracting an absolute
	     value from a relative value is meaningful, and is an
	     exception.  */
	  if (current_section != abs_output_section
	      && (other.section == abs_output_section
		  || (result.section == abs_output_section
		      && tree->type.node_code == '+'))
	      && (tree->type.node_code == '+'
		  || tree->type.node_code == '-'))
	    {
	      if (other.section != abs_output_section)
		{
		  /* Keep the section of the other term.  */
		  if (tree->type.node_code == '+')
		    other.value = result.value + other.value;
		  else
		    other.value = result.value - other.value;
		  return other;
		}
	    }
	  else if (result.section != other.section
		   || current_section == abs_output_section)
	    {
	      make_abs (&result);
	      make_abs (&other);
	    }

	  switch (tree->type.node_code)
	    {
	    case '%':
	      if (other.value == 0)
		einfo (_("%F%S %% by zero\n"));
	      result.value = ((bfd_signed_vma) result.value
			      % (bfd_signed_vma) other.value);
	      break;

	    case '/':
	      if (other.value == 0)
		einfo (_("%F%S / by zero\n"));
	      result.value = ((bfd_signed_vma) result.value
			      / (bfd_signed_vma) other.value);
	      break;

#define BOP(x,y) case x : result.value = result.value y other.value; break;
	      BOP ('+', +);
	      BOP ('*', *);
	      BOP ('-', -);
	      BOP (LSHIFT, <<);
	      BOP (RSHIFT, >>);
	      BOP (EQ, ==);
	      BOP (NE, !=);
	      BOP ('<', <);
	      BOP ('>', >);
	      BOP (LE, <=);
	      BOP (GE, >=);
	      BOP ('&', &);
	      BOP ('^', ^);
	      BOP ('|', |);
	      BOP (ANDAND, &&);
	      BOP (OROR, ||);

	    case MAX_K:
	      if (result.value < other.value)
		result = other;
	      break;

	    case MIN_K:
	      if (result.value > other.value)
		result = other;
	      break;

	    case ALIGN_K:
	      result.value = align_n (result.value, other.value);
	      break;
	      
	    case DATA_SEGMENT_ALIGN:
	      if (allocation_done != lang_first_phase_enum
		  && current_section == abs_output_section
		  && (exp_data_seg.phase == exp_dataseg_none
		      || exp_data_seg.phase == exp_dataseg_adjust
		      || allocation_done != lang_allocating_phase_enum))
		{
		  bfd_vma maxpage = result.value;

		  result.value = align_n (dot, maxpage);
		  if (exp_data_seg.phase != exp_dataseg_adjust)
		    {
		      result.value += dot & (maxpage - 1);
		      if (allocation_done == lang_allocating_phase_enum)
			{
			  exp_data_seg.phase = exp_dataseg_align_seen;
			  exp_data_seg.base = result.value;
			  exp_data_seg.pagesize = other.value;
			}
		    }
		  else if (other.value < maxpage)
		    result.value += (dot + other.value - 1)
				    & (maxpage - other.value);
		}
	      else
		result.valid_p = FALSE;
	      break;

	    default:
	      FAIL ();
	    }
	}
      else
	{
	  result.valid_p = FALSE;
	}
    }

  return result;
}

static etree_value_type
fold_trinary (etree_type *tree,
	      lang_output_section_statement_type *current_section,
	      lang_phase_type allocation_done,
	      bfd_vma dot,
	      bfd_vma *dotp)
{
  etree_value_type result;

  result = exp_fold_tree (tree->trinary.cond, current_section,
			  allocation_done, dot, dotp);
  if (result.valid_p)
    result = exp_fold_tree ((result.value
			     ? tree->trinary.lhs
			     : tree->trinary.rhs),
			    current_section,
			    allocation_done, dot, dotp);

  return result;
}

static etree_value_type
fold_name (etree_type *tree,
	   lang_output_section_statement_type *current_section,
	   lang_phase_type allocation_done,
	   bfd_vma dot)
{
  etree_value_type result;

  result.valid_p = FALSE;
  
  switch (tree->type.node_code)
    {
    case SIZEOF_HEADERS:
      if (allocation_done != lang_first_phase_enum)
	result = new_abs (bfd_sizeof_headers (output_bfd,
					      link_info.relocatable));
      break;
    case DEFINED:
      if (allocation_done == lang_first_phase_enum)
	lang_track_definedness (tree->name.name);
      else
	{
	  struct bfd_link_hash_entry *h;
	  int def_iteration
	    = lang_symbol_definition_iteration (tree->name.name);

	  h = bfd_wrapped_link_hash_lookup (output_bfd, &link_info,
					    tree->name.name,
					    FALSE, FALSE, TRUE);
	  result.value = (h != NULL
			  && (h->type == bfd_link_hash_defined
			      || h->type == bfd_link_hash_defweak
			      || h->type == bfd_link_hash_common)
			  && (def_iteration == lang_statement_iteration
			      || def_iteration == -1));
	  result.section = abs_output_section;
	  result.valid_p = TRUE;
	}
      break;
    case NAME:
      if (tree->name.name[0] == '.' && tree->name.name[1] == 0)
	{
	  if (allocation_done != lang_first_phase_enum)
	    result = new_rel_from_section (dot, current_section);
	}
      else if (allocation_done != lang_first_phase_enum)
	{
	  struct bfd_link_hash_entry *h;

	  h = bfd_wrapped_link_hash_lookup (output_bfd, &link_info,
					    tree->name.name,
					    TRUE, FALSE, TRUE);
	  if (!h)
	    einfo (_("%P%F: bfd_link_hash_lookup failed: %E\n"));
	  else if (h->type == bfd_link_hash_defined
		   || h->type == bfd_link_hash_defweak)
	    {
	      if (bfd_is_abs_section (h->u.def.section))
		result = new_abs (h->u.def.value);
	      else if (allocation_done == lang_final_phase_enum
		       || allocation_done == lang_allocating_phase_enum)
		{
		  asection *output_section;

		  output_section = h->u.def.section->output_section;
		  if (output_section == NULL)
		    einfo (_("%X%S: unresolvable symbol `%s' referenced in expression\n"),
			   tree->name.name);
		  else
		    {
		      lang_output_section_statement_type *os;

		      os = (lang_output_section_statement_lookup
			    (bfd_get_section_name (output_bfd,
						   output_section)));

		      /* FIXME: Is this correct if this section is
			 being linked with -R?  */
		      result = new_rel ((h->u.def.value
					 + h->u.def.section->output_offset),
					NULL,
					os);
		    }
		}
	    }
	  else if (allocation_done == lang_final_phase_enum)
	    einfo (_("%F%S: undefined symbol `%s' referenced in expression\n"),
		   tree->name.name);
	  else if (h->type == bfd_link_hash_new)
	    {
	      h->type = bfd_link_hash_undefined;
	      h->u.undef.abfd = NULL;
	      bfd_link_add_undef (link_info.hash, h);
	    }
	}
      break;

    case ADDR:
      if (allocation_done != lang_first_phase_enum)
	{
	  lang_output_section_statement_type *os;

	  os = lang_output_section_find (tree->name.name);
	  if (os && os->processed > 0)
	    result = new_rel (0, NULL, os);
	}
      break;

    case LOADADDR:
      if (allocation_done != lang_first_phase_enum)
	{
	  lang_output_section_statement_type *os;

	  os = lang_output_section_find (tree->name.name);
	  if (os && os->processed != 0)
	    {
	      if (os->load_base == NULL)
		result = new_rel (0, NULL, os);
	      else
		result = exp_fold_tree_no_dot (os->load_base,
					       abs_output_section,
					       allocation_done);
	    }
	}
      break;

    case SIZEOF:
      if (allocation_done != lang_first_phase_enum)
	{
	  int opb = bfd_octets_per_byte (output_bfd);
	  lang_output_section_statement_type *os;

	  os = lang_output_section_find (tree->name.name);
	  if (os && os->processed > 0)
	    result = new_abs (os->bfd_section->_raw_size / opb);
	}
      break;

    default:
      FAIL ();
      break;
    }

  return result;
}

etree_value_type
exp_fold_tree (etree_type *tree,
	       lang_output_section_statement_type *current_section,
	       lang_phase_type allocation_done,
	       bfd_vma dot,
	       bfd_vma *dotp)
{
  etree_value_type result;

  if (tree == NULL)
    {
      result.valid_p = FALSE;
      return result;
    }

  switch (tree->type.node_class)
    {
    case etree_value:
      result = new_rel (tree->value.value, tree->value.str, current_section);
      break;

    case etree_rel:
      if (allocation_done != lang_final_phase_enum)
	result.valid_p = FALSE;
      else
	result = new_rel ((tree->rel.value
			   + tree->rel.section->output_section->vma
			   + tree->rel.section->output_offset),
			  NULL,
			  current_section);
      break;

    case etree_assert:
      result = exp_fold_tree (tree->assert_s.child,
			      current_section,
			      allocation_done, dot, dotp);
      if (result.valid_p)
	{
	  if (! result.value)
	    einfo ("%F%P: %s\n", tree->assert_s.message);
	  return result;
	}
      break;

    case etree_unary:
      result = fold_unary (tree, current_section, allocation_done,
			   dot, dotp);
      break;

    case etree_binary:
      result = fold_binary (tree, current_section, allocation_done,
			    dot, dotp);
      break;

    case etree_trinary:
      result = fold_trinary (tree, current_section, allocation_done,
			     dot, dotp);
      break;

    case etree_assign:
    case etree_provide:
    case etree_provided:
      if (tree->assign.dst[0] == '.' && tree->assign.dst[1] == 0)
	{
	  /* Assignment to dot can only be done during allocation.  */
	  if (tree->type.node_class != etree_assign)
	    einfo (_("%F%S can not PROVIDE assignment to location counter\n"));
	  if (allocation_done == lang_allocating_phase_enum
	      || (allocation_done == lang_final_phase_enum
		  && current_section == abs_output_section))
	    {
	      result = exp_fold_tree (tree->assign.src,
				      current_section,
				      allocation_done, dot,
				      dotp);
	      if (! result.valid_p)
		einfo (_("%F%S invalid assignment to location counter\n"));
	      else
		{
		  if (current_section == NULL)
		    einfo (_("%F%S assignment to location counter invalid outside of SECTION\n"));
		  else
		    {
		      bfd_vma nextdot;

		      nextdot = (result.value
				 + current_section->bfd_section->vma);
		      if (nextdot < dot
			  && current_section != abs_output_section)
			einfo (_("%F%S cannot move location counter backwards (from %V to %V)\n"),
			       dot, nextdot);
		      else
			*dotp = nextdot;
		    }
		}
	    }
	}
      else
	{
	  result = exp_fold_tree (tree->assign.src,
				  current_section, allocation_done,
				  dot, dotp);
	  if (result.valid_p)
	    {
	      bfd_boolean create;
	      struct bfd_link_hash_entry *h;

	      if (tree->type.node_class == etree_assign)
		create = TRUE;
	      else
		create = FALSE;
	      h = bfd_link_hash_lookup (link_info.hash, tree->assign.dst,
					create, FALSE, TRUE);
	      if (h == NULL)
		{
		  if (create)
		    einfo (_("%P%F:%s: hash creation failed\n"),
			   tree->assign.dst);
		}
	      else if (tree->type.node_class == etree_provide
		       && h->type != bfd_link_hash_new
		       && h->type != bfd_link_hash_undefined
		       && h->type != bfd_link_hash_common)
		{
		  /* Do nothing.  The symbol was defined by some
		     object.  */
		}
	      else
		{
		  /* FIXME: Should we worry if the symbol is already
		     defined?  */
		  lang_update_definedness (tree->assign.dst, h);
		  h->type = bfd_link_hash_defined;
		  h->u.def.value = result.value;
		  h->u.def.section = result.section->bfd_section;
		  if (tree->type.node_class == etree_provide)
		    tree->type.node_class = etree_provided;
		}
	    }
	}
      break;

    case etree_name:
      result = fold_name (tree, current_section, allocation_done, dot);
      break;

    default:
      FAIL ();
      break;
    }

  return result;
}

static etree_value_type
exp_fold_tree_no_dot (etree_type *tree,
		      lang_output_section_statement_type *current_section,
		      lang_phase_type allocation_done)
{
  return exp_fold_tree (tree, current_section, allocation_done, 0, NULL);
}

etree_type *
exp_binop (int code, etree_type *lhs, etree_type *rhs)
{
  etree_type value, *new;
  etree_value_type r;

  value.type.node_code = code;
  value.binary.lhs = lhs;
  value.binary.rhs = rhs;
  value.type.node_class = etree_binary;
  r = exp_fold_tree_no_dot (&value,
			    abs_output_section,
			    lang_first_phase_enum);
  if (r.valid_p)
    {
      return exp_intop (r.value);
    }
  new = stat_alloc (sizeof (new->binary));
  memcpy (new, &value, sizeof (new->binary));
  return new;
}

etree_type *
exp_trinop (int code, etree_type *cond, etree_type *lhs, etree_type *rhs)
{
  etree_type value, *new;
  etree_value_type r;
  value.type.node_code = code;
  value.trinary.lhs = lhs;
  value.trinary.cond = cond;
  value.trinary.rhs = rhs;
  value.type.node_class = etree_trinary;
  r = exp_fold_tree_no_dot (&value, NULL, lang_first_phase_enum);
  if (r.valid_p)
    return exp_intop (r.value);

  new = stat_alloc (sizeof (new->trinary));
  memcpy (new, &value, sizeof (new->trinary));
  return new;
}

etree_type *
exp_unop (int code, etree_type *child)
{
  etree_type value, *new;

  etree_value_type r;
  value.unary.type.node_code = code;
  value.unary.child = child;
  value.unary.type.node_class = etree_unary;
  r = exp_fold_tree_no_dot (&value, abs_output_section,
			    lang_first_phase_enum);
  if (r.valid_p)
    return exp_intop (r.value);

  new = stat_alloc (sizeof (new->unary));
  memcpy (new, &value, sizeof (new->unary));
  return new;
}

etree_type *
exp_nameop (int code, const char *name)
{
  etree_type value, *new;
  etree_value_type r;
  value.name.type.node_code = code;
  value.name.name = name;
  value.name.type.node_class = etree_name;

  r = exp_fold_tree_no_dot (&value, NULL, lang_first_phase_enum);
  if (r.valid_p)
    return exp_intop (r.value);

  new = stat_alloc (sizeof (new->name));
  memcpy (new, &value, sizeof (new->name));
  return new;

}

etree_type *
exp_assop (int code, const char *dst, etree_type *src)
{
  etree_type value, *new;

  value.assign.type.node_code = code;

  value.assign.src = src;
  value.assign.dst = dst;
  value.assign.type.node_class = etree_assign;

#if 0
  if (exp_fold_tree_no_dot (&value, &result))
    return exp_intop (result);
#endif
  new = stat_alloc (sizeof (new->assign));
  memcpy (new, &value, sizeof (new->assign));
  return new;
}

/* Handle PROVIDE.  */

etree_type *
exp_provide (const char *dst, etree_type *src)
{
  etree_type *n;

  n = stat_alloc (sizeof (n->assign));
  n->assign.type.node_code = '=';
  n->assign.type.node_class = etree_provide;
  n->assign.src = src;
  n->assign.dst = dst;
  return n;
}

/* Handle ASSERT.  */

etree_type *
exp_assert (etree_type *exp, const char *message)
{
  etree_type *n;

  n = stat_alloc (sizeof (n->assert_s));
  n->assert_s.type.node_code = '!';
  n->assert_s.type.node_class = etree_assert;
  n->assert_s.child = exp;
  n->assert_s.message = message;
  return n;
}

void
exp_print_tree (etree_type *tree)
{
  if (config.map_file == NULL)
    config.map_file = stderr;

  if (tree == NULL)
    {
      minfo ("NULL TREE\n");
      return;
    }

  switch (tree->type.node_class)
    {
    case etree_value:
      minfo ("0x%v", tree->value.value);
      return;
    case etree_rel:
      if (tree->rel.section->owner != NULL)
	minfo ("%B:", tree->rel.section->owner);
      minfo ("%s+0x%v", tree->rel.section->name, tree->rel.value);
      return;
    case etree_assign:
#if 0
      if (tree->assign.dst->sdefs != NULL)
	fprintf (config.map_file, "%s (%x) ", tree->assign.dst->name,
		 tree->assign.dst->sdefs->value);
      else
	fprintf (config.map_file, "%s (UNDEFINED)", tree->assign.dst->name);
#endif
      fprintf (config.map_file, "%s", tree->assign.dst);
      exp_print_token (tree->type.node_code, TRUE);
      exp_print_tree (tree->assign.src);
      break;
    case etree_provide:
    case etree_provided:
      fprintf (config.map_file, "PROVIDE (%s, ", tree->assign.dst);
      exp_print_tree (tree->assign.src);
      fprintf (config.map_file, ")");
      break;
    case etree_binary:
      fprintf (config.map_file, "(");
      exp_print_tree (tree->binary.lhs);
      exp_print_token (tree->type.node_code, TRUE);
      exp_print_tree (tree->binary.rhs);
      fprintf (config.map_file, ")");
      break;
    case etree_trinary:
      exp_print_tree (tree->trinary.cond);
      fprintf (config.map_file, "?");
      exp_print_tree (tree->trinary.lhs);
      fprintf (config.map_file, ":");
      exp_print_tree (tree->trinary.rhs);
      break;
    case etree_unary:
      exp_print_token (tree->unary.type.node_code, FALSE);
      if (tree->unary.child)
	{
	  fprintf (config.map_file, " (");
	  exp_print_tree (tree->unary.child);
	  fprintf (config.map_file, ")");
	}
      break;

    case etree_assert:
      fprintf (config.map_file, "ASSERT (");
      exp_print_tree (tree->assert_s.child);
      fprintf (config.map_file, ", %s)", tree->assert_s.message);
      break;

    case etree_undef:
      fprintf (config.map_file, "????????");
      break;
    case etree_name:
      if (tree->type.node_code == NAME)
	{
	  fprintf (config.map_file, "%s", tree->name.name);
	}
      else
	{
	  exp_print_token (tree->type.node_code, FALSE);
	  if (tree->name.name)
	    fprintf (config.map_file, " (%s)", tree->name.name);
	}
      break;
    default:
      FAIL ();
      break;
    }
}

bfd_vma
exp_get_vma (etree_type *tree,
	     bfd_vma def,
	     char *name,
	     lang_phase_type allocation_done)
{
  etree_value_type r;

  if (tree != NULL)
    {
      r = exp_fold_tree_no_dot (tree, abs_output_section, allocation_done);
      if (! r.valid_p && name != NULL)
	einfo (_("%F%S nonconstant expression for %s\n"), name);
      return r.value;
    }
  else
    return def;
}

int
exp_get_value_int (etree_type *tree,
		   int def,
		   char *name,
		   lang_phase_type allocation_done)
{
  return exp_get_vma (tree, def, name, allocation_done);
}

fill_type *
exp_get_fill (etree_type *tree,
	      fill_type *def,
	      char *name,
	      lang_phase_type allocation_done)
{
  fill_type *fill;
  etree_value_type r;
  size_t len;
  unsigned int val;

  if (tree == NULL)
    return def;

  r = exp_fold_tree_no_dot (tree, abs_output_section, allocation_done);
  if (! r.valid_p && name != NULL)
    einfo (_("%F%S nonconstant expression for %s\n"), name);

  if (r.str != NULL && (len = strlen (r.str)) != 0)
    {
      unsigned char *dst;
      unsigned char *s;
      fill = xmalloc ((len + 1) / 2 + sizeof (*fill) - 1);
      fill->size = (len + 1) / 2;
      dst = fill->data;
      s = r.str;
      val = 0;
      do
	{
	  unsigned int digit;

	  digit = *s++ - '0';
	  if (digit > 9)
	    digit = (digit - 'A' + '0' + 10) & 0xf;
	  val <<= 4;
	  val += digit;
	  --len;
	  if ((len & 1) == 0)
	    {
	      *dst++ = val;
	      val = 0;
	    }
	}
      while (len != 0);
    }
  else
    {
      fill = xmalloc (4 + sizeof (*fill) - 1);
      val = r.value;
      fill->data[0] = (val >> 24) & 0xff;
      fill->data[1] = (val >> 16) & 0xff;
      fill->data[2] = (val >>  8) & 0xff;
      fill->data[3] = (val >>  0) & 0xff;
      fill->size = 4;
    }
  return fill;
}

bfd_vma
exp_get_abs_int (etree_type *tree,
		 int def ATTRIBUTE_UNUSED,
		 char *name,
		 lang_phase_type allocation_done)
{
  etree_value_type res;
  res = exp_fold_tree_no_dot (tree, abs_output_section, allocation_done);

  if (res.valid_p)
    res.value += res.section->bfd_section->vma;
  else
    einfo (_("%F%S non constant expression for %s\n"), name);

  return res.value;
}

static bfd_vma
align_n (bfd_vma value, bfd_vma align)
{
  if (align <= 1)
    return value;

  value = (value + align - 1) / align;
  return value * align;
}
