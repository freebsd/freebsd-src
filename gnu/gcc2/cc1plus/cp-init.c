/* Handle initialization things in C++.
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


/* High-level class interface. */

#include "config.h"
#include "tree.h"
#include "rtl.h"
#include "cp-tree.h"
#include "flags.h"

#undef NULL
#define NULL 0

/* In C++, structures with well-defined constructors are initialized by
   those constructors, unasked.  CURRENT_BASE_INIT_LIST
   holds a list of stmts for a BASE_INIT term in the grammar.
   This list has one element for each base class which must be
   initialized.  The list elements are [basename, init], with
   type basetype.  This allows the possibly anachronistic form
   (assuming d : a, b, c) "d (int a) : c(a+5), b (a-4), a (a+3)"
   where each successive term can be handed down the constructor
   line.  Perhaps this was not intended.  */
tree current_base_init_list, current_member_init_list;

void emit_base_init ();
void check_base_init ();
static void expand_aggr_vbase_init ();
void expand_member_init ();
void expand_aggr_init ();

static void expand_aggr_init_1 ();
static void expand_recursive_init_1 ();
static void expand_recursive_init ();
tree expand_vec_init ();
tree build_vec_delete ();

static void add_friend (), add_friends ();

/* Cache _builtin_new and _builtin_delete exprs.  */
static tree BIN, BID;

static tree minus_one;

/* Set up local variable for this file.  MUST BE CALLED AFTER
   INIT_DECL_PROCESSING.  */

tree BI_header_type, BI_header_size;

void init_init_processing ()
{
  tree op_id;
  tree fields[2];

  /* Define implicit `operator new' and `operator delete' functions.  */
  BIN = default_conversion (TREE_VALUE (IDENTIFIER_GLOBAL_VALUE (ansi_opname[(int) NEW_EXPR])));
  TREE_USED (TREE_OPERAND (BIN, 0)) = 0;
  BID = default_conversion (TREE_VALUE (IDENTIFIER_GLOBAL_VALUE (ansi_opname[(int) DELETE_EXPR])));
  TREE_USED (TREE_OPERAND (BID, 0)) = 0;
  minus_one = build_int_2 (-1, -1);

  op_id = ansi_opname[(int) NEW_EXPR];
  IDENTIFIER_GLOBAL_VALUE (op_id) = BIN;
  op_id = ansi_opname[(int) DELETE_EXPR];
  IDENTIFIER_GLOBAL_VALUE (op_id) = BID;

  /* Define the structure that holds header information for
     arrays allocated via operator new.  */
  BI_header_type = make_lang_type (RECORD_TYPE);
  fields[0] = build_lang_field_decl (FIELD_DECL, get_identifier ("nelts"),
				     sizetype);
  fields[1] = build_lang_field_decl (FIELD_DECL, get_identifier ("ptr_2comp"),
				     ptr_type_node);
  finish_builtin_type (BI_header_type, "__new_cookie", fields, 1, double_type_node);
  BI_header_size = size_in_bytes (BI_header_type);
}

/* Recursive subroutine of emit_base_init.  For main type T,
   recursively initialize the vfields of the base type PARENT.
   RECURSE is non-zero when this function is being called
   recursively.  */

static void
init_vfields (t, parent, recurse)
     tree t, parent;
     int recurse;
{
  tree vfields;

  /* Initialize all the virtual function table fields that
     do not come from virtual base classes.  */
  vfields = CLASSTYPE_VFIELDS (parent);
  while (vfields)
    {
      tree basetype = VF_DERIVED_VALUE (vfields)
	? TYPE_MAIN_VARIANT (VF_DERIVED_VALUE (vfields))
	  : VF_BASETYPE_VALUE (vfields);

      /* If the vtable installed by the constructor was not
	 the right one, fix that here.  */
      if (TREE_ADDRESSABLE (vfields)
	  && CLASSTYPE_NEEDS_VIRTUAL_REINIT (basetype)
	  && (recurse > 0
	      || TYPE_HAS_CONSTRUCTOR (basetype)
	      /* BASE_INIT_LIST has already initialized the immediate basetypes.  */
	      || get_base_distance (basetype, t, 0, (tree *) 0) > 1))
	{
	  tree binfo = binfo_value (basetype, t);
	  if ((recurse != 0 && (binfo != binfo_value (basetype, parent)))
	      || (recurse == 0
		  && BINFO_VTABLE (binfo) != TYPE_BINFO_VTABLE (basetype)))
	    {
	      tree ptr = convert_pointer_to (binfo, current_class_decl);
	      expand_expr_stmt (build_virtual_init (TYPE_BINFO (t), binfo, ptr));
	    }
	  init_vfields (t, basetype, recurse+1);
	}
      vfields = TREE_CHAIN (vfields);
    }
}

/* Perform whatever initialization have yet to be done on the
   base class of the class variable.  These actions are in
   the global variable CURRENT_BASE_INIT_LIST.  Such an
   action could be NULL_TREE, meaning that the user has explicitly
   called the base class constructor with no arguments.

   If there is a need for a call to a constructor, we
   must surround that call with a pushlevel/poplevel pair,
   since we are technically at the PARM level of scope.

   Argument IMMEDIATELY, if zero, forces a new sequence to be generated
   to contain these new insns, so it can be emitted later.  This sequence
   is saved in the global variable BASE_INIT_INSNS.  Otherwise, the insns
   are emitted into the current sequence.

   Note that emit_base_init does *not* initialize virtual
   base classes.  That is done specially, elsewhere.  */
   
void
emit_base_init (t, immediately)
     tree t;
     int immediately;
{
  extern tree in_charge_identifier;

  tree member, decl, vbases;
  tree init_list;
  int pass, start;
  tree t_binfo = TYPE_BINFO (t);
  tree binfos = BINFO_BASETYPES (t_binfo);
  int i, n_baseclasses = binfos ? TREE_VEC_LENGTH (binfos) : 0;
  tree fields_to_unmark = NULL_TREE;

  if (! immediately)
    {
      do_pending_stack_adjust ();
      start_sequence ();
    }

  if (write_symbols == NO_DEBUG)
    /* As a matter of principle, `start_sequence' should do this.  */
    emit_note (0, -1);
  else
    /* Always emit a line number note so we can step into constructors.  */
    emit_line_note_force (DECL_SOURCE_FILE (current_function_decl),
			  DECL_SOURCE_LINE (current_function_decl));

  /* In this case, we always need IN_CHARGE_NODE, because we have
     to know whether to deallocate or not before exiting.  */
  if (flag_handle_exceptions == 2
      && lookup_name (in_charge_identifier, 0) == NULL_TREE)
    {
      tree in_charge_node = pushdecl (build_decl (VAR_DECL, in_charge_identifier,
						  integer_type_node));
      store_init_value (in_charge_node, build (EQ_EXPR, integer_type_node,
					       current_class_decl,
					       integer_zero_node));
      expand_decl (in_charge_node);
      expand_decl_init (in_charge_node);
    }

  start = ! TYPE_USES_VIRTUAL_BASECLASSES (t);
  for (pass = start; pass < 2; pass++)
    {
      tree vbase_init_list = NULL_TREE;

      for (init_list = current_base_init_list; init_list;
	   init_list = TREE_CHAIN (init_list))
	{
	  tree basename = TREE_PURPOSE (init_list);
	  tree binfo;
	  tree init = TREE_VALUE (init_list);

	  if (basename == NULL_TREE)
	    {
	      /* Initializer for single base class.  Must not
		 use multiple inheritance or this is ambiguous.  */
	      switch (n_baseclasses)
		{
		case 0:
		  error ("type `%s' does not have a base class to initialize",
			 IDENTIFIER_POINTER (current_class_name));
		  return;
		case 1:
		  break;
		default:
		  error ("unnamed initializer ambiguous for type `%s' which uses multiple inheritance", IDENTIFIER_POINTER (current_class_name));
		  return;
		}
	      binfo = TREE_VEC_ELT (binfos, 0);
	    }
	  else if (is_aggr_typedef (basename, 1))
	    {
	      binfo = binfo_or_else (IDENTIFIER_TYPE_VALUE (basename), t);
	      if (binfo == NULL_TREE)
		continue;

	      /* Virtual base classes are special cases.  Their initializers
		 are recorded with this constructor, and they are used when
		 this constructor is the top-level constructor called.  */
	      if (! TREE_VIA_VIRTUAL (binfo))
		{
		  /* Otherwise, if it is not an immediate base class, complain.  */
		  for (i = n_baseclasses-1; i >= 0; i--)
		    if (BINFO_TYPE (binfo) == BINFO_TYPE (TREE_VEC_ELT (binfos, i)))
		      break;
		  if (i < 0)
		    {
		      error ("type `%s' is not an immediate base class of type `%s'",
			     IDENTIFIER_POINTER (basename),
			     IDENTIFIER_POINTER (current_class_name));
		      continue;
		    }
		}
	    }
	  else
	    continue;

	  /* The base initialization list goes up to the first
	     base class which can actually use it.  */

	  if (pass == start)
	    {
	      char *msgp = (! TYPE_HAS_CONSTRUCTOR (BINFO_TYPE (binfo)))
		? "cannot pass initialization up to class `%s'" : 0;

	      while (! TYPE_HAS_CONSTRUCTOR (BINFO_TYPE (binfo))
		     && BINFO_BASETYPES (binfo) != NULL_TREE
		     && TREE_VEC_LENGTH (BINFO_BASETYPES (binfo)) == 1)
		{
		  /* ?? This should be fixed in RENO by forcing
		     default constructors to exist.  */
		  SET_BINFO_BASEINIT_MARKED (binfo);
		  binfo = BINFO_BASETYPE (binfo, 0);
		}

	      if (TYPE_HAS_CONSTRUCTOR (BINFO_TYPE (binfo)))
		{
		  if (msgp)
		    {
		      if (pedantic)
			error_with_aggr_type (binfo, msgp);
		      else
			msgp = 0;
		    }
		}
	      else
		{
		  msgp = "no constructor found for initialization of `%s'";
		  error (msgp, IDENTIFIER_POINTER (basename));
		}

	      if (BINFO_BASEINIT_MARKED (binfo))
		{
		  msgp = "class `%s' initializer already specified";
		  error (msgp, IDENTIFIER_POINTER (basename));
		}
	      if (msgp)
		continue;

	      SET_BINFO_BASEINIT_MARKED (binfo);
	      if (TREE_VIA_VIRTUAL (binfo))
		{
		  vbase_init_list = tree_cons (init, BINFO_TYPE (binfo),
					       vbase_init_list);
		  continue;
		}
	      if (pass == 0)
		continue;
	    }
	  else if (TREE_VIA_VIRTUAL (binfo))
	    continue;

	  member = convert_pointer_to (binfo, current_class_decl);
	  expand_aggr_init_1 (t_binfo, 0,
			      build_indirect_ref (member, 0), init,
			      BINFO_OFFSET_ZEROP (binfo),
			      LOOKUP_PROTECTED_OK|LOOKUP_COMPLAIN);
	  if (flag_handle_exceptions == 2 && TYPE_NEEDS_DESTRUCTOR (BINFO_TYPE (binfo)))
	    {
	      cplus_expand_start_try (1);
	      push_exception_cleanup (member);
	    }
	}

      if (pass == 0)
	{
	  tree first_arg = TREE_CHAIN (DECL_ARGUMENTS (current_function_decl));
	  tree vbases;

	  if (DECL_NAME (current_function_decl) == NULL_TREE
	      && TREE_CHAIN (first_arg) != NULL_TREE)
	    {
	      /* If there are virtual baseclasses without initialization
		 specified, and this is a default X(X&) constructor,
		 build the initialization list so that each virtual baseclass
		 of the new object is initialized from the virtual baseclass
		 of the incoming arg.  */
	      tree init_arg = build_unary_op (ADDR_EXPR, TREE_CHAIN (first_arg), 0);
	      for (vbases = CLASSTYPE_VBASECLASSES (t);
		   vbases; vbases = TREE_CHAIN (vbases))
		{
		  if (BINFO_BASEINIT_MARKED (vbases) == 0)
		    {
		      member = convert_pointer_to (vbases, init_arg);
		      if (member == init_arg)
			member = TREE_CHAIN (first_arg);
		      else
			TREE_TYPE (member) = build_reference_type (BINFO_TYPE (vbases));
		      vbase_init_list = tree_cons (convert_from_reference (member),
						   vbases, vbase_init_list);
		      SET_BINFO_BASEINIT_MARKED (vbases);
		    }
		}
	    }
	  expand_start_cond (first_arg, 0);
	  expand_aggr_vbase_init (t_binfo, C_C_D, current_class_decl,
				  vbase_init_list);
	  expand_expr_stmt (build_vbase_vtables_init (t_binfo, t_binfo,
						      C_C_D, current_class_decl, 1));
	  expand_end_cond ();
	}
    }
  current_base_init_list = NULL_TREE;

  /* Now, perform default initialization of all base classes which
     have not yet been initialized, and unmark baseclasses which
     have been initialized.  */
  for (i = 0; i < n_baseclasses; i++)
    {
      tree base = current_class_decl;
      tree base_binfo = TREE_VEC_ELT (binfos, i);

      if (TYPE_NEEDS_CONSTRUCTING (BINFO_TYPE (base_binfo)))
	{
	  if (! TREE_VIA_VIRTUAL (base_binfo)
	      && ! BINFO_BASEINIT_MARKED (base_binfo))
	    {
	      tree ref;

	      if (BINFO_OFFSET_ZEROP (base_binfo))
		base = build1 (NOP_EXPR,
			       TYPE_POINTER_TO (BINFO_TYPE (base_binfo)),
			       current_class_decl);
	      else
		base = build (PLUS_EXPR,
			      TYPE_POINTER_TO (BINFO_TYPE (base_binfo)),
			      current_class_decl, BINFO_OFFSET (base_binfo));

	      ref = build_indirect_ref (base, 0);
	      expand_aggr_init_1 (t_binfo, 0, ref, NULL_TREE,
				  BINFO_OFFSET_ZEROP (base_binfo),
				  LOOKUP_PROTECTED_OK|LOOKUP_COMPLAIN);
	      if (flag_handle_exceptions == 2
		  && TYPE_NEEDS_DESTRUCTOR (BINFO_TYPE (base_binfo)))
		{
		  cplus_expand_start_try (1);
		  push_exception_cleanup (base);
		}
	    }
	}
      CLEAR_BINFO_BASEINIT_MARKED (base_binfo);
    }
  for (vbases = CLASSTYPE_VBASECLASSES (t); vbases; vbases = TREE_CHAIN (vbases))
    CLEAR_BINFO_BASEINIT_MARKED (vbases);

  /* Initialize all the virtual function table fields that
     do not come from virtual base classes.  */
  init_vfields (t, t, 0);

  if (CLASSTYPE_NEEDS_VIRTUAL_REINIT (t))
    expand_expr_stmt (build_virtual_init (TYPE_BINFO (t), t,
					  current_class_decl));

  /* Members we through expand_member_init.  We initialize all the members
     needing initialization that did not get it so far.  */
  for (; current_member_init_list;
       current_member_init_list = TREE_CHAIN (current_member_init_list))
    {
      tree name = TREE_PURPOSE (current_member_init_list);
      tree init = TREE_VALUE (current_member_init_list);
      tree field = (TREE_CODE (name) == COMPONENT_REF
		    ? TREE_OPERAND (name, 1) : IDENTIFIER_CLASS_VALUE (name));
      tree type;
      
      /* If one member shadows another, get the outermost one.  */
      if (TREE_CODE (field) == TREE_LIST)
	{
	  field = TREE_VALUE (field);
	  if (decl_type_context (field) != current_class_type)
	    error ("field `%s' not in immediate context");
	}

      type = TREE_TYPE (field);

      if (TREE_STATIC (field))
	{
	  error_with_aggr_type (DECL_FIELD_CONTEXT (field),
				"field `%s::%s' is static; only point of initialization is its declaration", IDENTIFIER_POINTER (name));
	  continue;
	}

      if (DECL_NAME (field))
	{
	  if (TREE_HAS_CONSTRUCTOR (field))
	    error ("multiple initializations given for member `%s'",
		   IDENTIFIER_POINTER (DECL_NAME (field)));
	}

      /* Mark this node as having been initialized.  */
      TREE_HAS_CONSTRUCTOR (field) = 1;
      if (DECL_FIELD_CONTEXT (field) != t)
	fields_to_unmark = tree_cons (NULL_TREE, field, fields_to_unmark);

      if (TREE_CODE (name) == COMPONENT_REF)
	{
	  /* Initialization of anonymous union.  */
	  expand_assignment (name, init, 0, 0);
	  continue;
	}
      decl = build_component_ref (C_C_D, name, 0, 1);

      if (TYPE_NEEDS_CONSTRUCTING (type))
	{
	  if (TREE_CODE (type) == ARRAY_TYPE
	      && init != NULL_TREE
	      && TREE_CHAIN (init) == NULL_TREE
	      && TREE_CODE (TREE_TYPE (TREE_VALUE (init))) == ARRAY_TYPE)
	    {
	      /* Initialization of one array from another.  */
	      expand_vec_init (TREE_OPERAND (decl, 1), decl,
			       array_type_nelts (type), TREE_VALUE (init), 1);
	    }
	  else
	    expand_aggr_init (decl, init, 0);
	}
      else
	{
	  if (init == NULL_TREE)
	    {
	      error ("types without constructors must have complete initializers");
	      init = error_mark_node;
	    }
	  else if (TREE_CHAIN (init))
	    {
	      warning ("initializer list treated as compound expression");
	      init = build_compound_expr (init);
	    }
	  else
	    init = TREE_VALUE (init);

	  expand_expr_stmt (build_modify_expr (decl, INIT_EXPR, init));
	}
      if (flag_handle_exceptions == 2 && TYPE_NEEDS_DESTRUCTOR (type))
	{
	  cplus_expand_start_try (1);
	  push_exception_cleanup (build_unary_op (ADDR_EXPR, decl, 0));
	}
    }

  for (member = TYPE_FIELDS (t); member; member = TREE_CHAIN (member))
    {
      /* All we care about is this unique member.  It contains
	 all the information we need to know, and that right early.  */
      tree type = TREE_TYPE (member);
      tree init = TREE_HAS_CONSTRUCTOR (member)
	? error_mark_node : DECL_INITIAL (member);

      /* Unmark this field.  If it is from an anonymous union,
         then unmark the field recursively.  */
      TREE_HAS_CONSTRUCTOR (member) = 0;
      if (TREE_ANON_UNION_ELEM (member))
	emit_base_init (TREE_TYPE (member), 1);

      /* Member had explicit initializer.  */
      if (init == error_mark_node)
	continue;

      if (TREE_CODE (member) != FIELD_DECL)
	continue;

      if (type == error_mark_node)
	continue;

      if (TYPE_NEEDS_CONSTRUCTING (type))
	{
	  if (init)
	    init = build_tree_list (NULL_TREE, init);
	  decl = build_component_ref (C_C_D, DECL_NAME (member), 0, 0);
	  expand_aggr_init (decl, init, 0);
	}
      else
	{
	  if (init)
	    {
	      decl = build_component_ref (C_C_D, DECL_NAME (member), 0, 0);
	      expand_expr_stmt (build_modify_expr (decl, INIT_EXPR, init));
	    }
	  else if (TREE_CODE (TREE_TYPE (member)) == REFERENCE_TYPE)
	    warning ("uninitialized reference member `%s'",
		     IDENTIFIER_POINTER (DECL_NAME (member)));
	}
      if (flag_handle_exceptions == 2 && TYPE_NEEDS_DESTRUCTOR (type))
	{
	  cplus_expand_start_try (1);
	  push_exception_cleanup (build_unary_op (ADDR_EXPR, decl, 0));
	}
    }
  /* Unmark fields which are initialized for the base class.  */
  while (fields_to_unmark)
    {
      TREE_HAS_CONSTRUCTOR (TREE_VALUE (fields_to_unmark)) = 0;
      fields_to_unmark = TREE_CHAIN (fields_to_unmark);
    }

  /* It is possible for the initializers to need cleanups.
     Expand those cleanups now that all the initialization
     has been done.  */
  expand_cleanups_to (NULL_TREE);

  if (! immediately)
    {
      extern rtx base_init_insns;

      do_pending_stack_adjust ();
      my_friendly_assert (base_init_insns == 0, 207);
      base_init_insns = get_insns ();
      end_sequence ();
    }

  /* All the implicit try blocks we built up will be zapped
     when we come to a real binding contour boundary.  */
}

