/* atof_generic.c - turn a string of digits into a Flonum
   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.
   
   This file is part of GAS, the GNU Assembler.
   
   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef lint
static char rcsid[] = "$Id: atof-generic.c,v 1.2 1993/11/03 00:51:14 paul Exp $";
#endif

#include <ctype.h>
#include <string.h>

#include "as.h"

#ifdef __GNUC__
#define alloca __builtin_alloca
#else
#ifdef sparc
#include <alloca.h>
#endif
#endif

/* #define FALSE (0) */
/* #define TRUE  (1) */

/***********************************************************************\
 *									*
 *	Given a string of decimal digits , with optional decimal	*
 *	mark and optional decimal exponent (place value) of the		*
 *	lowest_order decimal digit: produce a floating point		*
 *	number. The number is 'generic' floating point: our		*
 *	caller will encode it for a specific machine architecture.	*
 *									*
 *	Assumptions							*
 *		uses base (radix) 2					*
 *		this machine uses 2's complement binary integers	*
 *		target flonums use "      "         "       "		*
 *		target flonums exponents fit in a long		*
 *									*
 \***********************************************************************/

/*
  
  Syntax:
  
  <flonum> ::= <optional-sign> <decimal-number> <optional-exponent>
  <optional-sign> ::= '+' | '-' | {empty}
  <decimal-number> ::= <integer>
  | <integer> <radix-character> 
  | <integer> <radix-character> <integer> 
  | <radix-character> <integer>

  <optional-exponent> ::= {empty}
  | <exponent-character> <optional-sign> <integer>

  <integer> ::= <digit> | <digit> <integer>
  <digit> ::= '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
  <exponent-character> ::= {one character from "string_of_decimal_exponent_marks"}
  <radix-character> ::= {one character from "string_of_decimal_marks"}
  
  */

int				/* 0 if OK */
    atof_generic (
		  address_of_string_pointer, /* return pointer to just
						AFTER number we read. */
		  string_of_decimal_marks, /* At most one per number. */
		  string_of_decimal_exponent_marks,
		  address_of_generic_floating_point_number)
