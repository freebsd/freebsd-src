/* Handle the hair of processing (but not expanding) inline functions.
   Also manage function and variable name overloading.
   Copyright (C) 1987, 1989, 1992, 1993 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com)

   This file is part of GNU CC.
   
GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


#ifndef PARM_CAN_BE_ARRAY_TYPE
#define PARM_CAN_BE_ARRAY_TYPE 1
#endif

/* Handle method declarations.  */
#include <stdio.h>
#include "config.h"
#include "tree.h"
#include "cp-tree.h"
#include "obstack.h"

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

/* TREE_LIST of the current inline functions that need to be
   processed.  */
struct pending_inline *pending_inlines;

/* Obstack where we build text strings for overloading, etc.  */
static struct obstack scratch_obstack;
static char *scratch_firstobj;

# define OB_INIT() (scratch_firstobj ? (obstack_free (&scratch_obstack, scratch_firstobj), 0) : 0)
# define OB_PUTC(C) (obstack_1grow (&scratch_obstack, (C)))
# define OB_PUTC2(C1,C2)	\
  (obstack_1grow (&scratch_obstack, (C1)), obstack_1grow (&scratch_obstack, (C2)))
# define OB_PUTS(S) (obstack_grow (&scratch_obstack, (S), sizeof (S) - 1))
# define OB_PUTID(ID)  \
  (obstack_grow (&scratch_obstack, IDENTIFIER_POINTER (ID),	\
		 IDENTIFIER_LENGTH (ID)))
# define OB_PUTCP(S) (obstack_grow (&scratch_obstack, (S), strlen (S)))
# define OB_FINISH() (obstack_1grow (&scratch_obstack, '\0'))

/* Counter to help build parameter names in case they were omitted.  */
static int dummy_name;
static int in_parmlist;

/* This points to a safe place to resume processing in case an expression
   generates an error while we're trying to format it.  */
static int scratch_error_offset;

static void dump_type (), dump_decl ();
static void dump_init (), dump_unary_op (), dump_binary_op ();

#ifdef NO_AUTO_OVERLOAD
int is_overloaded ();
#endif

void
init_method ()
{
  gcc_obstack_init (&scratch_obstack);
  scratch_firstobj = (char *)obstack_alloc (&scratch_obstack, 0);
}

tree
make_anon_parm_name ()
{
  char buf[32];

  sprintf (buf, ANON_PARMNAME_FORMAT, dummy_name++);
  return get_identifier (buf);
}

void
clear_anon_parm_name ()
{
  /* recycle these names.  */
  dummy_name = 0;
}

static void
dump_readonly_or_volatile (t)
     tree t;
{
  if (TYPE_READONLY (t))
    OB_PUTS ("const ");
  if (TYPE_VOLATILE (t))
    OB_PUTS ("volatile ");
}

static void
dump_aggr_type (t)
     tree t;
{
  tree name;
  char *aggr_string;
  char *context_string = 0;

  if (TYPE_READONLY (t))
    OB_PUTS ("const ");
  if (TYPE_VOLATILE (t))
    OB_PUTS ("volatile ");
  if (TREE_CODE (t) == ENUMERAL_TYPE)
    aggr_string = "enum";
  else if (TREE_CODE (t) == UNION_TYPE)
    aggr_string = "union";
  else if (TYPE_LANG_SPECIFIC (t) && CLASSTYPE_DECLARED_CLASS (t))
    aggr_string = "class";
  else
    aggr_string = "struct";

  name = TYPE_NAME (t);

  if (TREE_CODE (name) == TYPE_DECL)
    {
#if 0 /* not yet, should get fixed properly later */
      if (DECL_CONTEXT (name))
	context_string = TYPE_NAME_STRING (DECL_CONTEXT (name));
#else
      if (DECL_LANG_SPECIFIC (name) && DECL_CLASS_CONTEXT (name))
	context_string = TYPE_NAME_STRING (DECL_CLASS_CONTEXT (name));
#endif
      name = DECL_NAME (name);
    }

  obstack_grow (&scratch_obstack, aggr_string, strlen (aggr_string));
  OB_PUTC (' ');
  if (context_string)
    {
      obstack_grow (&scratch_obstack, context_string, strlen (context_string));
      OB_PUTC2 (':', ':');
    }
  OB_PUTID (name);
}

/* This must be large enough to hold any anonymous parm name.  */
static char anon_buffer[sizeof (ANON_PARMNAME_FORMAT) + 20];
/* This must be large enough to hold any printed integer or floatingpoint value.  */
static char digit_buffer[128];

static void
dump_type_prefix (t, p)
     tree t;
     int *p;
{
  int old_p = 0;

  if (t == NULL_TREE)
    return;

  switch (TREE_CODE (t))
    {
    case ERROR_MARK:
      sprintf (anon_buffer, ANON_PARMNAME_FORMAT, dummy_name++);
      OB_PUTCP (anon_buffer);
      break;

    case UNKNOWN_TYPE:
      OB_PUTS ("<unknown type>");
      return;

    case TREE_LIST:
      dump_type (TREE_VALUE (t), &old_p);
      if (TREE_CHAIN (t))
	{
	  if (TREE_CHAIN (t) != void_list_node)
	    {
	      OB_PUTC (',');
	      dump_type (TREE_CHAIN (t), &old_p);
	    }
	}
      else OB_PUTS ("...");
      return;

    case POINTER_TYPE:
      *p += 1;
      dump_type_prefix (TREE_TYPE (t), p);
      while (*p)
	{
	  OB_PUTC ('*');
	  *p -= 1;
	}
      if (TYPE_READONLY (t))
	OB_PUTS ("const ");
      if (TYPE_VOLATILE (t))
	OB_PUTS ("volatile ");
      return;

    case OFFSET_TYPE:
      {
	tree type = TREE_TYPE (t);
	if (TREE_CODE (type) == FUNCTION_TYPE)
	  {
	    type = TREE_TYPE (type);
	    if (in_parmlist)
	      OB_PUTS ("auto ");
	  }

	dump_type_prefix (type, &old_p);

	OB_PUTC ('(');
	dump_type (TYPE_OFFSET_BASETYPE (t), &old_p);
	OB_PUTC2 (':', ':');
	while (*p)
	  {
	    OB_PUTC ('*');
	    *p -= 1;
	  }
	if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	  dump_readonly_or_volatile (t);
	return;
      }

    case METHOD_TYPE:
      {
	tree type = TREE_TYPE (t);
	if (in_parmlist)
	  OB_PUTS ("auto ");

	dump_type_prefix (type, &old_p);

	OB_PUTC ('(');
	dump_type (TYPE_METHOD_BASETYPE (t), &old_p);
	OB_PUTC2 (':', ':');
	while (*p)
	  {
	    OB_PUTC ('*');
	    *p -= 1;
	  }
	if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	  dump_readonly_or_volatile (t);
	return;
      }

    case REFERENCE_TYPE:
      dump_type_prefix (TREE_TYPE (t), p);
      OB_PUTC ('&');
      if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	dump_readonly_or_volatile (t);
      return;

    case ARRAY_TYPE:
      if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	dump_readonly_or_volatile (t);
      dump_type_prefix (TREE_TYPE (t), p);
      return;

    case FUNCTION_TYPE:
      if (in_parmlist)
	OB_PUTS ("auto ");
      dump_type_prefix (TREE_TYPE (t), &old_p);
      OB_PUTC ('(');
      while (*p)
	{
	  OB_PUTC ('*');
	  *p -= 1;
	}
      if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	dump_readonly_or_volatile (t);
      return;

    case IDENTIFIER_NODE:
      OB_PUTID (t);
      OB_PUTC (' ');
      break;

    case RECORD_TYPE:
    case UNION_TYPE:
    case ENUMERAL_TYPE:
      dump_aggr_type (t);
      break;

    case TYPE_DECL:
      if (TYPE_READONLY (t))
	OB_PUTS ("const ");
      if (TYPE_VOLATILE (t))
	OB_PUTS ("volatile ");
      OB_PUTID (DECL_NAME (t));
      OB_PUTC (' ');
      break;

    case INTEGER_TYPE:
#if 0
      /* Normally, `unsigned' is part of the deal.  Not so if it comes
	 with `const' or `volatile'.  */
      if (TYPE_MAIN_VARIANT (t) == unsigned_type (TYPE_MAIN_VARIANT (t))
	  && (TYPE_READONLY (t) || TYPE_VOLATILE (t)))
	OB_PUTS ("unsigned ");
#endif
      /* fall through.  */
    case REAL_TYPE:
    case VOID_TYPE:
      if (TYPE_READONLY (t))
	OB_PUTS ("const ");
      if (TYPE_VOLATILE (t))
	OB_PUTS ("volatile ");
      OB_PUTID (TYPE_IDENTIFIER (t));
      OB_PUTC (' ');
      break;

    default:
      my_friendly_abort (65);
    }
}

