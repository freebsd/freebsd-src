/* Support for printing C++ values for GDB, the GNU debugger.
   Copyright 1986, 1988, 1989, 1991 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "obstack.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "value.h"
#include "command.h"
#include "gdbcmd.h"
#include "demangle.h"

int vtblprint;			/* Controls printing of vtbl's */
int objectprint;		/* Controls looking up an object's derived type
				   using what we find in its vtables.  */
struct obstack dont_print_obstack;

static void
cplus_print_value PARAMS ((struct type *, char *, FILE *, int, int,
			   enum val_prettyprint, struct type **));

/* BEGIN-FIXME:  Hooks into typeprint.c, find a better home for prototypes. */

extern void
c_type_print_base PARAMS ((struct type *, FILE *, int, int));

extern void
c_type_print_varspec_prefix PARAMS ((struct type *, FILE *, int, int));

extern void
cp_type_print_method_args PARAMS ((struct type **, char *, char *, int,
				   FILE *));

extern struct obstack dont_print_obstack;

/* END-FIXME */


/* BEGIN-FIXME:  Hooks into c-valprint.c */

extern int
c_val_print PARAMS ((struct type *, char *, CORE_ADDR, FILE *, int, int, int,
		     enum val_prettyprint));
/* END-FIXME */


void
cp_print_class_method (valaddr, type, stream)
     char *valaddr;
     struct type *type;
     FILE *stream;
{
  struct type *domain;
  struct fn_field *f = NULL;
  int j = 0;
  int len2;
  int offset;
  char *kind = "";
  CORE_ADDR addr;
  struct symbol *sym;
  unsigned len;
  unsigned int i;

  check_stub_type (TYPE_TARGET_TYPE (type));
  domain = TYPE_DOMAIN_TYPE (TYPE_TARGET_TYPE (type));
  if (domain == (struct type *)NULL)
    {
      fprintf_filtered (stream, "<unknown>");
      return;
    }
  addr = unpack_pointer (lookup_pointer_type (builtin_type_void), valaddr);
  if (METHOD_PTR_IS_VIRTUAL (addr))
    {
      offset = METHOD_PTR_TO_VOFFSET (addr);
      len = TYPE_NFN_FIELDS (domain);
      for (i = 0; i < len; i++)
	{
	  f = TYPE_FN_FIELDLIST1 (domain, i);
	  len2 = TYPE_FN_FIELDLIST_LENGTH (domain, i);
	  
	  for (j = 0; j < len2; j++)
	    {
	      QUIT;
	      if (TYPE_FN_FIELD_VOFFSET (f, j) == offset)
		{
		  kind = "virtual ";
		  goto common;
		}
	    }
	}
    }
  else
    {
      sym = find_pc_function (addr);
      if (sym == 0)
	{
	  error ("invalid pointer to member function");
	}
      len = TYPE_NFN_FIELDS (domain);
      for (i = 0; i < len; i++)
	{
	  f = TYPE_FN_FIELDLIST1 (domain, i);
	  len2 = TYPE_FN_FIELDLIST_LENGTH (domain, i);
	  
	  for (j = 0; j < len2; j++)
	    {
	      QUIT;
	      if (TYPE_FN_FIELD_STUB (f, j))
		check_stub_method (domain, i, j);
	      if (STREQ (SYMBOL_NAME (sym), TYPE_FN_FIELD_PHYSNAME (f, j)))
		{
		  goto common;
		}
	    }
	}
    }
  common:
  if (i < len)
    {
      fprintf_filtered (stream, "&");
      c_type_print_varspec_prefix (TYPE_FN_FIELD_TYPE (f, j), stream, 0, 0);
      fprintf (stream, kind);
      if (TYPE_FN_FIELD_PHYSNAME (f, j)[0] == '_'
	  && TYPE_FN_FIELD_PHYSNAME (f, j)[1] == CPLUS_MARKER)
	{
	  cp_type_print_method_args (TYPE_FN_FIELD_ARGS (f, j) + 1, "~",
				     TYPE_FN_FIELDLIST_NAME (domain, i),
				     0, stream);
	}
      else
	{
	  cp_type_print_method_args (TYPE_FN_FIELD_ARGS (f, j), "",
				     TYPE_FN_FIELDLIST_NAME (domain, i),
				     0, stream);
	}
    }
  else
    {
      fprintf_filtered (stream, "(");
      type_print (type, "", stream, -1);
      fprintf_filtered (stream, ") %d", (int) addr >> 3);
    }
}

/* Return truth value for assertion that TYPE is of the type
   "pointer to virtual function".  */

int
cp_is_vtbl_ptr_type(type)
     struct type *type;
{
  char *typename = type_name_no_tag (type);
  /* This was what it was for gcc 2.4.5 and earlier.  */
  static const char vtbl_ptr_name_old[] =
    { CPLUS_MARKER,'v','t','b','l','_','p','t','r','_','t','y','p','e', 0 };
  /* It was changed to this after 2.4.5.  */
  static const char vtbl_ptr_name[] =
    { '_','_','v','t','b','l','_','p','t','r','_','t','y','p','e', 0 };

