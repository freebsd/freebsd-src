/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)printcmd.c	6.5 (Berkeley) 5/8/91";
#endif /* not lint */

/* Print values for GNU debugger GDB.
   Copyright (C) 1986, 1987, 1988, 1989 Free Software Foundation, Inc.

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
#include "frame.h"
#include "symtab.h"
#include "value.h"
#include "expression.h"

struct format_data
{
  int count;
  char format;
  char size;
};

/* Last specified output format.  */

static char last_format = 'x';

/* Last specified examination size.  'b', 'h', 'w' or `q'.  */

static char last_size = 'w';

/* Default address to examine next.  */

static CORE_ADDR next_address;

/* Last address examined.  */

static CORE_ADDR last_examine_address;

/* Contents of last address examined.
   This is not valid past the end of the `x' command!  */

static value last_examine_value;

/* Number of auto-display expression currently being displayed.
   So that we can deleted it if we get an error or a signal within it.
   -1 when not doing one.  */

int current_display_number;

static void do_one_display ();

void do_displays ();
void print_address ();
void print_floating ();
void print_scalar_formatted ();
void print_formatted_address ();


/* Decode a format specification.  *STRING_PTR should point to it.
   OFORMAT and OSIZE are used as defaults for the format and size
   if none are given in the format specification.
   If OSIZE is zero, then the size field of the returned value
   should be set only if a size is explicitly specified by the
   user.
   The structure returned describes all the data
   found in the specification.  In addition, *STRING_PTR is advanced
   past the specification and past all whitespace following it.  */

struct format_data
decode_format (string_ptr, oformat, osize)
     char **string_ptr;
     char oformat;
     char osize;
{
  struct format_data val;
  register char *p = *string_ptr;

  val.format = '?';
  val.size = '?';
  val.count = 1;

  if (*p >= '0' && *p <= '9')
    val.count = atoi (p);
  while (*p >= '0' && *p <= '9') p++;

  /* Now process size or format letters that follow.  */

  while (1)
    {
      if (*p == 'b' || *p == 'h' || *p == 'w' || *p == 'g')
	val.size = *p++;
#ifdef LONG_LONG
      else if (*p == 'l')
	{
	  val.size = 'g';
	  p++;
	}
#endif
      else if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))
	val.format = *p++;
      else
	break;
    }

#ifndef LONG_LONG
  /* Make sure 'g' size is not used on integer types.
     Well, actually, we can handle hex.  */
  if (val.size == 'g' && val.format != 'f' && val.format != 'x')
    val.size = 'w';
#endif

  while (*p == ' ' || *p == '\t') p++;
  *string_ptr = p;

  /* Set defaults for format and size if not specified.  */
  if (val.format == '?')
    {
      if (val.size == '?')
	{
	  /* Neither has been specified.  */
	  val.format = oformat;
	  val.size = osize;
	}
      else
	/* If a size is specified, any format makes a reasonable
	   default except 'i'.  */
	val.format = oformat == 'i' ? 'x' : oformat;
    }
  else if (val.size == '?')
    switch (val.format)
      {
      case 'a':
      case 's':
      case 'A':
	/* Addresses must be words.  */
	val.size = osize ? 'w' : osize;
	break;
      case 'f':
	/* Floating point has to be word or giantword.  */
	if (osize == 'w' || osize == 'g')
	  val.size = osize;
	else
	  /* Default it to giantword if the last used size is not
	     appropriate.  */
	  val.size = osize ? 'g' : osize;
	break;
      case 'c':
	/* Characters default to one byte.  */
	val.size = osize ? 'b' : osize;
	break;
      default:
	/* The default is the size most recently specified.  */
	val.size = osize;
      }

  return val;
}

/* Print value VAL on stdout according to FORMAT, a letter or 0.
   Do not end with a newline.
   0 means print VAL according to its own type.
   SIZE is the letter for the size of datum being printed.
   This is used to pad hex numbers so they line up.  */

static void
print_formatted (val, format, size)
     register value val;
     register char format;
     char size;
{
  int len = TYPE_LENGTH (VALUE_TYPE (val));

  if (VALUE_LVAL (val) == lval_memory)
    next_address = VALUE_ADDRESS (val) + len;

  switch (format)
    {
    case 's':
      next_address = VALUE_ADDRESS (val)
	+ value_print (value_addr (val), stdout, 0, Val_pretty_default);
      break;

    case 'i':
      next_address = VALUE_ADDRESS (val)
	+ print_insn (VALUE_ADDRESS (val), stdout);
      break;

    default:
      if (format == 0
	  || TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_ARRAY
	  || TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_STRUCT
	  || TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_UNION
	  || VALUE_REPEATED (val))
	value_print (val, stdout, format, Val_pretty_default);
      else
	print_scalar_formatted (VALUE_CONTENTS (val), VALUE_TYPE (val),
				format, size, stdout);
    }
}

/* Print a scalar of data of type TYPE, pointed to in GDB by VALADDR,
   according to letters FORMAT and SIZE on STREAM.
   FORMAT may not be zero.  Formats s and i are not supported at this level.

   This is how the elements of an array or structure are printed
   with a format.  */

