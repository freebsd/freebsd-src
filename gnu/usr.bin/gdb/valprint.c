/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)valprint.c	6.5 (Berkeley) 5/8/91";
#endif /* not lint */

/* Print values for GNU debugger gdb.
   Copyright (C) 1986, 1988, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include "defs.h"
#include "param.h"
#include "symtab.h"
#include "value.h"

/* GNU software is only expected to run on systems with 32-bit integers.  */
#define UINT_MAX 0xffffffff

/* Maximum number of chars to print for a string pointer value
   or vector contents, or UINT_MAX for no limit.  */

static unsigned int print_max;

static void type_print_varspec_suffix ();
static void type_print_varspec_prefix ();
static void type_print_base ();
static void type_print_method_args ();


char **unsigned_type_table;
char **signed_type_table;
char **float_type_table;


/* Print repeat counts if there are more than this
   many repetitions of an element in an array.  */
#define	REPEAT_COUNT_THRESHOLD	10

/* Print the character string STRING, printing at most LENGTH characters.
   Printing stops early if the number hits print_max; repeat counts
   are printed as appropriate.  Print ellipses at the end if we
   had to stop before printing LENGTH characters, or if FORCE_ELLIPSES.  */

void
print_string (stream, string, length, force_ellipses)
     FILE *stream;
     char *string;
     unsigned int length;
     int force_ellipses;
{
  register unsigned int i;
  unsigned int things_printed = 0;
  int in_quotes = 0;
  int need_comma = 0;

  if (length == 0)
    {
      fputs_filtered ("\"\"", stdout);
      return;
    }

  for (i = 0; i < length && things_printed < print_max; ++i)
    {
      /* Position of the character we are examining
	 to see whether it is repeated.  */
      unsigned int rep1;
      /* Number of repititions we have detected so far.  */
      unsigned int reps;

      QUIT;

      if (need_comma)
	{
	  fputs_filtered (", ", stream);
	  need_comma = 0;
	}

      rep1 = i + 1;
      reps = 1;
      while (rep1 < length && string[rep1] == string[i])
	{
	  ++rep1;
	  ++reps;
	}

      if (reps > REPEAT_COUNT_THRESHOLD)
	{
	  if (in_quotes)
	    {
	      fputs_filtered ("\", ", stream);
	      in_quotes = 0;
	    }
	  fputs_filtered ("'", stream);
	  printchar (string[i], stream, '\'');
	  fprintf_filtered (stream, "' <repeats %u times>", reps);
	  i = rep1 - 1;
	  things_printed += REPEAT_COUNT_THRESHOLD;
	  need_comma = 1;
	}
      else
	{
	  if (!in_quotes)
	    {
	      fputs_filtered ("\"", stream);
	      in_quotes = 1;
	    }
	  printchar (string[i], stream, '"');
	  ++things_printed;
	}
    }

  /* Terminate the quotes if necessary.  */
  if (in_quotes)
    fputs_filtered ("\"", stream);

  if (force_ellipses || i < length)
    fputs_filtered ("...", stream);
}

/* Print the value VAL in C-ish syntax on stream STREAM.
   FORMAT is a format-letter, or 0 for print in natural format of data type.
   If the object printed is a string pointer, returns
   the number of string bytes printed.  */

int
value_print (val, stream, format, pretty)
     value val;
     FILE *stream;
     char format;
     enum val_prettyprint pretty;
{
  register unsigned int i, n, typelen;

  /* A "repeated" value really contains several values in a row.
     They are made by the @ operator.
     Print such values as if they were arrays.  */

  if (VALUE_REPEATED (val))
    {
      n = VALUE_REPETITIONS (val);
      typelen = TYPE_LENGTH (VALUE_TYPE (val));
      fprintf_filtered (stream, "{");
      /* Print arrays of characters using string syntax.  */
      if (typelen == 1 && TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_INT
	  && format == 0)
	print_string (stream, VALUE_CONTENTS (val), n, 0);
      else
	{
	  unsigned int things_printed = 0;
	  
	  for (i = 0; i < n && things_printed < print_max; i++)
	    {
	      /* Position of the array element we are examining to see
		 whether it is repeated.  */
	      unsigned int rep1;
	      /* Number of repititions we have detected so far.  */
	      unsigned int reps;

	      if (i != 0)
		fprintf_filtered (stream, ", ");

	      rep1 = i + 1;
	      reps = 1;
	      while (rep1 < n
		     && !bcmp (VALUE_CONTENTS (val) + typelen * i,
			       VALUE_CONTENTS (val) + typelen * rep1, typelen))
		{
		  ++reps;
		  ++rep1;
		}

	      if (reps > REPEAT_COUNT_THRESHOLD)
		{
		  val_print (VALUE_TYPE (val),
			     VALUE_CONTENTS (val) + typelen * i,
			     VALUE_ADDRESS (val) + typelen * i,
			     stream, format, 1, 0, pretty);
		  fprintf (stream, " <repeats %u times>", reps);
		  i = rep1 - 1;
		  things_printed += REPEAT_COUNT_THRESHOLD;
		}
	      else
		{
		  val_print (VALUE_TYPE (val),
			     VALUE_CONTENTS (val) + typelen * i,
			     VALUE_ADDRESS (val) + typelen * i,
			     stream, format, 1, 0, pretty);
		  things_printed++;
		}
	    }
	  if (i < n)
	    fprintf_filtered (stream, "...");
	}
      fprintf_filtered (stream, "}");
      return n * typelen;
    }
  else
    {
      /* If it is a pointer, indicate what it points to.

	 Print type also if it is a reference.

         C++: if it is a member pointer, we will take care
	 of that when we print it.  */
      if (TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_PTR
	  || TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_REF)
	{
	  fprintf_filtered (stream, "(");
	  type_print (VALUE_TYPE (val), "", stream, -1);
	  fprintf_filtered (stream, ") ");

	  /* If this is a function pointer, try to print what
	     function it is pointing to by name.  */
	  if (TYPE_CODE (TYPE_TARGET_TYPE (VALUE_TYPE (val)))
	      == TYPE_CODE_FUNC)
	    {
	      print_address (((int *) VALUE_CONTENTS (val))[0], stream);
	      /* Return value is irrelevant except for string pointers.  */
	      return 0;
	    }
	}
      return val_print (VALUE_TYPE (val), VALUE_CONTENTS (val),
			VALUE_ADDRESS (val), stream, format, 1, 0, pretty);
    }
}