static void
dump_type_suffix (t, p)
     tree t;
     int *p;
{
  int old_p = 0;

  if (t == NULL_TREE)
    return;

  switch (TREE_CODE (t))
    {
    case ERROR_MARK:
      sprintf (anon_buffer, ANON_PARMNAME_FORMAT, dummy_name++);
      OB_PUTCP (anon_buffer);
      break;

    case UNKNOWN_TYPE:
      return;

    case POINTER_TYPE:
      dump_type_suffix (TREE_TYPE (t), p);
      return;

    case OFFSET_TYPE:
      {
	tree type = TREE_TYPE (t);

	OB_PUTC (')');
	if (TREE_CODE (type) == FUNCTION_TYPE)
	  {
#if 0
	    tree next_arg = TREE_CHAIN (TYPE_ARG_TYPES (type));
	    OB_PUTC ('(');
	    if (next_arg)
	      {
		if (next_arg != void_list_node)
		  {
		    in_parmlist++;
		    dump_type (next_arg, &old_p);
		    in_parmlist--;
		  }
	      }
	    else OB_PUTS ("...");
	    OB_PUTC (')');
	    dump_type_suffix (TREE_TYPE (type), p);
#else
	    my_friendly_abort (66);
#endif
	  }
	return;
      }

    case METHOD_TYPE:
      {
	tree next_arg;
	OB_PUTC (')');
	next_arg = TREE_CHAIN (TYPE_ARG_TYPES (t));
	OB_PUTC ('(');
	if (next_arg)
	  {
	    if (next_arg != void_list_node)
	      {
		in_parmlist++;
		dump_type (next_arg, &old_p);
		in_parmlist--;
	      }
	  }
	else OB_PUTS ("...");
	OB_PUTC (')');
	dump_readonly_or_volatile (TREE_TYPE (TREE_VALUE (TYPE_ARG_TYPES (t))));
	dump_type_suffix (TREE_TYPE (t), p);
	return;
      }

    case REFERENCE_TYPE:
      dump_type_suffix (TREE_TYPE (t), p);
      return;

    case ARRAY_TYPE:
      dump_type_suffix (TREE_TYPE (t), p);
      OB_PUTC2 ('[', ']');
      return;

    case FUNCTION_TYPE:
      OB_PUTC2 (')', '(');
      if (TYPE_ARG_TYPES (t) && TYPE_ARG_TYPES (t) != void_list_node)
	{
	  in_parmlist++;
	  dump_type (TYPE_ARG_TYPES (t), &old_p);
	  in_parmlist--;
	}
      OB_PUTC (')');
      dump_type_suffix (TREE_TYPE (t), p);
      return;

    case IDENTIFIER_NODE:
    case RECORD_TYPE:
    case UNION_TYPE:
    case ENUMERAL_TYPE:
    case TYPE_DECL:
    case INTEGER_TYPE:
    case REAL_TYPE:
    case VOID_TYPE:
      return;

    default:
      my_friendly_abort (67);
    }
}

static void
dump_type (t, p)
     tree t;
     int *p;
{
  int old_p = 0;

  if (t == NULL_TREE)
    return;

  switch (TREE_CODE (t))
    {
    case ERROR_MARK:
      sprintf (anon_buffer, ANON_PARMNAME_FORMAT, dummy_name++);
      OB_PUTCP (anon_buffer);
      break;

    case UNKNOWN_TYPE:
      OB_PUTS ("<unknown type>");
      return;

    case TREE_LIST:
      dump_type (TREE_VALUE (t), &old_p);
      if (TREE_CHAIN (t))
	{
	  if (TREE_CHAIN (t) != void_list_node)
	    {
	      OB_PUTC (',');
	      dump_type (TREE_CHAIN (t), &old_p);
	    }
	}
      else OB_PUTS ("...");
      return;

    case POINTER_TYPE:
      if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	dump_readonly_or_volatile (t);
      *p += 1;
      dump_type (TREE_TYPE (t), p);
      while (*p)
	{
	  OB_PUTC ('*');
	  *p -= 1;
	}
      return;

    case REFERENCE_TYPE:
      if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	dump_readonly_or_volatile (t);
      dump_type (TREE_TYPE (t), p);
      OB_PUTC ('&');
      return;

    case ARRAY_TYPE:
      if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	dump_readonly_or_volatile (t);
      dump_type (TREE_TYPE (t), p);
      OB_PUTC2 ('[', ']');
      return;

    case OFFSET_TYPE:
    case METHOD_TYPE:
    case FUNCTION_TYPE:
      dump_type_prefix (t, p);
      dump_type_suffix (t, p);
      return;

    case IDENTIFIER_NODE:
      OB_PUTID (t);
      OB_PUTC (' ');
      break;

    case RECORD_TYPE:
    case UNION_TYPE:
    case ENUMERAL_TYPE:
      dump_aggr_type (t);
      break;

    case TYPE_DECL:
      if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	dump_readonly_or_volatile (t);
      OB_PUTID (DECL_NAME (t));
      OB_PUTC (' ');
      break;

    case INTEGER_TYPE:
      /* Normally, `unsigned' is part of the deal.  Not so if it comes
	 with `const' or `volatile'.  */
      if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	dump_readonly_or_volatile (t);
#if 0
      if (TYPE_MAIN_VARIANT (t) == unsigned_type (TYPE_MAIN_VARIANT (t))
	  && (TYPE_READONLY (t) | TYPE_VOLATILE (t)))
	OB_PUTS ("unsigned ");
#endif
      OB_PUTID (TYPE_IDENTIFIER (t));
      OB_PUTC (' ');
      break;

    case REAL_TYPE:
    case VOID_TYPE:
      if (TYPE_READONLY (t) | TYPE_VOLATILE (t))
	dump_readonly_or_volatile (t);
      OB_PUTID (TYPE_IDENTIFIER (t));
      OB_PUTC (' ');
      break;

    case TEMPLATE_TYPE_PARM:
      OB_PUTS ("<template type parm ");
      OB_PUTID (TYPE_IDENTIFIER (t));
      OB_PUTC ('>');
      break;

    case UNINSTANTIATED_P_TYPE:
      OB_PUTID (DECL_NAME (UPT_TEMPLATE (t)));
      OB_PUTS ("<...>");
      break;

    default:
      my_friendly_abort (68);
    }
}

static void
dump_decl (t)
     tree t;
{
  int p = 0;

  if (t == NULL_TREE)
    return;

  switch (TREE_CODE (t))
    {
    case ERROR_MARK:
      OB_PUTS (" /* decl error */ ");
      break;

    case PARM_DECL:
      dump_type_prefix (TREE_TYPE (t), &p);
      if (DECL_NAME (t))
	dump_decl (DECL_NAME (t));
      else
	{
	  sprintf (anon_buffer, ANON_PARMNAME_FORMAT, dummy_name++);
	  OB_PUTCP (anon_buffer);
	  break;
	}
      dump_type_suffix (TREE_TYPE (t), &p);
      return;

    case CALL_EXPR:
      dump_decl (TREE_OPERAND (t, 0));
      OB_PUTC ('(');
      in_parmlist++;
      dump_decl (TREE_OPERAND (t, 1));
      in_parmlist--;
      t = tree_last (TYPE_ARG_TYPES (TREE_TYPE (t)));
      if (!t || t != void_list_node)
	OB_PUTS ("...");
      OB_PUTC (')');
      return;

    case ARRAY_REF:
      dump_decl (TREE_OPERAND (t, 0));
      OB_PUTC ('[');
      dump_decl (TREE_OPERAND (t, 1));
      OB_PUTC (']');
      return;

    case TYPE_DECL:
      OB_PUTID (DECL_NAME (t));
      OB_PUTC (' ');
      break;

    case TYPE_EXPR:
      my_friendly_abort (69);
      break;

    case IDENTIFIER_NODE:
      if (t == ansi_opname[(int) TYPE_EXPR])
	{
	  OB_PUTS ("operator ");
	  /* Not exactly IDENTIFIER_TYPE_VALUE.  */
	  dump_type (TREE_TYPE (t), &p);
	  return;
	}
      else if (IDENTIFIER_OPNAME_P (t))
	{
	  char *name_string = operator_name_string (t);
	  OB_PUTS ("operator ");
	  OB_PUTCP (name_string);
	  OB_PUTC (' ');
	}
      else
	{
	  OB_PUTID (t);
	  OB_PUTC (' ');
	}
      break;

    case BIT_NOT_EXPR:
      OB_PUTC2 ('~', ' ');
      dump_decl (TREE_OPERAND (t, 0));
      return;

    case SCOPE_REF:
      OB_PUTID (TREE_OPERAND (t, 0));
      OB_PUTC2 (':', ':');
      dump_decl (TREE_OPERAND (t, 1));
      return;

    case INDIRECT_REF:
      OB_PUTC ('*');
      dump_decl (TREE_OPERAND (t, 0));
      return;

    case ADDR_EXPR:
      OB_PUTC ('&');
      dump_decl (TREE_OPERAND (t, 0));
      return;

    default:
      my_friendly_abort (70);
    }
}

static void
dump_init_list (l)
     tree l;
{
  while (l)
    {
      dump_init (TREE_VALUE (l));
      if (TREE_CHAIN (l))
	OB_PUTC (',');
      l = TREE_CHAIN (l);
    }
}

