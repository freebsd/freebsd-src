/* Print values for GNU debugger GDB.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991 Free Software Foundation, Inc.

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
#include <string.h>
#include <varargs.h>
#include "frame.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "value.h"
#include "language.h"
#include "expression.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "target.h"
#include "breakpoint.h"
#include "demangle.h"

extern int asm_demangle;	/* Whether to demangle syms in asm printouts */
extern int addressprint;	/* Whether to print hex addresses in HLL " */

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

/* Largest offset between a symbolic value and an address, that will be
   printed as `0x1234 <symbol+offset>'.  */

static unsigned int max_symbolic_offset = UINT_MAX;

/* Append the source filename and linenumber of the symbol when
   printing a symbolic value as `<symbol at filename:linenum>' if set.  */
static int print_symbol_filename = 0;

/* Number of auto-display expression currently being displayed.
   So that we can disable it if we get an error or a signal within it.
   -1 when not doing one.  */

int current_display_number;

/* Flag to low-level print routines that this value is being printed
   in an epoch window.  We'd like to pass this as a parameter, but
   every routine would need to take it.  Perhaps we can encapsulate
   this in the I/O stream once we have GNU stdio. */

int inspect_it = 0;

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
  enum enable status;
};

/* Chain of expressions whose values should be displayed
   automatically each time the program stops.  */

static struct display *display_chain;

static int display_number;

/* Prototypes for local functions */

static void
delete_display PARAMS ((int));

static void
enable_display PARAMS ((char *, int));

static void
disable_display_command PARAMS ((char *, int));

static void
disassemble_command PARAMS ((char *, int));

static void
printf_command PARAMS ((char *, int));

static void
print_frame_nameless_args PARAMS ((struct frame_info *, long, int, int,
				   FILE *));

static void
display_info PARAMS ((char *, int));

static void
do_one_display PARAMS ((struct display *));

static void
undisplay_command PARAMS ((char *, int));

static void
free_display PARAMS ((struct display *));

static void
display_command PARAMS ((char *, int));

static void
x_command PARAMS ((char *, int));

static void
address_info PARAMS ((char *, int));

static void
set_command PARAMS ((char *, int));

static void
output_command PARAMS ((char *, int));

static void
call_command PARAMS ((char *, int));

static void
inspect_command PARAMS ((char *, int));

static void
print_command PARAMS ((char *, int));

static void
print_command_1 PARAMS ((char *, int, int));

static void
validate_format PARAMS ((struct format_data, char *));

static void
do_examine PARAMS ((struct format_data, CORE_ADDR));

static void
print_formatted PARAMS ((value, int, int));

static struct format_data
decode_format PARAMS ((char **, int, int));


/* Decode a format specification.  *STRING_PTR should point to it.
   OFORMAT and OSIZE are used as defaults for the format and size
   if none are given in the format specification.
   If OSIZE is zero, then the size field of the returned value
   should be set only if a size is explicitly specified by the
   user.
   The structure returned describes all the data
   found in the specification.  In addition, *STRING_PTR is advanced
   past the specification and past all whitespace following it.  */

static struct format_data
decode_format (string_ptr, oformat, osize)
     char **string_ptr;
     int oformat;
     int osize;
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
      else if (*p >= 'a' && *p <= 'z')
	val.format = *p++;
      else
	break;
    }

