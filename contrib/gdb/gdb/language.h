/* Source-language-related definitions for GDB.
   Copyright 1991, 1992 Free Software Foundation, Inc.
   Contributed by the Department of Computer Science at the State University
   of New York at Buffalo.

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

#if !defined (LANGUAGE_H)
#define LANGUAGE_H 1

#ifdef __STDC__		/* Forward decls for prototypes */
struct value;
struct objfile;
struct expression;
/* enum exp_opcode;	ANSI's `wisdom' didn't include forward enum decls. */
#endif

/* This used to be included to configure GDB for one or more specific
   languages.  Now it is shortcutted to configure for all of them.  FIXME.  */
/* #include "lang_def.h" */
#define	_LANG_c
#define	_LANG_m2
#define	_LANG_chill
#define _LANG_fortran

#define MAX_FORTRAN_DIMS  7   /* Maximum number of F77 array dims */ 

/* range_mode ==
   range_mode_auto:   range_check set automatically to default of language.
   range_mode_manual: range_check set manually by user.  */

extern enum range_mode {range_mode_auto, range_mode_manual} range_mode;

/* range_check ==
   range_check_on:    Ranges are checked in GDB expressions, producing errors.
   range_check_warn:  Ranges are checked, producing warnings.
   range_check_off:   Ranges are not checked in GDB expressions.  */

extern enum range_check
  {range_check_off, range_check_warn, range_check_on} range_check;

/* type_mode ==
   type_mode_auto:   type_check set automatically to default of language
   type_mode_manual: type_check set manually by user. */

extern enum type_mode {type_mode_auto, type_mode_manual} type_mode;

/* type_check ==
   type_check_on:    Types are checked in GDB expressions, producing errors.
   type_check_warn:  Types are checked, producing warnings.
   type_check_off:   Types are not checked in GDB expressions.  */

extern enum type_check
  {type_check_off, type_check_warn, type_check_on} type_check;

/* Information for doing language dependent formatting of printed values. */

struct language_format_info
{
  /* The format that can be passed directly to standard C printf functions
     to generate a completely formatted value in the format appropriate for
     the language. */

  char *la_format;

  /* The prefix to be used when directly printing a value, or constructing
     a standard C printf format.  This generally is everything up to the
     conversion specification (the part introduced by the '%' character
     and terminated by the conversion specifier character). */

  char *la_format_prefix;

  /* The conversion specifier.  This is generally everything after the
     field width and precision, typically only a single character such
     as 'o' for octal format or 'x' for hexadecimal format. */

  char *la_format_specifier;

  /* The suffix to be used when directly printing a value, or constructing
     a standard C printf format.  This generally is everything after the
     conversion specification (the part introduced by the '%' character
     and terminated by the conversion specifier character). */

  char *la_format_suffix;		/* Suffix for custom format string */
};

/* Structure tying together assorted information about a language.  */

struct language_defn
{
  /* Name of the language */
  
  char *la_name;

  /* its symtab language-enum (defs.h) */

  enum language la_language;

  /* Its builtin types.  This is a vector ended by a NULL pointer.  These
     types can be specified by name in parsing types in expressions,
     regardless of whether the program being debugged actually defines
     such a type.  */

  struct type ** const *la_builtin_type_vector;

  /* Default range checking */

  enum range_check la_range_check;

  /* Default type checking */

  enum type_check la_type_check;

  /* Parser function. */
  
  int (*la_parser) PARAMS((void));

  /* Parser error function */

  void (*la_error) PARAMS ((char *));

  /* Evaluate an expression. */
  struct value * (*evaluate_exp) PARAMS ((struct type*, struct expression *, 
					  int *, enum noside));

  void (*la_printchar) PARAMS ((int, GDB_FILE *));

  void (*la_printstr) PARAMS ((GDB_FILE *, char *, unsigned int, int));

  struct type *(*la_fund_type) PARAMS ((struct objfile *, int));

  /* Print a type using syntax appropriate for this language. */

  void (*la_print_type) PARAMS ((struct type *, char *, GDB_FILE *, int, int));

  /* Print a value using syntax appropriate for this language. */

  int (*la_val_print) PARAMS ((struct type *, char *,  CORE_ADDR, GDB_FILE *,
			       int, int, int, enum val_prettyprint));

  /* Print a top-level value using syntax appropriate for this language. */

