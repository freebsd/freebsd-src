/* Garbage collection primitives for GNU C++.
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.
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


#include "config.h"
#include "tree.h"
#include "cp-tree.h"
#include "flags.h"

#undef NULL
#define NULL 0

extern tree define_function ();
extern tree build_t_desc_overload ();

/* This is the function decl for the (pseudo-builtin) __gc_protect
   function.  Args are (class *value, int index); Returns value.  */
tree gc_protect_fndecl;

/* This is the function decl for the (pseudo-builtin) __gc_unprotect
   function.  Args are (int index); void return.  */
tree gc_unprotect_fndecl;

/* This is the function decl for the (pseudo-builtin) __gc_push
   function.  Args are (int length); void return.  */
tree gc_push_fndecl;

/* This is the function decl for the (pseudo-builtin) __gc_pop
   function.  Args are void; void return.  */
tree gc_pop_fndecl;

/* Special integers that are used to represent bits in gc-safe objects.  */
tree gc_nonobject;
tree gc_visible;
tree gc_white;
tree gc_offwhite;
tree gc_grey;
tree gc_black;

/* in c-common.c */
extern tree combine_strings PROTO((tree));

/* Predicate that returns non-zero if TYPE needs some kind of
   entry for the GC.  Returns zero otherwise.  */
int
type_needs_gc_entry (type)
     tree type;
{
  tree ttype = type;

  if (! flag_gc || type == error_mark_node)
    return 0;

  /* Aggregate types need gc entries if any of their members
     need gc entries.  */
  if (IS_AGGR_TYPE (type))
    {
      tree binfos;
      tree fields = TYPE_FIELDS (type);
      int i;

      /* We don't care about certain pointers.  Pointers
	 to virtual baseclasses are always up front.  We also
	 cull out virtual function table pointers because it's
	 easy, and it simplifies the logic.*/
      while (fields
	     && (DECL_NAME (fields) == NULL_TREE
		 || VFIELD_NAME_P (DECL_NAME (fields))
		 || VBASE_NAME_P (DECL_NAME (fields))
		 || !strcmp (IDENTIFIER_POINTER (DECL_NAME (fields)), "__bits")))
	fields = TREE_CHAIN (fields);

      while (fields)
	{
	  if (type_needs_gc_entry (TREE_TYPE (fields)))
	    return 1;
	  fields = TREE_CHAIN (fields);
	}

      binfos = TYPE_BINFO_BASETYPES (type);
      if (binfos)
	for (i = TREE_VEC_LENGTH (binfos)-1; i >= 0; i--)
	  if (type_needs_gc_entry (BINFO_TYPE (TREE_VEC_ELT (binfos, i))))
	    return 1;

      return 0;
    }

  while (TREE_CODE (ttype) == ARRAY_TYPE
	 && TREE_CODE (TREE_TYPE (ttype)) == ARRAY_TYPE)
    ttype = TREE_TYPE (ttype);
  if ((TREE_CODE (ttype) == POINTER_TYPE
       || TREE_CODE (ttype) == ARRAY_TYPE
       || TREE_CODE (ttype) == REFERENCE_TYPE)
      && IS_AGGR_TYPE (TREE_TYPE (ttype))
      && CLASSTYPE_DOSSIER (TREE_TYPE (ttype)))
    return 1;

  return 0;
}

/* Predicate that returns non-zero iff FROM is safe from the GC.
   
   If TO is nonzero, it means we know that FROM is being stored
   in TO, which make make it safe.  */
