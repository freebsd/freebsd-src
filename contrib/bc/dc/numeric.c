/* 
 * interface dc to the bc numeric routines
 *
 * Copyright (C) 1994, 1997 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can either send email to this
 * program's author (see below) or write to: The Free Software Foundation,
 * Inc.; 675 Mass Ave. Cambridge, MA 02139, USA.
 */

/* This should be the only module that knows the internals of type dc_num */
/* In this particular implementation we just slather out some glue and
 * make use of bc's numeric routines.
 */

#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include "bcdefs.h"
#include "proto.h"
#include "global.h"
#include "dc.h"
#include "dc-proto.h"

/* there is no POSIX standard for dc, so we'll take the GNU definitions */
int std_only = FALSE;

/* convert an opaque dc_num into a real bc_num */
#define CastNum(x)	((bc_num)(x))

/* add two dc_nums, place into *result;
 * return DC_SUCCESS on success, DC_DOMAIN_ERROR on domain error
 */
int
dc_add DC_DECLARG((a, b, kscale, result))
	dc_num a DC_DECLSEP
	dc_num b DC_DECLSEP
	int kscale DC_DECLSEP
	dc_num *result DC_DECLEND
{
	init_num((bc_num *)result);
	bc_add(CastNum(a), CastNum(b), (bc_num *)result, 0);
	return DC_SUCCESS;
}

/* subtract two dc_nums, place into *result;
 * return DC_SUCCESS on success, DC_DOMAIN_ERROR on domain error
 */
int
dc_sub DC_DECLARG((a, b, kscale, result))
	dc_num a DC_DECLSEP
	dc_num b DC_DECLSEP
	int kscale DC_DECLSEP
	dc_num *result DC_DECLEND
{
	init_num((bc_num *)result);
	bc_sub(CastNum(a), CastNum(b), (bc_num *)result, 0);
	return DC_SUCCESS;
}

/* multiply two dc_nums, place into *result;
 * return DC_SUCCESS on success, DC_DOMAIN_ERROR on domain error
 */
int
dc_mul DC_DECLARG((a, b, kscale, result))
	dc_num a DC_DECLSEP
	dc_num b DC_DECLSEP
	int kscale DC_DECLSEP
	dc_num *result DC_DECLEND
{
	init_num((bc_num *)result);
	bc_multiply(CastNum(a), CastNum(b), (bc_num *)result, kscale);
	return DC_SUCCESS;
}

/* divide two dc_nums, place into *result;
 * return DC_SUCCESS on success, DC_DOMAIN_ERROR on domain error
 */
int
dc_div DC_DECLARG((a, b, kscale, result))
	dc_num a DC_DECLSEP
	dc_num b DC_DECLSEP
	int kscale DC_DECLSEP
	dc_num *result DC_DECLEND
{
	init_num((bc_num *)result);
	if (bc_divide(CastNum(a), CastNum(b), (bc_num *)result, kscale)){
		fprintf(stderr, "%s: divide by zero\n", progname);
		return DC_DOMAIN_ERROR;
	}
	return DC_SUCCESS;
}

/* divide two dc_nums, place quotient into *quotient and remainder
 * into *remainder;
 * return DC_SUCCESS on success, DC_DOMAIN_ERROR on domain error
 */
int
dc_divrem DC_DECLARG((a, b, kscale, quotient, remainder))
	dc_num a DC_DECLSEP
	dc_num b DC_DECLSEP
	int kscale DC_DECLSEP
	dc_num *quotient DC_DECLSEP
	dc_num *remainder DC_DECLEND
{
	init_num((bc_num *)quotient);
	init_num((bc_num *)remainder);
	if (bc_divmod(CastNum(a), CastNum(b),
						(bc_num *)quotient, (bc_num *)remainder, kscale)){
		fprintf(stderr, "%s: divide by zero\n", progname);
		return DC_DOMAIN_ERROR;
	}
	return DC_SUCCESS;
}

/* place the reminder of dividing a by b into *result;
 * return DC_SUCCESS on success, DC_DOMAIN_ERROR on domain error
 */
int
dc_rem DC_DECLARG((a, b, kscale, result))
	dc_num a DC_DECLSEP
	dc_num b DC_DECLSEP
	int kscale DC_DECLSEP
	dc_num *result DC_DECLEND
{
	init_num((bc_num *)result);
	if (bc_modulo(CastNum(a), CastNum(b), (bc_num *)result, kscale)){
		fprintf(stderr, "%s: remainder by zero\n", progname);
		return DC_DOMAIN_ERROR;
	}
	return DC_SUCCESS;
}