static void
dump_init (t)
     tree t;
{
  int dummy;

  switch (TREE_CODE (t))
    {
    case VAR_DECL:
    case PARM_DECL:
      OB_PUTC (' ');
      OB_PUTID (DECL_NAME (t));
      OB_PUTC (' ');
      break;

    case FUNCTION_DECL:
      {
	tree name = DECL_ASSEMBLER_NAME (t);

	if (DESTRUCTOR_NAME_P (name))
	  {
	    OB_PUTC2 (' ', '~');
	    OB_PUTID (DECL_NAME (t));
	  }
	else if (IDENTIFIER_TYPENAME_P (name))
	  {
	    dummy = 0;
	    OB_PUTS ("operator ");
	    dump_type (TREE_TYPE (name), &dummy);
	  }
	else if (IDENTIFIER_OPNAME_P (name))
	  {
	    char *name_string = operator_name_string (name);
	      OB_PUTS ("operator ");
	    OB_PUTCP (name_string);
	    OB_PUTC (' ');
	  }
	else
	  {
	    OB_PUTC (' ');
	    OB_PUTID (DECL_NAME (t));
	  }
	OB_PUTC (' ');
      }
      break;

    case CONST_DECL:
      dummy = 0;
      OB_PUTC2 ('(', '(');
      dump_type (TREE_TYPE (t), &dummy);
      OB_PUTC (')');
      dump_init (DECL_INITIAL (t));
      OB_PUTC (')');
      return;

    case INTEGER_CST:
      /* If it's an enum, output its tag, rather than its value.  */
      if (TREE_TYPE (t) && TREE_CODE (TREE_TYPE (t)) == ENUMERAL_TYPE)
	{
	  char *p = enum_name_string (t, TREE_TYPE (t));
	  OB_PUTC (' ');
	  OB_PUTCP (p);
	  OB_PUTC (' ');
	}
      else
	sprintf (digit_buffer, " %d ", TREE_INT_CST_LOW (t));
      OB_PUTCP (digit_buffer);
      break;

    case REAL_CST:
      sprintf (digit_buffer, " %g ", TREE_REAL_CST (t));
      OB_PUTCP (digit_buffer);
      break;

    case STRING_CST:
      {
	char *p = TREE_STRING_POINTER (t);
	int len = TREE_STRING_LENGTH (t) - 1;
	int i;

	OB_PUTC ('\"');
	for (i = 0; i < len; i++)
	  {
	    register char c = p[i];
	    if (c == '\"' || c == '\\')
	      OB_PUTC ('\\');
	    if (c >= ' ' && c < 0177)
	      OB_PUTC (c);
	    else
	      {
		sprintf (digit_buffer, "\\%03o", c);
		OB_PUTCP (digit_buffer);
	      }
	  }
	OB_PUTC ('\"');
      }
      return;

    case COMPOUND_EXPR:
      dump_binary_op (",", t, 1);
      break;

    case COND_EXPR:
      OB_PUTC ('(');
      dump_init (TREE_OPERAND (t, 0));
      OB_PUTS (" ? ");
      dump_init (TREE_OPERAND (t, 1));
      OB_PUTS (" : ");
      dump_init (TREE_OPERAND (t, 2));
      OB_PUTC (')');
      return;

    case SAVE_EXPR:
      if (TREE_HAS_CONSTRUCTOR (t))
	{
	  dummy = 0;
	  OB_PUTS ("new ");
	  dump_type (TREE_TYPE (TREE_TYPE (t)), &dummy);
	  PARM_DECL_EXPR (t) = 1;
	}
      else
	{
	  sorry ("operand of SAVE_EXPR not understood");
	  scratch_obstack.next_free
	    = obstack_base (&scratch_obstack) + scratch_error_offset;
	}
      return;

    case NEW_EXPR:
      OB_PUTID (TYPE_IDENTIFIER (TREE_TYPE (t)));
      OB_PUTC ('(');
      dump_init_list (TREE_CHAIN (TREE_OPERAND (t, 1)));
      OB_PUTC (')');
      return;

    case CALL_EXPR:
      OB_PUTC ('(');
      dump_init (TREE_OPERAND (t, 0));
      dump_init_list (TREE_OPERAND (t, 1));
      OB_PUTC (')');
      return;

    case WITH_CLEANUP_EXPR:
      /* Note that this only works for G++ cleanups.  If somebody
	 builds a general cleanup, there's no way to represent it.  */
      dump_init (TREE_OPERAND (t, 0));
      return;

    case TARGET_EXPR:
      /* Note that this only works for G++ target exprs.  If somebody
	 builds a general TARGET_EXPR, there's no way to represent that
	 it initializes anything other that the parameter slot for the
	 default argument.  Note we may have cleared out the first
	 operand in expand_expr, so don't go killing ourselves.  */
      if (TREE_OPERAND (t, 1))
	dump_init (TREE_OPERAND (t, 1));
      return;

    case MODIFY_EXPR:
    case PLUS_EXPR:
    case MINUS_EXPR:
    case MULT_EXPR:
    case TRUNC_DIV_EXPR:
    case TRUNC_MOD_EXPR:
    case MIN_EXPR:
    case MAX_EXPR:
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
    case BIT_AND_EXPR:
    case BIT_ANDTC_EXPR:
    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
    case EQ_EXPR:
    case NE_EXPR:
      dump_binary_op (opname_tab[(int) TREE_CODE (t)], t,
		      strlen (opname_tab[(int) TREE_CODE (t)]));
      return;

    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
      dump_binary_op ("/", t, 1);
      return;

    case CEIL_MOD_EXPR:
    case FLOOR_MOD_EXPR:
    case ROUND_MOD_EXPR:
      dump_binary_op ("%", t, 1);
      return;

    case COMPONENT_REF:
      dump_binary_op (".", t, 1);
      return;

    case CONVERT_EXPR:
      dump_unary_op ("+", t, 1);
      return;

    case ADDR_EXPR:
      if (TREE_CODE (TREE_OPERAND (t, 0)) == FUNCTION_DECL
	  || TREE_CODE (TREE_OPERAND (t, 0)) == STRING_CST)
	dump_init (TREE_OPERAND (t, 0));
      else
	dump_unary_op ("&", t, 1);
      return;

    case INDIRECT_REF:
      if (TREE_HAS_CONSTRUCTOR (t))
	{
	  t = TREE_OPERAND (t, 0);
	  my_friendly_assert (TREE_CODE (t) == CALL_EXPR, 237);
	  dump_init (TREE_OPERAND (t, 0));
	  OB_PUTC ('(');
	  dump_init_list (TREE_CHAIN (TREE_OPERAND (t, 1)));
	  OB_PUTC (')');
	}
      else
	dump_unary_op ("*", t, 1);
      return;

    case NEGATE_EXPR:
    case BIT_NOT_EXPR:
    case TRUTH_NOT_EXPR:
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
      dump_unary_op (opname_tab [(int)TREE_CODE (t)], t,
		     strlen (opname_tab[(int) TREE_CODE (t)]));
      return;

    case POSTDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
      OB_PUTC ('(');
      dump_init (TREE_OPERAND (t, 0));
      OB_PUTCP (opname_tab[(int)TREE_CODE (t)]);
      OB_PUTC (')');
      return;

    case NOP_EXPR:
      dummy = 0;
      OB_PUTC2 ('(', '(');
      dump_type (TREE_TYPE (t), &dummy);
      OB_PUTC (')');
      dump_init (TREE_OPERAND (t, 0));
      OB_PUTC (')');
      return;

    case CONSTRUCTOR:
      OB_PUTC ('{');
      dump_init_list (CONSTRUCTOR_ELTS (t));
      OB_PUTC ('}');
      return;

      /*  This list is incomplete, but should suffice for now.
	  It is very important that `sorry' does not call
	  `report_error_function'.  That could cause an infinite loop.  */
    default:
      sorry ("`%s' not supported for default parameters",
	     tree_code_name[(int) TREE_CODE (t)]);

      /* fall through to ERROR_MARK...  */
    case ERROR_MARK:
      scratch_obstack.next_free
	= obstack_base (&scratch_obstack) + scratch_error_offset;
      return;
    }
}

static void
dump_binary_op (opstring, t, len)
     char *opstring;
     tree t;
     int len;
{
  OB_PUTC ('(');
  dump_init (TREE_OPERAND (t, 0));
  OB_PUTC (' ');
  OB_PUTCP (opstring);
  OB_PUTC (' ');
  dump_init (TREE_OPERAND (t, 1));
  OB_PUTC (')');
}

static void
dump_unary_op (opstring, t, len)
     char *opstring;
     tree t;
     int len;
{
  OB_PUTC ('(');
  OB_PUTC (' ');
  OB_PUTCP (opstring);
  OB_PUTC (' ');
  dump_init (TREE_OPERAND (t, 0));
  OB_PUTC (')');
}

/* Pretty printing for announce_function.  CNAME is the TYPE_DECL for
   the class that FNDECL belongs to, if we could not figure that out
   from FNDECL itself.  FNDECL is the declaration of the function we
   are interested in seeing.  PRINT_RET_TYPE_P is non-zero if we
   should print the type that this function returns.  */

char *
fndecl_as_string (cname, fndecl, print_ret_type_p)
     tree cname, fndecl;
     int print_ret_type_p;
{
  tree name = DECL_ASSEMBLER_NAME (fndecl);
  tree fntype = TREE_TYPE (fndecl);
  tree parmtypes = TYPE_ARG_TYPES (fntype);
  int p = 0;
  int spaces = 0;

  OB_INIT ();

  if (DECL_CLASS_CONTEXT (fndecl))
    cname = TYPE_NAME (DECL_CLASS_CONTEXT (fndecl));

  if (DECL_STATIC_FUNCTION_P (fndecl))
      OB_PUTS ("static ");
    
  if (print_ret_type_p && ! IDENTIFIER_TYPENAME_P (name))
    {
      dump_type_prefix (TREE_TYPE (fntype), &p);
      OB_PUTC (' ');
    }

  if (cname)
    {
      dump_type (cname, &p);
      *((char *) obstack_next_free (&scratch_obstack) - 1) = ':';
      OB_PUTC (':');
      if (TREE_CODE (fntype) == METHOD_TYPE && parmtypes)
	parmtypes = TREE_CHAIN (parmtypes);
      if (DECL_CONSTRUCTOR_FOR_VBASE_P (fndecl))
	/* Skip past "in_charge" identifier.  */
	parmtypes = TREE_CHAIN (parmtypes);
    }

  if (DESTRUCTOR_NAME_P (name))
    {
      OB_PUTC ('~');
      parmtypes = TREE_CHAIN (parmtypes);
      dump_decl (DECL_NAME (fndecl));
    }
  else if (IDENTIFIER_TYPENAME_P (name))
    {
      /* This cannot use the hack that the operator's return
	 type is stashed off of its name because it may be
	 used for error reporting.  In the case of conflicting
	 declarations, both will have the same name, yet
	 the types will be different, hence the TREE_TYPE field
	 of the first name will be clobbered by the second.  */
      OB_PUTS ("operator ");
      dump_type (TREE_TYPE (TREE_TYPE (fndecl)), &p);
    }
  else if (IDENTIFIER_OPNAME_P (name))
    {
      char *name_string = operator_name_string (name);
      OB_PUTS ("operator ");
      OB_PUTCP (name_string);
      OB_PUTC (' ');
    }
  else
    dump_decl (DECL_NAME (fndecl));

  OB_PUTC ('(');
  if (parmtypes)
    {
      in_parmlist++;
      if (parmtypes != void_list_node)
	spaces = 2;
      while (parmtypes && parmtypes != void_list_node)
	{
	  char *last_space;
	  dump_type (TREE_VALUE (parmtypes), &p);
	  last_space = (char *)obstack_next_free (&scratch_obstack);
	  while (last_space[-1] == ' ')
	    last_space--;
	  scratch_obstack.next_free = last_space;
	  if (TREE_PURPOSE (parmtypes))
	    {
	      scratch_error_offset = obstack_object_size (&scratch_obstack);
	      OB_PUTS (" (= ");
	      dump_init (TREE_PURPOSE (parmtypes));
	      OB_PUTC (')');
	    }
	  OB_PUTC2 (',', ' ');
	  parmtypes = TREE_CHAIN (parmtypes);
	}
      in_parmlist--;
    }
  
  if (parmtypes)
    {
      if (spaces)
	scratch_obstack.next_free = obstack_next_free (&scratch_obstack)-spaces;
    }
  else
    OB_PUTS ("...");

  OB_PUTC (')');

  if (print_ret_type_p && ! IDENTIFIER_TYPENAME_P (name))
    dump_type_suffix (TREE_TYPE (fntype), &p);

  if (TREE_CODE (fntype) == METHOD_TYPE)
    dump_readonly_or_volatile (TREE_TYPE (TREE_VALUE (TYPE_ARG_TYPES (fntype))));

  OB_FINISH ();
  
  return (char *)obstack_base (&scratch_obstack);
}

