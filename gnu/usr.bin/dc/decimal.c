/* 
 * Arbitrary precision decimal arithmetic.
 *
 * Copyright (C) 1984 Free Software Foundation, Inc.
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

/* Some known problems:

	Another problem with decimal_div is found when you try to
	divide a number with > scale fraction digits by 1.  The
	expected result is simply truncation, but all sorts of things
	happen instead.  An example is that the result of .99999998/1
	with scale set to 6 is .000001
		
	There are some problems in the behavior of the decimal package
	related to printing and parsing.  The
	printer is weird about very large output radices, tending to want
	to output single ASCII characters for any and all digits (even
	in radices > 127).  The UNIX bc approach is to print digit groups
	separated by spaces.  There is a rather overwrought workaround in
	the function decputc() in bcmisc.c, but it would be better if
	decimal.c got a fix for this.  */

/* For stand-alone testing, compile with -DTEST.
   This DTESTable feature defines a `main' function
   which is a simple loop that accepts input of the form
   number space op space number newline
   where op is +, -, *, /, %, p or r,
   and performs the operation and prints the operands and result.
   `p' means print the first number in the radix spec'd by the second.
   `r' means read the first one in the radix specified by the second
   (and print the result in decimal).
   Divide in this test keeps three fraction digits. */

#include "decimal.h"

#define MAX(a, b) (((a) > (b) ? (a) : (b)))

/* Some constant decimal numbers */

struct decimal decimal_zero = {0, 0, 0, 0, 0};

struct decimal decimal_one = {0, 0, 1, 0, 1};

/*** Assumes RADIX is even ***/
struct decimal decimal_half = {0, 1, 0, 0, RADIX / 2};

decimal static decimal_add1 (), decimal_sub1 ();
static void add_scaled ();
static int subtract_scaled ();

/* Create and return a decimal number that has `before' digits before
   the decimal point and `after' digits after.  The digits themselves are
   initialized to zero.  */

decimal
make_decimal (before, after)
     int before, after;
{
  decimal result;
  if (before >= 1<<16)
    {
      decimal_error ("%d too many decimal digits", before);
      return 0;
    }
  if (after >= 1<<15)
    {
      decimal_error ("%d too many decimal digits", after);
      return 0;
    }
  result = (decimal) malloc (sizeof (struct decimal) + before + after - 1);
  result->sign = 0;
  result->before = before;
  result->after = after;
  result->refcnt = 0;
  bzero (result->contents, before + after);
  return result;
}

/* Create a copy of the decimal number `b' and return it.  */

decimal
decimal_copy (b)
     decimal b;
{
  decimal result = make_decimal (b->before, b->after);
  bcopy (b->contents, result->contents, LENGTH(b));
  result->sign = b->sign;
  return result;
}

/* Copy a decimal number `b' but extend or truncate to exactly
   `digits' fraction digits. */

static decimal
decimal_copy_1 (b, digits)
     decimal b;
     int digits;
{
  if (digits > b->after)
    {
      decimal result = make_decimal (b->before, digits);
      bcopy (b->contents, result->contents + (digits - (int) b->after), LENGTH(b));
      return result;
    }
  else
    return decimal_round_digits (b, digits);
}

/* flush specified number `digits' of trailing fraction digits,
   and flush any trailing fraction zero digits exposed after they are gone.
   The number `b' is actually modified; no new storage is allocated.
   That is why this is not global.  */

static void
flush_trailing_digits (b, digits)
     decimal b;
     int digits;
{
  int flush = digits;
  int maxdig = b->after;

  while (flush < maxdig && !b->contents [flush])
    flush++;

  if (flush)
    {
      int i;

      b->after -= flush;
      for (i = 0; i < LENGTH (b); i++)
	b->contents[i] = b->contents[flush + i];
    }

}

/* Return nonzero integer if the value of decimal number `b' is zero.  */

int
decimal_zerop (b)
     decimal b;
{
  return !LENGTH(b);
}

