/* Process declarations and variables for C compiler.
   Copyright (C) 1988, 1992, 1993 Free Software Foundation, Inc.
   Hacked by Michael Tiemann (tiemann@cygnus.com)

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


/* Process declarations and symbol lookup for C front end.
   Also constructs types; the standard scalar types at initialization,
   and structure, union, array and enum types when they are declared.  */

/* ??? not all decl nodes are given the most useful possible
   line numbers.  For example, the CONST_DECLs for enum values.  */

#include "config.h"
#include <stdio.h>
#include "tree.h"
#include "rtl.h"
#include "flags.h"
#include "cp-tree.h"
#include "cp-decl.h"
#include "cp-lex.h"

extern tree grokdeclarator ();
static void grok_function_init ();

/* A list of virtual function tables we must make sure to write out.  */
tree pending_vtables;

/* A list of static class variables.  This is needed, because a
   static class variable can be declared inside the class without
   an initializer, and then initialized, staticly, outside the class.  */
tree pending_statics;

extern tree pending_addressable_inlines;

/* Used to help generate temporary names which are unique within
   a function.  Reset to 0 by start_function.  */

static int temp_name_counter;

/* Same, but not reset.  Local temp variables and global temp variables
   can have the same name.  */
static int global_temp_name_counter;

/* The (assembler) name of the first globally-visible object output.  */
extern char * first_global_object_name;

/* Flag used when debugging cp-spew.c */

extern int spew_debug;

/* C (and C++) language-specific option variables.  */

/* Nonzero means allow type mismatches in conditional expressions;
   just make their values `void'.   */

int flag_cond_mismatch;

/* Nonzero means give `double' the same size as `float'.  */

int flag_short_double;

/* Nonzero means don't recognize the keyword `asm'.  */

int flag_no_asm;

/* Nonzero means don't recognize the non-ANSI builtin functions.  */

int flag_no_builtin;

/* Nonzero means do some things the same way PCC does.  */

int flag_traditional;

/* Nonzero means to treat bitfields as unsigned unless they say `signed'.  */

int flag_signed_bitfields = 1;

/* Nonzero means handle `#ident' directives.  0 means ignore them.  */

int flag_no_ident = 0;

/* Nonzero means handle things in ANSI, instead of GNU fashion.  This
   flag should be tested for language behavior that's different between
   ANSI and GNU, but not so horrible as to merit a PEDANTIC label.  */

int flag_ansi = 0;

/* Nonzero means do emit exported implementations of functions even if
   they can be inlined.  */

int flag_implement_inlines = 1;

/* Nonzero means warn about implicit declarations.  */

int warn_implicit = 1;

/* Like `warn_return_type', but this is set by users, whereas
   `warn_return_type' is set by the compiler.  */

int explicit_warn_return_type;

/* Nonzero means give string constants the type `const char *'
   to get extra warnings from them.  These warnings will be too numerous
   to be useful, except in thoroughly ANSIfied programs.  */

int warn_write_strings;

/* Nonzero means warn about pointer casts that can drop a type qualifier
   from the pointer target type.  */

int warn_cast_qual;

/* Nonzero means warn that dbx info for template class methods isn't fully
   supported yet.  */

int warn_template_debugging;

/* Warn about traditional constructs whose meanings changed in ANSI C.  */

int warn_traditional;

/* Nonzero means warn about sizeof(function) or addition/subtraction
   of function pointers.  */

int warn_pointer_arith;

/* Nonzero means warn for non-prototype function decls
   or non-prototyped defs without previous prototype.  */

int warn_strict_prototypes;

/* Nonzero means warn for any function def without prototype decl.  */

int warn_missing_prototypes;

/* Nonzero means warn about multiple (redundant) decls for the same single
   variable or function.  */

int warn_redundant_decls;

/* Warn about *printf or *scanf format/argument anomalies. */

int warn_format;

/* Warn about a subscript that has type char.  */

int warn_char_subscripts = 0;

/* Warn if a type conversion is done that might have confusing results.  */

int warn_conversion;

/* Warn if adding () is suggested.  */

int warn_parentheses = 1;

/* Non-zero means warn in function declared in derived class has the
   same name as a virtual in the base class, but fails to match the
   type signature of any virtual function in the base class.  */
int warn_overloaded_virtual;

/* Non-zero means warn when converting between different enumeral types.  */
int warn_enum_clash;

/* Non-zero means warn when declaring a class that has a non virtual
   destructor, when it really ought to have a virtual one. */
int warn_nonvdtor = 1;

/* Nonzero means `$' can be in an identifier.
   See cccp.c for reasons why this breaks some obscure ANSI C programs.  */

#ifndef DOLLARS_IN_IDENTIFIERS
#define DOLLARS_IN_IDENTIFIERS 1
#endif
int dollars_in_ident = DOLLARS_IN_IDENTIFIERS;

/* Nonzero for -no-strict-prototype switch: do not consider empty
   argument prototype to mean function takes no arguments.  */

int strict_prototype = 1;
int strict_prototypes_lang_c, strict_prototypes_lang_cplusplus = 1;

/* Nonzero means that labels can be used as first-class objects */

int flag_labels_ok;

/* Non-zero means to collect statistics which might be expensive
   and to print them when we are done.  */
int flag_detailed_statistics;

/* C++ specific flags.  */   
/* Nonzero for -fall-virtual: make every member function (except
   constructors) lay down in the virtual function table.  Calls
   can then either go through the virtual function table or not,
   depending.  */

int flag_all_virtual;

/* Zero means that `this' is a *const.  This gives nice behavior in the
   2.0 world.  1 gives 1.2-compatible behavior.  2 gives Spring behavior.
   -2 means we're constructing an object and it has fixed type.  */

int flag_this_is_variable;

/* Nonzero means memoize our member lookups.  */

int flag_memoize_lookups; int flag_save_memoized_contexts;

/* 3 means write out only virtuals function tables `defined'
   in this implementation file.
   2 means write out only specific virtual function tables
   and give them (C) public visibility.
   1 means write out virtual function tables and give them
   (C) public visibility.
   0 means write out virtual function tables and give them
   (C) static visibility (default).
   -1 means declare virtual function tables extern.  */

int write_virtuals;

/* Nonzero means we should attempt to elide constructors when possible.  */

int flag_elide_constructors;

/* Same, but for inline functions: nonzero means write out debug info
   for inlines.  Zero means do not.  */

int flag_inline_debug;

/* Nonzero means recognize and handle exception handling constructs.
   2 means handle exceptions the way Spring wants them handled.  */

int flag_handle_exceptions;

/* Nonzero means recognize and handle exception handling constructs.
   Use ansi syntax and semantics.  WORK IN PROGRESS!
   2 means handle exceptions the way Spring wants them handled.  */

int flag_ansi_exceptions;

/* Nonzero means that member functions defined in class scope are
   inline by default.  */

int flag_default_inline = 1;

/* Controls whether enums and ints freely convert.
   1 means with complete freedom.
   0 means enums can convert to ints, but not vice-versa.  */
int flag_int_enum_equivalence;

/* Controls whether compiler is operating under LUCID's Cadillac
   system.  1 means yes, 0 means no.  */
int flag_cadillac;

/* Controls whether compiler generates code to build objects
   that can be collected when they become garbage.  */
int flag_gc;

/* Controls whether compiler generates 'dossiers' that give
   run-time type information.  */
int flag_dossier;

/* Nonzero if we wish to output cross-referencing information
   for the GNU class browser.  */
extern int flag_gnu_xref;

/* Nonzero if compiler can make `reasonable' assumptions about
   references and objects.  For example, the compiler must be
   conservative about the following and not assume that `a' is nonnull:

   obj &a = g ();
   a.f (2);

   In general, it is `reasonable' to assume that for many programs,
   and better code can be generated in that case.  */

int flag_assume_nonnull_objects;

/* Table of language-dependent -f options.
   STRING is the option name.  VARIABLE is the address of the variable.
   ON_VALUE is the value to store in VARIABLE
    if `-fSTRING' is seen as an option.
   (If `-fno-STRING' is seen as an option, the opposite value is stored.)  */

static struct { char *string; int *variable; int on_value;} lang_f_options[] =
{
  {"signed-char", &flag_signed_char, 1},
  {"unsigned-char", &flag_signed_char, 0},
  {"signed-bitfields", &flag_signed_bitfields, 1},
  {"unsigned-bitfields", &flag_signed_bitfields, 0},
  {"short-enums", &flag_short_enums, 1},
  {"short-double", &flag_short_double, 1},
  {"cond-mismatch", &flag_cond_mismatch, 1},
  {"asm", &flag_no_asm, 0},
  {"builtin", &flag_no_builtin, 0},
  {"ident", &flag_no_ident, 0},
  {"labels-ok", &flag_labels_ok, 1},
  {"stats", &flag_detailed_statistics, 1},
  {"this-is-variable", &flag_this_is_variable, 1},
  {"strict-prototype", &strict_prototypes_lang_cplusplus, 1},
  {"all-virtual", &flag_all_virtual, 1},
  {"memoize-lookups", &flag_memoize_lookups, 1},
  {"elide-constructors", &flag_elide_constructors, 1},
  {"inline-debug", &flag_inline_debug, 0},
  {"handle-exceptions", &flag_handle_exceptions, 1},
  {"ansi-exceptions", &flag_ansi_exceptions, 1},
  {"spring-exceptions", &flag_handle_exceptions, 2},
  {"default-inline", &flag_default_inline, 1},
  {"dollars-in-identifiers", &dollars_in_ident, 1},
  {"enum-int-equiv", &flag_int_enum_equivalence, 1},
  {"gc", &flag_gc, 1},
  {"dossier", &flag_dossier, 1},
  {"xref", &flag_gnu_xref, 1},
  {"nonnull-objects", &flag_assume_nonnull_objects, 1},
  {"implement-inlines", &flag_implement_inlines, 1},
};

/* Decode the string P as a language-specific option.
   Return 1 if it is recognized (and handle it);
   return 0 if not recognized.  */