/* Check that all fields are properly initialized after
   an assignment to `this'.  */
void
check_base_init (t)
     tree t;
{
  tree member;
  for (member = TYPE_FIELDS (t); member; member = TREE_CHAIN (member))
    if (DECL_NAME (member) && TREE_USED (member))
      error ("field `%s' used before initialized (after assignment to `this')",
	     IDENTIFIER_POINTER (DECL_NAME (member)));
}

/* This code sets up the virtual function tables appropriate for
   the pointer DECL.  It is a one-ply initialization.

   BINFO is the exact type that DECL is supposed to be.  In
   multiple inheritance, this might mean "C's A" if C : A, B.  */
tree
build_virtual_init (main_binfo, binfo, decl)
     tree main_binfo, binfo;
     tree decl;
{
  tree type;
  tree vtbl, vtbl_ptr;
  tree vtype;

  if (TREE_CODE (binfo) == TREE_VEC)
    type = BINFO_TYPE (binfo);
  else if (TREE_CODE (binfo) == RECORD_TYPE)
    {
      type = binfo;
      binfo = TYPE_BINFO (type);
    }
  else
    my_friendly_abort (46);

  vtype = DECL_CONTEXT (CLASSTYPE_VFIELD (type));
#if 0
  /* This code suggests that it's time to rewrite how we handle
     replicated baseclasses in G++.  */
  if (get_base_distance (vtype, TREE_TYPE (TREE_TYPE (decl)),
			 0, (tree *) 0) == -2)
    {
      tree binfos = TYPE_BINFO_BASETYPES (TREE_TYPE (TREE_TYPE (decl)));
      int i, n_baselinks = binfos ? TREE_VEC_LENGTH (binfos) : 0;
      tree result = NULL_TREE;

      for (i = n_baselinks-1; i >= 0; i--)
	{
	  tree base_binfo = TREE_VEC_ELT (binfos, i);
	  tree this_decl;

	  if (get_base_distance (vtype, BINFO_TYPE (base_binfo), 0, 0) == -1)
	    continue;

	  if (TREE_VIA_VIRTUAL (base_binfo))
	    this_decl = build_vbase_pointer (build_indirect_ref (decl), BINFO_TYPE (base_binfo));
	  else if (BINFO_OFFSET_ZEROP (base_binfo))
	    this_decl = build1 (NOP_EXPR, TYPE_POINTER_TO (BINFO_TYPE (base_binfo)),
				decl);
	  else
	    this_decl = build (PLUS_EXPR, TYPE_POINTER_TO (BINFO_TYPE (base_binfo)),
			       decl, BINFO_OFFSET (base_binfo));
	  result = tree_cons (NULL_TREE, build_virtual_init (main_binfo, base_binfo, this_decl), result);
	}
      return build_compound_expr (result);
    }
#endif

    {
#if 1
#if 1
      vtbl = BINFO_VTABLE (binfo_value (DECL_FIELD_CONTEXT (CLASSTYPE_VFIELD (type)), binfo));
#else
      /* The below does not work when we have to step through the
	 vfield, on our way down to the most base class for the
	 vfield. */
      vtbl = BINFO_VTABLE (binfo_value (DECL_FIELD_CONTEXT (CLASSTYPE_VFIELD (type)),
					BINFO_TYPE (main_binfo)));
#endif
#else
      my_friendly_assert (BINFO_TYPE (main_binfo) == BINFO_TYPE (binfo), 208);
      vtbl = BINFO_VTABLE (main_binfo);
#endif /* 1 */
      assemble_external (vtbl);
      TREE_USED (vtbl) = 1;
      vtbl = build1 (ADDR_EXPR, TYPE_POINTER_TO (TREE_TYPE (vtbl)), vtbl);
    }
  decl = convert_pointer_to (vtype, decl);
  vtbl_ptr = build_vfield_ref (build_indirect_ref (decl, 0), vtype);
  if (vtbl_ptr == error_mark_node)
    return error_mark_node;

  /* Have to convert VTBL since array sizes may be different.  */
  return build_modify_expr (vtbl_ptr, NOP_EXPR,
			    convert (TREE_TYPE (vtbl_ptr), vtbl));
}

/* Subroutine of `expand_aggr_vbase_init'.
   BINFO is the binfo of the type that is being initialized.
   INIT_LIST is the list of initializers for the virtual baseclass.  */
static void
expand_aggr_vbase_init_1 (binfo, exp, addr, init_list)
     tree binfo, exp, addr, init_list;
{
  tree init = value_member (BINFO_TYPE (binfo), init_list);
  tree ref = build_indirect_ref (addr, 0);
  if (init)
    init = TREE_PURPOSE (init);
  /* Call constructors, but don't set up vtables.  */
  expand_aggr_init_1 (binfo, exp, ref, init, 0,
		      LOOKUP_PROTECTED_OK|LOOKUP_COMPLAIN|LOOKUP_SPECULATIVELY);
  CLEAR_BINFO_VBASE_INIT_MARKED (binfo);
}

/* Initialize this object's virtual base class pointers.  This must be
   done only at the top-level of the object being constructed.

   INIT_LIST is list of initialization for constructor to perform.  */
static void
expand_aggr_vbase_init (binfo, exp, addr, init_list)
     tree binfo;
     tree exp;
     tree addr;
     tree init_list;
{
  tree type = BINFO_TYPE (binfo);

  if (TYPE_USES_VIRTUAL_BASECLASSES (type))
    {
      tree result = init_vbase_pointers (type, addr);
      tree vbases;

      if (result)
	expand_expr_stmt (build_compound_expr (result));

      /* Mark everything as having an initializer
	 (either explicit or default).  */
      for (vbases = CLASSTYPE_VBASECLASSES (type);
	   vbases; vbases = TREE_CHAIN (vbases))
	SET_BINFO_VBASE_INIT_MARKED (vbases);

      /* First, initialize baseclasses which could be baseclasses
	 for other virtual baseclasses.  */
      for (vbases = CLASSTYPE_VBASECLASSES (type);
	   vbases; vbases = TREE_CHAIN (vbases))
	/* Don't initialize twice.  */
	if (BINFO_VBASE_INIT_MARKED (vbases))
	  {
	    tree tmp = result;

	    while (BINFO_TYPE (vbases) != BINFO_TYPE (TREE_PURPOSE (tmp)))
	      tmp = TREE_CHAIN (tmp);
	    expand_aggr_vbase_init_1 (vbases, exp,
				      TREE_OPERAND (TREE_VALUE (tmp), 0),
				      init_list);
	  }

      /* Now initialize the baseclasses which don't have virtual baseclasses.  */
      for (; result; result = TREE_CHAIN (result))
	/* Don't initialize twice.  */
	if (BINFO_VBASE_INIT_MARKED (TREE_PURPOSE (result)))
	  {
	    my_friendly_abort (47);
	    expand_aggr_vbase_init_1 (TREE_PURPOSE (result), exp,
				      TREE_OPERAND (TREE_VALUE (result), 0),
				      init_list);
	  }
    }
}

/* Subroutine to perform parser actions for member initialization.
   S_ID is the scoped identifier.
   NAME is the name of the member.
   INIT is the initializer, or `void_type_node' if none.  */
void
do_member_init (s_id, name, init)
     tree s_id, name, init;
{
  tree binfo, base;

  if (current_class_type == NULL_TREE
      || ! is_aggr_typedef (s_id, 1))
    return;
  binfo = get_binfo (IDENTIFIER_TYPE_VALUE (s_id),
			  current_class_type, 1);
  if (binfo == error_mark_node)
    return;
  if (binfo == 0)
    {
      error_not_base_type (IDENTIFIER_TYPE_VALUE (s_id), current_class_type);
      return;
    }

  base = convert_pointer_to (binfo, current_class_decl);
  expand_member_init (build_indirect_ref (base, NULL_PTR), name, init);
}

/* Function to give error message if member initialization specification
   is erroneous.  FIELD is the member we decided to initialize.
   TYPE is the type for which the initialization is being performed.
   FIELD must be a member of TYPE, or the base type from which FIELD
   comes must not need a constructor.
   
   MEMBER_NAME is the name of the member.  */

static int
member_init_ok_or_else (field, type, member_name)
     tree field;
     tree type;
     char *member_name;
{
  if (field == error_mark_node) return 0;
  if (field == NULL_TREE)
    {
      error_with_aggr_type (type, "class `%s' does not have any field named `%s'",
			    member_name);
      return 0;
    }
  if (DECL_CONTEXT (field) != type
      && TYPE_NEEDS_CONSTRUCTOR (DECL_CONTEXT (field)))
    {
      error ("member `%s' comes from base class needing constructor", member_name);
      return 0;
    }
  return 1;
}

/* If NAME is a viable field name for the aggregate DECL,
   and PARMS is a viable parameter list, then expand an _EXPR
   which describes this initialization.

   Note that we do not need to chase through the class's base classes
   to look for NAME, because if it's in that list, it will be handled
   by the constructor for that base class.

   We do not yet have a fixed-point finder to instantiate types
   being fed to overloaded constructors.  If there is a unique
   constructor, then argument types can be got from that one.

   If INIT is non-NULL, then it the initialization should
   be placed in `current_base_init_list', where it will be processed
   by `emit_base_init'.  */
void
expand_member_init (exp, name, init)
     tree exp, name, init;
{
  extern tree ptr_type_node;	/* should be in tree.h */

  tree basetype = NULL_TREE, field;
  tree parm;
  tree rval, type;
  tree actual_name;

  if (exp == NULL_TREE)
    return;			/* complain about this later */

  type = TYPE_MAIN_VARIANT (TREE_TYPE (exp));

  if (name == NULL_TREE && IS_AGGR_TYPE (type))
    switch (CLASSTYPE_N_BASECLASSES (type))
      {
      case 0:
	error ("base class initializer specified, but no base class to initialize");
	return;
      case 1:
	basetype = TYPE_BINFO_BASETYPE (type, 0);
	break;
      default:
	error ("initializer for unnamed base class ambiguous");
	error_with_aggr_type (type, "(type `%s' uses multiple inheritance)");
	return;
      }

  if (init)
    {
      /* The grammar should not allow fields which have names
	 that are TYPENAMEs.  Therefore, if the field has
	 a non-NULL TREE_TYPE, we may assume that this is an
	 attempt to initialize a base class member of the current
	 type.  Otherwise, it is an attempt to initialize a
	 member field.  */

      if (init == void_type_node)
	init = NULL_TREE;

      if (name == NULL_TREE || IDENTIFIER_HAS_TYPE_VALUE (name))
	{
	  tree base_init;

	  if (name == NULL_TREE)
	    if (basetype)
	      name = TYPE_IDENTIFIER (basetype);
	    else
	      {
		error ("no base class to initialize");
		return;
	      }
	  else
	    {
	      basetype = IDENTIFIER_TYPE_VALUE (name);
	      if (basetype != type
		  && ! binfo_member (basetype, TYPE_BINFO (type))
		  && ! binfo_member (basetype, CLASSTYPE_VBASECLASSES (type)))
		{
		  if (IDENTIFIER_CLASS_VALUE (name))
		    goto try_member;
		  if (TYPE_USES_VIRTUAL_BASECLASSES (type))
		    error ("type `%s' is not an immediate or virtual basetype for `%s'",
			   IDENTIFIER_POINTER (name),
			   TYPE_NAME_STRING (type));
		  else
		    error ("type `%s' is not an immediate basetype for `%s'",
			   IDENTIFIER_POINTER (name),
			   TYPE_NAME_STRING (type));
		  return;
		}
	    }

	  if (purpose_member (name, current_base_init_list))
	    {
	      error ("base class `%s' already initialized",
		     IDENTIFIER_POINTER (name));
	      return;
	    }

	  base_init = build_tree_list (name, init);
	  TREE_TYPE (base_init) = basetype;
	  current_base_init_list = chainon (current_base_init_list, base_init);
	}
      else
	{
	  tree member_init;

	try_member:
	  field = lookup_field (type, name, 1, 0);

	  if (! member_init_ok_or_else (field, type, IDENTIFIER_POINTER (name)))
	    return;

	  if (purpose_member (name, current_member_init_list))
	    {
	      error ("field `%s' already initialized", IDENTIFIER_POINTER (name));
	      return;
	    }

	  member_init = build_tree_list (name, init);
	  TREE_TYPE (member_init) = TREE_TYPE (field);
	  current_member_init_list = chainon (current_member_init_list, member_init);
	}
      return;
    }
  else if (name == NULL_TREE)
    {
      compiler_error ("expand_member_init: name == NULL_TREE");
      return;
    }

  basetype = type;
  field = lookup_field (basetype, name, 0, 0);

  if (! member_init_ok_or_else (field, basetype, IDENTIFIER_POINTER (name)))
    return;

  /* now see if there is a constructor for this type
     which will take these args. */

  if (TYPE_HAS_CONSTRUCTOR (TREE_TYPE (field)))
    {
      tree parmtypes, fndecl;

      if (TREE_CODE (exp) == VAR_DECL || TREE_CODE (exp) == PARM_DECL)
	{
	  /* just know that we've seen something for this node */
	  DECL_INITIAL (exp) = error_mark_node;
	  TREE_USED (exp) = 1;
	}
      type = TYPE_MAIN_VARIANT (TREE_TYPE (field));
      actual_name = TYPE_IDENTIFIER (type);
      parm = build_component_ref (exp, name, 0, 0);

      /* Now get to the constructor.  */
      fndecl = TREE_VEC_ELT (CLASSTYPE_METHOD_VEC (type), 0);
      /* Get past destructor, if any.  */
      if (TYPE_HAS_DESTRUCTOR (type))
	fndecl = DECL_CHAIN (fndecl);

      if (fndecl)
	my_friendly_assert (TREE_CODE (fndecl) == FUNCTION_DECL, 209);

      /* If the field is unique, we can use the parameter
	 types to guide possible type instantiation.  */
      if (DECL_CHAIN (fndecl) == NULL_TREE)
	{
	  /* There was a confusion here between
	     FIELD and FNDECL.  The following code
	     should be correct, but abort is here
	     to make sure.  */
	  my_friendly_abort (48);
	  parmtypes = FUNCTION_ARG_CHAIN (fndecl);
	}
      else
	{
	  parmtypes = NULL_TREE;
	  fndecl = NULL_TREE;
	}

      init = convert_arguments (parm, parmtypes, NULL_TREE, fndecl, LOOKUP_NORMAL);
      if (init == NULL_TREE || TREE_TYPE (init) != error_mark_node)
	rval = build_method_call (NULL_TREE, actual_name, init, NULL_TREE, LOOKUP_NORMAL);
      else
	return;

      if (rval != error_mark_node)
	{
	  /* Now, fill in the first parm with our guy */
	  TREE_VALUE (TREE_OPERAND (rval, 1))
	    = build_unary_op (ADDR_EXPR, parm, 0);
	  TREE_TYPE (rval) = ptr_type_node;
	  TREE_SIDE_EFFECTS (rval) = 1;
	}
    }
  else if (TYPE_NEEDS_CONSTRUCTING (TREE_TYPE (field)))
    {
      parm = build_component_ref (exp, name, 0, 0);
      expand_aggr_init (parm, NULL_TREE, 0);
      rval = error_mark_node;
    }

  /* Now initialize the member.  It does not have to
     be of aggregate type to receive initialization.  */
  if (rval != error_mark_node)
    expand_expr_stmt (rval);
}

