/* This module handles expression trees.
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002
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
#include "ldgram.h"
#include "ldlang.h"
#include "libiberty.h"
#include "safe-ctype.h"

static void exp_print_token PARAMS ((token_code_type code, int infix_p));
static void make_abs PARAMS ((etree_value_type *ptr));
static etree_value_type new_abs PARAMS ((bfd_vma value));
static void check PARAMS ((lang_output_section_statement_type *os,
			   const char *name, const char *op));
static etree_value_type new_rel
  PARAMS ((bfd_vma, char *, lang_output_section_statement_type *section));
static etree_value_type new_rel_from_section
  PARAMS ((bfd_vma value, lang_output_section_statement_type *section));
static etree_value_type fold_unary
  PARAMS ((etree_type *tree,
	   lang_output_section_statement_type *current_section,
	   lang_phase_type allocation_done,
	   bfd_vma dot, bfd_vma *dotp));
static etree_value_type fold_binary
  PARAMS ((etree_type *tree,
	   lang_output_section_statement_type *current_section,
	   lang_phase_type allocation_done,
	   bfd_vma dot, bfd_vma *dotp));
static etree_value_type fold_trinary
  PARAMS ((etree_type *tree,
	   lang_output_section_statement_type *current_section,
	   lang_phase_type allocation_done,
	   bfd_vma dot, bfd_vma *dotp));
static etree_value_type fold_name
  PARAMS ((etree_type *tree,
	   lang_output_section_statement_type *current_section,
	   lang_phase_type allocation_done,
	   bfd_vma dot));
static etree_value_type exp_fold_tree_no_dot
  PARAMS ((etree_type *tree,
	   lang_output_section_statement_type *current_section,
	   lang_phase_type allocation_done));

struct exp_data_seg exp_data_seg;

/* Print the string representation of the given token.  Surround it
   with spaces if INFIX_P is true.  */

static void
exp_print_token (code, infix_p)
     token_code_type code;
     int infix_p;
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
    { REL, "relocateable" },
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
make_abs (ptr)
     etree_value_type *ptr;
{
  asection *s = ptr->section->bfd_section;
  ptr->value += s->vma;
  ptr->section = abs_output_section;
}

static etree_value_type
new_abs (value)
     bfd_vma value;
{
  etree_value_type new;
  new.valid_p = true;
  new.section = abs_output_section;
  new.value = value;
  return new;
}

static void
check (os, name, op)
     lang_output_section_statement_type *os;
     const char *name;
     const char *op;
{
  if (os == NULL)
    einfo (_("%F%P: %s uses undefined section %s\n"), op, name);
  if (! os->processed)
    einfo (_("%F%P: %s forward reference of section %s\n"), op, name);
}

etree_type *
exp_intop (value)
     bfd_vma value;
{
  etree_type *new = (etree_type *) stat_alloc (sizeof (new->value));
  new->type.node_code = INT;
  new->value.value = value;
  new->value.str = NULL;
  new->type.node_class = etree_value;
  return new;
}

etree_type *
exp_bigintop (value, str)
     bfd_vma value;
     char *str;
{
  etree_type *new = (etree_type *) stat_alloc (sizeof (new->value));
  new->type.node_code = INT;
  new->value.value = value;
  new->value.str = str;
  new->type.node_class = etree_value;
  return new;
}

/* Build an expression representing an unnamed relocateable value.  */

etree_type *
exp_relop (section, value)
     asection *section;
     bfd_vma value;
{
  etree_type *new = (etree_type *) stat_alloc (sizeof (new->rel));
  new->type.node_code = REL;
  new->type.node_class = etree_rel;
  new->rel.section = section;
  new->rel.value = value;
  return new;
}

static etree_value_type
new_rel (value, str, section)
     bfd_vma value;
     char *str;
     lang_output_section_statement_type *section;
{
  etree_value_type new;
  new.valid_p = true;
  new.value = value;
  new.str = str;
  new.section = section;
  return new;
}