  int (*la_value_print) PARAMS ((struct value *, GDB_FILE *,
				 int, enum val_prettyprint));

  /* Base 2 (binary) formats. */

  struct language_format_info la_binary_format;

  /* Base 8 (octal) formats. */

  struct language_format_info la_octal_format;

  /* Base 10 (decimal) formats */

  struct language_format_info la_decimal_format;

  /* Base 16 (hexadecimal) formats */

  struct language_format_info la_hex_format;

  /* Table for printing expressions */

  const struct op_print *la_op_print_tab;

  /* Zero if the language has first-class arrays.  True if there are no
     array values, and array objects decay to pointers, as in C. */

  char c_style_arrays;

  /* Index to use for extracting the first element of a string. */
  char string_lower_bound;

  /* Type of elements of strings. */
  struct type **string_char_type;

  /* Add fields above this point, so the magic number is always last. */
  /* Magic number for compat checking */

  long la_magic;

};

#define LANG_MAGIC	910823L

/* Pointer to the language_defn for our current language.  This pointer
   always points to *some* valid struct; it can be used without checking
   it for validity.

   The current language affects expression parsing and evaluation
   (FIXME: it might be cleaner to make the evaluation-related stuff
   separate exp_opcodes for each different set of semantics.  We
   should at least think this through more clearly with respect to
   what happens if the language is changed between parsing and
   evaluation) and printing of things like types and arrays.  It does
   *not* affect symbol-reading-- each source file in a symbol-file has
   its own language and we should keep track of that regardless of the
   language when symbols are read.  If we want some manual setting for
   the language of symbol files (e.g. detecting when ".c" files are
   C++), it should be a seprate setting from the current_language.  */

extern const struct language_defn *current_language;

/* Pointer to the language_defn expected by the user, e.g. the language
   of main(), or the language we last mentioned in a message, or C.  */

extern const struct language_defn *expected_language;

/* language_mode == 
   language_mode_auto:   current_language automatically set upon selection
			 of scope (e.g. stack frame)
   language_mode_manual: current_language set only by user.  */

extern enum language_mode
  {language_mode_auto, language_mode_manual} language_mode;

/* These macros define the behaviour of the expression 
   evaluator.  */

/* Should we strictly type check expressions? */
#define STRICT_TYPE (type_check != type_check_off)

/* Should we range check values against the domain of their type? */
#define RANGE_CHECK (range_check != range_check_off)

/* "cast" really means conversion */
/* FIXME -- should be a setting in language_defn */
#define CAST_IS_CONVERSION (current_language->la_language == language_c  || \
			    current_language->la_language == language_cplus)

extern void
language_info PARAMS ((int));

extern void
set_language PARAMS ((enum language));


/* This page contains functions that return things that are
   specific to languages.  Each of these functions is based on
   the current setting of working_lang, which the user sets
   with the "set language" command. */

#define create_fundamental_type(objfile,typeid) \
  (current_language->la_fund_type(objfile, typeid))

#define LA_PRINT_TYPE(type,varstring,stream,show,level) \
  (current_language->la_print_type(type,varstring,stream,show,level))

#define LA_VAL_PRINT(type,valaddr,addr,stream,fmt,deref,recurse,pretty) \
  (current_language->la_val_print(type,valaddr,addr,stream,fmt,deref, \
				  recurse,pretty))
#define LA_VALUE_PRINT(val,stream,fmt,pretty) \
  (current_language->la_value_print(val,stream,fmt,pretty))

/* Return a format string for printf that will print a number in one of
   the local (language-specific) formats.  Result is static and is
   overwritten by the next call.  Takes printf options like "08" or "l"
   (to produce e.g. %08x or %lx).  */

#define local_binary_format() \
  (current_language->la_binary_format.la_format)
#define local_binary_format_prefix() \
  (current_language->la_binary_format.la_format_prefix)
#define local_binary_format_specifier() \
  (current_language->la_binary_format.la_format_specifier)
#define local_binary_format_suffix() \
  (current_language->la_binary_format.la_format_suffix)

#define local_octal_format() \
  (current_language->la_octal_format.la_format)
#define local_octal_format_prefix() \
  (current_language->la_octal_format.la_format_prefix)
#define local_octal_format_specifier() \
  (current_language->la_octal_format.la_format_specifier)