/* This is like `expand_member_init', only it stores one aggregate
   value into another.

   INIT comes in two flavors: it is either a value which
   is to be stored in EXP, or it is a parameter list
   to go to a constructor, which will operate on EXP.
   If `init' is a CONSTRUCTOR, then we emit a warning message,
   explaining that such initializations are illegal.

   ALIAS_THIS is nonzero iff we are initializing something which is
   essentially an alias for C_C_D.  In this case, the base constructor
   may move it on us, and we must keep track of such deviations.

   If INIT resolves to a CALL_EXPR which happens to return
   something of the type we are looking for, then we know
   that we can safely use that call to perform the
   initialization.

   The virtual function table pointer cannot be set up here, because
   we do not really know its type.

   Virtual baseclass pointers are also set up here.

   This never calls operator=().

   When initializing, nothing is CONST.

   A default copy constructor may have to be used to perform the
   initialization.

   A constructor or a conversion operator may have to be used to
   perform the initialization, but not both, as it would be ambiguous.
   */

void
expand_aggr_init (exp, init, alias_this)
     tree exp, init;
     int alias_this;
{
  tree type = TREE_TYPE (exp);
  int was_const = TREE_READONLY (exp);

  if (init == error_mark_node)
    return;

  TREE_READONLY (exp) = 0;

  if (TREE_CODE (type) == ARRAY_TYPE)
    {
      /* Must arrange to initialize each element of EXP
	 from elements of INIT.  */
      int was_const_elts = TYPE_READONLY (TREE_TYPE (type));
      tree itype = init ? TREE_TYPE (init) : NULL_TREE;
      if (was_const_elts)
	{
	  tree atype = build_cplus_array_type (TYPE_MAIN_VARIANT (TREE_TYPE (type)),
					       TYPE_DOMAIN (type));
	  if (init && (TREE_TYPE (exp) == TREE_TYPE (init)))
	    TREE_TYPE (init) = atype;
	  TREE_TYPE (exp) = atype;
	}
      expand_vec_init (exp, exp, array_type_nelts (type), init,
		       init && comptypes (TREE_TYPE (init), TREE_TYPE (exp), 1));
      TREE_READONLY (exp) = was_const;
      TREE_TYPE (exp) = type;
      if (init) TREE_TYPE (init) = itype;
      return;
    }

  if (TREE_CODE (exp) == VAR_DECL || TREE_CODE (exp) == PARM_DECL)
    /* just know that we've seen something for this node */
    TREE_USED (exp) = 1;

  /* If initializing from a GNU C CONSTRUCTOR, consider the elts in the
     constructor as parameters to an implicit GNU C++ constructor.  */
  if (init && TREE_CODE (init) == CONSTRUCTOR
      && TYPE_HAS_CONSTRUCTOR (type)
      && TREE_TYPE (init) == type)
    init = CONSTRUCTOR_ELTS (init);
  expand_aggr_init_1 (TYPE_BINFO (type), exp, exp,
		      init, alias_this, LOOKUP_NORMAL);
  TREE_READONLY (exp) = was_const;
}

static void
expand_default_init (binfo, true_exp, exp, type, init, alias_this, flags)
     tree binfo;
     tree true_exp, exp;
     tree type;
     tree init;
     int alias_this;
     int flags;
{
  /* It fails because there may not be a constructor which takes
     its own type as the first (or only parameter), but which does
     take other types via a conversion.  So, if the thing initializing
     the expression is a unit element of type X, first try X(X&),
     followed by initialization by X.  If neither of these work
     out, then look hard.  */
  tree rval;
  tree parms;
  int xxref_init_possible;

  if (init == NULL_TREE || TREE_CODE (init) == TREE_LIST)
    {
      parms = init;
      if (parms) init = TREE_VALUE (parms);
    }
  else if (TREE_CODE (init) == INDIRECT_REF && TREE_HAS_CONSTRUCTOR (init))
    {
      rval = convert_for_initialization (exp, type, init, 0, 0, 0, 0);
      expand_expr_stmt (rval);
      return;
    }
  else parms = build_tree_list (NULL_TREE, init);

  if (TYPE_HAS_INIT_REF (type)
      || init == NULL_TREE
      || TREE_CHAIN (parms) != NULL_TREE)
    xxref_init_possible = 0;
  else
    {
      xxref_init_possible = LOOKUP_SPECULATIVELY;
      flags &= ~LOOKUP_COMPLAIN;
    }

  if (TYPE_USES_VIRTUAL_BASECLASSES (type))
    {
      if (true_exp == exp)
	parms = tree_cons (NULL_TREE, integer_one_node, parms);
      else
	parms = tree_cons (NULL_TREE, integer_zero_node, parms);
      flags |= LOOKUP_HAS_IN_CHARGE;
    }

  rval = build_method_call (exp, constructor_name (type),
			    parms, binfo, flags|xxref_init_possible);
  if (rval == NULL_TREE && xxref_init_possible)
    {
      /* It is an error to implement a default copy constructor if
	 (see ARM 12.8 for details) ... one case being if another
	 copy constructor already exists. */
      tree init_type = TREE_TYPE (init);
      if (TREE_CODE (init_type) == REFERENCE_TYPE)
	init_type = TREE_TYPE (init_type);
      if (TYPE_MAIN_VARIANT (init_type) == TYPE_MAIN_VARIANT (type)
	  || (IS_AGGR_TYPE (init_type)
	      && UNIQUELY_DERIVED_FROM_P (type, init_type)))
	{
	  if (type == BINFO_TYPE (binfo)
	      && TYPE_USES_VIRTUAL_BASECLASSES (type))
	    {
	      tree addr = build_unary_op (ADDR_EXPR, exp, 0);
	      expand_aggr_vbase_init (binfo, exp, addr, NULL_TREE);

	      expand_expr_stmt (build_vbase_vtables_init (binfo, binfo,
							  exp, addr, 1));
	    }
	  expand_expr_stmt (build_modify_expr (exp, INIT_EXPR, init));
	  return;
	}
      else
	rval = build_method_call (exp, constructor_name (type), parms,
				  binfo, flags);
    }

  /* Private, protected, or otherwise unavailable.  */
  if (rval == error_mark_node && (flags&LOOKUP_COMPLAIN))
    error_with_aggr_type (binfo, "in base initialization for class `%s'");
  /* A valid initialization using constructor.  */
  else if (rval != error_mark_node && rval != NULL_TREE)
    {
      /* p. 222: if the base class assigns to `this', then that
	 value is used in the derived class.  */
      if ((flag_this_is_variable & 1) && alias_this)
	{
	  TREE_TYPE (rval) = TREE_TYPE (current_class_decl);
	  expand_assignment (current_class_decl, rval, 0, 0);
	}
      else
	expand_expr_stmt (rval);
    }
  else if (parms && TREE_CHAIN (parms) == NULL_TREE)
    {
      /* If we are initializing one aggregate value
	 from another, and though there are constructors,
	 and none accept the initializer, just do a bitwise
	 copy.

	 The above sounds wrong, ``If a class has any copy
	 constructor defined, the default copy constructor will
	 not be generated.'' 12.8 Copying Class Objects  (mrs)

	 @@ This should reject initializer which a constructor
	 @@ rejected on visibility gounds, but there is
	 @@ no way right now to recognize that case with
	 @@ just `error_mark_node'.  */
      tree itype;
      init = TREE_VALUE (parms);
      itype = TREE_TYPE (init);
      if (TREE_CODE (itype) == REFERENCE_TYPE)
	{
	  init = convert_from_reference (init);
	  itype = TREE_TYPE (init);
	}
      itype = TYPE_MAIN_VARIANT (itype);

      /* This is currently how the default X(X&) constructor
	 is implemented.  */
      if (comptypes (TYPE_MAIN_VARIANT (type), itype, 0))
	{
#if 0
	  warning ("bitwise copy in initialization of type `%s'",
		   TYPE_NAME_STRING (type));
#endif
	  rval = build (INIT_EXPR, type, exp, init);
	  expand_expr_stmt (rval);
	}
      else
	{
	  error_with_aggr_type (binfo, "in base initialization for class `%s',");
	  error_with_aggr_type (type, "invalid initializer to constructor for type `%s'");
	  return;
	}
    }
  else
    {
      if (init == NULL_TREE)
	my_friendly_assert (parms == NULL_TREE, 210);
      if (parms == NULL_TREE && TREE_VIA_VIRTUAL (binfo))
	error_with_aggr_type (binfo, "virtual baseclass `%s' does not have default initializer");
      else
	{
	  error_with_aggr_type (binfo, "in base initialization for class `%s',");
	  /* This will make an error message for us.  */
	  build_method_call (exp, constructor_name (type), parms, binfo,
			     (TYPE_USES_VIRTUAL_BASECLASSES (type)
			      ? LOOKUP_NORMAL|LOOKUP_HAS_IN_CHARGE
			      : LOOKUP_NORMAL));
	}
      return;
    }
  /* Constructor has been called, but vtables may be for TYPE
     rather than for FOR_TYPE.  */
}

/* This function is responsible for initializing EXP with INIT
   (if any).

   BINFO is the binfo of the type for who we are performing the
   initialization.  For example, if W is a virtual base class of A and B,
   and C : A, B.
   If we are initializing B, then W must contain B's W vtable, whereas
   were we initializing C, W must contain C's W vtable.

   TRUE_EXP is nonzero if it is the true expression being initialized.
   In this case, it may be EXP, or may just contain EXP.  The reason we
   need this is because if EXP is a base element of TRUE_EXP, we
   don't necessarily know by looking at EXP where its virtual
   baseclass fields should really be pointing.  But we do know
   from TRUE_EXP.  In constructors, we don't know anything about
   the value being initialized.

   ALIAS_THIS serves the same purpose it serves for expand_aggr_init.

   FLAGS is just passes to `build_method_call'.  See that function for
   its description.  */

static void
expand_aggr_init_1 (binfo, true_exp, exp, init, alias_this, flags)
     tree binfo;
     tree true_exp, exp;
     tree init;
     int alias_this;
     int flags;
{
  tree type = TREE_TYPE (exp);
  tree init_type = NULL_TREE;
  tree rval;

  my_friendly_assert (init != error_mark_node && type != error_mark_node, 211);

  /* Use a function returning the desired type to initialize EXP for us.
     If the function is a constructor, and its first argument is
     NULL_TREE, know that it was meant for us--just slide exp on
     in and expand the constructor.  Constructors now come
     as TARGET_EXPRs.  */
  if (init)
    {
      tree init_list = NULL_TREE;

      if (TREE_CODE (init) == TREE_LIST)
	{
	  init_list = init;
	  if (TREE_CHAIN (init) == NULL_TREE)
	    init = TREE_VALUE (init);
	}

      init_type = TREE_TYPE (init);

      if (TREE_CODE (init) != TREE_LIST)
	{
	  if (TREE_CODE (init_type) == ERROR_MARK)
	    return;

#if 0
	  /* These lines are found troublesome 5/11/89.  */
	  if (TREE_CODE (init_type) == REFERENCE_TYPE)
	    init_type = TREE_TYPE (init_type);
#endif

	  /* This happens when we use C++'s functional cast notation.
	     If the types match, then just use the TARGET_EXPR
	     directly.  Otherwise, we need to create the initializer
	     separately from the object being initialized.  */
	  if (TREE_CODE (init) == TARGET_EXPR)
	    {
	      if (init_type == type)
		{
		  if (TREE_CODE (exp) == VAR_DECL
		      || TREE_CODE (exp) == RESULT_DECL)
		    /* Unify the initialization targets.  */
		    DECL_RTL (TREE_OPERAND (init, 0)) = DECL_RTL (exp);
		  else
		    DECL_RTL (TREE_OPERAND (init, 0)) = expand_expr (exp, NULL_RTX, 0, 0);

		  expand_expr_stmt (init);
		  return;
		}
	      else
		{
		  init = TREE_OPERAND (init, 1);
		  init = build (CALL_EXPR, init_type,
				TREE_OPERAND (init, 0), TREE_OPERAND (init, 1), 0);
		  TREE_SIDE_EFFECTS (init) = 1;
#if 0
		  TREE_RAISES (init) = ??
#endif
		    if (init_list)
		      TREE_VALUE (init_list) = init;
		}
	    }

	  if (init_type == type && TREE_CODE (init) == CALL_EXPR
#if 0
	      /* It is legal to directly initialize from a CALL_EXPR
		 without going through X(X&), apparently.  */
	      && ! TYPE_GETS_INIT_REF (type)
#endif
	      )
	    {
	      /* A CALL_EXPR is a legitimate form of initialization, so
		 we should not print this warning message.  */
#if 0
	      /* Should have gone away due to 5/11/89 change.  */
	      if (TREE_CODE (TREE_TYPE (init)) == REFERENCE_TYPE)
		init = convert_from_reference (init);
#endif
	      expand_assignment (exp, init, 0, 0);
	      if (exp == DECL_RESULT (current_function_decl))
		{
		  /* Failing this assertion means that the return value
		     from receives multiple initializations.  */
		  my_friendly_assert (DECL_INITIAL (exp) == NULL_TREE
				      || DECL_INITIAL (exp) == error_mark_node,
				      212);
		  DECL_INITIAL (exp) = init;
		}
	      return;
	    }
	  else if (init_type == type
		   && TREE_CODE (init) == COND_EXPR)
	    {
	      /* Push value to be initialized into the cond, where possible.
	         Avoid spurious warning messages when initializing the
		 result of this function.  */
	      TREE_OPERAND (init, 1)
		= build_modify_expr (exp, INIT_EXPR, TREE_OPERAND (init, 1));
	      if (exp == DECL_RESULT (current_function_decl))
		DECL_INITIAL (exp) = NULL_TREE;
	      TREE_OPERAND (init, 2)
		= build_modify_expr (exp, INIT_EXPR, TREE_OPERAND (init, 2));
	      if (exp == DECL_RESULT (current_function_decl))
		DECL_INITIAL (exp) = init;
	      expand_expr (init, const0_rtx, VOIDmode, 0);
	      free_temp_slots ();
	      return;
	    }
	}

      /* We did not know what we were initializing before.  Now we do.  */
      if (TREE_CODE (init) == TARGET_EXPR)
	{
	  tree tmp = TREE_OPERAND (TREE_OPERAND (init, 1), 1);

	  if (TREE_CODE (TREE_VALUE (tmp)) == NOP_EXPR
	      && TREE_OPERAND (TREE_VALUE (tmp), 0) == integer_zero_node)
	    {
	      /* In order for this to work for RESULT_DECLs, if their
		 type has a constructor, then they must be BLKmode
		 so that they will be meaningfully addressable.  */
	      tree arg = build_unary_op (ADDR_EXPR, exp, 0);
	      init = TREE_OPERAND (init, 1);
	      init = build (CALL_EXPR, build_pointer_type (TREE_TYPE (init)),
			    TREE_OPERAND (init, 0), TREE_OPERAND (init, 1), 0);
	      TREE_SIDE_EFFECTS (init) = 1;
#if 0
	      TREE_RAISES (init) = ??
#endif
	      TREE_VALUE (TREE_OPERAND (init, 1))
		= convert_pointer_to (TREE_TYPE (TREE_TYPE (TREE_VALUE (tmp))), arg);

	      if (alias_this)
		{
		  expand_assignment (current_function_decl, init, 0, 0);
		  return;
		}
	      if (exp == DECL_RESULT (current_function_decl))
		{
		  if (DECL_INITIAL (DECL_RESULT (current_function_decl)))
		    fatal ("return value from function receives multiple initializations");
		  DECL_INITIAL (exp) = init;
		}
	      expand_expr_stmt (init);
	      return;
	    }
	}

      /* Handle this case: when calling a constructor: xyzzy foo(bar);
	 which really means:  xyzzy foo = bar; Ugh!

	 We can also be called with an initializer for an object
	 which has virtual functions, but no constructors.  In that
	 case, we perform the assignment first, then initialize
	 the virtual function table pointer fields.  */

      if (! TYPE_NEEDS_CONSTRUCTING (type))
	{
	  if (init_list && TREE_CHAIN (init_list))
	    {
	      warning ("initializer list being treated as compound expression");
	      init = convert (TREE_TYPE (exp), build_compound_expr (init_list));
	      if (init == error_mark_node)
		return;
	    }
	  if (TREE_CODE (exp) == VAR_DECL
	      && TREE_CODE (init) == CONSTRUCTOR
	      && TREE_HAS_CONSTRUCTOR (init)
	      && flag_pic == 0)
	    store_init_value (exp, init);
	  else
	    expand_assignment (exp, init, 0, 0);

	  if (TYPE_VIRTUAL_P (type))
	    expand_recursive_init (binfo, true_exp, exp, init, CLASSTYPE_BASE_INIT_LIST (type), alias_this);
	  return;
	}

      /* See whether we can go through a type conversion operator.
	 This wins over going through a non-existent constructor.  If
	 there is a constructor, it is ambiguous.  */
      if (TREE_CODE (init) != TREE_LIST)
	{
	  tree ttype = TREE_CODE (init_type) == REFERENCE_TYPE
	    ? TREE_TYPE (init_type) : init_type;

	  if (ttype != type && IS_AGGR_TYPE (ttype))
	    {
	      tree rval = build_type_conversion (CONVERT_EXPR, type, init, 0);

	      if (rval)
		{
		  /* See if there is a constructor for``type'' that takes a
		     ``ttype''-typed object. */
		  tree parms = build_tree_list (NULL_TREE, init);
		  tree as_cons = NULL_TREE;
		  if (TYPE_HAS_CONSTRUCTOR (type))
		    as_cons = build_method_call (exp, constructor_name (type),
						 parms, binfo,
						 LOOKUP_SPECULATIVELY|LOOKUP_NO_CONVERSION);
		  if (as_cons != NULL_TREE && as_cons != error_mark_node)
		    {
		      tree name = TYPE_NAME (type);
		      if (TREE_CODE (name) == TYPE_DECL)
			name = DECL_NAME (name);
		      /* ANSI C++ June 5 1992 WP 12.3.2.6.1 */
		      error ("ambiguity between conversion to `%s' and constructor",
			     IDENTIFIER_POINTER (name));
		    }
		  else
		    expand_assignment (exp, rval, 0, 0);
		  return;
		}
	    }
	}
    }

  /* Handle default copy constructors here, does not matter if there is
     a constructor or not.  */
  if (type == init_type && IS_AGGR_TYPE (type)
      && init && TREE_CODE (init) != TREE_LIST)
    expand_default_init (binfo, true_exp, exp, type, init, alias_this, flags);
  /* Not sure why this is here... */
  else if (TYPE_HAS_CONSTRUCTOR (type))
    expand_default_init (binfo, true_exp, exp, type, init, alias_this, flags);
  else if (TREE_CODE (type) == ARRAY_TYPE)
    {
      if (TYPE_NEEDS_CONSTRUCTING (TREE_TYPE (type)))
	expand_vec_init (exp, exp, array_type_nelts (type), init, 0);
      else if (TYPE_VIRTUAL_P (TREE_TYPE (type)))
	sorry ("arrays of objects with virtual functions but no constructors");
    }
  else
    expand_recursive_init (binfo, true_exp, exp, init,
			   CLASSTYPE_BASE_INIT_LIST (type), alias_this);
}

