/* Handle parameterized types (templates) for GNU C++.
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.
   Written by Ken Raeburn (raeburn@cygnus.com) while at Watchmaker Computing.

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

/* Known bugs or deficiencies include:
   * templates for class static data don't work (methods only)
   * duplicated method templates can crash the compiler
   * interface/impl data is taken from file defining the template
   * all methods must be provided in header files; can't use a source
     file that contains only the method templates and "just win"
   * method templates must be seen before the expansion of the
     class template is done
 */

#include "config.h"
#include <stdio.h>
#include "obstack.h"

#include "tree.h"
#include "cp-tree.h"
#include "cp-decl.h"
#include "cp-parse.h"

extern struct obstack permanent_obstack;
extern tree grokdeclarator ();

extern int lineno;
extern char *input_filename;
struct pending_inline *pending_template_expansions;

int processing_template_decl;
int processing_template_defn;

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

static int unify ();
static void add_pending_template ();

void overload_template_name (), pop_template_decls ();

/* We've got a template header coming up; set obstacks up to save the
   nodes created permanently.  (There might be cases with nested templates
   where we don't have to do this, but they aren't implemented, and it
   probably wouldn't be worth the effort.)  */
void
begin_template_parm_list ()
{
  pushlevel (0);
  push_obstacks (&permanent_obstack, &permanent_obstack);
}

/* Process information from new template parameter NEXT and append it to the
   LIST being built.  The rules for use of a template parameter type name
   by later parameters are not well-defined for us just yet.  However, the
   only way to avoid having to parse expressions of unknown complexity (and
   with tokens of unknown types) is to disallow it completely.	So for now,
   that is what is assumed.  */
tree
process_template_parm (list, next)
     tree list, next;
{
  tree parm;
  int is_type;
  parm = next;
  my_friendly_assert (TREE_CODE (parm) == TREE_LIST, 259);
  is_type = TREE_CODE (TREE_PURPOSE (parm)) == IDENTIFIER_NODE;
  if (!is_type)
    {
      parm = TREE_PURPOSE (parm);
      my_friendly_assert (TREE_CODE (parm) == TREE_LIST, 260);
      parm = TREE_VALUE (parm);
      /* is a const-param */
      parm = grokdeclarator (TREE_VALUE (next), TREE_PURPOSE (next),
			     NORMAL, 0, NULL_TREE);
      /* A template parameter is not modifiable.  */
      TREE_READONLY (parm) = 1;
      if (TREE_CODE (TREE_TYPE (parm)) == RECORD_TYPE
	  || TREE_CODE (TREE_TYPE (parm)) == UNION_TYPE)
	{
	  sorry ("aggregate template parameter types");
	  TREE_TYPE (parm) = void_type_node;
	}
    }
  return chainon (list, parm);
}

/* The end of a template parameter list has been reached.  Process the
   tree list into a parameter vector, converting each parameter into a more
   useful form.	 Type parameters are saved as IDENTIFIER_NODEs, and others
   as PARM_DECLs.  */

tree
end_template_parm_list (parms)
     tree parms;
{
  int nparms = 0;
  tree saved_parmlist;
  tree parm;
  for (parm = parms; parm; parm = TREE_CHAIN (parm))
    nparms++;
  saved_parmlist = make_tree_vec (nparms);

  pushlevel (0);

  for (parm = parms, nparms = 0; parm; parm = TREE_CHAIN (parm), nparms++)
    {
      tree p = parm, decl;
      if (TREE_CODE (p) == TREE_LIST)
	{
	  tree t;
	  p = TREE_PURPOSE (p);
	  my_friendly_assert (TREE_CODE (p) == IDENTIFIER_NODE, 261);
	  t = make_node (TEMPLATE_TYPE_PARM);
	  TEMPLATE_TYPE_SET_INFO (t, saved_parmlist, nparms);
	  decl = build_lang_decl (TYPE_DECL, p, t);
	  TYPE_NAME (t) = decl;
	}
      else
	{
	  tree tinfo = make_node (TEMPLATE_CONST_PARM);
	  my_friendly_assert (TREE_PERMANENT (tinfo), 262);
	  if (!TREE_PERMANENT (p))
	    {
	      tree old_p = p;
	      TREE_PERMANENT (old_p) = 1;
	      p = copy_node (p);
	      TREE_PERMANENT (old_p) = 0;
	    }
	  TEMPLATE_CONST_SET_INFO (tinfo, saved_parmlist, nparms);
	  TREE_TYPE (tinfo) = TREE_TYPE (p);
	  decl = build_decl (CONST_DECL, DECL_NAME (p), TREE_TYPE (p));
	  DECL_INITIAL (decl) = tinfo;
	}
      TREE_VEC_ELT (saved_parmlist, nparms) = p;
      pushdecl (decl);
    }
  set_current_level_tags_transparency (1);
  processing_template_decl++;
  return saved_parmlist;
}

/* end_template_decl is called after a template declaration is seen.
   D1 is template header; D2 is class_head_sans_basetype or a
   TEMPLATE_DECL with its DECL_RESULT field set.  */
void
end_template_decl (d1, d2, is_class)
     tree d1, d2, is_class;
{
  tree decl;
  struct template_info *tmpl;

  tmpl = (struct template_info *) obstack_alloc (&permanent_obstack,
					    sizeof (struct template_info));
  tmpl->text = 0;
  tmpl->length = 0;
  tmpl->aggr = is_class;

  /* cloned from reinit_parse_for_template */
  tmpl->filename = input_filename;
  tmpl->lineno = lineno;
  tmpl->parm_vec = d1;          /* [eichin:19911015.2306EST] */

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "end_template_decl");
  debug_bindings_indentation += 4;
#endif

  if (d2 == NULL_TREE || d2 == error_mark_node)
    {
      decl = 0;
      goto lose;
    }

  if (is_class)
    {
      decl = build_lang_decl (TEMPLATE_DECL, d2, NULL_TREE);
    }
  else
    {
      if (TREE_CODE (d2) == TEMPLATE_DECL)
	decl = d2;
      else
	{
	  /* Class destructor templates and operator templates are
	     slipping past as non-template nodes.  Process them here, since
	     I haven't figured out where to catch them earlier.  I could
	     go do that, but it's a choice between getting that done and
	     staying only N months behind schedule.  Sorry....  */
	  enum tree_code code;
	  my_friendly_assert (TREE_CODE (d2) == CALL_EXPR, 263);
	  code = TREE_CODE (TREE_OPERAND (d2, 0));
	  my_friendly_assert (code == BIT_NOT_EXPR
		  || code == OP_IDENTIFIER
		  || code == SCOPE_REF, 264);
	  d2 = grokdeclarator (d2, NULL_TREE, MEMFUNCDEF, 0, NULL_TREE);
	  decl = build_lang_decl (TEMPLATE_DECL, DECL_NAME (d2),
				  TREE_TYPE (d2));
	  DECL_TEMPLATE_RESULT (decl) = d2;
	  DECL_CONTEXT (decl) = DECL_CONTEXT (d2);
	  DECL_CLASS_CONTEXT (decl) = DECL_CLASS_CONTEXT (d2);
	  DECL_NAME (decl) = DECL_NAME (d2);
	  TREE_TYPE (decl) = TREE_TYPE (d2);
	  TREE_PUBLIC (decl) = TREE_PUBLIC (d2) = 0;
	  DECL_EXTERNAL (decl) = (DECL_EXTERNAL (d2)
				  && !(DECL_CLASS_CONTEXT (d2)
				       && !DECL_THIS_EXTERN (d2)));
	}

      /* All routines creating TEMPLATE_DECL nodes should now be using
	 build_lang_decl, which will have set this up already.	*/
      my_friendly_assert (DECL_LANG_SPECIFIC (decl) != 0, 265);

      /* @@ Somewhere, permanent allocation isn't being used.  */
      if (! DECL_TEMPLATE_IS_CLASS (decl)
	  && TREE_CODE (DECL_TEMPLATE_RESULT (decl)) == FUNCTION_DECL)
	{
	  tree result = DECL_TEMPLATE_RESULT (decl);
	  /* Will do nothing if allocation was already permanent.  */
	  DECL_ARGUMENTS (result) = copy_to_permanent (DECL_ARGUMENTS (result));
	}

      /* If this is for a method, there's an extra binding level here.	*/
      if (! DECL_TEMPLATE_IS_CLASS (decl)
	  && DECL_CONTEXT (DECL_TEMPLATE_RESULT (decl)) != NULL_TREE)
	{
	  /* @@ Find out where this should be getting set!  */
	  tree r = DECL_TEMPLATE_RESULT (decl);
	  if (DECL_CLASS_CONTEXT (r) == NULL_TREE)
	    DECL_CLASS_CONTEXT (r) = DECL_CONTEXT (r);
	}
    }
  DECL_TEMPLATE_INFO (decl) = tmpl;
  DECL_TEMPLATE_PARMS (decl) = d1;
