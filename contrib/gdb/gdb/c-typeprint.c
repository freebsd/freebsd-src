/* Support for printing C and C++ types for GDB, the GNU debugger.
   Copyright 1986, 1988, 1989, 1991, 1993, 1994, 1995, 1996, 1998, 1999
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "obstack.h"
#include "bfd.h"		/* Binary File Description */
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "value.h"
#include "gdbcore.h"
#include "target.h"
#include "command.h"
#include "gdbcmd.h"
#include "language.h"
#include "demangle.h"
#include "c-lang.h"
#include "typeprint.h"

#include "gdb_string.h"
#include <errno.h>
#include <ctype.h>

/* Flag indicating target was compiled by HP compiler */
extern int hp_som_som_object_present;

static void
c_type_print_args PARAMS ((struct type *, GDB_FILE *));

static void
cp_type_print_derivation_info PARAMS ((GDB_FILE *, struct type *));

void
c_type_print_varspec_prefix PARAMS ((struct type *, GDB_FILE *, int, int));

static void
c_type_print_cv_qualifier PARAMS ((struct type *, GDB_FILE *, int, int));



/* Print a description of a type in the format of a 
   typedef for the current language.
   NEW is the new name for a type TYPE. */

void
c_typedef_print (type, new, stream)
   struct type *type;
   struct symbol *new;
   GDB_FILE *stream;
{
  CHECK_TYPEDEF (type);
   switch (current_language->la_language)
   {
#ifdef _LANG_c
   case language_c:
   case language_cplus:
      fprintf_filtered(stream, "typedef ");
      type_print(type,"",stream,0);
      if(TYPE_NAME ((SYMBOL_TYPE (new))) == 0
	 || !STREQ (TYPE_NAME ((SYMBOL_TYPE (new))), SYMBOL_NAME (new)))
	fprintf_filtered(stream,  " %s", SYMBOL_SOURCE_NAME(new));
      break;
#endif
#ifdef _LANG_m2
   case language_m2:
      fprintf_filtered(stream, "TYPE ");
      if(!TYPE_NAME(SYMBOL_TYPE(new)) ||
	 !STREQ (TYPE_NAME(SYMBOL_TYPE(new)), SYMBOL_NAME(new)))
	fprintf_filtered(stream, "%s = ", SYMBOL_SOURCE_NAME(new));
      else
	 fprintf_filtered(stream, "<builtin> = ");
      type_print(type,"",stream,0);
      break;
#endif
#ifdef _LANG_chill
   case language_chill:
      fprintf_filtered(stream, "SYNMODE ");
      if(!TYPE_NAME(SYMBOL_TYPE(new)) ||
	 !STREQ (TYPE_NAME(SYMBOL_TYPE(new)), SYMBOL_NAME(new)))
	fprintf_filtered(stream, "%s = ", SYMBOL_SOURCE_NAME(new));
      else
	 fprintf_filtered(stream, "<builtin> = ");
      type_print(type,"",stream,0);
      break;
#endif
   default:
      error("Language not supported.");
   }
   fprintf_filtered(stream, ";\n");
}


/* LEVEL is the depth to indent lines by.  */

void
c_print_type (type, varstring, stream, show, level)
     struct type *type;
     char *varstring;
     GDB_FILE *stream;
     int show;
     int level;
{
  register enum type_code code;
  int demangled_args;

  if (show > 0)
    CHECK_TYPEDEF (type);

  c_type_print_base (type, stream, show, level);
  code = TYPE_CODE (type);
  if ((varstring != NULL && *varstring != '\0')
      ||
      /* Need a space if going to print stars or brackets;
	 but not if we will print just a type name.  */
      ((show > 0 || TYPE_NAME (type) == 0)
       &&
       (code == TYPE_CODE_PTR || code == TYPE_CODE_FUNC
	|| code == TYPE_CODE_METHOD
	|| code == TYPE_CODE_ARRAY
	|| code == TYPE_CODE_MEMBER
	|| code == TYPE_CODE_REF)))
    fputs_filtered (" ", stream);
  c_type_print_varspec_prefix (type, stream, show, 0);

  if (varstring != NULL)
    {
      fputs_filtered (varstring, stream);

      /* For demangled function names, we have the arglist as part of the name,
	 so don't print an additional pair of ()'s */

      demangled_args = strchr(varstring, '(') != NULL;
      c_type_print_varspec_suffix (type, stream, show, 0, demangled_args);
    }
}
  
