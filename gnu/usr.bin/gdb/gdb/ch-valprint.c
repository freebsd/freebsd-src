/* Support for printing Chill values for GDB, the GNU debugger.
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
#include "valprint.h"
#include "expression.h"
#include "value.h"
#include "language.h"
#include "demangle.h"

static void
chill_print_value_fields PARAMS ((struct type *, char *, FILE *, int, int,
				  enum val_prettyprint, struct type **));


/* Print data of type TYPE located at VALADDR (within GDB), which came from
   the inferior at address ADDRESS, onto stdio stream STREAM according to
   FORMAT (a letter or 0 for natural format).  The data at VALADDR is in
   target byte order.

   If the data are a string pointer, returns the number of string characters
   printed.

   If DEREF_REF is nonzero, then dereference references, otherwise just print
   them like pointers.

   The PRETTY parameter controls prettyprinting.  */

int
chill_val_print (type, valaddr, address, stream, format, deref_ref, recurse,
		 pretty)
     struct type *type;
     char *valaddr;
     CORE_ADDR address;
     FILE *stream;
     int format;
     int deref_ref;
     int recurse;
     enum val_prettyprint pretty;
{
  LONGEST val;
  unsigned int i = 0;		/* Number of characters printed.  */
  struct type *elttype;
  CORE_ADDR addr;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
      if (TYPE_LENGTH (type) > 0 && TYPE_LENGTH (TYPE_TARGET_TYPE (type)) > 0)
	{
	  if (prettyprint_arrays)
	    {
	      print_spaces_filtered (2 + 2 * recurse, stream);
	    }
	  fprintf_filtered (stream, "[");
	  val_print_array_elements (type, valaddr, address, stream, format,
				    deref_ref, recurse, pretty, 0);
	  fprintf_filtered (stream, "]");
	}
      else
	{
	  error ("unimplemented in chill_val_print; unspecified array length");
	}
      break;

    case TYPE_CODE_INT:
      format = format ? format : output_format;
      if (format)
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	}
      else
	{
	  val_print_type_code_int (type, valaddr, stream);
	}
      break;

    case TYPE_CODE_CHAR:
      format = format ? format : output_format;
      if (format)
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	}
      else
	{
	  LA_PRINT_CHAR ((unsigned char) unpack_long (type, valaddr),
			 stream);
	}
      break;

    case TYPE_CODE_FLT:
      if (format)
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	}
      else
	{
	  print_floating (valaddr, type, stream);
	}
      break;

    case TYPE_CODE_BOOL:
      format = format ? format : output_format;
      if (format)
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	}
      else
	{
	  val = unpack_long (builtin_type_chill_bool, valaddr);
	  fprintf_filtered (stream, val ? "TRUE" : "FALSE");
	}
      break;

    case TYPE_CODE_UNDEF:
      /* This happens (without TYPE_FLAG_STUB set) on systems which don't use
	 dbx xrefs (NO_DBX_XREFS in gcc) if a file has a "struct foo *bar"
	 and no complete type for struct foo in that file.  */
      fprintf_filtered (stream, "<incomplete type>");
      break;

    case TYPE_CODE_PTR:
      if (format && format != 's')
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	  break;
	}
      addr = unpack_pointer (type, valaddr);
      elttype = TYPE_TARGET_TYPE (type);
      
      if (TYPE_CODE (elttype) == TYPE_CODE_FUNC)
	{
	  /* Try to print what function it points to.  */
	  print_address_demangle (addr, stream, demangle);
	  /* Return value is irrelevant except for string pointers.  */
	  return (0);
	}
      if (addressprint && format != 's')
	{
	  fprintf_filtered (stream, "H'%lx", (unsigned long) addr);
	}
      
      /* For a pointer to char or unsigned char, also print the string
	 pointed to, unless pointer is null.  */
      if (TYPE_LENGTH (elttype) == 1
	  && TYPE_CODE (elttype) == TYPE_CODE_CHAR
	  && (format == 0 || format == 's')
	  && addr != 0
	  && /* If print_max is UINT_MAX, the alloca below will fail.
		In that case don't try to print the string.  */
	  print_max < UINT_MAX)
	  {
	    i = val_print_string (addr, 0, stream);
	  }
      /* Return number of characters printed, plus one for the
	 terminating null if we have "reached the end".  */
      return (i + (print_max && i != print_max));
      break;

    case TYPE_CODE_STRING:
      if (format && format != 's')
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	  break;
	}
      if (addressprint && format != 's')
	{
	  /* This used to say `addr', which is unset at this point.
	     Is `address' what is meant?  */
	  fprintf_filtered (stream, "H'%lx ", (unsigned long) address);
	}
      i = TYPE_LENGTH (type);
      LA_PRINT_STRING (stream, valaddr, i, 0);
      /* Return number of characters printed, plus one for the terminating
	 null if we have "reached the end".  */
      return (i + (print_max && i != print_max));
      break;

    case TYPE_CODE_STRUCT:
      chill_print_value_fields (type, valaddr, stream, format, recurse, pretty,
				0);
      break;

    case TYPE_CODE_REF:
      if (addressprint)
        {
	  fprintf_filtered (stream, "LOC(H'%lx)",
	  		    unpack_long (builtin_type_int, valaddr));
	  if (deref_ref)
	    fputs_filtered (": ", stream);
        }
      /* De-reference the reference.  */
      if (deref_ref)
	{
	  if (TYPE_CODE (TYPE_TARGET_TYPE (type)) != TYPE_CODE_UNDEF)
	    {
	      value deref_val =
		value_at
		  (TYPE_TARGET_TYPE (type),
		   unpack_pointer (lookup_pointer_type (builtin_type_void),
				   valaddr));
	      val_print (VALUE_TYPE (deref_val),
			 VALUE_CONTENTS (deref_val),
			 VALUE_ADDRESS (deref_val), stream, format,
			 deref_ref, recurse + 1, pretty);
	    }
	  else
	    fputs_filtered ("???", stream);
	}
      break;

    case TYPE_CODE_ENUM:
      c_val_print (type, valaddr, address, stream, format,
		   deref_ref, recurse, pretty);
      break;

    case TYPE_CODE_MEMBER:
    case TYPE_CODE_UNION:
    case TYPE_CODE_FUNC:
    case TYPE_CODE_VOID:
    case TYPE_CODE_ERROR:
    case TYPE_CODE_RANGE:
    default:
      /* Let's derfer printing to the C printer, rather than
	 print an error message.  FIXME! */
      c_val_print (type, valaddr, address, stream, format,
		   deref_ref, recurse, pretty);
    }
  fflush (stream);
  return (0);
}