int   
lang_decode_option (p)
     char *p;
{
  if (!strcmp (p, "-ftraditional") || !strcmp (p, "-traditional"))
    flag_traditional = 1, dollars_in_ident = 1, flag_writable_strings = 1,
    flag_this_is_variable = 1;
  /* The +e options are for cfront compatibility.  They come in as
     `-+eN', to kludge around gcc.c's argument handling.  */
  else if (p[0] == '-' && p[1] == '+' && p[2] == 'e')
    {
      int old_write_virtuals = write_virtuals;
      if (p[3] == '1')
	write_virtuals = 1;
      else if (p[3] == '0')
	write_virtuals = -1;
      else if (p[3] == '2')
	write_virtuals = 2;
      else error ("invalid +e option");
      if (old_write_virtuals != 0
	  && write_virtuals != old_write_virtuals)
	error ("conflicting +e options given");
    }
  else if (p[0] == '-' && p[1] == 'f')
    {
      /* Some kind of -f option.
	 P's value is the option sans `-f'.
	 Search for it in the table of options.  */
      int found = 0, j;

      p += 2;
      /* Try special -f options.  */

      if (!strcmp (p, "save-memoized"))
	{
	  flag_memoize_lookups = 1;
	  flag_save_memoized_contexts = 1;
	  found = 1;
	}
      if (!strcmp (p, "no-save-memoized"))
	{
	  flag_memoize_lookups = 0;
	  flag_save_memoized_contexts = 0;
	  found = 1;
	}
      else if (! strncmp (p, "cadillac", 8))
	{
	  flag_cadillac = atoi (p+9);
	  found = 1;
	}
      else if (! strncmp (p, "no-cadillac", 11))
	{
	  flag_cadillac = 0;
	  found = 1;
	}
      else if (! strcmp (p, "gc"))
	{
	  flag_gc = 1;
	  /* This must come along for the ride.  */
	  flag_dossier = 1;
	  found = 1;
	}
      else if (! strcmp (p, "no-gc"))
	{
	  flag_gc = 0;
	  /* This must come along for the ride.  */
	  flag_dossier = 0;
	  found = 1;
	}
      else for (j = 0;
		!found && j < sizeof (lang_f_options) / sizeof (lang_f_options[0]);
		j++)
	{
	  if (!strcmp (p, lang_f_options[j].string))
	    {
	      *lang_f_options[j].variable = lang_f_options[j].on_value;
	      /* A goto here would be cleaner,
		 but breaks the vax pcc.  */
	      found = 1;
	    }
	  if (p[0] == 'n' && p[1] == 'o' && p[2] == '-'
	      && ! strcmp (p+3, lang_f_options[j].string))
	    {
	      *lang_f_options[j].variable = ! lang_f_options[j].on_value;
	      found = 1;
	    }
	}
      return found;
    }
  else if (p[0] == '-' && p[1] == 'W')
    {
      int setting = 1;

      /* The -W options control the warning behavior of the compiler.  */
      p += 2;

      if (p[0] == 'n' && p[1] == 'o' && p[2] == '-')
	setting = 0, p += 3;

      if (!strcmp (p, "implicit"))
	warn_implicit = setting;
      else if (!strcmp (p, "return-type"))
	explicit_warn_return_type = setting;
      else if (!strcmp (p, "write-strings"))
	warn_write_strings = setting;
      else if (!strcmp (p, "cast-qual"))
	warn_cast_qual = setting;
      else if (!strcmp (p, "traditional"))
	warn_traditional = setting;
      else if (!strcmp (p, "char-subscripts"))
	warn_char_subscripts = setting;
      else if (!strcmp (p, "pointer-arith"))
	warn_pointer_arith = setting;
      else if (!strcmp (p, "strict-prototypes"))
	warn_strict_prototypes = setting;
      else if (!strcmp (p, "missing-prototypes"))
	warn_missing_prototypes = setting;
      else if (!strcmp (p, "redundant-decls"))
	warn_redundant_decls = setting;
      else if (!strcmp (p, "format"))
	warn_format = setting;
      else if (!strcmp (p, "conversion"))
	warn_conversion = setting;
      else if (!strcmp (p, "parentheses"))
	warn_parentheses = setting;
      else if (!strcmp (p, "comment"))
	;			/* cpp handles this one.  */
      else if (!strcmp (p, "comments"))
	;			/* cpp handles this one.  */
      else if (!strcmp (p, "trigraphs"))
	;			/* cpp handles this one.  */
      else if (!strcmp (p, "import"))
	;			/* cpp handles this one.  */
      else if (!strcmp (p, "all"))
	{
	  extra_warnings = setting;
	  explicit_warn_return_type = setting;
	  warn_unused = setting;
	  warn_implicit = setting;
	  warn_switch = setting;
	  /* We save the value of warn_uninitialized, since if they put
	     -Wuninitialized on the command line, we need to generate a
	     warning about not using it without also specifying -O.  */
	  if (warn_uninitialized != 1)
	    warn_uninitialized = (setting ? 2 : 0);
	  warn_template_debugging = setting;
#if 0
	  warn_enum_clash = setting;
#endif
	}

      else if (!strcmp (p, "overloaded-virtual"))
	warn_overloaded_virtual = setting;
      else if (!strcmp (p, "enum-clash"))
	warn_enum_clash = setting;
      else return 0;
    }
  else if (!strcmp (p, "-ansi"))
    flag_no_asm = 1, dollars_in_ident = 0, flag_ansi = 1;
#ifdef SPEW_DEBUG
  /* Undocumented, only ever used when you're invoking cc1plus by hand, since
     it's probably safe to assume no sane person would ever want to use this
     under normal circumstances.  */
  else if (!strcmp (p, "-spew-debug"))
    spew_debug = 1;
#endif
  else
    return 0;

  return 1;
}

/* Incorporate `const' and `volatile' qualifiers for member functions.
   FUNCTION is a TYPE_DECL or a FUNCTION_DECL.
   QUALS is a list of qualifiers.  */
tree
grok_method_quals (ctype, function, quals)
     tree ctype, function, quals;
{
  tree fntype = TREE_TYPE (function);
  tree raises = TYPE_RAISES_EXCEPTIONS (fntype);

  do
    {
      extern tree ridpointers[];

      if (TREE_VALUE (quals) == ridpointers[(int)RID_CONST])
	{
	  if (TYPE_READONLY (ctype))
	    error ("duplicate `%s' %s",
		   IDENTIFIER_POINTER (TREE_VALUE (quals)),
		   (TREE_CODE (function) == FUNCTION_DECL
		    ? "for member function" : "in type declaration"));
	  ctype = build_type_variant (ctype, 1, TYPE_VOLATILE (ctype));
	  build_pointer_type (ctype);
	}
      else if (TREE_VALUE (quals) == ridpointers[(int)RID_VOLATILE])
	{
	  if (TYPE_VOLATILE (ctype))
	    error ("duplicate `%s' %s",
		   IDENTIFIER_POINTER (TREE_VALUE (quals)),
		   (TREE_CODE (function) == FUNCTION_DECL
		    ? "for member function" : "in type declaration"));
	  ctype = build_type_variant (ctype, TYPE_READONLY (ctype), 1);
	  build_pointer_type (ctype);
	}
      else
	my_friendly_abort (20);
      quals = TREE_CHAIN (quals);
    }
  while (quals);
  fntype = build_cplus_method_type (ctype, TREE_TYPE (fntype),
				    (TREE_CODE (fntype) == METHOD_TYPE
				     ? TREE_CHAIN (TYPE_ARG_TYPES (fntype))
				     : TYPE_ARG_TYPES (fntype)));
  if (raises)
    fntype = build_exception_variant (ctype, fntype, raises);

  TREE_TYPE (function) = fntype;
  return ctype;
}

/* This routine replaces cryptic DECL_NAMEs with readable DECL_NAMEs.
   It leaves DECL_ASSEMBLER_NAMEs with the correct value.  */
/* This does not yet work with user defined conversion operators
   It should.  */
static void
substitute_nice_name (decl)
     tree decl;
{
  if (DECL_NAME (decl) && TREE_CODE (DECL_NAME (decl)) == IDENTIFIER_NODE)
    {
      char *n = decl_as_string (DECL_NAME (decl));
      if (n[strlen (n) - 1] == ' ')
	n[strlen (n) - 1] = 0;
      DECL_NAME (decl) = get_identifier (n);
    }
}

/* Classes overload their constituent function names automatically.
   When a function name is declared in a record structure,
   its name is changed to it overloaded name.  Since names for
   constructors and destructors can conflict, we place a leading
   '$' for destructors.

   CNAME is the name of the class we are grokking for.

   FUNCTION is a FUNCTION_DECL.  It was created by `grokdeclarator'.

   FLAGS contains bits saying what's special about today's
   arguments.  1 == DESTRUCTOR.  2 == OPERATOR.

   If FUNCTION is a destructor, then we must add the `auto-delete' field
   as a second parameter.  There is some hair associated with the fact
   that we must "declare" this variable in the manner consistent with the
   way the rest of the arguments were declared.

   QUALS are the qualifiers for the this pointer.  */