/* Compare two decimal numbers arithmetically.
   The value is < 0 if b1 < b2, > 0 if b1 > b2, 0 if b1 = b2.
   This is the same way that `strcmp' reports the result of comparing
   strings.  */ 

int
decimal_compare (b1, b2)
     decimal b1, b2;
{
  int l1, l2;
  char *p1, *p2, *s1, *s2;
  int i;

  /* If signs differ, deduce result from the signs */

  if (b2->sign && !b1->sign) return 1;
  if (b1->sign && !b2->sign) return -1;

  /* If same sign but number of nonfraction digits differs,
     the one with more of them is farther from zero.  */

  if (b1->before != b2->before)
    if (b1->sign)
      return (int) (b2->before - b1->before);
    else
      return (int) (b1->before - b2->before);

  /* Else compare the numbers digit by digit from high end */
  l1 = LENGTH(b1);
  l2 = LENGTH(b2);  
  s1 = b1->contents;		/* Start of number -- don't back up digit pointer past here */
  s2 = b2->contents;
  p1 = b1->contents + l1;	/* Scanning pointer, for fetching digits.  */
  p2 = b2->contents + l2;
  for (i = MAX(l1, l2); i >= 0; i--)
    {
      int r = ((p1 != s1) ? *--p1 : 0) - ((p2 != s2) ? *--p2 : 0);
      if (r)
	return b1->sign ? -r : r;
    }
  return 0;
}

/* Return the number of digits stored in decimal number `b' */

int
decimal_length (b)
     decimal b;
{
  return LENGTH(b);
}

/* Return the number of fraction digits stored in decimal number `b'.  */

int
decimal_after (b)
     decimal b;
{
  return b->after;
}

/* Round decimal number `b' to have only `digits' fraction digits.
   Result is rounded to nearest unit in the last remaining digit.
   Return the result, another decimal number.  */

decimal
decimal_round_digits (b, digits)
     decimal b;
     int digits;
{
  decimal result;
  int old;

  if (b->after <= digits) return decimal_copy (b);

  if (digits < 0)
    {
      decimal_error ("request to keep negative number of digits %d", digits);
      return decimal_copy (b);
    }

  result = make_decimal (b->before + 1, b->after);
  result->sign = b->sign;
  bcopy (b->contents, result->contents, LENGTH(b));

  old = result->after;

  /* Add .5 * last place to keep, so that we round rather than truncate */
  /* Note this ignores sign of result, so if result is negative
     it is subtracting */

  add_scaled (result, DECIMAL_HALF, 1, old - digits - 1);

  /* Flush desired digits, and any trailing zeros exposed by them.  */

  flush_trailing_digits (result, old - digits);

  /* Flush leading digits -- always is one, unless was a carry into it */

  while (result->before > 0
	 && result->contents[LENGTH(result) - 1] == 0)
    result->before--;

  return result;
}

/* Truncate decimal number `b' to have only `digits' fraction digits.
   Any fraction digits in `b' beyond that are dropped and ignored.
   Truncation is toward zero.
   Return the result, another decimal number.  */

decimal
decimal_trunc_digits (b, digits)
     decimal b;
     int digits;
{
  decimal result = decimal_copy (b);
  int old = result->after;

  if (old <= digits) return result;

  if (digits < 0)
    {
      decimal_error ("request to keep negative number of digits %d", digits);
      return result;
    }

  flush_trailing_digits (result, old - digits);

  return result;
}

/* Return the fractional part of decimal number `b':
   that is, `b' - decimal_trunc_digits (`b') */

decimal
decimal_fraction (b)
     decimal b;
{
  decimal result = make_decimal (0, b->after);
  bcopy (b->contents, result->contents, b->after);
  return result;
}

/* return an integer whose value is that of decimal `b', sans its fraction.  */

int
decimal_to_int (b)
     decimal b;
{
  int result = 0;
  int i;
  int end = b->after;

  for (i = LENGTH(b) - 1; i >= end; i--)
    {
      result *= RADIX;
      result += b->contents[i];
    }
  return result;
}