int
value_safe_from_gc (to, from)
     tree to, from;
{
  /* First, return non-zero for easy cases: parameters,
     static variables.  */
  if (TREE_CODE (from) == PARM_DECL
      || (TREE_CODE (from) == VAR_DECL
	  && TREE_STATIC (from)))
    return 1;

  /* If something has its address taken, it cannot be
     in the heap, so it doesn't need to be protected.  */
  if (TREE_CODE (from) == ADDR_EXPR || TREE_REFERENCE_EXPR (from))
    return 1;

  /* If we are storing into a static variable, then what
     we store will be safe from the gc.  */
  if (to && TREE_CODE (to) == VAR_DECL
      && TREE_STATIC (to))
    return 1;

  /* Now recurse on structure of FROM.  */
  switch (TREE_CODE (from))
    {
    case COMPONENT_REF:
      /* These guys are special, and safe.  */
      if (TREE_CODE (TREE_OPERAND (from, 1)) == FIELD_DECL
	  && (VFIELD_NAME_P (DECL_NAME (TREE_OPERAND (from, 1)))
	      || VBASE_NAME_P (DECL_NAME (TREE_OPERAND (from, 1)))))
	return 1;
      /* fall through...  */
    case NOP_EXPR:
    case CONVERT_EXPR:
    case NON_LVALUE_EXPR:
    case WITH_CLEANUP_EXPR:
    case SAVE_EXPR:
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
      if (value_safe_from_gc (to, TREE_OPERAND (from, 0)))
	return 1;
      break;

    case VAR_DECL:
    case PARM_DECL:
      /* We can safely pass these things as parameters to functions.  */
      if (to == 0)
	return 1;

    case ARRAY_REF:
    case INDIRECT_REF:
    case RESULT_DECL:
    case OFFSET_REF:
    case CALL_EXPR:
    case METHOD_CALL_EXPR:
      break;

    case COMPOUND_EXPR:
    case TARGET_EXPR:
      if (value_safe_from_gc (to, TREE_OPERAND (from, 1)))
	return 1;
      break;

    case COND_EXPR:
      if (value_safe_from_gc (to, TREE_OPERAND (from, 1))
	  && value_safe_from_gc (to, TREE_OPERAND (from, 2)))
	return 1;
      break;

    case PLUS_EXPR:
    case MINUS_EXPR:
      if ((type_needs_gc_entry (TREE_TYPE (TREE_OPERAND (from, 0)))
	   || value_safe_from_gc (to, TREE_OPERAND (from, 0)))
	  && (type_needs_gc_entry (TREE_TYPE (TREE_OPERAND (from, 1))) == 0
	      || value_safe_from_gc (to, TREE_OPERAND (from, 1))))
	return 1;
      break;

    case RTL_EXPR:
      /* Every time we build an RTL_EXPR in the front-end, we must
	 ensure that everything in it is safe from the garbage collector.
	 ??? This has only been done for `build_new'.  */
      return 1;

    default:
      my_friendly_abort (41);
    }

  if (to == 0)
    return 0;

  /* FROM wasn't safe.  But other properties of TO might make it safe.  */
  switch (TREE_CODE (to))
    {
    case VAR_DECL:
    case PARM_DECL:
      /* We already culled out static VAR_DECLs above.  */
      return 0;

    case COMPONENT_REF:
      /* These guys are special, and safe.  */
      if (TREE_CODE (TREE_OPERAND (to, 1)) == FIELD_DECL
	  && (VFIELD_NAME_P (DECL_NAME (TREE_OPERAND (to, 1)))
	      || VBASE_NAME_P (DECL_NAME (TREE_OPERAND (to, 1)))))
	return 1;
      /* fall through...  */

    case NOP_EXPR:
    case NON_LVALUE_EXPR:
    case WITH_CLEANUP_EXPR:
    case SAVE_EXPR:
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
      return value_safe_from_gc (TREE_OPERAND (to, 0), from);

    case COMPOUND_EXPR:
    case TARGET_EXPR:
      return value_safe_from_gc (TREE_OPERAND (to, 1), from);

    case COND_EXPR:
      return (value_safe_from_gc (TREE_OPERAND (to, 1), from)
	      && value_safe_from_gc (TREE_OPERAND (to, 2), from));

    case INDIRECT_REF:
    case ARRAY_REF:
      /* This used to be 0, but our current restricted model
	 allows this to be 1.  We'll never get arrays this way.  */
      return 1;

    default:
      my_friendly_abort (42);
    }

  /* Catch-all case is that TO/FROM is not safe.  */
  return 0;
}

/* Function to build a static GC entry for DECL.  TYPE is DECL's type.

   For objects of type `class *', this is just an entry in the
   static vector __PTR_LIST__.

   For objects of type `class[]', this requires building an entry
   in the static vector __ARR_LIST__.

   For aggregates, this records all fields of type `class *'
   and `class[]' in the respective lists above.  */