void
grokclassfn (ctype, cname, function, flags, quals)
     tree ctype, cname, function;
     enum overload_flags flags;
     tree quals;
{
  tree fn_name = DECL_NAME (function);
  tree arg_types;
  tree parm;

  if (fn_name == NULL_TREE)
    {
      error ("name missing for member function");
      fn_name = get_identifier ("<anonymous>");
      DECL_NAME (function) = fn_name;
    }

  if (quals)
    ctype = grok_method_quals (ctype, function, quals);

  arg_types = TYPE_ARG_TYPES (TREE_TYPE (function));
  if (TREE_CODE (TREE_TYPE (function)) == METHOD_TYPE)
    {
      /* Must add the class instance variable up front.  */
      /* Right now we just make this a pointer.  But later
	 we may wish to make it special.  */
      tree type = TREE_VALUE (arg_types);

      if (flags == DTOR_FLAG)
	type = TYPE_MAIN_VARIANT (type);
      else if (DECL_CONSTRUCTOR_P (function))
	{
	  if (TYPE_USES_VIRTUAL_BASECLASSES (ctype))
	    {
	      DECL_CONSTRUCTOR_FOR_VBASE_P (function) = 1;
	      /* In this case we need "in-charge" flag saying whether
		 this constructor is responsible for initialization
		 of virtual baseclasses or not.  */
	      parm = build_decl (PARM_DECL, in_charge_identifier, integer_type_node);
	      DECL_ARG_TYPE (parm) = integer_type_node;
	      DECL_REGISTER (parm) = 1;
	      TREE_CHAIN (parm) = last_function_parms;
	      last_function_parms = parm;
	    }
	}

      parm = build_decl (PARM_DECL, this_identifier, type);
      /* Mark the artificial `this' parameter as "artificial".  */
      DECL_SOURCE_LINE (parm) = 0;
      DECL_ARG_TYPE (parm) = type;
      /* We can make this a register, so long as we don't
	 accidentally complain if someone tries to take its address.  */
      DECL_REGISTER (parm) = 1;
#if 0
      /* it is wrong to flag the object as readonly, when
	 flag_this_is_variable is 0. */
      if (flags != DTOR_FLAG
	  && (flag_this_is_variable <= 0 || TYPE_READONLY (type)))
#else
      if (flags != DTOR_FLAG && TYPE_READONLY (type))
#endif
	TREE_READONLY (parm) = 1;
      TREE_CHAIN (parm) = last_function_parms;
      last_function_parms = parm;
    }

  if (flags == DTOR_FLAG)
    {
      char *buf, *dbuf;
      tree const_integer_type = build_type_variant (integer_type_node, 1, 0);
      int len = sizeof (DESTRUCTOR_DECL_PREFIX)-1;

      arg_types = hash_tree_chain (const_integer_type, void_list_node);
      /* Build the overload name.  It will look like e.g. 7Example.  */
      if (IDENTIFIER_TYPE_VALUE (cname))
	dbuf = build_overload_name (IDENTIFIER_TYPE_VALUE (cname), 1, 1);
      else if (IDENTIFIER_LOCAL_VALUE (cname))
	dbuf = build_overload_name (TREE_TYPE (IDENTIFIER_LOCAL_VALUE (cname)), 1, 1);
      else
#if 0
	my_friendly_abort (346);
#else
      /* Using ctype fixes the `X::Y::~Y()' crash.  The cname has no type when
	 it's defined out of the class definition, since poplevel_class wipes
	 it out.  */
	dbuf = build_overload_name (ctype, 1, 1);
#endif
      buf = (char *)alloca (strlen (dbuf) + sizeof (DESTRUCTOR_DECL_PREFIX));
      bcopy (DESTRUCTOR_DECL_PREFIX, buf, len);
      buf[len] = '\0';
      strcat (buf, dbuf);
      DECL_ASSEMBLER_NAME (function) = get_identifier (buf);
      parm = build_decl (PARM_DECL, in_charge_identifier, const_integer_type);
      TREE_USED (parm) = 1;
      TREE_READONLY (parm) = 1;
      DECL_ARG_TYPE (parm) = const_integer_type;
      /* This is the same chain as DECL_ARGUMENTS (...).  */
      TREE_CHAIN (last_function_parms) = parm;

      TREE_TYPE (function) = build_cplus_method_type (ctype, void_type_node, arg_types);
      TYPE_HAS_DESTRUCTOR (ctype) = 1;
    }
  else
    {
      tree these_arg_types;

      if (DECL_CONSTRUCTOR_FOR_VBASE_P (function))
	{
	  arg_types = hash_tree_chain (integer_type_node, TREE_CHAIN (arg_types));
	  TREE_TYPE (function)
	    = build_cplus_method_type (ctype, TREE_TYPE (TREE_TYPE (function)), arg_types);
	  arg_types = TYPE_ARG_TYPES (TREE_TYPE (function));
	}

      these_arg_types = arg_types;

      if (TREE_CODE (TREE_TYPE (function)) == FUNCTION_TYPE)
	/* Only true for static member functions.  */
	these_arg_types = hash_tree_chain (TYPE_POINTER_TO (ctype), arg_types);

      DECL_ASSEMBLER_NAME (function)
	= build_decl_overload (fn_name, these_arg_types,
			       1 + DECL_CONSTRUCTOR_P (function));

#if 0
      /* This code is going into the compiler, but currently, it makes
	 libg++/src/Interger.cc not compile.  The problem is that the nice name
	 winds up going into the symbol table, and conversion operations look
	 for the manged name.  */
      substitute_nice_name (function);
#endif

#if 0
      if (flags == TYPENAME_FLAG)
	/* Not exactly an IDENTIFIER_TYPE_VALUE.  */
	TREE_TYPE (DECL_ASSEMBLER_NAME (function)) = TREE_TYPE (fn_name);
#endif
    }

  DECL_ARGUMENTS (function) = last_function_parms;
  /* First approximations.  */
  DECL_CONTEXT (function) = ctype;
  DECL_CLASS_CONTEXT (function) = ctype;
}

/* Generate errors possibly applicable for a given set of specifiers.  */
void
bad_specifiers (object, virtualp, quals, friendp, raises)
     char *object;
     int virtualp, quals, friendp, raises;
{
  if (virtualp)
    error ("%s declared `virtual'", object);
  if (quals)
    error ("`const' and `volatile' function specifiers invalid in %s declaration");
  if (friendp)
    error ("invalid friend declaration");
  if (raises)
    error ("invalid raises declaration");
}

/* Sanity check: report error if this function FUNCTION is not
   really a member of the class (CTYPE) it is supposed to belong to.
   CNAME and FLAGS are the same here as they are for grokclassfn above.  */

void
check_classfn (ctype, cname, function)
     tree ctype, cname, function;
{
  tree fn_name = DECL_NAME (function);
  tree fndecl;
  int need_quotes = 0;
  char *err_name;
  tree method_vec = CLASSTYPE_METHOD_VEC (ctype);
  tree *methods = 0;
  tree *end = 0;

  if (method_vec != 0)
    {
      methods = &TREE_VEC_ELT (method_vec, 0);
      end = TREE_VEC_END (method_vec);

      /* First suss out ctors and dtors.  */
      if (*methods && fn_name == cname)
	goto got_it;

      while (++methods != end)
	{
	  if (fn_name == DECL_NAME (*methods))
	    {
	    got_it:
	      fndecl = *methods;
	      while (fndecl)
		{
		  if (DECL_ASSEMBLER_NAME (function) == DECL_ASSEMBLER_NAME (fndecl))
		    return;
		  fndecl = DECL_CHAIN (fndecl);
		}
	      break;		/* loser */
	    }
	}
    }

  if (fn_name == ansi_opname[(int) TYPE_EXPR])
    {
      if (TYPE_HAS_CONVERSION (ctype))
	err_name = "such type conversion operator";
    }
  else if (IDENTIFIER_OPNAME_P (fn_name))
    {
      err_name = (char *)alloca (1024);
      sprintf (err_name, "`operator %s'", operator_name_string (fn_name));
    }
  else
    {
      err_name = IDENTIFIER_POINTER (fn_name);
      need_quotes = 1;
    }

  if (methods != end)
    if (need_quotes)
      error ("argument list for `%s' does not match any in class `%s'",
	     err_name, IDENTIFIER_POINTER (cname));
    else
      error ("argument list for %s does not match any in class `%s'",
	     err_name, IDENTIFIER_POINTER (cname));
  else
    {
      methods = 0;
      if (need_quotes)
	error ("no `%s' member function declared in class `%s'",
	       err_name, IDENTIFIER_POINTER (cname));
      else
	error ("no %s declared in class `%s'",
	       err_name, IDENTIFIER_POINTER (cname));
    }

  /* If we did not find the method in the class, add it to
     avoid spurious errors.  */
  add_method (ctype, methods, function);
}

/* Process the specs, declarator (NULL if omitted) and width (NULL if omitted)
   of a structure component, returning a FIELD_DECL node.
   QUALS is a list of type qualifiers for this decl (such as for declaring
   const member functions).

   This is done during the parsing of the struct declaration.
   The FIELD_DECL nodes are chained together and the lot of them
   are ultimately passed to `build_struct' to make the RECORD_TYPE node.

   C++:

   If class A defines that certain functions in class B are friends, then
   the way I have set things up, it is B who is interested in permission
   granted by A.  However, it is in A's context that these declarations
   are parsed.  By returning a void_type_node, class A does not attempt
   to incorporate the declarations of the friends within its structure.

   DO NOT MAKE ANY CHANGES TO THIS CODE WITHOUT MAKING CORRESPONDING
   CHANGES TO CODE IN `start_method'.  */