/* If TYPE is a derived type, then print out derivation information.
   Print only the actual base classes of this type, not the base classes
   of the base classes.  I.E.  for the derivation hierarchy:

	class A { int a; };
	class B : public A {int b; };
	class C : public B {int c; };

   Print the type of class C as:

   	class C : public B {
		int c;
	}

   Not as the following (like gdb used to), which is not legal C++ syntax for
   derived types and may be confused with the multiple inheritance form:

	class C : public B : public A {
		int c;
	}

   In general, gdb should try to print the types as closely as possible to
   the form that they appear in the source code. 
   Note that in case of protected derivation gcc will not say 'protected' 
   but 'private'. The HP's aCC compiler emits specific information for 
   derivation via protected inheritance, so gdb can print it out */

static void
cp_type_print_derivation_info (stream, type)
     GDB_FILE *stream;
     struct type *type;
{
  char *name;
  int i;

  for (i = 0; i < TYPE_N_BASECLASSES (type); i++)
    {
      fputs_filtered (i == 0 ? ": " : ", ", stream);
      fprintf_filtered (stream, "%s%s ",
			BASETYPE_VIA_PUBLIC (type, i) ? "public" 
			: (TYPE_FIELD_PROTECTED (type, i) ? "protected" : "private"),
			BASETYPE_VIA_VIRTUAL(type, i) ? " virtual" : "");
      name = type_name_no_tag (TYPE_BASECLASS (type, i));
      fprintf_filtered (stream, "%s", name ? name : "(null)");
    }
  if (i > 0)
    {
      fputs_filtered (" ", stream);
    }
}
/* Print the C++ method arguments ARGS to the file STREAM.  */
 
void
cp_type_print_method_args (args, prefix, varstring, staticp, stream)
     struct type **args;
     char *prefix;
     char *varstring;
     int staticp;
     GDB_FILE *stream;
{
  int i;
 
  fprintf_symbol_filtered (stream, prefix, language_cplus, DMGL_ANSI);
  fprintf_symbol_filtered (stream, varstring, language_cplus, DMGL_ANSI);
  fputs_filtered ("(", stream);
  if (args && args[!staticp] && args[!staticp]->code != TYPE_CODE_VOID)
    {
      i = !staticp;             /* skip the class variable */
      while (1)
        {
          type_print (args[i++], "", stream, 0);
          if (!args[i])
            {
              fprintf_filtered (stream, " ...");
              break;
            }
          else if (args[i]->code != TYPE_CODE_VOID)
            {
              fprintf_filtered (stream, ", ");
            }
          else break;
        }
    }
  else if (current_language->la_language == language_cplus)
    {
      fprintf_filtered (stream, "void");
    }
 
  fprintf_filtered (stream, ")");
}


/* Print any asterisks or open-parentheses needed before the
   variable name (to describe its type).

   On outermost call, pass 0 for PASSED_A_PTR.
   On outermost call, SHOW > 0 means should ignore
   any typename for TYPE and show its details.
   SHOW is always zero on recursive calls.  */

void
c_type_print_varspec_prefix (type, stream, show, passed_a_ptr)
     struct type *type;
     GDB_FILE *stream;
     int show;
     int passed_a_ptr;
{
  char *name;
  if (type == 0)
    return;

  if (TYPE_NAME (type) && show <= 0)
    return;

  QUIT;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_PTR:
      c_type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0, 1);
      fprintf_filtered (stream, "*");
      c_type_print_cv_qualifier (type, stream, 1, 0);
      break;

    case TYPE_CODE_MEMBER:
      if (passed_a_ptr)
	fprintf_filtered (stream, "(");
      c_type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0, 0);
      fprintf_filtered (stream, " ");
      name = type_name_no_tag (TYPE_DOMAIN_TYPE (type));
      if (name)
	fputs_filtered (name, stream);
      else
        c_type_print_base (TYPE_DOMAIN_TYPE (type), stream, 0, passed_a_ptr);
      fprintf_filtered (stream, "::");
      break;

    case TYPE_CODE_METHOD:
      if (passed_a_ptr)
	fprintf_filtered (stream, "(");
      c_type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0, 0);
      if (passed_a_ptr)
	{
	  fprintf_filtered (stream, " ");
	  c_type_print_base (TYPE_DOMAIN_TYPE (type), stream, 0, passed_a_ptr);
	  fprintf_filtered (stream, "::");
	}
      break;

    case TYPE_CODE_REF:
      c_type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0, 1);
      fprintf_filtered (stream, "&");
      c_type_print_cv_qualifier (type, stream, 1, 0);
      break;

    case TYPE_CODE_FUNC:
      c_type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0, 0);
      if (passed_a_ptr)
	fprintf_filtered (stream, "(");
      break;

    case TYPE_CODE_ARRAY:
      c_type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0, 0);
      if (passed_a_ptr)
	fprintf_filtered (stream, "(");
      break;

    case TYPE_CODE_UNDEF:
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_INT:
    case TYPE_CODE_FLT:
    case TYPE_CODE_VOID:
    case TYPE_CODE_ERROR:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_SET:
    case TYPE_CODE_RANGE:
    case TYPE_CODE_STRING:
    case TYPE_CODE_BITSTRING:
    case TYPE_CODE_COMPLEX:
    case TYPE_CODE_TYPEDEF:
      /* These types need no prefix.  They are listed here so that
	 gcc -Wall will reveal any types that haven't been handled.  */
      break;
    }
}