lose:
  if (decl)
    {
      /* If context of decl is non-null (i.e., method template), add it
	 to the appropriate class template, and pop the binding levels.  */
      if (! DECL_TEMPLATE_IS_CLASS (decl)
	  && DECL_CONTEXT (DECL_TEMPLATE_RESULT (decl)) != NULL_TREE)
	{
	  tree ctx = DECL_CONTEXT (DECL_TEMPLATE_RESULT (decl));
	  tree tmpl;
	  my_friendly_assert (TREE_CODE (ctx) == UNINSTANTIATED_P_TYPE, 266);
	  tmpl = UPT_TEMPLATE (ctx);
	  DECL_TEMPLATE_MEMBERS (tmpl) =
	    perm_tree_cons (DECL_NAME (decl), decl,
			    DECL_TEMPLATE_MEMBERS (tmpl));
	  poplevel (0, 0, 0);
	  poplevel (0, 0, 0);
	}
      /* Otherwise, go back to top level first, and push the template decl
	 again there.  */
      else
	{
	  poplevel (0, 0, 0);
	  poplevel (0, 0, 0);
	  if (TREE_TYPE (decl)
	      && IDENTIFIER_GLOBAL_VALUE (DECL_NAME (decl)) != NULL_TREE)
	    push_overloaded_decl (decl, 0);
	  else
	    pushdecl (decl);
	}
    }
#if 0 /* It happens sometimes, with syntactic or semantic errors.

	 One specific case:
	 template <class A, int X, int Y> class Foo { ... };
	 template <class A, int X, int y> Foo<X,Y>::method (Foo& x) { ... }
	 Note the missing "A" in the class containing "method".  */
  my_friendly_assert (global_bindings_p (), 267);
#else
  while (! global_bindings_p ())
    poplevel (0, 0, 0);
#endif
  pop_obstacks ();
  processing_template_decl--;
  (void) get_pending_sizes ();
#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif
}


/* Convert all template arguments to their appropriate types, and return
   a vector containing the resulting values.  If any error occurs, return
   error_mark_node.  */
static tree
coerce_template_parms (parms, arglist, in_decl)
     tree parms, arglist;
     tree in_decl;
{
  int nparms, i, lost = 0;
  tree vec;

  if (TREE_CODE (arglist) == TREE_VEC)
    nparms = TREE_VEC_LENGTH (arglist);
  else
    nparms = list_length (arglist);
  if (nparms != TREE_VEC_LENGTH (parms))
    {
      error ("incorrect number of parameters (%d, should be %d)",
	     nparms, TREE_VEC_LENGTH (parms));
      if (in_decl)
	error_with_decl (in_decl, "in template expansion for decl `%s'");
      return error_mark_node;
    }

  if (TREE_CODE (arglist) == TREE_VEC)
    vec = copy_node (arglist);
  else
    {
      vec = make_tree_vec (nparms);
      for (i = 0; i < nparms; i++)
	{
	  tree arg = arglist;
	  arglist = TREE_CHAIN (arglist);
	  if (arg == error_mark_node)
	    lost++;
	  else
	    arg = TREE_VALUE (arg);
	  TREE_VEC_ELT (vec, i) = arg;
	}
    }
  for (i = 0; i < nparms; i++)
    {
      tree arg = TREE_VEC_ELT (vec, i);
      tree parm = TREE_VEC_ELT (parms, i);
      tree val;
      int is_type, requires_type;

      is_type = TREE_CODE_CLASS (TREE_CODE (arg)) == 't';
      requires_type = TREE_CODE (parm) == IDENTIFIER_NODE;
      if (is_type != requires_type)
	{
	  if (in_decl)
	    error_with_decl (in_decl,
			     "type/value mismatch in template parameter list for `%s'");
	  lost++;
	  TREE_VEC_ELT (vec, i) = error_mark_node;
	  continue;
	}
      if (is_type)
	val = groktypename (arg);
      else
	val = digest_init (TREE_TYPE (parm), arg, (tree *) 0);

      if (val == error_mark_node)
	lost++;

      TREE_VEC_ELT (vec, i) = val;
    }
  if (lost)
    return error_mark_node;
  return vec;
}

/* Given class template name and parameter list, produce a user-friendly name
   for the instantiation.  Note that this name isn't necessarily valid as
   input to the compiler, because ">" characters may be adjacent.  */
static char *
mangle_class_name_for_template (name, parms, arglist)
     char *name;
     tree parms, arglist;
{
  static struct obstack scratch_obstack;
  static char *scratch_firstobj;
  int i, nparms;
  char ibuf[100];

  if (!scratch_firstobj)
    {
      gcc_obstack_init (&scratch_obstack);
      scratch_firstobj = obstack_alloc (&scratch_obstack, 1);
    }
  else
    obstack_free (&scratch_obstack, scratch_firstobj);

#if 0
#define buflen	sizeof(buf)
#define check	if (bufp >= buf+buflen-1) goto too_long
#define ccat(c) *bufp++=(c); check
#define advance	bufp+=strlen(bufp); check
#define cat(s)	strncpy(bufp, s, buf+buflen-bufp-1); advance
#else
#define check
#define ccat(c)	obstack_1grow (&scratch_obstack, (c));
#define advance
#define cat(s)	obstack_grow (&scratch_obstack, (s), strlen (s))
#endif
#define icat(n)	sprintf(ibuf,"%d",(n)); cat(ibuf)
#define xcat(n)	sprintf(ibuf,"%ux",n); cat(ibuf)

  cat (name);
  ccat ('<');
  nparms = TREE_VEC_LENGTH (parms);
  my_friendly_assert (nparms == TREE_VEC_LENGTH (arglist), 268);
  for (i = 0; i < nparms; i++)
    {
      tree parm = TREE_VEC_ELT (parms, i), arg = TREE_VEC_ELT (arglist, i);
      tree type, id;

      if (i)
	ccat (',');

      if (TREE_CODE (parm) == IDENTIFIER_NODE)
	{
	  /* parm is a type */
	  char *typename;

	  if (TYPE_NAME (arg)
	      && (TREE_CODE (arg) == RECORD_TYPE
		  || TREE_CODE (arg) == UNION_TYPE
		  || TREE_CODE (arg) == ENUMERAL_TYPE)
	      && TYPE_IDENTIFIER (arg)
	      && IDENTIFIER_POINTER (TYPE_IDENTIFIER (arg)))
	    typename = IDENTIFIER_POINTER (TYPE_IDENTIFIER (arg));
	  else
	    typename = type_as_string (arg);
	  cat (typename);
	  continue;
	}
      else
	my_friendly_assert (TREE_CODE (parm) == PARM_DECL, 269);

      /* Should do conversions as for "const" initializers.  */
      type = TREE_TYPE (parm);
      id = DECL_NAME (parm);
	
      if (TREE_CODE (arg) == TREE_LIST)
	{
	  /* New list cell was built because old chain link was in
	     use.  */
	  my_friendly_assert (TREE_PURPOSE (arg) == NULL_TREE, 270);
	  arg = TREE_VALUE (arg);
	}
      
      switch (TREE_CODE (type))
	{
	case INTEGER_TYPE:
	case ENUMERAL_TYPE:
	  if (TREE_CODE (arg) == INTEGER_CST)
	    {
	      if (TREE_INT_CST_HIGH (arg)
		  != (TREE_INT_CST_LOW (arg) >> (HOST_BITS_PER_WIDE_INT - 1)))
		{
		  tree val = arg;
		  if (TREE_INT_CST_HIGH (val) < 0)
		    {
		      ccat ('-');
		      val = build_int_2 (~TREE_INT_CST_LOW (val),
					 -TREE_INT_CST_HIGH (val));
		    }
		  /* Would "%x%0*x" or "%x%*0x" get zero-padding on all
		     systems?  */
		  {
		    static char format[10]; /* "%x%09999x\0" */
		    if (!format[0])
		      sprintf (format, "%%x%%0%dx", HOST_BITS_PER_INT / 4);
		    sprintf (ibuf, format, TREE_INT_CST_HIGH (val),
			     TREE_INT_CST_LOW (val));
		    cat (ibuf);
		  }
		}
	      else
		icat (TREE_INT_CST_LOW (arg));
	    }
	  else
	    {
	      error ("invalid integer constant for template parameter");
	      cat ("*error*");
	    }
	  break;
#ifndef REAL_IS_NOT_DOUBLE
	case REAL_TYPE:
	  sprintf (ibuf, "%e", TREE_REAL_CST (arg));
	  cat (ibuf);
	  break;
#endif
	case POINTER_TYPE:
	  if (TREE_CODE (arg) != ADDR_EXPR)
	    {
	      error ("invalid pointer constant for template parameter");
	      cat ("*error*");
	      break;
	    }
	  ccat ('&');
	  arg = TREE_OPERAND (arg, 0);
	  if (TREE_CODE (arg) == FUNCTION_DECL)
	    cat (fndecl_as_string (0, arg, 0));
	  else
	    {
	      my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (arg)) == 'd',
				  271);
	      cat (IDENTIFIER_POINTER (DECL_NAME (arg)));
	    }
	  break;
	default:
	  sorry ("encoding %s as template parm",
		 tree_code_name [(int) TREE_CODE (type)]);
	  my_friendly_abort (81);
	}
    }
  {
    char *bufp = obstack_next_free (&scratch_obstack);
    int offset = 0;
    while (bufp[offset - 1] == ' ')
      offset--;
    obstack_blank_fast (&scratch_obstack, offset);
  }
  ccat ('>');
  ccat ('\0');
  return (char *) obstack_base (&scratch_obstack);

 too_long:
  fatal ("out of (preallocated) string space creating template instantiation name");
  /* NOTREACHED */
  return NULL;
}