/* Same, but handtype a _TYPE.  */
char *
type_as_string (typ)
     tree typ;
{
  int p = 0;

  OB_INIT ();

  dump_type(typ,&p);

  OB_FINISH ();

  return (char *)obstack_base (&scratch_obstack);
}

/* A cross between type_as_string and fndecl_as_string.  */
char *
decl_as_string (decl)
     tree decl;
{
  OB_INIT ();

  dump_decl(decl);

  OB_FINISH ();

  return (char *)obstack_base (&scratch_obstack);
}

/* Move inline function definitions out of structure so that they
   can be processed normally.  CNAME is the name of the class
   we are working from, METHOD_LIST is the list of method lists
   of the structure.  We delete friend methods here, after
   saving away their inline function definitions (if any).  */

void
do_inline_function_hair (type, friend_list)
     tree type, friend_list;
{
  tree method = TYPE_METHODS (type);

  if (method && TREE_CODE (method) == TREE_VEC)
    {
      if (TREE_VEC_ELT (method, 0))
	method = TREE_VEC_ELT (method, 0);
      else
	method = TREE_VEC_ELT (method, 1);
    }

  while (method)
    {
      /* Do inline member functions.  */
      struct pending_inline *info = DECL_PENDING_INLINE_INFO (method);
      if (info)
	{
	  tree args;

	  my_friendly_assert (info->fndecl == method, 238);
	  args = DECL_ARGUMENTS (method);
	  while (args)
	    {
	      DECL_CONTEXT (args) = method;
	      args = TREE_CHAIN (args);
	    }

	  /* Allow this decl to be seen in global scope */
	  IDENTIFIER_GLOBAL_VALUE (DECL_ASSEMBLER_NAME (method)) = method;
	}
      method = TREE_CHAIN (method);
    }
  while (friend_list)
    {
      tree fndecl = TREE_VALUE (friend_list);
      struct pending_inline *info = DECL_PENDING_INLINE_INFO (fndecl);
      if (info)
	{
	  tree args;

	  my_friendly_assert (info->fndecl == fndecl, 239);
	  args = DECL_ARGUMENTS (fndecl);
	  while (args)
	    {
	      DECL_CONTEXT (args) = fndecl;
	      args = TREE_CHAIN (args);
	    }

	  /* Allow this decl to be seen in global scope */
	  IDENTIFIER_GLOBAL_VALUE (DECL_ASSEMBLER_NAME (fndecl)) = fndecl;
	}

      friend_list = TREE_CHAIN (friend_list);
    }
}

/* Report an argument type mismatch between the best declared function
   we could find and the current argument list that we have.  */
void
report_type_mismatch (cp, parmtypes, name_kind, err_name)
     struct candidate *cp;
     tree parmtypes;
     char *name_kind, *err_name;
{
  int i = cp->u.bad_arg;
  tree ttf, tta;
  char *tmp_firstobj;

  switch (i)
    {
    case -4:
      my_friendly_assert (TREE_CODE (cp->function) == TEMPLATE_DECL, 240);
      error ("type unification failed for function template `%s'", err_name);
      return;

    case -3:
      if (TYPE_READONLY (TREE_TYPE (TREE_VALUE (parmtypes))))
	error ("call to const %s `%s' with non-const object", name_kind, err_name);
      else
	error ("call to non-const %s `%s' with const object", name_kind, err_name);
      return;
    case -2:
      error ("too few arguments for %s `%s'", name_kind, err_name);
      return;
    case -1:
      error ("too many arguments for %s `%s'", name_kind, err_name);
      return;
    case 0:
      if (TREE_CODE (TREE_TYPE (cp->function)) == METHOD_TYPE)
	{
	  /* Happens when we have an ambiguous base class.  */
	  my_friendly_assert (get_binfo (DECL_CLASS_CONTEXT (cp->function),
			     TREE_TYPE (TREE_TYPE (TREE_VALUE (parmtypes))), 1) == error_mark_node,
			      241);
	  return;
	}
    }

  ttf = TYPE_ARG_TYPES (TREE_TYPE (cp->function));
  tta = parmtypes;

  while (i-- > 0)
    {
      ttf = TREE_CHAIN (ttf);
      tta = TREE_CHAIN (tta);
    }

  OB_INIT ();
  OB_PUTS ("bad argument ");
  sprintf (digit_buffer, "%d",
	   cp->u.bad_arg - (TREE_CODE (TREE_TYPE (cp->function)) == METHOD_TYPE) + 1);
  OB_PUTCP (digit_buffer);
  OB_PUTS (" for function `");

  tmp_firstobj = scratch_firstobj;
  scratch_firstobj = 0;
  fndecl_as_string (0, cp->function, 0);
  scratch_firstobj = tmp_firstobj;

  /* We know that the last char written is next_free-1.  */
  ((char *) obstack_next_free (&scratch_obstack))[-1] = '\'';
  OB_PUTS (" (type was ");

  /* Reset `i' so that type printing routines do the right thing.  */
  if (tta)
    {
      enum tree_code code = TREE_CODE (TREE_TYPE (TREE_VALUE (tta)));
      if (code == ERROR_MARK)
	OB_PUTS ("(failed type instantiation)");
      else
	{
	  i = (code == FUNCTION_TYPE || code == METHOD_TYPE);
	  dump_type (TREE_TYPE (TREE_VALUE (tta)), &i);
	}
    }
  else OB_PUTS ("void");
  OB_PUTC (')');
  OB_FINISH ();

  tmp_firstobj = (char *)alloca (obstack_object_size (&scratch_obstack));
  bcopy (obstack_base (&scratch_obstack), tmp_firstobj,
	 obstack_object_size (&scratch_obstack));
  error (tmp_firstobj);
}

/* Here is where overload code starts.  */

/* Array of types seen so far in top-level call to `build_overload_name'.
   Allocated and deallocated by caller.  */
static tree *typevec;

/* Number of types interned by `build_overload_name' so far.  */
static int maxtype;

/* Number of occurrences of last type seen.  */
static int nrepeats;

/* Nonzero if we should not try folding parameter types.  */
static int nofold;

#define ALLOCATE_TYPEVEC(PARMTYPES) \
  do { maxtype = 0, nrepeats = 0; \
       typevec = (tree *)alloca (list_length (PARMTYPES) * sizeof (tree)); } while (0)

#define DEALLOCATE_TYPEVEC(PARMTYPES) \
  do { tree t = (PARMTYPES); \
       while (t) { TREE_USED (TREE_VALUE (t)) = 0; t = TREE_CHAIN (t); } \
  } while (0)

/* Code to concatenate an asciified integer to a string.  */
static
#ifdef __GNUC__
__inline
#endif
void
icat (i)
     int i;
{
  /* Handle this case first, to go really quickly.  For many common values,
     the result of i/10 below is 1.  */
  if (i == 1)
    {
      OB_PUTC ('1');
      return;
    }

  if (i < 0)
    {
      OB_PUTC ('m');
      i = -i;
    }
  if (i < 10)
    OB_PUTC ('0' + i);
  else
    {
      icat (i / 10);
      OB_PUTC ('0' + (i % 10));
    }
}

static
#ifdef __GNUC__
__inline
#endif
void
flush_repeats (type)
     tree type;
{
  int tindex = 0;

  while (typevec[tindex] != type)
    tindex++;

  if (nrepeats > 1)
    {
      OB_PUTC ('N');
      icat (nrepeats);
      if (nrepeats > 9)
	OB_PUTC ('_');
    }
  else
    OB_PUTC ('T');
  nrepeats = 0;
  icat (tindex);
  if (tindex > 9)
    OB_PUTC ('_');
}

static void build_overload_identifier ();

static void
build_overload_nested_name (context)
     tree context;
{
  /* We use DECL_NAME here, because pushtag now sets the DECL_ASSEMBLER_NAME.  */
  tree name = DECL_NAME (context);
  if (DECL_CONTEXT (context))
    {
      context = DECL_CONTEXT (context);
      if (TREE_CODE_CLASS (TREE_CODE (context)) == 't')
	context = TYPE_NAME (context);
      build_overload_nested_name (context);
    }
  build_overload_identifier (name);
}