/* return a decimal whose value is the integer i.  */

decimal
decimal_from_int (i)
     int i;
{
  int log, tem;
  decimal result;

  for (log = 0, tem = (i > 0 ? i : - i); tem; log++, tem /= RADIX);

  result = make_decimal (log, 0);

  for (log = 0, tem = (i > 0 ? i : - i); tem; log++, tem /= RADIX)
    result->contents[log] = tem % RADIX;

  if (i < 0) result->sign = 1;
  return result;
}

/* Return (as an integer) the result of dividing decimal number `b' by
   integer `divisor'. 
   This is used in printing decimal numbers in other radices. */

int
decimal_int_rem (b, divisor)
     decimal b;
     int divisor;
{
  int len = LENGTH(b);
  int end = b->after;
  int accum = 0;
  int i;

  for (i = len - 1; i >= end; i--)
    {
      accum %= divisor;
      accum *= RADIX;
      accum += b->contents[i];
    }
  return accum % divisor;
}

/* Convert digit `digit' to a character and output it by calling
   `charout' with it as arg. */

static void
print_digit (digit, charout)
     int digit;
     void (*charout) ();
{
  if (digit < 10)
    charout ('0' + digit);
  else
    charout ('A' + digit - 10);
}

/* print decimal number `b' in radix `radix', assuming it is an integer.
   `r' is `radix' expressed as a decimal number. */

static
decimal_print_1 (b, r, radix, charout)
     decimal b, r;
     int radix;
     void (*charout) ();
{
  int digit = decimal_int_rem (b, radix);
  decimal rest = decimal_div (b, r, 0);

  if (!decimal_zerop (rest))
    decimal_print_1 (rest, r, radix, charout);

  print_digit (digit, charout);

  free (rest);
}

/* User entry: print decimal number `b' in radix `radix' (an integer),
   outputting characters by calling `charout'.  */

void
decimal_print (b, charout, radix)
     decimal b;
     void (*charout) ();
     int radix;
{
  if (b->sign) charout ('-');

  if (radix == RADIX)
    {
      /* decimal output => just print the digits, inserting a point in
	 the proper place.  */ 
      int i;
      int before = b->before;
      int len = before + b->after;
      for (i = 0; i < len; i++)
	{
	  if (i == before) charout ('.');
	  /* Broken if RADIX /= 10
	     charout ('0' + b->contents [len - 1 - i]); */
	  print_digit (b->contents [len - 1 - i], charout);
	}
      if (!len)
	charout ('0');
    }
  else
    {
      /* nonstandard radix: must use multiply and divide to determine the
	 digits of the number in that radix.  */

      int i;
      extern double log10 ();
      /* Compute the number of fraction digits we want to have in the
         new radix.  They should contain the same amount of
         information as the decimal digits we have.  */
      int nfrac = (b->after / log10 ((double) radix) + .99);
      decimal r = decimal_from_int (radix);
      decimal intpart = decimal_trunc_digits (b, 0);

      /* print integer part */
      decimal_print_1 (intpart, r, radix, charout);
      free (intpart);

      /* print fraction part */
      if (nfrac)
	{
          decimal tem1, tem2;
	  tem1 = decimal_fraction (b);
	  charout ('.');
	  /* repeatedly multiply by `radix', print integer part as one digit,
	     and flush the integer part.  */
	  for (i = 0; i < nfrac; i++)
	    {
	      tem2 = decimal_mul (tem1, r);
	      free (tem1);
	      print_digit (decimal_to_int (tem2), charout);
	      tem1 = decimal_fraction (tem2);
	      free (tem2);
	    }
	  free (tem1);
	}
      free (r);
    }
}

static int
decode_digit (digitchar)
     char digitchar;
{
  if ('0' <= digitchar && digitchar <= '9')
    return digitchar - '0';
  if ('a' <= digitchar && digitchar <= 'z')
    return digitchar - 'a' + 10;
  if ('A' <= digitchar && digitchar <= 'Z')
    return digitchar - 'A' + 10;
  return -1;
}