/* Given an IDENTIFIER_NODE (type TEMPLATE_DECL) and a chain of
   parameters, find the desired type.

   D1 is the PTYPENAME terminal, and ARGLIST is the list of arguments.
   Since ARGLIST is build on the decl_obstack, we must copy it here
   to keep it from being reclaimed when the decl storage is reclaimed.

   IN_DECL, if non-NULL, is the template declaration we are trying to
   instantiate.  */
tree
lookup_template_class (d1, arglist, in_decl)
     tree d1, arglist;
     tree in_decl;
{
  tree template, parmlist;
  char *mangled_name;
  tree id;

  my_friendly_assert (TREE_CODE (d1) == IDENTIFIER_NODE, 272);
  template = IDENTIFIER_GLOBAL_VALUE (d1); /* XXX */
  if (! template)
    template = IDENTIFIER_CLASS_VALUE (d1);
  /* With something like `template <class T> class X class X { ... };'
     we could end up with D1 having nothing but an IDENTIFIER_LOCAL_VALUE.
     We don't want to do that, but we have to deal with the situation, so
     let's give them some syntax errors to chew on instead of a crash.  */
  if (! template)
    return error_mark_node;
  if (TREE_CODE (template) != TEMPLATE_DECL)
    {
      error ("non-template type '%s' used as a template",
	     IDENTIFIER_POINTER (d1));
      if (in_decl)
	error_with_decl (in_decl, "for template declaration `%s'");
      return error_mark_node;
    }
  parmlist = DECL_TEMPLATE_PARMS (template);

  arglist = coerce_template_parms (parmlist, arglist, in_decl);
  if (arglist == error_mark_node)
    return error_mark_node;
  if (uses_template_parms (arglist))
    {
      tree t = make_lang_type (UNINSTANTIATED_P_TYPE);
      tree d;
      id = make_anon_name ();
      d = build_lang_decl (TYPE_DECL, id, t);
      TYPE_NAME (t) = d;
      TYPE_VALUES (t) = build_tree_list (template, arglist);
      pushdecl_top_level (d);
    }
  else
    {
      mangled_name = mangle_class_name_for_template (IDENTIFIER_POINTER (d1),
						     parmlist, arglist);
      id = get_identifier (mangled_name);
    }
  if (!IDENTIFIER_TEMPLATE (id))
    {
      arglist = copy_to_permanent (arglist);
      IDENTIFIER_TEMPLATE (id) = perm_tree_cons (template, arglist, NULL_TREE);
    }
  return id;
}

void
push_template_decls (parmlist, arglist, class_level)
     tree parmlist, arglist;
     int class_level;
{
  int i, nparms;

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "push_template_decls");
  debug_bindings_indentation += 4;
#endif

  /* Don't want to push values into global context.  */
  if (!class_level)
    pushlevel (0);
  nparms = TREE_VEC_LENGTH (parmlist);

  for (i = 0; i < nparms; i++)
    {
      int requires_type, is_type;
      tree parm = TREE_VEC_ELT (parmlist, i);
      tree arg = TREE_VEC_ELT (arglist, i);
      tree decl = 0;

      requires_type = TREE_CODE (parm) == IDENTIFIER_NODE;
      is_type = TREE_CODE_CLASS (TREE_CODE (arg)) == 't';
      if (is_type)
	{
	  /* add typename to namespace */
	  if (!requires_type)
	    {
	      error ("template use error: type provided where value needed");
	      continue;
	    }
	  decl = arg;
	  my_friendly_assert (TREE_CODE_CLASS (TREE_CODE (decl)) == 't', 273);
	  decl = build_lang_decl (TYPE_DECL, parm, decl);
	}
      else
	{
	  /* add const decl to namespace */
	  tree val;
	  if (requires_type)
	    {
	      error ("template use error: value provided where type needed");
	      continue;
	    }
	  val = digest_init (TREE_TYPE (parm), arg, (tree *) 0);
	  if (val != error_mark_node)
	    {
	      decl = build_decl (VAR_DECL, DECL_NAME (parm), TREE_TYPE (parm));
	      DECL_INITIAL (decl) = val;
	      TREE_READONLY (decl) = 1;
	    }
	}
      if (decl != 0)
	{
	  layout_decl (decl, 0);
	  if (class_level)
	    pushdecl_class_level (decl);
	  else
	    pushdecl (decl);
	}
    }
  if (!class_level)
    set_current_level_tags_transparency (1);
#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif
}

void
pop_template_decls (parmlist, arglist, class_level)
     tree parmlist, arglist;
     int class_level;
{
#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "pop_template_decls");
  debug_bindings_indentation += 4;
#endif

  if (!class_level)
    poplevel (0, 0, 0);

#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif
}

/* Should be defined in cp-parse.h.  */
extern int yychar;

int
uses_template_parms (t)
     tree t;
{
  if (!t)
    return 0;
  switch (TREE_CODE (t))
    {
    case INDIRECT_REF:
    case COMPONENT_REF:
      /* We assume that the object must be instantiated in order to build
	 the COMPONENT_REF, so we test only whether the type of the
	 COMPONENT_REF uses template parms.  */
      return uses_template_parms (TREE_TYPE (t));

    case IDENTIFIER_NODE:
      if (!IDENTIFIER_TEMPLATE (t))
	return 0;
      return uses_template_parms (TREE_VALUE (IDENTIFIER_TEMPLATE (t)));

      /* aggregates of tree nodes */
    case TREE_VEC:
      {
	int i = TREE_VEC_LENGTH (t);
	while (i--)
	  if (uses_template_parms (TREE_VEC_ELT (t, i)))
	    return 1;
	return 0;
      }
    case TREE_LIST:
      if (uses_template_parms (TREE_PURPOSE (t))
	  || uses_template_parms (TREE_VALUE (t)))
	return 1;
      return uses_template_parms (TREE_CHAIN (t));

      /* constructed type nodes */
    case POINTER_TYPE:
    case REFERENCE_TYPE:
      return uses_template_parms (TREE_TYPE (t));
    case RECORD_TYPE:
    case UNION_TYPE:
      if (!TYPE_NAME (t))
	return 0;
      if (!TYPE_IDENTIFIER (t))
	return 0;
      return uses_template_parms (TYPE_IDENTIFIER (t));
    case FUNCTION_TYPE:
      if (uses_template_parms (TYPE_ARG_TYPES (t)))
	return 1;
      return uses_template_parms (TREE_TYPE (t));
    case ARRAY_TYPE:
      if (uses_template_parms (TYPE_DOMAIN (t)))
	return 1;
      return uses_template_parms (TREE_TYPE (t));
    case OFFSET_TYPE:
      if (uses_template_parms (TYPE_OFFSET_BASETYPE (t)))
	return 1;
      return uses_template_parms (TREE_TYPE (t));
    case METHOD_TYPE:
      if (uses_template_parms (TYPE_OFFSET_BASETYPE (t)))
	return 1;
      if (uses_template_parms (TYPE_ARG_TYPES (t)))
	return 1;
      return uses_template_parms (TREE_TYPE (t));

      /* decl nodes */
    case TYPE_DECL:
      return uses_template_parms (DECL_NAME (t));
    case FUNCTION_DECL:
      if (uses_template_parms (TREE_TYPE (t)))
	return 1;
      /* fall through */
    case VAR_DECL:
    case PARM_DECL:
      /* ??? What about FIELD_DECLs?  */
      /* The type of a decl can't use template parms if the name of the
	 variable doesn't, because it's impossible to resolve them.  So
	 ignore the type field for now.	 */
      if (DECL_CONTEXT (t) && uses_template_parms (DECL_CONTEXT (t)))
	return 1;
      if (uses_template_parms (TREE_TYPE (t)))
	{
	  error ("template parms used where they can't be resolved");
	}
      return 0;

    case CALL_EXPR:
      return uses_template_parms (TREE_TYPE (t));
    case ADDR_EXPR:
      return uses_template_parms (TREE_OPERAND (t, 0));

      /* template parm nodes */
    case TEMPLATE_TYPE_PARM:
    case TEMPLATE_CONST_PARM:
      return 1;

      /* simple type nodes */
    case INTEGER_TYPE:
      if (uses_template_parms (TYPE_MIN_VALUE (t)))
	return 1;
      return uses_template_parms (TYPE_MAX_VALUE (t));

    case REAL_TYPE:
    case VOID_TYPE:
    case ENUMERAL_TYPE:
      return 0;

      /* constants */
    case INTEGER_CST:
    case REAL_CST:
    case STRING_CST:
      return 0;

    case ERROR_MARK:
      /* Non-error_mark_node ERROR_MARKs are bad things.  */
      my_friendly_assert (t == error_mark_node, 274);
      /* NOTREACHED */
      return 0;

    case UNINSTANTIATED_P_TYPE:
      return 1;

    default:
      switch (TREE_CODE_CLASS (TREE_CODE (t)))
	{
	case '1':
	case '2':
	case '3':
	case '<':
	  {
	    int i;
	    for (i = tree_code_length[(int) TREE_CODE (t)]; --i >= 0;)
	      if (uses_template_parms (TREE_OPERAND (t, i)))
		return 1;
	    return 0;
	  }
	default:
	  break;
	}
      sorry ("testing %s for template parms",
	     tree_code_name [(int) TREE_CODE (t)]);
      my_friendly_abort (82);
      /* NOTREACHED */
      return 0;
    }
}