tree
grokfield (declarator, declspecs, raises, init, asmspec_tree)
     tree declarator, declspecs, raises, init, asmspec_tree;
{
  register tree value;
  char *asmspec = 0;

  /* Convert () initializers to = initializers.  */
  if (init == NULL_TREE && declarator != NULL_TREE
      && TREE_CODE (declarator) == CALL_EXPR
      && TREE_OPERAND (declarator, 0)
      && (TREE_CODE (TREE_OPERAND (declarator, 0)) == IDENTIFIER_NODE
	  || TREE_CODE (TREE_OPERAND (declarator, 0)) == SCOPE_REF)
      && parmlist_is_exprlist (TREE_OPERAND (declarator, 1)))
    {
      init = TREE_OPERAND (declarator, 1);
      declarator = TREE_OPERAND (declarator, 0);
    }

  if (init
      && TREE_CODE (init) == TREE_LIST
      && TREE_VALUE (init) == error_mark_node
      && TREE_CHAIN (init) == NULL_TREE)
	init = NULL_TREE;

  value = grokdeclarator (declarator, declspecs, FIELD, init != 0, raises);
  if (! value)
    return NULL_TREE; /* friends went bad.  */

  /* Pass friendly classes back.  */
  if (TREE_CODE (value) == VOID_TYPE)
    return void_type_node;

  if (DECL_NAME (value) != NULL_TREE
      && IDENTIFIER_POINTER (DECL_NAME (value))[0] == '_'
      && ! strcmp (IDENTIFIER_POINTER (DECL_NAME (value)), "_vptr"))
    error_with_decl (value, "member `%s' conflicts with virtual function table field name");

  /* Stash away type declarations.  */
  if (TREE_CODE (value) == TYPE_DECL)
    {
      DECL_NONLOCAL (value) = 1;
      CLASSTYPE_LOCAL_TYPEDECLS (current_class_type) = 1;
      set_identifier_type_value (DECL_NAME (value), TREE_TYPE (value));
      pushdecl_class_level (value);
      return value;
    }

  if (DECL_IN_AGGR_P (value))
    {
      error_with_decl (value, "`%s' is already defined in the class %s",
		       TYPE_NAME_STRING (DECL_CONTEXT (value)));
      return void_type_node;
    }

  if (flag_cadillac)
    cadillac_start_decl (value);

  if (asmspec_tree)
    asmspec = TREE_STRING_POINTER (asmspec_tree);

  if (init != 0)
    {
      if (TREE_CODE (value) == FUNCTION_DECL)
	{
	  grok_function_init (value, init);
	  init = NULL_TREE;
	}
      else if (pedantic)
	{
	  error ("fields cannot have initializers");
	  init = NULL_TREE;
	}
      else
	{
	  /* We allow initializers to become parameters to base initializers.  */
	  if (TREE_CODE (init) == TREE_LIST)
	    {
	      if (TREE_CHAIN (init) == NULL_TREE)
		init = TREE_VALUE (init);
	      else
		init = digest_init (TREE_TYPE (value), init, (tree *)0);
	    }

	  if (TREE_CODE (init) == CONST_DECL)
	    init = DECL_INITIAL (init);
	  else if (TREE_READONLY_DECL_P (init))
	    init = decl_constant_value (init);
	  else if (TREE_CODE (init) == CONSTRUCTOR)
	    init = digest_init (TREE_TYPE (value), init, (tree *)0);
	  my_friendly_assert (TREE_PERMANENT (init), 192);
	  if (init == error_mark_node)
	    /* We must make this look different than `error_mark_node'
	       because `decl_const_value' would mis-interpret it
	       as only meaning that this VAR_DECL is defined.  */
	    init = build1 (NOP_EXPR, TREE_TYPE (value), init);
	  else if (! TREE_CONSTANT (init))
	    {
	      /* We can allow references to things that are effectively
		 static, since references are initialized with the address.  */
	      if (TREE_CODE (TREE_TYPE (value)) != REFERENCE_TYPE
		  || (TREE_STATIC (init) == 0
		      && (TREE_CODE_CLASS (TREE_CODE (init)) != 'd'
			  || DECL_EXTERNAL (init) == 0)))
		{
		  error ("field initializer is not constant");
		  init = error_mark_node;
		}
	    }
	}
    }

  /* The corresponding pop_obstacks is in finish_decl.  */
  push_obstacks_nochange ();

  if (TREE_CODE (value) == VAR_DECL)
    {
      /* We cannot call pushdecl here, because that would
	 fill in the value of our TREE_CHAIN.  Instead, we
	 modify finish_decl to do the right thing, namely, to
	 put this decl out straight away.  */
      if (TREE_STATIC (value))
	{
	  /* current_class_type can be NULL_TREE in case of error.  */
	  if (asmspec == 0 && current_class_type)
	    {
	      tree name;
	      char *buf, *buf2;

	      buf2 = build_overload_name (current_class_type, 1, 1);
	      buf = (char *)alloca (IDENTIFIER_LENGTH (DECL_NAME (value))
				    + sizeof (STATIC_NAME_FORMAT)
				    + strlen (buf2));
	      sprintf (buf, STATIC_NAME_FORMAT, buf2,
		       IDENTIFIER_POINTER (DECL_NAME (value)));
	      name = get_identifier (buf);
	      TREE_PUBLIC (value) = 1;
	      DECL_INITIAL (value) = error_mark_node;
	      DECL_ASSEMBLER_NAME (value) = name;
	    }
	  pending_statics = perm_tree_cons (NULL_TREE, value, pending_statics);

	  /* Static consts need not be initialized in the class definition.  */
	  if (init != NULL_TREE && TYPE_NEEDS_CONSTRUCTING (TREE_TYPE (value)))
	    {
	      static int explanation = 0;

	      error ("initializer invalid for static member with constructor");
	      if (explanation++ == 0)
		error ("(you really want to initialize it separately)");
	      init = 0;
	    }
	  /* Force the compiler to know when an uninitialized static
	     const member is being used.  */
	  if (TYPE_READONLY (value) && init == 0)
	    TREE_USED (value) = 1;
	}
      DECL_INITIAL (value) = init;
      DECL_IN_AGGR_P (value) = 1;

      finish_decl (value, init, asmspec_tree, 1);
      pushdecl_class_level (value);
      return value;
    }
  if (TREE_CODE (value) == FIELD_DECL)
    {
      if (asmspec)
	DECL_ASSEMBLER_NAME (value) = get_identifier (asmspec);
      if (DECL_INITIAL (value) == error_mark_node)
	init = error_mark_node;
      finish_decl (value, init, asmspec_tree, 1);
      DECL_INITIAL (value) = init;
      DECL_IN_AGGR_P (value) = 1;
      return value;
    }
  if (TREE_CODE (value) == FUNCTION_DECL)
    {
      /* grokdeclarator defers setting this.  */
      TREE_PUBLIC (value) = 1;
      if (DECL_CHAIN (value) != NULL_TREE)
	{
	  /* Need a fresh node here so that we don't get circularity
	     when we link these together.  */
	  value = copy_node (value);
	  /* When does this happen?  */
	  my_friendly_assert (init == NULL_TREE, 193);
	}
      finish_decl (value, init, asmspec_tree, 1);

      /* Pass friends back this way.  */
      if (DECL_FRIEND_P (value))
	return void_type_node;

      DECL_IN_AGGR_P (value) = 1;
      return value;
    }
  my_friendly_abort (21);
  /* NOTREACHED */
  return NULL_TREE;
}

/* Like `grokfield', but for bitfields.
   WIDTH is non-NULL for bit fields only, and is an INTEGER_CST node.  */

tree
grokbitfield (declarator, declspecs, width)
     tree declarator, declspecs, width;
{
  register tree value = grokdeclarator (declarator, declspecs, BITFIELD, 0, NULL_TREE);

  if (! value) return NULL_TREE; /* friends went bad.  */

  /* Pass friendly classes back.  */
  if (TREE_CODE (value) == VOID_TYPE)
    return void_type_node;

  if (TREE_CODE (value) == TYPE_DECL)
    {
      error_with_decl (value, "cannot declare `%s' to be a bitfield type");
      return NULL_TREE;
    }

  if (DECL_IN_AGGR_P (value))
    {
      error_with_decl (value, "`%s' is already defined in the class %s",
		       TYPE_NAME_STRING (DECL_CONTEXT (value)));
      return void_type_node;
    }

  GNU_xref_member (current_class_name, value);

  if (TREE_STATIC (value))
    {
      error_with_decl (value, "static member `%s' cannot be a bitfield");
      return NULL_TREE;
    }
  finish_decl (value, NULL_TREE, NULL_TREE, 0);

  if (width != error_mark_node)
    {
      /* detect invalid field size.  */
      if (TREE_CODE (width) == CONST_DECL)
	width = DECL_INITIAL (width);
      else if (TREE_READONLY_DECL_P (width))
	width = decl_constant_value (width);
      if (TREE_CODE (width) != INTEGER_CST)
	{
	  error_with_decl (value, "structure field `%s' width not an integer constant");
	  DECL_INITIAL (value) = NULL_TREE;
	}
      else
	{
	  DECL_INITIAL (value) = width;
	  DECL_BIT_FIELD (value) = 1;
	}
    }

  DECL_IN_AGGR_P (value) = 1;
  return value;
}

/* Like GROKFIELD, except that the declarator has been
   buried in DECLSPECS.  Find the declarator, and
   return something that looks like it came from
   GROKFIELD.  */
tree
groktypefield (declspecs, parmlist)
     tree declspecs;
     tree parmlist;
{
  tree spec = declspecs;
  tree prev = NULL_TREE;

  tree type_id = NULL_TREE;
  tree quals = NULL_TREE;
  tree lengths = NULL_TREE;
  tree decl = NULL_TREE;

  while (spec)
    {
      register tree id = TREE_VALUE (spec);

      if (TREE_CODE (spec) != TREE_LIST)
	/* Certain parse errors slip through.  For example,
	   `int class ();' is not caught by the parser. Try
	   weakly to recover here.  */
	return NULL_TREE;

      if (TREE_CODE (id) == TYPE_DECL
	  || (TREE_CODE (id) == IDENTIFIER_NODE && TREE_TYPE (id)))
	{
	  /* We have a constructor/destructor or
	     conversion operator.  Use it.  */
	  if (prev)
	    TREE_CHAIN (prev) = TREE_CHAIN (spec);
	  else
	    declspecs = TREE_CHAIN (spec);

	  type_id = id;
	  goto found;
	}
      prev = spec;
      spec = TREE_CHAIN (spec);
    }

  /* Nope, we have a conversion operator to a scalar type or something
     else, that includes things like constructor declarations for
     templates.  */
  spec = declspecs;
  while (spec)
    {
      tree id = TREE_VALUE (spec);

      if (TREE_CODE (id) == IDENTIFIER_NODE)
	{
	  if (id == ridpointers[(int)RID_INT]
	      || id == ridpointers[(int)RID_DOUBLE]
	      || id == ridpointers[(int)RID_FLOAT]
	      || id == ridpointers[(int)RID_WCHAR])
	    {
	      if (type_id)
		error ("extra `%s' ignored",
		       IDENTIFIER_POINTER (id));
	      else
		type_id = id;
	    }
	  else if (id == ridpointers[(int)RID_LONG]
		   || id == ridpointers[(int)RID_SHORT]
		   || id == ridpointers[(int)RID_CHAR])
	    {
	      lengths = tree_cons (NULL_TREE, id, lengths);
	    }
	  else if (id == ridpointers[(int)RID_VOID])
	    {
	      if (type_id)
		error ("spurious `void' type ignored");
	      else
		error ("conversion to `void' type invalid");
	    }
	  else if (id == ridpointers[(int)RID_AUTO]
		   || id == ridpointers[(int)RID_REGISTER]
		   || id == ridpointers[(int)RID_TYPEDEF]
		   || id == ridpointers[(int)RID_CONST]
		   || id == ridpointers[(int)RID_VOLATILE])
	    {
	      error ("type specifier `%s' used invalidly",
		     IDENTIFIER_POINTER (id));
	    }
	  else if (id == ridpointers[(int)RID_FRIEND]
		   || id == ridpointers[(int)RID_VIRTUAL]
		   || id == ridpointers[(int)RID_INLINE]
		   || id == ridpointers[(int)RID_UNSIGNED]
		   || id == ridpointers[(int)RID_SIGNED]
		   || id == ridpointers[(int)RID_STATIC]
		   || id == ridpointers[(int)RID_EXTERN])
	    {
	      quals = tree_cons (NULL_TREE, id, quals);
	    }
	  else
	    {
	      /* Happens when we have a global typedef
		 and a class-local member function with
		 the same name.  */
	      type_id = id;
	      goto found;
	    }
	}
      else if (TREE_CODE (id) == RECORD_TYPE)
	{
	  type_id = TYPE_NAME (id);
	  if (TREE_CODE (type_id) == TYPE_DECL)
	    type_id = DECL_NAME (type_id);
	  if (type_id == NULL_TREE)
	    error ("identifier for aggregate type conversion omitted");
	}
      else if (TREE_CODE_CLASS (TREE_CODE (id)) == 't')
	error ("`operator' missing on conversion operator or tag missing from type");
      else
	my_friendly_abort (194);
      spec = TREE_CHAIN (spec);
    }

  if (type_id)
    declspecs = chainon (lengths, quals);
  else if (lengths)
    {
      if (TREE_CHAIN (lengths))
	error ("multiple length specifiers");
      type_id = ridpointers[(int)RID_INT];
      declspecs = chainon (lengths, quals);
    }
  else if (quals)
    {
      error ("no type given, defaulting to `operator int ...'");
      type_id = ridpointers[(int)RID_INT];
      declspecs = quals;
    }
  else
    return NULL_TREE;

 found:
  decl = grokdeclarator (build_parse_node (CALL_EXPR, type_id, parmlist, NULL_TREE),
			 declspecs, FIELD, 0, NULL_TREE);
  if (decl == NULL_TREE)
    return NULL_TREE;

  if (TREE_CODE (decl) == FUNCTION_DECL && DECL_CHAIN (decl) != NULL_TREE)
    {
      /* Need a fresh node here so that we don't get circularity
	 when we link these together.  */
      decl = copy_node (decl);
    }

  if (decl == void_type_node
      || (TREE_CODE (decl) == FUNCTION_DECL
	  && TREE_CODE (TREE_TYPE (decl)) != METHOD_TYPE))
    /* bunch of friends.  */
    return decl;

  if (DECL_IN_AGGR_P (decl))
    {
      error_with_decl (decl, "`%s' already defined in the class ");
      return void_type_node;
    }

  finish_decl (decl, NULL_TREE, NULL_TREE, 0);

  /* If this declaration is common to another declaration
     complain about such redundancy, and return NULL_TREE
     so that we don't build a circular list.  */
  if (DECL_CHAIN (decl))
    {
      error_with_decl (decl, "function `%s' declared twice in class %s",
		       TYPE_NAME_STRING (DECL_CONTEXT (decl)));
      return NULL_TREE;
    }
  DECL_IN_AGGR_P (decl) = 1;
  return decl;
}