void
build_static_gc_entry (decl, type)
     tree decl;
     tree type;
{
  /* Now, figure out what sort of entry to build.  */
  if (TREE_CODE (type) == POINTER_TYPE
      || TREE_CODE (type) == REFERENCE_TYPE)
    assemble_gc_entry (IDENTIFIER_POINTER (DECL_NAME (decl)));
  else if (TREE_CODE (type) == RECORD_TYPE)
    {
      tree ref = get_temp_name (build_reference_type (type), 1);
      DECL_INITIAL (ref) = build1 (ADDR_EXPR, TREE_TYPE (ref), decl);
      TREE_CONSTANT (DECL_INITIAL (ref)) = 1;
      finish_decl (ref, DECL_INITIAL (ref), 0, 0);
    }
  else
    {
      /* Not yet implemented.
	 
	 Cons up a static variable that holds address and length info
	 and add that to ___ARR_LIST__.  */
      my_friendly_abort (43);
    }
}

/* Protect FROM from the GC, assuming FROM is going to be
   stored into TO.  We handle three cases for TO here:

   case 1: TO is a stack variable.
   case 2: TO is zero (which means it is a parameter).
   case 3: TO is a return value.  */

tree
protect_value_from_gc (to, from)
     tree to, from;
{
  if (to == 0)
    {
      tree cleanup;

      to = get_temp_regvar (TREE_TYPE (from), from);

      /* Convert from integer to list form since we'll use it twice.  */
      DECL_GC_OFFSET (to) = build_tree_list (NULL_TREE, DECL_GC_OFFSET (to));
      cleanup = build_function_call (gc_unprotect_fndecl,
				     DECL_GC_OFFSET (to));

      if (! expand_decl_cleanup (to, cleanup))
	{
	  compiler_error ("cannot unprotect parameter in this scope");
	  return error_mark_node;
	}
    }

  /* Should never need to protect a value that's headed for static storage.  */
  if (TREE_STATIC (to))
    my_friendly_abort (44);

  switch (TREE_CODE (to))
    {
    case COMPONENT_REF:
    case INDIRECT_REF:
      return protect_value_from_gc (TREE_OPERAND (to, 0), from);

    case VAR_DECL:
    case PARM_DECL:
      {
	tree rval;
	if (DECL_GC_OFFSET (to) == NULL_TREE)
	  {
	    /* Because of a cast or a conversion, we might stick
	       a value into a variable that would not normally
	       have a GC entry.  */
	    DECL_GC_OFFSET (to) = size_int (++current_function_obstack_index);
	  }

	if (TREE_CODE (DECL_GC_OFFSET (to)) != TREE_LIST)
	  {
	    DECL_GC_OFFSET (to)
	      = build_tree_list (NULL_TREE, DECL_GC_OFFSET (to));
	  }

	current_function_obstack_usage = 1;
	rval = build_function_call (gc_protect_fndecl,
				    tree_cons (NULL_TREE, from,
					       DECL_GC_OFFSET (to)));
	TREE_TYPE (rval) = TREE_TYPE (from);
	return rval;
      }
    }

  /* If we fall through the switch, assume we lost.  */
  my_friendly_abort (45);
  /* NOTREACHED */
  return NULL_TREE;
}

/* Given the expression EXP of type `class *', return the head
   of the object pointed to by EXP.  */
tree
build_headof (exp)
     tree exp;
{
  tree type = TREE_TYPE (exp);
  tree vptr, offset;

  if (TREE_CODE (type) != POINTER_TYPE)
    {
      error ("`headof' applied to non-pointer type");
      return error_mark_node;
    }

  vptr = build1 (INDIRECT_REF, TYPE_POINTER_TO (vtable_entry_type), exp);
  offset = build_component_ref (build_array_ref (vptr, integer_one_node),
				get_identifier (VTABLE_DELTA_NAME),
				NULL_TREE, 0);
  return build (PLUS_EXPR, class_star_type_node, exp,
		convert (integer_type_node, offset));
}

/* Given the expression EXP of type `class *', return the
   type descriptor for the object pointed to by EXP.  */