void
instantiate_member_templates (arg)
     tree arg;
{
  tree t;
  tree classname = TREE_VALUE (arg);
  tree id = classname;
  tree members = DECL_TEMPLATE_MEMBERS (TREE_PURPOSE (IDENTIFIER_TEMPLATE (id)));

  for (t = members; t; t = TREE_CHAIN (t))
    {
      tree parmvec, type, classparms, tdecl, t2;
      int nparms, xxx, i;

      my_friendly_assert (TREE_VALUE (t) != NULL_TREE, 275);
      my_friendly_assert (TREE_CODE (TREE_VALUE (t)) == TEMPLATE_DECL, 276);
      /* @@ Should verify that class parm list is a list of
	 distinct template parameters, and covers all the template
	 parameters.  */
      tdecl = TREE_VALUE (t);
      type = DECL_CONTEXT (DECL_TEMPLATE_RESULT (tdecl));
      classparms = UPT_PARMS (type);
      nparms = TREE_VEC_LENGTH (classparms);
      parmvec = make_tree_vec (nparms);
      for (i = 0; i < nparms; i++)
	TREE_VEC_ELT (parmvec, i) = NULL_TREE;
      switch (unify (DECL_TEMPLATE_PARMS (tdecl),
		     &TREE_VEC_ELT (parmvec, 0), nparms,
		     type, IDENTIFIER_TYPE_VALUE (classname),
		     &xxx))
	{
	case 0:
	  /* Success -- well, no inconsistency, at least.  */
	  for (i = 0; i < nparms; i++)
	    if (TREE_VEC_ELT (parmvec, i) == NULL_TREE)
	      goto failure;
	  t2 = instantiate_template (tdecl,
				     &TREE_VEC_ELT (parmvec, 0));
	  type = IDENTIFIER_TYPE_VALUE (id);
	  my_friendly_assert (type != 0, 277);
	  if (CLASSTYPE_INTERFACE_UNKNOWN (type))
	    {
	      DECL_EXTERNAL (t2) = 0;
	      TREE_PUBLIC (t2) = 0;
	    }
	  else
	    {
	      DECL_EXTERNAL (t2) = CLASSTYPE_INTERFACE_ONLY (type);
	      TREE_PUBLIC (t2) = 1;
	    }
	  break;
	case 1:
	  /* Failure.  */
	failure:
	  error ("type unification error instantiating %s::%s",
		 IDENTIFIER_POINTER (classname),
		 IDENTIFIER_POINTER (DECL_NAME (tdecl)));
	  error_with_decl (tdecl, "for template declaration `%s'");

	  continue /* loop of members */;
	default:
	  /* Eek, a bug.  */
	  my_friendly_abort (83);
	}
    }
}

tree
instantiate_class_template (classname, setup_parse)
     tree classname;
     int setup_parse;
{
  struct template_info *template_info;
  tree template, t1;

  if (classname == error_mark_node)
    return error_mark_node;

  my_friendly_assert (TREE_CODE (classname) == IDENTIFIER_NODE, 278);
  template = IDENTIFIER_TEMPLATE (classname);

  if (IDENTIFIER_HAS_TYPE_VALUE (classname))
    {
      tree type = IDENTIFIER_TYPE_VALUE (classname);
      if (TREE_CODE (type) == UNINSTANTIATED_P_TYPE)
	return type;
      if (TYPE_BEING_DEFINED (type)
	  || TYPE_SIZE (type)
	  || CLASSTYPE_USE_TEMPLATE (type) != 0)
	return type;
    }
  if (uses_template_parms (classname))
    {
      if (!TREE_TYPE (classname))
	{
	  tree t = make_lang_type (RECORD_TYPE);
	  tree d = build_lang_decl (TYPE_DECL, classname, t);
	  DECL_NAME (d) = classname;
	  TYPE_NAME (t) = d;
	  pushdecl (d);
	}
      return NULL_TREE;
    }

  t1 = TREE_PURPOSE (template);
  my_friendly_assert (TREE_CODE (t1) == TEMPLATE_DECL, 279);

  /* If a template is declared but not defined, accept it; don't crash.
     Later uses requiring the definition will be flagged as errors by
     other code.  Thanks to niklas@appli.se for this bug fix.  */
  if (DECL_TEMPLATE_INFO (t1)->text == 0)
    setup_parse = 0;

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "instantiate_class_template");
  debug_bindings_indentation += 4;
#endif

  push_to_top_level ();
  push_template_decls (DECL_TEMPLATE_PARMS (TREE_PURPOSE (template)),
		       TREE_VALUE (template), 0);
  set_current_level_tags_transparency (1);
  template_info = DECL_TEMPLATE_INFO (t1);
  if (setup_parse)
    {
      feed_input (template_info->text, template_info->length, (struct obstack *)0);
      lineno = template_info->lineno;
      input_filename = template_info->filename;
      /* Get interface/implementation back in sync.  */
      extract_interface_info ();
      overload_template_name (classname, 0);
      yychar = PRE_PARSED_CLASS_DECL;
      yylval.ttype = build_tree_list (class_type_node, classname);
      processing_template_defn++;
    }
  else
    {
      tree t, decl, id, tmpl;

      id = classname;
      tmpl = TREE_PURPOSE (IDENTIFIER_TEMPLATE (id));
      t = xref_tag (DECL_TEMPLATE_INFO (tmpl)->aggr, id, NULL_TREE);
      my_friendly_assert (TREE_CODE (t) == RECORD_TYPE, 280);
#if 1
      lineno = template_info->lineno;
      input_filename = template_info->filename;
      /* Get interface/implementation back in sync.  */
      extract_interface_info ();
#endif

      /* Now, put a copy of the decl in global scope, to avoid
       * recursive expansion.  */
      decl = IDENTIFIER_LOCAL_VALUE (id);
      if (!decl)
	decl = IDENTIFIER_CLASS_VALUE (id);
      if (decl)
	{
	  my_friendly_assert (TREE_CODE (decl) == TYPE_DECL, 281);
	  /* We'd better make sure we're on the permanent obstack or else
	   * we'll get a "friendly" abort 124 in pushdecl.  Perhaps a
	   * copy_to_permanent would be sufficient here, but then a
	   * sharing problem might occur.  I don't know -- niklas@appli.se */
	  push_obstacks (&permanent_obstack, &permanent_obstack);
	  pushdecl_top_level (copy_node (decl));
	  pop_obstacks ();
	}
      pop_from_top_level ();
    }

#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif

  return NULL_TREE;
}

static int
list_eq (t1, t2)
     tree t1, t2;
{
  if (t1 == NULL_TREE)
    return t2 == NULL_TREE;
  if (t2 == NULL_TREE)
    return 0;
  /* Don't care if one declares its arg const and the other doesn't -- the
     main variant of the arg type is all that matters.  */
  if (TYPE_MAIN_VARIANT (TREE_VALUE (t1))
      != TYPE_MAIN_VARIANT (TREE_VALUE (t2)))
    return 0;
  return list_eq (TREE_CHAIN (t1), TREE_CHAIN (t2));
}