static void
build_overload_value (type, value)
     tree type, value;
{
  while (TREE_CODE (value) == NON_LVALUE_EXPR)
    value = TREE_OPERAND (value, 0);
  my_friendly_assert (TREE_CODE (type) == PARM_DECL, 242);
  type = TREE_TYPE (type);
  switch (TREE_CODE (type))
    {
    case INTEGER_TYPE:
    case ENUMERAL_TYPE:
      {
	my_friendly_assert (TREE_CODE (value) == INTEGER_CST, 243);
	if (TYPE_PRECISION (value) == 2 * HOST_BITS_PER_WIDE_INT)
	  {
	    if (tree_int_cst_lt (value, integer_zero_node))
	      {
		OB_PUTC ('m');
		value = build_int_2 (~ TREE_INT_CST_LOW (value),
				     - TREE_INT_CST_HIGH (value));
	      }
	    if (TREE_INT_CST_HIGH (value)
		!= (TREE_INT_CST_LOW (value) >> (HOST_BITS_PER_WIDE_INT - 1)))
	      {
		/* need to print a DImode value in decimal */
		sorry ("conversion of long long as PT parameter");
	      }
	    /* else fall through to print in smaller mode */
	  }
	/* Wordsize or smaller */
	icat (TREE_INT_CST_LOW (value));
	return;
      }
#ifndef REAL_IS_NOT_DOUBLE
    case REAL_TYPE:
      {
	REAL_VALUE_TYPE val;
	char *bufp = digit_buffer;
	extern char *index ();

	my_friendly_assert (TREE_CODE (value) == REAL_CST, 244);
	val = TREE_REAL_CST (value);
	if (val < 0)
	  {
	    val = -val;
	    *bufp++ = 'm';
	  }
	sprintf (bufp, "%e", val);
	bufp = (char *) index (bufp, 'e');
	if (!bufp)
	  strcat (digit_buffer, "e0");
	else
	  {
	    char *p;
	    bufp++;
	    if (*bufp == '-')
	      {
		*bufp++ = 'm';
	      }
	    p = bufp;
	    if (*p == '+')
	      p++;
	    while (*p == '0')
	      p++;
	    if (*p == 0)
	      {
		*bufp++ = '0';
		*bufp = 0;
	      }
	    else if (p != bufp)
	      {
		while (*p)
		  *bufp++ = *p++;
		*bufp = 0;
	      }
	  }
	OB_PUTCP (digit_buffer);
	return;
      }
#endif
    case POINTER_TYPE:
      value = TREE_OPERAND (value, 0);
      if (TREE_CODE (value) == VAR_DECL)
	{
	  my_friendly_assert (DECL_NAME (value) != 0, 245);
	  build_overload_identifier (DECL_NAME (value));
	  return;
	}
      else if (TREE_CODE (value) == FUNCTION_DECL)
	{
	  my_friendly_assert (DECL_NAME (value) != 0, 246);
	  build_overload_identifier (DECL_NAME (value));
	  return;
	}
      else
	my_friendly_abort (71);
      break; /* not really needed */

    default:
      sorry ("conversion of %s as PT parameter",
	     tree_code_name [(int) TREE_CODE (type)]);
      my_friendly_abort (72);
    }
}

static void
build_overload_identifier (name)
     tree name;
{
  if (IDENTIFIER_TEMPLATE (name))
    {
      tree template, parmlist, arglist, tname;
      int i, nparms;
      template = IDENTIFIER_TEMPLATE (name);
      arglist = TREE_VALUE (template);
      template = TREE_PURPOSE (template);
      tname = DECL_NAME (template);
      parmlist = DECL_ARGUMENTS (template);
      nparms = TREE_VEC_LENGTH (parmlist);
      OB_PUTC ('t');
      icat (IDENTIFIER_LENGTH (tname));
      OB_PUTID (tname);
      icat (nparms);
      for (i = 0; i < nparms; i++)
	{
	  tree parm = TREE_VEC_ELT (parmlist, i);
	  tree arg = TREE_VEC_ELT (arglist, i);
	  if (TREE_CODE (parm) == IDENTIFIER_NODE)
	    {
	      /* This parameter is a type.  */
	      OB_PUTC ('Z');
	      build_overload_name (arg, 0, 0);
	    }
	  else
	    {
	      /* It's a PARM_DECL.  */
	      build_overload_name (TREE_TYPE (parm), 0, 0);
	      build_overload_value (parm, arg);
	    }
	}
    }
  else
    {
      icat (IDENTIFIER_LENGTH (name));
      OB_PUTID (name);
    }
}

/* Given a list of parameters in PARMTYPES, create an unambiguous
   overload string. Should distinguish any type that C (or C++) can
   distinguish. I.e., pointers to functions are treated correctly.

   Caller must deal with whether a final `e' goes on the end or not.

   Any default conversions must take place before this function
   is called.

   BEGIN and END control initialization and finalization of the
   obstack where we build the string.  */

char *
build_overload_name (parmtypes, begin, end)
     tree parmtypes;
     int begin, end;
{
  int just_one;
  tree parmtype;

  if (begin) OB_INIT ();

  if (just_one = (TREE_CODE (parmtypes) != TREE_LIST))
    {
      parmtype = parmtypes;
      goto only_one;
    }

  while (parmtypes)
    {
      parmtype = TREE_VALUE (parmtypes);

    only_one:

      if (! nofold)
	{
	  if (! just_one)
	    /* Every argument gets counted.  */
	    typevec[maxtype++] = parmtype;

	  if (TREE_USED (parmtype))
	    {
	      if (! just_one && parmtype == typevec[maxtype-2])
		nrepeats++;
	      else
		{
		  if (nrepeats)
		    flush_repeats (parmtype);
		  if (! just_one && TREE_CHAIN (parmtypes)
		      && parmtype == TREE_VALUE (TREE_CHAIN (parmtypes)))
		    nrepeats++;
		  else
		    {
		      int tindex = 0;

		      while (typevec[tindex] != parmtype)
			tindex++;
		      OB_PUTC ('T');
		      icat (tindex);
		      if (tindex > 9)
			OB_PUTC ('_');
		    }
		}
	      goto next;
	    }
	  if (nrepeats)
	    flush_repeats (typevec[maxtype-2]);
	  if (! just_one
	      /* Only cache types which take more than one character.  */
	      && (parmtype != TYPE_MAIN_VARIANT (parmtype)
		  || (TREE_CODE (parmtype) != INTEGER_TYPE
		      && TREE_CODE (parmtype) != REAL_TYPE)))
	    TREE_USED (parmtype) = 1;
	}

      if (TREE_READONLY (parmtype))
	OB_PUTC ('C');
      if (TREE_CODE (parmtype) == INTEGER_TYPE
	  && TYPE_MAIN_VARIANT (parmtype) == unsigned_type (TYPE_MAIN_VARIANT (parmtype)))
	OB_PUTC ('U');
      if (TYPE_VOLATILE (parmtype))
	OB_PUTC ('V');

      switch (TREE_CODE (parmtype))
	{
	case OFFSET_TYPE:
	  OB_PUTC ('O');
	  build_overload_name (TYPE_OFFSET_BASETYPE (parmtype), 0, 0);
	  OB_PUTC ('_');
	  build_overload_name (TREE_TYPE (parmtype), 0, 0);
	  break;

	case REFERENCE_TYPE:
	  OB_PUTC ('R');
	  goto more;

	case ARRAY_TYPE:
#if PARM_CAN_BE_ARRAY_TYPE
	  {
	    tree length;

	    OB_PUTC ('A');
	    if (TYPE_DOMAIN (parmtype) == NULL_TREE)
	      {
		error ("parameter type with unspecified array bounds invalid");
		icat (1);
	      }
	    else
	      {
		length = array_type_nelts (parmtype);
		if (TREE_CODE (length) == INTEGER_CST)
		  icat (TREE_INT_CST_LOW (length) + 1);
	      }
	    OB_PUTC ('_');
	    goto more;
	  }
#else
	  OB_PUTC ('P');
	  goto more;
#endif

	case POINTER_TYPE:
	  OB_PUTC ('P');
	more:
	  build_overload_name (TREE_TYPE (parmtype), 0, 0);
	  break;

	case FUNCTION_TYPE:
	case METHOD_TYPE:
	  {
	    tree firstarg = TYPE_ARG_TYPES (parmtype);
	    /* Otherwise have to implement reentrant typevecs,
	       unmark and remark types, etc.  */
	    int old_nofold = nofold;
	    nofold = 1;

	    if (nrepeats)
	      flush_repeats (typevec[maxtype-1]);

	    /* @@ It may be possible to pass a function type in
	       which is not preceded by a 'P'.  */
	    if (TREE_CODE (parmtype) == FUNCTION_TYPE)
	      {
		OB_PUTC ('F');
		if (firstarg == NULL_TREE)
		  OB_PUTC ('e');
		else if (firstarg == void_list_node)
		  OB_PUTC ('v');
		else
		  build_overload_name (firstarg, 0, 0);
	      }
	    else
	      {
		int constp = TYPE_READONLY (TREE_TYPE (TREE_VALUE (firstarg)));
		int volatilep = TYPE_VOLATILE (TREE_TYPE (TREE_VALUE (firstarg)));
		OB_PUTC ('M');
		firstarg = TREE_CHAIN (firstarg);

		build_overload_name (TYPE_METHOD_BASETYPE (parmtype), 0, 0);
		if (constp)
		  OB_PUTC ('C');
		if (volatilep)
		  OB_PUTC ('V');

		/* For cfront 2.0 compatibility.  */
		OB_PUTC ('F');

		if (firstarg == NULL_TREE)
		  OB_PUTC ('e');
		else if (firstarg == void_list_node)
		  OB_PUTC ('v');
		else
		  build_overload_name (firstarg, 0, 0);
	      }

	    /* Separate args from return type.  */
	    OB_PUTC ('_');
	    build_overload_name (TREE_TYPE (parmtype), 0, 0);
	    nofold = old_nofold;
	    break;
	  }

	case INTEGER_TYPE:
	  parmtype = TYPE_MAIN_VARIANT (parmtype);
	  if (parmtype == integer_type_node
	      || parmtype == unsigned_type_node)
	    OB_PUTC ('i');
	  else if (parmtype == long_integer_type_node
		   || parmtype == long_unsigned_type_node)
	    OB_PUTC ('l');
	  else if (parmtype == short_integer_type_node
		   || parmtype == short_unsigned_type_node)
	    OB_PUTC ('s');
	  else if (parmtype == signed_char_type_node)
	    {
	      OB_PUTC ('S');
	      OB_PUTC ('c');
	    }
	  else if (parmtype == char_type_node
		   || parmtype == unsigned_char_type_node)
	    OB_PUTC ('c');
	  else if (parmtype == wchar_type_node)
	    OB_PUTC ('w');
	  else if (parmtype == long_long_integer_type_node
	      || parmtype == long_long_unsigned_type_node)
	    OB_PUTC ('x');
#if 0
	  /* it would seem there is no way to enter these in source code,
	     yet.  (mrs) */
	  else if (parmtype == long_long_long_integer_type_node
	      || parmtype == long_long_long_unsigned_type_node)
	    OB_PUTC ('q');
#endif
	  else
	    my_friendly_abort (73);
	  break;

	case REAL_TYPE:
	  parmtype = TYPE_MAIN_VARIANT (parmtype);
	  if (parmtype == long_double_type_node)
	    OB_PUTC ('r');
	  else if (parmtype == double_type_node)
	    OB_PUTC ('d');
	  else if (parmtype == float_type_node)
	    OB_PUTC ('f');
	  else my_friendly_abort (74);
	  break;

	case VOID_TYPE:
	  if (! just_one)
	    {
#if 0
	      extern tree void_list_node;

	      /* See if anybody is wasting memory.  */
	      my_friendly_assert (parmtypes == void_list_node, 247);
#endif
	      /* This is the end of a parameter list.  */
	      if (end) OB_FINISH ();
	      return (char *)obstack_base (&scratch_obstack);
	    }
	  OB_PUTC ('v');
	  break;

	case ERROR_MARK:	/* not right, but nothing is anyway */
	  break;

	  /* have to do these */
	case UNION_TYPE:
	case RECORD_TYPE:
	  if (! just_one)
	    /* Make this type signature look incompatible
	       with AT&T.  */
	    OB_PUTC ('G');
	  goto common;
	case ENUMERAL_TYPE:
	common:
	  {
	    tree name = TYPE_NAME (parmtype);
	    int i = 1;

	    if (TREE_CODE (name) == TYPE_DECL)
	      {
		tree context = name;
		while (DECL_CONTEXT (context))
		  {
		    i += 1;
		    context = DECL_CONTEXT (context);
		    if (TREE_CODE_CLASS (TREE_CODE (context)) == 't')
		      context = TYPE_NAME (context);
		  }
		name = DECL_NAME (name);
	      }
	    my_friendly_assert (TREE_CODE (name) == IDENTIFIER_NODE, 248);
	    if (i > 1)
	      {
		OB_PUTC ('Q');
		icat (i);
		build_overload_nested_name (TYPE_NAME (parmtype));
	      }
	    else
	      build_overload_identifier (name);
	    break;
	  }

	case UNKNOWN_TYPE:
	  /* This will take some work.  */
	  OB_PUTC ('?');
	  break;

	case TEMPLATE_TYPE_PARM:
	case TEMPLATE_CONST_PARM:
        case UNINSTANTIATED_P_TYPE:
	  /* We don't ever want this output, but it's inconvenient not to
	     be able to build the string.  This should cause assembler
	     errors we'll notice.  */
	  {
	    static int n;
	    sprintf (digit_buffer, " *%d", n++);
	    OB_PUTCP (digit_buffer);
	  }
	  break;

	default:
	  my_friendly_abort (75);
	}

    next:
      if (just_one) break;
      parmtypes = TREE_CHAIN (parmtypes);
    }
  if (! just_one)
    {
      if (nrepeats)
	flush_repeats (typevec[maxtype-1]);

      /* To get here, parms must end with `...'. */
      OB_PUTC ('e');
    }

  if (end) OB_FINISH ();
  return (char *)obstack_base (&scratch_obstack);
}