/* A pointer which holds the initializer.  First call to
   expand_aggr_init gets this value pointed to, and sets it to init_null.  */
static tree *init_ptr, init_null;

/* Subroutine of expand_recursive_init:

   ADDR is the address of the expression being initialized.
   INIT_LIST is the cons-list of initializations to be performed.
   ALIAS_THIS is its same, lovable self.  */
static void
expand_recursive_init_1 (binfo, true_exp, addr, init_list, alias_this)
     tree binfo, true_exp, addr;
     tree init_list;
     int alias_this;
{
  while (init_list)
    {
      if (TREE_PURPOSE (init_list))
	{
	  if (TREE_CODE (TREE_PURPOSE (init_list)) == FIELD_DECL)
	    {
	      tree member = TREE_PURPOSE (init_list);
	      tree subexp = build_indirect_ref (convert_pointer_to (TREE_VALUE (init_list), addr), 0);
	      tree member_base = build (COMPONENT_REF, TREE_TYPE (member), subexp, member);
	      if (IS_AGGR_TYPE (TREE_TYPE (member)))
		expand_aggr_init (member_base, DECL_INITIAL (member), 0);
	      else if (TREE_CODE (TREE_TYPE (member)) == ARRAY_TYPE
		       && TYPE_NEEDS_CONSTRUCTING (TREE_TYPE (member)))
		{
		  member_base = save_expr (default_conversion (member_base));
		  expand_vec_init (member, member_base,
				   array_type_nelts (TREE_TYPE (member)),
				   DECL_INITIAL (member), 0);
		}
	      else expand_expr_stmt (build_modify_expr (member_base, INIT_EXPR, DECL_INITIAL (member)));
	    }
	  else if (TREE_CODE (TREE_PURPOSE (init_list)) == TREE_LIST)
	    {
	      expand_recursive_init_1 (binfo, true_exp, addr, TREE_PURPOSE (init_list), alias_this);
	      expand_recursive_init_1 (binfo, true_exp, addr, TREE_VALUE (init_list), alias_this);
	    }
	  else if (TREE_CODE (TREE_PURPOSE (init_list)) == ERROR_MARK)
	    {
	      /* Only initialize the virtual function tables if we
		 are initializing the ultimate users of those vtables.  */
	      if (TREE_VALUE (init_list))
		{
		  /* We have to ensure that the second argment to
		     build_virtual_init is in binfo's hierarchy.  */
		  expand_expr_stmt (build_virtual_init (binfo,
							get_binfo (TREE_VALUE (init_list), binfo, 0),
							addr));
		  if (TREE_VALUE (init_list) == binfo
		      && TYPE_USES_VIRTUAL_BASECLASSES (BINFO_TYPE (binfo)))
		    expand_expr_stmt (build_vbase_vtables_init (binfo, binfo, true_exp, addr, 1));
		}
	    }
	  else my_friendly_abort (49);
	}
      else if (TREE_VALUE (init_list)
	       && TREE_CODE (TREE_VALUE (init_list)) == TREE_VEC)
	{
	  tree subexp = build_indirect_ref (convert_pointer_to (TREE_VALUE (init_list), addr), 0);
	  expand_aggr_init_1 (binfo, true_exp, subexp, *init_ptr,
			      alias_this && BINFO_OFFSET_ZEROP (TREE_VALUE (init_list)),
			      LOOKUP_PROTECTED_OK|LOOKUP_COMPLAIN);

	  /* INIT_PTR is used up.  */
	  init_ptr = &init_null;
	}
      else
	my_friendly_abort (50);
      init_list = TREE_CHAIN (init_list);
    }
}

/* Initialize EXP with INIT.  Type EXP does not have a constructor,
   but it has a baseclass with a constructor or a virtual function
   table which needs initializing.

   INIT_LIST is a cons-list describing what parts of EXP actually
   need to be initialized.  INIT is given to the *unique*, first
   constructor within INIT_LIST.  If there are multiple first
   constructors, such as with multiple inheritance, INIT must
   be zero or an ambiguity error is reported.

   ALIAS_THIS is passed from `expand_aggr_init'.  See comments
   there.  */

static void
expand_recursive_init (binfo, true_exp, exp, init, init_list, alias_this)
     tree binfo, true_exp, exp, init;
     tree init_list;
     int alias_this;
{
  tree *old_init_ptr = init_ptr;
  tree addr = build_unary_op (ADDR_EXPR, exp, 0);
  init_ptr = &init;

  if (true_exp == exp && TYPE_USES_VIRTUAL_BASECLASSES (BINFO_TYPE (binfo)))
    {
      expand_aggr_vbase_init (binfo, exp, addr, init_list);
      expand_expr_stmt (build_vbase_vtables_init (binfo, binfo, true_exp, addr, 1));
    }
  expand_recursive_init_1 (binfo, true_exp, addr, init_list, alias_this);

  if (*init_ptr)
    {
      tree type = TREE_TYPE (exp);

      if (TREE_CODE (type) == REFERENCE_TYPE)
	type = TREE_TYPE (type);
      if (IS_AGGR_TYPE (type))
	error_with_aggr_type (type, "unexpected argument to constructor `%s'");
      else
	error ("unexpected argument to constructor");
    }
  init_ptr = old_init_ptr;
}

/* Report an error if NAME is not the name of a user-defined,
   aggregate type.  If OR_ELSE is nonzero, give an error message.  */
int
is_aggr_typedef (name, or_else)
     tree name;
     int or_else;
{
  tree type;

  if (name == error_mark_node)
    return 0;

  if (! IDENTIFIER_HAS_TYPE_VALUE (name))
    {
      if (or_else)
	error ("`%s' fails to be an aggregate typedef",
	       IDENTIFIER_POINTER (name));
      return 0;
    }
  type = IDENTIFIER_TYPE_VALUE (name);
  if (! IS_AGGR_TYPE (type))
    {
      if (or_else)
	error ("type `%s' is of non-aggregate type",
	       IDENTIFIER_POINTER (name));
      return 0;
    }
  return 1;
}

/* This code could just as well go in `cp-class.c', but is placed here for
   modularity.  */

/* For an expression of the form CNAME :: NAME (PARMLIST), build
   the appropriate function call.  */
tree
build_member_call (cname, name, parmlist)
     tree cname, name, parmlist;
{
  tree type, t;
  tree method_name = name;
  int dtor = 0;
  int dont_use_this = 0;
  tree basetype_path, decl;

  if (TREE_CODE (method_name) == BIT_NOT_EXPR)
    {
      method_name = TREE_OPERAND (method_name, 0);
      dtor = 1;
    }

  if (TREE_CODE (cname) == SCOPE_REF)
    cname = resolve_scope_to_name (NULL_TREE, cname);

  if (cname == NULL_TREE || ! is_aggr_typedef (cname, 1))
    return error_mark_node;

  /* An operator we did not like.  */
  if (name == NULL_TREE)
    return error_mark_node;

  if (dtor)
    {
      if (! TYPE_HAS_DESTRUCTOR (IDENTIFIER_TYPE_VALUE (cname)))
	error ("type `%s' does not have a destructor",
	       IDENTIFIER_POINTER (cname));
      else
	error ("destructor specification error");
      return error_mark_node;
    }

  type = IDENTIFIER_TYPE_VALUE (cname);

  /* No object?  Then just fake one up, and let build_method_call
     figure out what to do.  */
  if (current_class_type == 0
      || get_base_distance (type, current_class_type, 0, &basetype_path) == -1)
    dont_use_this = 1;

  if (dont_use_this)
    {
      basetype_path = NULL_TREE;
      decl = build1 (NOP_EXPR, TYPE_POINTER_TO (type), error_mark_node);
    }
  else if (current_class_decl == 0)
    {
      dont_use_this = 1;
      decl = build1 (NOP_EXPR, TYPE_POINTER_TO (type), error_mark_node);
    }
  else
    {
      decl = current_class_decl;
      if (TREE_TYPE (decl) != type)
	decl = convert (TYPE_POINTER_TO (type), decl);
    }

  if (t = lookup_fnfields (TYPE_BINFO (type), method_name, 0))
    return build_method_call (decl, method_name, parmlist, basetype_path,
			      LOOKUP_NORMAL|LOOKUP_NONVIRTUAL);
  if (TREE_CODE (name) == IDENTIFIER_NODE
      && (t = lookup_field (TYPE_BINFO (type), name, 1, 0)))
    {
      if (t == error_mark_node)
	return error_mark_node;
      if (TREE_CODE (t) == FIELD_DECL)
	{
	  if (dont_use_this)
	    {
	      error ("invalid use of non-static field `%s'",
		     IDENTIFIER_POINTER (name));
	      return error_mark_node;
	    }
	  decl = build (COMPONENT_REF, TREE_TYPE (t), decl, t);
	}
      else if (TREE_CODE (t) == VAR_DECL)
	decl = t;
      else
	{
	  error ("invalid use of member `%s::%s'",
		 IDENTIFIER_POINTER (cname), name);
	  return error_mark_node;
	}
      if (TYPE_LANG_SPECIFIC (TREE_TYPE (decl))
	  && TYPE_OVERLOADS_CALL_EXPR (TREE_TYPE (decl)))
	return build_opfncall (CALL_EXPR, LOOKUP_NORMAL, decl, parmlist, NULL_TREE);
      return build_function_call (decl, parmlist);
    }
  else
    {
      char *err_name;
      if (TREE_CODE (name) == IDENTIFIER_NODE)
	{
	  if (IDENTIFIER_OPNAME_P (name))
	    {
	      char *op_name = operator_name_string (method_name);
	      err_name = (char *)alloca (13 + strlen (op_name));
	      sprintf (err_name, "operator %s", op_name);
	    }
	  else
	    err_name = IDENTIFIER_POINTER (name);
	}
      else
	my_friendly_abort (51);

      error ("no method `%s::%s'", IDENTIFIER_POINTER (cname), err_name);
      return error_mark_node;
    }
}

/* Build a reference to a member of an aggregate.  This is not a
   C++ `&', but really something which can have its address taken,
   and then act as a pointer to member, for example CNAME :: FIELD
   can have its address taken by saying & CNAME :: FIELD.

   @@ Prints out lousy diagnostics for operator <typename>
   @@ fields.

   @@ This function should be rewritten and placed in cp-search.c.  */
tree
build_offset_ref (cname, name)
     tree cname, name;
{
  tree decl, type, fnfields, fields, t = error_mark_node;
  tree basetypes = NULL_TREE;
  int dtor = 0;

  if (TREE_CODE (cname) == SCOPE_REF)
    cname = resolve_scope_to_name (NULL_TREE, cname);

  if (cname == NULL_TREE || ! is_aggr_typedef (cname, 1))
    return error_mark_node;

  type = IDENTIFIER_TYPE_VALUE (cname);

  if (TREE_CODE (name) == BIT_NOT_EXPR)
    {
      dtor = 1;
      name = TREE_OPERAND (name, 0);
    }

  if (TYPE_SIZE (type) == 0)
    {
      t = IDENTIFIER_CLASS_VALUE (name);
      if (t == 0)
	{
	  error_with_aggr_type (type, "incomplete type `%s' does not have member `%s'", IDENTIFIER_POINTER (name));
	  return error_mark_node;
	}
      if (TREE_CODE (t) == TYPE_DECL)
	{
	  error_with_decl (t, "member `%s' is just a type declaration");
	  return error_mark_node;
	}
      if (TREE_CODE (t) == VAR_DECL || TREE_CODE (t) == CONST_DECL)
	{
	  TREE_USED (t) = 1;
	  return t;
	}
      if (TREE_CODE (t) == FIELD_DECL)
	sorry ("use of member in incomplete aggregate type");
      else if (TREE_CODE (t) == FUNCTION_DECL)
	sorry ("use of member function in incomplete aggregate type");
      else
	my_friendly_abort (52);
      return error_mark_node;
    }

  if (TREE_CODE (name) == TYPE_EXPR)
    /* Pass a TYPE_DECL to build_component_type_expr.  */
    return build_component_type_expr (TYPE_NAME (TREE_TYPE (cname)),
				      name, NULL_TREE, 1);

  fnfields = lookup_fnfields (TYPE_BINFO (type), name, 1);
  fields = lookup_field (type, name, 0, 0);

  if (fields == error_mark_node || fnfields == error_mark_node)
    return error_mark_node;

  if (current_class_type == 0
      || get_base_distance (type, current_class_type, 0, &basetypes) == -1)
    {
      basetypes = TYPE_BINFO (type);
      decl = build1 (NOP_EXPR,
		     IDENTIFIER_TYPE_VALUE (cname),
		     error_mark_node);
    }
  else if (current_class_decl == 0)
    decl = build1 (NOP_EXPR, IDENTIFIER_TYPE_VALUE (cname),
		   error_mark_node);
  else decl = C_C_D;

  /* A lot of this logic is now handled in lookup_field and
     lookup_fnfield. */
  if (fnfields)
    {
      basetypes = TREE_PURPOSE (fnfields);

      /* Go from the TREE_BASELINK to the member function info.  */
      t = TREE_VALUE (fnfields);

      if (fields)
	{
	  if (DECL_FIELD_CONTEXT (fields) == DECL_FIELD_CONTEXT (t))
	    {
	      error ("ambiguous member reference: member `%s' defined as both field and function",
		     IDENTIFIER_POINTER (name));
	      return error_mark_node;
	    }
	  if (UNIQUELY_DERIVED_FROM_P (DECL_FIELD_CONTEXT (fields), DECL_FIELD_CONTEXT (t)))
	    ;
	  else if (UNIQUELY_DERIVED_FROM_P (DECL_FIELD_CONTEXT (t), DECL_FIELD_CONTEXT (fields)))
	    t = fields;
	  else
	    {
	      error ("ambiguous member reference: member `%s' derives from distinct classes in multiple inheritance lattice");
	      return error_mark_node;
	    }
	}

      if (t == TREE_VALUE (fnfields))
	{
	  extern int flag_save_memoized_contexts;

	  /* This does not handle visibility checking yet.  */
	  if (DECL_CHAIN (t) == NULL_TREE || dtor)
	    {
	      enum visibility_type visibility;

	      /* unique functions are handled easily.  */
	    unique:
	      visibility = compute_visibility (basetypes, t);
	      if (visibility == visibility_protected)
		{
		  error_with_decl (t, "member function `%s' is protected");
		  error ("in this context");
		  return error_mark_node;
		}
	      if (visibility == visibility_private)
		{
		  error_with_decl (t, "member function `%s' is private");
		  error ("in this context");
		  return error_mark_node;
		}
	      assemble_external (t);
	      return build (OFFSET_REF, TREE_TYPE (t), decl, t);
	    }

	  /* overloaded functions may need more work.  */
	  if (cname == name)
	    {
	      if (TYPE_HAS_DESTRUCTOR (type)
		  && DECL_CHAIN (DECL_CHAIN (t)) == NULL_TREE)
		{
		  t = DECL_CHAIN (t);
		  goto unique;
		}
	    }
	  /* FNFIELDS is most likely allocated on the search_obstack,
	     which will go away after this class scope.  If we need
	     to save this value for later (either for memoization
	     or for use as an initializer for a static variable), then
	     do so here.

	     ??? The smart thing to do for the case of saving initializers
	     is to resolve them before we're done with this scope.  */
	  if (!TREE_PERMANENT (fnfields)
	      && ((flag_save_memoized_contexts && global_bindings_p ())
		  || ! allocation_temporary_p ()))
	    fnfields = copy_list (fnfields);
	  t = build_tree_list (error_mark_node, fnfields);
	  TREE_TYPE (t) = build_offset_type (type, unknown_type_node);
	  return t;
	}
    }

  /* Now that we know we are looking for a field, see if we
     have access to that field.  Lookup_field will give us the
     error message.  */

  t = lookup_field (basetypes, name, 1, 0);

  if (t == error_mark_node)
    return error_mark_node;

  if (t == NULL_TREE)
    {
      char *print_name, *non_operator = "<";

      if (name == ansi_opname[(int) TYPE_EXPR])
	{
	  error ("type conversion operator not a member of type `%s'",
		 IDENTIFIER_POINTER (cname));
	  return error_mark_node;
	}

      if ((IDENTIFIER_POINTER (name))[0] == '_'
	  && (IDENTIFIER_POINTER (name))[1] == '_')
	print_name = operator_name_string (name);
      else
	print_name = non_operator;

      /* First character of "<invalid operator>".  */
      if (print_name[0] == '<')
	error ("field `%s' is not a member of type `%s'",
	       IDENTIFIER_POINTER (name),
	       IDENTIFIER_POINTER (cname));
      else
	error ("operator `%s' is not a member of type `%s'",
	       print_name, IDENTIFIER_POINTER (cname));
      return error_mark_node;
    }

  if (TREE_CODE (t) == TYPE_DECL)
    {
      error_with_decl (t, "member `%s' is just a type declaration");
      return error_mark_node;
    }
  /* static class members and class-specific enum
     values can be returned without further ado.  */
  if (TREE_CODE (t) == VAR_DECL || TREE_CODE (t) == CONST_DECL)
    {
      assemble_external (t);
      TREE_USED (t) = 1;
      return t;
    }

  /* static class functions too.  */
  if (TREE_CODE (t) == FUNCTION_DECL && TREE_CODE (TREE_TYPE (t)) == FUNCTION_TYPE)
    my_friendly_abort (53);

  /* In member functions, the form `cname::name' is no longer
     equivalent to `this->cname::name'.  */
  return build (OFFSET_REF, build_offset_type (type, TREE_TYPE (t)), decl, t);
}