static etree_value_type
new_rel_from_section (value, section)
     bfd_vma value;
     lang_output_section_statement_type *section;
{
  etree_value_type new;
  new.valid_p = true;
  new.value = value;
  new.str = NULL;
  new.section = section;

  new.value -= section->bfd_section->vma;

  return new;
}

static etree_value_type
fold_unary (tree, current_section, allocation_done, dot, dotp)
     etree_type *tree;
     lang_output_section_statement_type *current_section;
     lang_phase_type allocation_done;
     bfd_vma dot;
     bfd_vma *dotp;
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
	    result.valid_p = false;
	  break;

	case ABSOLUTE:
	  if (allocation_done != lang_first_phase_enum)
	    {
	      result.value += result.section->bfd_section->vma;
	      result.section = abs_output_section;
	    }
	  else
	    result.valid_p = false;
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
	    result.valid_p = false;
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
	    result.valid_p = false;
	  break;

	default:
	  FAIL ();
	  break;
	}
    }

  return result;
}

static etree_value_type
fold_binary (tree, current_section, allocation_done, dot, dotp)
     etree_type *tree;
     lang_output_section_statement_type *current_section;
     lang_phase_type allocation_done;
     bfd_vma dot;
     bfd_vma *dotp;
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
		result.valid_p = false;
	      break;

	    default:
	      FAIL ();
	    }
	}
      else
	{
	  result.valid_p = false;
	}
    }

  return result;
}

static etree_value_type
fold_trinary (tree, current_section, allocation_done, dot, dotp)
     etree_type *tree;
     lang_output_section_statement_type *current_section;
     lang_phase_type allocation_done;
     bfd_vma dot;
     bfd_vma *dotp;
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

etree_value_type
invalid ()
{
  etree_value_type new;
  new.valid_p = false;
  return new;
}

static etree_value_type
fold_name (tree, current_section, allocation_done, dot)
     etree_type *tree;
     lang_output_section_statement_type *current_section;
     lang_phase_type allocation_done;
     bfd_vma dot;
{
  etree_value_type result;

  switch (tree->type.node_code)
    {
    case SIZEOF_HEADERS:
      if (allocation_done != lang_first_phase_enum)
	{
	  result = new_abs ((bfd_vma)
			    bfd_sizeof_headers (output_bfd,
						link_info.relocateable));
	}
      else
	{
	  result.valid_p = false;
	}
      break;
    case DEFINED:
      if (allocation_done == lang_first_phase_enum)
	result.valid_p = false;
      else
	{
	  struct bfd_link_hash_entry *h;

	  h = bfd_wrapped_link_hash_lookup (output_bfd, &link_info,
					    tree->name.name,
					    false, false, true);
	  result.value = (h != (struct bfd_link_hash_entry *) NULL
			  && (h->type == bfd_link_hash_defined
			      || h->type == bfd_link_hash_defweak
			      || h->type == bfd_link_hash_common));
	  result.section = 0;
	  result.valid_p = true;
	}
      break;
    case NAME:
      result.valid_p = false;
      if (tree->name.name[0] == '.' && tree->name.name[1] == 0)
	{
	  if (allocation_done != lang_first_phase_enum)
	    result = new_rel_from_section (dot, current_section);
	  else
	    result = invalid ();
	}
      else if (allocation_done != lang_first_phase_enum)
	{
	  struct bfd_link_hash_entry *h;

	  h = bfd_wrapped_link_hash_lookup (output_bfd, &link_info,
					    tree->name.name,
					    false, false, true);
	  if (h != NULL
	      && (h->type == bfd_link_hash_defined
		  || h->type == bfd_link_hash_defweak))
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
	}
      break;

    case ADDR:
      if (allocation_done != lang_first_phase_enum)
	{
	  lang_output_section_statement_type *os;

	  os = lang_output_section_find (tree->name.name);
	  check (os, tree->name.name, "ADDR");
	  result = new_rel (0, NULL, os);
	}
      else
	result = invalid ();
      break;

    case LOADADDR:
      if (allocation_done != lang_first_phase_enum)
	{
	  lang_output_section_statement_type *os;

	  os = lang_output_section_find (tree->name.name);
	  check (os, tree->name.name, "LOADADDR");
	  if (os->load_base == NULL)
	    result = new_rel (0, NULL, os);
	  else
	    result = exp_fold_tree_no_dot (os->load_base,
					   abs_output_section,
					   allocation_done);
	}
      else
	result = invalid ();
      break;

    case SIZEOF:
      if (allocation_done != lang_first_phase_enum)
	{
	  int opb = bfd_octets_per_byte (output_bfd);
	  lang_output_section_statement_type *os;

	  os = lang_output_section_find (tree->name.name);
	  check (os, tree->name.name, "SIZEOF");
	  result = new_abs (os->bfd_section->_raw_size / opb);
	}
      else
	result = invalid ();
      break;

    default:
      FAIL ();
      break;
    }

  return result;
}