#ifndef CC_HAS_LONG_LONG
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
     register int format;
     int size;
{
  int len = TYPE_LENGTH (VALUE_TYPE (val));

  if (VALUE_LVAL (val) == lval_memory)
    next_address = VALUE_ADDRESS (val) + len;

  switch (format)
    {
    case 's':
      next_address = VALUE_ADDRESS (val)
	+ value_print (value_addr (val), stdout, format, Val_pretty_default);
      break;

    case 'i':
      /* The old comment says
	 "Force output out, print_insn not using _filtered".
	 I'm not completely sure what that means, I suspect most print_insn
	 now do use _filtered, so I guess it's obsolete.  */
      /* We often wrap here if there are long symbolic names.  */
      wrap_here ("    ");
      next_address = VALUE_ADDRESS (val)
	+ print_insn (VALUE_ADDRESS (val), stdout);
      break;

    default:
      if (format == 0
	  || TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_ARRAY
	  || TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_STRING
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
     int format;
     int size;
     FILE *stream;
{
  LONGEST val_long;
  int len = TYPE_LENGTH (type);

  if (len > sizeof (LONGEST)
      && (format == 't'
	  || format == 'c'
	  || format == 'o'
	  || format == 'u'
	  || format == 'd'
	  || format == 'x'))
    {
      /* We can't print it normally, but we can print it in hex.
         Printing it in the wrong radix is more useful than saying
	 "use /x, you dummy".  */
      /* FIXME:  we could also do octal or binary if that was the
	 desired format.  */
      /* FIXME:  we should be using the size field to give us a minimum
	 field width to print.  */
      val_print_type_code_int (type, valaddr, stream);
      return;
    }

  val_long = unpack_long (type, valaddr);

  /* If we are printing it as unsigned, truncate it in case it is actually
     a negative signed value (e.g. "print/u (short)-1" should print 65535
     (if shorts are 16 bits) instead of 4294967295).  */
  if (format != 'd')
    {
      if (len < sizeof (LONGEST))
	val_long &= ((LONGEST) 1 << HOST_CHAR_BIT * len) - 1;
    }

  switch (format)
    {
    case 'x':
      if (!size)
	{
	  /* no size specified, like in print.  Print varying # of digits. */
	  print_longest (stream, 'x', 1, val_long);
	}
      else
	switch (size)
	  {
	  case 'b':
	  case 'h':
	  case 'w':
	  case 'g':
	    print_longest (stream, size, 1, val_long);
	    break;
	  default:
	    error ("Undefined output size \"%c\".", size);
	  }
      break;

    case 'd':
      print_longest (stream, 'd', 1, val_long);
      break;

    case 'u':
      print_longest (stream, 'u', 0, val_long);
      break;

    case 'o':
      if (val_long)
	print_longest (stream, 'o', 1, val_long);
      else
	fprintf_filtered (stream, "0");
      break;

    case 'a':
      print_address (unpack_pointer (type, valaddr), stream);
      break;

    case 'c':
      value_print (value_from_longest (builtin_type_char, val_long), stream, 0,
		   Val_pretty_default);
      break;

    case 'f':
      if (len == sizeof (float))
	type = builtin_type_float;
      else if (len == sizeof (double))
	type = builtin_type_double;
      print_floating (valaddr, type, stream);
      break;

    case 0:
      abort ();

    case 't':
      /* Binary; 't' stands for "two".  */
      {
        char bits[8*(sizeof val_long) + 1];
	char *cp = bits;
	int width;

        if (!size)
	  width = 8*(sizeof val_long);
        else
          switch (size)
	    {
	    case 'b':
	      width = 8;
	      break;
	    case 'h':
	      width = 16;
	      break;
	    case 'w':
	      width = 32;
	      break;
	    case 'g':
	      width = 64;
	      break;
	    default:
	      error ("Undefined output size \"%c\".", size);
	    }

        bits[width] = '\0';
        while (width-- > 0)
          {
            bits[width] = (val_long & 1) ? '1' : '0';
            val_long >>= 1;
          }
	if (!size)
	  {
	    while (*cp && *cp == '0')
	      cp++;
	    if (*cp == '\0')
	      cp--;
	  }
	fprintf_filtered (stream, local_binary_format_prefix());
        fprintf_filtered (stream, cp);
	fprintf_filtered (stream, local_binary_format_suffix());
      }
      break;

    default:
      error ("Undefined output format \"%c\".", format);
    }
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
		   value_from_longest (lookup_pointer_type (builtin_type_void),
				    (LONGEST) addr));
}

/* Optionally print address ADDR symbolically as <SYMBOL+OFFSET> on STREAM,
   after LEADIN.  Print nothing if no symbolic name is found nearby.
   DO_DEMANGLE controls whether to print a symbol in its native "raw" form,
   or to interpret it as a possible C++ name and convert it back to source
   form.  However note that DO_DEMANGLE can be overridden by the specific
   settings of the demangle and asm_demangle variables. */

void
print_address_symbolic (addr, stream, do_demangle, leadin)
     CORE_ADDR addr;
     FILE *stream;
     int do_demangle;
     char *leadin;
{
  CORE_ADDR name_location;
  register struct symbol *symbol;
  char *name;

  /* First try to find the address in the symbol tables to find
     static functions. If that doesn't succeed we try the minimal symbol
     vector for symbols in non-text space.
     FIXME: Should find a way to get at the static non-text symbols too.  */
  
  symbol = find_pc_function (addr);
  if (symbol)
    {
    name_location = BLOCK_START (SYMBOL_BLOCK_VALUE (symbol));
    if (do_demangle)
      name = SYMBOL_SOURCE_NAME (symbol);
    else
      name = SYMBOL_LINKAGE_NAME (symbol);
    }
  else
    {
    register struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (addr);

    /* If nothing comes out, don't print anything symbolic.  */
    if (msymbol == NULL)
      return;
    name_location = SYMBOL_VALUE_ADDRESS (msymbol);
    if (do_demangle)
      name = SYMBOL_SOURCE_NAME (msymbol);
    else
      name = SYMBOL_LINKAGE_NAME (msymbol);
    }

  /* If the nearest symbol is too far away, don't print anything symbolic.  */

  /* For when CORE_ADDR is larger than unsigned int, we do math in
     CORE_ADDR.  But when we detect unsigned wraparound in the
     CORE_ADDR math, we ignore this test and print the offset,
     because addr+max_symbolic_offset has wrapped through the end
     of the address space back to the beginning, giving bogus comparison.  */
  if (addr > name_location + max_symbolic_offset
      && name_location + max_symbolic_offset > name_location)
    return;

  fputs_filtered (leadin, stream);
  fputs_filtered ("<", stream);
  fputs_filtered (name, stream);
  if (addr != name_location)
    fprintf_filtered (stream, "+%u", (unsigned int)(addr - name_location));

  /* Append source filename and line number if desired.  */
  if (symbol && print_symbol_filename)
    {
      struct symtab_and_line sal;

      sal = find_pc_line (addr, 0);
      if (sal.symtab)
	fprintf_filtered (stream, " at %s:%d", sal.symtab->filename, sal.line);
    }
  fputs_filtered (">", stream);
}

/* Print address ADDR symbolically on STREAM.
   First print it as a number.  Then perhaps print
   <SYMBOL + OFFSET> after the number.  */

void
print_address (addr, stream)
     CORE_ADDR addr;
     FILE *stream;
{
#if 0 && defined (ADDR_BITS_REMOVE)
  /* This is wrong for pointer to char, in which we do want to print
     the low bits.  */
  fprintf_filtered (stream, local_hex_format(),
		    (unsigned long) ADDR_BITS_REMOVE(addr));
#else
  fprintf_filtered (stream, local_hex_format(), (unsigned long) addr);
#endif
  print_address_symbolic (addr, stream, asm_demangle, " ");
}

/* Print address ADDR symbolically on STREAM.  Parameter DEMANGLE
   controls whether to print the symbolic name "raw" or demangled.
   Global setting "addressprint" controls whether to print hex address
   or not.  */

void
print_address_demangle (addr, stream, do_demangle)
     CORE_ADDR addr;
     FILE *stream;
     int do_demangle;
{
  if (addr == 0) {
    fprintf_filtered (stream, "0");
  } else if (addressprint) {
    fprintf_filtered (stream, local_hex_format(), (unsigned long) addr);
    print_address_symbolic (addr, stream, do_demangle, " ");
  } else {
    print_address_symbolic (addr, stream, do_demangle, "");
  }
}


/* These are the types that $__ will get after an examine command of one
   of these sizes.  */

static struct type *examine_b_type;
static struct type *examine_h_type;
static struct type *examine_w_type;
static struct type *examine_g_type;

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
  struct type *val_type = NULL;
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
    val_type = examine_b_type;
  else if (size == 'h')
    val_type = examine_h_type;
  else if (size == 'w')
    val_type = examine_w_type;
  else if (size == 'g')
    val_type = examine_g_type;

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

/*  Evaluate string EXP as an expression in the current language and
    print the resulting value.  EXP may contain a format specifier as the
    first argument ("/x myvar" for example, to print myvar in hex).
    */

static void
print_command_1 (exp, inspect, voidprint)
     char *exp;
     int inspect;
     int voidprint;
{
  struct expression *expr;
  register struct cleanup *old_chain = 0;
  register char format = 0;
  register value val;
  struct format_data fmt;
  int cleanup = 0;

  /* Pass inspect flag to the rest of the print routines in a global (sigh). */
  inspect_it = inspect;

  if (exp && *exp == '/')
    {
      exp++;
      fmt = decode_format (&exp, last_format, 0);
      validate_format (fmt, "print");
      last_format = format = fmt.format;
    }
  else
    {
      fmt.count = 1;
      fmt.format = 0;
      fmt.size = 0;
    }

  if (exp && *exp)
    {
      extern int objectprint;
      struct type *type;
      expr = parse_expression (exp);
      old_chain = make_cleanup (free_current_contents, &expr);
      cleanup = 1;
      val = evaluate_expression (expr);

      /* C++: figure out what type we actually want to print it as.  */
      type = VALUE_TYPE (val);

      if (objectprint
	  && (   TYPE_CODE (type) == TYPE_CODE_PTR
	      || TYPE_CODE (type) == TYPE_CODE_REF)
	  && (   TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_STRUCT
	      || TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_UNION))
	{
	  value v;

	  v = value_from_vtable_info (val, TYPE_TARGET_TYPE (type));
	  if (v != 0)
	    {
	      val = v;
	      type = VALUE_TYPE (val);
	    }
	}
    }
  else
    val = access_value_history (0);

  if (voidprint || (val && VALUE_TYPE (val) &&
                    TYPE_CODE (VALUE_TYPE (val)) != TYPE_CODE_VOID))
    {
      int histindex = record_latest_value (val);

      if (inspect)
	printf ("\031(gdb-makebuffer \"%s\"  %d '(\"", exp, histindex);
      else
	if (histindex >= 0) printf_filtered ("$%d = ", histindex);

      print_formatted (val, format, fmt.size);
      printf_filtered ("\n");
      if (inspect)
	printf("\") )\030");
    }

  if (cleanup)
    do_cleanups (old_chain);
  inspect_it = 0;	/* Reset print routines to normal */
}

/* ARGSUSED */
static void
print_command (exp, from_tty)
     char *exp;
     int from_tty;
{
  print_command_1 (exp, 0, 1);
}

/* Same as print, except in epoch, it gets its own window */
/* ARGSUSED */
static void
inspect_command (exp, from_tty)
     char *exp;
     int from_tty;
{
  extern int epoch_interface;

  print_command_1 (exp, epoch_interface, 1);
}

/* Same as print, except it doesn't print void results. */
/* ARGSUSED */
static void
call_command (exp, from_tty)
     char *exp;
     int from_tty;
{
  print_command_1 (exp, 0, 0);
}

/* ARGSUSED */
static void
output_command (exp, from_tty)
     char *exp;
     int from_tty;
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
      validate_format (fmt, "output");
      format = fmt.format;
    }

  expr = parse_expression (exp);
  old_chain = make_cleanup (free_current_contents, &expr);

  val = evaluate_expression (expr);

  print_formatted (val, format, fmt.size);

  do_cleanups (old_chain);
}