  return (typename != NULL
	  && (STREQ (typename, vtbl_ptr_name)
	      || STREQ (typename, vtbl_ptr_name_old)));
}

/* Return truth value for the assertion that TYPE is of the type
   "pointer to virtual function table".  */

int
cp_is_vtbl_member(type)
     struct type *type;
{
  if (TYPE_CODE (type) == TYPE_CODE_PTR)
    type = TYPE_TARGET_TYPE (type);
  else
    return 0;

  if (TYPE_CODE (type) == TYPE_CODE_ARRAY
      && TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_STRUCT)
    /* Virtual functions tables are full of pointers to virtual functions.  */
    return cp_is_vtbl_ptr_type (TYPE_TARGET_TYPE (type));
  return 0;
}

/* Mutually recursive subroutines of cplus_print_value and c_val_print to
   print out a structure's fields: cp_print_value_fields and cplus_print_value.

   TYPE, VALADDR, STREAM, RECURSE, and PRETTY have the
   same meanings as in cplus_print_value and c_val_print.

   DONT_PRINT is an array of baseclass types that we
   should not print, or zero if called from top level.  */

void
cp_print_value_fields (type, valaddr, stream, format, recurse, pretty,
		       dont_print)
     struct type *type;
     char *valaddr;
     FILE *stream;
     int format;
     int recurse;
     enum val_prettyprint pretty;
     struct type **dont_print;
{
  int i, len, n_baseclasses;

  check_stub_type (type);

  fprintf_filtered (stream, "{");
  len = TYPE_NFIELDS (type);
  n_baseclasses = TYPE_N_BASECLASSES (type);

  /* Print out baseclasses such that we don't print
     duplicates of virtual baseclasses.  */
  if (n_baseclasses > 0)
    cplus_print_value (type, valaddr, stream, format, recurse+1, pretty,
		       dont_print);

  if (!len && n_baseclasses == 1)
    fprintf_filtered (stream, "<No data fields>");
  else
    {
      extern int inspect_it;
      int fields_seen = 0;

      for (i = n_baseclasses; i < len; i++)
	{
	  /* Check if static field */
	  if (TYPE_FIELD_STATIC (type, i))
	    continue;
	  if (fields_seen)
	    fprintf_filtered (stream, ", ");
	  else if (n_baseclasses > 0)
	    {
	      if (pretty)
		{
		  fprintf_filtered (stream, "\n");
		  print_spaces_filtered (2 + 2 * recurse, stream);
		  fputs_filtered ("members of ", stream);
		  fputs_filtered (type_name_no_tag (type), stream);
		  fputs_filtered (": ", stream);
		}
	    }
	  fields_seen = 1;

	  if (pretty)
	    {
	      fprintf_filtered (stream, "\n");
	      print_spaces_filtered (2 + 2 * recurse, stream);
	    }
	  else 
	    {
	      wrap_here (n_spaces (2 + 2 * recurse));
	    }
	  if (inspect_it)
	    {
	      if (TYPE_CODE (TYPE_FIELD_TYPE (type, i)) == TYPE_CODE_PTR)
		fputs_filtered ("\"( ptr \"", stream);
	      else
		fputs_filtered ("\"( nodef \"", stream);
	      fprintf_symbol_filtered (stream, TYPE_FIELD_NAME (type, i),
				       language_cplus,
				       DMGL_PARAMS | DMGL_ANSI);
	      fputs_filtered ("\" \"", stream);
	      fprintf_symbol_filtered (stream, TYPE_FIELD_NAME (type, i),
				       language_cplus,
				       DMGL_PARAMS | DMGL_ANSI);
	      fputs_filtered ("\") \"", stream);
	    }
	  else
	    {
	      fprintf_symbol_filtered (stream, TYPE_FIELD_NAME (type, i),
				       language_cplus,
				       DMGL_PARAMS | DMGL_ANSI);
	      fputs_filtered (" = ", stream);
	    }
	  if (TYPE_FIELD_PACKED (type, i))
	    {
	      value v;

	      /* Bitfields require special handling, especially due to byte
		 order problems.  */
	      v = value_from_longest (TYPE_FIELD_TYPE (type, i),
				   unpack_field_as_long (type, valaddr, i));

	      c_val_print (TYPE_FIELD_TYPE (type, i), VALUE_CONTENTS (v), 0,
			   stream, format, 0, recurse + 1, pretty);
	    }
	  else
	    {
	      c_val_print (TYPE_FIELD_TYPE (type, i), 
			   valaddr + TYPE_FIELD_BITPOS (type, i) / 8,
			   0, stream, format, 0, recurse + 1, pretty);
	    }
	}
      if (pretty)
	{
	  fprintf_filtered (stream, "\n");
	  print_spaces_filtered (2 * recurse, stream);
	}
    }
  fprintf_filtered (stream, "}");
}

/* Special val_print routine to avoid printing multiple copies of virtual
   baseclasses.  */