static int prettyprint;	/* Controls prettyprinting of structures.  */
int unionprint;		/* Controls printing of nested unions.  */
static void scalar_print_hack();
void (*default_scalar_print)() = scalar_print_hack;

/* Print data of type TYPE located at VALADDR (within GDB),
   which came from the inferior at address ADDRESS,
   onto stdio stream STREAM according to FORMAT
   (a letter or 0 for natural format).

   If the data are a string pointer, returns the number of
   sting characters printed.

   if DEREF_REF is nonzero, then dereference references,
   otherwise just print them like pointers.

   The PRETTY parameter controls prettyprinting.  */

int
val_print (type, valaddr, address, stream, format,
	   deref_ref, recurse, pretty)
     struct type *type;
     char *valaddr;
     CORE_ADDR address;
     FILE *stream;
     char format;
     int deref_ref;
     int recurse;
     enum val_prettyprint pretty;
{
  register unsigned int i;
  int len, n_baseclasses;
  struct type *elttype;
  int eltlen;
  LONGEST val;
  unsigned char c;

  if (pretty == Val_pretty_default)
    {
      pretty = prettyprint ? Val_prettyprint : Val_no_prettyprint;
    }
  
  QUIT;

  if (TYPE_FLAGS (type) & TYPE_FLAG_STUB)
    {
      fprintf_filtered (stream, "<Type not defined in this context>");
      fflush (stream);
      return 0;
    }
  
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
      if (TYPE_LENGTH (type) >= 0
	  && TYPE_LENGTH (TYPE_TARGET_TYPE (type)) > 0)
	{
	  elttype = TYPE_TARGET_TYPE (type);
	  eltlen = TYPE_LENGTH (elttype);
	  len = TYPE_LENGTH (type) / eltlen;
	  fprintf_filtered (stream, "{");
	  /* For an array of chars, print with string syntax.  */
	  if (eltlen == 1 && TYPE_CODE (elttype) == TYPE_CODE_INT
	      && format == 0)
	    print_string (stream, valaddr, len, 0);
	  else
	    {
	      unsigned int things_printed = 0;
	      
	      for (i = 0; i < len && things_printed < print_max; i++)
		{
		  /* Position of the array element we are examining to see
		     whether it is repeated.  */
		  unsigned int rep1;
		  /* Number of repititions we have detected so far.  */
		  unsigned int reps;
		  
		  if (i > 0)
		    fprintf_filtered (stream, ", ");
		  
		  rep1 = i + 1;
		  reps = 1;
		  while (rep1 < len
			 && !bcmp (valaddr + i * eltlen,
				   valaddr + rep1 * eltlen, eltlen))
		    {
		      ++reps;
		      ++rep1;
		    }

		  if (reps > REPEAT_COUNT_THRESHOLD)
		    {
		      val_print (elttype, valaddr + i * eltlen,
				 0, stream, format, deref_ref,
				 recurse + 1, pretty);
		      fprintf_filtered (stream, " <repeats %u times>", reps);
		      i = rep1 - 1;
		      things_printed += REPEAT_COUNT_THRESHOLD;
		    }
		  else
		    {
		      val_print (elttype, valaddr + i * eltlen,
				 0, stream, format, deref_ref,
				 recurse + 1, pretty);
		      things_printed++;
		    }
		}
	      if (i < len)
		fprintf_filtered (stream, "...");
	    }
	  fprintf_filtered (stream, "}");
	  break;
	}
      /* Array of unspecified length: treat like pointer to first elt.  */
      valaddr = (char *) &address;