char **address_of_string_pointer;
const char *string_of_decimal_marks;
const char *string_of_decimal_exponent_marks;
FLONUM_TYPE *address_of_generic_floating_point_number;
{
	int return_value; /* 0 means OK. */
	char * first_digit;
	/* char *last_digit; JF unused */
	int number_of_digits_before_decimal;
	int number_of_digits_after_decimal;
	long decimal_exponent;
	int number_of_digits_available;
	char digits_sign_char;
	
	/*
	 * Scan the input string, abstracting (1)digits (2)decimal mark (3) exponent.
	 * It would be simpler to modify the string, but we don't; just to be nice
	 * to caller.
	 * We need to know how many digits we have, so we can allocate space for
	 * the digits' value.
	 */
	
	char *p;
	char c;
	int seen_significant_digit;
	
	first_digit = *address_of_string_pointer;
	c = *first_digit;
	
	if (c == '-' || c == '+') {
		digits_sign_char = c;
		first_digit++;
	} else
	    digits_sign_char = '+';
	
	if ((first_digit[0] == 'n' || first_digit[0] == 'N')
	    && (first_digit[1] == 'a' || first_digit[1] == 'A')
	    && (first_digit[2] == 'n' || first_digit[2] == 'N')) {
		address_of_generic_floating_point_number->sign = 0;
		address_of_generic_floating_point_number->exponent = 0;
		address_of_generic_floating_point_number->leader =
		    address_of_generic_floating_point_number->low;
		*address_of_string_pointer = first_digit + 3;
		return(0);
	}
	
	/* 99e999 is a "special" token to some older, broken compilers.  */
	if ((first_digit[0] == 'i' || first_digit[0] == 'I') 
	     && (first_digit[1] == 'n' || first_digit[1] == 'N')
	     && (first_digit[2] == 'f' || first_digit[2] == 'F')) {
		address_of_generic_floating_point_number->sign =
		    digits_sign_char == '+' ? 'P' : 'N';
		address_of_generic_floating_point_number->exponent = 0;
		address_of_generic_floating_point_number->leader =
		    address_of_generic_floating_point_number->low;
		
		if ((first_digit[3] == 'i' || first_digit[3] == 'I')
		    && (first_digit[4] == 'n' || first_digit[4] == 'N')
		    && (first_digit[5] == 'i' || first_digit[5] == 'I')
		    && (first_digit[6] == 't' || first_digit[6] == 'T')
		    && (first_digit[7] == 'y' || first_digit[7] == 'Y')) {
			*address_of_string_pointer = first_digit + 8;
		} else {
			*address_of_string_pointer = first_digit + 3;
		}
		return(0);
	}
	
	if (strncmp(first_digit, "99e999", 6) == 0) {
		address_of_generic_floating_point_number->sign =
		    digits_sign_char == '+' ? 'P' : 'N';
		address_of_generic_floating_point_number->exponent = 0;
		address_of_generic_floating_point_number->leader =
		    address_of_generic_floating_point_number->low;
		*address_of_string_pointer = first_digit + 6;
		return(0);
	}

	number_of_digits_before_decimal = 0;
	number_of_digits_after_decimal = 0;
	decimal_exponent = 0;
	seen_significant_digit = 0;
	for (p = first_digit; (((c = * p) != '\0')
			       && (!c || ! strchr(string_of_decimal_marks, c))
			       && (!c || !strchr(string_of_decimal_exponent_marks, c)));
	     p++) {
		if (isdigit(c)) {
			if (seen_significant_digit || c > '0') {
				++number_of_digits_before_decimal;
				seen_significant_digit = 1;
			} else {
				first_digit++;
			}
		} else {
			break; /* p -> char after pre-decimal digits. */
		}
	} /* For each digit before decimal mark. */
	
#ifndef OLD_FLOAT_READS
	/* Ignore trailing 0's after the decimal point.  The original code here
	 * (ifdef'd out) does not do this, and numbers like
	 *	4.29496729600000000000e+09	(2**31)
	 * come out inexact for some reason related to length of the digit
	 * string.
	 */
	if (c && strchr(string_of_decimal_marks, c)) {
		int zeros = 0;	/* Length of current string of zeros */
		
		for (p++; (c = *p) && isdigit(c); p++) {
			if (c == '0') {
				zeros++;
			} else {
				number_of_digits_after_decimal += 1 + zeros;
				zeros = 0;
			}
		}
	}
#else
	if (c && strchr(string_of_decimal_marks, c)) {
		for (p++; (((c = *p) != '\0')
			   && (!c || !strchr(string_of_decimal_exponent_marks, c)));
		     p++) {
			if (isdigit(c)) {
				number_of_digits_after_decimal++; /* This may be retracted below. */
				if (/* seen_significant_digit || */ c > '0') {
					seen_significant_digit = TRUE;
				}
			} else {
				if (!seen_significant_digit) {
					number_of_digits_after_decimal = 0;
				}
				break;
			}
		} /* For each digit after decimal mark. */
	}
	
	while (number_of_digits_after_decimal && first_digit[number_of_digits_before_decimal
							     + number_of_digits_after_decimal] == '0')
	    --number_of_digits_after_decimal;
	/* last_digit = p; JF unused */
#endif
	
	if (c && strchr(string_of_decimal_exponent_marks, c) ) {
		char digits_exponent_sign_char;
		
		c = *++p;
		if (c && strchr ("+-",c)) {
			digits_exponent_sign_char = c;
			c = *++p;
		} else {
			digits_exponent_sign_char = '+';
		}
		
		for ( ; (c); c = *++p) {
			if (isdigit(c)) {
				decimal_exponent = decimal_exponent * 10 + c - '0';
				/*
				 * BUG! If we overflow here, we lose!
				 */
			} else {
				break;
			}
		}
		
		if (digits_exponent_sign_char == '-') {
			decimal_exponent = -decimal_exponent;
		}
	}
	
	*address_of_string_pointer = p;


	
	number_of_digits_available =
	    number_of_digits_before_decimal + number_of_digits_after_decimal;
	return_value = 0;
	if (number_of_digits_available == 0) {
		address_of_generic_floating_point_number->exponent = 0; /* Not strictly necessary */
		address_of_generic_floating_point_number->leader
		    = -1 + address_of_generic_floating_point_number->low;
		address_of_generic_floating_point_number->sign = digits_sign_char;
		/* We have just concocted (+/-)0.0E0 */

	} else {
		int count;	/* Number of useful digits left to scan. */
		
		LITTLENUM_TYPE *digits_binary_low;
		int precision;
		int maximum_useful_digits;
		int number_of_digits_to_use;
		int more_than_enough_bits_for_digits;
		int more_than_enough_littlenums_for_digits;
		int size_of_digits_in_littlenums;
		int size_of_digits_in_chars;
		FLONUM_TYPE power_of_10_flonum;
		FLONUM_TYPE digits_flonum;
		
		precision = (address_of_generic_floating_point_number->high
			     - address_of_generic_floating_point_number->low
			     + 1); /* Number of destination littlenums. */
		
		/* Includes guard bits (two littlenums worth) */
		maximum_useful_digits = (((double) (precision - 2))
					 * ((double) (LITTLENUM_NUMBER_OF_BITS))
					 / (LOG_TO_BASE_2_OF_10))
		    + 2; /* 2 :: guard digits. */
		
		if (number_of_digits_available > maximum_useful_digits) {
			number_of_digits_to_use = maximum_useful_digits;
		} else {
			number_of_digits_to_use = number_of_digits_available;
		}
		
		decimal_exponent += number_of_digits_before_decimal - number_of_digits_to_use;
		
		more_than_enough_bits_for_digits
		    = ((((double)number_of_digits_to_use) * LOG_TO_BASE_2_OF_10) + 1);
		
		more_than_enough_littlenums_for_digits
		    = (more_than_enough_bits_for_digits
		       / LITTLENUM_NUMBER_OF_BITS)
			+ 2;
		
		/*
		 * Compute (digits) part. In "12.34E56" this is the "1234" part.
		 * Arithmetic is exact here. If no digits are supplied then
		 * this part is a 0 valued binary integer.
		 * Allocate room to build up the binary number as littlenums.
		 * We want this memory to disappear when we leave this function.
		 * Assume no alignment problems => (room for n objects) ==
		 * n * (room for 1 object).
		 */
		
		size_of_digits_in_littlenums = more_than_enough_littlenums_for_digits;
		size_of_digits_in_chars = size_of_digits_in_littlenums
		    * sizeof(LITTLENUM_TYPE);
		
		digits_binary_low = (LITTLENUM_TYPE *)
		    alloca(size_of_digits_in_chars);
		
		memset((char *)digits_binary_low, '\0', size_of_digits_in_chars);
		
		/* Digits_binary_low[] is allocated and zeroed. */
		
		/*
		 * Parse the decimal digits as if * digits_low was in the units position.
		 * Emit a binary number into digits_binary_low[].
		 *
		 * Use a large-precision version of:
		 * (((1st-digit) * 10 + 2nd-digit) * 10 + 3rd-digit ...) * 10 + last-digit
		 */
		
		for (p = first_digit, count = number_of_digits_to_use; count; p++,  --count) {
			c = *p;
			if (isdigit(c)) {
				/*
				 * Multiply by 10. Assume can never overflow.
				 * Add this digit to digits_binary_low[].
				 */
				
				long carry;
				LITTLENUM_TYPE *littlenum_pointer;
				LITTLENUM_TYPE *littlenum_limit;
				
				littlenum_limit = digits_binary_low
				    + more_than_enough_littlenums_for_digits
					- 1;

				carry = c - '0'; /* char -> binary */

				for (littlenum_pointer = digits_binary_low;
				     littlenum_pointer <= littlenum_limit;
				     littlenum_pointer++) {
					long work;
					
					work = carry + 10 * (long) (*littlenum_pointer);
					*littlenum_pointer = work & LITTLENUM_MASK;
					carry = work >> LITTLENUM_NUMBER_OF_BITS;
				}

				if (carry != 0) {
					/*
					 * We have a GROSS internal error.
					 * This should never happen.
					 */
					as_fatal("failed sanity check.");	/* RMS prefers abort() to any message. */
				}
			} else {
				++ count;	/* '.' doesn't alter digits used count. */
			} /* if valid digit */
		} /* for each digit */
		
		
		/*
		 * Digits_binary_low[] properly encodes the value of the digits.
		 * Forget about any high-order littlenums that are 0.
		 */
		while (digits_binary_low[size_of_digits_in_littlenums - 1] == 0
		       && size_of_digits_in_littlenums >= 2)
		    size_of_digits_in_littlenums--;
		
		digits_flonum.low	= digits_binary_low;
		digits_flonum.high	= digits_binary_low + size_of_digits_in_littlenums - 1;
		digits_flonum.leader	= digits_flonum.high;
		digits_flonum.exponent = 0;
		/*
		 * The value of digits_flonum.sign should not be important.
		 * We have already decided the output's sign.
		 * We trust that the sign won't influence the other parts of the number!
		 * So we give it a value for these reasons:
		 * (1) courtesy to humans reading/debugging
		 *     these numbers so they don't get excited about strange values
		 * (2) in future there may be more meaning attached to sign,
		 *     and what was
		 *     harmless noise may become disruptive, ill-conditioned (or worse)
		 *     input.
		 */
		digits_flonum.sign = '+';
		
		{
			/*
			 * Compute the mantssa (& exponent) of the power of 10.
			 * If sucessful, then multiply the power of 10 by the digits
			 * giving return_binary_mantissa and return_binary_exponent.
			 */
			
			LITTLENUM_TYPE *power_binary_low;
			int decimal_exponent_is_negative;
			/* This refers to the "-56" in "12.34E-56". */
			/* FALSE: decimal_exponent is positive (or 0) */
			/* TRUE:  decimal_exponent is negative */
			FLONUM_TYPE temporary_flonum;
			LITTLENUM_TYPE *temporary_binary_low;
			int size_of_power_in_littlenums;
			int size_of_power_in_chars;
			
			size_of_power_in_littlenums = precision;
			/* Precision has a built-in fudge factor so we get a few guard bits. */
			
			decimal_exponent_is_negative = decimal_exponent < 0;
			if (decimal_exponent_is_negative) {
				decimal_exponent = -decimal_exponent;
			}

			/* From now on: the decimal exponent is > 0. Its sign is seperate. */
			
			size_of_power_in_chars = size_of_power_in_littlenums
			    * sizeof(LITTLENUM_TYPE) + 2;

			power_binary_low = (LITTLENUM_TYPE *) alloca(size_of_power_in_chars);
			temporary_binary_low = (LITTLENUM_TYPE *) alloca(size_of_power_in_chars);
			memset((char *)power_binary_low, '\0', size_of_power_in_chars);
			* power_binary_low = 1;
			power_of_10_flonum.exponent = 0;
			power_of_10_flonum.low = power_binary_low;
			power_of_10_flonum.leader = power_binary_low;
			power_of_10_flonum.high	= power_binary_low + size_of_power_in_littlenums - 1;
			power_of_10_flonum.sign = '+';
			temporary_flonum.low = temporary_binary_low;
			temporary_flonum.high = temporary_binary_low + size_of_power_in_littlenums - 1;
			/*
			 * (power) == 1.
			 * Space for temporary_flonum allocated.
			 */
			
			/*
			 * ...
			 *
			 * WHILE	more bits
			 * DO	find next bit (with place value)
			 *	multiply into power mantissa
			 * OD
			 */
			{
				int place_number_limit;
				/* Any 10^(2^n) whose "n" exceeds this */
				/* value will fall off the end of */
				/* flonum_XXXX_powers_of_ten[]. */
				int place_number;
				const FLONUM_TYPE *multiplicand; /* -> 10^(2^n) */
				
				place_number_limit = table_size_of_flonum_powers_of_ten;

				multiplicand = (decimal_exponent_is_negative
						? flonum_negative_powers_of_ten
						: flonum_positive_powers_of_ten);

				for (place_number = 1;	/* Place value of this bit of exponent. */
				     decimal_exponent;	/* Quit when no more 1 bits in exponent. */
				     decimal_exponent >>= 1, place_number++) {
					if (decimal_exponent & 1) {
						if (place_number > place_number_limit) {
							/*
							 * The decimal exponent has a magnitude so great that
							 * our tables can't help us fragment it.  Although this
							 * routine is in error because it can't imagine a
							 * number that big, signal an error as if it is the
							 * user's fault for presenting such a big number.
							 */
							return_value = ERROR_EXPONENT_OVERFLOW;
							/*
							 * quit out of loop gracefully
							 */
							decimal_exponent = 0;
						} else {
#ifdef TRACE
							printf("before multiply, place_number = %d., power_of_10_flonum:\n",
							       place_number);

							flonum_print(&power_of_10_flonum);
							(void)putchar('\n');
#endif
							flonum_multip(multiplicand + place_number,
								      &power_of_10_flonum, &temporary_flonum);
							flonum_copy(&temporary_flonum, &power_of_10_flonum);
						} /* If this bit of decimal_exponent was computable.*/
					} /* If this bit of decimal_exponent was set. */
				} /* For each bit of binary representation of exponent */
#ifdef TRACE
				printf(" after computing power_of_10_flonum: ");
				flonum_print(&power_of_10_flonum );
				(void) putchar('\n');
#endif
			}
			
		}
		
		/*
		 * power_of_10_flonum is power of ten in binary (mantissa) , (exponent).
		 * It may be the number 1, in which case we don't NEED to multiply.
		 *
		 * Multiply (decimal digits) by power_of_10_flonum.
		 */
		
		flonum_multip(&power_of_10_flonum, &digits_flonum, address_of_generic_floating_point_number);
		/* Assert sign of the number we made is '+'. */
		address_of_generic_floating_point_number->sign = digits_sign_char;
		
	} /* If we had any significant digits. */
	return(return_value);
} /* atof_generic () */

/* end of atof_generic.c */