/* Given an object EXP and a member function reference MEMBER,
   return the address of the actual member function.  */
tree
get_member_function (exp_addr_ptr, exp, member)
     tree *exp_addr_ptr;
     tree exp, member;
{
  tree ctype = TREE_TYPE (exp);
  tree function = save_expr (build_unary_op (ADDR_EXPR, member, 0));

  if (TYPE_VIRTUAL_P (ctype)
      || (flag_all_virtual == 1 && TYPE_OVERLOADS_METHOD_CALL_EXPR (ctype)))
    {
      tree e0, e1, e3;
      tree exp_addr;

      /* Save away the unadulterated `this' pointer.  */
      exp_addr = save_expr (*exp_addr_ptr);

      /* Cast function to signed integer.  */
      e0 = build1 (NOP_EXPR, integer_type_node, function);

#ifdef VTABLE_USES_MASK
      /* If we are willing to limit the number of
	 virtual functions a class may have to some
	 *small* number, then if, for a function address,
	 we are passed some small number, we know that
	 it is a virtual function index, and work from there.  */
      e1 = build (BIT_AND_EXPR, integer_type_node, e0, vtbl_mask);
#else
      /* There is a hack here that takes advantage of
	 twos complement arithmetic, and the fact that
	 there are more than one UNITS to the WORD.
	 If the high bit is set for the `function',
	 then we pretend it is a virtual function,
	 and the array indexing will knock this bit
	 out the top, leaving a valid index.  */
      if (UNITS_PER_WORD <= 1)
	my_friendly_abort (54);

      e1 = build (GT_EXPR, integer_type_node, e0, integer_zero_node);
      e1 = build_compound_expr (tree_cons (NULL_TREE, exp_addr,
					   build_tree_list (NULL_TREE, e1)));
      e1 = save_expr (e1);
#endif

      if (TREE_SIDE_EFFECTS (*exp_addr_ptr))
	{
	  exp = build_indirect_ref (exp_addr, 0);
	  *exp_addr_ptr = exp_addr;
	}

      /* This is really hairy: if the function pointer is a pointer
	 to a non-virtual member function, then we can't go mucking
	 with the `this' pointer (any more than we already have to
	 this point).  If it is a pointer to a virtual member function,
	 then we have to adjust the `this' pointer according to
	 what the virtual function table tells us.  */

      e3 = build_vfn_ref (exp_addr_ptr, exp, e0);
      my_friendly_assert (e3 != error_mark_node, 213);

      /* Change this pointer type from `void *' to the
	 type it is really supposed to be.  */
      TREE_TYPE (e3) = TREE_TYPE (function);

      /* If non-virtual, use what we had originally.  Otherwise,
	 use the value we get from the virtual function table.  */
      *exp_addr_ptr = build_conditional_expr (e1, exp_addr, *exp_addr_ptr);

      function = build_conditional_expr (e1, function, e3);
    }
  return build_indirect_ref (function, 0);
}

/* If a OFFSET_REF made it through to here, then it did
   not have its address taken.  */

tree
resolve_offset_ref (exp)
     tree exp;
{
  tree type = TREE_TYPE (exp);
  tree base = NULL_TREE;
  tree member;
  tree basetype, addr;

  if (TREE_CODE (exp) == TREE_LIST)
    return build_unary_op (ADDR_EXPR, exp, 0);

  if (TREE_CODE (exp) != OFFSET_REF)
    {
      my_friendly_assert (TREE_CODE (type) == OFFSET_TYPE, 214);
      if (TYPE_OFFSET_BASETYPE (type) != current_class_type)
	{
	  error ("object missing in use of pointer-to-member construct");
	  return error_mark_node;
	}
      member = exp;
      type = TREE_TYPE (type);
      base = C_C_D;
    }
  else
    {
      member = TREE_OPERAND (exp, 1);
      base = TREE_OPERAND (exp, 0);
    }

  if (TREE_CODE (member) == VAR_DECL
      || TREE_CODE (TREE_TYPE (member)) == FUNCTION_TYPE)
    {
      /* These were static members.  */
      if (mark_addressable (member) == 0)
	return error_mark_node;
      return member;
    }

  /* Syntax error can cause a member which should
     have been seen as static to be grok'd as non-static.  */
  if (TREE_CODE (member) == FIELD_DECL && C_C_D == NULL_TREE)
    {
      if (TREE_ADDRESSABLE (member) == 0)
	{
	  error_with_decl (member, "member `%s' is non-static in static member function context");
	  error ("at this point in file");
	  TREE_ADDRESSABLE (member) = 1;
	}
      return error_mark_node;
    }

  /* The first case is really just a reference to a member of `this'.  */
  if (TREE_CODE (member) == FIELD_DECL
      && (base == C_C_D
	  || (TREE_CODE (base) == NOP_EXPR
	      && TREE_OPERAND (base, 0) == error_mark_node)))
    {
      tree basetype_path;
      enum visibility_type visibility;

      basetype = DECL_CONTEXT (member);
      if (get_base_distance (basetype, current_class_type, 0, &basetype_path) < 0)
	{
	  error_not_base_type (basetype, current_class_type);
	  return error_mark_node;
	}
      addr = convert_pointer_to (basetype, current_class_decl);
      visibility = compute_visibility (basetype_path, member);
      if (visibility == visibility_public)
	return build (COMPONENT_REF, TREE_TYPE (member),
		      build_indirect_ref (addr, 0), member);
      if (visibility == visibility_protected)
	{
	  error_with_decl (member, "member `%s' is protected");
	  error ("in this context");
	  return error_mark_node;
	}
      if (visibility == visibility_private)
	{
	  error_with_decl (member, "member `%s' is private");
	  error ("in this context");
	  return error_mark_node;
	}
      my_friendly_abort (55);
    }

  /* If this is a reference to a member function, then return
     the address of the member function (which may involve going
     through the object's vtable), otherwise, return an expression
     for the dereferenced pointer-to-member construct.  */
  addr = build_unary_op (ADDR_EXPR, base, 0);

  if (TREE_CODE (TREE_TYPE (member)) == METHOD_TYPE)
    {
      basetype = DECL_CLASS_CONTEXT (member);
      addr = convert_pointer_to (basetype, addr);
      return build_unary_op (ADDR_EXPR, get_member_function (&addr, build_indirect_ref (addr, 0), member), 0);
    }
  else if (TREE_CODE (TREE_TYPE (member)) == OFFSET_TYPE)
    {
      basetype = TYPE_OFFSET_BASETYPE (TREE_TYPE (member));
      addr = convert_pointer_to (basetype, addr);
      member = convert (ptr_type_node, build_unary_op (ADDR_EXPR, member, 0));
      return build1 (INDIRECT_REF, type,
		     build (PLUS_EXPR, ptr_type_node, addr, member));
    }
  my_friendly_abort (56);
  /* NOTREACHED */
  return NULL_TREE;
}

/* Return either DECL or its known constant value (if it has one).  */

tree
decl_constant_value (decl)
     tree decl;
{
  if (! TREE_THIS_VOLATILE (decl)
#if 0
      /* These may be necessary for C, but they break C++.  */
      ! TREE_PUBLIC (decl)
      /* Don't change a variable array bound or initial value to a constant
	 in a place where a variable is invalid.  */
      && ! pedantic
#endif /* 0 */
      && DECL_INITIAL (decl) != 0
      && TREE_CODE (DECL_INITIAL (decl)) != ERROR_MARK
      /* This is invalid if initial value is not constant.
	 If it has either a function call, a memory reference,
	 or a variable, then re-evaluating it could give different results.  */
      && TREE_CONSTANT (DECL_INITIAL (decl))
      /* Check for cases where this is sub-optimal, even though valid.  */
      && TREE_CODE (DECL_INITIAL (decl)) != CONSTRUCTOR
#if 0
      /* We must allow this to work outside of functions so that
	 static constants can be used for array sizes.  */
      && current_function_decl != 0
      && DECL_MODE (decl) != BLKmode
#endif
      )
    return DECL_INITIAL (decl);
  return decl;
}

/* Friend handling routines.  */
/* Friend data structures:

   Friend lists come from TYPE_DECL nodes.  Since all aggregate
   types are automatically typedef'd, these node are guaranteed
   to exist.

   The TREE_PURPOSE of a friend list is the name of the friend,
   and its TREE_VALUE is another list.

   The TREE_PURPOSE of that list is a type, which allows
   all functions of a given type to be friends.
   The TREE_VALUE of that list is an individual function
   which is a friend.

   Non-member friends will match only by their DECL.  Their
   member type is NULL_TREE, while the type of the inner
   list will either be of aggregate type or error_mark_node.  */

/* Tell if this function specified by DECL
   can be a friend of type TYPE.
   Return nonzero if friend, zero otherwise.

   DECL can be zero if we are calling a constructor or accessing a
   member in global scope.  */
int
is_friend (type, decl)
     tree type, decl;
{
  tree typedecl = TYPE_NAME (type);
  tree ctype;
  tree list;
  tree name;

  if (decl == NULL_TREE)
    return 0;

  ctype = DECL_CLASS_CONTEXT (decl);
  if (ctype)
    {
      list = CLASSTYPE_FRIEND_CLASSES (TREE_TYPE (typedecl));
      while (list)
	{
	  if (ctype == TREE_VALUE (list))
	    return 1;
	  list = TREE_CHAIN (list);
	}
    }

  list = DECL_FRIENDLIST (typedecl);
  name = DECL_NAME (decl);
  while (list)
    {
      if (name == TREE_PURPOSE (list))
	{
	  tree friends = TREE_VALUE (list);
	  name = DECL_ASSEMBLER_NAME (decl);
	  while (friends)
	    {
	      if (ctype == TREE_PURPOSE (friends))
		return 1;
	      if (name == DECL_ASSEMBLER_NAME (TREE_VALUE (friends)))
		return 1;
	      friends = TREE_CHAIN (friends);
	    }
	  return 0;
	}
      list = TREE_CHAIN (list);
    }
  return 0;
}

/* Add a new friend to the friends of the aggregate type TYPE.
   DECL is the FUNCTION_DECL of the friend being added.  */
static void
add_friend (type, decl)
     tree type, decl;
{
  tree typedecl = TYPE_NAME (type);
  tree list = DECL_FRIENDLIST (typedecl);
  tree name = DECL_NAME (decl);
  tree ctype = TREE_CODE (TREE_TYPE (decl)) == METHOD_TYPE
    ? DECL_CLASS_CONTEXT (decl) : error_mark_node;

  while (list)
    {
      if (name == TREE_PURPOSE (list))
	{
	  tree friends = TREE_VALUE (list);
	  while (friends)
	    {
	      if (decl == TREE_VALUE (friends))
		{
		  warning_with_decl (decl, "`%s' is already a friend of class `%s'", IDENTIFIER_POINTER (DECL_NAME (typedecl)));
		  return;
		}
	      friends = TREE_CHAIN (friends);
	    }
	  TREE_VALUE (list) = tree_cons (ctype, decl, TREE_VALUE (list));
	  return;
	}
      list = TREE_CHAIN (list);
    }
  DECL_FRIENDLIST (typedecl)
    = tree_cons (DECL_NAME (decl), build_tree_list (error_mark_node, decl),
		 DECL_FRIENDLIST (typedecl));
  if (DECL_NAME (decl) == ansi_opname[(int) MODIFY_EXPR])
    {
      tree parmtypes = TYPE_ARG_TYPES (TREE_TYPE (decl));
      TYPE_HAS_ASSIGNMENT (TREE_TYPE (typedecl)) = 1;
      TYPE_GETS_ASSIGNMENT (TREE_TYPE (typedecl)) = 1;
      if (parmtypes && TREE_CHAIN (parmtypes))
	{
	  tree parmtype = TREE_VALUE (TREE_CHAIN (parmtypes));
	  if (TREE_CODE (parmtype) == REFERENCE_TYPE
	      && TREE_TYPE (parmtypes) == TREE_TYPE (typedecl))
	    {
	      TYPE_HAS_ASSIGN_REF (TREE_TYPE (typedecl)) = 1;
	      TYPE_GETS_ASSIGN_REF (TREE_TYPE (typedecl)) = 1;
	    }
	}
    }
}

/* Declare that every member function NAME in FRIEND_TYPE
   (which may be NULL_TREE) is a friend of type TYPE.  */
static void
add_friends (type, name, friend_type)
     tree type, name, friend_type;
{
  tree typedecl = TYPE_NAME (type);
  tree list = DECL_FRIENDLIST (typedecl);

  while (list)
    {
      if (name == TREE_PURPOSE (list))
	{
	  tree friends = TREE_VALUE (list);
	  while (friends && TREE_PURPOSE (friends) != friend_type)
	    friends = TREE_CHAIN (friends);
	  if (friends)
	    if (friend_type)
	      warning ("method `%s::%s' is already a friend of class",
		       TYPE_NAME_STRING (friend_type),
		       IDENTIFIER_POINTER (name));
	    else
	      warning ("function `%s' is already a friend of class `%s'",
		       IDENTIFIER_POINTER (name),
		       IDENTIFIER_POINTER (DECL_NAME (typedecl)));
	  else
	    TREE_VALUE (list) = tree_cons (friend_type, NULL_TREE,
					   TREE_VALUE (list));
	  return;
	}
      list = TREE_CHAIN (list);
    }
  DECL_FRIENDLIST (typedecl) =
    tree_cons (name,
	       build_tree_list (friend_type, NULL_TREE),
	       DECL_FRIENDLIST (typedecl));
  if (! strncmp (IDENTIFIER_POINTER (name),
		 IDENTIFIER_POINTER (ansi_opname[(int) MODIFY_EXPR]),
		 strlen (IDENTIFIER_POINTER (ansi_opname[(int) MODIFY_EXPR]))))
    {
      TYPE_HAS_ASSIGNMENT (TREE_TYPE (typedecl)) = 1;
      TYPE_GETS_ASSIGNMENT (TREE_TYPE (typedecl)) = 1;
      sorry ("declaring \"friend operator =\" will not find \"operator = (X&)\" if it exists");
    }
}

/* Set up a cross reference so that type TYPE will
   make member function CTYPE::DECL a friend when CTYPE
   is finally defined.  */
void
xref_friend (type, decl, ctype)
     tree type, decl, ctype;
{
  tree typedecl = TYPE_NAME (type);
  tree friend_decl = TYPE_NAME (ctype);
  tree t = tree_cons (NULL_TREE, ctype, DECL_UNDEFINED_FRIENDS (typedecl));

  DECL_UNDEFINED_FRIENDS (typedecl) = t;
  SET_DECL_WAITING_FRIENDS (friend_decl, tree_cons (type, t, DECL_WAITING_FRIENDS (friend_decl)));
  TREE_TYPE (DECL_WAITING_FRIENDS (friend_decl)) = decl;
}

/* Set up a cross reference so that functions with name NAME and
   type CTYPE know that they are friends of TYPE.  */
void
xref_friends (type, name, ctype)
     tree type, name, ctype;
{
  tree typedecl = TYPE_NAME (type);
  tree friend_decl = TYPE_NAME (ctype);
  tree t = tree_cons (NULL_TREE, ctype,
		      DECL_UNDEFINED_FRIENDS (typedecl));

  DECL_UNDEFINED_FRIENDS (typedecl) = t;
  SET_DECL_WAITING_FRIENDS (friend_decl, tree_cons (type, t, DECL_WAITING_FRIENDS (friend_decl)));
  TREE_TYPE (DECL_WAITING_FRIENDS (friend_decl)) = name;
}

/* Make FRIEND_TYPE a friend class to TYPE.  If FRIEND_TYPE has already
   been defined, we make all of its member functions friends of
   TYPE.  If not, we make it a pending friend, which can later be added
   when its definition is seen.  If a type is defined, then its TYPE_DECL's
   DECL_UNDEFINED_FRIENDS contains a (possibly empty) list of friend
   classes that are not defined.  If a type has not yet been defined,
   then the DECL_WAITING_FRIENDS contains a list of types
   waiting to make it their friend.  Note that these two can both
   be in use at the same time!  */
void
make_friend_class (type, friend_type)
     tree type, friend_type;
{
  tree classes;

  if (type == friend_type)
    {
      warning ("class `%s' is implicitly friends with itself",
	       TYPE_NAME_STRING (type));
      return;
    }

  GNU_xref_hier (TYPE_NAME_STRING (type),
		 TYPE_NAME_STRING (friend_type), 0, 0, 1);

  classes = CLASSTYPE_FRIEND_CLASSES (type);
  while (classes && TREE_VALUE (classes) != friend_type)
    classes = TREE_CHAIN (classes);
  if (classes)
    warning ("class `%s' is already friends with class `%s'",
	     TYPE_NAME_STRING (TREE_VALUE (classes)), TYPE_NAME_STRING (type));
  else
    {
      CLASSTYPE_FRIEND_CLASSES (type)
	= tree_cons (NULL_TREE, friend_type, CLASSTYPE_FRIEND_CLASSES (type));
    }
}

/* Main friend processor.  This is large, and for modularity purposes,
   has been removed from grokdeclarator.  It returns `void_type_node'
   to indicate that something happened, though a FIELD_DECL is
   not returned.

   CTYPE is the class this friend belongs to.

   DECLARATOR is the name of the friend.

   DECL is the FUNCTION_DECL that the friend is.

   In case we are parsing a friend which is part of an inline
   definition, we will need to store PARM_DECL chain that comes
   with it into the DECL_ARGUMENTS slot of the FUNCTION_DECL.

   FLAGS is just used for `grokclassfn'.

   QUALS say what special qualifies should apply to the object
   pointed to by `this'.  */