/* Print out "const" and "volatile" attributes.
   TYPE is a pointer to the type being printed out.
   STREAM is the output destination.
   NEED_SPACE = 1 indicates an initial white space is needed */

static void
c_type_print_cv_qualifier (type, stream, need_pre_space, need_post_space)
  struct type *type;
  GDB_FILE *stream;
  int need_pre_space;
  int need_post_space;
{
  int flag = 0;
  
  if (TYPE_CONST (type))
    {
      if (need_pre_space)
        fprintf_filtered (stream, " ");
      fprintf_filtered (stream, "const");
      flag = 1;
    }
  
  if (TYPE_VOLATILE (type))
    {
      if (flag || need_pre_space)
        fprintf_filtered (stream, " ");
      fprintf_filtered (stream, "volatile");
      flag = 1;
    }

  if (flag && need_post_space)
    fprintf_filtered (stream, " ");
}




static void
c_type_print_args (type, stream)
     struct type *type;
     GDB_FILE *stream;
{
  int i;
  struct type **args;

  fprintf_filtered (stream, "(");
  args = TYPE_ARG_TYPES (type);
  if (args != NULL)
    {
      if (args[1] == NULL)
	{
	  fprintf_filtered (stream, "...");
	}
      else if ((args[1]->code == TYPE_CODE_VOID) &&
               (current_language->la_language == language_cplus))
        {
          fprintf_filtered (stream, "void");
        }
      else
	{
	  for (i = 1;
	       args[i] != NULL && args[i]->code != TYPE_CODE_VOID;
	       i++)
	    {
	      c_print_type (args[i], "", stream, -1, 0);
	      if (args[i+1] == NULL)
		{
		  fprintf_filtered (stream, "...");
		}
	      else if (args[i+1]->code != TYPE_CODE_VOID)
		{
		  fprintf_filtered (stream, ",");
		  wrap_here ("    ");
		}
	    }
	}
    }
  else if (current_language->la_language == language_cplus)
    {
      fprintf_filtered (stream, "void");
    }
  
  fprintf_filtered (stream, ")");
}

/* Print any array sizes, function arguments or close parentheses
   needed after the variable name (to describe its type).
   Args work like c_type_print_varspec_prefix.  */

