//    This is part of the iostream library, providing input/output for C++.
//    Copyright (C) 1992 Per Bothner.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Library General Public License for more details.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "ioprivate.h"

// Format floating-point number and print them.
// Return number of chars printed, or EOF on error.

// sign_mode == '+' : print "-" or "+"
// sign_mode == ' ' : print "-" or " "
// sign_mode == '\0' : print "-' or ""

int __outfloat(double value, streambuf *sb, char type,
	       int width, int precision, ios::fmtflags flags,
	       char sign_mode, char fill)
{
    int count = 0;
#define PUT(x) do {if (sb->sputc(x) < 0) goto error; count++;} while (0)
#define PUTN(p, n) \
  do {int _n=n; count+=_n; if (sb->sputn(p,_n) != _n) goto error;} while(0)
#define PADN(fill, n) \
  do {int _n = n; count+=_n; if (sb->padn(fill, _n) < 0) goto error;} while (0)
    ios::fmtflags pad_kind = flags & (ios::left|ios::right|ios::internal);
    int skip_zeroes = 0;
    int show_dot = (flags & ios::showpoint) != 0;
    int decpt;
    int sign;
    int mode;
#define EBUF_SIZE 12
#define EBUF_END &ebuf[EBUF_SIZE]
    char ebuf[EBUF_SIZE];
    char *end;
    int exp = 0;
    switch (type) {
      case 'f':
	mode = 3;
	break;
      case 'e':
      case 'E':
	exp = type;
	mode = 2;
	if (precision != 999)
	  precision++;  // Add one to include digit before decimal point.
	break;
      case 'g':
      case 'G':
	exp = type == 'g' ? 'e' : 'E';
	if (precision == 0) precision = 1;
	if (!(flags & ios::showpoint))
	    skip_zeroes = 1;
	type = 'g';
	mode = 2;
	break;
    }
    /* Do the actual convension */
    if (precision == 999 && mode != 3)
      mode = 0;
    char *p = dtoa(value, mode, precision, &decpt, &sign, &end);
    register int i;
    int useful_digits = end-p;
    char *exponent_start = EBUF_END;
    if (mode == 0)
      precision = useful_digits;
    // Check if we need to emit an exponent.
    if (mode != 3 && decpt != 9999) {
	i = decpt - 1;
	if ((type != 'g' && type != 'F') || i < -4 || i >= precision) {
	    // Print the exponent into ebuf.
	    // We write ebuf in reverse order (right-to-left).
	    char sign;
	    if (i >= 0)
		sign = '+';
	    else
		sign = '-', i = -i;
	    /* Note: ANSI requires at least 2 exponent digits. */
	    do {
		*--exponent_start = (i % 10) + '0';
		i /= 10;
	    } while (i >= 10);
	    *--exponent_start = i + '0';
	    *--exponent_start = sign;
	    *--exponent_start = exp;
	}
    }
    int exponent_size = EBUF_END - exponent_start;
    if (mode == 1)
	precision = 1;
    /* If we print an exponent, always show just one digit before point. */
    if (exponent_size)
	decpt = 1;
    if (decpt == 9999) { // Infinity or NaN
	decpt = useful_digits;
	precision = 0;
	show_dot = 0;
    }

   // dtoa truncates trailing zeroes.  Set the variable trailing_zeroes to
   // the number of 0's we have to add (after the decimal point).
   int trailing_zeroes = 0;
   if (skip_zeroes)
       trailing_zeroes = 0;
   else if (type == 'f')
       trailing_zeroes = useful_digits <= decpt ? precision
	   : precision-(useful_digits-decpt);
   else if (exponent_size) // 'e' 'E' or 'g' format using exponential notation.
       trailing_zeroes = precision - useful_digits;
   else // 'g' format not using exponential notation.
       trailing_zeroes = useful_digits <= decpt ? precision - decpt
	   : precision-useful_digits;
    if (trailing_zeroes < 0) trailing_zeroes = 0;

    if (trailing_zeroes != 0 || useful_digits > decpt)
	show_dot = 1;
    int print_sign;
    if (sign_mode == 0)
	print_sign = sign ? '-' : 0;
    else if (sign_mode == '+')
	print_sign = sign ? '-' : '+';
    else /* if (sign_mode == ' ') */
	print_sign = sign ? '-' : ' ';
    
    // Calculate the width (before padding).
    int unpadded_width =
	(print_sign != 0) + trailing_zeroes + exponent_size + show_dot
	    + useful_digits
	    + (decpt > useful_digits ? decpt - useful_digits
	       : decpt > 0 ? 0 : 1 - decpt);

    int padding = width > unpadded_width ? width - unpadded_width : 0;
    if (padding > 0
	&& pad_kind != (ios::fmtflags)ios::left
	&& pad_kind != (ios::fmtflags)ios::internal) // Default (right) adjust.
	PADN(fill, padding);
    if (print_sign)
	PUT(print_sign);
    if (pad_kind == (ios::fmtflags)ios::internal && padding > 0)
	PADN(fill, padding);
    if (decpt > 0) {
	if (useful_digits >= decpt)
	    PUTN(p, decpt);
	else {
	    PUTN(p, useful_digits);
	    PADN('0', decpt-useful_digits);
	}
	if (show_dot) {
	    PUT('.');
	    // Print digits after the decimal point.
	    if (useful_digits > decpt)
		PUTN(p + decpt, useful_digits-decpt);
	}
    }
    else {
	PUT('0');
	if (show_dot) {
	    PUT('.');
	    PADN('0', -decpt);
	    // Print digits after the decimal point.
	    PUTN(p, useful_digits);
	}
    }
    PADN('0', trailing_zeroes);
    if (exponent_size)
	PUTN(exponent_start, exponent_size);
    if (pad_kind == (ios::fmtflags)ios::left && padding > 0) // Left adjustment
	PADN(fill, padding);
    return count;
  error:
    return EOF;
}