static tree
tsubst (t, args, nargs, in_decl)
     tree t, *args;
     int nargs;
     tree in_decl;
{
  tree type;

  if (t == NULL_TREE || t == error_mark_node)
    return t;

  type = TREE_TYPE (t);
  if (type
      /* Minor optimization.
	 ?? Are these really the most frequent cases?  Is the savings
	 significant?  */
      && type != integer_type_node
      && type != void_type_node
      && type != char_type_node)
    type = build_type_variant (tsubst (type, args, nargs, in_decl),
			       TYPE_READONLY (type),
			       TYPE_VOLATILE (type));
  switch (TREE_CODE (t))
    {
    case ERROR_MARK:
    case IDENTIFIER_NODE:
    case OP_IDENTIFIER:
    case VOID_TYPE:
    case REAL_TYPE:
    case ENUMERAL_TYPE:
    case INTEGER_CST:
    case REAL_CST:
    case STRING_CST:
    case RECORD_TYPE:
    case UNION_TYPE:
      return t;

    case INTEGER_TYPE:
      if (t == integer_type_node)
	return t;

      if (TREE_CODE (TYPE_MIN_VALUE (t)) == INTEGER_CST
	  && TREE_CODE (TYPE_MAX_VALUE (t)) == INTEGER_CST)
	return t;
      return build_index_2_type (tsubst (TYPE_MIN_VALUE (t), args, nargs, in_decl),
				 tsubst (TYPE_MAX_VALUE (t), args, nargs, in_decl));

    case TEMPLATE_TYPE_PARM:
      return build_type_variant (args[TEMPLATE_TYPE_IDX (t)],
				 TYPE_READONLY (t),
				 TYPE_VOLATILE (t));

    case TEMPLATE_CONST_PARM:
      return args[TEMPLATE_CONST_IDX (t)];

    case FUNCTION_DECL:
      {
	tree r;
	tree fnargs, result;
	
	if (type == TREE_TYPE (t)
	    && (DECL_CONTEXT (t) == NULL_TREE
		|| TREE_CODE_CLASS (TREE_CODE (DECL_CONTEXT (t))) != 't'))
	  return t;
	fnargs = tsubst (DECL_ARGUMENTS (t), args, nargs, t);
	result = tsubst (DECL_RESULT (t), args, nargs, t);
	if (DECL_CONTEXT (t) != NULL_TREE
	    && TREE_CODE_CLASS (TREE_CODE (DECL_CONTEXT (t))) == 't')
	  {
	    /* Look it up in that class, and return the decl node there,
	       instead of creating a new one.  */
	    tree ctx, methods, name, method;
	    int n_methods;
	    int i, found = 0;

	    name = DECL_NAME (t);
	    ctx = tsubst (DECL_CONTEXT (t), args, nargs, t);
	    methods = CLASSTYPE_METHOD_VEC (ctx);
	    if (methods == NULL_TREE)
	      /* No methods at all -- no way this one can match.  */
	      goto no_match;
	    n_methods = TREE_VEC_LENGTH (methods);

	    r = NULL_TREE;

	    if (!strncmp (OPERATOR_TYPENAME_FORMAT,
			  IDENTIFIER_POINTER (name),
			  sizeof (OPERATOR_TYPENAME_FORMAT) - 1))
	      {
		/* Type-conversion operator.  Reconstruct the name, in
		   case it's the name of one of the template's parameters.  */
		name = build_typename_overload (TREE_TYPE (type));
	      }

	    if (DECL_CONTEXT (t) != NULL_TREE
		&& TREE_CODE_CLASS (TREE_CODE (DECL_CONTEXT (t))) == 't'
		&& constructor_name (DECL_CONTEXT (t)) == DECL_NAME (t))
	      name = constructor_name (ctx);
#if 0
	    fprintf (stderr, "\nfor function %s in class %s:\n",
		     IDENTIFIER_POINTER (name),
		     IDENTIFIER_POINTER (TYPE_IDENTIFIER (ctx)));
#endif
	    for (i = 0; i < n_methods; i++)
	      {
		int pass;

		method = TREE_VEC_ELT (methods, i);
		if (method == NULL_TREE || DECL_NAME (method) != name)
		  continue;

		pass = 0;
	      maybe_error:
		for (; method; method = TREE_CHAIN (method))
		  {
		    my_friendly_assert (TREE_CODE (method) == FUNCTION_DECL,
					282);
		    if (TREE_TYPE (method) != type)
		      {
			tree mtype = TREE_TYPE (method);
			tree t1, t2;

			/* Keep looking for a method that matches
			   perfectly.  This takes care of the problem
			   where destructors (which have implicit int args)
			   look like constructors which have an int arg.  */
			if (pass == 0)
			  continue;

			t1 = TYPE_ARG_TYPES (mtype);
			t2 = TYPE_ARG_TYPES (type);
			if (TREE_CODE (mtype) == FUNCTION_TYPE)
			  t2 = TREE_CHAIN (t2);

			if (list_eq (t1, t2))
			  {
			    if (TREE_CODE (mtype) == FUNCTION_TYPE)
			      {
				tree newtype;
				newtype = build_function_type (TREE_TYPE (type),
							       TYPE_ARG_TYPES (type));
				newtype = build_type_variant (newtype,
							      TYPE_READONLY (type),
							      TYPE_VOLATILE (type));
				type = newtype;
				if (TREE_TYPE (type) != TREE_TYPE (mtype))
				  goto maybe_bad_return_type;
			      }
			    else if (TYPE_METHOD_BASETYPE (mtype)
				     == TYPE_METHOD_BASETYPE (type))
			      {
				/* Types didn't match, but arg types and
				   `this' do match, so the return type is
				   all that should be messing it up.  */
			      maybe_bad_return_type:
				if (TREE_TYPE (type) != TREE_TYPE (mtype))
				  error ("inconsistent return types for method `%s' in class `%s'",
					 IDENTIFIER_POINTER (name),
					 IDENTIFIER_POINTER (TYPE_IDENTIFIER (ctx)));
			      }
			    r = method;
			    break;
			  }
			found = 1;
			continue;
		      }
#if 0
		    fprintf (stderr, "\tfound %s\n\n",
			     IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (method)));
#endif
		    if (DECL_ARGUMENTS (method)
			&& ! TREE_PERMANENT (DECL_ARGUMENTS (method)))
		      /* @@ Is this early enough?  Might we want to do
			 this instead while processing the expansion?	 */
		      DECL_ARGUMENTS (method)
			= tsubst (DECL_ARGUMENTS (t), args, nargs, t);
		    r = method;
		    break;
		  }
		if (r == NULL_TREE && pass == 0)
		  {
		    pass = 1;
		    method = TREE_VEC_ELT (methods, i);
		    goto maybe_error;
		  }
	      }
	    if (r == NULL_TREE)
	      {
	      no_match:
		error (found
		       ? "template for method `%s' doesn't match any in class `%s'"
		       : "method `%s' not found in class `%s'",
		       IDENTIFIER_OPNAME_P (name)
		       ? operator_name_string (name) : IDENTIFIER_POINTER (name),
		       IDENTIFIER_POINTER (TYPE_IDENTIFIER (ctx)));
		if (in_decl)
		  error_with_decl (in_decl, "in attempt to instantiate `%s' declared at this point in file");
		return error_mark_node;
	      }
	  }
	else
	  {
	    r = DECL_NAME (t);
	    {
	      tree decls, val;
	      int got_it = 0;

	      decls = IDENTIFIER_GLOBAL_VALUE (r);
	      if (decls == NULL_TREE)
		/* no match */;
	      else if (TREE_CODE (decls) == TREE_LIST)
		while (decls)
		  {
		    val = TREE_VALUE (decls);
		    decls = TREE_CHAIN (decls);
		  try_one:
		    if (TREE_CODE (val) == FUNCTION_DECL
			&& TREE_TYPE (val) == type)
		      {
			got_it = 1;
			r = val;
			break;
		      }
		  }
	      else
		{
		  val = decls;
		  decls = NULL_TREE;
		  goto try_one;
		}

	      if (!got_it)
		{
		  r = build_decl_overload (r, TYPE_VALUES (type),
					   DECL_CONTEXT (t) != NULL_TREE);
		  r = build_lang_decl (FUNCTION_DECL, r, type);
		}
	    }
	  }
	TREE_PUBLIC (r) = TREE_PUBLIC (t);
	DECL_EXTERNAL (r) = DECL_EXTERNAL (t);
	TREE_STATIC (r) = TREE_STATIC (t);
	DECL_INLINE (r) = DECL_INLINE (t);
	DECL_SOURCE_FILE (r) = DECL_SOURCE_FILE (t);
	DECL_SOURCE_LINE (r) = DECL_SOURCE_LINE (t);
	DECL_CLASS_CONTEXT (r) = tsubst (DECL_CLASS_CONTEXT (t), args, nargs, t);
	make_decl_rtl (r, NULL_PTR, 1);
	DECL_ARGUMENTS (r) = fnargs;
	DECL_RESULT (r) = result;
	if (DECL_CONTEXT (t) == NULL_TREE
	    || TREE_CODE_CLASS (TREE_CODE (DECL_CONTEXT (t))) != 't')
	  push_overloaded_decl_top_level (r, 0);
	return r;
      }

    case PARM_DECL:
      {
	tree r;
	r = build_decl (PARM_DECL, DECL_NAME (t), type);
	DECL_INITIAL (r) = TREE_TYPE (r);
	if (TREE_CHAIN (t))
	  TREE_CHAIN (r) = tsubst (TREE_CHAIN (t), args, nargs, TREE_CHAIN (t));
	return r;
      }

    case TREE_LIST:
      {
	tree purpose, value, chain, result;
	int via_public, via_virtual, via_protected;

	if (t == void_list_node)
	  return t;

	via_public = TREE_VIA_PUBLIC (t);
	via_protected = TREE_VIA_PROTECTED (t);
	via_virtual = TREE_VIA_VIRTUAL (t);

	purpose = TREE_PURPOSE (t);
	if (purpose)
	  purpose = tsubst (purpose, args, nargs, in_decl);
	value = TREE_VALUE (t);
	if (value)
	  value = tsubst (value, args, nargs, in_decl);
	chain = TREE_CHAIN (t);
	if (chain && chain != void_type_node)
	  chain = tsubst (chain, args, nargs, in_decl);
	if (purpose == TREE_PURPOSE (t)
	    && value == TREE_VALUE (t)
	    && chain == TREE_CHAIN (t))
	  return t;
	result = hash_tree_cons (via_public, via_virtual, via_protected,
				 purpose, value, chain);
	TREE_PARMLIST (result) = TREE_PARMLIST (t);
	return result;
      }
    case TREE_VEC:
      {
	int len = TREE_VEC_LENGTH (t), need_new = 0, i;
	tree *elts = (tree *) alloca (len * sizeof (tree));
	bzero (elts, len * sizeof (tree));

	for (i = 0; i < len; i++)
	  {
	    elts[i] = tsubst (TREE_VEC_ELT (t, i), args, nargs, in_decl);
	    if (elts[i] != TREE_VEC_ELT (t, i))
	      need_new = 1;
	  }

	if (!need_new)
	  return t;

	t = make_tree_vec (len);
	for (i = 0; i < len; i++)
	  TREE_VEC_ELT (t, i) = elts[i];
	return t;
      }
    case POINTER_TYPE:
    case REFERENCE_TYPE:
      {
	tree r;
	enum tree_code code;
	if (type == TREE_TYPE (t))
	  return t;

	code = TREE_CODE (t);
	if (code == POINTER_TYPE)
	  r = build_pointer_type (type);
	else
	  r = build_reference_type (type);
	r = build_type_variant (r, TYPE_READONLY (t), TYPE_VOLATILE (t));
	/* Will this ever be needed for TYPE_..._TO values?  */
	layout_type (r);
	return r;
      }
    case FUNCTION_TYPE:
    case METHOD_TYPE:
      {
	tree values = TYPE_VALUES (t); /* same as TYPE_ARG_TYPES */
	tree context = TYPE_CONTEXT (t);
	tree new_value;

	/* Don't bother recursing if we know it won't change anything.	*/
	if (! (values == void_type_node
	       || values == integer_type_node))
	  values = tsubst (values, args, nargs, in_decl);
	if (context)
	  context = tsubst (context, args, nargs, in_decl);
	/* Could also optimize cases where return value and
	   values have common elements (e.g., T min(const &T, const T&).  */

	/* If the above parameters haven't changed, just return the type.  */
	if (type == TREE_TYPE (t)
	    && values == TYPE_VALUES (t)
	    && context == TYPE_CONTEXT (t))
	  return t;

	/* Construct a new type node and return it.  */
	if (TREE_CODE (t) == FUNCTION_TYPE
	    && context == NULL_TREE)
	  {
	    new_value = build_function_type (type, values);
	  }
	else if (context == NULL_TREE)
	  {
	    tree base = tsubst (TREE_TYPE (TREE_VALUE (TYPE_ARG_TYPES (t))),
				args, nargs, in_decl);
	    new_value = build_cplus_method_type (base, type,
						 TREE_CHAIN (values));
	  }
	else
	  {
	    new_value = make_node (TREE_CODE (t));
	    TREE_TYPE (new_value) = type;
	    TYPE_CONTEXT (new_value) = context;
	    TYPE_VALUES (new_value) = values;
	    TYPE_SIZE (new_value) = TYPE_SIZE (t);
	    TYPE_ALIGN (new_value) = TYPE_ALIGN (t);
	    TYPE_MODE (new_value) = TYPE_MODE (t);
	    if (TYPE_METHOD_BASETYPE (t))
	      TYPE_METHOD_BASETYPE (new_value) = tsubst (TYPE_METHOD_BASETYPE (t),
							 args, nargs, in_decl);
	    /* Need to generate hash value.  */
	    my_friendly_abort (84);
	  }
	new_value = build_type_variant (new_value,
					TYPE_READONLY (t),
					TYPE_VOLATILE (t));
	return new_value;
      }
    case ARRAY_TYPE:
      {
	tree domain = tsubst (TYPE_DOMAIN (t), args, nargs, in_decl);
	tree r;
	if (type == TREE_TYPE (t) && domain == TYPE_DOMAIN (t))
	  return t;
	r = build_cplus_array_type (type, domain);
	return r;
      }

    case UNINSTANTIATED_P_TYPE:
      {
	int nparms = TREE_VEC_LENGTH (DECL_TEMPLATE_PARMS (UPT_TEMPLATE (t)));
	tree argvec = make_tree_vec (nparms);
	tree parmvec = UPT_PARMS (t);
	int i;
	tree id;
	for (i = 0; i < nparms; i++)
	  TREE_VEC_ELT (argvec, i) = tsubst (TREE_VEC_ELT (parmvec, i),
					     args, nargs, in_decl);
	id = lookup_template_class (DECL_NAME (UPT_TEMPLATE (t)), argvec, NULL_TREE);
	if (! IDENTIFIER_HAS_TYPE_VALUE (id)) {
	  instantiate_class_template(id, 0);
	  /* set up pending_classes */
	  add_pending_template (id);

	  TYPE_MAIN_VARIANT (IDENTIFIER_TYPE_VALUE (id)) =
	    IDENTIFIER_TYPE_VALUE (id);
	}
	return build_type_variant (IDENTIFIER_TYPE_VALUE (id),
				   TYPE_READONLY (t),
				   TYPE_VOLATILE (t));
      }

    case MINUS_EXPR:
    case PLUS_EXPR:
      return fold (build (TREE_CODE (t), TREE_TYPE (t),
			  tsubst (TREE_OPERAND (t, 0), args, nargs, in_decl),
			  tsubst (TREE_OPERAND (t, 1), args, nargs, in_decl)));

    case NEGATE_EXPR:
    case NOP_EXPR:
      return fold (build1 (TREE_CODE (t), TREE_TYPE (t),
			   tsubst (TREE_OPERAND (t, 0), args, nargs, in_decl)));

    default:
      sorry ("use of `%s' in function template",
	     tree_code_name [(int) TREE_CODE (t)]);
      return error_mark_node;
    }
}