    case TYPE_CODE_PTR:
      if (format)
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	  break;
	}
      if (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_METHOD)
	{
	  struct type *domain = TYPE_DOMAIN_TYPE (TYPE_TARGET_TYPE (type));
	  struct type *target = TYPE_TARGET_TYPE (TYPE_TARGET_TYPE (type));
	  struct fn_field *f;
	  int j, len2;
	  char *kind = "";

	  val = unpack_long (builtin_type_int, valaddr);
	  if (val < 128)
	    {
	      len = TYPE_NFN_FIELDS (domain);
	      for (i = 0; i < len; i++)
		{
		  f = TYPE_FN_FIELDLIST1 (domain, i);
		  len2 = TYPE_FN_FIELDLIST_LENGTH (domain, i);

		  for (j = 0; j < len2; j++)
		    {
		      QUIT;
		      if (TYPE_FN_FIELD_VOFFSET (f, j) == val)
			{
			  kind = "virtual";
			  goto common;
			}
		    }
		}
	    }
	  else
	    {
	      struct symbol *sym = find_pc_function ((CORE_ADDR) val);
	      if (sym == 0)
		error ("invalid pointer to member function");
	      len = TYPE_NFN_FIELDS (domain);
	      for (i = 0; i < len; i++)
		{
		  f = TYPE_FN_FIELDLIST1 (domain, i);
		  len2 = TYPE_FN_FIELDLIST_LENGTH (domain, i);

		  for (j = 0; j < len2; j++)
		    {
		      QUIT;
		      if (!strcmp (SYMBOL_NAME (sym), TYPE_FN_FIELD_PHYSNAME (f, j)))
			goto common;
		    }
		}
	    }
	common:
	  if (i < len)
	    {
	      fprintf_filtered (stream, "&");
	      type_print_varspec_prefix (TYPE_FN_FIELD_TYPE (f, j), stream, 0, 0);
	      fprintf (stream, kind);
	      if (TYPE_FN_FIELD_PHYSNAME (f, j)[0] == '_'
		  && TYPE_FN_FIELD_PHYSNAME (f, j)[1] == '$')
		type_print_method_args
		  (TYPE_FN_FIELD_ARGS (f, j) + 1, "~",
		   TYPE_FN_FIELDLIST_NAME (domain, i), 0, stream);
	      else
		type_print_method_args
		  (TYPE_FN_FIELD_ARGS (f, j), "",
		   TYPE_FN_FIELDLIST_NAME (domain, i), 0, stream);
	      break;
	    }
	  fprintf_filtered (stream, "(");
  	  type_print (type, "", stream, -1);
	  fprintf_filtered (stream, ") %d", (int) val >> 3);
	}
      else if (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_MEMBER)
	{
	  struct type *domain = TYPE_DOMAIN_TYPE (TYPE_TARGET_TYPE (type));
	  struct type *target = TYPE_TARGET_TYPE (TYPE_TARGET_TYPE (type));
	  char *kind = "";

	  /* VAL is a byte offset into the structure type DOMAIN.
	     Find the name of the field for that offset and
	     print it.  */
	  int extra = 0;
	  int bits = 0;
	  len = TYPE_NFIELDS (domain);
	  /* @@ Make VAL into bit offset */
	  val = unpack_long (builtin_type_int, valaddr) << 3;
	  for (i = 0; i < len; i++)
	    {
	      int bitpos = TYPE_FIELD_BITPOS (domain, i);
	      QUIT;
	      if (val == bitpos)
		break;
	      if (val < bitpos && i > 0)
		{
		  int ptrsize = (TYPE_LENGTH (builtin_type_char) * TYPE_LENGTH (target));
		  /* Somehow pointing into a field.  */
		  i -= 1;
		  extra = (val - TYPE_FIELD_BITPOS (domain, i));
		  if (extra & 0x3)
		    bits = 1;
		  else
		    extra >>= 3;
		  break;
		}
	    }
	  if (i < len)
	    {
	      fprintf_filtered (stream, "&");
	      type_print_base (domain, stream, 0, 0);
	      fprintf_filtered (stream, "::");
	      fputs_filtered (TYPE_FIELD_NAME (domain, i), stream);
	      if (extra)
		fprintf_filtered (stream, " + %d bytes", extra);
	      if (bits)
		fprintf_filtered (stream, " (offset in bits)");
	      break;
	    }
	  fprintf_filtered (stream, "%d", val >> 3);
	}
      else
	{
	  fprintf_filtered (stream, "0x%x", * (int *) valaddr);
	  /* For a pointer to char or unsigned char,
	     also print the string pointed to, unless pointer is null.  */
	  
	  /* For an array of chars, print with string syntax.  */
	  elttype = TYPE_TARGET_TYPE (type);
	  i = 0;		/* Number of characters printed.  */
	  if (TYPE_LENGTH (elttype) == 1 
	      && TYPE_CODE (elttype) == TYPE_CODE_INT
	      && format == 0
	      && unpack_long (type, valaddr) != 0
	      /* If print_max is UINT_MAX, the alloca below will fail.
	         In that case don't try to print the string.  */
	      && print_max < UINT_MAX)
	    {
	      fprintf_filtered (stream, " ");

	      /* Get first character.  */
	      if (read_memory ( (CORE_ADDR) unpack_long (type, valaddr),
			       &c, 1))
		{
		  /* First address out of bounds.  */
		  fprintf_filtered (stream, "<Address 0x%x out of bounds>",
			   (* (int *) valaddr));
		  break;
		}
	      else
		{
		  /* A real string.  */
		  int out_of_bounds = 0;
		  char *string = (char *) alloca (print_max);

		  /* If the loop ends by us hitting print_max characters,
		     we need to have elipses at the end.  */
		  int force_ellipses = 1;

		  /* This loop only fetches print_max characters, even
		     though print_string might want to print more
		     (with repeated characters).  This is so that
		     we don't spend forever fetching if we print
		     a long string consisting of the same character
		     repeated.  */
		  while (i < print_max)
		    {
		      QUIT;
		      if (read_memory ((CORE_ADDR) unpack_long (type, valaddr)
				       + i, &c, 1))
			{
			  out_of_bounds = 1;
			  force_ellipses = 0;
			  break;
			}
		      else if (c == '\0')
			{
			  force_ellipses = 0;
			  break;
			}
		      else
			string[i++] = c;
		    }

		  if (i != 0)
		    print_string (stream, string, i, force_ellipses);
		  if (out_of_bounds)
		    fprintf_filtered (stream,
				      " <Address 0x%x out of bounds>",
				      (*(int *) valaddr) + i);
		}

	      fflush (stream);
	    }
	  /* Return number of characters printed, plus one for the
	     terminating null if we have "reached the end".  */
	  return i + (print_max && i != print_max);
	}
      break;

    case TYPE_CODE_MEMBER:
      error ("not implemented: member type in val_print");
      break;

    case TYPE_CODE_REF:
      fprintf_filtered (stream, "(0x%x &) = ", * (int *) valaddr);
      /* De-reference the reference.  */
      if (deref_ref)
	{
	  if (TYPE_CODE (TYPE_TARGET_TYPE (type)) != TYPE_CODE_UNDEF)
	    {
	      value val = value_at (TYPE_TARGET_TYPE (type), * (int *) valaddr);
	      val_print (VALUE_TYPE (val), VALUE_CONTENTS (val),
			 VALUE_ADDRESS (val), stream, format,
			 deref_ref, recurse + 1, pretty);
	    }
	  else
	    fprintf_filtered (stream, "???");
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
      fprintf_filtered (stream, "{");
      len = TYPE_NFIELDS (type);
      n_baseclasses = TYPE_N_BASECLASSES (type);
      for (i = 1; i <= n_baseclasses; i++)
	{
	  fprintf_filtered (stream, "\n");
	  if (pretty)
	    print_spaces_filtered (2 + 2 * recurse, stream);
	  fputs_filtered ("<", stream);
	  fputs_filtered (TYPE_NAME (TYPE_BASECLASS (type, i)), stream);
	  fputs_filtered ("> = ", stream);
	  val_print (TYPE_FIELD_TYPE (type, 0),
		     valaddr + TYPE_FIELD_BITPOS (type, i-1) / 8,
		     0, stream, 0, 0, recurse + 1, pretty);
	}
      if (i > 1) {
	fprintf_filtered (stream, "\n");
	print_spaces_filtered (2 + 2 * recurse, stream);
	fputs_filtered ("members of ", stream);
        fputs_filtered (TYPE_NAME (type), stream);
        fputs_filtered (": ", stream);
      }
      if (!len && i == 1)
	fprintf_filtered (stream, "<No data fields>");
      else
	{
	  for (i -= 1; i < len; i++)
	    {
	      if (i > n_baseclasses) fprintf_filtered (stream, ", ");
	      if (pretty)
		{
		  fprintf_filtered (stream, "\n");
		  print_spaces_filtered (2 + 2 * recurse, stream);
		}
	      fputs_filtered (TYPE_FIELD_NAME (type, i), stream);
	      fputs_filtered (" = ", stream);
	      /* check if static field */
	      if (TYPE_FIELD_STATIC (type, i))
		{
		  value v;
		  
		  v = value_static_field (type, TYPE_FIELD_NAME (type, i), i);
		  val_print (TYPE_FIELD_TYPE (type, i),
			     VALUE_CONTENTS (v), 0, stream, format,
			     deref_ref, recurse + 1, pretty);
		}
	      else if (TYPE_FIELD_PACKED (type, i))
		{
		  char *valp = (char *) & val;
		  union {int i; char c;} test;
		  test.i = 1;
		  if (test.c != 1)
		    valp += sizeof val - TYPE_LENGTH (TYPE_FIELD_TYPE (type, i));
		  val = unpack_field_as_long (type, valaddr, i);
		  val_print (TYPE_FIELD_TYPE (type, i), valp, 0,
			     stream, format, deref_ref, recurse + 1, pretty);
		}
	      else
		{
		  val_print (TYPE_FIELD_TYPE (type, i), 
			     valaddr + TYPE_FIELD_BITPOS (type, i) / 8,
			     0, stream, format, deref_ref,
			     recurse + 1, pretty);
		}
	    }
	  if (pretty)
	    {
	      fprintf_filtered (stream, "\n");
	      print_spaces_filtered (2 * recurse, stream);
	    }
	}
      fprintf_filtered (stream, "}");
      break;

    case TYPE_CODE_ENUM:
      if (format)
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	  break;
	}
      len = TYPE_NFIELDS (type);
      val = unpack_long (builtin_type_int, valaddr);
      for (i = 0; i < len; i++)
	{
	  QUIT;
	  if (val == TYPE_FIELD_BITPOS (type, i))
	    break;
	}
      if (i < len)
	fputs_filtered (TYPE_FIELD_NAME (type, i), stream);
      else
	fprintf_filtered (stream, "%d", (int) val);
      break;

    case TYPE_CODE_FUNC:
      if (format)
	{
	  print_scalar_formatted (valaddr, type, format, 0, stream);
	  break;
	}
      fprintf_filtered (stream, "{");
      type_print (type, "", stream, -1);
      fprintf_filtered (stream, "} ");
      fprintf_filtered (stream, "0x%x", address);
      break;

    case TYPE_CODE_INT:
      if (format)
	print_scalar_formatted (valaddr, type, format, 0, stream);
      else
	{
	  (*default_scalar_print)(stream, type, unpack_long(type, valaddr));
#ifdef notdef
	  if (TYPE_LENGTH (type) == 1)
	    {
	      fprintf_filtered (stream, " '");
	      printchar ((unsigned char) unpack_long (type, valaddr), 
			 stream, '\'');
	      fprintf_filtered (stream, "'");
	    }
#endif
	}
      break;

    case TYPE_CODE_FLT:
      if (format)
	print_scalar_formatted (valaddr, type, format, 0, stream);
      else
	print_floating (valaddr, type, stream);
      break;

    case TYPE_CODE_VOID:
      fprintf_filtered (stream, "void");
      break;

    default:
      error ("Invalid type code in symbol table.");
    }
  fflush (stream);
  return 0;
}