int
dc_modexp DC_DECLARG((base, expo, mod, kscale, result))
	dc_num base DC_DECLSEP
	dc_num expo DC_DECLSEP
	dc_num mod DC_DECLSEP
	int kscale DC_DECLSEP
	dc_num *result DC_DECLEND
{
	init_num((bc_num *)result);
	if (bc_raisemod(CastNum(base), CastNum(expo), CastNum(mod),
					(bc_num *)result, kscale)){
		if (is_zero(CastNum(mod)))
			fprintf(stderr, "%s: remainder by zero\n", progname);
		return DC_DOMAIN_ERROR;
	}
	return DC_SUCCESS;
}

/* place the result of exponentiationg a by b into *result;
 * return DC_SUCCESS on success, DC_DOMAIN_ERROR on domain error
 */
int
dc_exp DC_DECLARG((a, b, kscale, result))
	dc_num a DC_DECLSEP
	dc_num b DC_DECLSEP
	int kscale DC_DECLSEP
	dc_num *result DC_DECLEND
{
	init_num((bc_num *)result);
	bc_raise(CastNum(a), CastNum(b), (bc_num *)result, kscale);
	return DC_SUCCESS;
}

/* take the square root of the value, place into *result;
 * return DC_SUCCESS on success, DC_DOMAIN_ERROR on domain error
 */
int
dc_sqrt DC_DECLARG((value, kscale, result))
	dc_num value DC_DECLSEP
	int kscale DC_DECLSEP
	dc_num *result DC_DECLEND
{
	bc_num tmp;

	tmp = copy_num(CastNum(value));
	if (!bc_sqrt(&tmp, kscale)){
		fprintf(stderr, "%s: square root of negative number\n", progname);
		free_num(&tmp);
		return DC_DOMAIN_ERROR;
	}
	*((bc_num *)result) = tmp;
	return DC_SUCCESS;
}

/* compare dc_nums a and b;
 *  return a negative value if a < b;
 *  return a positive value if a > b;
 *  return zero value if a == b
 */
int
dc_compare DC_DECLARG((a, b))
	dc_num a DC_DECLSEP
	dc_num b DC_DECLEND
{
	return bc_compare(CastNum(a), CastNum(b));
}

/* attempt to convert a dc_num to its corresponding int value
 * If discard_flag is true then deallocate the value after use.
 */
int
dc_num2int DC_DECLARG((value, discard_flag))
	dc_num value DC_DECLSEP
	dc_boolean discard_flag DC_DECLEND
{
	long result;

	result = num2long(CastNum(value));
	if (discard_flag)
		dc_free_num(&value);
	return (int)result;
}

/* convert a C integer value into a dc_num */
/* For convenience of the caller, package the dc_num
 * into a dc_data result.
 */
dc_data
dc_int2data DC_DECLARG((value))
	int value DC_DECLEND
{
	dc_data result;

	init_num((bc_num *)&result.v.number);
	int2num((bc_num *)&result.v.number, value);
 	result.dc_type = DC_NUMBER;
	return result;
}

/* get a dc_num from some input stream;
 *  input is a function which knows how to read the desired input stream
 *  ibase is the input base (2<=ibase<=DC_IBASE_MAX)
 *  *readahead will be set to the readahead character consumed while
 *   looking for the end-of-number
 */
/* For convenience of the caller, package the dc_num
 * into a dc_data result.
 */
dc_data
dc_getnum DC_DECLARG((input, ibase, readahead))
	int (*input) DC_PROTO((void)) DC_DECLSEP
	int ibase DC_DECLSEP
	int *readahead DC_DECLEND
{
	bc_num	base;
	bc_num	result;
	bc_num	build;
	bc_num	tmp;
	bc_num	divisor;
	dc_data	full_result;
	int		negative = 0;
	int		digit;
	int		decimal;
	int		c;

	init_num(&tmp);
	init_num(&build);
	init_num(&base);
	result = copy_num(_zero_);
	int2num(&base, ibase);
	c = (*input)();
	while (isspace(c))
		c = (*input)();
	if (c == '_' || c == '-'){
		negative = c;
		c = (*input)();
	}else if (c == '+'){
		c = (*input)();
	}
	while (isspace(c))
		c = (*input)();
	for (;;){
		if (isdigit(c))
			digit = c - '0';
		else if ('A' <= c && c <= 'F')
			digit = 10 + c - 'A';
		else
			break;
		c = (*input)();
		int2num(&tmp, digit);
		bc_multiply(result, base, &result, 0);
		bc_add(result, tmp, &result, 0);
	}
	if (c == '.'){
		free_num(&build);
		free_num(&tmp);
		divisor = copy_num(_one_);
		build = copy_num(_zero_);
		decimal = 0;
		for (;;){
			c = (*input)();
			if (isdigit(c))
				digit = c - '0';
			else if ('A' <= c && c <= 'F')
				digit = 10 + c - 'A';
			else
				break;
			int2num(&tmp, digit);
			bc_multiply(build, base, &build, 0);
			bc_add(build, tmp, &build, 0);
			bc_multiply(divisor, base, &divisor, 0);
			++decimal;
		}
		bc_divide(build, divisor, &build, decimal);
		bc_add(result, build, &result, 0);
	}
	/* Final work. */
	if (negative)
		bc_sub(_zero_, result, &result, 0);

	free_num(&tmp);
	free_num(&build);
	free_num(&base);
	if (readahead)
		*readahead = c;
	full_result.v.number = (dc_num)result;
	full_result.dc_type = DC_NUMBER;
	return full_result;
}