void
c_type_print_varspec_suffix (type, stream, show, passed_a_ptr, demangled_args)
     struct type *type;
     GDB_FILE *stream;
     int show;
     int passed_a_ptr;
     int demangled_args;
{
  if (type == 0)
    return;

  if (TYPE_NAME (type) && show <= 0)
    return;

  QUIT;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
      if (passed_a_ptr)
	fprintf_filtered (stream, ")");
      
      fprintf_filtered (stream, "[");
      if (TYPE_LENGTH (type) >= 0 && TYPE_LENGTH (TYPE_TARGET_TYPE (type)) > 0
	  && TYPE_ARRAY_UPPER_BOUND_TYPE(type) != BOUND_CANNOT_BE_DETERMINED)
	fprintf_filtered (stream, "%d",
			  (TYPE_LENGTH (type)
			   / TYPE_LENGTH (TYPE_TARGET_TYPE (type))));
      fprintf_filtered (stream, "]");
      
      c_type_print_varspec_suffix (TYPE_TARGET_TYPE (type), stream, 0, 0, 0);
      break;

    case TYPE_CODE_MEMBER:
      if (passed_a_ptr)
	fprintf_filtered (stream, ")");
      c_type_print_varspec_suffix (TYPE_TARGET_TYPE (type), stream, 0, 0, 0);
      break;

    case TYPE_CODE_METHOD:
      if (passed_a_ptr)
	fprintf_filtered (stream, ")");
      c_type_print_varspec_suffix (TYPE_TARGET_TYPE (type), stream, 0, 0, 0);
      if (passed_a_ptr)
	{
	  c_type_print_args (type, stream);
	}
      break;

    case TYPE_CODE_PTR:
    case TYPE_CODE_REF:
      c_type_print_varspec_suffix (TYPE_TARGET_TYPE (type), stream, 0, 1, 0);
      break;

    case TYPE_CODE_FUNC:
      if (passed_a_ptr)
	fprintf_filtered (stream, ")");
      if (!demangled_args)
	{ int i, len = TYPE_NFIELDS (type);
	  fprintf_filtered (stream, "(");
          if ((len == 0) && (current_language->la_language == language_cplus))
            {
              fprintf_filtered (stream, "void");
            }
          else
            for (i = 0; i < len; i++)
              {
                if (i > 0)
                  {
                    fputs_filtered (", ", stream);
                    wrap_here ("    ");
                  }
                c_print_type (TYPE_FIELD_TYPE (type, i), "", stream, -1, 0);
              }
	  fprintf_filtered (stream, ")");
	}
      c_type_print_varspec_suffix (TYPE_TARGET_TYPE (type), stream, 0,
				   passed_a_ptr, 0);
      break;

    case TYPE_CODE_UNDEF:
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_INT:
    case TYPE_CODE_FLT:
    case TYPE_CODE_VOID:
    case TYPE_CODE_ERROR:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_SET:
    case TYPE_CODE_RANGE:
    case TYPE_CODE_STRING:
    case TYPE_CODE_BITSTRING:
    case TYPE_CODE_COMPLEX:
    case TYPE_CODE_TYPEDEF:
      /* These types do not need a suffix.  They are listed so that
	 gcc -Wall will report types that may not have been considered.  */
      break;
    }
}

/* Print the name of the type (or the ultimate pointer target,
   function value or array element), or the description of a
   structure or union.

   SHOW positive means print details about the type (e.g. enum values),
   and print structure elements passing SHOW - 1 for show.
   SHOW negative means just print the type name or struct tag if there is one.
   If there is no name, print something sensible but concise like
   "struct {...}".
   SHOW zero means just print the type name or struct tag if there is one.
   If there is no name, print something sensible but not as concise like
   "struct {int x; int y;}".

   LEVEL is the number of spaces to indent by.
   We increase it for some recursive calls.  */