/* Print a description of a type TYPE
   in the form of a declaration of a variable named VARSTRING.
   Output goes to STREAM (via stdio).
   If SHOW is positive, we show the contents of the outermost level
   of structure even if there is a type name that could be used instead.
   If SHOW is negative, we never show the details of elements' types.  */

void
type_print (type, varstring, stream, show)
     struct type *type;
     char *varstring;
     FILE *stream;
     int show;
{
  type_print_1 (type, varstring, stream, show, 0);
}

/* LEVEL is the depth to indent lines by.  */

void
type_print_1 (type, varstring, stream, show, level)
     struct type *type;
     char *varstring;
     FILE *stream;
     int show;
     int level;
{
  register enum type_code code;
  type_print_base (type, stream, show, level);
  code = TYPE_CODE (type);
  if ((varstring && *varstring)
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
    fprintf_filtered (stream, " ");
  type_print_varspec_prefix (type, stream, show, 0);
  fputs_filtered (varstring, stream);
  type_print_varspec_suffix (type, stream, show, 0);
}

/* Print the method arguments ARGS to the file STREAM.  */
static void
type_print_method_args (args, prefix, varstring, staticp, stream)
     struct type **args;
     char *prefix, *varstring;
     int staticp;
     FILE *stream;
{
  int i;

  fputs_filtered (" ", stream);
  fputs_filtered (prefix, stream);
  fputs_filtered (varstring, stream);
  fputs_filtered (" (", stream);
  if (args && args[!staticp] && args[!staticp]->code != TYPE_CODE_VOID)
    {
      i = !staticp;		/* skip the class variable */
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
  fprintf_filtered (stream, ")");
}
  
/* If TYPE is a derived type, then print out derivation
   information.  Print out all layers of the type heirarchy
   until we encounter one with multiple inheritance.
   At that point, print out that ply, and return.  */
static void
type_print_derivation_info (stream, type)
     FILE *stream;
     struct type *type;
{
  char *name;
  int i, n_baseclasses = TYPE_N_BASECLASSES (type);
  struct type *basetype = 0;

  while (type && n_baseclasses == 1)
    {
      basetype = TYPE_BASECLASS (type, 1);
      if (TYPE_NAME (basetype) && (name = TYPE_NAME (basetype)))
	{
	  while (*name != ' ') name++;
	  fprintf_filtered (stream, ": %s%s ",
		   TYPE_VIA_PUBLIC (basetype) ? "public" : "private",
		   TYPE_VIA_VIRTUAL (basetype) ? " virtual" : "");
	  fputs_filtered (name + 1, stream);
	  fputs_filtered (" ", stream);
	}
      n_baseclasses = TYPE_N_BASECLASSES (basetype);
      type = basetype;
    }

  if (type)
    {
      if (n_baseclasses != 0)
	fprintf_filtered (stream, ": ");
      for (i = 1; i <= n_baseclasses; i++)
	{
	  basetype = TYPE_BASECLASS (type, i);
	  if (TYPE_NAME (basetype) && (name = TYPE_NAME (basetype)))
	    {
	      while (*name != ' ') name++;
	      fprintf_filtered (stream, "%s%s ",
		       TYPE_VIA_PUBLIC (basetype) ? "public" : "private",
		       TYPE_VIA_VIRTUAL (basetype) ? " virtual" : "");
	      fputs_filtered (name + 1, stream);
	    }
	  if (i < n_baseclasses)
	    fprintf_filtered (stream, ", ");
	}
      fprintf_filtered (stream, " ");
    }
}

/* Print any asterisks or open-parentheses needed before the
   variable name (to describe its type).

   On outermost call, pass 0 for PASSED_A_PTR.
   On outermost call, SHOW > 0 means should ignore
   any typename for TYPE and show its details.
   SHOW is always zero on recursive calls.  */

static void
type_print_varspec_prefix (type, stream, show, passed_a_ptr)
     struct type *type;
     FILE *stream;
     int show;
     int passed_a_ptr;
{
  if (type == 0)
    return;

  if (TYPE_NAME (type) && show <= 0)
    return;

  QUIT;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_PTR:
      type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0, 1);
      fprintf_filtered (stream, "*");
      break;

    case TYPE_CODE_MEMBER:
      if (passed_a_ptr)
	fprintf_filtered (stream, "(");
      type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0,
				 0);
      fprintf_filtered (stream, " ");
      type_print_base (TYPE_DOMAIN_TYPE (type), stream, 0,
		       passed_a_ptr);
      fprintf_filtered (stream, "::");
      break;

    case TYPE_CODE_METHOD:
      if (passed_a_ptr)
	fprintf (stream, "(");
      type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0,
				 0);
      fprintf_filtered (stream, " ");
      type_print_base (TYPE_DOMAIN_TYPE (type), stream, 0,
		       passed_a_ptr);
      fprintf_filtered (stream, "::");
      break;

    case TYPE_CODE_REF:
      type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0, 1);
      fprintf_filtered (stream, "&");
      break;

    case TYPE_CODE_FUNC:
      type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0,
				 0);
      if (passed_a_ptr)
	fprintf_filtered (stream, "(");
      break;

    case TYPE_CODE_ARRAY:
      type_print_varspec_prefix (TYPE_TARGET_TYPE (type), stream, 0,
				 0);
      if (passed_a_ptr)
	fprintf_filtered (stream, "(");
    }
}