/* Parse string `s' into a number using radix `radix'
   and return result as a decimal number.  */

decimal
decimal_parse (s, radix)
     char *s;
     int radix;
{
  int i, len, before = -1;
  char *p;
  char c;
  decimal result;
  int negative = 0;
  int excess_digit = 0;

  if (*s == '-')
    {
      s++;
      negative = 1;
    }

  /* First scan for valid characters.
     Count total num digits, and count num before the decimal point.  */

  p = s;
  i = 0;
  while (c = *p++)
    {
      if (c == '.')
        {
	  if (before >= 0)
	    decimal_error ("two decimal points in %s", s);
          before = i;
	}
      else if (c == '0' && !i && before < 0)
	s++;   /* Discard leading zeros */
      else if (decode_digit (c) >= 0)
	{
	  i++;
	  if (decode_digit (c) > RADIX)
	    excess_digit = 1;
	}
      else
	decimal_error ("invalid number %s", s);
    }

  len = i;
  if (before < 0) before = i;

  p = s;

  /* Now parse those digits */

  if (radix != RADIX || excess_digit)
    {
      decimal r = decimal_from_int (radix);
      extern double log10 ();
      int digits = (len - before) * log10 ((double) radix) + .99;
      result = decimal_copy (DECIMAL_ZERO);

      /* Parse all the digits into an integer, ignoring decimal point,
	 by multiplying by `radix'.  */

      while (i > 0 && (c = *p++))
	{
	  if (c != '.')
	    {
	      decimal newdig = decimal_from_int (decode_digit (c));
	      decimal prod = decimal_mul (result, r);
	      decimal newresult = decimal_add (newdig, prod);

	      free (newdig);  free (prod);  free (result);
	      result = newresult;
	      i--;
	    }
	}

      /* Now put decimal point in right place
	 by dividing by `radix' once for each digit
	 that really should have followed the decimal point.  */

      for (i = before; i < len; i++)
	{
	  decimal newresult = decimal_div (result, r, digits);
	  free (result);
	  result = newresult;
	}
      free (r);
    }
  else
    {
      /* radix is standard - just copy the digits into a decimal number.  */

      int tem;
      result = make_decimal (before, len - before);

      while (i > 0 && (c = *p++))
	{
	  if ((c != '.') &&
	      ((tem = decode_digit (c)) >= 0))
	    result->contents [--i] = tem;
	}
    }

  if (negative) result->sign = 1;
  flush_trailing_digits (result, 0);
  return result;
}

/* Add b1 and b2, considering their signs */

decimal
decimal_add (b1, b2)
     decimal b1, b2;
{
  decimal v;

  if (b1->sign != b2->sign)
    v = decimal_sub1 (b1, b2);
  else
    v = decimal_add1 (b1, b2);
  if (b1->sign && !decimal_zerop (v))
    v->sign = !v->sign;
  return v;
}

/* Add b1 and minus b2, considering their signs */

decimal
decimal_sub (b1, b2)
     decimal b1, b2;
{
  decimal v;

  if (b1->sign != b2->sign)
    v = decimal_add1 (b1, b2);
  else
    v = decimal_sub1 (b1, b2);
  if (b1->sign && !decimal_zerop (v))
    v->sign = !v->sign;
  return v;
}

/* Return the negation of b2.  */

decimal
decimal_neg (b2)
     decimal b2;
{
  decimal v = decimal_copy (b2);

  if (!decimal_zerop (v))
    v->sign = !v->sign;
  return v;
}

/* add magnitudes of b1 and b2, ignoring their signs. */

