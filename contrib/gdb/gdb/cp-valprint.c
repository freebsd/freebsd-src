/* Support for printing C++ values for GDB, the GNU debugger.
   Copyright 1986, 1988, 1989, 1991, 1994, 1995, 1996
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
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "value.h"
#include "command.h"
#include "gdbcmd.h"
#include "demangle.h"
#include "annotate.h"
#include "gdb_string.h"
#include "c-lang.h"

int vtblprint;			/* Controls printing of vtbl's */
int objectprint;		/* Controls looking up an object's derived type
				   using what we find in its vtables.  */
static int static_field_print;	/* Controls printing of static fields. */

static struct obstack dont_print_vb_obstack;
static struct obstack dont_print_statmem_obstack;

static void
cp_print_static_field PARAMS ((struct type *, value_ptr, GDB_FILE *, int, int,
			       enum val_prettyprint));

static void
cp_print_value PARAMS ((struct type *, char *, CORE_ADDR, GDB_FILE *,
			int, int, enum val_prettyprint, struct type **));

void
cp_print_class_method (valaddr, type, stream)
     char *valaddr;
     struct type *type;
     GDB_FILE *stream;
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
  struct type *target_type = check_typedef (TYPE_TARGET_TYPE (type));

  domain = TYPE_DOMAIN_TYPE (target_type);
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
      fprintf_unfiltered (stream, kind);
      if (TYPE_FN_FIELD_PHYSNAME (f, j)[0] == '_'
	  && is_cplus_marker (TYPE_FN_FIELD_PHYSNAME (f, j)[1]))
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

/* This was what it was for gcc 2.4.5 and earlier.  */
static const char vtbl_ptr_name_old[] =
  { CPLUS_MARKER,'v','t','b','l','_','p','t','r','_','t','y','p','e', 0 };
/* It was changed to this after 2.4.5.  */
const char vtbl_ptr_name[] =
  { '_','_','v','t','b','l','_','p','t','r','_','t','y','p','e', 0 };

/* Return truth value for assertion that TYPE is of the type
   "pointer to virtual function".  */

int
cp_is_vtbl_ptr_type(type)
     struct type *type;
{
  char *typename = type_name_no_tag (type);

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
    {
      type = TYPE_TARGET_TYPE (type);
      if (TYPE_CODE (type) == TYPE_CODE_ARRAY)
	{
	  type = TYPE_TARGET_TYPE (type);
	  if (TYPE_CODE (type) == TYPE_CODE_STRUCT /* if not using thunks */
	      || TYPE_CODE (type) == TYPE_CODE_PTR) /* if using thunks */
	    {
	      /* Virtual functions tables are full of pointers
		 to virtual functions. */
	      return cp_is_vtbl_ptr_type (type);
	    }
	}
    }
  return 0;
}

/* Mutually recursive subroutines of cp_print_value and c_val_print to
   print out a structure's fields: cp_print_value_fields and cp_print_value.
  
   TYPE, VALADDR, ADDRESS, STREAM, RECURSE, and PRETTY have the
   same meanings as in cp_print_value and c_val_print.

   DONT_PRINT is an array of baseclass types that we
   should not print, or zero if called from top level.  */