tree
do_friend (ctype, declarator, decl, parmdecls, flags, quals)
     tree ctype, declarator, decl, parmdecls;
     enum overload_flags flags;
     tree quals;
{
  /* first, lets find out if what we are making a friend needs overloading */
  tree previous_decl;
  int was_c_linkage = 0;
  /* if we find something in scope, let see if it has extern "C" linkage */
  /* This code is pretty general and should be ripped out and reused
     as a separate function. */
  if (DECL_NAME (decl))
    {
      previous_decl=lookup_name (DECL_NAME (decl), 0);
      if (previous_decl && TREE_CODE (previous_decl) == TREE_LIST)
	{
	  do
	    {
	      if (TREE_TYPE (TREE_VALUE (previous_decl)) == TREE_TYPE (decl))
		{
		  previous_decl = TREE_VALUE (previous_decl);
		  break;
		}
	      previous_decl = TREE_CHAIN (previous_decl);
	    }
	  while (previous_decl);
	}
      if (previous_decl && TREE_CODE (previous_decl) == FUNCTION_DECL)
	if (TREE_TYPE (decl) == TREE_TYPE (previous_decl))
	    if (DECL_LANGUAGE (previous_decl) == lang_c)
	    {
	      /* it did, so lets not overload this */
	      was_c_linkage = 1;
	    }
    }
	  
  if (ctype)
    {
      tree cname = TYPE_NAME (ctype);
      if (TREE_CODE (cname) == TYPE_DECL)
	cname = DECL_NAME (cname);

      /* A method friend.  */
      if (TREE_CODE (decl) == FUNCTION_DECL)
	{
	  if (flags == NO_SPECIAL && ctype && declarator == cname)
	    DECL_CONSTRUCTOR_P (decl) = 1;

	  /* This will set up DECL_ARGUMENTS for us.  */
	  grokclassfn (ctype, cname, decl, flags, quals);
	  if (TYPE_SIZE (ctype) != 0)
	    check_classfn (ctype, cname, decl);

	  if (TREE_TYPE (decl) != error_mark_node)
	    {
	      if (TYPE_SIZE (ctype))
		{
		  /* We don't call pushdecl here yet, or ever on this
		     actual FUNCTION_DECL.  We must preserve its TREE_CHAIN
		     until the end.  */
		  make_decl_rtl (decl, NULL_PTR, 1);
		  add_friend (current_class_type, decl);
		}
	      else
		xref_friend (current_class_type, decl, ctype);
	      DECL_FRIEND_P (decl) = 1;
	    }
	}
      else
	{
	  /* Possibly a bunch of method friends.  */

	  /* Get the class they belong to.  */
	  tree ctype = IDENTIFIER_TYPE_VALUE (cname);

	  /* This class is defined, use its methods now.  */
	  if (TYPE_SIZE (ctype))
	    {
	      tree fields = lookup_fnfields (TYPE_BINFO (ctype), declarator, 0);
	      if (fields)
		add_friends (current_class_type, declarator, ctype);
	      else
		error ("method `%s' is not a member of class `%s'",
		       IDENTIFIER_POINTER (declarator),
		       IDENTIFIER_POINTER (cname));
	    }
	  else
	    xref_friends (current_class_type, declarator, ctype);
	  decl = void_type_node;
	}
    }
  /* never overload C functions */
  else if (TREE_CODE (decl) == FUNCTION_DECL
	   && ((IDENTIFIER_LENGTH (declarator) == 4
		&& IDENTIFIER_POINTER (declarator)[0] == 'm'
		&& ! strcmp (IDENTIFIER_POINTER (declarator), "main"))
	       || (IDENTIFIER_LENGTH (declarator) > 10
		   && IDENTIFIER_POINTER (declarator)[0] == '_'
		   && IDENTIFIER_POINTER (declarator)[1] == '_'
		   && strncmp (IDENTIFIER_POINTER (declarator)+2,
			       "builtin_", 8) == 0)
	       || was_c_linkage))
    {
      /* raw "main", and builtin functions never gets overloaded,
	 but they can become friends.  */
      TREE_PUBLIC (decl) = 1;
      add_friend (current_class_type, decl);
      DECL_FRIEND_P (decl) = 1;
      if (IDENTIFIER_POINTER (declarator)[0] == '_')
	{
	  if (! strcmp (IDENTIFIER_POINTER (declarator)+10, "new"))
	    TREE_GETS_NEW (current_class_type) = 0;
	  else if (! strcmp (IDENTIFIER_POINTER (declarator)+10, "delete"))
	    TREE_GETS_DELETE (current_class_type) = 0;
	}
      decl = void_type_node;
    }
  /* A global friend.
     @@ or possibly a friend from a base class ?!?  */
  else if (TREE_CODE (decl) == FUNCTION_DECL)
    {
      /* Friends must all go through the overload machinery,
	 even though they may not technically be overloaded.

	 Note that because classes all wind up being top-level
	 in their scope, their friend wind up in top-level scope as well.  */
      DECL_ASSEMBLER_NAME (decl)
	= build_decl_overload (declarator, TYPE_ARG_TYPES (TREE_TYPE (decl)),
			       TREE_CODE (TREE_TYPE (decl)) == METHOD_TYPE);
      DECL_ARGUMENTS (decl) = parmdecls;

      /* We can call pushdecl here, because the TREE_CHAIN of this
	 FUNCTION_DECL is not needed for other purposes.  */
      decl = pushdecl_top_level (decl);

      make_decl_rtl (decl, NULL_PTR, 1);
      add_friend (current_class_type, decl);

      if (! TREE_OVERLOADED (declarator)
	  && IDENTIFIER_GLOBAL_VALUE (declarator)
	  && TREE_CODE (IDENTIFIER_GLOBAL_VALUE (declarator)) == FUNCTION_DECL)
	{
	  error ("friend `%s' implicitly overloaded",
		 IDENTIFIER_POINTER (declarator));
	  error_with_decl (IDENTIFIER_GLOBAL_VALUE (declarator),
			   "after declaration of non-overloaded `%s'");
	}
      DECL_FRIEND_P (decl) = 1;
      DECL_OVERLOADED (decl) = 1;
      TREE_OVERLOADED (declarator) = 1;
      decl = push_overloaded_decl (decl, 1);
    }
  else
    {
      /* @@ Should be able to ingest later definitions of this function
	 before use.  */
      tree decl = IDENTIFIER_GLOBAL_VALUE (declarator);
      if (decl == NULL_TREE)
	{
	  warning ("implicitly declaring `%s' as struct",
		   IDENTIFIER_POINTER (declarator));
	  decl = xref_tag (record_type_node, declarator, NULL_TREE);
	  decl = TYPE_NAME (decl);
	}

      /* Allow abbreviated declarations of overloaded functions,
	 but not if those functions are really class names.  */
      if (TREE_CODE (decl) == TREE_LIST && TREE_TYPE (TREE_PURPOSE (decl)))
	{
	  warning ("`friend %s' archaic, use `friend class %s' instead",
		   IDENTIFIER_POINTER (declarator),
		   IDENTIFIER_POINTER (declarator));
	  decl = TREE_TYPE (TREE_PURPOSE (decl));
	}

      if (TREE_CODE (decl) == TREE_LIST)
	add_friends (current_class_type, TREE_PURPOSE (decl), NULL_TREE);
      else
	make_friend_class (current_class_type, TREE_TYPE (decl));
      decl = void_type_node;
    }
  return decl;
}

/* TYPE has now been defined.  It may, however, have a number of things
   waiting make make it their friend.  We resolve these references
   here.  */
void
embrace_waiting_friends (type)
     tree type;
{
  tree decl = TYPE_NAME (type);
  tree waiters;

  if (TREE_CODE (decl) != TYPE_DECL)
    return;

  for (waiters = DECL_WAITING_FRIENDS (decl); waiters;
       waiters = TREE_CHAIN (waiters))
    {
      tree waiter = TREE_PURPOSE (waiters);
      tree waiter_prev = TREE_VALUE (waiters);
      tree decl = TREE_TYPE (waiters);
      tree name = decl ? (TREE_CODE (decl) == IDENTIFIER_NODE
			  ? decl : DECL_NAME (decl)) : NULL_TREE;
      if (name)
	{
	  /* @@ There may be work to be done since we have not verified
	     @@ consistency between original and friend declarations
	     @@ of the functions waiting to become friends.  */
	  tree field = lookup_fnfields (TYPE_BINFO (type), name, 0);
	  if (field)
	    if (decl == name)
	      add_friends (waiter, name, type);
	    else
	      add_friend (waiter, decl);
	  else
	    error_with_file_and_line (DECL_SOURCE_FILE (TYPE_NAME (waiter)),
				      DECL_SOURCE_LINE (TYPE_NAME (waiter)),
				      "no method `%s' defined in class `%s' to be friend",
				      IDENTIFIER_POINTER (DECL_NAME (TREE_TYPE (waiters))),
				      TYPE_NAME_STRING (type));
	}
      else
	make_friend_class (type, waiter);

      if (TREE_CHAIN (waiter_prev))
	TREE_CHAIN (waiter_prev) = TREE_CHAIN (TREE_CHAIN (waiter_prev));
      else
	DECL_UNDEFINED_FRIENDS (TYPE_NAME (waiter)) = NULL_TREE;
    }
}

/* Common subroutines of build_new and build_vec_delete.  */

/* Common interface for calling "builtin" functions that are not
   really builtin.  */

tree
build_builtin_call (type, node, arglist)
     tree type;
     tree node;
     tree arglist;
{
  tree rval = build (CALL_EXPR, type, node, arglist, 0);
  TREE_SIDE_EFFECTS (rval) = 1;
  assemble_external (TREE_OPERAND (node, 0));
  TREE_USED (TREE_OPERAND (node, 0)) = 1;
  return rval;
}

/* Generate a C++ "new" expression. DECL is either a TREE_LIST
   (which needs to go through some sort of groktypename) or it
   is the name of the class we are newing. INIT is an initialization value.
   It is either an EXPRLIST, an EXPR_NO_COMMAS, or something in braces.
   If INIT is void_type_node, it means do *not* call a constructor
   for this instance.

   For types with constructors, the data returned is initialized
   by the appropriate constructor.

   Whether the type has a constructor or not, if it has a pointer
   to a virtual function table, then that pointer is set up
   here.

   Unless I am mistaken, a call to new () will return initialized
   data regardless of whether the constructor itself is private or
   not.

   Note that build_new does nothing to assure that any special
   alignment requirements of the type are met.  Rather, it leaves
   it up to malloc to do the right thing.  Otherwise, folding to
   the right alignment cal cause problems if the user tries to later
   free the memory returned by `new'.

   PLACEMENT is the `placement' list for user-defined operator new ().  */

tree
build_new (placement, decl, init, use_global_new)
     tree placement;
     tree decl, init;
     int use_global_new;
{
  tree type, true_type, size, rval;
  tree init1 = NULL_TREE, nelts;
  int has_call = 0, has_array = 0;

  tree pending_sizes = NULL_TREE;

  if (decl == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (decl) == TREE_LIST)
    {
      tree absdcl = TREE_VALUE (decl);
      tree last_absdcl = NULL_TREE;
      int old_immediate_size_expand;

      if (current_function_decl
	  && DECL_CONSTRUCTOR_P (current_function_decl))
	{
	  old_immediate_size_expand = immediate_size_expand;
	  immediate_size_expand = 0;
	}

      nelts = integer_one_node;

      if (absdcl && TREE_CODE (absdcl) == CALL_EXPR)
	{
	  /* probably meant to be a call */
	  has_call = 1;
	  init1 = TREE_OPERAND (absdcl, 1);
	  absdcl = TREE_OPERAND (absdcl, 0);
	  TREE_VALUE (decl) = absdcl;
	}
      while (absdcl && TREE_CODE (absdcl) == INDIRECT_REF)
	{
	  last_absdcl = absdcl;
	  absdcl = TREE_OPERAND (absdcl, 0);
	}

      if (absdcl && TREE_CODE (absdcl) == ARRAY_REF)
	{
	  /* probably meant to be a vec new */
	  tree this_nelts;

	  has_array = 1;
	  this_nelts = TREE_OPERAND (absdcl, 1);
	  if (this_nelts != error_mark_node)
	    {
	      if (this_nelts == NULL_TREE)
		error ("new of array type fails to specify size");
	      else
		{
		  this_nelts = save_expr (this_nelts);
		  absdcl = TREE_OPERAND (absdcl, 0);
	          if (this_nelts == integer_zero_node)
		    {
		      warning ("zero size array reserves no space");
		      nelts = integer_zero_node;
		    }
		  else
		    nelts = build_binary_op (MULT_EXPR, nelts, this_nelts, 1);
		}
	    }
	  else
	    nelts = integer_zero_node;
	}

      if (last_absdcl)
	TREE_OPERAND (last_absdcl, 0) = absdcl;
      else
	TREE_VALUE (decl) = absdcl;

      type = true_type = groktypename (decl);
      if (! type || type == error_mark_node
	  || true_type == error_mark_node)
	return error_mark_node;

      /* ``A reference cannot be created by the new operator.  A reference
	 is not an object (8.2.2, 8.4.3), so a pointer to it could not be
	 returned by new.'' ARM 5.3.3 */
      if (TREE_CODE (type) == REFERENCE_TYPE)
	error ("new cannot be applied to a reference type");

      type = TYPE_MAIN_VARIANT (type);
      if (type == void_type_node)
	{
	  error ("invalid type: `void []'");
	  return error_mark_node;
	}
      if (current_function_decl
	  && DECL_CONSTRUCTOR_P (current_function_decl))
	{
	  pending_sizes = get_pending_sizes ();
	  immediate_size_expand = old_immediate_size_expand;
	}
    }
  else if (TREE_CODE (decl) == IDENTIFIER_NODE)
    {
      if (IDENTIFIER_HAS_TYPE_VALUE (decl))
	{
	  /* An aggregate type.  */
	  type = IDENTIFIER_TYPE_VALUE (decl);
	  decl = TYPE_NAME (type);
	}
      else
	{
	  /* A builtin type.  */
	  decl = lookup_name (decl, 1);
	  my_friendly_assert (TREE_CODE (decl) == TYPE_DECL, 215);
	  type = TREE_TYPE (decl);
	}
      true_type = type;
    }
  else if (TREE_CODE (decl) == TYPE_DECL)
    {
      type = TREE_TYPE (decl);
      true_type = type;
    }
  else
    {
      type = decl;
      true_type = type;
      decl = TYPE_NAME (type);
    }

  if (TYPE_SIZE (type) == 0)
    {
      if (type == void_type_node)
	error ("invalid type for new: `void'");
      else
	incomplete_type_error (0, type);
      return error_mark_node;
    }

  if (TYPE_LANG_SPECIFIC (type) && CLASSTYPE_ABSTRACT_VIRTUALS (type))
    {
      abstract_virtuals_error (NULL_TREE, type);
      return error_mark_node;
    }

  /* If our base type is an array, then make sure we know how many elements
     it has.  */
  while (TREE_CODE (type) == ARRAY_TYPE)
    {
      tree this_nelts = array_type_nelts_top (type);
      if (nelts == integer_one_node)
	{
	  has_array = 1;
	  nelts = this_nelts;
	}
      else
	{
	  my_friendly_assert (has_array != 0, 216);
	  nelts = build_binary_op (MULT_EXPR, nelts, this_nelts, 1);
	}
      type = TREE_TYPE (type);
    }
  if (has_array)
    size = fold (build_binary_op (MULT_EXPR, size_in_bytes (type), nelts, 1));
  else
    size = size_in_bytes (type);

  if (has_call)
    init = init1;

  /* Get to the target type of TRUE_TYPE, so we can decide whether
     any constructors need to be called or not.  */
  type = true_type;
  while (TREE_CODE (type) == ARRAY_TYPE)
    type = TREE_TYPE (type);

  /* Get a little extra space to store a couple of things before the new'ed
     array. */
  if (has_array && TYPE_NEEDS_DESTRUCTOR (true_type))
    {
      tree extra = BI_header_size;

      size = size_binop (PLUS_EXPR, size, extra);
    }

  /* Allocate the object. */
  if (TYPE_LANG_SPECIFIC (true_type)
      && (TREE_GETS_NEW (true_type) && !use_global_new))
    rval = build_opfncall (NEW_EXPR, LOOKUP_NORMAL,
			   TYPE_POINTER_TO (true_type), size, placement);
  else if (placement)
    {
      rval = build_opfncall (NEW_EXPR, LOOKUP_GLOBAL|LOOKUP_COMPLAIN,
			     ptr_type_node, size, placement);
      rval = convert (TYPE_POINTER_TO (true_type), rval);
    }
  else if (flag_this_is_variable > 0
	   && TYPE_HAS_CONSTRUCTOR (true_type) && init != void_type_node)
    {
      if (init == NULL_TREE || TREE_CODE (init) == TREE_LIST)
	rval = NULL_TREE;
      else
	{
	  error ("constructors take parameter lists");
	  return error_mark_node;
	}
    }
  else
    {
      rval = build_builtin_call (build_pointer_type (true_type),
				 BIN, build_tree_list (NULL_TREE, size));
#if 0
      /* See comment above as to why this is disabled.  */
      if (alignment)
	{
	  rval = build (PLUS_EXPR, TYPE_POINTER_TO (true_type), rval,
			alignment);
	  rval = build (BIT_AND_EXPR, TYPE_POINTER_TO (true_type),
			rval, build1 (BIT_NOT_EXPR, integer_type_node,
				      alignment));
	}
#endif
      TREE_CALLS_NEW (rval) = 1;
      TREE_SIDE_EFFECTS (rval) = 1;
    }

  /* if rval is NULL_TREE I don't have to allocate it, but are we totally
     sure we have some extra bytes in that case for the BI_header_size
     cookies? And how does that interact with the code below? (mrs) */
  /* Finish up some magic for new'ed arrays */
  if (has_array && TYPE_NEEDS_DESTRUCTOR (true_type) && rval != NULL_TREE)
    {
      tree extra = BI_header_size;
      tree cookie, exp1, exp2;
      rval = convert (ptr_type_node, rval);    /* convert to void * first */
      rval = convert (string_type_node, rval); /* lets not add void* and ints */
      rval = save_expr (build_binary_op (PLUS_EXPR, rval, extra, 1));
      /* Store header info.  */
      cookie = build_indirect_ref (build (MINUS_EXPR, TYPE_POINTER_TO (BI_header_type),
					  rval, extra), 0);
      exp1 = build (MODIFY_EXPR, void_type_node,
		    build_component_ref (cookie, get_identifier ("nelts"), 0, 0),
		    nelts);
      TREE_SIDE_EFFECTS (exp1) = 1;
      exp2 = build (MODIFY_EXPR, void_type_node,
		    build_component_ref (cookie, get_identifier ("ptr_2comp"), 0, 0),
		    build (MINUS_EXPR, ptr_type_node, integer_zero_node, rval));
      TREE_SIDE_EFFECTS (exp2) = 1;
      rval = convert (build_pointer_type (true_type), rval);
      TREE_CALLS_NEW (rval) = 1;
      TREE_SIDE_EFFECTS (rval) = 1;
      rval = build_compound_expr (tree_cons (NULL_TREE, exp1,
					     tree_cons (NULL_TREE, exp2,
							build_tree_list (NULL_TREE, rval))));
    }

  /* We've figured out where the allocation is to go.
     If we're not eliding constructors, then if a constructor
     is defined, we must go through it.  */
  if (!has_array && (rval == NULL_TREE || !flag_elide_constructors)
      && TYPE_HAS_CONSTRUCTOR (true_type) && init != void_type_node)
    {
      tree newrval;
      /* Constructors are never virtual.  */
      int flags = LOOKUP_NORMAL|LOOKUP_NONVIRTUAL;
      /* If a copy constructor might work, set things up so that we can
	 try that after this. */
      if (rval != NULL_TREE)
	flags = ~LOOKUP_COMPLAIN & (flags|LOOKUP_SPECULATIVELY);
      
      if (rval && TYPE_USES_VIRTUAL_BASECLASSES (true_type))
	{
	  init = tree_cons (NULL_TREE, integer_one_node, init);
	  flags |= LOOKUP_HAS_IN_CHARGE;
	}
      newrval = build_method_call (rval, constructor_name (true_type),
				init, NULL_TREE, flags);
      if (newrval)
	{
	  rval = newrval;
	  TREE_HAS_CONSTRUCTOR (rval) = 1;
	  goto done;
	}
      /* Didn't find the constructor, maybe it is a call to a copy constructor
	 that we should implement. */
    }

  if (rval == error_mark_node)
    return error_mark_node;
  rval = save_expr (rval);
  TREE_HAS_CONSTRUCTOR (rval) = 1;

  /* Don't call any constructors or do any initialization.  */
  if (init == void_type_node)
    goto done;

  if (TYPE_NEEDS_CONSTRUCTING (type)
      || (has_call || init))
    {
      extern tree static_aggregates;

      if (current_function_decl == NULL_TREE)
	{
	  /* In case of static initialization, SAVE_EXPR is good enough.  */
	  init = copy_to_permanent (init);
	  rval = copy_to_permanent (rval);
	  static_aggregates = perm_tree_cons (init, rval, static_aggregates);
	}
      else
	{
	  /* Have to wrap this in RTL_EXPR for two cases:
	     in base or member initialization and if we
	     are a branch of a ?: operator.  Since we
	     can't easily know the latter, just do it always.  */
	  tree xval = make_node (RTL_EXPR);

	  TREE_TYPE (xval) = TREE_TYPE (rval);
	  do_pending_stack_adjust ();
	  start_sequence ();

	  /* As a matter of principle, `start_sequence' should do this.  */
	  emit_note (0, -1);

	  if (has_array)
	    rval = expand_vec_init (decl, rval,
				    build_binary_op (MINUS_EXPR, nelts, integer_one_node, 1),
				    init, 0);
	  else
	    expand_aggr_init (build_indirect_ref (rval, 0), init, 0);

	  do_pending_stack_adjust ();

	  TREE_SIDE_EFFECTS (xval) = 1;
	  TREE_CALLS_NEW (xval) = 1;
	  RTL_EXPR_SEQUENCE (xval) = get_insns ();
	  end_sequence ();

	  if (TREE_CODE (rval) == SAVE_EXPR)
	    {
	      /* Errors may cause this to not get evaluated.  */
	      if (SAVE_EXPR_RTL (rval) == 0)
		SAVE_EXPR_RTL (rval) = const0_rtx;
	      RTL_EXPR_RTL (xval) = SAVE_EXPR_RTL (rval);
	    }
	  else
	    {
	      my_friendly_assert (TREE_CODE (rval) == VAR_DECL, 217);
	      RTL_EXPR_RTL (xval) = DECL_RTL (rval);
	    }
	  rval = xval;
	}
    }
#if 0
  /* It would seem that the above code handles this better than the code
     below. (mrs) */
  else if (has_call || init)
    {
      if (IS_AGGR_TYPE (type))
	{
	  /*  default copy constructor may be missing from the below. (mrs) */
	  error_with_aggr_type (type, "no constructor for type `%s'");
	  rval = error_mark_node;
	}
      else
	{
	  /* New 2.0 interpretation: `new int (10)' means
	     allocate an int, and initialize it with 10.  */

	  init = build_c_cast (type, init);
	  rval = build (COMPOUND_EXPR, TREE_TYPE (rval),
			build_modify_expr (build_indirect_ref (rval, 0),
					   NOP_EXPR, init),
			rval);
	  TREE_SIDE_EFFECTS (rval) = 1;
	}
    }
#endif
 done:
  if (pending_sizes)
    rval = build_compound_expr (chainon (pending_sizes,
					 build_tree_list (NULL_TREE, rval)));

  if (flag_gc)
    {
      extern tree gc_visible;
      tree objbits;
      tree update_expr;

      rval = save_expr (rval);
      /* We don't need a `headof' operation to do this because
	 we know where the object starts.  */
      objbits = build1 (INDIRECT_REF, unsigned_type_node,
			build (MINUS_EXPR, ptr_type_node,
			       rval, c_sizeof_nowarn (unsigned_type_node)));
      update_expr = build_modify_expr (objbits, BIT_IOR_EXPR, gc_visible);
      rval = build_compound_expr (tree_cons (NULL_TREE, rval,
					     tree_cons (NULL_TREE, update_expr,
							build_tree_list (NULL_TREE, rval))));
    }

  return save_expr (rval);
}