void
print_scalar_formatted (valaddr, type, format, size, stream)
     char *valaddr;
     struct type *type;
     char format;
     int size;
     FILE *stream;
{
  LONGEST val_long;
  int len = TYPE_LENGTH (type);

  if (size == 'g' && sizeof (LONGEST) < 8
      && format == 'x')
    {
      /* ok, we're going to have to get fancy here.  Assumption: a
         long is four bytes.  */
      unsigned long v1, v2, tmp;

      v1 = unpack_long (builtin_type_long, valaddr);
      v2 = unpack_long (builtin_type_long, valaddr + 4);

#ifdef BYTES_BIG_ENDIAN
#else
      /* Little endian -- swap the two for printing */
      tmp = v1;
      v1 = v2;
      v2 = tmp;
#endif
  
      switch (format)
	{
	case 'x':
	  fprintf_filtered (stream, "0x%08x%08x", v1, v2);
	  break;
	default:
	  error ("Output size \"g\" unimplemented for format \"%c\".",
		 format);
	}
      return;
    }
      
  val_long = unpack_long (type, valaddr);

  /* If value is unsigned, truncate it in case negative.  */
  if (format != 'd')
    {
      if (len == sizeof (char))
	val_long &= (1 << 8 * sizeof(char)) - 1;
      else if (len == sizeof (short))
	val_long &= (1 << 8 * sizeof(short)) - 1;
      else if (len == sizeof (long))
	val_long &= (unsigned long) - 1;
    }

  switch (format)
    {
    case 'x':
#ifdef LONG_LONG
      if (!size)
	size = (len < sizeof (long long) ? 'w' : 'g');
      switch (size)
	{
	case 'b':
	  fprintf_filtered (stream, "0x%02llx", val_long);
	  break;
	case 'h':
	  fprintf_filtered (stream, "0x%04llx", val_long);
	  break;
	case 0:		/* no size specified, like in print */
	case 'w':
	  fprintf_filtered (stream, "0x%08llx", val_long);
	  break;
	case 'g':
	  fprintf_filtered (stream, "0x%016llx", val_long);
	  break;
	default:
	  error ("Undefined output size \"%c\".", size);
	}
#else 
      switch (size)
	{
	case 'b':
	  fprintf_filtered (stream, "0x%02x", val_long);
	  break;
	case 'h':
	  fprintf_filtered (stream, "0x%04x", val_long);
	  break;
	case 0:		/* no size specified, like in print */
	case 'w':
	  fprintf_filtered (stream, "0x%08x", val_long);
	  break;
	case 'g':
	  fprintf_filtered (stream, "0x%o16x", val_long);
	  break;
	default:
	  error ("Undefined output size \"%c\".", size);
	}
#endif /* not LONG_LONG */
      break;

    case 'd':
#ifdef LONG_LONG
      fprintf_filtered (stream, "%lld", val_long);
#else
      fprintf_filtered (stream, "%d", val_long);
#endif
      break;

    case 'u':
#ifdef LONG_LONG
      fprintf_filtered (stream, "%llu", val_long);
#else
      fprintf_filtered (stream, "%u", val_long);
#endif
      break;

    case 'o':
      if (val_long)
#ifdef LONG_LONG
	fprintf_filtered (stream, "0%llo", val_long);
#else
	fprintf_filtered (stream, "0%o", val_long);
#endif
      else
	fprintf_filtered (stream, "0");
      break;

    case 'a':
      print_address ((CORE_ADDR) val_long, stream);
      break;

    case 'A':
      print_formatted_address ((CORE_ADDR) val_long, stream);
      break;

    case 'c':
      value_print (value_from_long (builtin_type_char, val_long), stream, 0,
		   Val_pretty_default);
      break;

    case 'f':
      if (len == sizeof (float))
	type = builtin_type_float;
      else if (len == sizeof (double))
	type = builtin_type_double;
      print_floating(valaddr, type, stream);
      break;

    case 0:
      abort ();

    default:
      error ("Undefined output format \"%c\".", format);
    }
}

/* Print a floating point value of type TYPE, pointed to in GDB by VALADDR,
   on STREAM.  */

void
print_floating(valaddr, type, stream)
     char *valaddr;
     struct type *type;
     FILE *stream;
{
  double doub;
  int inv;
  int len = TYPE_LENGTH (type);
  
  doub = unpack_double (type, valaddr, &inv);
  if (inv)
    fprintf_filtered (stream, "Invalid float value");
  else if (doub != doub)
    {
      /* Surely it is an IEEE floating point NaN. */

      long low, high, *arg = (long *)valaddr;	/* ASSUMED 32 BITS */
      int nonneg;

      if (len <= sizeof(float))
	{
	  /* It's single precision. */
	  low = *arg;
	  nonneg  =  low >= 0;
	  low &= 0x7fffff;
	  high = 0;
	}
      else
	{
	  /* It's double precision.
	     Get the high and low words of the fraction.
	     Distinguish big and little-endian machines.  */
#ifdef WORDS_BIG_ENDIAN
	  low = arg[1], high = arg[0];
#else
	  low = arg[0], high = arg[1];
#endif
	  nonneg  =  high >= 0;
	  high &= 0xfffff;
	}
      if (high)
	fprintf_filtered (stream, "-NaN(0x%lx%.8lx)" + nonneg, high, low);
      else
	fprintf_filtered (stream, "-NaN(0x%lx)" + nonneg, low);
    }
  else
    fprintf_filtered (stream, len <= sizeof(float) ? "%.6g" : "%.17g", doub);
}

/* Specify default address for `x' command.
   `info lines' uses this.  */

void
set_next_address (addr)
     CORE_ADDR addr;
{
  next_address = addr;

  /* Make address available to the user as $_.  */
  set_internalvar (lookup_internalvar ("_"),
		   value_from_long (builtin_type_int, (LONGEST) addr));
}

/* Optionally print address ADDR symbolically as <SYMBOL+OFFSET> on STREAM. */

void
print_address_symbolic (addr, stream)
     CORE_ADDR addr;
     FILE *stream;
{
  register char *format;
  int name_location;
  register int i = find_pc_misc_function (addr);

  /* If nothing comes out, don't print anything symbolic.  */
  if (i < 0) return;
  name_location = misc_function_vector[i].address;

  if (addr - name_location)
    format = " <%s+%d>";
  else
    format = " <%s>";

  fprintf_filtered (stream, format,
		    misc_function_vector[i].name, addr - name_location);
}

/* Print address ADDR symbolically on STREAM.
   First print it as a number.  Then perhaps print
   <SYMBOL + OFFSET> after the number.  */

void
print_address (addr, stream)
     CORE_ADDR addr;
     FILE *stream;
{
  fprintf_filtered (stream, "0x%x", addr);
  print_address_symbolic (addr, stream);
}

/* Like print_address but opnly prints symbolically. */

void
print_formatted_address (addr, stream)
     CORE_ADDR addr;
     FILE *stream;
{
  register int i = 0;
  register char *format;
  register struct symbol *fs;
  char *name;
  int name_location;

  i = find_pc_partial_function (addr, &name, &name_location);

  /* If nothing comes out, don't print anything symbolic.  */
  
  if (i == 0)
    fprintf_filtered (stream, "0x%x", addr);
  else if (addr - name_location)
    fprintf_filtered (stream, "%s+%d", name, addr - name_location);
  else
    fprintf_filtered (stream, "%s", name);
}