tree
instantiate_template (tmpl, targ_ptr)
     tree tmpl, *targ_ptr;
{
  tree targs, fndecl;
  int i, len;
  struct pending_inline *p;
  struct template_info *t;
  struct obstack *old_fmp_obstack;
  extern struct obstack *function_maybepermanent_obstack;

  push_obstacks (&permanent_obstack, &permanent_obstack);
  old_fmp_obstack = function_maybepermanent_obstack;
  function_maybepermanent_obstack = &permanent_obstack;

  my_friendly_assert (TREE_CODE (tmpl) == TEMPLATE_DECL, 283);
  len = TREE_VEC_LENGTH (DECL_TEMPLATE_PARMS (tmpl));

  for (fndecl = DECL_TEMPLATE_INSTANTIATIONS (tmpl);
       fndecl; fndecl = TREE_CHAIN (fndecl))
    {
      tree *t1 = &TREE_VEC_ELT (TREE_PURPOSE (fndecl), 0);
      for (i = len - 1; i >= 0; i--)
	if (t1[i] != targ_ptr[i])
	  goto no_match;

      /* Here, we have a match.  */
      fndecl = TREE_VALUE (fndecl);
      function_maybepermanent_obstack = old_fmp_obstack;
      pop_obstacks ();
      return fndecl;

    no_match:
      ;
    }

  targs = make_tree_vec (len);
  i = len;
  while (i--)
    TREE_VEC_ELT (targs, i) = targ_ptr[i];

  /* substitute template parameters */
  fndecl = tsubst (DECL_RESULT (tmpl), targ_ptr,
		   TREE_VEC_LENGTH (targs), tmpl);

  /* If it's a static member fn in the template, we need to change it
     into a FUNCTION_TYPE and chop off its this pointer.  */
  if (TREE_CODE (TREE_TYPE (DECL_RESULT (tmpl))) == METHOD_TYPE
      && fndecl != error_mark_node
      && DECL_STATIC_FUNCTION_P (fndecl))
    {
      tree olddecl = DECL_RESULT (tmpl);
      revert_static_member_fn (&TREE_TYPE (olddecl), &DECL_RESULT (tmpl),
			       &TYPE_ARG_TYPES (TREE_TYPE (olddecl)));
      /* Chop off the this pointer that grokclassfn so kindly added
	 for us (it didn't know yet if the fn was static or not).  */
      DECL_ARGUMENTS (olddecl) = TREE_CHAIN (DECL_ARGUMENTS (olddecl));
      DECL_ARGUMENTS (fndecl) = TREE_CHAIN (DECL_ARGUMENTS (fndecl));
    }
     
  t = DECL_TEMPLATE_INFO (tmpl);
  if (t->text)
    {
      p = (struct pending_inline *) permalloc (sizeof (struct pending_inline));
      p->parm_vec = t->parm_vec;
      p->bindings = targs;
      p->can_free = 0;
      p->deja_vu = 0;
      p->lineno = t->lineno;
      p->filename = t->filename;
      p->buf = t->text;
      p->len = t->length;
      p->fndecl = fndecl;
      p->interface = 1;		/* unknown */
    }
  else
    p = (struct pending_inline *)0;

  DECL_TEMPLATE_INSTANTIATIONS (tmpl) =
    tree_cons (targs, fndecl, DECL_TEMPLATE_INSTANTIATIONS (tmpl));

  function_maybepermanent_obstack = old_fmp_obstack;
  pop_obstacks ();

  if (fndecl == error_mark_node || p == (struct pending_inline *)0)
    {
      /* do nothing */
    }
  else if (DECL_INLINE (fndecl))
    {
      DECL_PENDING_INLINE_INFO (fndecl) = p;
      p->next = pending_inlines;
      pending_inlines = p;
    }
  else
    {
      p->next = pending_template_expansions;
      pending_template_expansions = p;
    }
  return fndecl;
}