/* The precedence rules of this grammar (or any other deterministic LALR
   grammar, for that matter), place the CALL_EXPR somewhere where we
   may not want it.  The solution is to grab the first CALL_EXPR we see,
   pretend that that is the one that belongs to the parameter list of
   the type conversion function, and leave everything else alone.
   We pull it out in place.

   CALL_REQUIRED is non-zero if we should complain if a CALL_EXPR
   does not appear in DECL.  */
tree
grokoptypename (decl, call_required)
     tree decl;
     int call_required;
{
  tree tmp, last;

  my_friendly_assert (TREE_CODE (decl) == TYPE_EXPR, 195);

  tmp = TREE_OPERAND (decl, 0);
  last = NULL_TREE;

  while (tmp)
    {
      switch (TREE_CODE (tmp))
	{
	case CALL_EXPR:
	  {
	    tree parms = TREE_OPERAND (tmp, 1);

	    if (last)
	      TREE_OPERAND (last, 0) = TREE_OPERAND (tmp, 0);
	    else
	      TREE_OPERAND (decl, 0) = TREE_OPERAND (tmp, 0);
	    if (parms
		&& TREE_CODE (TREE_VALUE (parms)) == TREE_LIST)
	      TREE_VALUE (parms)
		= grokdeclarator (TREE_VALUE (TREE_VALUE (parms)),
				  TREE_PURPOSE (TREE_VALUE (parms)),
				  TYPENAME, 0, NULL_TREE);
	    if (parms)
	      {
		if (TREE_VALUE (parms) != void_type_node)
		  error ("operator <typename> requires empty parameter list");
		else
		  /* Canonicalize parameter lists.  */
		  TREE_OPERAND (tmp, 1) = void_list_node;
	      }

	    last = grokdeclarator (TREE_OPERAND (decl, 0),
				   TREE_TYPE (decl),
				   TYPENAME, 0, NULL_TREE);
	    TREE_OPERAND (tmp, 0) = build_typename_overload (last);
	    TREE_TYPE (TREE_OPERAND (tmp, 0)) = last;
	    return tmp;
	  }

	case INDIRECT_REF:
	case ADDR_EXPR:
	case ARRAY_REF:
	  break;

	case SCOPE_REF:
	  /* This is legal when declaring a conversion to
	     something of type pointer-to-member.  */
	  if (TREE_CODE (TREE_OPERAND (tmp, 1)) == INDIRECT_REF)
	    {
	      tmp = TREE_OPERAND (tmp, 1);
	    }
	  else
	    {
#if 0
	      /* We may need to do this if grokdeclarator cannot handle this.  */
	      error ("type `member of class %s' invalid return type",
		     TYPE_NAME_STRING (TREE_OPERAND (tmp, 0)));
	      TREE_OPERAND (tmp, 1) = build_parse_node (INDIRECT_REF, TREE_OPERAND (tmp, 1));
#endif
	      tmp = TREE_OPERAND (tmp, 1);
	    }
	  break;

	default:
	  my_friendly_abort (196);
	}
      last = tmp;
      tmp = TREE_OPERAND (tmp, 0);
    }

  if (call_required)
    error ("operator <typename> construct requires parameter list");

  last = grokdeclarator (TREE_OPERAND (decl, 0),
			 TREE_TYPE (decl),
			 TYPENAME, 0, NULL_TREE);
  tmp = build_parse_node (CALL_EXPR, build_typename_overload (last),
			  void_list_node, NULL_TREE);
  TREE_TYPE (TREE_OPERAND (tmp, 0)) = last;
  return tmp;
}

/* When a function is declared with an initializer,
   do the right thing.  Currently, there are two possibilities:

   class B
   {
    public:
     // initialization possibility #1.
     virtual void f () = 0;
     int g ();
   };
   
   class D1 : B
   {
    public:
     int d1;
     // error, no f ();
   };
   
   class D2 : B
   {
    public:
     int d2;
     void f ();
   };
   
   class D3 : B
   {
    public:
     int d3;
     // initialization possibility #2
     void f () = B::f;
   };

*/

static void
grok_function_init (decl, init)
     tree decl;
     tree init;
{
  /* An initializer for a function tells how this function should
     be inherited.  */
  tree type = TREE_TYPE (decl);
  extern tree abort_fndecl;

  if (TREE_CODE (type) == FUNCTION_TYPE)
    error_with_decl (decl, "initializer specified for non-member function `%s'");
  else if (DECL_VINDEX (decl) == NULL_TREE)
    error_with_decl (decl, "initializer specified for non-virtual method `%s'");
  else if (integer_zerop (init))
    {
      /* Mark this function as being "defined".  */
      DECL_INITIAL (decl) = error_mark_node;
      /* pure virtual destructors must be defined. */
      if (!DESTRUCTOR_NAME_P (DECL_ASSEMBLER_NAME (decl)))
	{
	  /* Give this node rtl from `abort'.  */
	  DECL_RTL (decl) = DECL_RTL (abort_fndecl);
	}
      DECL_ABSTRACT_VIRTUAL_P (decl) = 1;
    }
  else if (TREE_CODE (init) == OFFSET_REF
	   && TREE_OPERAND (init, 0) == NULL_TREE
	   && TREE_CODE (TREE_TYPE (init)) == METHOD_TYPE)
    {
      tree basetype = DECL_CLASS_CONTEXT (init);
      tree basefn = TREE_OPERAND (init, 1);
      if (TREE_CODE (basefn) != FUNCTION_DECL)
	error_with_decl (decl, "non-method initializer invalid for method `%s'");
      else if (! BINFO_OFFSET_ZEROP (TYPE_BINFO (DECL_CLASS_CONTEXT (basefn))))
	sorry ("base member function from other than first base class");
      else
	{
	  tree binfo = get_binfo (basetype, TYPE_METHOD_BASETYPE (type), 1);
	  if (binfo == error_mark_node)
	    ;
	  else if (binfo == 0)
	    error_not_base_type (TYPE_METHOD_BASETYPE (TREE_TYPE (init)),
				 TYPE_METHOD_BASETYPE (type));
	  else
	    {
	      /* Mark this function as being defined,
		 and give it new rtl.  */
	      DECL_INITIAL (decl) = error_mark_node;
	      DECL_RTL (decl) = DECL_RTL (basefn);
	    }
	}
    }
  else
    error_with_decl (decl, "invalid initializer for virtual method `%s'");
}

/* When we get a declaration of the form

   type cname::fname ...

   the node for `cname::fname' gets built here in a special way.
   Namely, we push into `cname's scope.  When this declaration is
   processed, we pop back out.  */
tree
build_push_scope (cname, name)
     tree cname;
     tree name;
{
  extern int current_class_depth;
  tree ctype, rval;

  if (cname == error_mark_node)
    return error_mark_node;

  ctype = IDENTIFIER_TYPE_VALUE (cname);

  if (ctype == NULL_TREE || ! IS_AGGR_TYPE (ctype))
    {
      error ("`%s' not defined as aggregate type", IDENTIFIER_POINTER (cname));
      return name;
    }

  rval = build_parse_node (SCOPE_REF, cname, name);

  /* Don't need to push the scope here, but we do need to return
     a SCOPE_REF for something like

     class foo { typedef int bar (foo::*foo_fn)(void); };  */
  if (ctype == current_class_type)
    return rval;

  pushclass (ctype, 3);
  TREE_COMPLEXITY (rval) = current_class_depth;
  return rval;
}

void cplus_decl_attributes (decl, attributes)
     tree decl, attributes;
{
  if (decl)
    decl_attributes (decl, attributes);
}

/* CONSTRUCTOR_NAME:
   Return the name for the constructor (or destructor) for the specified
   class.  Argument can be RECORD_TYPE, TYPE_DECL, or IDENTIFIER_NODE.  */
tree
constructor_name (thing)
     tree thing;
{
  tree t;
  if (TREE_CODE (thing) == UNINSTANTIATED_P_TYPE)
    return DECL_NAME (UPT_TEMPLATE (thing));
  if (TREE_CODE (thing) == RECORD_TYPE || TREE_CODE (thing) == UNION_TYPE)
    thing = TYPE_NAME (thing);
  if (TREE_CODE (thing) == TYPE_DECL
      || (TREE_CODE (thing) == TEMPLATE_DECL
	  && DECL_TEMPLATE_IS_CLASS (thing)))
    thing = DECL_NAME (thing);
  my_friendly_assert (TREE_CODE (thing) == IDENTIFIER_NODE, 197);
  t = IDENTIFIER_TEMPLATE (thing);
  if (!t)
    return thing;
  t = TREE_PURPOSE (t);
  return DECL_NAME (t);
}