/* ARGSUSED */
static void
set_command (exp, from_tty)
     char *exp;
     int from_tty;
{
  struct expression *expr = parse_expression (exp);
  register struct cleanup *old_chain
    = make_cleanup (free_current_contents, &expr);
  evaluate_expression (expr);
  do_cleanups (old_chain);
}

/* ARGSUSED */
static void
address_info (exp, from_tty)
     char *exp;
     int from_tty;
{
  register struct symbol *sym;
  register struct minimal_symbol *msymbol;
  register long val;
  register long basereg;
  int is_a_field_of_this;	/* C++: lookup_symbol sets this to nonzero
				   if exp is a field of `this'. */

  if (exp == 0)
    error ("Argument required.");

  sym = lookup_symbol (exp, get_selected_block (), VAR_NAMESPACE, 
		       &is_a_field_of_this, (struct symtab **)NULL);
  if (sym == NULL)
    {
      if (is_a_field_of_this)
	{
	  printf ("Symbol \"%s\" is a field of the local class variable `this'\n", exp);
	  return;
	}

      msymbol = lookup_minimal_symbol (exp, (struct objfile *) NULL);

      if (msymbol != NULL)
	printf ("Symbol \"%s\" is at %s in a file compiled without debugging.\n",
		exp,
		local_hex_string((unsigned long) SYMBOL_VALUE_ADDRESS (msymbol)));
      else
	error ("No symbol \"%s\" in current context.", exp);
      return;
    }

  printf ("Symbol \"%s\" is ", SYMBOL_NAME (sym));
  val = SYMBOL_VALUE (sym);
  basereg = SYMBOL_BASEREG (sym);

  switch (SYMBOL_CLASS (sym))
    {
    case LOC_CONST:
    case LOC_CONST_BYTES:
      printf ("constant");
      break;

    case LOC_LABEL:
      printf ("a label at address %s",
	      local_hex_string((unsigned long) SYMBOL_VALUE_ADDRESS (sym)));
      break;

    case LOC_REGISTER:
      printf ("a variable in register %s", reg_names[val]);
      break;

    case LOC_STATIC:
      printf ("static storage at address %s",
	      local_hex_string((unsigned long) SYMBOL_VALUE_ADDRESS (sym)));
      break;

    case LOC_REGPARM:
      printf ("an argument in register %s", reg_names[val]);
      break;

    case LOC_REGPARM_ADDR:
      printf ("address of an argument in register %s", reg_names[val]);
      break;

    case LOC_ARG:
      printf ("an argument at offset %ld", val);
      break;

    case LOC_LOCAL_ARG:
      printf ("an argument at frame offset %ld", val);
      break;

    case LOC_LOCAL:
      printf ("a local variable at frame offset %ld", val);
      break;

    case LOC_REF_ARG:
      printf ("a reference argument at offset %ld", val);
      break;

    case LOC_BASEREG:
      printf ("a variable at offset %ld from register %s",
	      val, reg_names[basereg]);
      break;

    case LOC_BASEREG_ARG:
      printf ("an argument at offset %ld from register %s",
	      val, reg_names[basereg]);
      break;

    case LOC_TYPEDEF:
      printf ("a typedef");
      break;

    case LOC_BLOCK:
      printf ("a function at address %s",
	      local_hex_string((unsigned long) BLOCK_START (SYMBOL_BLOCK_VALUE (sym))));
      break;

    case LOC_OPTIMIZED_OUT:
      printf_filtered ("optimized out");
      break;
      
    default:
      printf ("of unknown (botched) type");
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
    }

  /* If we have an expression, evaluate it and use it as the address.  */

  if (exp != 0 && *exp != 0)
    {
      expr = parse_expression (exp);
      /* Cause expression not to be there any more
	 if this command is repeated with Newline.
	 But don't clobber a user-defined command's definition.  */
      if (from_tty)
	*exp = 0;
      old_chain = make_cleanup (free_current_contents, &expr);
      val = evaluate_expression (expr);
      if (TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_REF)
	val = value_ind (val);
      /* In rvalue contexts, such as this, functions are coerced into
	 pointers to functions.  This makes "x/i main" work.  */
      if (/* last_format == 'i'
	  && */ TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_FUNC
	  && VALUE_LVAL (val) == lval_memory)
	next_address = VALUE_ADDRESS (val);
      else
	next_address = value_as_pointer (val);
      do_cleanups (old_chain);
    }

  do_examine (fmt, next_address);

  /* If the examine succeeds, we remember its size and format for next time.  */
  last_size = fmt.size;
  last_format = fmt.format;

  /* Set a couple of internal variables if appropriate. */
  if (last_examine_value)
    {
      /* Make last address examined available to the user as $_.  Use
	 the correct pointer type.  */
      set_internalvar (lookup_internalvar ("_"),
	       value_from_longest (
		 lookup_pointer_type (VALUE_TYPE (last_examine_value)),
				   (LONGEST) last_examine_address));
      
      /* Make contents of last address examined available to the user as $__.*/
      set_internalvar (lookup_internalvar ("__"), last_examine_value);
    }
}


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
  expr = parse_expression (exp);

  new = (struct display *) xmalloc (sizeof (struct display));

  new->exp = expr;
  new->block = innermost_block;
  new->next = display_chain;
  new->number = ++display_number;
  new->format = fmt;
  new->status = enabled;
  display_chain = new;

  if (from_tty && target_has_execution)
    do_one_display (new);

  dont_repeat ();
}