/* Examine data at address ADDR in format FMT.
   Fetch it from memory and print on stdout.  */

static void
do_examine (fmt, addr)
     struct format_data fmt;
     CORE_ADDR addr;
{
  register char format = 0;
  register char size;
  register int count = 1;
  struct type *val_type;
  register int i;
  register int maxelts;

  format = fmt.format;
  size = fmt.size;
  count = fmt.count;
  next_address = addr;

  /* String or instruction format implies fetch single bytes
     regardless of the specified size.  */
  if (format == 's' || format == 'i')
    size = 'b';

  if (size == 'b')
    val_type = builtin_type_char;
  else if (size == 'h')
    val_type = builtin_type_short;
  else if (size == 'w')
    val_type = builtin_type_long;
  else if (size == 'g')
#ifndef LONG_LONG
    val_type = builtin_type_double;
#else
    val_type = builtin_type_long_long;
#endif

  maxelts = 8;
  if (size == 'w')
    maxelts = 4;
  if (size == 'g')
    maxelts = 2;
  if (format == 's' || format == 'i')
    maxelts = 1;

  /* Print as many objects as specified in COUNT, at most maxelts per line,
     with the address of the next one at the start of each line.  */

  while (count > 0)
    {
      print_address (next_address, stdout);
      printf_filtered (":");
      for (i = maxelts;
	   i > 0 && count > 0;
	   i--, count--)
	{
	  printf_filtered ("\t");
	  /* Note that print_formatted sets next_address for the next
	     object.  */
	  last_examine_address = next_address;
	  last_examine_value = value_at (val_type, next_address);
	  print_formatted (last_examine_value, format, size);
	}
      printf_filtered ("\n");
      fflush (stdout);
    }
}

static void
validate_format (fmt, cmdname)
     struct format_data fmt;
     char *cmdname;
{
  if (fmt.size != 0)
    error ("Size letters are meaningless in \"%s\" command.", cmdname);
  if (fmt.count != 1)
    error ("Item count other than 1 is meaningless in \"%s\" command.",
	   cmdname);
  if (fmt.format == 'i' || fmt.format == 's')
    error ("Format letter \"%c\" is meaningless in \"%s\" command.",
	   fmt.format, cmdname);
}

static void
print_command (exp)
     char *exp;
{
  struct expression *expr;
  register struct cleanup *old_chain = 0;
  register char format = 0;
  register value val;
  struct format_data fmt;
  int histindex;
  int cleanup = 0;

  if (exp && *exp == '/')
    {
      exp++;
      fmt = decode_format (&exp, last_format, 0);
      validate_format (fmt, "print");
      last_format = format = fmt.format;
    }

  if (exp && *exp)
    {
      expr = parse_c_expression (exp);
      old_chain = make_cleanup (free_current_contents, &expr);
      cleanup = 1;
      val = evaluate_expression (expr);
    }
  else
    val = access_value_history (0);

  histindex = record_latest_value (val);
  if (histindex >= 0) printf_filtered ("$%d = ", histindex);

  print_formatted (val, format, fmt.size);
  printf_filtered ("\n");

  if (cleanup)
    do_cleanups (old_chain);
}

static void
output_command (exp)
     char *exp;
{
  struct expression *expr;
  register struct cleanup *old_chain;
  register char format = 0;
  register value val;
  struct format_data fmt;

  if (exp && *exp == '/')
    {
      exp++;
      fmt = decode_format (&exp, 0, 0);
      validate_format (fmt, "print");
      format = fmt.format;
    }

  expr = parse_c_expression (exp);
  old_chain = make_cleanup (free_current_contents, &expr);

  val = evaluate_expression (expr);

  print_formatted (val, format, fmt.size);

  do_cleanups (old_chain);
}

static void
set_command (exp)
     char *exp;
{
  struct expression *expr = parse_c_expression (exp);
  register struct cleanup *old_chain
    = make_cleanup (free_current_contents, &expr);
  evaluate_expression (expr);
  do_cleanups (old_chain);
}

static void
address_info (exp)
     char *exp;
{
  register struct symbol *sym;
  register CORE_ADDR val;
  int is_a_field_of_this;	/* C++: lookup_symbol sets this to nonzero
				   if exp is a field of `this'. */

  if (exp == 0)
    error ("Argument required.");

  sym = lookup_symbol (exp, get_selected_block (), VAR_NAMESPACE, 
		       &is_a_field_of_this);
  if (sym == 0)
    {
      register int i;

      if (is_a_field_of_this)
	{
	  printf ("Symbol \"%s\" is a field of the local class variable `this'\n", exp);
	  return;
	}

      for (i = 0; i < misc_function_count; i++)
	if (!strcmp (misc_function_vector[i].name, exp))
	  break;

      if (i < misc_function_count)
	printf ("Symbol \"%s\" is at 0x%x in a file compiled without -g.\n",
		exp, misc_function_vector[i].address);
      else
	error ("No symbol \"%s\" in current context.", exp);
      return;
    }

  printf ("Symbol \"%s\" is ", SYMBOL_NAME (sym));
  val = SYMBOL_VALUE (sym);

  switch (SYMBOL_CLASS (sym))
    {
    case LOC_CONST:
    case LOC_CONST_BYTES:
      printf ("constant");
      break;

    case LOC_LABEL:
      printf ("a label at address 0x%x", val);
      break;

    case LOC_REGISTER:
      printf ("a variable in register %s", reg_names[val]);
      break;

    case LOC_STATIC:
      printf ("static at address 0x%x", val);
      break;

    case LOC_REGPARM:
      printf ("an argument in register %s", reg_names[val]);
      break;
      
    case LOC_ARG:
      printf ("an argument at offset %d", val);
      break;

    case LOC_LOCAL:
      printf ("a local variable at frame offset %d", val);
      break;

    case LOC_REF_ARG:
      printf ("a reference argument at offset %d", val);
      break;

    case LOC_TYPEDEF:
      printf ("a typedef");
      break;

    case LOC_BLOCK:
      printf ("a function at address 0x%x",
	      BLOCK_START (SYMBOL_BLOCK_VALUE (sym)));
      break;
    }
  printf (".\n");
}