etree_value_type
exp_fold_tree (tree, current_section, allocation_done, dot, dotp)
     etree_type *tree;
     lang_output_section_statement_type *current_section;
     lang_phase_type allocation_done;
     bfd_vma dot;
     bfd_vma *dotp;
{
  etree_value_type result;

  if (tree == NULL)
    {
      result.valid_p = false;
      return result;
    }

  switch (tree->type.node_class)
    {
    case etree_value:
      result = new_rel (tree->value.value, tree->value.str, current_section);
      break;

    case etree_rel:
      if (allocation_done != lang_final_phase_enum)
	result.valid_p = false;
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
	      boolean create;
	      struct bfd_link_hash_entry *h;

	      if (tree->type.node_class == etree_assign)
		create = true;
	      else
		create = false;
	      h = bfd_link_hash_lookup (link_info.hash, tree->assign.dst,
					create, false, false);
	      if (h == (struct bfd_link_hash_entry *) NULL)
		{
		  if (tree->type.node_class == etree_assign)
		    einfo (_("%P%F:%s: hash creation failed\n"),
			   tree->assign.dst);
		}
	      else if (tree->type.node_class == etree_provide
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
exp_fold_tree_no_dot (tree, current_section, allocation_done)
     etree_type *tree;
     lang_output_section_statement_type *current_section;
     lang_phase_type allocation_done;
{
  return exp_fold_tree (tree, current_section, allocation_done,
			(bfd_vma) 0, (bfd_vma *) NULL);
}

etree_type *
exp_binop (code, lhs, rhs)
     int code;
     etree_type *lhs;
     etree_type *rhs;
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
  new = (etree_type *) stat_alloc (sizeof (new->binary));
  memcpy ((char *) new, (char *) &value, sizeof (new->binary));
  return new;
}

etree_type *
exp_trinop (code, cond, lhs, rhs)
     int code;
     etree_type *cond;
     etree_type *lhs;
     etree_type *rhs;
{
  etree_type value, *new;
  etree_value_type r;
  value.type.node_code = code;
  value.trinary.lhs = lhs;
  value.trinary.cond = cond;
  value.trinary.rhs = rhs;
  value.type.node_class = etree_trinary;
  r = exp_fold_tree_no_dot (&value,
			    (lang_output_section_statement_type *) NULL,
			    lang_first_phase_enum);
  if (r.valid_p)
    return exp_intop (r.value);

  new = (etree_type *) stat_alloc (sizeof (new->trinary));
  memcpy ((char *) new, (char *) &value, sizeof (new->trinary));
  return new;
}

etree_type *
exp_unop (code, child)
     int code;
     etree_type *child;
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

  new = (etree_type *) stat_alloc (sizeof (new->unary));
  memcpy ((char *) new, (char *) &value, sizeof (new->unary));
  return new;
}

etree_type *
exp_nameop (code, name)
     int code;
     const char *name;
{
  etree_type value, *new;
  etree_value_type r;
  value.name.type.node_code = code;
  value.name.name = name;
  value.name.type.node_class = etree_name;

  r = exp_fold_tree_no_dot (&value,
			    (lang_output_section_statement_type *) NULL,
			    lang_first_phase_enum);
  if (r.valid_p)
    return exp_intop (r.value);

  new = (etree_type *) stat_alloc (sizeof (new->name));
  memcpy ((char *) new, (char *) &value, sizeof (new->name));
  return new;

}

etree_type *
exp_assop (code, dst, src)
     int code;
     const char *dst;
     etree_type *src;
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
  new = (etree_type *) stat_alloc (sizeof (new->assign));
  memcpy ((char *) new, (char *) &value, sizeof (new->assign));
  return new;
}