#define local_octal_format_suffix() \
  (current_language->la_octal_format.la_format_suffix)

#define local_decimal_format() \
  (current_language->la_decimal_format.la_format)
#define local_decimal_format_prefix() \
  (current_language->la_decimal_format.la_format_prefix)
#define local_decimal_format_specifier() \
  (current_language->la_decimal_format.la_format_specifier)
#define local_decimal_format_suffix() \
  (current_language->la_decimal_format.la_format_suffix)

#define local_hex_format() \
  (current_language->la_hex_format.la_format)
#define local_hex_format_prefix() \
  (current_language->la_hex_format.la_format_prefix)
#define local_hex_format_specifier() \
  (current_language->la_hex_format.la_format_specifier)
#define local_hex_format_suffix() \
  (current_language->la_hex_format.la_format_suffix)

#define LA_PRINT_CHAR(ch, stream) \
  (current_language->la_printchar(ch, stream))
#define LA_PRINT_STRING(stream, string, length, force_ellipses) \
  (current_language->la_printstr(stream, string, length, force_ellipses))

/* Test a character to decide whether it can be printed in literal form
   or needs to be printed in another representation.  For example,
   in C the literal form of the character with octal value 141 is 'a'
   and the "other representation" is '\141'.  The "other representation"
   is program language dependent. */

#define PRINT_LITERAL_FORM(c) \
  ((c)>=0x20 && ((c)<0x7F || (c)>=0xA0) && (!sevenbit_strings || (c)<0x80))

/* Return a format string for printf that will print a number in one of
   the local (language-specific) formats.  Result is static and is
   overwritten by the next call.  Takes printf options like "08" or "l"
   (to produce e.g. %08x or %lx).  */

extern char *
local_decimal_format_custom PARAMS ((char *));	/* language.c */

extern char *
local_octal_format_custom PARAMS ((char *));	/* language.c */

extern char *
local_hex_format_custom PARAMS ((char *));	/* language.c */

/* Return a string that contains a number formatted in one of the local
   (language-specific) formats.  Result is static and is overwritten by
   the next call.  Takes printf options like "08" or "l".  */

extern char *
local_hex_string PARAMS ((unsigned long));		/* language.c */

extern char *
local_hex_string_custom PARAMS ((unsigned long, char *)); /* language.c */

/* Type predicates */

extern int
simple_type PARAMS ((struct type *));

extern int
ordered_type PARAMS ((struct type *));

extern int
same_type PARAMS ((struct type *, struct type *));

extern int
integral_type PARAMS ((struct type *));

extern int
numeric_type PARAMS ((struct type *));

extern int
character_type PARAMS ((struct type *));

extern int
boolean_type PARAMS ((struct type *));

extern int
float_type PARAMS ((struct type *));

extern int
pointer_type PARAMS ((struct type *));

extern int
structured_type PARAMS ((struct type *));

/* Checks Binary and Unary operations for semantic type correctness */
/* FIXME:  Does not appear to be used */
#define unop_type_check(v,o) binop_type_check((v),NULL,(o))

extern void
binop_type_check PARAMS ((struct value *, struct value *, int));

/* Error messages */

extern void
op_error PARAMS ((char *fmt, enum exp_opcode, int));

#define type_op_error(f,o) \
   op_error((f),(o),type_check==type_check_on ? 1 : 0)
#define range_op_error(f,o) \
   op_error((f),(o),range_check==range_check_on ? 1 : 0)

extern void
type_error PARAMS ((char *, ...))
     ATTR_FORMAT(printf, 1, 2);

void
range_error PARAMS ((char *, ...))
     ATTR_FORMAT(printf, 1, 2);

/* Data:  Does this value represent "truth" to the current language?  */

extern int
value_true PARAMS ((struct value *));

extern struct type * lang_bool_type PARAMS ((void));

/* The type used for Boolean values in the current language. */
#define LA_BOOL_TYPE lang_bool_type ()

/* Misc:  The string representing a particular enum language.  */

extern const struct language_defn *
language_def PARAMS ((enum language));

extern char *
language_str PARAMS ((enum language));

/* Add a language to the set known by GDB (at initialization time).  */

extern void
add_language PARAMS ((const struct language_defn *));

extern enum language
get_frame_language PARAMS ((void));		/* In stack.c */

#endif	/* defined (LANGUAGE_H) */