static void
x_command (exp, from_tty)
     char *exp;
     int from_tty;
{
  struct expression *expr;
  struct format_data fmt;
  struct cleanup *old_chain;
  struct value *val;

  fmt.format = last_format;
  fmt.size = last_size;
  fmt.count = 1;

  if (exp && *exp == '/')
    {
      exp++;
      fmt = decode_format (&exp, last_format, last_size);
      last_size = fmt.size;
      last_format = fmt.format;
    }

  /* If we have an expression, evaluate it and use it as the address.  */

  if (exp != 0 && *exp != 0)
    {
      expr = parse_c_expression (exp);
      /* Cause expression not to be there any more
	 if this command is repeated with Newline.
	 But don't clobber a user-defined command's definition.  */
      if (from_tty)
	*exp = 0;
      old_chain = make_cleanup (free_current_contents, &expr);
      val = evaluate_expression (expr);
      /* In rvalue contexts, such as this, functions are coerced into
	 pointers to functions.  This makes "x/i main" work.  */
      if (/* last_format == 'i'
	  && */ TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_FUNC
	  && VALUE_LVAL (val) == lval_memory)
	next_address = VALUE_ADDRESS (val);
      else
	next_address = (CORE_ADDR) value_as_long (val);
      do_cleanups (old_chain);
    }

  do_examine (fmt, next_address);

  /* Set a couple of internal variables if appropriate. */
  if (last_examine_value)
    {
      /* Make last address examined available to the user as $_.  */
      set_internalvar (lookup_internalvar ("_"),
		       value_from_long (builtin_type_int, 
					(LONGEST) last_examine_address));
      
      /* Make contents of last address examined available to the user as $__.*/
      set_internalvar (lookup_internalvar ("__"), last_examine_value);
    }
}

/* Commands for printing types of things.  */

static void
whatis_command (exp)
     char *exp;
{
  struct expression *expr;
  register value val;
  register struct cleanup *old_chain;

  if (exp)
    {
      expr = parse_c_expression (exp);
      old_chain = make_cleanup (free_current_contents, &expr);
      val = evaluate_type (expr);
    }
  else
    val = access_value_history (0);

  printf_filtered ("type = ");
  /* Most of the time users do not want to see all the fields
     in a structure.  If they do they can use the "ptype" command.
     Hence the "-1" below.  */
  type_print (VALUE_TYPE (val), "", stdout, -1);
  printf_filtered ("\n");

  if (exp)
    do_cleanups (old_chain);
}

static void
ptype_command (typename)
     char *typename;
{
  register char *p = typename;
  register int len;
  extern struct block *get_current_block ();
  register struct block *b
    = (have_inferior_p () || have_core_file_p ()) ? get_current_block () : 0;
  register struct type *type;

  if (typename == 0)
    error_no_arg ("type name");

  while (*p && *p != ' ' && *p != '\t') p++;
  len = p - typename;
  while (*p == ' ' || *p == '\t') p++;

  if (len == 6 && !strncmp (typename, "struct", 6))
    type = lookup_struct (p, b);
  else if (len == 5 && !strncmp (typename, "union", 5))
    type = lookup_union (p, b);
  else if (len == 4 && !strncmp (typename, "enum", 4))
    type = lookup_enum (p, b);
  else
    {
      type = lookup_typename (typename, b, 1);
      if (type == 0)
	{
	  register struct symbol *sym
	    = lookup_symbol (typename, b, STRUCT_NAMESPACE, 0);
	  if (sym == 0)
	    error ("No type named %s.", typename);
	  printf_filtered ("No type named %s, but there is a ",
		  typename);
	  switch (TYPE_CODE (SYMBOL_TYPE (sym)))
	    {
	    case TYPE_CODE_STRUCT:
	      printf_filtered ("struct");
	      break;

	    case TYPE_CODE_UNION:
	      printf_filtered ("union");
	      break;

	    case TYPE_CODE_ENUM:
	      printf_filtered ("enum");
	    }
	  printf_filtered (" %s.  Type \"help ptype\".\n", typename);
	  type = SYMBOL_TYPE (sym);
	}
    }

  type_print (type, "", stdout, 1);
  printf_filtered ("\n");
}

enum display_status {disabled, enabled};

struct display
{
  /* Chain link to next auto-display item.  */
  struct display *next;
  /* Expression to be evaluated and displayed.  */
  struct expression *exp;
  /* Item number of this auto-display item.  */
  int number;
  /* Display format specified.  */
  struct format_data format;
  /* Innermost block required by this expression when evaluated */
  struct block *block;
  /* Status of this display (enabled or disabled) */
  enum display_status status;
};

/* Chain of expressions whose values should be displayed
   automatically each time the program stops.  */

static struct display *display_chain;

static int display_number;

/* Add an expression to the auto-display chain.
   Specify the expression.  */

static void
display_command (exp, from_tty)
     char *exp;
     int from_tty;
{
  struct format_data fmt;
  register struct expression *expr;
  register struct display *new;
  extern struct block *innermost_block;

  if (exp == 0)
    {
      do_displays ();
      return;
    }

  if (*exp == '/')
    {
      exp++;
      fmt = decode_format (&exp, 0, 0);
      if (fmt.size && fmt.format == 0)
	fmt.format = 'x';
      if (fmt.format == 'i' || fmt.format == 's')
	fmt.size = 'b';
    }
  else
    {
      fmt.format = 0;
      fmt.size = 0;
      fmt.count = 0;
    }

  innermost_block = 0;
  expr = parse_c_expression (exp);

  new = (struct display *) xmalloc (sizeof (struct display));

  new->exp = expr;
  new->block = innermost_block;
  new->next = display_chain;
  new->number = ++display_number;
  new->format = fmt;
  new->status = enabled;
  display_chain = new;

  if (from_tty && have_inferior_p ())
    do_one_display (new);

  dont_repeat ();
}

static void
free_display (d)
     struct display *d;
{
  free (d->exp);
  free (d);
}

/* Clear out the display_chain.
   Done when new symtabs are loaded, since this invalidates
   the types stored in many expressions.  */

void
clear_displays ()
{
  register struct display *d;

  while (d = display_chain)
    {
      free (d->exp);
      display_chain = d->next;
      free (d);
    }
}

/* Delete the auto-display number NUM.  */