/* Generate an identifier that encodes the (ANSI) exception TYPE. */

/* This should be part of `ansi_opname', or at least be defined by the std.  */
#define EXCEPTION_NAME_PREFIX "__ex"
#define EXCEPTION_NAME_LENGTH 4

tree
cplus_exception_name (type)
     tree type;
{
  OB_INIT ();
  OB_PUTS (EXCEPTION_NAME_PREFIX);
  return get_identifier (build_overload_name (type, 0, 1));
}

/* Change the name of a function definition so that it may be
   overloaded. NAME is the name of the function to overload,
   PARMS is the parameter list (which determines what name the
   final function obtains).

   FOR_METHOD is 1 if this overload is being performed
   for a method, rather than a function type.  It is 2 if
   this overload is being performed for a constructor.  */
tree
build_decl_overload (dname, parms, for_method)
     tree dname;
     tree parms;
     int for_method;
{
  char *name = IDENTIFIER_POINTER (dname);

  if (dname == ansi_opname[(int) NEW_EXPR]
      && parms != NULL_TREE
      && TREE_CODE (parms) == TREE_LIST
      && TREE_VALUE (parms) == sizetype
      && TREE_CHAIN (parms) == void_list_node)
    return get_identifier ("__builtin_new");
  else if (dname == ansi_opname[(int) DELETE_EXPR]
	   && parms != NULL_TREE
	   && TREE_CODE (parms) == TREE_LIST
	   && TREE_VALUE (parms) == ptr_type_node
	   && TREE_CHAIN (parms) == void_list_node)
    return get_identifier ("__builtin_delete");
  else if (dname == ansi_opname[(int) DELETE_EXPR]
	   && parms != NULL_TREE
	   && TREE_CODE (parms) == TREE_LIST
	   && TREE_VALUE (parms) == ptr_type_node
	   && TREE_CHAIN (parms) != NULL_TREE
	   && TREE_CODE (TREE_CHAIN (parms)) == TREE_LIST
	   && TREE_VALUE (TREE_CHAIN (parms)) == sizetype
	   && TREE_CHAIN (TREE_CHAIN (parms)) == void_list_node)
    return get_identifier ("__builtin_delete");

  OB_INIT ();
  if (for_method != 2)
    OB_PUTCP (name);
  /* Otherwise, we can divine that this is a constructor,
     and figure out its name without any extra encoding.  */

  OB_PUTC2 ('_', '_');
  if (for_method)
    {
#if 0
      /* We can get away without doing this.  */
      OB_PUTC ('M');
#endif
      parms = temp_tree_cons (NULL_TREE, TREE_TYPE (TREE_VALUE (parms)), TREE_CHAIN (parms));
    }
  else
    OB_PUTC ('F');

  if (parms == NULL_TREE)
    OB_PUTC2 ('e', '\0');
  else if (parms == void_list_node)
    OB_PUTC2 ('v', '\0');
  else
    {
      ALLOCATE_TYPEVEC (parms);
      nofold = 0;
      if (for_method)
	{
	  build_overload_name (TREE_VALUE (parms), 0, 0);

	  typevec[maxtype++] = TREE_VALUE (parms);
	  TREE_USED (TREE_VALUE (parms)) = 1;

	  if (TREE_CHAIN (parms))
	    build_overload_name (TREE_CHAIN (parms), 0, 1);
	  else
	    OB_PUTC2 ('e', '\0');
	}
      else
	build_overload_name (parms, 0, 1);
      DEALLOCATE_TYPEVEC (parms);
    }
  return get_identifier (obstack_base (&scratch_obstack));
}

/* Build an overload name for the type expression TYPE.  */
tree
build_typename_overload (type)
     tree type;
{
  tree id;

  OB_INIT ();
  OB_PUTID (ansi_opname[(int) TYPE_EXPR]);
  nofold = 1;
  build_overload_name (type, 0, 1);
  id = get_identifier (obstack_base (&scratch_obstack));
  IDENTIFIER_OPNAME_P (id) = 1;
  return id;
}

#ifndef NO_DOLLAR_IN_LABEL
#define T_DESC_FORMAT "TD$"
#define I_DESC_FORMAT "ID$"
#define M_DESC_FORMAT "MD$"
#else
#if !defined(NO_DOT_IN_LABEL)
#define T_DESC_FORMAT "TD."
#define I_DESC_FORMAT "ID."
#define M_DESC_FORMAT "MD."
#else
#define T_DESC_FORMAT "__t_desc_"
#define I_DESC_FORMAT "__i_desc_"
#define M_DESC_FORMAT "__m_desc_"
#endif
#endif

/* Build an overload name for the type expression TYPE.  */
tree
build_t_desc_overload (type)
     tree type;
{
  OB_INIT ();
  OB_PUTS (T_DESC_FORMAT);
  nofold = 1;

#if 0
  /* Use a different format if the type isn't defined yet.  */
  if (TYPE_SIZE (type) == NULL_TREE)
    {
      char *p;
      int changed;

      for (p = tname; *p; p++)
	if (isupper (*p))
	  {
	    changed = 1;
	    *p = tolower (*p);
	  }
      /* If there's no change, we have an inappropriate T_DESC_FORMAT.  */
      my_friendly_assert (changed != 0, 249);
    }
#endif

  build_overload_name (type, 0, 1);
  return get_identifier (obstack_base (&scratch_obstack));
}

/* Top-level interface to explicit overload requests. Allow NAME
   to be overloaded. Error if NAME is already declared for the current
   scope. Warning if function is redundantly overloaded. */

void
declare_overloaded (name)
     tree name;
{
#ifdef NO_AUTO_OVERLOAD
  if (is_overloaded (name))
    warning ("function `%s' already declared overloaded",
	     IDENTIFIER_POINTER (name));
  else if (IDENTIFIER_GLOBAL_VALUE (name))
    error ("overloading function `%s' that is already defined",
	   IDENTIFIER_POINTER (name));
  else
    {
      TREE_OVERLOADED (name) = 1;
      IDENTIFIER_GLOBAL_VALUE (name) = build_tree_list (name, NULL_TREE);
      TREE_TYPE (IDENTIFIER_GLOBAL_VALUE (name)) = unknown_type_node;
    }
#else
  if (current_lang_name == lang_name_cplusplus)
    {
      if (0)
	warning ("functions are implicitly overloaded in C++");
    }
  else if (current_lang_name == lang_name_c)
    error ("overloading function `%s' cannot be done in C language context");
  else
    my_friendly_abort (76);
#endif
}

#ifdef NO_AUTO_OVERLOAD
/* Check to see if NAME is overloaded. For first approximation,
   check to see if its TREE_OVERLOADED is set.  This is used on
   IDENTIFIER nodes.  */
int
is_overloaded (name)
     tree name;
{
  /* @@ */
  return (TREE_OVERLOADED (name)
	  && (! IDENTIFIER_CLASS_VALUE (name) || current_class_type == 0)
	  && ! IDENTIFIER_LOCAL_VALUE (name));
}
#endif

/* Given a tree_code CODE, and some arguments (at least one),
   attempt to use an overloaded operator on the arguments.

   For unary operators, only the first argument need be checked.
   For binary operators, both arguments may need to be checked.

   Member functions can convert class references to class pointers,
   for one-level deep indirection.  More than that is not supported.
   Operators [](), ()(), and ->() must be member functions.

   We call function call building calls with nonzero complain if
   they are our only hope.  This is true when we see a vanilla operator
   applied to something of aggregate type.  If this fails, we are free to
   return `error_mark_node', because we will have reported the error.

   Operators NEW and DELETE overload in funny ways: operator new takes
   a single `size' parameter, and operator delete takes a pointer to the
   storage being deleted.  When overloading these operators, success is
   assumed.  If there is a failure, report an error message and return
   `error_mark_node'.  */