/* Mutually recursive subroutines of cplus_print_value and c_val_print to
   print out a structure's fields: cp_print_value_fields and cplus_print_value.

   TYPE, VALADDR, STREAM, RECURSE, and PRETTY have the
   same meanings as in cplus_print_value and c_val_print.

   DONT_PRINT is an array of baseclass types that we
   should not print, or zero if called from top level.  */

static void
chill_print_value_fields (type, valaddr, stream, format, recurse, pretty,
			  dont_print)
     struct type *type;
     char *valaddr;
     FILE *stream;
     int format;
     int recurse;
     enum val_prettyprint pretty;
     struct type **dont_print;
{
  int i, len;
  int fields_seen = 0;

  check_stub_type (type);

  fprintf_filtered (stream, "[");
  len = TYPE_NFIELDS (type);
  if (len == 0)
    {
      fprintf_filtered (stream, "<No data fields>");
    }
  else
    {
      for (i = 0; i < len; i++)
	{
	  if (fields_seen)
	    {
	      fprintf_filtered (stream, ", ");
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
	  fputs_filtered (".", stream);
	  fprintf_symbol_filtered (stream, TYPE_FIELD_NAME (type, i),
				   language_chill, DMGL_NO_OPTS);
	  fputs_filtered (": ", stream);
	  if (TYPE_FIELD_PACKED (type, i))
	    {
	      value v;

	      /* Bitfields require special handling, especially due to byte
		 order problems.  */
	      v = value_from_longest (TYPE_FIELD_TYPE (type, i),
				      unpack_field_as_long (type, valaddr, i));

	      chill_val_print (TYPE_FIELD_TYPE (type, i), VALUE_CONTENTS (v), 0,
			       stream, format, 0, recurse + 1, pretty);
	    }
	  else
	    {
	      chill_val_print (TYPE_FIELD_TYPE (type, i), 
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
  fprintf_filtered (stream, "]");
}