/* Cache the value of this class's main virtual function table pointer
   in a register variable.  This will save one indirection if a
   more than one virtual function call is made this function.  */
void
setup_vtbl_ptr ()
{
  extern rtx base_init_insns;

  if (base_init_insns == 0
      && DECL_CONSTRUCTOR_P (current_function_decl))
    emit_base_init (current_class_type, 0);

#if 0
  /* This has something a little wrong with it.

     On a sun4, code like:

        be L6
        ld [%i0],%o1

     is generated, when the below is used when -O4 is given.  The delay
     slot it filled with an instruction that is safe, when this isn't
     used, like in:

        be L6
        sethi %hi(LC1),%o0
        ld [%i0],%o1

     on code like:

        struct A {
          virtual void print() { printf("xxx"); }
          void f();
        };

        void A::f() {
          if (this) {
            print();
          } else {
            printf("0");
          }
        }

     And that is why this is disabled for now. (mrs)
  */

  if ((flag_this_is_variable & 1) == 0
      && optimize
      && current_class_type
      && CLASSTYPE_VSIZE (current_class_type)
      && ! DECL_STATIC_FUNCTION_P (current_function_decl))
    {
      tree vfield = build_vfield_ref (C_C_D, current_class_type);
      current_vtable_decl = CLASSTYPE_VTBL_PTR (current_class_type);
      DECL_RTL (current_vtable_decl) = 0;
      DECL_INITIAL (current_vtable_decl) = error_mark_node;
      /* Have to cast the initializer, since it may have come from a
	 more base class then we ascribe CURRENT_VTABLE_DECL to be.  */
      finish_decl (current_vtable_decl, convert_force (TREE_TYPE (current_vtable_decl), vfield), 0, 0);
      current_vtable_decl = build_indirect_ref (current_vtable_decl, 0);
    }
  else
#endif
    current_vtable_decl = NULL_TREE;
}

/* Record the existence of an addressable inline function.  */
void
mark_inline_for_output (decl)
     tree decl;
{
  if (DECL_PENDING_INLINE_INFO (decl) != 0
      && ! DECL_PENDING_INLINE_INFO (decl)->deja_vu)
    {
      struct pending_inline *t = pending_inlines;
      my_friendly_assert (DECL_SAVED_INSNS (decl) == 0, 198);
      while (t)
	{
	  if (t == DECL_PENDING_INLINE_INFO (decl))
	    break;
	  t = t->next;
	}
      if (t == 0)
	{
	  t = DECL_PENDING_INLINE_INFO (decl);
	  t->next = pending_inlines;
	  pending_inlines = t;
	}
      DECL_PENDING_INLINE_INFO (decl) = 0;
    }
  pending_addressable_inlines = perm_tree_cons (NULL_TREE, decl,
						pending_addressable_inlines);
}

void
clear_temp_name ()
{
  temp_name_counter = 0;
}

/* Hand off a unique name which can be used for variable we don't really
   want to know about anyway, for example, the anonymous variables which
   are needed to make references work.  Declare this thing so we can use it.
   The variable created will be of type TYPE.

   STATICP is nonzero if this variable should be static.  */

tree
get_temp_name (type, staticp)
     tree type;
     int staticp;
{
  char buf[sizeof (AUTO_TEMP_FORMAT) + 20];
  tree decl;
  int toplev = global_bindings_p ();

  push_obstacks_nochange ();
  if (toplev || staticp)
    {
      end_temporary_allocation ();
      sprintf (buf, AUTO_TEMP_FORMAT, global_temp_name_counter++);
      decl = pushdecl_top_level (build_decl (VAR_DECL, get_identifier (buf), type));
    }
  else
    {
      sprintf (buf, AUTO_TEMP_FORMAT, temp_name_counter++);
      decl = pushdecl (build_decl (VAR_DECL, get_identifier (buf), type));
    }
  TREE_USED (decl) = 1;
  TREE_STATIC (decl) = staticp;

  /* If this is a local variable, then lay out its rtl now.
     Otherwise, callers of this function are responsible for dealing
     with this variable's rtl.  */
  if (! toplev)
    {
      expand_decl (decl);
      expand_decl_init (decl);
    }
  pop_obstacks ();

  return decl;
}

/* Get a variable which we can use for multiple assignments.
   It is not entered into current_binding_level, because
   that breaks things when it comes time to do final cleanups
   (which take place "outside" the binding contour of the function).  */
tree
get_temp_regvar (type, init)
     tree type, init;
{
  static char buf[sizeof (AUTO_TEMP_FORMAT) + 20] = { '_' };
  tree decl;

  sprintf (buf+1, AUTO_TEMP_FORMAT, temp_name_counter++);
  decl = build_decl (VAR_DECL, get_identifier (buf), type);
  TREE_USED (decl) = 1;
  DECL_REGISTER (decl) = 1;

  if (init)
    store_init_value (decl, init);

  /* We can expand these without fear, since they cannot need
     constructors or destructors.  */
  expand_decl (decl);
  expand_decl_init (decl);

  if (type_needs_gc_entry (type))
    DECL_GC_OFFSET (decl) = size_int (++current_function_obstack_index);

  return decl;
}

/* Make the macro TEMP_NAME_P available to units which do not
   include c-tree.h.  */
int
temp_name_p (decl)
     tree decl;
{
  return TEMP_NAME_P (decl);
}

/* Finish off the processing of a UNION_TYPE structure.
   If there are static members, then all members are
   static, and must be laid out together.  If the
   union is an anonymous union, we arrange for that
   as well.  PUBLIC_P is nonzero if this union is
   not declared static.  */
void
finish_anon_union (anon_union_decl)
     tree anon_union_decl;
{
  tree type = TREE_TYPE (anon_union_decl);
  tree field, main_decl = NULL_TREE;
  tree elems = NULL_TREE;
  int public_p = TREE_PUBLIC (anon_union_decl);
  int static_p = TREE_STATIC (anon_union_decl);
  int external_p = DECL_EXTERNAL (anon_union_decl);

  if ((field = TYPE_FIELDS (type)) == NULL_TREE)
    return;

  if (public_p && (static_p || external_p))
    error ("optimizer cannot handle global anonymous unions");

  while (field)
    {
      tree decl = build_decl (VAR_DECL, DECL_NAME (field), TREE_TYPE (field));
      /* tell `pushdecl' that this is not tentative.  */
      DECL_INITIAL (decl) = error_mark_node;
      TREE_PUBLIC (decl) = public_p;
      TREE_STATIC (decl) = static_p;
      DECL_EXTERNAL (decl) = external_p;
      decl = pushdecl (decl);

      /* Only write out one anon union element--choose the one that
	 can hold them all.  */
      if (main_decl == NULL_TREE
	  && DECL_SIZE (decl) == DECL_SIZE (anon_union_decl))
	{
	  main_decl = decl;
	}
      else
	{
	  /* ??? This causes there to be no debug info written out
	     about this decl.  */
	  TREE_ASM_WRITTEN (decl) = 1;
	}

      DECL_INITIAL (decl) = NULL_TREE;
      /* If there's a cleanup to do, it belongs in the
	 TREE_PURPOSE of the following TREE_LIST.  */
      elems = tree_cons (NULL_TREE, decl, elems);
      TREE_TYPE (elems) = type;
      field = TREE_CHAIN (field);
    }
  if (static_p)
    {
      make_decl_rtl (main_decl, 0, global_bindings_p ());
      DECL_RTL (anon_union_decl) = DECL_RTL (main_decl);
    }

  /* The following call assumes that there are never any cleanups
     for anonymous unions--a reasonable assumption.  */
  expand_anon_union_decl (anon_union_decl, NULL_TREE, elems);

  if (flag_cadillac)
    cadillac_finish_anon_union (anon_union_decl);
}

/* Finish and output a table which is generated by the compiler.
   NAME is the name to give the table.
   TYPE is the type of the table entry.
   INIT is all the elements in the table.
   PUBLICP is non-zero if this table should be given external visibility.  */
tree
finish_table (name, type, init, publicp)
     tree name, type, init;
     int publicp;
{
  tree itype, atype, decl;
  static tree empty_table;
  int is_empty = 0;
  tree asmspec;

  itype = build_index_type (size_int (list_length (init) - 1));
  atype = build_cplus_array_type (type, itype);
  layout_type (atype);

  if (TREE_VALUE (init) == integer_zero_node
      && TREE_CHAIN (init) == NULL_TREE)
    {
      if (empty_table == NULL_TREE)
	{
	  empty_table = get_temp_name (atype, 1);
	  init = build (CONSTRUCTOR, atype, NULL_TREE, init);
	  TREE_CONSTANT (init) = 1;
	  TREE_STATIC (init) = 1;
	  DECL_INITIAL (empty_table) = init;
	  asmspec = build_string (IDENTIFIER_LENGTH (DECL_NAME (empty_table)),
				  IDENTIFIER_POINTER (DECL_NAME (empty_table)));
	  finish_decl (empty_table, init, asmspec, 0);
	}
      is_empty = 1;
    }

  if (name == NULL_TREE)
    {
      if (is_empty)
	return empty_table;
      decl = get_temp_name (atype, 1);
    }
  else
    {
      decl = build_decl (VAR_DECL, name, atype);
      decl = pushdecl (decl);
      TREE_STATIC (decl) = 1;
    }

  if (is_empty == 0)
    {
      TREE_PUBLIC (decl) = publicp;
      init = build (CONSTRUCTOR, atype, NULL_TREE, init);
      TREE_CONSTANT (init) = 1;
      TREE_STATIC (init) = 1;
      DECL_INITIAL (decl) = init;
      asmspec = build_string (IDENTIFIER_LENGTH (DECL_NAME (decl)),
			      IDENTIFIER_POINTER (DECL_NAME (decl)));
    }
  else
    {
      /* This will cause DECL to point to EMPTY_TABLE in rtl-land.  */
      DECL_EXTERNAL (decl) = 1;
      TREE_STATIC (decl) = 0;
      init = 0;
      asmspec = build_string (IDENTIFIER_LENGTH (DECL_NAME (empty_table)),
			      IDENTIFIER_POINTER (DECL_NAME (empty_table)));
    }

  finish_decl (decl, init, asmspec, 0);
  return decl;
}