/* NOSTRICT */
tree
build_opfncall (code, flags, xarg1, xarg2, arg3)
     enum tree_code code;
     int flags;
     tree xarg1, xarg2, arg3;
{
  tree rval = 0;
  tree arg1, arg2;
  tree type1, type2, fnname;
  tree fields1 = 0, parms = 0;
  tree global_fn;
  int try_second;
  int binary_is_unary;

  if (xarg1 == error_mark_node)
    return error_mark_node;

  if (code == COND_EXPR)
    {
      if (TREE_CODE (xarg2) == ERROR_MARK
	  || TREE_CODE (arg3) == ERROR_MARK)
	return error_mark_node;
    }
  if (code == COMPONENT_REF)
    if (TREE_CODE (TREE_TYPE (xarg1)) == POINTER_TYPE)
      return rval;

  /* First, see if we can work with the first argument */
  type1 = TREE_TYPE (xarg1);

  /* Some tree codes have length > 1, but we really only want to
     overload them if their first argument has a user defined type.  */
  switch (code)
    {
    case PREINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case COMPONENT_REF:
      binary_is_unary = 1;
      try_second = 0;
      break;

      /* ARRAY_REFs and CALL_EXPRs must overload successfully.
	 If they do not, return error_mark_node instead of NULL_TREE.  */
    case ARRAY_REF:
      if (xarg2 == error_mark_node)
	return error_mark_node;
    case CALL_EXPR:
      rval = error_mark_node;
      binary_is_unary = 0;
      try_second = 0;
      break;

    case NEW_EXPR:
      {
	/* For operators `new' (`delete'), only check visibility
	   if we are in a constructor (destructor), and we are
	   allocating for that constructor's (destructor's) type.  */

	fnname = ansi_opname[(int) NEW_EXPR];
	if (flags & LOOKUP_GLOBAL)
	  return build_overload_call (fnname, tree_cons (NULL_TREE, xarg2, arg3),
				      flags & LOOKUP_COMPLAIN,
				      (struct candidate *)0);

	if (current_function_decl == NULL_TREE
	    || !DECL_CONSTRUCTOR_P (current_function_decl)
	    || current_class_type != TYPE_MAIN_VARIANT (type1))
	  flags = LOOKUP_COMPLAIN;
	rval = build_method_call (build1 (NOP_EXPR, xarg1, error_mark_node),
				  fnname, tree_cons (NULL_TREE, xarg2, arg3),
				  NULL_TREE, flags);
	if (rval == error_mark_node)
	  /* User might declare fancy operator new, but invoke it
	     like standard one.  */
	  return rval;

	TREE_TYPE (rval) = xarg1;
	TREE_CALLS_NEW (rval) = 1;
	return rval;
      }
      break;

    case DELETE_EXPR:
      {
	/* See comment above.  */

	fnname = ansi_opname[(int) DELETE_EXPR];
	if (flags & LOOKUP_GLOBAL)
	  return build_overload_call (fnname,
				      tree_cons (NULL_TREE, xarg1,
						 build_tree_list (NULL_TREE, xarg2)),
				      flags & LOOKUP_COMPLAIN,
				      (struct candidate *)0);

	if (current_function_decl == NULL_TREE
	    || !DESTRUCTOR_NAME_P (DECL_ASSEMBLER_NAME (current_function_decl))
	    || current_class_type != TYPE_MAIN_VARIANT (type1))
	  flags = LOOKUP_COMPLAIN;
	rval = build_method_call (build1 (NOP_EXPR, TREE_TYPE (xarg1),
					  error_mark_node),
				  fnname, tree_cons (NULL_TREE, xarg1,
						     build_tree_list (NULL_TREE, xarg2)),
				  NULL_TREE, flags);
	/* This happens when the user mis-declares `operator delete'.
	   Should now be impossible.  */
	my_friendly_assert (rval != error_mark_node, 250);
	TREE_TYPE (rval) = void_type_node;
	return rval;
      }
      break;

    default:
      binary_is_unary = 0;
      try_second = tree_code_length [(int) code] == 2;
      if (try_second && xarg2 == error_mark_node)
	return error_mark_node;
      break;
    }

  if (try_second && xarg2 == error_mark_node)
    return error_mark_node;

  /* What ever it was, we do not know how to deal with it.  */
  if (type1 == NULL_TREE)
    return rval;

  if (TREE_CODE (type1) == OFFSET_TYPE)
    type1 = TREE_TYPE (type1);

  if (TREE_CODE (type1) == REFERENCE_TYPE)
    {
      arg1 = convert_from_reference (xarg1);
      type1 = TREE_TYPE (arg1);
    }
  else
    {
      arg1 = xarg1;
    }

  if (!IS_AGGR_TYPE (type1))
    {
      /* Try to fail. First, fail if unary */
      if (! try_second)
	return rval;
      /* Second, see if second argument is non-aggregate. */
      type2 = TREE_TYPE (xarg2);
      if (TREE_CODE (type2) == OFFSET_TYPE)
	type2 = TREE_TYPE (type2);
      if (TREE_CODE (type2) == REFERENCE_TYPE)
	{
	  arg2 = convert_from_reference (xarg2);
	  type2 = TREE_TYPE (arg2);
	}
      else
	{
	  arg2 = xarg2;
	}

      if (!IS_AGGR_TYPE (type2))
	return rval;
      try_second = 0;
    }

  if (try_second)
    {
      /* First arg may succeed; see whether second should.  */
      type2 = TREE_TYPE (xarg2);
      if (TREE_CODE (type2) == OFFSET_TYPE)
	type2 = TREE_TYPE (type2);
      if (TREE_CODE (type2) == REFERENCE_TYPE)
	{
	  arg2 = convert_from_reference (xarg2);
	  type2 = TREE_TYPE (arg2);
	}
      else
	{
	  arg2 = xarg2;
	}

      if (! IS_AGGR_TYPE (type2))
	try_second = 0;
    }

  if (type1 == unknown_type_node
      || (try_second && TREE_TYPE (xarg2) == unknown_type_node))
    {
      /* This will not be implemented in the foreseeable future.  */
      return rval;
    }

  if (code == MODIFY_EXPR)
    fnname = ansi_assopname[(int) TREE_CODE (arg3)];
  else
    fnname = ansi_opname[(int) code];

  global_fn = IDENTIFIER_GLOBAL_VALUE (fnname);

  /* This is the last point where we will accept failure.  This
     may be too eager if we wish an overloaded operator not to match,
     but would rather a normal operator be called on a type-converted
     argument.  */

  if (IS_AGGR_TYPE (type1))
    {
      fields1 = lookup_fnfields (TYPE_BINFO (type1), fnname, 0);
      /* ARM $13.4.7, prefix/postfix ++/--.  */
      if (code == POSTINCREMENT_EXPR || code == POSTDECREMENT_EXPR)
	{
	  xarg2 = integer_zero_node;
	  binary_is_unary = 0;

	  if (fields1)
	    {
	      tree t, t2;
	      int have_postfix = 0;

	      /* Look for an `operator++ (int)'.  If they didn't have
		 one, then we fall back to the old way of doing things.  */
	      for (t = TREE_VALUE (fields1); t ; t = TREE_CHAIN (t))
		{
		  t2 = TYPE_ARG_TYPES (TREE_TYPE (t));
		  if (TREE_CHAIN (t2) != NULL_TREE
		      && TREE_VALUE (TREE_CHAIN (t2)) == integer_type_node)
		    {
		      have_postfix = 1;
		      break;
		    }
		}

	      if (! have_postfix)
		{
		  char *op = POSTINCREMENT_EXPR ? "++" : "--";

		  /* There's probably a LOT of code in the world that
		     relies upon this old behavior.  So we'll only give this
		     warning when we've been given -pedantic.  A few
		     releases after 2.4, we'll convert this to be a pedwarn
		     or something else more appropriate.  */
		  if (pedantic)
		    warning ("no `operator%s (int)' declared for postfix `%s'",
			     op, op);
		  xarg2 = NULL_TREE;
		  binary_is_unary = 1;
		}
	    }
	}
    }

  if (fields1 == NULL_TREE && global_fn == NULL_TREE)
    return rval;

  /* If RVAL winds up being `error_mark_node', we will return
     that... There is no way that normal semantics of these
     operators will succeed.  */

  /* This argument may be an uncommitted OFFSET_REF.  This is
     the case for example when dealing with static class members
     which are referenced from their class name rather than
     from a class instance.  */
  if (TREE_CODE (xarg1) == OFFSET_REF
      && TREE_CODE (TREE_OPERAND (xarg1, 1)) == VAR_DECL)
    xarg1 = TREE_OPERAND (xarg1, 1);
  if (try_second && xarg2 && TREE_CODE (xarg2) == OFFSET_REF
      && TREE_CODE (TREE_OPERAND (xarg2, 1)) == VAR_DECL)
    xarg2 = TREE_OPERAND (xarg2, 1);

  if (global_fn)
    flags |= LOOKUP_GLOBAL;

  if (code == CALL_EXPR)
    {
      /* This can only be a member function.  */
      return build_method_call (xarg1, fnname, xarg2,
				NULL_TREE, LOOKUP_NORMAL);
    }
  else if (tree_code_length[(int) code] == 1 || binary_is_unary)
    {
      parms = NULL_TREE;
      rval = build_method_call (xarg1, fnname, NULL_TREE, NULL_TREE, flags);
    }
  else if (code == COND_EXPR)
    {
      parms = tree_cons (0, xarg2, build_tree_list (NULL_TREE, arg3));
      rval = build_method_call (xarg1, fnname, parms, NULL_TREE, flags);
    }
  else if (code == METHOD_CALL_EXPR)
    {
      /* must be a member function.  */
      parms = tree_cons (NULL_TREE, xarg2, arg3);
      return build_method_call (xarg1, fnname, parms, NULL_TREE, LOOKUP_NORMAL);
    }
  else if (fields1)
    {
      parms = build_tree_list (NULL_TREE, xarg2);
      rval = build_method_call (xarg1, fnname, parms, NULL_TREE, flags);
    }
  else
    {
      parms = tree_cons (NULL_TREE, xarg1,
			 build_tree_list (NULL_TREE, xarg2));
      rval = build_overload_call (fnname, parms, flags & LOOKUP_COMPLAIN,
				  (struct candidate *)0);
    }

  /* If we did not win, do not lose yet, since type conversion may work.  */
  if (TREE_CODE (rval) == ERROR_MARK)
    {
      if (flags & LOOKUP_COMPLAIN)
	return rval;
      return 0;
    }

  return rval;
}