static decimal
decimal_add1 (b1, b2)
     decimal b1, b2;
{
  int before = MAX (b1->before, b2->before);
  int after = MAX (b1->after, b2->after);

  int len = before+after+1;
  decimal result = make_decimal (before+1, after);

  int i;
  char *s1 = b1->contents;
  char *s2 = b2->contents;
  char *p1 = s1 + b1->after - after;
  char *p2 = s2 + b2->after - after;
  char *e1 = s1 + b1->before + b1->after;
  char *e2 = s2 + b2->before + b2->after;
  char *pr = result->contents;
  int accum = 0;

  for (i = 0; i < len; i++, p1++, p2++)
    {
      accum /= RADIX;
      if (p1 >= s1 && p1 < e1) accum += *p1;
      if (p2 >= s2 && p2 < e2) accum += *p2;
      *pr++ = accum % RADIX;
    }
  if (!accum)
    (result->before)--;

  flush_trailing_digits (result, 0);

  return result;
}

/* subtract magnitude of b2 from that or b1, returning signed decimal
   number. */ 

static decimal
decimal_sub1 (b1, b2)
     decimal b1, b2;
{
  int before = MAX (b1->before, b2->before);
  int after = MAX (b1->after, b2->after);

  int len = before+after;
  decimal result = make_decimal (before, after);

  int i;
  char *s1 = b1->contents;
  char *s2 = b2->contents;
  char *p1 = s1 + b1->after - after;
  char *p2 = s2 + b2->after - after;
  char *e1 = s1 + b1->before + b1->after;
  char *e2 = s2 + b2->before + b2->after;
  char *pr = result->contents;
  int accum = 0;

  for (i = 0; i < len; i++, p1++, p2++)
    {
      if (p1 >= s1 && p1 < e1) accum += *p1;
      if (p2 >= s2 && p2 < e2) accum -= *p2;
      if (accum < 0 && accum % RADIX)
        *pr = RADIX - (- accum) % RADIX;
      else
	*pr = accum % RADIX;
      accum -= *pr++;
      accum /= RADIX;
    }

  /* If result is negative, subtract it from RADIX**length
     so that we get the right digits for sign-magnitude
     rather than RADIX-complement */

  if (accum)
    {
      result->sign = 1;
      pr = result->contents;
      accum = 0;
      for (i = 0; i < len; i++)
	{
	  accum -= *pr;
	  if (accum)
	    *pr = accum + RADIX;
	  else
	    *pr = 0;
	  accum -= *pr++;
	  accum /= RADIX;
	}
    }

  /* flush leading nonfraction zero digits */

  while (result->before && *--pr == 0)
    (result->before)--;

  flush_trailing_digits (result, 0);

  return result;
}

/* multiply b1 and b2 keeping `digits' fraction digits */

decimal
decimal_mul_rounded (b1, b2, digits)
     decimal b1, b2;
     int digits;
{
  decimal tem = decimal_mul (b1, b2);
  decimal result = decimal_round_digits (tem, digits);
  free (tem);
  return result;
}

/* multiply b1 and b2 keeping the right number of fraction digits
   for the `dc' program with precision = `digits'.  */

decimal
decimal_mul_dc (b1, b2, digits)
     decimal b1, b2;
     int digits;
{
  decimal tem = decimal_mul (b1, b2);
  decimal result
    = decimal_round_digits (tem, MAX (digits, MAX (b1->after, b2->after)));
  free (tem);
  return result;
}

/* multiply b1 and b2 as decimal error-free values;
   keep LENGTH(b1) plus LENGTH(b2) significant figures. */

decimal
decimal_mul (b1, b2)
     decimal b1, b2;
{
  decimal result = make_decimal (b1->before + b2->before, b1->after + b2->after);
  int i;
  int length2 = LENGTH(b2);
  char *pr;

  for (i = 0; i < length2; i++)
    add_scaled (result, b1, b2->contents[i], i);

  /* flush leading nonfraction zero digits */

  pr = result->contents + LENGTH(result);
  while (result->before && *--pr == 0)
    (result->before)--;

  flush_trailing_digits (result, 0);   /* flush trailing zeros */

  /* Set sign properly */

  if (b1->sign != b2->sign && LENGTH(result))
    result->sign = 1;

  return result;
}

/* Modify decimal number `into' by adding `from',
   multiplied by `factor' (which should be nonnegative and less than RADIX)
   and shifted left `scale' digits at the least significant end. */