/* `expand_vec_init' performs initialization of a vector of aggregate
   types.

   DECL is passed only for error reporting, and provides line number
   and source file name information.
   BASE is the space where the vector will be.
   MAXINDEX is the maximum index of the array (one less than the
	    number of elements).
   INIT is the (possibly NULL) initializer.

   FROM_ARRAY is 0 if we should init everything with INIT
   (i.e., every element initialized from INIT).
   FROM_ARRAY is 1 if we should index into INIT in parallel
   with initialization of DECL.
   FROM_ARRAY is 2 if we should index into INIT in parallel,
   but use assignment instead of initialization.  */

tree
expand_vec_init (decl, base, maxindex, init, from_array)
     tree decl, base, maxindex, init;
     int from_array;
{
  tree rval;
  tree iterator, base2 = NULL_TREE;
  tree type = TREE_TYPE (TREE_TYPE (base));
  tree size;

  maxindex = convert (integer_type_node, maxindex);
  if (maxindex == error_mark_node)
    return error_mark_node;

  if (current_function_decl == NULL_TREE)
    {
      rval = make_tree_vec (3);
      TREE_VEC_ELT (rval, 0) = base;
      TREE_VEC_ELT (rval, 1) = maxindex;
      TREE_VEC_ELT (rval, 2) = init;
      return rval;
    }

  size = size_in_bytes (type);

  /* Set to zero in case size is <= 0.  Optimizer will delete this if
     it is not needed.  */
  rval = get_temp_regvar (TYPE_POINTER_TO (type), convert (TYPE_POINTER_TO (type),
							   null_pointer_node));
  base = default_conversion (base);
  base = convert (TYPE_POINTER_TO (type), base);
  expand_assignment (rval, base, 0, 0);
  base = get_temp_regvar (TYPE_POINTER_TO (type), base);

  if (init != NULL_TREE
      && TREE_CODE (init) == CONSTRUCTOR
      && TREE_TYPE (init) == TREE_TYPE (decl))
    {
      /* Initialization of array from {...}.  */
      tree elts = CONSTRUCTOR_ELTS (init);
      tree baseref = build1 (INDIRECT_REF, type, base);
      tree baseinc = build (PLUS_EXPR, TYPE_POINTER_TO (type), base, size);
      int host_i = TREE_INT_CST_LOW (maxindex);

      if (IS_AGGR_TYPE (type))
	{
	  while (elts)
	    {
	      host_i -= 1;
	      expand_aggr_init (baseref, TREE_VALUE (elts), 0);

	      expand_assignment (base, baseinc, 0, 0);
	      elts = TREE_CHAIN (elts);
	    }
	  /* Initialize any elements by default if possible.  */
	  if (host_i >= 0)
	    {
	      if (TYPE_NEEDS_CONSTRUCTING (type) == 0)
		{
		  if (obey_regdecls)
		    use_variable (DECL_RTL (base));
		  goto done_init;
		}

	      iterator = get_temp_regvar (integer_type_node,
					  build_int_2 (host_i, 0));
	      init = NULL_TREE;
	      goto init_by_default;
	    }
	}
      else
	while (elts)
	  {
	    expand_assignment (baseref, TREE_VALUE (elts), 0, 0);

	    expand_assignment (base, baseinc, 0, 0);
	    elts = TREE_CHAIN (elts);
	  }

      if (obey_regdecls)
	use_variable (DECL_RTL (base));
    }
  else
    {
      iterator = get_temp_regvar (integer_type_node, maxindex);

    init_by_default:

      /* If initializing one array from another,
	 initialize element by element.  */
      if (from_array)
	{
	  if (decl == NULL_TREE
	      || (init && !comptypes (TREE_TYPE (init), TREE_TYPE (decl), 1)))
	    {
	      sorry ("initialization of array from dissimilar array type");
	      return error_mark_node;
	    }
	  if (init)
	    {
	      base2 = default_conversion (init);
	      base2 = get_temp_regvar (TYPE_POINTER_TO (type), base2);
	    }
	  else if (TYPE_LANG_SPECIFIC (type)
		   && TYPE_NEEDS_CONSTRUCTING (type)
		   && ! TYPE_HAS_DEFAULT_CONSTRUCTOR (type))
	    {
	      error ("initializer ends prematurely");
	      return error_mark_node;
	    }
	}

      expand_start_cond (build (GE_EXPR, integer_type_node,
				iterator, integer_zero_node), 0);
      expand_start_loop_continue_elsewhere (1);

      if (from_array)
	{
	  tree to = build1 (INDIRECT_REF, type, base);
	  tree from;

	  if (base2)
	    from = build1 (INDIRECT_REF, type, base2);
	  else
	    from = NULL_TREE;

	  if (from_array == 2)
	    expand_expr_stmt (build_modify_expr (to, NOP_EXPR, from));
	  else if (TYPE_NEEDS_CONSTRUCTING (type))
	    expand_aggr_init (to, from, 0);
	  else if (from)
	    expand_assignment (to, from, 0, 0);
	  else my_friendly_abort (57);
	}
      else if (TREE_CODE (type) == ARRAY_TYPE)
	{
	  if (init != 0)
	    sorry ("cannot initialize multi-dimensional array with initializer");
	  expand_vec_init (decl, build1 (NOP_EXPR, TYPE_POINTER_TO (TREE_TYPE (type)), base),
			   array_type_nelts (type), 0, 0);
	}
      else
	expand_aggr_init (build1 (INDIRECT_REF, type, base), init, 0);

      expand_assignment (base,
			 build (PLUS_EXPR, TYPE_POINTER_TO (type), base, size),
			 0, 0);
      if (base2)
	expand_assignment (base2,
			   build (PLUS_EXPR, TYPE_POINTER_TO (type), base2, size), 0, 0);
      expand_loop_continue_here ();
      expand_exit_loop_if_false (0, build (NE_EXPR, integer_type_node,
					   build (PREDECREMENT_EXPR, integer_type_node, iterator, integer_one_node), minus_one));

      if (obey_regdecls)
	{
	  use_variable (DECL_RTL (base));
	  if (base2)
	    use_variable (DECL_RTL (base2));
	}
      expand_end_loop ();
      expand_end_cond ();
      if (obey_regdecls)
	use_variable (DECL_RTL (iterator));
    }
 done_init:

  if (obey_regdecls)
    use_variable (DECL_RTL (rval));
  return rval;
}

/* Free up storage of type TYPE, at address ADDR.

   TYPE is a POINTER_TYPE and can be ptr_type_node for no special type
   of pointer.

   VIRTUAL_SIZE is the ammount of storage that was allocated, and is
   used as the second argument to operator delete.  It can include
   things like padding and magic size cookies.  It has virtual in it,
   because if you have a base pointer and you delete through a virtual
   destructor, it should be the size of the dynamic object, not the
   static object, see Free Store 12.5 ANSI C++ WP.

   This does not call any destructors.  */
tree
build_x_delete (type, addr, use_global_delete, virtual_size)
     tree type, addr;
     int use_global_delete;
     tree virtual_size;
{
  tree rval;

  if (!use_global_delete
      && TYPE_LANG_SPECIFIC (TREE_TYPE (type))
      && TREE_GETS_DELETE (TREE_TYPE (type)))
    rval = build_opfncall (DELETE_EXPR, LOOKUP_NORMAL, addr, virtual_size, NULL_TREE);
  else
    rval = build_builtin_call (void_type_node, BID,
			       tree_cons (NULL_TREE, addr,
					  build_tree_list (NULL_TREE,
							   virtual_size)));
  return rval;
}

/* Objects returned by `build_new' may point to just what the user
   requested (in the case of `new X'), or they may have a cookie
   consisting of a special value (the two's complement of the pointer
   address) and the number of elements allocated (in the case of
   `new X[N]'.  In the latter case, we need to adjust the pointer
   that's passed back to the storage allocator.  */

static tree
maybe_adjust_addr_for_delete (addr)
     tree addr;
{
  tree cookie_addr;
  tree cookie;
  tree adjusted_addr, ptr_2comp;

  if (TREE_SIDE_EFFECTS (addr))
    addr = save_expr (addr);

  cookie_addr = build (MINUS_EXPR, TYPE_POINTER_TO (BI_header_type),
		       addr, BI_header_size);
  cookie = build_indirect_ref (cookie_addr, 0);

  ptr_2comp = build_component_ref (cookie, get_identifier ("ptr_2comp"), 0, 0);
  adjusted_addr = save_expr (build (MINUS_EXPR, TREE_TYPE (addr), addr, BI_header_size));

  /* We must zero out the storage here because if the memory is freed,
     then later reallocated, we might get a false positive when the
     address is reused.  */
  adjusted_addr = build_compound_expr (tree_cons (NULL_TREE,
						  build_modify_expr (ptr_2comp, NOP_EXPR, null_pointer_node),
						  build_tree_list (NULL_TREE, adjusted_addr)));

  addr = build (COND_EXPR, TREE_TYPE (addr),
		build (TRUTH_ORIF_EXPR, integer_type_node,
		       build (EQ_EXPR, integer_type_node,
			      addr, integer_zero_node),
		       build (PLUS_EXPR, integer_type_node,
			      convert (ptr_type_node, addr), ptr_2comp)),
		addr,
		adjusted_addr);
  return addr;
}

/* Generate a call to a destructor. TYPE is the type to cast ADDR to.
   ADDR is an expression which yields the store to be destroyed.
   AUTO_DELETE is nonzero if a call to DELETE should be made or not.
   If in the program, (AUTO_DELETE & 2) is non-zero, we tear down the
   virtual baseclasses.
   If in the program, (AUTO_DELETE & 1) is non-zero, then we deallocate.

   FLAGS is the logical disjunction of zero or more LOOKUP_
   flags.  See cp-tree.h for more info.

   MAYBE_ADJUST is nonzero iff we may need to adjust the address
   of the object being deleted before calling `operator delete'.
   This can happen when a user allocates an array with `operator new'
   and simply calls delete.  Ideally this is unnecessary, but there
   is much code that does `p = new char[n]; ... delete p;' and this code
   would crash otherwise.

   This function does not delete an object's virtual base classes.  */