void
delete_display (num)
     int num;
{
  register struct display *d1, *d;

  if (!display_chain)
    error ("No display number %d.", num);

  if (display_chain->number == num)
    {
      d1 = display_chain;
      display_chain = d1->next;
      free_display (d1);
    }
  else
    for (d = display_chain; ; d = d->next)
      {
	if (d->next == 0)
	  error ("No display number %d.", num);
	if (d->next->number == num)
	  {
	    d1 = d->next;
	    d->next = d1->next;
	    free_display (d1);
	    break;
	  }
      }
}

/* Delete some values from the auto-display chain.
   Specify the element numbers.  */

static void
undisplay_command (args)
     char *args;
{
  register char *p = args;
  register char *p1;
  register int num;
  register struct display *d, *d1;

  if (args == 0)
    {
      if (query ("Delete all auto-display expressions? "))
	clear_displays ();
      dont_repeat ();
      return;
    }

  while (*p)
    {
      p1 = p;
      while (*p1 >= '0' && *p1 <= '9') p1++;
      if (*p1 && *p1 != ' ' && *p1 != '\t')
	error ("Arguments must be display numbers.");

      num = atoi (p);

      delete_display (num);

      p = p1;
      while (*p == ' ' || *p == '\t') p++;
    }
  dont_repeat ();
}

/* Display a single auto-display.  
   Do nothing if the display cannot be printed in the current context,
   or if the display is disabled. */

static void
do_one_display (d)
     struct display *d;
{
  int within_current_scope;

  if (d->status == disabled)
    return;

  if (d->block)
    within_current_scope = contained_in (get_selected_block (), d->block);
  else
    within_current_scope = 1;
  if (!within_current_scope)
    return;

  current_display_number = d->number;

  printf_filtered ("%d: ", d->number);
  if (d->format.size)
    {
      printf_filtered ("x/");
      if (d->format.count != 1)
	printf_filtered ("%d", d->format.count);
      printf_filtered ("%c", d->format.format);
      if (d->format.format != 'i' && d->format.format != 's')
	printf_filtered ("%c", d->format.size);
      printf_filtered (" ");
      print_expression (d->exp, stdout);
      if (d->format.count != 1)
	printf_filtered ("\n");
      else
	printf_filtered ("  ");
      do_examine (d->format,
		  (CORE_ADDR) value_as_long (evaluate_expression (d->exp)));

    }
  else
    {
      if (d->format.format)
	printf_filtered ("/%c ", d->format.format);
      print_expression (d->exp, stdout);
      printf_filtered (" = ");
      print_formatted (evaluate_expression (d->exp),
		       d->format.format, d->format.size);
      printf_filtered ("\n");
    }

  fflush (stdout);
  current_display_number = -1;
}

/* Display all of the values on the auto-display chain which can be
   evaluated in the current scope.  */

void
do_displays ()
{
  register struct display *d;

  for (d = display_chain; d; d = d->next)
    do_one_display (d);
}

/* Delete the auto-display which we were in the process of displaying.
   This is done when there is an error or a signal.  */

void
disable_display (num)
     int num;
{
  register struct display *d;

  for (d = display_chain; d; d = d->next)
    if (d->number == num)
      {
	d->status = disabled;
	return;
      }
  printf ("No display number %d.\n", num);
}
  
void
disable_current_display ()
{
  if (current_display_number >= 0)
    {
      disable_display (current_display_number);
      fprintf (stderr, "Disabling display %d to avoid infinite recursion.\n",
	       current_display_number);
    }
  current_display_number = -1;
}

static void
display_info ()
{
  register struct display *d;

  if (!display_chain)
    printf ("There are no auto-display expressions now.\n");
  else
      printf_filtered ("Auto-display expressions now in effect:\n\
Num Enb Expression\n");

  for (d = display_chain; d; d = d->next)
    {
      printf_filtered ("%d:   %c  ", d->number, "ny"[(int)d->status]);
      if (d->format.size)
	printf_filtered ("/%d%c%c ", d->format.count, d->format.size,
		d->format.format);
      else if (d->format.format)
	printf_filtered ("/%c ", d->format.format);
      print_expression (d->exp, stdout);
      if (d->block && !contained_in (get_selected_block (), d->block))
	printf_filtered (" (cannot be evaluated in the current context)");
      printf_filtered ("\n");
      fflush (stdout);
    }
}

void
enable_display (args)
     char *args;
{
  register char *p = args;
  register char *p1;
  register int num;
  register struct display *d;

  if (p == 0)
    {
      for (d = display_chain; d; d = d->next)
	d->status = enabled;
    }
  else
    while (*p)
      {
	p1 = p;
	while (*p1 >= '0' && *p1 <= '9')
	  p1++;
	if (*p1 && *p1 != ' ' && *p1 != '\t')
	  error ("Arguments must be display numbers.");
	
	num = atoi (p);
	
	for (d = display_chain; d; d = d->next)
	  if (d->number == num)
	    {
	      d->status = enabled;
	      goto win;
	    }
	printf ("No display number %d.\n", num);
      win:
	p = p1;
	while (*p == ' ' || *p == '\t')
	  p++;
      }
}

void
disable_display_command (args, from_tty)
     char *args;
     int from_tty;
{
  register char *p = args;
  register char *p1;
  register int num;
  register struct display *d;

  if (p == 0)
    {
      for (d = display_chain; d; d = d->next)
	d->status = disabled;
    }
  else
    while (*p)
      {
	p1 = p;
	while (*p1 >= '0' && *p1 <= '9')
	  p1++;
	if (*p1 && *p1 != ' ' && *p1 != '\t')
	  error ("Arguments must be display numbers.");
	
	num = atoi (p);
	
	disable_display (atoi (p));

	p = p1;
	while (*p == ' ' || *p == '\t')
	  p++;
      }
}


/* Print the value in stack frame FRAME of a variable
   specified by a struct symbol.  */

void
print_variable_value (var, frame, stream)
     struct symbol *var;
     FRAME frame;
     FILE *stream;
{
  value val = read_var_value (var, frame);
  value_print (val, stream, 0, Val_pretty_default);
}

static int
compare_ints (i, j)
     int *i, *j;
{
  return *i - *j;
}

/* Print the arguments of a stack frame, given the function FUNC
   running in that frame (as a symbol), the info on the frame,
   and the number of args according to the stack frame (or -1 if unknown).  */