static void
cplus_print_value (type, valaddr, stream, format, recurse, pretty, dont_print)
     struct type *type;
     char *valaddr;
     FILE *stream;
     int format;
     int recurse;
     enum val_prettyprint pretty;
     struct type **dont_print;
{
  struct obstack tmp_obstack;
  struct type **last_dont_print
    = (struct type **)obstack_next_free (&dont_print_obstack);
  int i, n_baseclasses = TYPE_N_BASECLASSES (type);

  if (dont_print == 0)
    {
      /* If we're at top level, carve out a completely fresh
	 chunk of the obstack and use that until this particular
	 invocation returns.  */
      tmp_obstack = dont_print_obstack;
      /* Bump up the high-water mark.  Now alpha is omega.  */
      obstack_finish (&dont_print_obstack);
    }

  for (i = 0; i < n_baseclasses; i++)
    {
      char *baddr;
      int err;
      char *basename = TYPE_NAME (TYPE_BASECLASS (type, i));

      if (BASETYPE_VIA_VIRTUAL (type, i))
	{
	  struct type **first_dont_print
	    = (struct type **)obstack_base (&dont_print_obstack);

	  int j = (struct type **)obstack_next_free (&dont_print_obstack)
	    - first_dont_print;

	  while (--j >= 0)
	    if (TYPE_BASECLASS (type, i) == first_dont_print[j])
	      goto flush_it;

	  obstack_ptr_grow (&dont_print_obstack, TYPE_BASECLASS (type, i));
	}

      /* Fix to use baseclass_offset instead. FIXME */
      baddr = baseclass_addr (type, i, valaddr, 0, &err);
      if (err == 0 && baddr == 0)
	error ("could not find virtual baseclass %s\n",
	       basename ? basename : "");

      if (pretty)
	{
	  fprintf_filtered (stream, "\n");
	  print_spaces_filtered (2 * recurse, stream);
	}
      fputs_filtered ("<", stream);
      /* Not sure what the best notation is in the case where there is no
	 baseclass name.  */
      fputs_filtered (basename ? basename : "", stream);
      fputs_filtered ("> = ", stream);
      if (err != 0)
	fprintf_filtered (stream,
			  "<invalid address 0x%lx>", (unsigned long) baddr);
      else
	cp_print_value_fields (TYPE_BASECLASS (type, i), baddr, stream, format,
			       recurse, pretty,
			       (struct type **) obstack_base (&dont_print_obstack));
      fputs_filtered (", ", stream);

    flush_it:
      ;
    }

  if (dont_print == 0)
    {
      /* Free the space used to deal with the printing
	 of this type from top level.  */
      obstack_free (&dont_print_obstack, last_dont_print);
      /* Reset watermark so that we can continue protecting
	 ourselves from whatever we were protecting ourselves.  */
      dont_print_obstack = tmp_obstack;
    }
}

void
cp_print_class_member (valaddr, domain, stream, prefix)
     char *valaddr;
     struct type *domain;
     FILE *stream;
     char *prefix;
{
  
  /* VAL is a byte offset into the structure type DOMAIN.
     Find the name of the field for that offset and
     print it.  */
  int extra = 0;
  int bits = 0;
  register unsigned int i;
  unsigned len = TYPE_NFIELDS (domain);
  /* @@ Make VAL into bit offset */
  LONGEST val = unpack_long (builtin_type_int, valaddr) << 3;
  for (i = TYPE_N_BASECLASSES (domain); i < len; i++)
    {
      int bitpos = TYPE_FIELD_BITPOS (domain, i);
      QUIT;
      if (val == bitpos)
	break;
      if (val < bitpos && i != 0)
	{
	  /* Somehow pointing into a field.  */
	  i -= 1;
	  extra = (val - TYPE_FIELD_BITPOS (domain, i));
	  if (extra & 0x7)
	    bits = 1;
	  else
	    extra >>= 3;
	  break;
	}
    }
  if (i < len)
    {
      char *name;
      fprintf_filtered (stream, prefix);
      name = type_name_no_tag (domain);
      if (name)
        fputs_filtered (name, stream);
      else
	c_type_print_base (domain, stream, 0, 0);
      fprintf_filtered (stream, "::");
      fputs_filtered (TYPE_FIELD_NAME (domain, i), stream);
      if (extra)
	fprintf_filtered (stream, " + %d bytes", extra);
      if (bits)
	fprintf_filtered (stream, " (offset in bits)");
    }
  else
    fprintf_filtered (stream, "%d", val >> 3);
}

void
_initialize_cp_valprint ()
{
  add_show_from_set
    (add_set_cmd ("vtbl", class_support, var_boolean, (char *)&vtblprint,
		  "Set printing of C++ virtual function tables.",
		  &setprintlist),
     &showprintlist);

  add_show_from_set
    (add_set_cmd ("object", class_support, var_boolean, (char *)&objectprint,
	  "Set printing of object's derived type based on vtable info.",
		  &setprintlist),
     &showprintlist);

  /* Give people the defaults which they are used to.  */
  objectprint = 0;
  vtblprint = 0;
  obstack_begin (&dont_print_obstack, 32 * sizeof (struct type *));
}