static void
add_scaled (into, from, factor, scale)
     decimal into, from;
     int factor, scale;
{
  char *pf = from->contents;
  char *pi = into->contents + scale;
  int lengthf = LENGTH(from);
  int lengthi = LENGTH(into) - scale;
  
  int accum = 0;
  int i;

  for (i = 0; i < lengthi; i++)
    {
      accum /= RADIX;
      if (i < lengthf)
        accum += *pf++ * factor;
      accum += *pi;
      *pi++ = accum % RADIX;
    }
}
 
/* Divide decimal number `b1' by `b2', keeping at most `digits'
   fraction digits. 
   Returns the result as a decimal number.

   When division is not exact, the quotient is truncated toward zero.  */

decimal
decimal_div (b1, b2, digits)
     decimal b1, b2;
     int digits;
{
  decimal result = make_decimal (MAX(1, (int) (1 + b1->before - b2->before)), digits);

  /* b1copy holds what is left of the dividend,
     that is not accounted for by the quotient digits already known */

  decimal b1copy = decimal_copy_1 (b1, b2->after + digits);
  int length1 = LENGTH(b1copy);
  int length2 = LENGTH(b2);
  int lengthr = LENGTH(result);
  int i;

  /* leading_divisor_digits contains the first two divisor digits, as
     an integer */ 

  int leading_divisor_digits = b2->contents[length2-1]*RADIX;
  if (length2 > 1)
    leading_divisor_digits += b2->contents[length2-2];

  if (decimal_zerop (b2))
    {
      decimal_error ("divisor is zero", 0);
      return decimal_copy (DECIMAL_ZERO);
    }

  if (lengthr <= (length1 - length2))
    abort();		 /* My reasoning says this cannot happen, I hope */

  for (i = length1 - length2; i >= 0; i--)
    {
      /* Guess the next quotient digit (in order of decreasing significance)
	 using integer division */

      int guess;
      int trial_dividend = b1copy->contents[length2+i-1]*RADIX;
      if (i != length1 - length2)
	trial_dividend += b1copy->contents[length2+i]*RADIX*RADIX;
      if (length2 + i > 1)
	trial_dividend += b1copy->contents[length2+i-2];

      guess = trial_dividend / leading_divisor_digits;

      /* Remove the quotient times this digit from the dividend left */
      /* We may find that the quotient digit is too large,
	 when we consider the entire divisor.
	 Then we decrement the quotient digit and add the divisor back in */

      if (guess && 0 > subtract_scaled (b1copy, b2, guess, i))
	{
	  guess--;
	  add_scaled (b1copy, b2, 1, i);
	}

      if (guess >= RADIX)
	{
	  result->contents[i + 1] += guess / RADIX;
	  guess %= RADIX;
	}
      result->contents[i] = guess;
    }

  free (b1copy);

  result->sign = (b1->sign != b2->sign);

  /* flush leading nonfraction zero digits */

  {
    char *pr = result->contents + lengthr;
    while (result->before && *--pr == 0)
      (result->before)--;
  }

  flush_trailing_digits (result, 0);	/* Flush trailing zero fraction digits */

  return result;
}

/* The remainder for the above division.
   Same as `b1' - (`b1' / `b2') * 'b2'.
   Note that the value depends on the number of fraction digits
   that were kept in computing `b1' / `b2';
   the argument `digits' specifies this.

   The remainder has the same sign as the dividend.
   The divisor's sign is ignored.  */