static void print_frame_nameless_args ();

void
print_frame_args (func, fi, num, stream)
     struct symbol *func;
     struct frame_info *fi;
     int num;
     FILE *stream;
{
  struct block *b;
  int nsyms = 0;
  int first = 1;
  register int i;
  register int last_regparm = 0;
  register struct symbol *lastsym, *sym, *nextsym;
  register value val;
  /* Offset of stack argument that is at the highest offset.
     -1 if we haven't come to a stack argument yet.  */
  CORE_ADDR highest_offset = (CORE_ADDR) -1;
  register CORE_ADDR addr = FRAME_ARGS_ADDRESS (fi);

  if (func)
    {
      b = SYMBOL_BLOCK_VALUE (func);
      nsyms = BLOCK_NSYMS (b);
    }

  for (i = 0; i < nsyms; i++)
    {
      QUIT;
      sym = BLOCK_SYM (b, i);

      if (SYMBOL_CLASS (sym) != LOC_REGPARM
	  && SYMBOL_CLASS (sym) != LOC_ARG
	  && SYMBOL_CLASS (sym) != LOC_REF_ARG)
	continue;

      /* Print the next arg.  */
      if (SYMBOL_CLASS (sym) == LOC_REGPARM)
	val = value_from_register (SYMBOL_TYPE (sym),
				   SYMBOL_VALUE (sym),
				   FRAME_INFO_ID (fi));
      else
	{
	  int current_offset = SYMBOL_VALUE (sym);
	  int arg_size = TYPE_LENGTH (SYMBOL_TYPE (sym));
	  
	  if (SYMBOL_CLASS (sym) == LOC_REF_ARG)
	    val = value_at (SYMBOL_TYPE (sym),
			    read_memory_integer (addr + current_offset,
						 sizeof (CORE_ADDR)));
	  else
	    val = value_at (SYMBOL_TYPE (sym), addr + current_offset);

	  /* Round up address of next arg to multiple of size of int.  */
	  current_offset
	    = (((current_offset + sizeof (int) - 1) / sizeof (int))
	       * sizeof (int));

	  /* If this is the highest offset seen yet, set highest_offset.  */
	  if (highest_offset == (CORE_ADDR)-1
	      || ((current_offset
		   + (arg_size - sizeof (int) + 3) / (sizeof (int)))
		  > highest_offset))
	    highest_offset = current_offset;
	}

      if (! first)
	fprintf_filtered (stream, ", ");
      fputs_filtered (SYMBOL_NAME (sym), stream);
      fputs_filtered ("=", stream);

/* Nonzero if a LOC_ARG which is a struct is useless.  */
#if !defined (STRUCT_ARG_SYM_GARBAGE)
#define STRUCT_ARG_SYM_GARBAGE(gcc_p) 0
#endif

      if (STRUCT_ARG_SYM_GARBAGE (b->gcc_compile_flag)
	  && TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_STRUCT
	  && SYMBOL_CLASS (sym) == LOC_ARG)
	{
	  /* Try looking up that name.  SunOS4 puts out a usable
	     symbol as a local variable (in addition to the one
	     for the arg).  */
	  struct symbol *sym2 =
	    lookup_symbol (SYMBOL_NAME (sym), b, VAR_NAMESPACE, 0);

	  if (sym2 != NULL)
	    val = value_of_variable (sym2);
	  else
	    {
	      fputs_filtered ("?", stream);
	      first = 0;
	      continue;
	    }
	}

      value_print (val, stream, 0, Val_no_prettyprint);
      first = 0;
    }

  /* Don't print nameless args in situations where we don't know
     enough about the stack to find them.  */
  if (num != -1)
    {
      if (highest_offset != (CORE_ADDR) -1
	  && num * sizeof (int) + FRAME_ARGS_SKIP > highest_offset)
	print_frame_nameless_args (fi, addr,
				   highest_offset + sizeof (int),
				   num * sizeof (int) + FRAME_ARGS_SKIP,
				   stream);
      else 
	print_frame_nameless_args (fi, addr, FRAME_ARGS_SKIP,
				   num * sizeof (int) + FRAME_ARGS_SKIP,
				   stream);
    }
}

static void
print_frame_nameless_args (fi, argsaddr, start, end, stream)
     struct frame_info *fi;
     CORE_ADDR argsaddr;
     int start;
     int end;
     FILE *stream;
{
	extern void (*default_scalar_print)();
	LONGEST v;
	int p = start;
	char *s = "";

	for (p = start; p < end; p += sizeof(int)) {
		QUIT;
#if defined(NAMELESS_ARG)
		v = NAMELESS_ARG(fi, (p - start) / sizeof(int));
#else
		v = read_memory_integer (argsaddr + p, sizeof (int));
#endif
		fprintf_filtered (stream, s);
		s = ", ";
		(*default_scalar_print) (stream, builtin_type_int, v);
	}
}