void
c_type_print_base (type, stream, show, level)
     struct type *type;
     GDB_FILE *stream;
     int show;
     int level;
{
  register int i;
  register int len;
  register int lastval;
  char *mangled_name;
  char *demangled_name;
  char *demangled_no_static;
  enum {s_none, s_public, s_private, s_protected} section_type;
  int need_access_label = 0;
  int j, len2;

  QUIT;

  wrap_here ("    ");
  if (type == NULL)
    {
      fputs_filtered ("<type unknown>", stream);
      return;
    }

  /* When SHOW is zero or less, and there is a valid type name, then always
     just print the type name directly from the type.  */
  /* If we have "typedef struct foo {. . .} bar;" do we want to print it
     as "struct foo" or as "bar"?  Pick the latter, because C++ folk tend
     to expect things like "class5 *foo" rather than "struct class5 *foo".  */

  if (show <= 0
      && TYPE_NAME (type) != NULL)
    {
      c_type_print_cv_qualifier (type, stream, 0, 1);
      fputs_filtered (TYPE_NAME (type), stream);
      return;
    }

  CHECK_TYPEDEF (type);
	  
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_TYPEDEF:
    case TYPE_CODE_ARRAY:
    case TYPE_CODE_PTR:
    case TYPE_CODE_MEMBER:
    case TYPE_CODE_REF:
    case TYPE_CODE_FUNC:
    case TYPE_CODE_METHOD:
      c_type_print_base (TYPE_TARGET_TYPE (type), stream, show, level);
      break;

    case TYPE_CODE_STRUCT:
      c_type_print_cv_qualifier (type, stream, 0, 1);
      /* Note TYPE_CODE_STRUCT and TYPE_CODE_CLASS have the same value,
       * so we use another means for distinguishing them.
       */
      if (HAVE_CPLUS_STRUCT (type)) {
        switch (TYPE_DECLARED_TYPE(type)) {
          case DECLARED_TYPE_CLASS:
	    fprintf_filtered (stream, "class ");
            break;
          case DECLARED_TYPE_UNION:
            fprintf_filtered (stream, "union ");
            break;
          case DECLARED_TYPE_STRUCT:
            fprintf_filtered (stream, "struct ");
            break;
          default:
            /* If there is a CPLUS_STRUCT, assume class if not
             * otherwise specified in the declared_type field.
             */
	    fprintf_filtered (stream, "class ");
            break;
        } /* switch */
      } else {
        /* If not CPLUS_STRUCT, then assume it's a C struct */ 
        fprintf_filtered (stream, "struct ");
      }
      goto struct_union;

    case TYPE_CODE_UNION:
      c_type_print_cv_qualifier (type, stream, 0, 1);
      fprintf_filtered (stream, "union ");

    struct_union:

      /* Print the tag if it exists. 
       * The HP aCC compiler emits
       * a spurious "{unnamed struct}"/"{unnamed union}"/"{unnamed enum}"
       * tag  for unnamed struct/union/enum's, which we don't
       * want to print.
       */
      if (TYPE_TAG_NAME (type) != NULL &&
          strncmp(TYPE_TAG_NAME(type), "{unnamed", 8))
	{
	  fputs_filtered (TYPE_TAG_NAME (type), stream);
	  if (show > 0)
	    fputs_filtered (" ", stream);
	}
      wrap_here ("    ");
      if (show < 0)
	{
	  /* If we just printed a tag name, no need to print anything else.  */
	  if (TYPE_TAG_NAME (type) == NULL)
	    fprintf_filtered (stream, "{...}");
	}
      else if (show > 0 || TYPE_TAG_NAME (type) == NULL)
	{
	  cp_type_print_derivation_info (stream, type);
	  
	  fprintf_filtered (stream, "{\n");
	  if ((TYPE_NFIELDS (type) == 0) && (TYPE_NFN_FIELDS (type) == 0))
	    {
	      if (TYPE_FLAGS (type) & TYPE_FLAG_STUB)
		fprintfi_filtered (level + 4, stream, "<incomplete type>\n");
	      else
		fprintfi_filtered (level + 4, stream, "<no data fields>\n");
	    }

	  /* Start off with no specific section type, so we can print
	     one for the first field we find, and use that section type
	     thereafter until we find another type. */

	  section_type = s_none;

          /* For a class, if all members are private, there's no need
             for a "private:" label; similarly, for a struct or union
             masquerading as a class, if all members are public, there's
             no need for a "public:" label. */ 

          if ((TYPE_DECLARED_TYPE (type) == DECLARED_TYPE_CLASS) ||
               (TYPE_DECLARED_TYPE (type) == DECLARED_TYPE_TEMPLATE))
            {
              QUIT;
              len = TYPE_NFIELDS (type);
              for (i = TYPE_N_BASECLASSES (type); i < len; i++)
                if (!TYPE_FIELD_PRIVATE (type, i))
                  {
                    need_access_label = 1;
                    break;
                  }
              QUIT;
              if (!need_access_label)
                {
                  len2 = TYPE_NFN_FIELDS (type);
                  for (j = 0; j < len2; j++)
                    {
                      len = TYPE_FN_FIELDLIST_LENGTH (type, j);
                      for (i = 0; i < len; i++)
                        if (!TYPE_FN_FIELD_PRIVATE (TYPE_FN_FIELDLIST1 (type, j), i))
                          {
                            need_access_label = 1;
                            break;
                          }
                      if (need_access_label)
                        break;
                    }
                }
            }
          else if ((TYPE_DECLARED_TYPE (type) == DECLARED_TYPE_STRUCT) ||
                   (TYPE_DECLARED_TYPE (type) == DECLARED_TYPE_UNION))
            {
              QUIT;
              len = TYPE_NFIELDS (type);
              for (i = TYPE_N_BASECLASSES (type); i < len; i++)
                if (TYPE_FIELD_PRIVATE (type, i) || TYPE_FIELD_PROTECTED (type, i))
                  {
                    need_access_label = 1;
                    break;
                  }
              QUIT;
              if (!need_access_label)
                {
                  len2 = TYPE_NFN_FIELDS (type);
                  for (j = 0; j < len2; j++)
                    {
                      QUIT;
                      len = TYPE_FN_FIELDLIST_LENGTH (type, j);
                      for (i = 0; i < len; i++)
                        if (TYPE_FN_FIELD_PRIVATE (TYPE_FN_FIELDLIST1 (type, j), i) ||
                            TYPE_FN_FIELD_PROTECTED (TYPE_FN_FIELDLIST1 (type, j), i))
                          {
                            need_access_label = 1;
                            break;
                          }
                      if (need_access_label)
                        break;
                    }
                }
            }

	  /* If there is a base class for this type,
	     do not print the field that it occupies.  */

	  len = TYPE_NFIELDS (type);
	  for (i = TYPE_N_BASECLASSES (type); i < len; i++)
	    {
	      QUIT;
	      /* Don't print out virtual function table.  */
              /* HP ANSI C++ case */
              if (TYPE_HAS_VTABLE(type) && (STREQN (TYPE_FIELD_NAME (type, i), "__vfp", 5)))
                 continue;
              /* Other compilers */
              /* pai:: FIXME : check for has_vtable < 0 */
	      if (STREQN (TYPE_FIELD_NAME (type, i), "_vptr", 5)
		  && is_cplus_marker ((TYPE_FIELD_NAME (type, i))[5]))
		continue;

	      /* If this is a C++ class we can print the various C++ section
		 labels. */

	      if (HAVE_CPLUS_STRUCT (type) && need_access_label)
		{
		  if (TYPE_FIELD_PROTECTED (type, i))
		    {
		      if (section_type != s_protected)
			{
			  section_type = s_protected;
			  fprintfi_filtered (level + 2, stream,
					     "protected:\n");
			}
		    }
		  else if (TYPE_FIELD_PRIVATE (type, i))
		    {
		      if (section_type != s_private)
			{
			  section_type = s_private;
			  fprintfi_filtered (level + 2, stream, "private:\n");
			}
		    }
		  else
		    {
		      if (section_type != s_public)
			{
			  section_type = s_public;
			  fprintfi_filtered (level + 2, stream, "public:\n");
			}
		    }
		}

	      print_spaces_filtered (level + 4, stream);
	      if (TYPE_FIELD_STATIC (type, i))
		{
		  fprintf_filtered (stream, "static ");
		}
	      c_print_type (TYPE_FIELD_TYPE (type, i),
			    TYPE_FIELD_NAME (type, i),
			    stream, show - 1, level + 4);
	      if (!TYPE_FIELD_STATIC (type, i)
		  && TYPE_FIELD_PACKED (type, i))
		{
		  /* It is a bitfield.  This code does not attempt
		     to look at the bitpos and reconstruct filler,
		     unnamed fields.  This would lead to misleading
		     results if the compiler does not put out fields
		     for such things (I don't know what it does).  */
		  fprintf_filtered (stream, " : %d",
				    TYPE_FIELD_BITSIZE (type, i));
		}
	      fprintf_filtered (stream, ";\n");
	    }

	  /* If there are both fields and methods, put a space between. */
	  len = TYPE_NFN_FIELDS (type);
	  if (len && section_type != s_none)
	     fprintf_filtered (stream, "\n");

	  /* C++: print out the methods */
	  for (i = 0; i < len; i++)
	    {
	      struct fn_field *f = TYPE_FN_FIELDLIST1 (type, i);
	      int j, len2 = TYPE_FN_FIELDLIST_LENGTH (type, i);
	      char *method_name = TYPE_FN_FIELDLIST_NAME (type, i);
	      char *name = type_name_no_tag (type);
	      int is_constructor = name && STREQ(method_name, name);
	      for (j = 0; j < len2; j++)
		{
		  char *physname = TYPE_FN_FIELD_PHYSNAME (f, j);
		  int is_full_physname_constructor = 
		    ((physname[0] == '_' && physname[1] == '_'
		      && strchr ("0123456789Qt", physname[2]))
		     || STREQN (physname, "__ct__", 6)
		     || DESTRUCTOR_PREFIX_P (physname)
		     || STREQN (physname, "__dt__", 6));

		  QUIT;
		  if (TYPE_FN_FIELD_PROTECTED (f, j))
		    {
		      if (section_type != s_protected)
			{
			  section_type = s_protected;
			  fprintfi_filtered (level + 2, stream,
					     "protected:\n");
			}
		    }
		  else if (TYPE_FN_FIELD_PRIVATE (f, j))
		    {
		      if (section_type != s_private)
			{
			  section_type = s_private;
			  fprintfi_filtered (level + 2, stream, "private:\n");
			}
		    }
		  else
		    {
		      if (section_type != s_public)
			{
			  section_type = s_public;
			  fprintfi_filtered (level + 2, stream, "public:\n");
			}
		    }

		  print_spaces_filtered (level + 4, stream);
		  if (TYPE_FN_FIELD_VIRTUAL_P (f, j))
		    fprintf_filtered (stream, "virtual ");
		  else if (TYPE_FN_FIELD_STATIC_P (f, j))
		    fprintf_filtered (stream, "static ");
		  if (TYPE_TARGET_TYPE (TYPE_FN_FIELD_TYPE (f, j)) == 0)
		    {
		      /* Keep GDB from crashing here.  */
		      fprintf_filtered (stream, "<undefined type> %s;\n",
			       TYPE_FN_FIELD_PHYSNAME (f, j));
		      break;
		    }
		  else if (!is_constructor &&                  /* constructors don't have declared types */
                           !is_full_physname_constructor &&    /*    " "  */ 
                           !strstr (method_name, "operator ")) /* Not a type conversion operator */
                                                               /* (note space -- other operators don't have it) */ 
		    {
		      type_print (TYPE_TARGET_TYPE (TYPE_FN_FIELD_TYPE (f, j)),
				  "", stream, -1);
		      fputs_filtered (" ", stream);
		    }
		  if (TYPE_FN_FIELD_STUB (f, j))
		    /* Build something we can demangle.  */
		    mangled_name = gdb_mangle_name (type, i, j);
		  else
		    mangled_name = TYPE_FN_FIELD_PHYSNAME (f, j);

		  demangled_name =
		    cplus_demangle (mangled_name,
				    DMGL_ANSI | DMGL_PARAMS);
		  if (demangled_name == NULL)
		    {
		      /* in some cases (for instance with the HP demangling),
			 if a function has more than 10 arguments, 
			 the demangling will fail.
			 Let's try to reconstruct the function signature from 
			 the symbol information	*/
		      if (!TYPE_FN_FIELD_STUB (f, j))
			cp_type_print_method_args (TYPE_FN_FIELD_ARGS (f, j), "",
						   method_name,
						   TYPE_FN_FIELD_STATIC_P (f, j),
						   stream);
		      else
			fprintf_filtered (stream, "<badly mangled name '%s'>",
					  mangled_name);
		    }
		  else
		    {
		      char *p;
		      char *demangled_no_class = demangled_name;
		      
		      while (p = strchr (demangled_no_class, ':'))
			{
			  demangled_no_class = p;
			  if (*++demangled_no_class == ':')
			    ++demangled_no_class;
			}
		      /* get rid of the static word appended by the demangler */
		      p = strstr (demangled_no_class, " static");
		      if (p != NULL)
			{
			  int length = p - demangled_no_class;
			  demangled_no_static = (char *) xmalloc (length + 1);
			  strncpy (demangled_no_static, demangled_no_class, length);
                          *(demangled_no_static + length) = '\0';
			  fputs_filtered (demangled_no_static, stream);
			  free (demangled_no_static);
			}
		      else
			fputs_filtered (demangled_no_class, stream);
		      free (demangled_name);
		    }

		  if (TYPE_FN_FIELD_STUB (f, j))
		    free (mangled_name);

		  fprintf_filtered (stream, ";\n");
		}
	    }

      if (TYPE_LOCALTYPE_PTR (type) && show >= 0)
        fprintfi_filtered (level, stream, " (Local at %s:%d)\n",
                           TYPE_LOCALTYPE_FILE (type),
                           TYPE_LOCALTYPE_LINE (type));

	  fprintfi_filtered (level, stream, "}");
	}
      if (TYPE_CODE(type) == TYPE_CODE_TEMPLATE)
        goto go_back;
      break;

    case TYPE_CODE_ENUM:
      c_type_print_cv_qualifier (type, stream, 0, 1);
      /* HP C supports sized enums */
      if (hp_som_som_object_present)
        switch (TYPE_LENGTH (type))
          {
            case 1:
              fputs_filtered ("char ", stream);
              break;
            case 2:
              fputs_filtered ("short ", stream);
              break;
            default:
              break;
          }
       fprintf_filtered (stream, "enum ");
      /* Print the tag name if it exists.
         The aCC compiler emits a spurious 
         "{unnamed struct}"/"{unnamed union}"/"{unnamed enum}"
         tag for unnamed struct/union/enum's, which we don't
         want to print. */
      if (TYPE_TAG_NAME (type) != NULL &&
          strncmp(TYPE_TAG_NAME(type), "{unnamed", 8))
	{
	  fputs_filtered (TYPE_TAG_NAME (type), stream);
	  if (show > 0)
	    fputs_filtered (" ", stream);
	}

      wrap_here ("    ");
      if (show < 0)
	{
	  /* If we just printed a tag name, no need to print anything else.  */
	  if (TYPE_TAG_NAME (type) == NULL)
	    fprintf_filtered (stream, "{...}");
	}
      else if (show > 0 || TYPE_TAG_NAME (type) == NULL)
	{
	  fprintf_filtered (stream, "{");
	  len = TYPE_NFIELDS (type);
	  lastval = 0;
	  for (i = 0; i < len; i++)
	    {
	      QUIT;
	      if (i) fprintf_filtered (stream, ", ");
	      wrap_here ("    ");
	      fputs_filtered (TYPE_FIELD_NAME (type, i), stream);
	      if (lastval != TYPE_FIELD_BITPOS (type, i))
		{
		  fprintf_filtered (stream, " = %d", TYPE_FIELD_BITPOS (type, i));
		  lastval = TYPE_FIELD_BITPOS (type, i);
		}
	      lastval++;
	    }
	  fprintf_filtered (stream, "}");
	}
      break;

    case TYPE_CODE_VOID:
      fprintf_filtered (stream, "void");
      break;

    case TYPE_CODE_UNDEF:
      fprintf_filtered (stream, "struct <unknown>");
      break;

    case TYPE_CODE_ERROR:
      fprintf_filtered (stream, "<unknown type>");
      break;

    case TYPE_CODE_RANGE:
      /* This should not occur */
      fprintf_filtered (stream, "<range type>");
      break;

    case TYPE_CODE_TEMPLATE:
      /* Called on "ptype t" where "t" is a template.
         Prints the template header (with args), e.g.:
           template <class T1, class T2> class "
         and then merges with the struct/union/class code to
         print the rest of the definition. */
      c_type_print_cv_qualifier (type, stream, 0, 1);
      fprintf_filtered (stream, "template <");
      for (i = 0; i < TYPE_NTEMPLATE_ARGS(type); i++) {
        struct template_arg templ_arg;
        templ_arg = TYPE_TEMPLATE_ARG(type, i);
        fprintf_filtered (stream, "class %s", templ_arg.name);
        if (i < TYPE_NTEMPLATE_ARGS(type)-1)
          fprintf_filtered (stream, ", ");
      }
      fprintf_filtered (stream, "> class ");
      /* Yuck, factor this out to a subroutine so we can call
         it and return to the point marked with the "goback:" label... - RT */
      goto struct_union; 
go_back:
      if (TYPE_NINSTANTIATIONS(type) > 0) {
        fprintf_filtered (stream, "\ntemplate instantiations:\n");
        for (i = 0; i < TYPE_NINSTANTIATIONS(type); i++) {
          fprintf_filtered(stream, "  ");
          c_type_print_base (TYPE_INSTANTIATION(type, i), stream, 0, level);
          if (i < TYPE_NINSTANTIATIONS(type)-1) fprintf_filtered(stream, "\n");
        }
      }
      break;
       
    default:
      /* Handle types not explicitly handled by the other cases,
	 such as fundamental types.  For these, just print whatever
	 the type name is, as recorded in the type itself.  If there
	 is no type name, then complain. */
      if (TYPE_NAME (type) != NULL)
	{
          c_type_print_cv_qualifier (type, stream, 0, 1);
	  fputs_filtered (TYPE_NAME (type), stream);
	}
      else
	{
	  /* At least for dump_symtab, it is important that this not be
	     an error ().  */
	  fprintf_filtered (stream, "<invalid type code %d>",
			    TYPE_CODE (type));
	}
      break;
    }
}