decimal
decimal_rem (b1, b2, digits)
     decimal b1, b2;
     int digits;
{
  decimal b1copy = decimal_copy_1 (b1, b2->after + digits);
  int length1 = LENGTH(b1copy);
  int length2 = LENGTH(b2);
  int i;

  int leading_divisor_digits = b2->contents[length2-1]*RADIX;

  if (length2 > 1)
    leading_divisor_digits += b2->contents[length2-2];

  if (decimal_zerop (b2))
    {
      decimal_error ("divisor is zero", 0);
      return decimal_copy (DECIMAL_ZERO);
    }

  /* Do like division, above, but throw away the quotient.
     Keep only the final `rest of dividend', which becomes the remainder.  */

  for (i = length1 - length2; i >= 0; i--)
    {
      int guess;
      int trial_dividend = b1copy->contents[length2+i-1]*RADIX;
      if (i != length1 - length2)
	trial_dividend += b1copy->contents[length2+i]*RADIX*RADIX;
      if (length2 + i > 1)
	trial_dividend += b1copy->contents[length2+i-2];

      guess = trial_dividend / leading_divisor_digits;

      if (guess && 0 > subtract_scaled (b1copy, b2, guess, i))
	{
	  guess--;
	  add_scaled (b1copy, b2, 1, i);
	}
      /* No need to check whether guess exceeds RADIX
	 since we are not saving guess.  */
    }

  /* flush leading nonfraction zero digits */

  {
    char *pr = b1copy->contents + length1;
    while (b1copy->before && *--pr == 0)
      (b1copy->before)--;
  }

  flush_trailing_digits (b1copy, 0);
  return b1copy;
}

/* returns negative number if we chose factor too large */

static int
subtract_scaled (into, from, factor, scale)
     decimal into, from;
     int factor, scale;
{
  char *pf = from->contents;
  char *pi = into->contents + scale;
  int lengthf = LENGTH(from);
  int lengthi = LENGTH(into) - scale;
  int accum = 0;
  int i;

  for (i = 0; i < lengthi && i <= lengthf; i++)
    {
      if (i < lengthf)
        accum -= *pf++ * factor;
      accum += *pi;
      if (accum < 0 && accum % RADIX)
        *pi = RADIX - (- accum) % RADIX;
      else
	*pi = accum % RADIX;
      accum -= *pi++;
      accum /= RADIX;
    }
  return accum;
}

/* Return the square root of decimal number D, using Newton's method.
   Number of fraction digits returned is max of FRAC_DIGITS
   and D's number of fraction digits.  */

decimal
decimal_sqrt (d, frac_digits)
     decimal d;
     int frac_digits;
{
  decimal guess;
  int notdone = 1;

  if (decimal_zerop (d)) return d;
  if (d->sign)
    {
      decimal_error ("square root argument negative", 0);
      return decimal_copy (DECIMAL_ZERO);
    }

  frac_digits = MAX (frac_digits, d->after);

  /* Compute an initial guess by taking the square root 
     of a nearby power of RADIX.  */

  if (d->before)
    {
      guess = make_decimal ((d->before + 1) / 2, 0);
      guess->contents[guess->before - 1] = 1;
    }
  else
    {
      /* Arg is less than 1; compute nearest power of RADIX */
      char *p = d->contents + LENGTH(d);
      char *sp = p;

      while (!*--p);	/* Find most significant nonzero digit */
      if (sp - p == 1)
	{
	  /* Arg is bigger than 1/RADIX; use 1 as a guess */
	  guess = decimal_copy (DECIMAL_ONE);
	}
      else
	{
	  guess = make_decimal (0, (sp - p) / 2);
	  guess->contents[0] = 1;
	}
    }

  /* Iterate doing guess = (guess + d/guess) / 2  */

  while (notdone)
    {
      decimal tem1 = decimal_div (d, guess, frac_digits + 1);
      decimal tem2 = decimal_add (guess, tem1);
      decimal tem3 = decimal_mul_rounded (tem2, DECIMAL_HALF, frac_digits);
      notdone = decimal_compare (guess, tem3);
      free (tem1);
      free (tem2);
      free (guess);
      guess = tem3;
      if (decimal_zerop (guess)) return guess;  /* Avoid divide-by-zero */
    }

  return guess;
}

/* Raise decimal number `base' to power of integer part of decimal
   number `expt'.
   This function depends on using radix 10.
   It is too hard to write it to work for any value of RADIX,
   so instead it is simply not available if RADIX is not ten.  */

#if !(RADIX - 10)