static void
printf_command (arg)
     char *arg;
{
  register char *f;
  register char *s = arg;
  char *string;
  value *val_args;
  int nargs = 0;
  int allocated_args = 20;
  char *arg_bytes;

  val_args = (value *) xmalloc (allocated_args * sizeof (value));

  if (s == 0)
    error_no_arg ("format-control string and values to print");

  /* Skip white space before format string */
  while (*s == ' ' || *s == '\t') s++;

  /* A format string should follow, enveloped in double quotes */
  if (*s++ != '"')
    error ("Bad format string, missing '\"'.");

  /* Parse the format-control string and copy it into the string STRING,
     processing some kinds of escape sequence.  */

  f = string = (char *) alloca (strlen (s) + 1);
  while (*s != '"')
    {
      int c = *s++;
      switch (c)
	{
	case '\0':
	  error ("Bad format string, non-terminated '\"'.");
	  /* doesn't return */

	case '\\':
	  switch (c = *s++)
	    {
	    case '\\':
	      *f++ = '\\';
	      break;
	    case 'n':
	      *f++ = '\n';
	      break;
	    case 't':
	      *f++ = '\t';
	      break;
	    case 'r':
	      *f++ = '\r';
	      break;
	    case '"':
	      *f++ = '"';
	      break;
	    default:
	      /* ??? TODO: handle other escape sequences */
	      error ("Unrecognized \\ escape character in format string.");
	    }
	  break;

	default:
	  *f++ = c;
	}
    }

  /* Skip over " and following space and comma.  */
  s++;
  *f++ = '\0';
  while (*s == ' ' || *s == '\t') s++;

  if (*s != ',' && *s != 0)
    error ("Invalid argument syntax");

  if (*s == ',') s++;
  while (*s == ' ' || *s == '\t') s++;

  {
    /* Now scan the string for %-specs and see what kinds of args they want.
       argclass[I] classifies the %-specs so we can give vprintf something
       of the right size.  */
 
    enum argclass {int_arg, string_arg, double_arg, long_long_arg};
    enum argclass *argclass;
    int nargs_wanted;
    int argindex;
    int lcount;
    int i;
 
    argclass = (enum argclass *) alloca (strlen (s) * sizeof *argclass);
    nargs_wanted = 0;
    f = string;
    while (*f)
      if (*f++ == '%')
	{
	  lcount = 0;
	  while (index ("0123456789.hlL-+ #", *f)) 
	    {
	      if (*f == 'l' || *f == 'L')
		lcount++;
	      f++;
	    }
	  if (*f == 's')
	    argclass[nargs_wanted++] = string_arg;
	  else if (*f == 'e' || *f == 'f' || *f == 'g')
	    argclass[nargs_wanted++] = double_arg;
	  else if (lcount > 1)
	    argclass[nargs_wanted++] = long_long_arg;
	  else if (*f != '%')
	    argclass[nargs_wanted++] = int_arg;
	  f++;
	}
 
    /* Now, parse all arguments and evaluate them.
       Store the VALUEs in VAL_ARGS.  */
 
    while (*s != '\0')
      {
	char *s1;
	if (nargs == allocated_args)
	  val_args = (value *) xrealloc (val_args,
					 (allocated_args *= 2)
					 * sizeof (value));
	s1 = s;
	val_args[nargs] = parse_to_comma_and_eval (&s1);
 
	/* If format string wants a float, unchecked-convert the value to
	   floating point of the same size */
 
	if (argclass[nargs] == double_arg)
	  {
	    if (TYPE_LENGTH (VALUE_TYPE (val_args[nargs])) == sizeof (float))
	      VALUE_TYPE (val_args[nargs]) = builtin_type_float;
	    if (TYPE_LENGTH (VALUE_TYPE (val_args[nargs])) == sizeof (double))
	      VALUE_TYPE (val_args[nargs]) = builtin_type_double;
	  }
	nargs++;
	s = s1;
	if (*s == ',')
	  s++;
      }
 
    if (nargs != nargs_wanted)
      error ("Wrong number of arguments for specified format-string");
 
    /* Now lay out an argument-list containing the arguments
       as doubles, integers and C pointers.  */
 
    arg_bytes = (char *) alloca (sizeof (double) * nargs);
    argindex = 0;
    for (i = 0; i < nargs; i++)
      {
	if (argclass[i] == string_arg)
	  {
	    char *str;
	    int tem, j;
	    tem = value_as_long (val_args[i]);
 
	    /* This is a %s argument.  Find the length of the string.  */
	    for (j = 0; ; j++)
	      {
		char c;
		QUIT;
		read_memory (tem + j, &c, 1);
		if (c == 0)
		  break;
	      }
 
	    /* Copy the string contents into a string inside GDB.  */
	    str = (char *) alloca (j + 1);
	    read_memory (tem, str, j);
	    str[j] = 0;
 
	    /* Pass address of internal copy as the arg to vprintf.  */
	    *((int *) &arg_bytes[argindex]) = (int) str;
	    argindex += sizeof (int);
	  }
	else if (VALUE_TYPE (val_args[i])->code == TYPE_CODE_FLT)
	  {
	    *((double *) &arg_bytes[argindex]) = value_as_double (val_args[i]);
	    argindex += sizeof (double);
	  }
	else
#ifdef LONG_LONG
	  if (argclass[i] == long_long_arg)
	    {
	      *(long long *) &arg_bytes[argindex] = value_as_long (val_args[i]);
	      argindex += sizeof (long long);
	    }
	  else
#endif
	    {
	      *((int *) &arg_bytes[argindex]) = value_as_long (val_args[i]);
	      argindex += sizeof (int);
	    }
      }
  }
  vprintf (string, arg_bytes);
}

/* Helper function for asdump_command.  Finds the bounds of a function
   for a specified section of text.  PC is an address within the
   function which you want bounds for; *LOW and *HIGH are set to the
   beginning (inclusive) and end (exclusive) of the function.  This
   function returns 1 on success and 0 on failure.  */

static int
containing_function_bounds (pc, low, high)
     CORE_ADDR pc, *low, *high;
{
  int scan;

  if (!find_pc_partial_function (pc, 0, low))
    return 0;

  scan = *low;
  do {
    scan++;
    if (!find_pc_partial_function (scan, 0, high))
      return 0;
  } while (*low == *high);

  return 1;
}

/* Dump a specified section of assembly code.  With no command line
   arguments, this command will dump the assembly code for the
   function surrounding the pc value in the selected frame.  With one
   argument, it will dump the assembly code surrounding that pc value.
   Two arguments are interpeted as bounds within which to dump
   assembly.  */

static void
disassemble_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  CORE_ADDR low, high;
  CORE_ADDR pc;
  char *space_index;

  if (!arg)
    {
      if (!selected_frame)
	error ("No frame selected.\n");

      pc = get_frame_pc (selected_frame);
      if (!containing_function_bounds (pc, &low, &high))
	error ("No function contains pc specified by selected frame.\n");
    }
  else if (!(space_index = (char *) index (arg, ' ')))
    {
      /* One argument.  */
      pc = parse_and_eval_address (arg);
      if (!containing_function_bounds (pc, &low, &high))
	error ("No function contains specified pc.\n");
    }
  else
    {
      /* Two arguments.  */
      *space_index = '\0';
      low = parse_and_eval_address (arg);
      high = parse_and_eval_address (space_index + 1);
    }

  printf_filtered ("Dump of assembler code ");
  if (!space_index)
    {
      char *name;
      find_pc_partial_function (pc, &name, 0);
      printf_filtered ("for function %s:\n", name);
    }
  else
    printf_filtered ("from 0x%x to 0x%x:\n", low, high);

  /* Dump the specified range.  */
  for (pc = low; pc < high; )
    {
      QUIT;
      print_address (pc, stdout);
      printf_filtered (":\t");
      pc += print_insn (pc, stdout);
      printf_filtered ("\n");
    }
  printf_filtered ("End of assembler dump.\n");
  fflush (stdout);
}


