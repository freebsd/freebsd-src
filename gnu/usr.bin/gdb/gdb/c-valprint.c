/* Support for printing C values for GDB, the GNU debugger.
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
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "value.h"
#include "demangle.h"
#include "valprint.h"
#include "language.h"

/* BEGIN-FIXME */

extern int vtblprint;		/* Controls printing of vtbl's */
extern int demangle;		/* whether to print C++ syms raw or src-form */

extern void
cp_print_class_member PARAMS ((char *, struct type *, FILE *, char *));

extern void
cp_print_class_method PARAMS ((char *, struct type *, FILE *));

extern void
cp_print_value_fields PARAMS ((struct type *, char *, FILE *, int, int,
			       enum val_prettyprint, struct type **));

extern int
cp_is_vtbl_ptr_type PARAMS ((struct type *));

extern int
cp_is_vtbl_member PARAMS ((struct type *));

/* END-FIXME */


/* BEGIN-FIXME:  Hooks into c-typeprint.c */

extern void
c_type_print_varspec_prefix PARAMS ((struct type *, FILE *, int, int));

extern void
cp_type_print_method_args PARAMS ((struct type **, char *, char *, int,
				   FILE *));
/* END-FIXME */


extern struct obstack dont_print_obstack;


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
c_val_print (type, valaddr, address, stream, format, deref_ref, recurse,
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
  register unsigned int i = 0;		/* Number of characters printed */
  unsigned len;
  struct type *elttype;
  unsigned eltlen;
  LONGEST val;
  CORE_ADDR addr;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
      if (TYPE_LENGTH (type) > 0 && TYPE_LENGTH (TYPE_TARGET_TYPE (type)) > 0)
	{
	  elttype = TYPE_TARGET_TYPE (type);
	  eltlen = TYPE_LENGTH (elttype);
	  len = TYPE_LENGTH (type) / eltlen;
	  if (prettyprint_arrays)
	    {
	      print_spaces_filtered (2 + 2 * recurse, stream);
	    }
	  /* For an array of chars, print with string syntax.  */
	  if (eltlen == 1 && TYPE_CODE (elttype) == TYPE_CODE_INT
	      && (format == 0 || format == 's'))
	    {
	      LA_PRINT_STRING (stream, valaddr, len, 0);
	      i = len;
	    }
	  else
	    {
	      fprintf_filtered (stream, "{");
	      /* If this is a virtual function table, print the 0th
		 entry specially, and the rest of the members normally.  */
	      if (cp_is_vtbl_ptr_type (elttype))
		{
		  i = 1;
		  fprintf_filtered (stream, "%d vtable entries", len - 1);
		}
	      else
		{
		  i = 0;
		}
	      val_print_array_elements (type, valaddr, address, stream,
					format, deref_ref, recurse, pretty, i);
	      fprintf_filtered (stream, "}");
	    }
	  break;
	}
      /* Array of unspecified length: treat like pointer to first elt.  */
      addr = address;
      goto print_unpacked_pointer;

    case TYPE_CODE_PTR:
      if (format && format != 's')
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	  break;
	}
      if (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_METHOD)
	{
	  cp_print_class_method (valaddr, type, stream);
	}
      else if (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_MEMBER)
	{
	  cp_print_class_member (valaddr,
				 TYPE_DOMAIN_TYPE (TYPE_TARGET_TYPE (type)),
				 stream, "&");
	}
      else
	{
	  addr = unpack_pointer (type, valaddr);
	print_unpacked_pointer:
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
	      fprintf_filtered (stream, "0x%lx", (unsigned long)addr);
	    }

	  /* For a pointer to char or unsigned char, also print the string
	     pointed to, unless pointer is null.  */
	  if (TYPE_LENGTH (elttype) == 1
	      && TYPE_CODE (elttype) == TYPE_CODE_INT
	      && (format == 0 || format == 's')
	      && addr != 0)
	    {
	      i = val_print_string (addr, 0, stream);
	    }
	  else if (cp_is_vtbl_member(type))
  	    {
	      /* print vtbl's nicely */
	      CORE_ADDR vt_address = unpack_pointer (type, valaddr);

	      struct minimal_symbol *msymbol =
		lookup_minimal_symbol_by_pc (vt_address);
	      if ((msymbol != NULL) &&
		  (vt_address == SYMBOL_VALUE_ADDRESS (msymbol)))
		{
		  fputs_filtered (" <", stream);
		  fputs_filtered (SYMBOL_SOURCE_NAME (msymbol), stream);
		  fputs_filtered (">", stream);
		}
	      if (vtblprint)
	        {
		  value vt_val;
	          struct symbol *wsym = (struct symbol *)NULL;
	          struct type *wtype;
		  struct symtab *s;
		  struct block *block = (struct block *)NULL;
		  int is_this_fld;


              	  wsym = lookup_symbol (SYMBOL_NAME(msymbol), block, 
				VAR_NAMESPACE, &is_this_fld, &s);
 
		  if (wsym)
		    {
	              wtype = SYMBOL_TYPE(wsym);
		    }
		  else
		    {
		      wtype = TYPE_TARGET_TYPE(type);
		    }
		  vt_val = value_at (wtype, vt_address);
		  val_print (VALUE_TYPE (vt_val), VALUE_CONTENTS (vt_val),
			     VALUE_ADDRESS (vt_val), stream, format,
			     deref_ref, recurse + 1, pretty);
		  if (pretty)
		    {
		      fprintf_filtered (stream, "\n");
		      print_spaces_filtered (2 + 2 * recurse, stream);
		    }
	        }
	      }

	  /* Return number of characters printed, plus one for the
	     terminating null if we have "reached the end".  */
	  return (i + (print_max && i != print_max));
	}
      break;

    case TYPE_CODE_MEMBER:
      error ("not implemented: member type in c_val_print");
      break;

    case TYPE_CODE_REF:
      if (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_MEMBER)
        {
	  cp_print_class_member (valaddr,
				 TYPE_DOMAIN_TYPE (TYPE_TARGET_TYPE (type)),
				 stream, "");
	  break;
	}
      if (addressprint)
        {
	  fprintf_filtered (stream, "@0x%lx",
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

    case TYPE_CODE_UNION:
      if (recurse && !unionprint)
	{
	  fprintf_filtered (stream, "{...}");
	  break;
	}
      /* Fall through.  */
    case TYPE_CODE_STRUCT:
      if (vtblprint && cp_is_vtbl_ptr_type(type))
	{
          /* Print the unmangled name if desired.  */
	  print_address_demangle(*((int *) (valaddr +	/* FIXME bytesex */
	      TYPE_FIELD_BITPOS (type, VTBL_FNADDR_OFFSET) / 8)),
	      stream, demangle);
	  break;
	}
      cp_print_value_fields (type, valaddr, stream, format, recurse, pretty,
			     0);
      break;

    case TYPE_CODE_ENUM:
      if (format)
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	  break;
	}
      len = TYPE_NFIELDS (type);
      val = unpack_long (type, valaddr);
      for (i = 0; i < len; i++)
	{
	  QUIT;
	  if (val == TYPE_FIELD_BITPOS (type, i))
	    {
	      break;
	    }
	}
      if (i < len)
	{
	  fputs_filtered (TYPE_FIELD_NAME (type, i), stream);
	}
      else
	{
	  print_longest (stream, 'd', 0, val);
	}
      break;

    case TYPE_CODE_FUNC:
      if (format)
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	  break;
	}
      /* FIXME, we should consider, at least for ANSI C language, eliminating
	 the distinction made between FUNCs and POINTERs to FUNCs.  */
      fprintf_filtered (stream, "{");
      type_print (type, "", stream, -1);
      fprintf_filtered (stream, "} ");
      /* Try to print what function it points to, and its address.  */
      print_address_demangle (address, stream, demangle);
      break;

    case TYPE_CODE_BOOL:
      /* Do something at least vaguely reasonable, for example if the
	 language is set wrong.  */

    case TYPE_CODE_INT:
      format = format ? format : output_format;
      if (format)
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	}
      else
	{
	  val_print_type_code_int (type, valaddr, stream);
	  /* C and C++ has no single byte int type, char is used instead.
	     Since we don't know whether the value is really intended to
	     be used as an integer or a character, print the character
	     equivalent as well. */
	  if (TYPE_LENGTH (type) == 1)
	    {
	      fputs_filtered (" ", stream);
	      LA_PRINT_CHAR ((unsigned char) unpack_long (type, valaddr),
			     stream);
	    }
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
	  fprintf_filtered (stream, TYPE_UNSIGNED (type) ? "%u" : "%d",
			    unpack_long (type, valaddr));
	  fputs_filtered (" ", stream);
	  LA_PRINT_CHAR ((unsigned char) unpack_long (type, valaddr), stream);
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

    case TYPE_CODE_VOID:
      fprintf_filtered (stream, "void");
      break;

    case TYPE_CODE_ERROR:
      fprintf_filtered (stream, "<error type>");
      break;

    case TYPE_CODE_RANGE:
      /* FIXME, we should not ever have to print one of these yet.  */
      fprintf_filtered (stream, "<range type>");
      break;

    case TYPE_CODE_UNDEF:
      /* This happens (without TYPE_FLAG_STUB set) on systems which don't use
	 dbx xrefs (NO_DBX_XREFS in gcc) if a file has a "struct foo *bar"
	 and no complete type for struct foo in that file.  */
      fprintf_filtered (stream, "<incomplete type>");
      break;

    default:
      error ("Invalid C/C++ type code %d in symbol table.", TYPE_CODE (type));
    }
  fflush (stream);
  return (0);
}