tree
build_delete (type, addr, auto_delete, flags, use_global_delete, maybe_adjust)
     tree type, addr;
     tree auto_delete;
     int flags;
     int use_global_delete;
     int maybe_adjust;
{
  tree function, parms;
  tree member;
  tree expr;
  tree ref;
  int ptr;

  if (addr == error_mark_node)
    return error_mark_node;

  /* Can happen when CURRENT_EXCEPTION_OBJECT gets its type
     set to `error_mark_node' before it gets properly cleaned up.  */
  if (type == error_mark_node)
    return error_mark_node;

  type = TYPE_MAIN_VARIANT (type);

  if (TREE_CODE (type) == POINTER_TYPE)
    {
      type = TREE_TYPE (type);
      if (TYPE_SIZE (type) == 0)
	{
	  incomplete_type_error (0, type);
	  return error_mark_node;
	}
      if (TREE_CODE (type) == ARRAY_TYPE)
	goto handle_array;
      if (! IS_AGGR_TYPE (type))
	{
	  tree virtual_size;

	  /* This is probably wrong. It should be the size of the virtual
	     object being deleted.  */
	  virtual_size = c_sizeof_nowarn (type);

	  if (maybe_adjust)
	    addr = maybe_adjust_addr_for_delete (addr);
	  return build_builtin_call (void_type_node, BID,
				     tree_cons (NULL_TREE, addr,
						build_tree_list (NULL_TREE, virtual_size)));
	}
      if (TREE_SIDE_EFFECTS (addr))
	addr = save_expr (addr);
      ref = build_indirect_ref (addr, 0);
      ptr = 1;
    }
  else if (TREE_CODE (type) == ARRAY_TYPE)
    {
    handle_array:
      if (TREE_SIDE_EFFECTS (addr))
	addr = save_expr (addr);
      return build_vec_delete (addr, array_type_nelts (type),
			       c_sizeof_nowarn (TREE_TYPE (type)),
			       NULL_TREE, auto_delete, integer_two_node);
    }
  else
    {
      /* Don't check PROTECT here; leave that decision to the
	 destructor.  If the destructor is visible, call it,
	 else report error.  */
      addr = build_unary_op (ADDR_EXPR, addr, 0);
      if (TREE_SIDE_EFFECTS (addr))
	addr = save_expr (addr);

      if (TREE_CONSTANT (addr))
	addr = convert_pointer_to (type, addr);
      else
	addr = convert_force (build_pointer_type (type), addr);

      if (TREE_CODE (addr) == NOP_EXPR
	  && TREE_OPERAND (addr, 0) == current_class_decl)
	ref = C_C_D;
      else
	ref = build_indirect_ref (addr, 0);
      ptr = 0;
    }

  my_friendly_assert (IS_AGGR_TYPE (type), 220);

  if (! TYPE_NEEDS_DESTRUCTOR (type))
    {
      tree virtual_size;

      /* This is probably wrong. It should be the size of the virtual object
	 being deleted.  */
      virtual_size = c_sizeof_nowarn (type);

      if (auto_delete == integer_zero_node)
	return void_zero_node;
      if (maybe_adjust && addr != current_class_decl)
	addr = maybe_adjust_addr_for_delete (addr);
      if (TREE_GETS_DELETE (type) && !use_global_delete)
	return build_opfncall (DELETE_EXPR, LOOKUP_NORMAL, addr, virtual_size, NULL_TREE);
      return build_builtin_call (void_type_node, BID,
				 tree_cons (NULL_TREE, addr,
					    build_tree_list (NULL_TREE, virtual_size)));
    }
  parms = build_tree_list (NULL_TREE, addr);

  /* Below, we will reverse the order in which these calls are made.
     If we have a destructor, then that destructor will take care
     of the base classes; otherwise, we must do that here.  */
  if (TYPE_HAS_DESTRUCTOR (type))
    {
      tree dtor = DECL_MAIN_VARIANT (TREE_VEC_ELT (CLASSTYPE_METHOD_VEC (type), 0));
      tree basetypes = TYPE_BINFO (type);

      if (flags & LOOKUP_PROTECT)
	{
	  enum visibility_type visibility = compute_visibility (basetypes, dtor);

	  if (visibility == visibility_private)
	    {
	      if (flags & LOOKUP_COMPLAIN)
		error_with_aggr_type (type, "destructor for type `%s' is private in this scope");
	      return error_mark_node;
	    }
	  else if (visibility == visibility_protected
		   && (flags & LOOKUP_PROTECTED_OK) == 0)
	    {
	      if (flags & LOOKUP_COMPLAIN)
		error_with_aggr_type (type, "destructor for type `%s' is protected in this scope");
	      return error_mark_node;
	    }
	}

      /* Once we are in a destructor, try not going through
	 the virtual function table to find the next destructor.  */
      if (DECL_VINDEX (dtor)
	  && ! (flags & LOOKUP_NONVIRTUAL)
	  && TREE_CODE (auto_delete) != PARM_DECL
	  && (ptr == 1 || ! resolves_to_fixed_type_p (ref, 0)))
	{
	  /* This destructor must be called via virtual function table.  */
	  dtor = TREE_VEC_ELT (CLASSTYPE_METHOD_VEC (DECL_CONTEXT (dtor)), 0);
	  expr = convert_pointer_to (DECL_CLASS_CONTEXT (dtor), TREE_VALUE (parms));
	  if (expr != TREE_VALUE (parms))
	    {
	      expr = fold (expr);
	      ref = build_indirect_ref (expr, 0);
	      TREE_VALUE (parms) = expr;
	    }
	  function = build_vfn_ref (&TREE_VALUE (parms), ref, DECL_VINDEX (dtor));
	  if (function == error_mark_node)
	    return error_mark_node;
	  TREE_TYPE (function) = build_pointer_type (TREE_TYPE (dtor));
	  TREE_CHAIN (parms) = build_tree_list (NULL_TREE, auto_delete);
	  expr = build_function_call (function, parms);
	  if (ptr && (flags & LOOKUP_DESTRUCTOR) == 0)
	    {
	      /* Handle the case where a virtual destructor is
		 being called on an item that is 0.

		 @@ Does this really need to be done?  */
	      tree ifexp = build_binary_op(NE_EXPR, addr, integer_zero_node,1);
#if 0
	      if (TREE_CODE (ref) == VAR_DECL
		  || TREE_CODE (ref) == COMPONENT_REF)
		warning ("losing in build_delete");
#endif
	      expr = build (COND_EXPR, void_type_node,
			    ifexp, expr, void_zero_node);
	    }
	}
      else
	{
	  tree ifexp;

	  if ((flags & LOOKUP_DESTRUCTOR)
	      || TREE_CODE (ref) == VAR_DECL
	      || TREE_CODE (ref) == PARM_DECL
	      || TREE_CODE (ref) == COMPONENT_REF
	      || TREE_CODE (ref) == ARRAY_REF)
	    /* These can't be 0.  */
	    ifexp = integer_one_node;
	  else
	    /* Handle the case where a non-virtual destructor is
	       being called on an item that is 0.  */
	    ifexp = build_binary_op (NE_EXPR, addr, integer_zero_node, 1);

	  /* Used to mean that this destructor was known to be empty,
	     but that's now obsolete.  */
	  my_friendly_assert (DECL_INITIAL (dtor) != void_type_node, 221);

	  TREE_CHAIN (parms) = build_tree_list (NULL_TREE, auto_delete);
	  expr = build_function_call (dtor, parms);

	  if (ifexp != integer_one_node)
	    expr = build (COND_EXPR, void_type_node,
			  ifexp, expr, void_zero_node);
	}
      return expr;
    }
  else
    {
      /* This can get visibilities wrong.  */
      tree binfos = BINFO_BASETYPES (TYPE_BINFO (type));
      int i, n_baseclasses = binfos ? TREE_VEC_LENGTH (binfos) : 0;
      tree base_binfo = n_baseclasses > 0 ? TREE_VEC_ELT (binfos, 0) : NULL_TREE;
      tree exprstmt = NULL_TREE;
      tree parent_auto_delete = auto_delete;
      tree cond;

      /* If this type does not have a destructor, but does have
	 operator delete, call the parent parent destructor (if any),
	 but let this node do the deleting.  Otherwise, it is ok
	 to let the parent destructor do the deleting.  */
      if (TREE_GETS_DELETE (type) && !use_global_delete)
	{
	  parent_auto_delete = integer_zero_node;
	  if (auto_delete == integer_zero_node)
	    cond = NULL_TREE;
	  else
	    {
	      tree virtual_size;

	        /* This is probably wrong. It should be the size of the
		   virtual object being deleted.  */
	      virtual_size = c_sizeof_nowarn (type);

	      expr = build_opfncall (DELETE_EXPR, LOOKUP_NORMAL, addr,
				     virtual_size, NULL_TREE);
	      if (expr == error_mark_node)
		return error_mark_node;
	      if (auto_delete != integer_one_node)
		cond = build (COND_EXPR, void_type_node,
			      build (BIT_AND_EXPR, integer_type_node,
				     auto_delete, integer_one_node),
			      expr, void_zero_node);
	      else cond = expr;
	    }
	}
      else if (base_binfo == NULL_TREE
	       || (TREE_VIA_VIRTUAL (base_binfo) == 0
		   && ! TYPE_NEEDS_DESTRUCTOR (BINFO_TYPE (base_binfo))))
	{
	  tree virtual_size;

	  /* This is probably wrong. It should be the size of the virtual
	     object being deleted.  */
	  virtual_size = c_sizeof_nowarn (type);

	  cond = build (COND_EXPR, void_type_node,
			build (BIT_AND_EXPR, integer_type_node, auto_delete, integer_one_node),
			build_builtin_call (void_type_node, BID,
					    tree_cons (NULL_TREE, addr,
						       build_tree_list (NULL_TREE, virtual_size))),
			void_zero_node);
	}
      else cond = NULL_TREE;

      if (cond)
	exprstmt = build_tree_list (NULL_TREE, cond);

      if (base_binfo
	  && ! TREE_VIA_VIRTUAL (base_binfo)
	  && TYPE_NEEDS_DESTRUCTOR (BINFO_TYPE (base_binfo)))
	{
	  tree this_auto_delete;

	  if (BINFO_OFFSET_ZEROP (base_binfo))
	    this_auto_delete = parent_auto_delete;
	  else
	    this_auto_delete = integer_zero_node;

	  expr = build_delete (TYPE_POINTER_TO (BINFO_TYPE (base_binfo)), addr,
			       this_auto_delete, flags|LOOKUP_PROTECTED_OK, 0, 0);
	  exprstmt = tree_cons (NULL_TREE, expr, exprstmt);
	}

      /* Take care of the remaining baseclasses.  */
      for (i = 1; i < n_baseclasses; i++)
	{
	  base_binfo = TREE_VEC_ELT (binfos, i);
	  if (! TYPE_NEEDS_DESTRUCTOR (BINFO_TYPE (base_binfo))
	      || TREE_VIA_VIRTUAL (base_binfo))
	    continue;

	  /* May be zero offset if other baseclasses are virtual.  */
	  expr = fold (build (PLUS_EXPR, TYPE_POINTER_TO (BINFO_TYPE (base_binfo)),
			      addr, BINFO_OFFSET (base_binfo)));

	  expr = build_delete (TYPE_POINTER_TO (BINFO_TYPE (base_binfo)), expr,
			       integer_zero_node,
			       flags|LOOKUP_PROTECTED_OK, 0, 0);

	  exprstmt = tree_cons (NULL_TREE, expr, exprstmt);
	}

      for (member = TYPE_FIELDS (type); member; member = TREE_CHAIN (member))
	{
	  if (TREE_CODE (member) != FIELD_DECL)
	    continue;
	  if (TYPE_NEEDS_DESTRUCTOR (TREE_TYPE (member)))
	    {
	      tree this_member = build_component_ref (ref, DECL_NAME (member), 0, 0);
	      tree this_type = TREE_TYPE (member);
	      expr = build_delete (this_type, this_member, integer_two_node, flags, 0, 0);
	      exprstmt = tree_cons (NULL_TREE, expr, exprstmt);
	    }
	}

      if (exprstmt)
	return build_compound_expr (exprstmt);
      /* Virtual base classes make this function do nothing.  */
      return void_zero_node;
    }
}

/* For type TYPE, delete the virtual baseclass objects of DECL.  */

tree
build_vbase_delete (type, decl)
     tree type, decl;
{
  tree vbases = CLASSTYPE_VBASECLASSES (type);
  tree result = NULL_TREE;
  tree addr = build_unary_op (ADDR_EXPR, decl, 0);
  my_friendly_assert (addr != error_mark_node, 222);
  while (vbases)
    {
      tree this_addr = convert_force (TYPE_POINTER_TO (BINFO_TYPE (vbases)), addr);
      result = tree_cons (NULL_TREE,
			  build_delete (TREE_TYPE (this_addr), this_addr,
					integer_zero_node,
					LOOKUP_NORMAL|LOOKUP_DESTRUCTOR, 0, 0),
			  result);
      vbases = TREE_CHAIN (vbases);
    }
  return build_compound_expr (nreverse (result));
}

/* Build a C++ vector delete expression.
   MAXINDEX is the number of elements to be deleted.
   ELT_SIZE is the nominal size of each element in the vector.
   BASE is the expression that should yield the store to be deleted.
   DTOR_DUMMY is a placeholder for a destructor.  The library function
   __builtin_vec_delete has a pointer to function in this position.
   This function expands (or synthesizes) these calls itself.
   AUTO_DELETE_VEC says whether the container (vector) should be deallocated.
   AUTO_DELETE say whether each item in the container should be deallocated.

   This also calls delete for virtual baseclasses of elements of the vector.

   Update: MAXINDEX is no longer needed.  The size can be extracted from the
   start of the vector for pointers, and from the type for arrays.  We still
   use MAXINDEX for arrays because it happens to already have one of the
   values we'd have to extract.  (We could use MAXINDEX with pointers to
   confirm the size, and trap if the numbers differ; not clear that it'd
   be worth bothering.)  */
tree
build_vec_delete (base, maxindex, elt_size, dtor_dummy, auto_delete_vec, auto_delete)
     tree base, maxindex, elt_size;
     tree dtor_dummy;
     tree auto_delete_vec, auto_delete;
{
  tree ptype = TREE_TYPE (base);
  tree type;
  tree virtual_size;
  /* Temporary variables used by the loop.  */
  tree tbase, size_exp, tbase_init;

  /* This is the body of the loop that implements the deletion of a
     single element, and moves temp variables to next elements.  */
  tree body;

  /* This is the LOOP_EXPR that governs the deletion of the elements.  */
  tree loop;

  /* This is the thing that governs what to do after the loop has run.  */
  tree deallocate_expr = 0;

  /* This is the BIND_EXPR which holds the outermost iterator of the
     loop.  It is convenient to set this variable up and test it before
     executing any other code in the loop.
     This is also the containing expression returned by this function.  */
  tree controller = NULL_TREE;

  /* This is the BLOCK to record the symbol binding for debugging.  */
  tree block;

  base = stabilize_reference (base);

  /* Since we can use base many times, save_epr it. */
  if (TREE_SIDE_EFFECTS (base))
    base = save_expr (base);

  if (TREE_CODE (ptype) == POINTER_TYPE)
    {
      /* Step back one from start of vector, and read dimension.  */
      tree cookie_addr = build (MINUS_EXPR, TYPE_POINTER_TO (BI_header_type),
				base, BI_header_size);
      tree cookie = build_indirect_ref (cookie_addr, 0);
      maxindex = build_component_ref (cookie, get_identifier ("nelts"), 0, 0);
      do
	ptype = TREE_TYPE (ptype);
      while (TREE_CODE (ptype) == ARRAY_TYPE);
    }
  else if (TREE_CODE (ptype) == ARRAY_TYPE)
    {
      /* get the total number of things in the array, maxindex is a bad name */
      maxindex = array_type_nelts_total (ptype);
      while (TREE_CODE (ptype) == ARRAY_TYPE)
	ptype = TREE_TYPE (ptype);
      base = build_unary_op (ADDR_EXPR, base, 1);
    }
  else
    {
      error ("type to vector delete is neither pointer or array type");
      return error_mark_node;
    }
  type = ptype;
  ptype = TYPE_POINTER_TO (type);

  size_exp = size_in_bytes (type);

  if (! IS_AGGR_TYPE (type) || ! TYPE_NEEDS_DESTRUCTOR (type))
    {
      loop = integer_zero_node;
      goto no_destructor;
    }

  /* The below is short by BI_header_size */
  virtual_size = fold (size_binop (MULT_EXPR, size_exp, maxindex));

  tbase = build_decl (VAR_DECL, NULL_TREE, ptype);
  tbase_init = build_modify_expr (tbase, NOP_EXPR,
				  fold (build (PLUS_EXPR, ptype,
					       base,
					       virtual_size)));
  DECL_REGISTER (tbase) = 1;
  controller = build (BIND_EXPR, void_type_node, tbase, 0, 0);
  TREE_SIDE_EFFECTS (controller) = 1;
  block = build_block (tbase, 0, 0, 0, 0);
  add_block_current_level (block);

  if (auto_delete != integer_zero_node
      && auto_delete != integer_two_node)
    {
      tree base_tbd = convert (ptype,
			       build_binary_op (MINUS_EXPR,
						convert (ptr_type_node, base),
						BI_header_size,
						1));
      /* This is the real size */
      virtual_size = size_binop (PLUS_EXPR, virtual_size, BI_header_size);
      body = build_tree_list (NULL_TREE,
			      build_x_delete (ptr_type_node, base_tbd, 0,
					      virtual_size));
      body = build (COND_EXPR, void_type_node,
		    build (BIT_AND_EXPR, integer_type_node,
			   auto_delete, integer_one_node),
		    body, integer_zero_node);
    }
  else
    body = NULL_TREE;

  body = tree_cons (NULL_TREE,
		    build_delete (ptype, tbase, auto_delete,
				  LOOKUP_NORMAL|LOOKUP_DESTRUCTOR, 0, 0),
		    body);

  body = tree_cons (NULL_TREE,
		    build_modify_expr (tbase, NOP_EXPR, build (MINUS_EXPR, ptype, tbase, size_exp)),
		    body);

  body = tree_cons (NULL_TREE,
		    build (EXIT_EXPR, void_type_node,
			   build (EQ_EXPR, integer_type_node, base, tbase)),
		    body);

  loop = build (LOOP_EXPR, void_type_node, build_compound_expr (body));

  loop = tree_cons (NULL_TREE, tbase_init,
		    tree_cons (NULL_TREE, loop, NULL_TREE));
  loop = build_compound_expr (loop);

 no_destructor:
  /* If the delete flag is one, or anything else with the low bit set,
     delete the storage.  */
  if (auto_delete_vec == integer_zero_node
      || auto_delete_vec == integer_two_node)
    deallocate_expr = integer_zero_node;
  else
    {
      tree base_tbd;

      /* The below is short by BI_header_size */
      virtual_size = fold (size_binop (MULT_EXPR, size_exp, maxindex));

      if (loop == integer_zero_node)
	/* no header */
	base_tbd = base;
      else
	{
	  base_tbd = convert (ptype,
			      build_binary_op (MINUS_EXPR,
					       convert (string_type_node, base),
					       BI_header_size,
					       1));
	  /* True size with header. */
	  virtual_size = size_binop (PLUS_EXPR, virtual_size, BI_header_size);
	}
      deallocate_expr = build_x_delete (ptr_type_node, base_tbd, 1,
					virtual_size);
      if (auto_delete_vec != integer_one_node)
	deallocate_expr = build (COND_EXPR, void_type_node,
				 build (BIT_AND_EXPR, integer_type_node,
					auto_delete_vec, integer_one_node),
				 deallocate_expr, integer_zero_node);
    }

  if (loop && deallocate_expr != integer_zero_node)
    {
      body = tree_cons (NULL_TREE, loop,
			tree_cons (NULL_TREE, deallocate_expr, NULL_TREE));
      body = build_compound_expr (body);
    }
  else
    body = loop;

  /* Outermost wrapper: If pointer is null, punt.  */
  body = build (COND_EXPR, void_type_node,
		build (NE_EXPR, integer_type_node, base, integer_zero_node),
		body, integer_zero_node);

  if (controller)
    {
      TREE_OPERAND (controller, 1) = body;
      return controller;
    }
  else
    return convert (void_type_node, body);
}