extern struct cmd_list_element *enablelist, *disablelist, *deletelist;
extern struct cmd_list_element *cmdlist, *setlist;

void
_initialize_printcmd ()
{
  current_display_number = -1;

  add_info ("address", address_info,
	   "Describe where variable VAR is stored.");

  add_com ("x", class_vars, x_command,
	   "Examine memory: x/FMT ADDRESS.\n\
ADDRESS is an expression for the memory address to examine.\n\
FMT is a repeat count followed by a format letter and a size letter.\n\
Format letters are o(octal), x(hex), d(decimal), u(unsigned decimal),\n\
 f(float), a(address), i(instruction), c(char) and s(string).\n\
Size letters are b(byte), h(halfword), w(word), g(giant, 8 bytes).\n\
  g is meaningful only with f, for type double.\n\
The specified number of objects of the specified size are printed\n\
according to the format.\n\n\
Defaults for format and size letters are those previously used.\n\
Default count is 1.  Default address is following last thing printed\n\
with this command or \"print\".");

  add_com ("disassemble", class_vars, disassemble_command,
	   "Disassemble a specified section of memory.\n\
Default is the function surrounding the pc of the selected frame.\n\
With a single argument, the function surrounding that address is dumped.\n\
Two arguments are taken as a range of memory to dump.");

  add_com ("ptype", class_vars, ptype_command,
	   "Print definition of type TYPE.\n\
Argument may be a type name defined by typedef, or \"struct STRUCTNAME\"\n\
or \"union UNIONNAME\" or \"enum ENUMNAME\".\n\
The selected stack frame's lexical context is used to look up the name.");

  add_com ("whatis", class_vars, whatis_command,
	   "Print data type of expression EXP.");

  add_info ("display", display_info,
	    "Expressions to display when program stops, with code numbers.");

  add_cmd ("undisplay", class_vars, undisplay_command,
	   "Cancel some expressions to be displayed when program stops.\n\
Arguments are the code numbers of the expressions to stop displaying.\n\
No argument means cancel all automatic-display expressions.\n\
\"delete display\" has the same effect as this command.\n\
Do \"info display\" to see current list of code numbers.",
		  &cmdlist);

  add_com ("display", class_vars, display_command,
	   "Print value of expression EXP each time the program stops.\n\
/FMT may be used before EXP as in the \"print\" command.\n\
/FMT \"i\" or \"s\" or including a size-letter is allowed,\n\
as in the \"x\" command, and then EXP is used to get the address to examine\n\
and examining is done as in the \"x\" command.\n\n\
With no argument, display all currently requested auto-display expressions.\n\
Use \"undisplay\" to cancel display requests previously made.");

  add_cmd ("display", class_vars, enable_display, 
	   "Enable some expressions to be displayed when program stops.\n\
Arguments are the code numbers of the expressions to resume displaying.\n\
No argument means enable all automatic-display expressions.\n\
Do \"info display\" to see current list of code numbers.", &enablelist);

  add_cmd ("display", class_vars, disable_display_command, 
	   "Disable some expressions to be displayed when program stops.\n\
Arguments are the code numbers of the expressions to stop displaying.\n\
No argument means disable all automatic-display expressions.\n\
Do \"info display\" to see current list of code numbers.", &disablelist);

  add_cmd ("display", class_vars, undisplay_command, 
	   "Cancel some expressions to be displayed when program stops.\n\
Arguments are the code numbers of the expressions to stop displaying.\n\
No argument means cancel all automatic-display expressions.\n\
Do \"info display\" to see current list of code numbers.", &deletelist);

  add_com ("printf", class_vars, printf_command,
	"printf \"printf format string\", arg1, arg2, arg3, ..., argn\n\
This is useful for formatted output in user-defined commands.");
  add_com ("output", class_vars, output_command,
	   "Like \"print\" but don't put in value history and don't print newline.\n\
This is useful in user-defined commands.");

  add_prefix_cmd ("set", class_vars, set_command,
"Perform an assignment VAR = EXP.\n\
You must type the \"=\".  VAR may be a debugger \"convenience\" variable\n\
(names starting with $), a register (a few standard names starting with $),\n\
or an actual variable in the program being debugged.  EXP is any expression.\n\
Use \"set variable\" for variables with names identical to set subcommands.\n\
\nWith a subcommand, this command modifies parts of the gdb environment",
                  &setlist, "set ", 1, &cmdlist);

  add_cmd ("variable", class_vars, set_command,
           "Perform an assignment VAR = EXP.\n\
You must type the \"=\".  VAR may be a debugger \"convenience\" variable\n\
(names starting with $), a register (a few standard names starting with $),\n\
or an actual variable in the program being debugged.  EXP is any expression.\n\
This may usually be abbreviated to simply \"set\".",
           &setlist);

  add_com ("print", class_vars, print_command,
	   concat ("Print value of expression EXP.\n\
Variables accessible are those of the lexical environment of the selected\n\
stack frame, plus all those whose scope is global or an entire file.\n\
\n\
$NUM gets previous value number NUM.  $ and $$ are the last two values.\n\
$$NUM refers to NUM'th value back from the last one.\n\
Names starting with $ refer to registers (with the values they would have\n\
if the program were to return to the stack frame now selected, restoring\n\
all registers saved by frames farther in) or else to debugger\n\
\"convenience\" variables (any such name not a known register).\n\
Use assignment expressions to give values to convenience variables.\n",
		   "\n\
\{TYPE}ADREXP refers to a datum of data type TYPE, located at address ADREXP.\n\
@ is a binary operator for treating consecutive data objects\n\
anywhere in memory as an array.  FOO@NUM gives an array whose first\n\
element is FOO, whose second element is stored in the space following\n\
where FOO is stored, etc.  FOO must be an expression whose value\n\
resides in memory.\n",
		   "\n\
EXP may be preceded with /FMT, where FMT is a format letter\n\
but no count or size letter (see \"x\" command)."));
  add_com_alias ("p", "print", class_vars, 1);
}