/* Print any array sizes, function arguments or close parentheses
   needed after the variable name (to describe its type).
   Args work like type_print_varspec_prefix.  */

static void
type_print_varspec_suffix (type, stream, show, passed_a_ptr)
     struct type *type;
     FILE *stream;
     int show;
     int passed_a_ptr;
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
      if (TYPE_LENGTH (type) >= 0
	  && TYPE_LENGTH (TYPE_TARGET_TYPE (type)) > 0)
	fprintf_filtered (stream, "%d",
			  (TYPE_LENGTH (type)
			   / TYPE_LENGTH (TYPE_TARGET_TYPE (type))));
      fprintf_filtered (stream, "]");
      
      type_print_varspec_suffix (TYPE_TARGET_TYPE (type), stream, 0,
				 0);
      break;

    case TYPE_CODE_MEMBER:
      if (passed_a_ptr)
	fprintf_filtered (stream, ")");
      type_print_varspec_suffix (TYPE_TARGET_TYPE (type), stream, 0, 0);
      break;

    case TYPE_CODE_METHOD:
      if (passed_a_ptr)
	fprintf_filtered (stream, ")");
      type_print_varspec_suffix (TYPE_TARGET_TYPE (type), stream, 0, 0);
      if (passed_a_ptr)
	{
	  int i;
	  struct type **args = TYPE_ARG_TYPES (type);

	  fprintf_filtered (stream, "(");
	  if (args[1] == 0)
	    fprintf_filtered (stream, "...");
	  else for (i = 1; args[i] != 0 && args[i]->code != TYPE_CODE_VOID; i++)
	    {
	      type_print_1 (args[i], "", stream, -1, 0);
	      if (args[i+1] == 0)
		fprintf_filtered (stream, "...");
	      else if (args[i+1]->code != TYPE_CODE_VOID)
		fprintf_filtered (stream, ",");
	    }
	  fprintf_filtered (stream, ")");
	}
      break;

    case TYPE_CODE_PTR:
    case TYPE_CODE_REF:
      type_print_varspec_suffix (TYPE_TARGET_TYPE (type), stream, 0, 1);
      break;

    case TYPE_CODE_FUNC:
      type_print_varspec_suffix (TYPE_TARGET_TYPE (type), stream, 0,
				 passed_a_ptr);
      if (passed_a_ptr)
	fprintf_filtered (stream, ")");
      fprintf_filtered (stream, "()");
      break;
    }
}