static void
free_display (d)
     struct display *d;
{
  free ((PTR)d->exp);
  free ((PTR)d);
}

/* Clear out the display_chain.
   Done when new symtabs are loaded, since this invalidates
   the types stored in many expressions.  */

void
clear_displays ()
{
  register struct display *d;

  while ((d = display_chain) != NULL)
    {
      free ((PTR)d->exp);
      display_chain = d->next;
      free ((PTR)d);
    }
}

/* Delete the auto-display number NUM.  */

static void
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
undisplay_command (args, from_tty)
     char *args;
     int from_tty;
{
  register char *p = args;
  register char *p1;
  register int num;

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
      CORE_ADDR addr;
      
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
      
      addr = value_as_pointer (evaluate_expression (d->exp));
      if (d->format.format == 'i')
	addr = ADDR_BITS_REMOVE (addr);
      
      do_examine (d->format, addr);
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
display_info (ignore, from_tty)
     char *ignore;
     int from_tty;
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

static void
enable_display (args, from_tty)
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

/* ARGSUSED */
static void
disable_display_command (args, from_tty)
     char *args;
     int from_tty;
{
  register char *p = args;
  register char *p1;
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

/* Print the arguments of a stack frame, given the function FUNC
   running in that frame (as a symbol), the info on the frame,
   and the number of args according to the stack frame (or -1 if unknown).  */

/* References here and elsewhere to "number of args according to the
   stack frame" appear in all cases to refer to "number of ints of args
   according to the stack frame".  At least for VAX, i386, isi.  */

void
print_frame_args (func, fi, num, stream)
     struct symbol *func;
     struct frame_info *fi;
     int num;
     FILE *stream;
{
  struct block *b = NULL;
  int nsyms = 0;
  int first = 1;
  register int i;
  register struct symbol *sym;
  register value val;
  /* Offset of next stack argument beyond the one we have seen that is
     at the highest offset.
     -1 if we haven't come to a stack argument yet.  */
  long highest_offset = -1;
  int arg_size;
  /* Number of ints of arguments that we have printed so far.  */
  int args_printed = 0;

  if (func)
    {
      b = SYMBOL_BLOCK_VALUE (func);
      nsyms = BLOCK_NSYMS (b);
    }

  for (i = 0; i < nsyms; i++)
    {
      QUIT;
      sym = BLOCK_SYM (b, i);

      /* Keep track of the highest stack argument offset seen, and
	 skip over any kinds of symbols we don't care about.  */

      switch (SYMBOL_CLASS (sym)) {
      case LOC_ARG:
      case LOC_REF_ARG:
	{
	  long current_offset = SYMBOL_VALUE (sym);

	  arg_size = TYPE_LENGTH (SYMBOL_TYPE (sym));
	  
	  /* Compute address of next argument by adding the size of
	     this argument and rounding to an int boundary.  */
	  current_offset
	    = ((current_offset + arg_size + sizeof (int) - 1)
	       & ~(sizeof (int) - 1));

	  /* If this is the highest offset seen yet, set highest_offset.  */
	  if (highest_offset == -1
	      || (current_offset > highest_offset))
	    highest_offset = current_offset;

	  /* Add the number of ints we're about to print to args_printed.  */
	  args_printed += (arg_size + sizeof (int) - 1) / sizeof (int);
	}

      /* We care about types of symbols, but don't need to keep track of
	 stack offsets in them.  */
      case LOC_REGPARM:
      case LOC_REGPARM_ADDR:
      case LOC_LOCAL_ARG:
      case LOC_BASEREG_ARG:
	break;

      /* Other types of symbols we just skip over.  */
      default:
	continue;
      }

      /* We have to look up the symbol because arguments can have
	 two entries (one a parameter, one a local) and the one we
	 want is the local, which lookup_symbol will find for us.
	 This includes gcc1 (not gcc2) on the sparc when passing a
	 small structure and gcc2 when the argument type is float
	 and it is passed as a double and converted to float by
	 the prologue (in the latter case the type of the LOC_ARG
	 symbol is double and the type of the LOC_LOCAL symbol is
	 float).  There are also LOC_ARG/LOC_REGISTER pairs which
	 are not combined in symbol-reading.  */
      /* But if the parameter name is null, don't try it.
	 Null parameter names occur on the RS/6000, for traceback tables.
	 FIXME, should we even print them?  */

      if (*SYMBOL_NAME (sym))
        sym = lookup_symbol
	  (SYMBOL_NAME (sym),
	   b, VAR_NAMESPACE, (int *)NULL, (struct symtab **)NULL);

      /* Print the current arg.  */
      if (! first)
	fprintf_filtered (stream, ", ");
      wrap_here ("    ");
      fprintf_symbol_filtered (stream, SYMBOL_SOURCE_NAME (sym),
			       SYMBOL_LANGUAGE (sym), DMGL_PARAMS | DMGL_ANSI);
      fputs_filtered ("=", stream);

      /* Avoid value_print because it will deref ref parameters.  We just
	 want to print their addresses.  Print ??? for args whose address
	 we do not know.  We pass 2 as "recurse" to val_print because our
	 standard indentation here is 4 spaces, and val_print indents
	 2 for each recurse.  */
      val = read_var_value (sym, FRAME_INFO_ID (fi));
      if (val)
        val_print (VALUE_TYPE (val), VALUE_CONTENTS (val), VALUE_ADDRESS (val),
		   stream, 0, 0, 2, Val_no_prettyprint);
      else
	fputs_filtered ("???", stream);
      first = 0;
    }

  /* Don't print nameless args in situations where we don't know
     enough about the stack to find them.  */
  if (num != -1)
    {
      long start;

      if (highest_offset == -1)
	start = FRAME_ARGS_SKIP;
      else
	start = highest_offset;

      print_frame_nameless_args (fi, start, num - args_printed,
				 first, stream);
    }
}

/* Print nameless args on STREAM.
   FI is the frameinfo for this frame, START is the offset
   of the first nameless arg, and NUM is the number of nameless args to
   print.  FIRST is nonzero if this is the first argument (not just
   the first nameless arg).  */
static void
print_frame_nameless_args (fi, start, num, first, stream)
     struct frame_info *fi;
     long start;
     int num;
     int first;
     FILE *stream;
{
  int i;
  CORE_ADDR argsaddr;
  long arg_value;

  for (i = 0; i < num; i++)
    {
      QUIT;
#ifdef NAMELESS_ARG_VALUE
      NAMELESS_ARG_VALUE (fi, start, &arg_value);
#else
      argsaddr = FRAME_ARGS_ADDRESS (fi);
      if (!argsaddr)
	return;

      arg_value = read_memory_integer (argsaddr + start, sizeof (int));
#endif

      if (!first)
	fprintf_filtered (stream, ", ");

#ifdef	PRINT_NAMELESS_INTEGER
      PRINT_NAMELESS_INTEGER (stream, arg_value);
#else
#ifdef PRINT_TYPELESS_INTEGER
      PRINT_TYPELESS_INTEGER (stream, builtin_type_int, (LONGEST) arg_value);
#else
      fprintf_filtered (stream, "%d", arg_value);
#endif /* PRINT_TYPELESS_INTEGER */
#endif /* PRINT_NAMELESS_INTEGER */
      first = 0;
      start += sizeof (int);
    }
}

/* ARGSUSED */
static void
printf_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  register char *f;
  register char *s = arg;
  char *string;
  value *val_args;
  char *substrings;
  char *current_substring;
  int nargs = 0;
  int allocated_args = 20;
  va_list args_to_vprintf;
  struct cleanup *old_cleanups;

  val_args = (value *) xmalloc (allocated_args * sizeof (value));
  old_cleanups = make_cleanup (free_current_contents, &val_args);

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

  /* Need extra space for the '\0's.  Doubling the size is sufficient.  */
  substrings = alloca (strlen (string) * 2);
  current_substring = substrings;

  {
    /* Now scan the string for %-specs and see what kinds of args they want.
       argclass[I] classifies the %-specs so we can give vprintf something
       of the right size.  */

    enum argclass {no_arg, int_arg, string_arg, double_arg, long_long_arg};
    enum argclass *argclass;
    enum argclass this_argclass;
    char *last_arg;
    int nargs_wanted;
    int lcount;
    int i;

    argclass = (enum argclass *) alloca (strlen (s) * sizeof *argclass);
    nargs_wanted = 0;
    f = string;
    last_arg = string;
    while (*f)
      if (*f++ == '%')
	{
	  lcount = 0;
	  while (strchr ("0123456789.hlL-+ #", *f)) 
	    {
	      if (*f == 'l' || *f == 'L')
		lcount++;
	      f++;
	    }
	  switch (*f)
	    {
	    case 's':
	      this_argclass = string_arg;
	      break;

	    case 'e':
	    case 'f':
	    case 'g':
	      this_argclass = double_arg;
	      break;

	    case '*':
	      error ("`*' not supported for precision or width in printf");

	    case 'n':
	      error ("Format specifier `n' not supported in printf");

	    case '%':
	      this_argclass = no_arg;
	      break;

	    default:
	      if (lcount > 1)
		this_argclass = long_long_arg;
	      else
		this_argclass = int_arg;
	      break;
	    }
	  f++;
	  if (this_argclass != no_arg)
	    {
	      strncpy (current_substring, last_arg, f - last_arg);
	      current_substring += f - last_arg;
	      *current_substring++ = '\0';
	      last_arg = f;
	      argclass[nargs_wanted++] = this_argclass;
	    }
	}

    /* Now, parse all arguments and evaluate them.
       Store the VALUEs in VAL_ARGS.  */

    while (*s != '\0')
      {
	char *s1;
	if (nargs == allocated_args)
	  val_args = (value *) xrealloc ((char *) val_args,
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

    /* FIXME: We should be using vprintf_filtered, but as long as it
       has an arbitrary limit that is unacceptable.  Correct fix is
       for vprintf_filtered to scan down the format string so it knows
       how big a buffer it needs (perhaps by putting a vasprintf (see
       GNU C library) in libiberty).

       But for now, just force out any pending output, so at least the output
       appears in the correct order.  */
    wrap_here ((char *)NULL);

    /* Now actually print them.  */
    current_substring = substrings;
    for (i = 0; i < nargs; i++)
      {
	switch (argclass[i])
	  {
	  case string_arg:
	    {
	      char *str;
	      CORE_ADDR tem;
	      int j;
	      tem = value_as_pointer (val_args[i]);

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

	      /* Don't use printf_filtered because of arbitrary limit.  */
	      printf (current_substring, str);
	    }
	    break;
	  case double_arg:
	    {
	      double val = value_as_double (val_args[i]);
	      /* Don't use printf_filtered because of arbitrary limit.  */
	      printf (current_substring, val);
	      break;
	    }
	  case long_long_arg:
#if defined (CC_HAS_LONG_LONG) && defined (PRINTF_HAS_LONG_LONG)
	    {
	      long long val = value_as_long (val_args[i]);
	      /* Don't use printf_filtered because of arbitrary limit.  */
	      printf (current_substring, val);
	      break;
	    }
#else
	    error ("long long not supported in printf");
#endif
	  case int_arg:
	    {
	      /* FIXME: there should be separate int_arg and long_arg.  */
	      long val = value_as_long (val_args[i]);
	      /* Don't use printf_filtered because of arbitrary limit.  */
	      printf (current_substring, val);
	      break;
	    }
	  default:
	    error ("internal error in printf_command");
	  }
	/* Skip to the next substring.  */
	current_substring += strlen (current_substring) + 1;
      }
    /* Print the portion of the format string after the last argument.  */
    /* It would be OK to use printf_filtered here.  */
    printf (last_arg);
  }
  do_cleanups (old_cleanups);
}

/* Dump a specified section of assembly code.  With no command line
   arguments, this command will dump the assembly code for the
   function surrounding the pc value in the selected frame.  With one
   argument, it will dump the assembly code surrounding that pc value.
   Two arguments are interpeted as bounds within which to dump
   assembly.  */

/* ARGSUSED */
static void
disassemble_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  CORE_ADDR low, high;
  char *name;
  CORE_ADDR pc;
  char *space_index;

  name = NULL;
  if (!arg)
    {
      if (!selected_frame)
	error ("No frame selected.\n");

      pc = get_frame_pc (selected_frame);
      if (find_pc_partial_function (pc, &name, &low, &high) == 0)
	error ("No function contains program counter for selected frame.\n");
    }
  else if (!(space_index = (char *) strchr (arg, ' ')))
    {
      /* One argument.  */
      pc = parse_and_eval_address (arg);
      if (find_pc_partial_function (pc, &name, &low, &high) == 0)
	error ("No function contains specified address.\n");
    }
  else
    {
      /* Two arguments.  */
      *space_index = '\0';
      low = parse_and_eval_address (arg);
      high = parse_and_eval_address (space_index + 1);
    }

  printf_filtered ("Dump of assembler code ");
  if (name != NULL)
    {
      printf_filtered ("for function %s:\n", name);
    }
  else
    {
      printf_filtered ("from %s ", local_hex_string((unsigned long) low));
      printf_filtered ("to %s:\n", local_hex_string((unsigned long) high));
    }

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
  t(binary), f(float), a(address), i(instruction), c(char) and s(string).\n\
Size letters are b(byte), h(halfword), w(word), g(giant, 8 bytes).\n\
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

#if 0
  add_com ("whereis", class_vars, whereis_command,
	   "Print line number and file of definition of variable.");
#endif
  
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
"Evaluate expression EXP and assign result to variable VAR, using assignment\n\
syntax appropriate for the current language (VAR = EXP or VAR := EXP for\n\
example).  VAR may be a debugger \"convenience\" variable (names starting\n\
with $), a register (a few standard names starting with $), or an actual\n\
variable in the program being debugged.  EXP is any valid expression.\n\
Use \"set variable\" for variables with names identical to set subcommands.\n\
\nWith a subcommand, this command modifies parts of the gdb environment.\n\
You can see these environment settings with the \"show\" command.",
                  &setlist, "set ", 1, &cmdlist);

  /* "call" is the same as "set", but handy for dbx users to call fns. */
  add_com ("call", class_vars, call_command,
	   "Call a function in the program.\n\
The argument is the function name and arguments, in the notation of the\n\
current working language.  The result is printed and saved in the value\n\
history, if it is not void.");

  add_cmd ("variable", class_vars, set_command,
"Evaluate expression EXP and assign result to variable VAR, using assignment\n\
syntax appropriate for the current language (VAR = EXP or VAR := EXP for\n\
example).  VAR may be a debugger \"convenience\" variable (names starting\n\
with $), a register (a few standard names starting with $), or an actual\n\
variable in the program being debugged.  EXP is any valid expression.\n\
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
{TYPE}ADREXP refers to a datum of data type TYPE, located at address ADREXP.\n\
@ is a binary operator for treating consecutive data objects\n\
anywhere in memory as an array.  FOO@NUM gives an array whose first\n\
element is FOO, whose second element is stored in the space following\n\
where FOO is stored, etc.  FOO must be an expression whose value\n\
resides in memory.\n",
		   "\n\
EXP may be preceded with /FMT, where FMT is a format letter\n\
but no count or size letter (see \"x\" command).", NULL));
  add_com_alias ("p", "print", class_vars, 1);

  add_com ("inspect", class_vars, inspect_command,
"Same as \"print\" command, except that if you are running in the epoch\n\
environment, the value is printed in its own window.");

  add_show_from_set (
      add_set_cmd ("max-symbolic-offset", no_class, var_uinteger,
		   (char *)&max_symbolic_offset,
	"Set the largest offset that will be printed in <symbol+1234> form.",
		   &setprintlist),
      &showprintlist);
  add_show_from_set (
      add_set_cmd ("symbol-filename", no_class, var_boolean,
		   (char *)&print_symbol_filename,
	"Set printing of source filename and line number with <symbol>.",
		   &setprintlist),
      &showprintlist);

  examine_b_type = init_type (TYPE_CODE_INT, 1, 0, NULL, NULL);
  examine_h_type = init_type (TYPE_CODE_INT, 2, 0, NULL, NULL);
  examine_w_type = init_type (TYPE_CODE_INT, 4, 0, NULL, NULL);
  examine_g_type = init_type (TYPE_CODE_INT, 8, 0, NULL, NULL);
}