tree
build_classof (exp)
     tree exp;
{
  tree type = TREE_TYPE (exp);
  tree vptr;
  tree t_desc_entry;

  if (TREE_CODE (type) != POINTER_TYPE)
    {
      error ("`classof' applied to non-pointer type");
      return error_mark_node;
    }

  vptr = build1 (INDIRECT_REF, TYPE_POINTER_TO (vtable_entry_type), exp);
  t_desc_entry = build_component_ref (build_array_ref (vptr, integer_one_node),
				      get_identifier (VTABLE_PFN_NAME),
				      NULL_TREE, 0);
  TREE_TYPE (t_desc_entry) = TYPE_POINTER_TO (__t_desc_type_node);
  return t_desc_entry;
}

/* Build and initialize various sorts of descriptors.  Every descriptor
   node has a name associated with it (the name created by mangling).
   For this reason, we use the identifier as our access to the __*_desc
   nodes, instead of sticking them directly in the types.  Otherwise we
   would burden all built-in types (and pointer types) with slots that
   we don't necessarily want to use.

   For each descriptor we build, we build a variable that contains
   the descriptor's information.  When we need this info at runtime,
   all we need is access to these variables.

   Note: these constructors always return the address of the descriptor
   info, since that is simplest for their mutual interaction.  */

static tree
build_generic_desc (decl, elems)
     tree decl;
     tree elems;
{
  tree init = build (CONSTRUCTOR, TREE_TYPE (decl), NULL_TREE, elems);
  TREE_CONSTANT (init) = 1;
  TREE_STATIC (init) = 1;
  TREE_READONLY (init) = 1;

  DECL_INITIAL (decl) = init;
  TREE_STATIC (decl) = 1;
  layout_decl (decl, 0);
  finish_decl (decl, init, 0, 0);

  return IDENTIFIER_AS_DESC (DECL_NAME (decl));
}

/* Build an initializer for a __t_desc node.  So that we can take advantage
   of recursion, we accept NULL for TYPE.
   DEFINITION is greater than zero iff we must define the type descriptor
   (as opposed to merely referencing it).  1 means treat according to
   #pragma interface/#pragma implementation rules.  2 means define as
   global and public, no matter what.  */