/* Handle PROVIDE.  */

etree_type *
exp_provide (dst, src)
     const char *dst;
     etree_type *src;
{
  etree_type *n;

  n = (etree_type *) stat_alloc (sizeof (n->assign));
  n->assign.type.node_code = '=';
  n->assign.type.node_class = etree_provide;
  n->assign.src = src;
  n->assign.dst = dst;
  return n;
}

/* Handle ASSERT.  */

etree_type *
exp_assert (exp, message)
     etree_type *exp;
     const char *message;
{
  etree_type *n;

  n = (etree_type *) stat_alloc (sizeof (n->assert_s));
  n->assert_s.type.node_code = '!';
  n->assert_s.type.node_class = etree_assert;
  n->assert_s.child = exp;
  n->assert_s.message = message;
  return n;
}

void
exp_print_tree (tree)
     etree_type *tree;
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
      if (tree->assign.dst->sdefs != (asymbol *) NULL)
	fprintf (config.map_file, "%s (%x) ", tree->assign.dst->name,
		 tree->assign.dst->sdefs->value);
      else
	fprintf (config.map_file, "%s (UNDEFINED)", tree->assign.dst->name);
#endif
      fprintf (config.map_file, "%s", tree->assign.dst);
      exp_print_token (tree->type.node_code, true);
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
      exp_print_token (tree->type.node_code, true);
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
      exp_print_token (tree->unary.type.node_code, false);
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
	  exp_print_token (tree->type.node_code, false);
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
exp_get_vma (tree, def, name, allocation_done)
     etree_type *tree;
     bfd_vma def;
     char *name;
     lang_phase_type allocation_done;
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
exp_get_value_int (tree, def, name, allocation_done)
     etree_type *tree;
     int def;
     char *name;
     lang_phase_type allocation_done;
{
  return (int) exp_get_vma (tree, (bfd_vma) def, name, allocation_done);
}

fill_type *
exp_get_fill (tree, def, name, allocation_done)
     etree_type *tree;
     fill_type *def;
     char *name;
     lang_phase_type allocation_done;
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
      fill = (fill_type *) xmalloc ((len + 1) / 2 + sizeof (*fill) - 1);
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
      fill = (fill_type *) xmalloc (4 + sizeof (*fill) - 1);
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
exp_get_abs_int (tree, def, name, allocation_done)
     etree_type *tree;
     int def ATTRIBUTE_UNUSED;
     char *name;
     lang_phase_type allocation_done;
{
  etree_value_type res;
  res = exp_fold_tree_no_dot (tree, abs_output_section, allocation_done);

  if (res.valid_p)
    res.value += res.section->bfd_section->vma;
  else
    einfo (_("%F%S non constant expression for %s\n"), name);

  return res.value;
}

bfd_vma align_n (value, align)
     bfd_vma value;
     bfd_vma align;
{
  if (align <= 1)
    return value;

  value = (value + align - 1) / align;
  return value * align;
}