void
undo_template_name_overload (id, classlevel)
     tree id;
     int classlevel;
{
  tree template;

  template = IDENTIFIER_TEMPLATE (id);
  if (!template)
    return;

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "undo_template_name_overload");
  debug_bindings_indentation += 4;
#endif

#if 0 /* not yet, should get fixed properly later */
  poplevel (0, 0, 0);
#endif
  if (!classlevel)
    poplevel (0, 0, 0);
#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif
}

void
overload_template_name (id, classlevel)
     tree id;
     int classlevel;
{
  tree template, t, decl;
  struct template_info *tinfo;

  my_friendly_assert (TREE_CODE (id) == IDENTIFIER_NODE, 284);
  template = IDENTIFIER_TEMPLATE (id);
  if (!template)
    return;

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "overload_template_name(%d)", classlevel);
  debug_bindings_indentation += 4;
#endif
  template = TREE_PURPOSE (template);
  tinfo = DECL_TEMPLATE_INFO (template);
  template = DECL_NAME (template);
  my_friendly_assert (template != NULL_TREE, 285);

  if (!classlevel)
    {
      pushlevel (1);
      declare_pseudo_global_level ();
    }

  t = xref_tag (tinfo->aggr, id, NULL_TREE);
  my_friendly_assert (TREE_CODE (t) == RECORD_TYPE
	  || TREE_CODE (t) == UNINSTANTIATED_P_TYPE, 286);

  decl = build_decl (TYPE_DECL, template, t);

#if 0 /* fix this later */
  /* We don't want to call here if the work has already been done.  */
  t = (classlevel
       ? IDENTIFIER_CLASS_VALUE (template)
       : IDENTIFIER_LOCAL_VALUE (template));
  if (t
      && TREE_CODE (t) == TYPE_DECL
      && TREE_TYPE (t) == t)
    my_friendly_abort (85);
#endif

  if (classlevel)
    pushdecl_class_level (decl);
  else
#if 0 /* not yet, should get fixed properly later */
    pushdecl (decl);
  pushlevel (1);
#else
    {
      pushdecl (decl);
      /* @@ Is this necessary now?  */
      IDENTIFIER_LOCAL_VALUE (template) = decl;
    }
#endif

#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif
}

/* T1 is PRE_PARSED_CLASS_DECL; T3 is result of XREF_TAG lookup.  */
void
end_template_instantiation (t1, t3)
     tree t1, t3;
{
  extern struct pending_input *to_be_restored;
  tree t, decl;

#ifdef DEBUG_CP_BINDING_LEVELS
  indent_to (stderr, debug_bindings_indentation);
  fprintf (stderr, "end_template_instantiation");
  debug_bindings_indentation += 4;
#endif

  processing_template_defn--;

  /* Restore the old parser input state.  */
  if (yychar == YYEMPTY)
    yychar = yylex ();
  if (yychar != END_OF_SAVED_INPUT)
    error ("parse error at end of class template");
  else
    {
      restore_pending_input (to_be_restored);
      to_be_restored = 0;
    }

  /* Our declarations didn't get stored in the global slot, since
     there was a (supposedly tags-transparent) scope in between.  */
  t = IDENTIFIER_TYPE_VALUE (TREE_VALUE (t1));
  my_friendly_assert (t != NULL_TREE
		      && TREE_CODE_CLASS (TREE_CODE (t)) == 't',
		      287);
  CLASSTYPE_USE_TEMPLATE (t) = 2;
  /* Always make methods of template classes static, until we've
     got a decent scheme for handling them.  The pragmas as they
     are now are inadequate.  */
  CLASSTYPE_INTERFACE_UNKNOWN (t) = 1;
  decl = IDENTIFIER_GLOBAL_VALUE (TREE_VALUE (t1));
  my_friendly_assert (TREE_CODE (decl) == TYPE_DECL, 288);

  undo_template_name_overload (TREE_VALUE (t1), 0);
  t = IDENTIFIER_TEMPLATE (TREE_VALUE (t1));
  pop_template_decls (DECL_TEMPLATE_PARMS (TREE_PURPOSE (t)), TREE_VALUE (t),
		      0);
  pop_from_top_level ();

  /* This will fix up the type-value field.  */
  pushdecl_top_level (decl);

  /* Restore interface/implementation settings.	 */
  extract_interface_info ();

#ifdef DEBUG_CP_BINDING_LEVELS
  debug_bindings_indentation -= 4;
#endif
}

/* Store away the text of an inline template function.	No rtl is
   generated for this function until it is actually needed.  */

void
reinit_parse_for_template (yychar, d1, d2)
     int yychar;
     tree d1, d2;
{
  struct template_info *template_info;

  if (d2 == NULL_TREE || d2 == error_mark_node)
    {
    lose:
      /* @@ Should use temp obstack, and discard results.  */
      reinit_parse_for_block (yychar, &permanent_obstack, 1);
      return;
    }

  if (TREE_CODE (d2) == IDENTIFIER_NODE)
    d2 = IDENTIFIER_GLOBAL_VALUE (d2);
  if (!d2)
    goto lose;
  template_info = DECL_TEMPLATE_INFO (d2);
  if (!template_info)
    {
      template_info = (struct template_info *) permalloc (sizeof (struct template_info));
      bzero (template_info, sizeof (struct template_info));
      DECL_TEMPLATE_INFO (d2) = template_info;
    }
  template_info->filename = input_filename;
  template_info->lineno = lineno;
  reinit_parse_for_block (yychar, &permanent_obstack, 1);
  template_info->text = obstack_base (&permanent_obstack);
  template_info->length = obstack_object_size (&permanent_obstack);
  obstack_finish (&permanent_obstack);
  template_info->parm_vec = d1;
}

/* Type unification.

   We have a function template signature with one or more references to
   template parameters, and a parameter list we wish to fit to this
   template.  If possible, produce a list of parameters for the template
   which will cause it to fit the supplied parameter list.

   Return zero for success, 2 for an incomplete match that doesn't resolve
   all the types, and 1 for complete failure.  An error message will be
   printed only for an incomplete match.

   TPARMS[NTPARMS] is an array of template parameter types;
   TARGS[NTPARMS] is the array of template parameter values.  PARMS is
   the function template's signature (using TEMPLATE_PARM_IDX nodes),
   and ARGS is the argument list we're trying to match against it.  */

int
type_unification (tparms, targs, parms, args, nsubsts)
     tree tparms, *targs, parms, args;
     int *nsubsts;
{
  tree parm, arg;
  int i;
  int ntparms = TREE_VEC_LENGTH (tparms);

  my_friendly_assert (TREE_CODE (tparms) == TREE_VEC, 289);
  my_friendly_assert (TREE_CODE (parms) == TREE_LIST, 290);
  /* ARGS could be NULL (via a call from cp-parse.y to
     build_x_function_call).  */
  if (args)
    my_friendly_assert (TREE_CODE (args) == TREE_LIST, 291);
  my_friendly_assert (ntparms > 0, 292);

  bzero (targs, sizeof (tree) * ntparms);

  while (parms
	 && parms != void_list_node
	 && args)
    {
      parm = TREE_VALUE (parms);
      parms = TREE_CHAIN (parms);
      arg = TREE_VALUE (args);
      args = TREE_CHAIN (args);

      if (arg == error_mark_node)
	return 1;
      if (arg == unknown_type_node)
	return 1;
#if 0
      if (TREE_CODE (arg) == VAR_DECL)
	arg = TREE_TYPE (arg);
      else if (TREE_CODE_CLASS (TREE_CODE (arg)) == 'e')
	arg = TREE_TYPE (arg);
#else
      my_friendly_assert (TREE_TYPE (arg) != NULL_TREE, 293);
      arg = TREE_TYPE (arg);
#endif

      switch (unify (tparms, targs, ntparms, parm, arg, nsubsts))
	{
	case 0:
	  break;
	case 1:
	  return 1;
	}
    }
  /* Fail if we've reached the end of the parm list, and more args
     are present, and the parm list isn't variadic.  */
  if (args && parms == void_list_node)
    return 1;
  /* Fail if parms are left and they don't have default values.	 */
  if (parms
      && parms != void_list_node
      && TREE_PURPOSE (parms) == NULL_TREE)
    return 1;
  for (i = 0; i < ntparms; i++)
    if (!targs[i])
      {
	error ("incomplete type unification");
	return 2;
      }
  return 0;
}