tree
build_t_desc (type, definition)
     tree type;
     int definition;
{
  tree tdecl;
  tree tname, name_string;
  tree elems, fields;
  tree parents, vbases, offsets, ivars, methods, target_type;
  int method_count = 0, field_count = 0;

  if (type == NULL_TREE)
    return NULL_TREE;

  tname = build_t_desc_overload (type);
  if (IDENTIFIER_AS_DESC (tname)
      && (!definition || TREE_ASM_WRITTEN (IDENTIFIER_AS_DESC (tname))))
    return IDENTIFIER_AS_DESC (tname);

  tdecl = lookup_name (tname, 0);
  if (tdecl == NULL_TREE)
    {
      tdecl = build_decl (VAR_DECL, tname, __t_desc_type_node);
      DECL_EXTERNAL (tdecl) = 1;
      TREE_PUBLIC (tdecl) = 1;
      tdecl = pushdecl_top_level (tdecl);
    }
  /* If we previously defined it, return the defined result.  */
  else if (definition && DECL_INITIAL (tdecl))
    return IDENTIFIER_AS_DESC (tname);

  if (definition)
    {
      tree taggr = type;
      /* Let T* and T& be written only when T is written (if T is an aggr).
         We do this for const, but not for volatile, since volatile
	 is rare and const is not.  */
      if (!TYPE_VOLATILE (taggr)
	  && (TREE_CODE (taggr) == POINTER_TYPE
	      || TREE_CODE (taggr) == REFERENCE_TYPE)
	  && IS_AGGR_TYPE (TREE_TYPE (taggr)))
	taggr = TREE_TYPE (taggr);

      /* If we know that we don't need to write out this type's
	 vtable, then don't write out it's dossier.  Somebody
	 else will take care of that.  */
      if (IS_AGGR_TYPE (taggr) && CLASSTYPE_VFIELD (taggr))
	{
	  if (CLASSTYPE_VTABLE_NEEDS_WRITING (taggr))
	    {
	      TREE_PUBLIC (tdecl) = !(CLASSTYPE_INTERFACE_ONLY (taggr)
				      || CLASSTYPE_INTERFACE_UNKNOWN (taggr));
	      TREE_STATIC (tdecl) = 1;
	      DECL_EXTERNAL (tdecl) = 0;
	    }
	  else
	    {
	      if (write_virtuals != 0)
		TREE_PUBLIC (tdecl) = 1;
	    }
	}
      else
	{
	  DECL_EXTERNAL (tdecl) = 0;
	  TREE_STATIC (tdecl) = 1;
	  TREE_PUBLIC (tdecl) = (definition > 1);
	}
    }
  SET_IDENTIFIER_AS_DESC (tname, build_unary_op (ADDR_EXPR, tdecl, 0));
  if (!definition || DECL_EXTERNAL (tdecl))
    {
      /* That's it!  */
      finish_decl (tdecl, 0, 0, 0);
      return IDENTIFIER_AS_DESC (tname);
    }

  /* Show that we are defining the t_desc for this type.  */
  DECL_INITIAL (tdecl) = error_mark_node;

  parents = build_tree_list (NULL_TREE, integer_zero_node);
  vbases = build_tree_list (NULL_TREE, integer_zero_node);
  offsets = build_tree_list (NULL_TREE, integer_zero_node);
  methods = NULL_TREE;
  ivars = NULL_TREE;

  if (TYPE_LANG_SPECIFIC (type))
    {
      int i = CLASSTYPE_N_BASECLASSES (type);
      tree method_vec = CLASSTYPE_METHOD_VEC (type);
      tree *meth, *end;
      tree binfos = TYPE_BINFO_BASETYPES (type);
      tree vb = CLASSTYPE_VBASECLASSES (type);

      while (--i >= 0)
	parents = tree_cons (NULL_TREE, build_t_desc (BINFO_TYPE (TREE_VEC_ELT (binfos, i)), 0), parents);

      while (vb)
	{
	  vbases = tree_cons (NULL_TREE, build_t_desc (BINFO_TYPE (vb), 0), vbases);
	  offsets = tree_cons (NULL_TREE, BINFO_OFFSET (vb), offsets);
	  vb = TREE_CHAIN (vb);
	}

      if (method_vec)
	for (meth = TREE_VEC_END (method_vec),
	     end = &TREE_VEC_ELT (method_vec, 0); meth-- != end; )
	  if (*meth)
	    {
	      methods = tree_cons (NULL_TREE, build_m_desc (*meth), methods);
	      method_count++;
	    }
    }

  if (IS_AGGR_TYPE (type))
    {
      for (fields = TYPE_FIELDS (type); fields; fields = TREE_CHAIN (fields))
	if (TREE_CODE (fields) == FIELD_DECL
	    || TREE_CODE (fields) == VAR_DECL)
	  {
	    ivars = tree_cons (NULL_TREE, build_i_desc (fields), ivars);
	    field_count++;
	  }
      ivars = nreverse (ivars);
    }

  parents = finish_table (0, TYPE_POINTER_TO (__t_desc_type_node), parents, 0);
  vbases = finish_table (0, TYPE_POINTER_TO (__t_desc_type_node), vbases, 0);
  offsets = finish_table (0, integer_type_node, offsets, 0);
  methods = finish_table (0, __m_desc_type_node, methods, 0);
  ivars = finish_table (0, __i_desc_type_node, ivars, 0);
  if (TREE_TYPE (type))
    target_type = build_t_desc (TREE_TYPE (type), definition);
  else
    target_type = integer_zero_node;

  name_string = combine_strings (build_string (IDENTIFIER_LENGTH (tname)+1, IDENTIFIER_POINTER (tname)));

  elems = tree_cons (NULL_TREE, build_unary_op (ADDR_EXPR, name_string, 0),
	   tree_cons (NULL_TREE,
		      TYPE_SIZE(type)? size_in_bytes(type) : integer_zero_node,
	     /* really should use bitfield initialization here.  */
	     tree_cons (NULL_TREE, integer_zero_node,
	      tree_cons (NULL_TREE, target_type,
	       tree_cons (NULL_TREE, build_int_2 (field_count, 2),
		tree_cons (NULL_TREE, build_int_2 (method_count, 2),
		 tree_cons (NULL_TREE, build_unary_op (ADDR_EXPR, ivars, 0),
		  tree_cons (NULL_TREE, build_unary_op (ADDR_EXPR, methods, 0),
		   tree_cons (NULL_TREE, build_unary_op (ADDR_EXPR, parents, 0),
		    tree_cons (NULL_TREE, build_unary_op (ADDR_EXPR, vbases, 0),
		     build_tree_list (NULL_TREE, build_unary_op (ADDR_EXPR, offsets, 0))))))))))));
  return build_generic_desc (tdecl, elems);
}