/* This function takes an identifier, ID, and attempts to figure out what
   it means. There are a number of possible scenarios, presented in increasing
   order of hair:

   1) not in a class's scope
   2) in class's scope, member name of the class's method
   3) in class's scope, but not a member name of the class
   4) in class's scope, member name of a class's variable

   NAME is $1 from the bison rule. It is an IDENTIFIER_NODE.
   VALUE is $$ from the bison rule. It is the value returned by lookup_name ($1)
   yychar is the pending input character (suitably encoded :-).

   As a last ditch, try to look up the name as a label and return that
   address.

   Values which are declared as being of REFERENCE_TYPE are
   automatically dereferenced here (as a hack to make the
   compiler faster).  */

tree
hack_identifier (value, name, yychar)
     tree value, name;
     int yychar;
{
  tree type;

  if (TREE_CODE (value) == ERROR_MARK)
    {
      if (current_class_name)
	{
	  tree fields = lookup_fnfields (TYPE_BINFO (current_class_type), name, 1);
	  if (fields == error_mark_node)
	    return error_mark_node;
	  if (fields)
	    {
	      tree fndecl;

	      fndecl = TREE_VALUE (fields);
	      my_friendly_assert (TREE_CODE (fndecl) == FUNCTION_DECL, 251);
	      if (DECL_CHAIN (fndecl) == NULL_TREE)
		{
		  warning ("methods cannot be converted to function pointers");
		  return fndecl;
		}
	      else
		{
		  error ("ambiguous request for method pointer `%s'",
			 IDENTIFIER_POINTER (name));
		  return error_mark_node;
		}
	    }
	}
      if (flag_labels_ok && IDENTIFIER_LABEL_VALUE (name))
	{
	  return IDENTIFIER_LABEL_VALUE (name);
	}
      return error_mark_node;
    }

  type = TREE_TYPE (value);
  if (TREE_CODE (value) == FIELD_DECL)
    {
      if (current_class_decl == NULL_TREE)
	{
	  error ("request for member `%s' in static member function",
		 IDENTIFIER_POINTER (DECL_NAME (value)));
	  return error_mark_node;
	}
      TREE_USED (current_class_decl) = 1;
      if (yychar == '(')
	if (! ((TYPE_LANG_SPECIFIC (type)
		&& TYPE_OVERLOADS_CALL_EXPR (type))
	       || (TREE_CODE (type) == REFERENCE_TYPE
		   && TYPE_LANG_SPECIFIC (TREE_TYPE (type))
		   && TYPE_OVERLOADS_CALL_EXPR (TREE_TYPE (type))))
	    && TREE_CODE (type) != FUNCTION_TYPE
	    && TREE_CODE (type) != METHOD_TYPE
	    && (TREE_CODE (type) != POINTER_TYPE
		|| (TREE_CODE (TREE_TYPE (type)) != FUNCTION_TYPE
		    && TREE_CODE (TREE_TYPE (type)) != METHOD_TYPE)))
	  {
	    error ("component `%s' is not a method",
		   IDENTIFIER_POINTER (name));
	    return error_mark_node;
	  }
      /* Mark so that if we are in a constructor, and then find that
	 this field was initialized by a base initializer,
	 we can emit an error message.  */
      TREE_USED (value) = 1;
      return build_component_ref (C_C_D, name, 0, 1);
    }

  if (TREE_CODE (value) == TREE_LIST)
    {
      tree t = value;
      while (t && TREE_CODE (t) == TREE_LIST)
	{
	  assemble_external (TREE_VALUE (t));
	  TREE_USED (t) = 1;
	  t = TREE_CHAIN (t);
	}
    }
  else
    {
      assemble_external (value);
      TREE_USED (value) = 1;
    }

  if (TREE_CODE_CLASS (TREE_CODE (value)) == 'd' && DECL_NONLOCAL (value))
    {
      if (DECL_CLASS_CONTEXT (value) != current_class_type)
	{
	  tree path;
	  enum visibility_type visibility;
	  register tree context
	    = (TREE_CODE (value) == FUNCTION_DECL && DECL_VIRTUAL_P (value))
	      ? DECL_CLASS_CONTEXT (value)
	      : DECL_CONTEXT (value);

	  get_base_distance (context, current_class_type, 0, &path);
	  visibility = compute_visibility (path, value);
	  if (visibility != visibility_public)
	    {
	      if (TREE_CODE (value) == VAR_DECL)
		error ("static member `%s' is from private base class",
		       IDENTIFIER_POINTER (name));
	      else
		error ("enum `%s' is from private base class",
		       IDENTIFIER_POINTER (name));
	      return error_mark_node;
	    }
	}
      return value;
    }
  if (TREE_CODE (value) == TREE_LIST && TREE_NONLOCAL_FLAG (value))
    {
      if (type == 0)
	{
	  error ("request for member `%s' is ambiguous in multiple inheritance lattice",
		 IDENTIFIER_POINTER (name));
	  return error_mark_node;
	}

      return value;
    }

  if (TREE_CODE (type) == REFERENCE_TYPE)
    {
      my_friendly_assert (TREE_CODE (value) == VAR_DECL
			  || TREE_CODE (value) == PARM_DECL
			  || TREE_CODE (value) == RESULT_DECL, 252);
      if (DECL_REFERENCE_SLOT (value))
	return DECL_REFERENCE_SLOT (value);
    }
  return value;
}


/* Given an object OF, and a type conversion operator COMPONENT
   build a call to the conversion operator, if a call is requested,
   or return the address (as a pointer to member function) if one is not.

   OF can be a TYPE_DECL or any kind of datum that would normally
   be passed to `build_component_ref'.  It may also be NULL_TREE,
   in which case `current_class_type' and `current_class_decl'
   provide default values.

   BASETYPE_PATH, if non-null, is the path of basetypes
   to go through before we get the the instance of interest.

   PROTECT says whether we apply C++ scoping rules or not.  */
tree
build_component_type_expr (of, component, basetype_path, protect)
     tree of, component, basetype_path;
     int protect;
{
  tree cname = NULL_TREE;
  tree tmp, last;
  tree name;
  int flags = protect ? LOOKUP_NORMAL : LOOKUP_COMPLAIN;

  if (of)
    my_friendly_assert (IS_AGGR_TYPE (TREE_TYPE (of)), 253);
  my_friendly_assert (TREE_CODE (component) == TYPE_EXPR, 254);

  tmp = TREE_OPERAND (component, 0);
  last = NULL_TREE;

  while (tmp)
    {
      switch (TREE_CODE (tmp))
	{
	case CALL_EXPR:
	  if (last)
	    TREE_OPERAND (last, 0) = TREE_OPERAND (tmp, 0);
	  else
	    TREE_OPERAND (component, 0) = TREE_OPERAND (tmp, 0);
	  if (TREE_OPERAND (tmp, 0)
	      && TREE_OPERAND (tmp, 0) != void_list_node)
	    {
	      error ("operator <typename> requires empty parameter list");
	      TREE_OPERAND (tmp, 0) = NULL_TREE;
	    }
	  last = groktypename (build_tree_list (TREE_TYPE (component),
						TREE_OPERAND (component, 0)));
	  name = build_typename_overload (last);
	  TREE_TYPE (name) = last;

	  if (of && TREE_CODE (of) != TYPE_DECL)
	    return build_method_call (of, name, NULL_TREE, NULL_TREE, flags);
	  else if (of)
	    {
	      tree this_this;

	      if (current_class_decl == NULL_TREE)
		{
		  error ("object required for `operator <typename>' call");
		  return error_mark_node;
		}

	      this_this = convert_pointer_to (TREE_TYPE (of), current_class_decl);
	      return build_method_call (this_this, name, NULL_TREE,
					NULL_TREE, flags | LOOKUP_NONVIRTUAL);
	    }
	  else if (current_class_decl)
	    return build_method_call (tmp, name, NULL_TREE, NULL_TREE, flags);

	  error ("object required for `operator <typename>' call");
	  return error_mark_node;

	case INDIRECT_REF:
	case ADDR_EXPR:
	case ARRAY_REF:
	  break;

	case SCOPE_REF:
	  my_friendly_assert (cname == 0, 255);
	  cname = TREE_OPERAND (tmp, 0);
	  tmp = TREE_OPERAND (tmp, 1);
	  break;

	default:
	  my_friendly_abort (77);
	}
      last = tmp;
      tmp = TREE_OPERAND (tmp, 0);
    }

  last = groktypename (build_tree_list (TREE_TYPE (component), TREE_OPERAND (component, 0)));
  name = build_typename_overload (last);
  TREE_TYPE (name) = last;
  if (of && TREE_CODE (of) == TYPE_DECL)
    {
      if (cname == NULL_TREE)
	{
	  cname = DECL_NAME (of);
	  of = NULL_TREE;
	}
      else my_friendly_assert (cname == DECL_NAME (of), 256);
    }

  if (of)
    {
      tree this_this;

      if (current_class_decl == NULL_TREE)
	{
	  error ("object required for `operator <typename>' call");
	  return error_mark_node;
	}

      this_this = convert_pointer_to (TREE_TYPE (of), current_class_decl);
      return build_component_ref (this_this, name, 0, protect);
    }
  else if (cname)
    return build_offset_ref (cname, name);
  else if (current_class_name)
    return build_offset_ref (current_class_name, name);

  error ("object required for `operator <typename>' member reference");
  return error_mark_node;
}