/* Print the name of the type (or the ultimate pointer target,
   function value or array element), or the description of a
   structure or union.

   SHOW nonzero means don't print this type as just its name;
   show its real definition even if it has a name.
   SHOW zero means print just typename or struct tag if there is one
   SHOW negative means abbreviate structure elements.
   SHOW is decremented for printing of structure elements.

   LEVEL is the depth to indent by.
   We increase it for some recursive calls.  */

static void
type_print_base (type, stream, show, level)
     struct type *type;
     FILE *stream;
     int show;
     int level;
{
  char *name;
  register int i;
  register int len;
  register int lastval;

  QUIT;

  if (type == 0)
    {
      fprintf_filtered (stream, "type unknown");
      return;
    }

  if (TYPE_NAME (type) && show <= 0)
    {
      fputs_filtered (TYPE_NAME (type), stream);
      return;
    }

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_ARRAY:
    case TYPE_CODE_PTR:
    case TYPE_CODE_MEMBER:
    case TYPE_CODE_REF:
    case TYPE_CODE_FUNC:
    case TYPE_CODE_METHOD:
      type_print_base (TYPE_TARGET_TYPE (type), stream, show, level);
      break;

    case TYPE_CODE_STRUCT:
      fprintf_filtered (stream, "struct ");
      goto struct_union;

    case TYPE_CODE_UNION:
      fprintf_filtered (stream, "union ");
    struct_union:
      if (TYPE_NAME (type) && (name = TYPE_NAME (type)))
	{
	  while (*name != ' ') name++;
	  fputs_filtered (name + 1, stream);
	  fputs_filtered (" ", stream);
	}
      if (show < 0)
	fprintf_filtered (stream, "{...}");
      else
	{
	  int i;

	  type_print_derivation_info (stream, type);
	  
	  fprintf_filtered (stream, "{");
	  len = TYPE_NFIELDS (type);
	  if (len)
	    fprintf_filtered (stream, "\n");
	  else
	    {
	      if (TYPE_FLAGS (type) & TYPE_FLAG_STUB)
		fprintf_filtered (stream, "<incomplete type>\n");
	      else
		fprintf_filtered (stream, "<no data fields>\n");
	    }

	  /* If there is a base class for this type,
	     do not print the field that it occupies.  */
	  for (i = TYPE_N_BASECLASSES (type); i < len; i++)
	    {
	      QUIT;
	      /* Don't print out virtual function table.  */
	      if (! strncmp (TYPE_FIELD_NAME (type, i),
			   "_vptr$", 6))
		continue;

	      print_spaces_filtered (level + 4, stream);
	      if (TYPE_FIELD_STATIC (type, i))
		{
		  fprintf_filtered (stream, "static ");
		}
	      type_print_1 (TYPE_FIELD_TYPE (type, i),
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

	  /* C++: print out the methods */
	  len = TYPE_NFN_FIELDS (type);
	  if (len) fprintf_filtered (stream, "\n");
	  for (i = 0; i < len; i++)
	    {
	      struct fn_field *f = TYPE_FN_FIELDLIST1 (type, i);
	      int j, len2 = TYPE_FN_FIELDLIST_LENGTH (type, i);

	      for (j = 0; j < len2; j++)
		{
		  QUIT;
		  print_spaces_filtered (level + 4, stream);
		  if (TYPE_FN_FIELD_VIRTUAL_P (f, j))
		    fprintf_filtered (stream, "virtual ");
		  else if (TYPE_FN_FIELD_STATIC_P (f, j))
		    fprintf_filtered (stream, "static ");
		  type_print (TYPE_TARGET_TYPE (TYPE_FN_FIELD_TYPE (f, j)), "", stream, 0);
		  if (TYPE_FN_FIELD_PHYSNAME (f, j)[0] == '_'
		      && TYPE_FN_FIELD_PHYSNAME (f, j)[1] == '$')
		    type_print_method_args
		      (TYPE_FN_FIELD_ARGS (f, j) + 1, "~",
		       TYPE_FN_FIELDLIST_NAME (type, i), 0, stream);
		  else
		    type_print_method_args
		      (TYPE_FN_FIELD_ARGS (f, j), "",
		       TYPE_FN_FIELDLIST_NAME (type, i),
		       TYPE_FN_FIELD_STATIC_P (f, j), stream);

		  fprintf_filtered (stream, ";\n");
		}
	      if (len2) fprintf_filtered (stream, "\n");
	    }

	  print_spaces_filtered (level, stream);
	  fprintf_filtered (stream, "}");
	}
      break;

    case TYPE_CODE_ENUM:
      fprintf_filtered (stream, "enum ");
      if (TYPE_NAME (type))
	{
	  name = TYPE_NAME (type);
	  while (*name != ' ') name++;
	  fputs_filtered (name + 1, stream);
	  fputs_filtered (" ", stream);
	}
      if (show < 0)
	fprintf_filtered (stream, "{...}");
      else
	{
	  fprintf_filtered (stream, "{");
	  len = TYPE_NFIELDS (type);
	  lastval = 0;
	  for (i = 0; i < len; i++)
	    {
	      QUIT;
	      if (i) fprintf_filtered (stream, ", ");
	      fputs_filtered (TYPE_FIELD_NAME (type, i), stream);
	      if (lastval != TYPE_FIELD_BITPOS (type, i))
		{
		  fprintf_filtered (stream, " : %d", TYPE_FIELD_BITPOS (type, i));
		  lastval = TYPE_FIELD_BITPOS (type, i);
		}
	      lastval++;
	    }
	  fprintf_filtered (stream, "}");
	}
      break;

    case TYPE_CODE_INT:
      if (TYPE_UNSIGNED (type))
	name = unsigned_type_table[TYPE_LENGTH (type)];
      else
	name = signed_type_table[TYPE_LENGTH (type)];
      fputs_filtered (name, stream);
      break;

    case TYPE_CODE_FLT:
      name = float_type_table[TYPE_LENGTH (type)];
      fputs_filtered (name, stream);
      break;

    case TYPE_CODE_VOID:
      fprintf_filtered (stream, "void");
      break;

    case 0:
      fprintf_filtered (stream, "struct unknown");
      break;

    default:
      error ("Invalid type code in symbol table.");
    }
}

static void
scalar_print_decimal(stream, type, val)
	FILE *stream;
	struct type *type;
	LONGEST val;
{
	fprintf_filtered(stream, TYPE_UNSIGNED(type)? "%lu":"%ld", val);
}

static void
scalar_print_hex(stream, type, val)
	FILE *stream;
	struct type *type;
	LONGEST val;
{
	switch (TYPE_LENGTH(type)) {
	case 1:
		fprintf_filtered (stream, "0x%02lx", val);
		break;
	case 2:
		fprintf_filtered (stream, "0x%04lx", val);
		break;
	case 4:
		fprintf_filtered (stream, "0x%08lx", val);
		break;
	default:
		fprintf_filtered (stream, "0x%lx", val);
		break;
	}
}

static void
scalar_print_octal(stream, type, val)
	FILE *stream;
	struct type *type;
	LONGEST val;
{
	switch (TYPE_LENGTH(type)) {
	case 1:
		fprintf_filtered (stream, "0%03lo", val);
		break;
	case 2:
		fprintf_filtered (stream, "0%06lo", val);
		break;
	case 4:
		fprintf_filtered (stream, "0%012lo", val);
		break;
	default:
		fprintf_filtered (stream, "0%lo", val);
		break;
	}
}

static void
scalar_print_hack(stream, type, val)
	FILE *stream;
	struct type *type;
	LONGEST val;
{
	if (TYPE_UNSIGNED(type))
		scalar_print_hex(stream, type, val);
	else
		scalar_print_decimal(stream, type, val);
}

static void
set_maximum_command (arg)
     char *arg;
{
  if (!arg) error_no_arg ("value for maximum elements to print");
  print_max = parse_and_eval_address (arg);
  if (print_max == 0)
    print_max = UINT_MAX;
}

static void
set_base_command(arg)
     char *arg;
{
	int base;

	if (!arg)
		base = 0;
	else
		base = parse_and_eval_address (arg);
	switch (base) {
	default:
		default_scalar_print = scalar_print_hack;
		break;
	case 8:
		default_scalar_print = scalar_print_octal;
		break;
	case 10:
		default_scalar_print = scalar_print_decimal;
		break;
	case 16:
		default_scalar_print = scalar_print_hex;
		break;
	}
}

static void
set_prettyprint_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  prettyprint = parse_binary_operation ("set prettyprint", arg);
}