void
cp_print_value_fields (type, valaddr, address, stream, format, recurse, pretty,
		       dont_print_vb, dont_print_statmem)
     struct type *type;
     char *valaddr;
     CORE_ADDR address;
     GDB_FILE *stream;
     int format;
     int recurse;
     enum val_prettyprint pretty;
     struct type **dont_print_vb;
     int dont_print_statmem;
{
  int i, len, n_baseclasses;
  struct obstack tmp_obstack;
  char *last_dont_print = obstack_next_free (&dont_print_statmem_obstack);

  CHECK_TYPEDEF (type);

  fprintf_filtered (stream, "{");
  len = TYPE_NFIELDS (type);
  n_baseclasses = TYPE_N_BASECLASSES (type);

  /* Print out baseclasses such that we don't print
     duplicates of virtual baseclasses.  */
  if (n_baseclasses > 0)
    cp_print_value (type, valaddr, address, stream,
		    format, recurse+1, pretty, dont_print_vb);

  if (!len && n_baseclasses == 1)
    fprintf_filtered (stream, "<No data fields>");
  else
    {
      extern int inspect_it;
      int fields_seen = 0;

      if (dont_print_statmem == 0)
	{
	  /* If we're at top level, carve out a completely fresh
	     chunk of the obstack and use that until this particular
	     invocation returns.  */
	  tmp_obstack = dont_print_statmem_obstack;
	  obstack_finish (&dont_print_statmem_obstack);
	}

      for (i = n_baseclasses; i < len; i++)
	{
	  /* If requested, skip printing of static fields.  */
	  if (!static_field_print && TYPE_FIELD_STATIC (type, i))
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
	      if (TYPE_FIELD_STATIC (type, i))
		fputs_filtered ("static ", stream);
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
	      annotate_field_begin (TYPE_FIELD_TYPE (type, i));

	      if (TYPE_FIELD_STATIC (type, i))
		fputs_filtered ("static ", stream);
	      fprintf_symbol_filtered (stream, TYPE_FIELD_NAME (type, i),
				       language_cplus,
				       DMGL_PARAMS | DMGL_ANSI);
	      annotate_field_name_end ();
	      fputs_filtered (" = ", stream);
	      annotate_field_value ();
	    }

	  if (!TYPE_FIELD_STATIC (type, i) && TYPE_FIELD_PACKED (type, i))
	    {
	      value_ptr v;

	      /* Bitfields require special handling, especially due to byte
		 order problems.  */
	      if (TYPE_FIELD_IGNORE (type, i))
		{
		   fputs_filtered ("<optimized out or zero length>", stream);
		}
	      else
		{
	           v = value_from_longest (TYPE_FIELD_TYPE (type, i),
				   unpack_field_as_long (type, valaddr, i));

                   val_print (TYPE_FIELD_TYPE(type, i), VALUE_CONTENTS (v), 0,
			      stream, format, 0, recurse + 1, pretty);
		}
	    }
	  else
	    {
	      if (TYPE_FIELD_IGNORE (type, i))
		{
		   fputs_filtered ("<optimized out or zero length>", stream);
		}
	      else if (TYPE_FIELD_STATIC (type, i))
		{
		  value_ptr v;
		  char *phys_name = TYPE_FIELD_STATIC_PHYSNAME (type, i);
		  struct symbol *sym =
		      lookup_symbol (phys_name, 0, VAR_NAMESPACE, 0, NULL);
		  if (sym == NULL)
		    fputs_filtered ("<optimized out>", stream);
		  else
		    {
		      v = value_at (TYPE_FIELD_TYPE (type, i),
				    (CORE_ADDR)SYMBOL_BLOCK_VALUE (sym));
		      cp_print_static_field (TYPE_FIELD_TYPE (type, i), v,
					     stream, format, recurse + 1,
					     pretty);
		    }
		}
	      else
		{
	           val_print (TYPE_FIELD_TYPE (type, i), 
			      valaddr + TYPE_FIELD_BITPOS (type, i) / 8,
			      0, stream, format, 0, recurse + 1, pretty);
		}
	    }
	  annotate_field_end ();
	}

      if (dont_print_statmem == 0)
	{
	  /* Free the space used to deal with the printing
	     of the members from top level.  */
	  obstack_free (&dont_print_statmem_obstack, last_dont_print);
	  dont_print_statmem_obstack = tmp_obstack;
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
cp_print_value (type, valaddr, address, stream, format, recurse, pretty,
		dont_print_vb)
     struct type *type;
     char *valaddr;
     CORE_ADDR address;
     GDB_FILE *stream;
     int format;
     int recurse;
     enum val_prettyprint pretty;
     struct type **dont_print_vb;
{
  struct obstack tmp_obstack;
  struct type **last_dont_print
    = (struct type **)obstack_next_free (&dont_print_vb_obstack);
  int i, n_baseclasses = TYPE_N_BASECLASSES (type);

  if (dont_print_vb == 0)
    {
      /* If we're at top level, carve out a completely fresh
	 chunk of the obstack and use that until this particular
	 invocation returns.  */
      tmp_obstack = dont_print_vb_obstack;
      /* Bump up the high-water mark.  Now alpha is omega.  */
      obstack_finish (&dont_print_vb_obstack);
    }

  for (i = 0; i < n_baseclasses; i++)
    {
      int boffset;
      struct type *baseclass = check_typedef (TYPE_BASECLASS (type, i));
      char *basename = TYPE_NAME (baseclass);

      if (BASETYPE_VIA_VIRTUAL (type, i))
	{
	  struct type **first_dont_print
	    = (struct type **)obstack_base (&dont_print_vb_obstack);

	  int j = (struct type **)obstack_next_free (&dont_print_vb_obstack)
	    - first_dont_print;

	  while (--j >= 0)
	    if (baseclass == first_dont_print[j])
	      goto flush_it;

	  obstack_ptr_grow (&dont_print_vb_obstack, baseclass);
	}

      boffset = baseclass_offset (type, i , valaddr, address);

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
      if (boffset == -1)
	fprintf_filtered (stream, "<invalid address>");
      else
	cp_print_value_fields (baseclass, valaddr + boffset, address + boffset,
			       stream, format, recurse, pretty,
			       (struct type **) obstack_base (&dont_print_vb_obstack),
			       0);
      fputs_filtered (", ", stream);

    flush_it:
      ;
    }

  if (dont_print_vb == 0)
    {
      /* Free the space used to deal with the printing
	 of this type from top level.  */
      obstack_free (&dont_print_vb_obstack, last_dont_print);
      /* Reset watermark so that we can continue protecting
	 ourselves from whatever we were protecting ourselves.  */
      dont_print_vb_obstack = tmp_obstack;
    }
}

/* Print value of a static member.
   To avoid infinite recursion when printing a class that contains
   a static instance of the class, we keep the addresses of all printed
   static member classes in an obstack and refuse to print them more
   than once.

   VAL contains the value to print, TYPE, STREAM, RECURSE, and PRETTY
   have the same meanings as in c_val_print.  */

static void
cp_print_static_field (type, val, stream, format, recurse, pretty)
     struct type *type;
     value_ptr val;
     GDB_FILE *stream;
     int format;
     int recurse;
     enum val_prettyprint pretty;
{
  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
    {
      CORE_ADDR *first_dont_print;
      int i;

      first_dont_print
	= (CORE_ADDR *)obstack_base (&dont_print_statmem_obstack);
      i = (CORE_ADDR *)obstack_next_free (&dont_print_statmem_obstack)
	- first_dont_print;

      while (--i >= 0)
	{
	  if (VALUE_ADDRESS (val) == first_dont_print[i])
	    {
	      fputs_filtered ("<same as static member of an already seen type>",
			      stream);
	      return;
	    }
	}

      obstack_grow (&dont_print_statmem_obstack, (char *) &VALUE_ADDRESS (val),
		    sizeof (CORE_ADDR));

      CHECK_TYPEDEF (type);
      cp_print_value_fields (type, VALUE_CONTENTS (val), VALUE_ADDRESS (val),
			     stream, format, recurse, pretty, NULL, 1);
      return;
    }
  val_print (type, VALUE_CONTENTS (val), VALUE_ADDRESS (val),
	     stream, format, 0, recurse, pretty);
}

void
cp_print_class_member (valaddr, domain, stream, prefix)
     char *valaddr;
     struct type *domain;
     GDB_FILE *stream;
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
    (add_set_cmd ("static-members", class_support, var_boolean,
		  (char *)&static_field_print,
		  "Set printing of C++ static members.",
		  &setprintlist),
     &showprintlist);
  /* Turn on printing of static fields.  */
  static_field_print = 1;

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
  obstack_begin (&dont_print_vb_obstack, 32 * sizeof (struct type *));
  obstack_specify_allocation (&dont_print_statmem_obstack,
			      32 * sizeof (CORE_ADDR), sizeof (CORE_ADDR),
			      xmalloc, free);
}