/* return the "length" of the number */
int
dc_numlen DC_DECLARG((value))
	dc_num value DC_DECLEND
{
	bc_num num = CastNum(value);

	/* is this right??? */
	return num->n_len + num->n_scale;
}

/* return the scale factor of the passed dc_num
 * If discard_flag is true then deallocate the value after use.
 */
int
dc_tell_scale DC_DECLARG((value, discard_flag))
	dc_num value DC_DECLSEP
	dc_boolean discard_flag DC_DECLEND
{
	int kscale;

	kscale = CastNum(value)->n_scale;
	if (discard_flag)
		dc_free_num(&value);
	return kscale;
}


/* initialize the math subsystem */
void
dc_math_init DC_DECLVOID()
{
	init_numbers();
}

/* print out a dc_num in output base obase to stdout;
 * if newline is true, terminate output with a '\n';
 * if discard_flag is true then deallocate the value after use
 */
void
dc_out_num DC_DECLARG((value, obase, newline, discard_flag))
	dc_num value DC_DECLSEP
	int obase DC_DECLSEP
	dc_boolean newline DC_DECLSEP
	dc_boolean discard_flag DC_DECLEND
{
	out_num(CastNum(value), obase, out_char);
	if (newline)
		out_char('\n');
	if (discard_flag)
		dc_free_num(&value);
}


/* deallocate an instance of a dc_num */
void
dc_free_num DC_DECLARG((value))
	dc_num *value DC_DECLEND
{
	free_num((bc_num *)value);
}

/* return a duplicate of the number in the passed value */
/* The mismatched data types forces the caller to deal with
 * bad dc_type'd dc_data values, and makes it more convenient
 * for the caller to not have to do the grunge work of setting
 * up a dc_type result.
 */
dc_data
dc_dup_num DC_DECLARG((value))
	dc_num value DC_DECLEND
{
	dc_data result;

	++CastNum(value)->n_refs;
	result.v.number = value;
	result.dc_type = DC_NUMBER;
	return result;
}



/*---------------------------------------------------------------------------\
| The rest of this file consists of stubs for bc routines called by numeric.c|
| so as to minimize the amount of bc code needed to build dc.                |
| The bulk of the code was just lifted straight out of the bc source.        |
\---------------------------------------------------------------------------*/

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_STDARG_H
# include <stdarg.h>
#else
# include <varargs.h>
#endif


int out_col = 0;

/* Output routines: Write a character CH to the standard output.
   It keeps track of the number of characters output and may
   break the output with a "\<cr>". */

void
out_char (ch)
     char ch;
{

  if (ch == '\n')
    {
      out_col = 0;
      putchar ('\n');
    }
  else
    {
      out_col++;
      if (out_col == 70)
	{
	  putchar ('\\');
	  putchar ('\n');
	  out_col = 1;
	}
      putchar (ch);
    }
}

/* Malloc could not get enough memory. */

void
out_of_memory()
{
  dc_memfail();
}

/* Runtime error will  print a message and stop the machine. */

#ifdef HAVE_STDARG_H
#ifdef __STDC__
void
rt_error (char *mesg, ...)
#else
void
rt_error (mesg)
     char *mesg;
#endif
#else
void
rt_error (mesg, va_alist)
     char *mesg;
#endif
{
  va_list args;
  char error_mesg [255];

#ifdef HAVE_STDARG_H
  va_start (args, mesg);
#else
  va_start (args);
#endif
  vsprintf (error_mesg, mesg, args);
  va_end (args);
  
  fprintf (stderr, "Runtime error: %s\n", error_mesg);
}


/* A runtime warning tells of some action taken by the processor that
   may change the program execution but was not enough of a problem
   to stop the execution. */

#ifdef HAVE_STDARG_H
#ifdef __STDC__
void
rt_warn (char *mesg, ...)
#else
void
rt_warn (mesg)
     char *mesg;
#endif
#else
void
rt_warn (mesg, va_alist)
     char *mesg;
#endif
{
  va_list args;
  char error_mesg [255];

#ifdef HAVE_STDARG_H
  va_start (args, mesg);
#else
  va_start (args);
#endif
  vsprintf (error_mesg, mesg, args);
  va_end (args);

  fprintf (stderr, "Runtime warning: %s\n", error_mesg);
}