/* Finish processing a builtin type TYPE.  It's name is NAME,
   its fields are in the array FIELDS.  LEN is the number of elements
   in FIELDS.

   It is given the same alignment as ALIGN_TYPE.  */
void
finish_builtin_type (type, name, fields, len, align_type)
     tree type;
     char *name;
     tree fields[];
     int len;
     tree align_type;
{
  register int i;

  TYPE_FIELDS (type) = fields[0];
  for (i = 0; i < len; i++)
    {
      layout_type (TREE_TYPE (fields[i]));
      DECL_FIELD_CONTEXT (fields[i]) = type;
      TREE_CHAIN (fields[i]) = fields[i+1];
    }
  DECL_FIELD_CONTEXT (fields[i]) = type;
  DECL_CLASS_CONTEXT (fields[i]) = type;
  TYPE_ALIGN (type) = TYPE_ALIGN (align_type);
  layout_type (type);
#if 0 /* not yet, should get fixed properly later */
  TYPE_NAME (type) = make_type_decl (get_identifier (name), type);
#else
  TYPE_NAME (type) = build_decl (TYPE_DECL, get_identifier (name), type);
#endif
  layout_decl (TYPE_NAME (type), 0);
}

/* Auxiliary functions to make type signatures for
   `operator new' and `operator delete' correspond to
   what compiler will be expecting.  */

extern tree sizetype;

tree
coerce_new_type (type)
     tree type;
{
  int e1 = 0, e2 = 0;

  if (TREE_CODE (type) == METHOD_TYPE)
    type = build_function_type (TREE_TYPE (type), TREE_CHAIN (TYPE_ARG_TYPES (type)));
  if (TREE_TYPE (type) != ptr_type_node)
    e1 = 1, error ("`operator new' must return type `void *'");

  /* Technically the type must be `size_t', but we may not know
     what that is.  */
  if (TYPE_ARG_TYPES (type) == NULL_TREE)
    e1 = 1, error ("`operator new' takes type `size_t' parameter");
  else if (TREE_CODE (TREE_VALUE (TYPE_ARG_TYPES (type))) != INTEGER_TYPE
	   || TYPE_PRECISION (TREE_VALUE (TYPE_ARG_TYPES (type))) != TYPE_PRECISION (sizetype))
    e2 = 1, error ("`operator new' takes type `size_t' as first parameter");
  if (e2)
    type = build_function_type (ptr_type_node, tree_cons (NULL_TREE, sizetype, TREE_CHAIN (TYPE_ARG_TYPES (type))));
  else if (e1)
    type = build_function_type (ptr_type_node, TYPE_ARG_TYPES (type));
  return type;
}

tree
coerce_delete_type (type)
     tree type;
{
  int e1 = 0, e2 = 0, e3 = 0;
  tree arg_types = TYPE_ARG_TYPES (type);

  if (TREE_CODE (type) == METHOD_TYPE)
    {
      type = build_function_type (TREE_TYPE (type), TREE_CHAIN (arg_types));
      arg_types = TREE_CHAIN (arg_types);
    }
  if (TREE_TYPE (type) != void_type_node)
    e1 = 1, error ("`operator delete' must return type `void'");
  if (arg_types == NULL_TREE
      || TREE_VALUE (arg_types) != ptr_type_node)
    e2 = 1, error ("`operator delete' takes type `void *' as first parameter");

  if (arg_types
      && TREE_CHAIN (arg_types)
      && TREE_CHAIN (arg_types) != void_list_node)
    {
      /* Again, technically this argument must be `size_t', but again
	 we may not know what that is.  */
      tree t2 = TREE_VALUE (TREE_CHAIN (arg_types));
      if (TREE_CODE (t2) != INTEGER_TYPE
	  || TYPE_PRECISION (t2) != TYPE_PRECISION (sizetype))
	e3 = 1, error ("second argument to `operator delete' must be of type `size_t'");
      else if (TREE_CHAIN (TREE_CHAIN (arg_types)) != void_list_node)
	{
	  e3 = 1;
	  if (TREE_CHAIN (TREE_CHAIN (arg_types)))
	    error ("too many arguments in declaration of `operator delete'");
	  else
	    error ("`...' invalid in specification of `operator delete'");
	}
    }
  if (e3)
    arg_types = tree_cons (NULL_TREE, ptr_type_node, build_tree_list (NULL_TREE, sizetype));
  else if (e3 |= e2)
    {
      if (arg_types == NULL_TREE)
	arg_types = void_list_node;
      else
	arg_types = tree_cons (NULL_TREE, ptr_type_node, TREE_CHAIN (arg_types));
    }
  else e3 |= e1;

  if (e3)
    type = build_function_type (void_type_node, arg_types);

  return type;
}

static void
write_vtable_entries (decl)
     tree decl;
{
  tree entries = TREE_CHAIN (CONSTRUCTOR_ELTS (DECL_INITIAL (decl)));

  if (flag_dossier)
    entries = TREE_CHAIN (entries);

  for (; entries; entries = TREE_CHAIN (entries))
    {
      tree fnaddr = FNADDR_FROM_VTABLE_ENTRY (TREE_VALUE (entries));
      tree fn = TREE_OPERAND (fnaddr, 0);
      if (! DECL_EXTERNAL (fn) && ! TREE_ASM_WRITTEN (fn)
	  && DECL_SAVED_INSNS (fn))
	{
	  if (TREE_PUBLIC (DECL_CLASS_CONTEXT (fn)))
	    TREE_PUBLIC (fn) = 1;
	  TREE_ADDRESSABLE (fn) = 1;
	  output_inline_function (fn);
	}
      else
	assemble_external (fn);
    }
}

/* Note even though prev is never used in here, walk_vtables
   expects this to have two arguments, so concede.  */
static void
finish_vtable_typedecl (prev, vars)
     tree prev, vars;
{
  tree decl = TYPE_BINFO_VTABLE (TREE_TYPE (vars));

  /* If we are controlled by `+e2', obey.  */
  if (write_virtuals == 2)
    {
      tree binfo = value_member (DECL_NAME (vars), pending_vtables);
      if (binfo)
	TREE_PURPOSE (binfo) = void_type_node;
      else
	decl = NULL_TREE;
    }
  /* If this type has inline virtual functions, then
     write those functions out now.  */
  if (decl && write_virtuals >= 0
      && ! DECL_EXTERNAL (decl) && (TREE_PUBLIC (decl) || TREE_USED (decl)))
    write_vtable_entries (decl);
}

static void
finish_vtable_vardecl (prev, vars)
     tree prev, vars;
{
  if (write_virtuals >= 0
      && ! DECL_EXTERNAL (vars) && (TREE_PUBLIC (vars) || TREE_USED (vars)))
    {
      extern tree the_null_vtable_entry;

      /* Stuff this virtual function table's size into
	 `pfn' slot of `the_null_vtable_entry'.  */
      tree nelts = array_type_nelts (TREE_TYPE (vars));
      SET_FNADDR_FROM_VTABLE_ENTRY (the_null_vtable_entry, nelts);
      /* Kick out the dossier before writing out the vtable.  */
      if (flag_dossier)
	rest_of_decl_compilation (TREE_OPERAND (FNADDR_FROM_VTABLE_ENTRY (TREE_VALUE (TREE_CHAIN (CONSTRUCTOR_ELTS (DECL_INITIAL (vars))))), 0), 0, 1, 1);

      /* Write it out.  */
      write_vtable_entries (vars);
      if (TREE_TYPE (DECL_INITIAL (vars)) == 0)
	store_init_value (vars, DECL_INITIAL (vars));
      rest_of_decl_compilation (vars, 0, 1, 1);
    }
  /* We know that PREV must be non-zero here.  */
  TREE_CHAIN (prev) = TREE_CHAIN (vars);
}

void
walk_vtables (typedecl_fn, vardecl_fn)
     register void (*typedecl_fn)();
     register void (*vardecl_fn)();
{
  tree prev, vars;

  for (prev = 0, vars = getdecls (); vars; vars = TREE_CHAIN (vars))
    {
      if (TREE_CODE (vars) == TYPE_DECL
	  && TYPE_LANG_SPECIFIC (TREE_TYPE (vars))
	  && CLASSTYPE_VSIZE (TREE_TYPE (vars)))
	{
	  if (typedecl_fn) (*typedecl_fn) (prev, vars);
	}
      else if (TREE_CODE (vars) == VAR_DECL && DECL_VIRTUAL_P (vars))
	{
	  if (vardecl_fn) (*vardecl_fn) (prev, vars);
	}
      else
	prev = vars;
    }
}

extern int parse_time, varconst_time;

#define TIMEVAR(VAR, BODY)    \
do { int otime = get_run_time (); BODY; VAR += get_run_time () - otime; } while (0)

/* This routine is called from the last rule in yyparse ().
   Its job is to create all the code needed to initialize and
   destroy the global aggregates.  We do the destruction
   first, since that way we only need to reverse the decls once.  */