static void
set_unionprint_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  unionprint = parse_binary_operation ("set unionprint", arg);
}

format_info (arg, from_tty)
     char *arg;
     int from_tty;
{
  if (arg)
    error ("\"info format\" does not take any arguments.");
  printf ("Prettyprinting of structures is %s.\n",
	  prettyprint ? "on" : "off");
  printf ("Printing of unions interior to structures is %s.\n",
	  unionprint ? "on" : "off");
  if (print_max == UINT_MAX)
    printf_filtered
      ("There is no maximum number of array elements printed.\n");
  else
    printf_filtered
      ("The maximum number of array elements printed is %d.\n", print_max);
}

extern struct cmd_list_element *setlist;

void
_initialize_valprint ()
{
  add_cmd ("base", class_support, set_base_command,
	   "Change default integer print radix to 8, 10 or 16\n\
No args returns to the ad-hoc default of `16' for unsigned values\n\
and `10' otherwise.",
	   &setlist);
  add_cmd ("array-max", class_vars, set_maximum_command,
	   "Set NUMBER as limit on string chars or array elements to print.\n\
\"set array-max 0\" causes there to be no limit.",
	   &setlist);

  add_cmd ("prettyprint", class_support, set_prettyprint_command,
	   "Turn prettyprinting of structures on and off.",
	   &setlist);
  add_alias_cmd ("pp", "prettyprint", class_support, 1, &setlist);

  add_cmd ("unionprint", class_support, set_unionprint_command,
	   "Turn printing of unions interior to structures on and off.",
	   &setlist);

  add_info ("format", format_info,
	    "Show current settings of data formatting options.");

  /* Give people the defaults which they are used to.  */
  prettyprint = 0;
  unionprint = 1;

  print_max = 200;

  unsigned_type_table
    = (char **) xmalloc ((1 + sizeof (unsigned LONGEST)) * sizeof (char *));
  bzero (unsigned_type_table, (1 + sizeof (unsigned LONGEST)));
  unsigned_type_table[sizeof (unsigned char)] = "unsigned char";
  unsigned_type_table[sizeof (unsigned short)] = "unsigned short";
  unsigned_type_table[sizeof (unsigned long)] = "unsigned long";
  unsigned_type_table[sizeof (unsigned int)] = "unsigned int";
#ifdef LONG_LONG
  unsigned_type_table[sizeof (unsigned long long)] = "unsigned long long";
#endif

  signed_type_table
    = (char **) xmalloc ((1 + sizeof (LONGEST)) * sizeof (char *));
  bzero (signed_type_table, (1 + sizeof (LONGEST)));
  signed_type_table[sizeof (char)] = "char";
  signed_type_table[sizeof (short)] = "short";
  signed_type_table[sizeof (long)] = "long";
  signed_type_table[sizeof (int)] = "int";
#ifdef LONG_LONG
  signed_type_table[sizeof (long long)] = "long long";
#endif

  float_type_table
    = (char **) xmalloc ((1 + sizeof (double)) * sizeof (char *));
  bzero (float_type_table, (1 + sizeof (double)));
  float_type_table[sizeof (float)] = "float";
  float_type_table[sizeof (double)] = "double";
}