/* Tail recursion is your friend.  */
static int
unify (tparms, targs, ntparms, parm, arg, nsubsts)
     tree tparms, *targs, parm, arg;
     int *nsubsts;
{
  int idx;

  /* I don't think this will do the right thing with respect to types.
     But the only case I've seen it in so far has been array bounds, where
     signedness is the only information lost, and I think that will be
     okay.  */
  while (TREE_CODE (parm) == NOP_EXPR)
    parm = TREE_OPERAND (parm, 0);

  if (arg == error_mark_node)
    return 1;
  if (arg == unknown_type_node)
    return 1;
  if (arg == parm)
    return 0;

  switch (TREE_CODE (parm))
    {
    case TEMPLATE_TYPE_PARM:
      (*nsubsts)++;
      if (TEMPLATE_TYPE_TPARMLIST (parm) != tparms)
	{
	  error ("mixed template headers?!");
	  my_friendly_abort (86);
	  return 1;
	}
      idx = TEMPLATE_TYPE_IDX (parm);
      /* Simple cases: Value already set, does match or doesn't.  */
      if (targs[idx] == arg)
	return 0;
      else if (targs[idx])
	return 1;
      /* Check for mixed types and values.  */
      if (TREE_CODE (TREE_VEC_ELT (tparms, idx)) != IDENTIFIER_NODE)
	return 1;
      targs[idx] = arg;
      return 0;
    case TEMPLATE_CONST_PARM:
      (*nsubsts)++;
      idx = TEMPLATE_CONST_IDX (parm);
      if (targs[idx] == arg)
	return 0;
      else if (targs[idx])
	{
	  my_friendly_abort (87);
	  return 1;
	}
/*	else if (typeof arg != tparms[idx])
	return 1;*/

      targs[idx] = copy_to_permanent (arg);
      return 0;

    case POINTER_TYPE:
      if (TREE_CODE (arg) != POINTER_TYPE)
	return 1;
      return unify (tparms, targs, ntparms, TREE_TYPE (parm), TREE_TYPE (arg),
		    nsubsts);

    case REFERENCE_TYPE:
      return unify (tparms, targs, ntparms, TREE_TYPE (parm), arg, nsubsts);

    case ARRAY_TYPE:
      if (TREE_CODE (arg) != ARRAY_TYPE)
	return 1;
      if (unify (tparms, targs, ntparms, TYPE_DOMAIN (parm), TYPE_DOMAIN (arg),
		 nsubsts) != 0)
	return 1;
      return unify (tparms, targs, ntparms, TREE_TYPE (parm), TREE_TYPE (arg),
		    nsubsts);

    case REAL_TYPE:
    case INTEGER_TYPE:
      if (TREE_CODE (parm) == INTEGER_TYPE && TREE_CODE (arg) == INTEGER_TYPE)
	{
	  if (TYPE_MIN_VALUE (parm) && TYPE_MIN_VALUE (arg)
	      && unify (tparms, targs, ntparms,
			TYPE_MIN_VALUE (parm), TYPE_MIN_VALUE (arg), nsubsts))
	    return 1;
	  if (TYPE_MAX_VALUE (parm) && TYPE_MAX_VALUE (arg)
	      && unify (tparms, targs, ntparms,
			TYPE_MAX_VALUE (parm), TYPE_MAX_VALUE (arg), nsubsts))
	    return 1;
	}
      /* As far as unification is concerned, this wins.	 Later checks
	 will invalidate it if necessary.  */
      return 0;

      /* Types INTEGER_CST and MINUS_EXPR can come from array bounds.  */
    case INTEGER_CST:
      if (TREE_CODE (arg) != INTEGER_CST)
	return 1;
      return !tree_int_cst_equal (parm, arg);

    case MINUS_EXPR:
      {
	tree t1, t2;
	t1 = TREE_OPERAND (parm, 0);
	t2 = TREE_OPERAND (parm, 1);
	if (TREE_CODE (t1) != TEMPLATE_CONST_PARM)
	  return 1;
	return unify (tparms, targs, ntparms, t1,
		      fold (build (PLUS_EXPR, integer_type_node, arg, t2)),
		      nsubsts);
      }

    case TREE_VEC:
      {
	int i;
	if (TREE_CODE (arg) != TREE_VEC)
	  return 1;
	if (TREE_VEC_LENGTH (parm) != TREE_VEC_LENGTH (arg))
	  return 1;
	for (i = TREE_VEC_LENGTH (parm) - 1; i >= 0; i--)
	  if (unify (tparms, targs, ntparms,
		     TREE_VEC_ELT (parm, i), TREE_VEC_ELT (arg, i),
		     nsubsts))
	    return 1;
	return 0;
      }

    case UNINSTANTIATED_P_TYPE:
      {
	tree a;
	/* Unification of something that is not a template fails. (mrs) */
	if (TYPE_NAME (arg) == 0)
	  return 1;
	a = IDENTIFIER_TEMPLATE (TYPE_IDENTIFIER (arg));
	/* Unification of something that is not a template fails. (mrs) */
	if (a == 0)
	  return 1;
	if (UPT_TEMPLATE (parm) != TREE_PURPOSE (a))
	  /* different templates */
	  return 1;
	return unify (tparms, targs, ntparms, UPT_PARMS (parm), TREE_VALUE (a),
		      nsubsts);
      }

    case RECORD_TYPE:
      /* Unification of something that is not a template fails. (mrs) */
      return 1;

    default:
      sorry ("use of `%s' in template type unification",
	     tree_code_name [(int) TREE_CODE (parm)]);
      return 1;
    }
}


#undef DEBUG

int
do_pending_expansions ()
{
  struct pending_inline *i, *new_list = 0;

  if (!pending_template_expansions)
    return 0;

#ifdef DEBUG
  fprintf (stderr, "\n\n\t\t IN DO_PENDING_EXPANSIONS\n\n");
#endif

  i = pending_template_expansions;
  while (i)
    {
      tree context;

      struct pending_inline *next = i->next;
      tree t = i->fndecl;

      int decision = 0;
#define DECIDE(N) if(1){decision=(N); goto decided;}else

      my_friendly_assert (TREE_CODE (t) == FUNCTION_DECL
			  || TREE_CODE (t) == VAR_DECL, 294);
      if (TREE_ASM_WRITTEN (t))
	DECIDE (0);
      /* If it's a method, let the class type decide it.
	 @@ What if the method template is in a separate file?
	 Maybe both file contexts should be taken into account?  */
      context = DECL_CONTEXT (t);
      if (context != NULL_TREE
	  && TREE_CODE_CLASS (TREE_CODE (context)) == 't')
	{
	  /* If `unknown', we might want a static copy.
	     If `implementation', we want a global one.
	     If `interface', ext ref.  */
	  if (!CLASSTYPE_INTERFACE_UNKNOWN (context))
	    DECIDE (!CLASSTYPE_INTERFACE_ONLY (context));
#if 0 /* This doesn't get us stuff needed only by the file initializer.  */
	  DECIDE (TREE_USED (t));
#else /* This compiles too much stuff, but that's probably better in
	 most cases than never compiling the stuff we need.  */
	  DECIDE (1);
#endif
	}
      /* else maybe call extract_interface_info? */
      if (TREE_USED (t)) /* is this right? */
	DECIDE (1);

    decided:
#ifdef DEBUG
      print_node_brief (stderr, decision ? "yes: " : "no: ", t, 0);
      fprintf (stderr, "\t%s\n",
	       (DECL_ASSEMBLER_NAME (t)
		? IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (t))
		: ""));
#endif
      if (decision == 1)
	{
	  i->next = pending_inlines;
	  pending_inlines = i;
	}
      else
	{
	  i->next = new_list;
	  new_list = i;
	}
      i = next;
    }
  pending_template_expansions = new_list;
  if (!pending_inlines)
    return 0;
  do_pending_inlines ();
  return 1;
}


struct pending_template {
  struct pending_template *next;
  tree id;
};

static struct pending_template* pending_templates;

void
do_pending_templates ()
{
  struct pending_template* t;
  
  for ( t = pending_templates; t; t = t->next)
    {
      instantiate_class_template (t->id, 1);
    }

  for ( t = pending_templates; t; t = pending_templates)
    {
      pending_templates = t->next;
      free(t);
    }
}

static void
add_pending_template (pt)
     tree pt;
{
  struct pending_template *p;
  
  p = (struct pending_template *) malloc (sizeof (struct pending_template));
  p->next = pending_templates;
  pending_templates = p;
  p->id = pt;
}