void
finish_file ()
{
  extern int lineno;
  int start_time, this_time;

  char *buf;
  char *p;
  tree fnname;
  tree vars = static_aggregates;
  int needs_cleaning = 0, needs_messing_up = 0;

  if (main_input_filename == 0)
    main_input_filename = input_filename;
  if (!first_global_object_name)
    first_global_object_name = main_input_filename;

  buf = (char *) alloca (sizeof (FILE_FUNCTION_FORMAT)
			 + strlen (first_global_object_name));

  if (flag_detailed_statistics)
    dump_tree_statistics ();

  /* Bad parse errors.  Just forget about it.  */
  if (! global_bindings_p () || current_class_type)
    return;

  start_time = get_run_time ();

  /* Push into C language context, because that's all
     we'll need here.  */
  push_lang_context (lang_name_c);

  /* Set up the name of the file-level functions we may need.  */
  /* Use a global object (which is already required to be unique over
     the program) rather than the file name (which imposes extra
     constraints).  -- Raeburn@MIT.EDU, 10 Jan 1990.  */
  sprintf (buf, FILE_FUNCTION_FORMAT, first_global_object_name);

  /* Don't need to pull wierd characters out of global names.  */
  if (first_global_object_name == main_input_filename)
    {
      for (p = buf+11; *p; p++)
	if (! ((*p >= '0' && *p <= '9')
#if 0 /* we always want labels, which are valid C++ identifiers (+ `$') */
#ifndef ASM_IDENTIFY_GCC	/* this is required if `.' is invalid -- k. raeburn */
	       || *p == '.'
#endif
#endif
#ifndef NO_DOLLAR_IN_LABEL	/* this for `$'; unlikely, but... -- kr */
	       || *p == '$'
#endif
#ifndef NO_DOT_IN_LABEL		/* this for `.'; unlikely, but... */
	       || *p == '.'
#endif
	       || (*p >= 'A' && *p <= 'Z')
	       || (*p >= 'a' && *p <= 'z')))
	  *p = '_';
    }

  /* See if we really need the hassle.  */
  while (vars && needs_cleaning == 0)
    {
      tree decl = TREE_VALUE (vars);
      tree type = TREE_TYPE (decl);
      if (TYPE_NEEDS_DESTRUCTOR (type))
	{
	  needs_cleaning = 1;
	  needs_messing_up = 1;
	  break;
	}
      else
	needs_messing_up |= TYPE_NEEDS_CONSTRUCTING (type);
      vars = TREE_CHAIN (vars);
    }
  if (needs_cleaning == 0)
    goto mess_up;

  /* Otherwise, GDB can get confused, because in only knows
     about source for LINENO-1 lines.  */
  lineno -= 1;

  fnname = get_identifier (buf);
  start_function (void_list_node, build_parse_node (CALL_EXPR, fnname, void_list_node, NULL_TREE), 0, 0);
  fnname = DECL_ASSEMBLER_NAME (current_function_decl);
  store_parm_decls ();

  pushlevel (0);
  clear_last_expr ();
  push_momentary ();
  expand_start_bindings (0);

  /* These must be done in backward order to destroy,
     in which they happen to be!  */
  while (vars)
    {
      tree decl = TREE_VALUE (vars);
      tree type = TREE_TYPE (decl);
      tree temp = TREE_PURPOSE (vars);

      if (TYPE_NEEDS_DESTRUCTOR (type))
	{
	  if (TREE_STATIC (vars))
	    expand_start_cond (build_binary_op (NE_EXPR, temp, integer_zero_node, 1), 0);
	  if (TREE_CODE (type) == ARRAY_TYPE)
	    temp = decl;
	  else
	    {
	      mark_addressable (decl);
	      temp = build1 (ADDR_EXPR, TYPE_POINTER_TO (type), decl);
	    }
	  temp = build_delete (TREE_TYPE (temp), temp,
			       integer_two_node, LOOKUP_NORMAL|LOOKUP_NONVIRTUAL|LOOKUP_DESTRUCTOR, 0, 0);
	  expand_expr_stmt (temp);

	  if (TREE_STATIC (vars))
	    expand_end_cond ();
	}
      vars = TREE_CHAIN (vars);
    }

  expand_end_bindings (getdecls (), 1, 0);
  poplevel (1, 0, 0);
  pop_momentary ();

  finish_function (lineno, 0);

  assemble_destructor (IDENTIFIER_POINTER (fnname));

  /* if it needed cleaning, then it will need messing up: drop through  */

 mess_up:
  /* Must do this while we think we are at the top level.  */
  vars = nreverse (static_aggregates);
  if (vars != NULL_TREE)
    {
      buf[FILE_FUNCTION_PREFIX_LEN] = 'I';

      fnname = get_identifier (buf);
      start_function (void_list_node, build_parse_node (CALL_EXPR, fnname, void_list_node, NULL_TREE), 0, 0);
      fnname = DECL_ASSEMBLER_NAME (current_function_decl);
      store_parm_decls ();

      pushlevel (0);
      clear_last_expr ();
      push_momentary ();
      expand_start_bindings (0);

      while (vars)
	{
	  tree decl = TREE_VALUE (vars);
	  tree init = TREE_PURPOSE (vars);

	  /* If this was a static attribute within some function's scope,
	     then don't initialize it here.  Also, don't bother
	     with initializers that contain errors.  */
	  if (TREE_STATIC (vars)
	      || (init && TREE_CODE (init) == TREE_LIST
		  && value_member (error_mark_node, init)))
	    {
	      vars = TREE_CHAIN (vars);
	      continue;
	    }

	  if (TREE_CODE (decl) == VAR_DECL)
	    {
	      /* Set these global variables so that GDB at least puts
		 us near the declaration which required the initialization.  */
	      input_filename = DECL_SOURCE_FILE (decl);
	      lineno = DECL_SOURCE_LINE (decl);
	      emit_note (input_filename, lineno);

	      if (init)
		{
		  if (TREE_CODE (init) == VAR_DECL)
		    {
		      /* This behavior results when there are
			 multiple declarations of an aggregate,
			 the last of which defines it.  */
		      if (DECL_RTL (init) == DECL_RTL (decl))
			{
			  my_friendly_assert (DECL_INITIAL (decl) == error_mark_node
				  || (TREE_CODE (DECL_INITIAL (decl)) == CONSTRUCTOR
				      && CONSTRUCTOR_ELTS (DECL_INITIAL (decl)) == NULL_TREE),
					      199);
			  init = DECL_INITIAL (init);
			  if (TREE_CODE (init) == CONSTRUCTOR
			      && CONSTRUCTOR_ELTS (init) == NULL_TREE)
			    init = NULL_TREE;
			}
#if 0
		      else if (TREE_TYPE (decl) == TREE_TYPE (init))
			{
#if 1
			  my_friendly_abort (200);
#else
			  /* point to real decl's rtl anyway.  */
			  DECL_RTL (init) = DECL_RTL (decl);
			  my_friendly_assert (DECL_INITIAL (decl) == error_mark_node,
					      201);
			  init = DECL_INITIAL (init);
#endif				/* 1 */
			}
#endif				/* 0 */
		    }
		}
	      if (IS_AGGR_TYPE (TREE_TYPE (decl))
		  || init == 0
		  || TREE_CODE (TREE_TYPE (decl)) == ARRAY_TYPE)
		{
		  /* Set this up so is_friend() works properly on _GLOBAL_ fns.  */
		  tree old_dcc = DECL_CLASS_CONTEXT (current_function_decl);
		  if (old_dcc == NULL_TREE)
		    DECL_CLASS_CONTEXT (current_function_decl) = TREE_TYPE (decl);
		  expand_aggr_init (decl, init, 0);
		  DECL_CLASS_CONTEXT (current_function_decl) = old_dcc;
		}
	      else if (TREE_CODE (init) == TREE_VEC)
		{
		  expand_expr (expand_vec_init (decl, TREE_VEC_ELT (init, 0),
						TREE_VEC_ELT (init, 1),
						TREE_VEC_ELT (init, 2), 0),
			       const0_rtx, VOIDmode, 0);
		  free_temp_slots ();
		}
	      else
		expand_assignment (decl, init, 0, 0);
	    }
	  else if (TREE_CODE (decl) == SAVE_EXPR)
	    {
	      if (! PARM_DECL_EXPR (decl))
		{
		  /* a `new' expression at top level.  */
		  expand_expr (decl, const0_rtx, VOIDmode, 0);
		  free_temp_slots ();
		  expand_aggr_init (build_indirect_ref (decl, 0), init, 0);
		}
	    }
	  else if (decl == error_mark_node)
	    ;
	  else my_friendly_abort (22);
	  vars = TREE_CHAIN (vars);
	}

      expand_end_bindings (getdecls (), 1, 0);
      poplevel (1, 0, 0);
      pop_momentary ();

      finish_function (lineno, 0);
      assemble_constructor (IDENTIFIER_POINTER (fnname));
    }

  /* Done with C language context needs.  */
  pop_lang_context ();

  /* Now write out any static class variables (which may have since
     learned how to be initialized).  */
  while (pending_statics)
    {
      tree decl = TREE_VALUE (pending_statics);
      if (TREE_USED (decl) == 1
	  || TREE_READONLY (decl) == 0
	  || DECL_INITIAL (decl) == 0)
	rest_of_decl_compilation (decl, IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl)), 1, 1);
      pending_statics = TREE_CHAIN (pending_statics);
    }

  this_time = get_run_time ();
  parse_time -= this_time - start_time;
  varconst_time += this_time - start_time;

  /* Now write out inline functions which had their addresses taken
     and which were not declared virtual and which were not declared
     `extern inline'.  */
  while (pending_addressable_inlines)
    {
      tree decl = TREE_VALUE (pending_addressable_inlines);
      if (! TREE_ASM_WRITTEN (decl)
	  && ! DECL_EXTERNAL (decl)
	  && DECL_SAVED_INSNS (decl))
	output_inline_function (decl);
      pending_addressable_inlines = TREE_CHAIN (pending_addressable_inlines);
    }

  start_time = get_run_time ();

  /* Now delete from the chain of variables all virtual function tables.
     We output them all ourselves, because each will be treated specially.  */

#if 1
  /* The reason for pushing garbage onto the global_binding_level is to
     ensure that we can slice out _DECLs which pertain to virtual function
     tables.  If the last thing pushed onto the global_binding_level was a
     virtual function table, then slicing it out would slice away all the
     decls (i.e., we lose the head of the chain).

     There are several ways of getting the same effect, from changing the
     way that iterators over the chain treat the elements that pertain to
     virtual function tables, moving the implementation of this code to
     cp-decl.c (where we can manipulate global_binding_level directly),
     popping the garbage after pushing it and slicing away the vtable
     stuff, or just leaving it alone. */

  /* Make last thing in global scope not be a virtual function table.  */
#if 0 /* not yet, should get fixed properly later */
  vars = make_type_decl (get_identifier (" @%$#@!"), integer_type_node);
#else
  vars = build_decl (TYPE_DECL, get_identifier (" @%$#@!"), integer_type_node);
#endif
  DECL_IGNORED_P (vars) = 1;
  DECL_SOURCE_LINE (vars) = 0;
  pushdecl (vars);
#endif

  walk_vtables (finish_vtable_typedecl, finish_vtable_vardecl);

  if (write_virtuals == 2)
    {
      /* Now complain about an virtual function tables promised
	 but not delivered.  */
      while (pending_vtables)
	{
	  if (TREE_PURPOSE (pending_vtables) == NULL_TREE)
	    error ("virtual function table for `%s' not defined",
		   IDENTIFIER_POINTER (TREE_VALUE (pending_vtables)));
	  pending_vtables = TREE_CHAIN (pending_vtables);
	}
    }

  permanent_allocation ();
  this_time = get_run_time ();
  parse_time -= this_time - start_time;
  varconst_time += this_time - start_time;

  if (flag_detailed_statistics)
    dump_time_statistics ();
}