/* Build an initializer for a __i_desc node.  */
tree
build_i_desc (decl)
     tree decl;
{
  tree elems, name_string;
  tree taggr;

  name_string = DECL_NAME (decl);
  name_string = combine_strings (build_string (IDENTIFIER_LENGTH (name_string)+1, IDENTIFIER_POINTER (name_string)));

  /* Now decide whether this ivar should cause it's type to get
     def'd or ref'd in this file.  If the type we are looking at
     has a proxy definition, we look at the proxy (i.e., a
     `foo *' is equivalent to a `foo').  */
  taggr = TREE_TYPE (decl);

  if ((TREE_CODE (taggr) == POINTER_TYPE
       || TREE_CODE (taggr) == REFERENCE_TYPE)
      && TYPE_VOLATILE (taggr) == 0)
    taggr = TREE_TYPE (taggr);

  elems = tree_cons (NULL_TREE, build_unary_op (ADDR_EXPR, name_string, 0),
	     tree_cons (NULL_TREE, DECL_FIELD_BITPOS (decl),
		build_tree_list (NULL_TREE, build_t_desc (TREE_TYPE (decl),
							  ! IS_AGGR_TYPE (taggr)))));
  taggr = build (CONSTRUCTOR, __i_desc_type_node, NULL_TREE, elems);
  TREE_CONSTANT (taggr) = 1;
  TREE_STATIC (taggr) = 1;
  TREE_READONLY (taggr) = 1;
  return taggr;
}

/* Build an initializer for a __m_desc node.  */
tree
build_m_desc (decl)
     tree decl;
{
  tree taggr, elems, name_string;
  tree parm_count, req_count, vindex, vcontext;
  tree parms;
  int p_count, r_count;
  tree parm_types = NULL_TREE;

  for (parms = TYPE_ARG_TYPES (TREE_TYPE (decl)), p_count = 0, r_count = 0;
       parms != NULL_TREE; parms = TREE_CHAIN (parms), p_count++)
    {
      taggr = TREE_VALUE (parms);
      if ((TREE_CODE (taggr) == POINTER_TYPE
	   || TREE_CODE (taggr) == REFERENCE_TYPE)
	  && TYPE_VOLATILE (taggr) == 0)
	taggr = TREE_TYPE (taggr);

      parm_types = tree_cons (NULL_TREE, build_t_desc (TREE_VALUE (parms),
						       ! IS_AGGR_TYPE (taggr)),
			      parm_types);
      if (TREE_PURPOSE (parms) == NULL_TREE)
	r_count++;
    }

  parm_types = finish_table (0, TYPE_POINTER_TO (__t_desc_type_node),
			     nreverse (parm_types), 0);
  parm_count = build_int_2 (p_count, 0);
  req_count = build_int_2 (r_count, 0);

  if (DECL_VINDEX (decl))
    vindex = DECL_VINDEX (decl);
  else
    vindex = integer_zero_node;
  if (DECL_CONTEXT (decl)
      && TREE_CODE_CLASS (TREE_CODE (DECL_CONTEXT (decl))) == 't')
    vcontext = build_t_desc (DECL_CONTEXT (decl), 0);
  else
    vcontext = integer_zero_node;
  name_string = DECL_NAME (decl);
  if (name_string == NULL)
      name_string = DECL_ASSEMBLER_NAME (decl);
  name_string = combine_strings (build_string (IDENTIFIER_LENGTH (name_string)+1, IDENTIFIER_POINTER (name_string)));

  /* Now decide whether the return type of this mvar
     should cause it's type to get def'd or ref'd in this file.
     If the type we are looking at has a proxy definition,
     we look at the proxy (i.e., a `foo *' is equivalent to a `foo').  */
  taggr = TREE_TYPE (TREE_TYPE (decl));

  if ((TREE_CODE (taggr) == POINTER_TYPE
       || TREE_CODE (taggr) == REFERENCE_TYPE)
      && TYPE_VOLATILE (taggr) == 0)
    taggr = TREE_TYPE (taggr);

  elems = tree_cons (NULL_TREE, build_unary_op (ADDR_EXPR, name_string, 0),
	     tree_cons (NULL_TREE, vindex,
		tree_cons (NULL_TREE, vcontext,
		   tree_cons (NULL_TREE, build_t_desc (TREE_TYPE (TREE_TYPE (decl)),
						       ! IS_AGGR_TYPE (taggr)),
		      tree_cons (NULL_TREE, build_c_cast (TYPE_POINTER_TO (default_function_type), build_unary_op (ADDR_EXPR, decl, 0)),
			 tree_cons (NULL_TREE, parm_count,
			    tree_cons (NULL_TREE, req_count,
			       build_tree_list (NULL_TREE, build_unary_op (ADDR_EXPR, parm_types, 0)))))))));

  taggr = build (CONSTRUCTOR, __m_desc_type_node, NULL_TREE, elems);
  TREE_CONSTANT (taggr) = 1;
  TREE_STATIC (taggr) = 1;
  TREE_READONLY (taggr) = 1;
  return taggr;
}