decimal
decimal_expt (base, expt, frac_digits)
     decimal base, expt;
     int frac_digits;
{
  decimal accum = decimal_copy (DECIMAL_ONE);
  decimal basis1 = base;
  int digits = expt->before;
  int dig = 0;				/* Expt digit being processed */

  if (expt->sign)
  /* If negative power, take reciprocal first thing
     so that fraction digit truncation won't destroy
     what will ultimately be nonfraction digits.  */
    basis1 = decimal_div (DECIMAL_ONE, base, frac_digits);
  while (dig < digits)
    {
      decimal basis2, basis4, basis8, basis10;
      int thisdigit = expt->contents[expt->after + dig];

      /* Compute factors to multiply in for each bit of this digit */

      basis2 = decimal_mul_rounded (basis1, basis1, frac_digits);
      basis4 = decimal_mul_rounded (basis2, basis2, frac_digits);
      basis8 = decimal_mul_rounded (basis4, basis4, frac_digits);

      /* Now accumulate the factors this digit value selects */

      if (thisdigit & 1)
	{
	  decimal accum1 = decimal_mul_rounded (accum, basis1, frac_digits);
	  free (accum);
	  accum = accum1;
	}

      if (thisdigit & 2)
	{
	  decimal accum1 = decimal_mul_rounded (accum, basis2, frac_digits);
	  free (accum);
	  accum = accum1;
	}

      if (thisdigit & 4)
	{
	  decimal accum1 = decimal_mul_rounded (accum, basis4, frac_digits);
	  free (accum);
	  accum = accum1;
	}

      if (thisdigit & 8)
	{
	  decimal accum1 = decimal_mul_rounded (accum, basis8, frac_digits);
	  free (accum);
	  accum = accum1;
	}

      /* If there are further digits, compute the basis1 for the next digit */

      if (++dig < digits)
	basis10 = decimal_mul_rounded (basis2, basis8, frac_digits);

      /* Free intermediate results */

      if (basis1 != base) free (basis1);
      free (basis2);
      free (basis4);
      free (basis8);
      basis1 = basis10;
    }
  return accum;
}
#endif

#ifdef TEST

fputchar (c)
     char c;
{
  putchar (c);
}

/* Top level that can be used to test the arithmetic functions */

main ()
{
  char s1[40], s2[40];
  decimal b1, b2, b3;
  char c;

  while (1)
    {
      scanf ("%s %c %s", s1, &c, s2);
      b1 = decimal_parse (s1, RADIX);
      b2 = decimal_parse (s2, RADIX);
      switch (c)
	{
	default:
	  c = '+';
	case '+':
	  b3 = decimal_add (b1, b2);
	  break;
	case '*':
	  b3 = decimal_mul (b1, b2);
	  break;
        case '/':
	  b3 = decimal_div (b1, b2, 3);
	  break;
	case '%':
	  b3 = decimal_rem (b1, b2, 3);
	  break;
        case 'p':
	  decimal_print (b1, fputchar, RADIX);
	  printf (" printed in base %d is ", decimal_to_int (b2));
	  decimal_print (b1, fputchar, decimal_to_int (b2));
	  printf ("\n");
	  continue;
	case 'r':
	  printf ("%s read in base %d is ", s1, decimal_to_int (b2));
	  decimal_print (decimal_parse (s1, decimal_to_int (b2)), fputchar, RADIX);
	  printf ("\n");
	  continue;
	}
      decimal_print (b1, fputchar, RADIX);
      printf (" %c ", c);
      decimal_print (b2, fputchar, RADIX);
      printf (" = ");
      decimal_print (b3, fputchar, RADIX);
      printf ("\n");
    }
}

decimal_error (s1, s2)
     char *s1, *s2;
{
  printf ("\n");
  printf (s1, s2);
  printf ("\n");
}

static void
pbi (b)
      int b;
{
  decimal_print ((decimal) b, fputchar, RADIX);
}

static void
pb (b)
      decimal b;
{
  decimal_print (b, fputchar, RADIX);
}

#endif