/* Conditionally emit code to set up an unwind-protect for the
   garbage collector.  If this function doesn't do anything that involves
   the garbage collector, then do nothing.  Otherwise, call __gc_push
   at the beginning and __gc_pop at the end.

   NOTE!  The __gc_pop function must operate transparently, since
   it comes where the logical return label lies.  This means that
   at runtime *it* must preserve any return value registers.  */

void
expand_gc_prologue_and_epilogue ()
{
  extern tree maybe_gc_cleanup;
  struct rtx_def *last_parm_insn, *mark;
  extern struct rtx_def *get_last_insn ();
  extern struct rtx_def *get_first_nonparm_insn ();
  extern struct rtx_def *previous_insn ();
  tree action;

  /* If we didn't need the obstack, don't cons any space.  */
  if (current_function_obstack_index == 0
      || current_function_obstack_usage == 0)
    return;

  mark = get_last_insn ();
  last_parm_insn = get_first_nonparm_insn ();
  if (last_parm_insn == 0) last_parm_insn = mark;
  else last_parm_insn = previous_insn (last_parm_insn);

  action = build_function_call (gc_push_fndecl,
				build_tree_list (NULL_TREE, size_int (++current_function_obstack_index)));
  expand_expr_stmt (action);

  reorder_insns (next_insn (mark), get_last_insn (), last_parm_insn);

  /* This will be expanded as a cleanup.  */
  TREE_VALUE (maybe_gc_cleanup)
    = build_function_call (gc_pop_fndecl, NULL_TREE);
}

/* Some day we'll use this function as a call-back and clean
   up all the unnecessary gc dribble that we otherwise create.  */
void
lang_expand_end_bindings (first, last)
     struct rtx_def *first, *last;
{
}

void
init_gc_processing ()
{
  tree parmtypes = hash_tree_chain (class_star_type_node,
				    hash_tree_chain (integer_type_node, NULL_TREE));
  gc_protect_fndecl = define_function ("__gc_protect",
				       build_function_type (class_star_type_node, parmtypes),
				       NOT_BUILT_IN, 0, 0);

  parmtypes = hash_tree_chain (integer_type_node, NULL_TREE);
  gc_unprotect_fndecl = define_function ("__gc_unprotect",
					 build_function_type (void_type_node, parmtypes),
					 NOT_BUILT_IN, 0, 0);

  gc_push_fndecl = define_function ("__gc_push",
				    TREE_TYPE (gc_unprotect_fndecl),
				    NOT_BUILT_IN, 0, 0);

  gc_pop_fndecl = define_function ("__gc_pop",
				   build_function_type (void_type_node,
							void_list_node),
				   NOT_BUILT_IN, 0, 0);
  gc_nonobject = build_int_2 (0x80000000, 0);
  gc_visible = build_int_2 (0x40000000, 0);
  gc_white = integer_zero_node;
  gc_offwhite = build_int_2 (0x10000000, 0);
  gc_grey = build_int_2 (0x20000000, 0);
  gc_black = build_int_2 (0x30000000, 0);
}
