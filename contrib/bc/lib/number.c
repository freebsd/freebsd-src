/* number.c: Implements arbitrary precision numbers. */
/*
    Copyright (C) 1991, 1992, 1993, 1994, 1997, 2000 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to:

      The Free Software Foundation, Inc.
      59 Temple Place, Suite 330
      Boston, MA 02111-1307 USA.


    You may contact the author by:
       e-mail:  philnelson@acm.org
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/

#include <stdio.h>
#include <config.h>
#include <number.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>/* Prototypes needed for external utility routines. */

#define bc_rt_warn rt_warn
#define bc_rt_error rt_error
#define bc_out_of_memory out_of_memory

_PROTOTYPE(void rt_warn, (char *mesg ,...));
_PROTOTYPE(void rt_error, (char *mesg ,...));
_PROTOTYPE(void out_of_memory, (void));

/* Storage used for special numbers. */
bc_num _zero_;
bc_num _one_;
bc_num _two_;

static bc_num _bc_Free_list = NULL;

/* new_num allocates a number and sets fields to known values. */

bc_num
bc_new_num (length, scale)
     int length, scale;
{
  bc_num temp;

  if (_bc_Free_list != NULL) {
    temp = _bc_Free_list;
    _bc_Free_list = temp->n_next;
  } else {
    temp = (bc_num) malloc (sizeof(bc_struct));
    if (temp == NULL) bc_out_of_memory ();
  }
  temp->n_sign = PLUS;
  temp->n_len = length;
  temp->n_scale = scale;
  temp->n_refs = 1;
  temp->n_ptr = (char *) malloc (length+scale);
  if (temp->n_ptr == NULL) bc_out_of_memory();
  temp->n_value = temp->n_ptr;
  memset (temp->n_ptr, 0, length+scale);
  return temp;
}

/* "Frees" a bc_num NUM.  Actually decreases reference count and only
   frees the storage if reference count is zero. */

void
bc_free_num (num)
    bc_num *num;
{
  if (*num == NULL) return;
  (*num)->n_refs--;
  if ((*num)->n_refs == 0) {
    if ((*num)->n_ptr)
      free ((*num)->n_ptr);
    (*num)->n_next = _bc_Free_list;
    _bc_Free_list = *num;
  }
  *num = NULL;
}


/* Intitialize the number package! */

void
bc_init_numbers ()
{
  _zero_ = bc_new_num (1,0);
  _one_  = bc_new_num (1,0);
  _one_->n_value[0] = 1;
  _two_  = bc_new_num (1,0);
  _two_->n_value[0] = 2;
}


/* Make a copy of a number!  Just increments the reference count! */

bc_num
bc_copy_num (num)
     bc_num num;
{
  num->n_refs++;
  return num;
}


/* Initialize a number NUM by making it a copy of zero. */

void
bc_init_num (num)
     bc_num *num;
{
  *num = bc_copy_num (_zero_);
}

/* For many things, we may have leading zeros in a number NUM.
   _bc_rm_leading_zeros just moves the data "value" pointer to the
   correct place and adjusts the length. */

static void
_bc_rm_leading_zeros (num)
     bc_num num;
{
  /* We can move n_value to point to the first non zero digit! */
  while (*num->n_value == 0 && num->n_len > 1) {
    num->n_value++;
    num->n_len--;
  }
}


/* Compare two bc numbers.  Return value is 0 if equal, -1 if N1 is less
   than N2 and +1 if N1 is greater than N2.  If USE_SIGN is false, just
   compare the magnitudes. */

static int
_bc_do_compare (n1, n2, use_sign, ignore_last)
     bc_num n1, n2;
     int use_sign;
     int ignore_last;
{
  char *n1ptr, *n2ptr;
  int  count;

  /* First, compare signs. */
  if (use_sign && n1->n_sign != n2->n_sign)
    {
      if (n1->n_sign == PLUS)
	return (1);	/* Positive N1 > Negative N2 */
      else
	return (-1);	/* Negative N1 < Positive N1 */
    }

  /* Now compare the magnitude. */
  if (n1->n_len != n2->n_len)
    {
      if (n1->n_len > n2->n_len)
	{
	  /* Magnitude of n1 > n2. */
	  if (!use_sign || n1->n_sign == PLUS)
	    return (1);
	  else
	    return (-1);
	}
      else
	{
	  /* Magnitude of n1 < n2. */
	  if (!use_sign || n1->n_sign == PLUS)
	    return (-1);
	  else
	    return (1);
	}
    }

  /* If we get here, they have the same number of integer digits.
     check the integer part and the equal length part of the fraction. */
  count = n1->n_len + MIN (n1->n_scale, n2->n_scale);
  n1ptr = n1->n_value;
  n2ptr = n2->n_value;

  while ((count > 0) && (*n1ptr == *n2ptr))
    {
      n1ptr++;
      n2ptr++;
      count--;
    }
  if (ignore_last && count == 1 && n1->n_scale == n2->n_scale)
    return (0);
  if (count != 0)
    {
      if (*n1ptr > *n2ptr)
	{
	  /* Magnitude of n1 > n2. */
	  if (!use_sign || n1->n_sign == PLUS)
	    return (1);
	  else
	    return (-1);
	}
      else
	{
	  /* Magnitude of n1 < n2. */
	  if (!use_sign || n1->n_sign == PLUS)
	    return (-1);
	  else
	    return (1);
	}
    }

  /* They are equal up to the last part of the equal part of the fraction. */
  if (n1->n_scale != n2->n_scale)
    {
      if (n1->n_scale > n2->n_scale)
	{
	  for (count = n1->n_scale-n2->n_scale; count>0; count--)
	    if (*n1ptr++ != 0)
	      {
		/* Magnitude of n1 > n2. */
		if (!use_sign || n1->n_sign == PLUS)
		  return (1);
		else
		  return (-1);
	      }
	}
      else
	{
	  for (count = n2->n_scale-n1->n_scale; count>0; count--)
	    if (*n2ptr++ != 0)
	      {
		/* Magnitude of n1 < n2. */
		if (!use_sign || n1->n_sign == PLUS)
		  return (-1);
		else
		  return (1);
	      }
	}
    }

  /* They must be equal! */
  return (0);
}


/* This is the "user callable" routine to compare numbers N1 and N2. */

int
bc_compare (n1, n2)
     bc_num n1, n2;
{
  return _bc_do_compare (n1, n2, TRUE, FALSE);
}

/* In some places we need to check if the number is negative. */

char
bc_is_neg (num)
     bc_num num;
{
  return num->n_sign == MINUS;
}

/* In some places we need to check if the number NUM is zero. */

char
bc_is_zero (num)
     bc_num num;
{
  int  count;
  char *nptr;

  /* Quick check. */
  if (num == _zero_) return TRUE;

  /* Initialize */
  count = num->n_len + num->n_scale;
  nptr = num->n_value;

  /* The check */
  while ((count > 0) && (*nptr++ == 0)) count--;

  if (count != 0)
    return FALSE;
  else
    return TRUE;
}

/* In some places we need to check if the number NUM is almost zero.
   Specifically, all but the last digit is 0 and the last digit is 1.
   Last digit is defined by scale. */

char
bc_is_near_zero (num, scale)
     bc_num num;
     int scale;
{
  int  count;
  char *nptr;

  /* Error checking */
  if (scale > num->n_scale)
    scale = num->n_scale;

  /* Initialize */
  count = num->n_len + scale;
  nptr = num->n_value;

  /* The check */
  while ((count > 0) && (*nptr++ == 0)) count--;

  if (count != 0 && (count != 1 || *--nptr != 1))
    return FALSE;
  else
    return TRUE;
}


/* Perform addition: N1 is added to N2 and the value is
   returned.  The signs of N1 and N2 are ignored.
   SCALE_MIN is to set the minimum scale of the result. */

static bc_num
_bc_do_add (n1, n2, scale_min)
     bc_num n1, n2;
     int scale_min;
{
  bc_num sum;
  int sum_scale, sum_digits;
  char *n1ptr, *n2ptr, *sumptr;
  int carry, n1bytes, n2bytes;
  int count;

  /* Prepare sum. */
  sum_scale = MAX (n1->n_scale, n2->n_scale);
  sum_digits = MAX (n1->n_len, n2->n_len) + 1;
  sum = bc_new_num (sum_digits, MAX(sum_scale, scale_min));

  /* Zero extra digits made by scale_min. */
  if (scale_min > sum_scale)
    {
      sumptr = (char *) (sum->n_value + sum_scale + sum_digits);
      for (count = scale_min - sum_scale; count > 0; count--)
	*sumptr++ = 0;
    }

  /* Start with the fraction part.  Initialize the pointers. */
  n1bytes = n1->n_scale;
  n2bytes = n2->n_scale;
  n1ptr = (char *) (n1->n_value + n1->n_len + n1bytes - 1);
  n2ptr = (char *) (n2->n_value + n2->n_len + n2bytes - 1);
  sumptr = (char *) (sum->n_value + sum_scale + sum_digits - 1);

  /* Add the fraction part.  First copy the longer fraction.*/
  if (n1bytes != n2bytes)
    {
      if (n1bytes > n2bytes)
	while (n1bytes>n2bytes)
	  { *sumptr-- = *n1ptr--; n1bytes--;}
      else
	while (n2bytes>n1bytes)
	  { *sumptr-- = *n2ptr--; n2bytes--;}
    }

  /* Now add the remaining fraction part and equal size integer parts. */
  n1bytes += n1->n_len;
  n2bytes += n2->n_len;
  carry = 0;
  while ((n1bytes > 0) && (n2bytes > 0))
    {
      *sumptr = *n1ptr-- + *n2ptr-- + carry;
      if (*sumptr > (BASE-1))
	{
	   carry = 1;
	   *sumptr -= BASE;
	}
      else
	carry = 0;
      sumptr--;
      n1bytes--;
      n2bytes--;
    }

  /* Now add carry the longer integer part. */
  if (n1bytes == 0)
    { n1bytes = n2bytes; n1ptr = n2ptr; }
  while (n1bytes-- > 0)
    {
      *sumptr = *n1ptr-- + carry;
      if (*sumptr > (BASE-1))
	{
	   carry = 1;
	   *sumptr -= BASE;
	 }
      else
	carry = 0;
      sumptr--;
    }

  /* Set final carry. */
  if (carry == 1)
    *sumptr += 1;

  /* Adjust sum and return. */
  _bc_rm_leading_zeros (sum);
  return sum;
}


/* Perform subtraction: N2 is subtracted from N1 and the value is
   returned.  The signs of N1 and N2 are ignored.  Also, N1 is
   assumed to be larger than N2.  SCALE_MIN is the minimum scale
   of the result. */

static bc_num
_bc_do_sub (n1, n2, scale_min)
     bc_num n1, n2;
     int scale_min;
{
  bc_num diff;
  int diff_scale, diff_len;
  int min_scale, min_len;
  char *n1ptr, *n2ptr, *diffptr;
  int borrow, count, val;

  /* Allocate temporary storage. */
  diff_len = MAX (n1->n_len, n2->n_len);
  diff_scale = MAX (n1->n_scale, n2->n_scale);
  min_len = MIN  (n1->n_len, n2->n_len);
  min_scale = MIN (n1->n_scale, n2->n_scale);
  diff = bc_new_num (diff_len, MAX(diff_scale, scale_min));

  /* Zero extra digits made by scale_min. */
  if (scale_min > diff_scale)
    {
      diffptr = (char *) (diff->n_value + diff_len + diff_scale);
      for (count = scale_min - diff_scale; count > 0; count--)
	*diffptr++ = 0;
    }

  /* Initialize the subtract. */
  n1ptr = (char *) (n1->n_value + n1->n_len + n1->n_scale -1);
  n2ptr = (char *) (n2->n_value + n2->n_len + n2->n_scale -1);
  diffptr = (char *) (diff->n_value + diff_len + diff_scale -1);

  /* Subtract the numbers. */
  borrow = 0;

  /* Take care of the longer scaled number. */
  if (n1->n_scale != min_scale)
    {
      /* n1 has the longer scale */
      for (count = n1->n_scale - min_scale; count > 0; count--)
	*diffptr-- = *n1ptr--;
    }
  else
    {
      /* n2 has the longer scale */
      for (count = n2->n_scale - min_scale; count > 0; count--)
	{
	  val = - *n2ptr-- - borrow;
	  if (val < 0)
	    {
	      val += BASE;
	      borrow = 1;
	    }
	  else
	    borrow = 0;
	  *diffptr-- = val;
	}
    }

  /* Now do the equal length scale and integer parts. */

  for (count = 0; count < min_len + min_scale; count++)
    {
      val = *n1ptr-- - *n2ptr-- - borrow;
      if (val < 0)
	{
	  val += BASE;
	  borrow = 1;
	}
      else
	borrow = 0;
      *diffptr-- = val;
    }

  /* If n1 has more digits then n2, we now do that subtract. */
  if (diff_len != min_len)
    {
      for (count = diff_len - min_len; count > 0; count--)
	{
	  val = *n1ptr-- - borrow;
	  if (val < 0)
	    {
	      val += BASE;
	      borrow = 1;
	    }
	  else
	    borrow = 0;
	  *diffptr-- = val;
	}
    }

  /* Clean up and return. */
  _bc_rm_leading_zeros (diff);
  return diff;
}


/* Here is the full subtract routine that takes care of negative numbers.
   N2 is subtracted from N1 and the result placed in RESULT.  SCALE_MIN
   is the minimum scale for the result. */

void
bc_sub (n1, n2, result, scale_min)
     bc_num n1, n2, *result;
     int scale_min;
{
  bc_num diff = NULL;
  int cmp_res;
  int res_scale;

  if (n1->n_sign != n2->n_sign)
    {
      diff = _bc_do_add (n1, n2, scale_min);
      diff->n_sign = n1->n_sign;
    }
  else
    {
      /* subtraction must be done. */
      /* Compare magnitudes. */
      cmp_res = _bc_do_compare (n1, n2, FALSE, FALSE);
      switch (cmp_res)
	{
	case -1:
	  /* n1 is less than n2, subtract n1 from n2. */
	  diff = _bc_do_sub (n2, n1, scale_min);
	  diff->n_sign = (n2->n_sign == PLUS ? MINUS : PLUS);
	  break;
	case  0:
	  /* They are equal! return zero! */
	  res_scale = MAX (scale_min, MAX(n1->n_scale, n2->n_scale));
	  diff = bc_new_num (1, res_scale);
	  memset (diff->n_value, 0, res_scale+1);
	  break;
	case  1:
	  /* n2 is less than n1, subtract n2 from n1. */
	  diff = _bc_do_sub (n1, n2, scale_min);
	  diff->n_sign = n1->n_sign;
	  break;
	}
    }

  /* Clean up and return. */
  bc_free_num (result);
  *result = diff;
}


/* Here is the full add routine that takes care of negative numbers.
   N1 is added to N2 and the result placed into RESULT.  SCALE_MIN
   is the minimum scale for the result. */

void
bc_add (n1, n2, result, scale_min)
     bc_num n1, n2, *result;
     int scale_min;
{
  bc_num sum = NULL;
  int cmp_res;
  int res_scale;

  if (n1->n_sign == n2->n_sign)
    {
      sum = _bc_do_add (n1, n2, scale_min);
      sum->n_sign = n1->n_sign;
    }
  else
    {
      /* subtraction must be done. */
      cmp_res = _bc_do_compare (n1, n2, FALSE, FALSE);  /* Compare magnitudes. */
      switch (cmp_res)
	{
	case -1:
	  /* n1 is less than n2, subtract n1 from n2. */
	  sum = _bc_do_sub (n2, n1, scale_min);
	  sum->n_sign = n2->n_sign;
	  break;
	case  0:
	  /* They are equal! return zero with the correct scale! */
	  res_scale = MAX (scale_min, MAX(n1->n_scale, n2->n_scale));
	  sum = bc_new_num (1, res_scale);
	  memset (sum->n_value, 0, res_scale+1);
	  break;
	case  1:
	  /* n2 is less than n1, subtract n2 from n1. */
	  sum = _bc_do_sub (n1, n2, scale_min);
	  sum->n_sign = n1->n_sign;
	}
    }

  /* Clean up and return. */
  bc_free_num (result);
  *result = sum;
}

/* Recursive vs non-recursive multiply crossover ranges. */
#if defined(MULDIGITS)
#include "muldigits.h"
#else
#define MUL_BASE_DIGITS 80
#endif

int mul_base_digits = MUL_BASE_DIGITS;
#define MUL_SMALL_DIGITS mul_base_digits/4

/* Multiply utility routines */

static bc_num
new_sub_num (length, scale, value)
     int length, scale;
     char *value;
{
  bc_num temp;

  if (_bc_Free_list != NULL) {
    temp = _bc_Free_list;
    _bc_Free_list = temp->n_next;
  } else {
    temp = (bc_num) malloc (sizeof(bc_struct));
    if (temp == NULL) bc_out_of_memory ();
  }
  temp->n_sign = PLUS;
  temp->n_len = length;
  temp->n_scale = scale;
  temp->n_refs = 1;
  temp->n_ptr = NULL;
  temp->n_value = value;
  return temp;
}

static void
_bc_simp_mul (bc_num n1, int n1len, bc_num n2, int n2len, bc_num *prod,
	      int full_scale)
{
  char *n1ptr, *n2ptr, *pvptr;
  char *n1end, *n2end;		/* To the end of n1 and n2. */
  int indx, sum, prodlen;

  prodlen = n1len+n2len+1;

  *prod = bc_new_num (prodlen, 0);

  n1end = (char *) (n1->n_value + n1len - 1);
  n2end = (char *) (n2->n_value + n2len - 1);
  pvptr = (char *) ((*prod)->n_value + prodlen - 1);
  sum = 0;

  /* Here is the loop... */
  for (indx = 0; indx < prodlen-1; indx++)
    {
      n1ptr = (char *) (n1end - MAX(0, indx-n2len+1));
      n2ptr = (char *) (n2end - MIN(indx, n2len-1));
      while ((n1ptr >= n1->n_value) && (n2ptr <= n2end))
	sum += *n1ptr-- * *n2ptr++;
      *pvptr-- = sum % BASE;
      sum = sum / BASE;
    }
  *pvptr = sum;
}


/* A special adder/subtractor for the recursive divide and conquer
   multiply algorithm.  Note: if sub is called, accum must
   be larger that what is being subtracted.  Also, accum and val
   must have n_scale = 0.  (e.g. they must look like integers. *) */
static void
_bc_shift_addsub (bc_num accum, bc_num val, int shift, int sub)
{
  signed char *accp, *valp;
  int  count, carry;

  count = val->n_len;
  if (val->n_value[0] == 0)
    count--;
  assert (accum->n_len+accum->n_scale >= shift+count);
  
  /* Set up pointers and others */
  accp = (signed char *)(accum->n_value +
			 accum->n_len + accum->n_scale - shift - 1);
  valp = (signed char *)(val->n_value + val->n_len - 1);
  carry = 0;

  if (sub) {
    /* Subtraction, carry is really borrow. */
    while (count--) {
      *accp -= *valp-- + carry;
      if (*accp < 0) {
	carry = 1;
        *accp-- += BASE;
      } else {
	carry = 0;
	accp--;
      }
    }
    while (carry) {
      *accp -= carry;
      if (*accp < 0)
	*accp-- += BASE;
      else
	carry = 0;
    }
  } else {
    /* Addition */
    while (count--) {
      *accp += *valp-- + carry;
      if (*accp > (BASE-1)) {
	carry = 1;
        *accp-- -= BASE;
      } else {
	carry = 0;
	accp--;
      }
    }
    while (carry) {
      *accp += carry;
      if (*accp > (BASE-1))
	*accp-- -= BASE;
      else
	carry = 0;
    }
  }
}

/* Recursive divide and conquer multiply algorithm.  
   Based on 
   Let u = u0 + u1*(b^n)
   Let v = v0 + v1*(b^n)
   Then uv = (B^2n+B^n)*u1*v1 + B^n*(u1-u0)*(v0-v1) + (B^n+1)*u0*v0

   B is the base of storage, number of digits in u1,u0 close to equal.
*/
static void
_bc_rec_mul (bc_num u, int ulen, bc_num v, int vlen, bc_num *prod,
	     int full_scale)
{ 
  bc_num u0, u1, v0, v1;
  int u0len, v0len;
  bc_num m1, m2, m3, d1, d2;
  int n, prodlen, m1zero;
  int d1len, d2len;

  /* Base case? */
  if ((ulen+vlen) < mul_base_digits
      || ulen < MUL_SMALL_DIGITS
      || vlen < MUL_SMALL_DIGITS ) {
    _bc_simp_mul (u, ulen, v, vlen, prod, full_scale);
    return;
  }

  /* Calculate n -- the u and v split point in digits. */
  n = (MAX(ulen, vlen)+1) / 2;

  /* Split u and v. */
  if (ulen < n) {
    u1 = bc_copy_num (_zero_);
    u0 = new_sub_num (ulen,0, u->n_value);
  } else {
    u1 = new_sub_num (ulen-n, 0, u->n_value);
    u0 = new_sub_num (n, 0, u->n_value+ulen-n);
  }
  if (vlen < n) {
    v1 = bc_copy_num (_zero_);
    v0 = new_sub_num (vlen,0, v->n_value);
  } else {
    v1 = new_sub_num (vlen-n, 0, v->n_value);
    v0 = new_sub_num (n, 0, v->n_value+vlen-n);
    }
  _bc_rm_leading_zeros (u1);
  _bc_rm_leading_zeros (u0);
  u0len = u0->n_len;
  _bc_rm_leading_zeros (v1);
  _bc_rm_leading_zeros (v0);
  v0len = v0->n_len;

  m1zero = bc_is_zero(u1) || bc_is_zero(v1);

  /* Calculate sub results ... */

  bc_init_num(&d1);
  bc_init_num(&d2);
  bc_sub (u1, u0, &d1, 0);
  d1len = d1->n_len;
  bc_sub (v0, v1, &d2, 0);
  d2len = d2->n_len;


  /* Do recursive multiplies and shifted adds. */
  if (m1zero)
    m1 = bc_copy_num (_zero_);
  else
    _bc_rec_mul (u1, u1->n_len, v1, v1->n_len, &m1, 0);

  if (bc_is_zero(d1) || bc_is_zero(d2))
    m2 = bc_copy_num (_zero_);
  else
    _bc_rec_mul (d1, d1len, d2, d2len, &m2, 0);

  if (bc_is_zero(u0) || bc_is_zero(v0))
    m3 = bc_copy_num (_zero_);
  else
    _bc_rec_mul (u0, u0->n_len, v0, v0->n_len, &m3, 0);

  /* Initialize product */
  prodlen = ulen+vlen+1;
  *prod = bc_new_num(prodlen, 0);

  if (!m1zero) {
    _bc_shift_addsub (*prod, m1, 2*n, 0);
    _bc_shift_addsub (*prod, m1, n, 0);
  }
  _bc_shift_addsub (*prod, m3, n, 0);
  _bc_shift_addsub (*prod, m3, 0, 0);
  _bc_shift_addsub (*prod, m2, n, d1->n_sign != d2->n_sign);

  /* Now clean up! */
  bc_free_num (&u1);
  bc_free_num (&u0);
  bc_free_num (&v1);
  bc_free_num (&m1);
  bc_free_num (&v0);
  bc_free_num (&m2);
  bc_free_num (&m3);
  bc_free_num (&d1);
  bc_free_num (&d2);
}

/* The multiply routine.  N2 times N1 is put int PROD with the scale of
   the result being MIN(N2 scale+N1 scale, MAX (SCALE, N2 scale, N1 scale)).
   */

void
bc_multiply (n1, n2, prod, scale)
     bc_num n1, n2, *prod;
     int scale;
{
  bc_num pval; 
  int len1, len2;
  int full_scale, prod_scale;

  /* Initialize things. */
  len1 = n1->n_len + n1->n_scale;
  len2 = n2->n_len + n2->n_scale;
  full_scale = n1->n_scale + n2->n_scale;
  prod_scale = MIN(full_scale,MAX(scale,MAX(n1->n_scale,n2->n_scale)));

  /* Do the multiply */
  _bc_rec_mul (n1, len1, n2, len2, &pval, full_scale);

  /* Assign to prod and clean up the number. */
  pval->n_sign = ( n1->n_sign == n2->n_sign ? PLUS : MINUS );
  pval->n_value = pval->n_ptr;
  pval->n_len = len2 + len1 + 1 - full_scale;
  pval->n_scale = prod_scale;
  _bc_rm_leading_zeros (pval);
  if (bc_is_zero (pval))
    pval->n_sign = PLUS;
  bc_free_num (prod);
  *prod = pval;
}

/* Some utility routines for the divide:  First a one digit multiply.
   NUM (with SIZE digits) is multiplied by DIGIT and the result is
   placed into RESULT.  It is written so that NUM and RESULT can be
   the same pointers.  */

static void
_one_mult (num, size, digit, result)
     unsigned char *num;
     int size, digit;
     unsigned char *result;
{
  int carry, value;
  unsigned char *nptr, *rptr;

  if (digit == 0)
    memset (result, 0, size);
  else
    {
      if (digit == 1)
	memcpy (result, num, size);
      else
	{
	  /* Initialize */
	  nptr = (unsigned char *) (num+size-1);
	  rptr = (unsigned char *) (result+size-1);
	  carry = 0;

	  while (size-- > 0)
	    {
	      value = *nptr-- * digit + carry;
	      *rptr-- = value % BASE;
	      carry = value / BASE;
	    }

	  if (carry != 0) *rptr = carry;
	}
    }
}


/* The full division routine. This computes N1 / N2.  It returns
   0 if the division is ok and the result is in QUOT.  The number of
   digits after the decimal point is SCALE. It returns -1 if division
   by zero is tried.  The algorithm is found in Knuth Vol 2. p237. */

int
bc_divide (n1, n2, quot, scale)
     bc_num n1, n2, *quot;
     int scale;
{
  bc_num qval;
  unsigned char *num1, *num2;
  unsigned char *ptr1, *ptr2, *n2ptr, *qptr;
  int  scale1, val;
  unsigned int  len1, len2, scale2, qdigits, extra, count;
  unsigned int  qdig, qguess, borrow, carry;
  unsigned char *mval;
  char zero;
  unsigned int  norm;

  /* Test for divide by zero. */
  if (bc_is_zero (n2)) return -1;

  /* Test for divide by 1.  If it is we must truncate. */
  if (n2->n_scale == 0)
    {
      if (n2->n_len == 1 && *n2->n_value == 1)
	{
	  qval = bc_new_num (n1->n_len, scale);
	  qval->n_sign = (n1->n_sign == n2->n_sign ? PLUS : MINUS);
	  memset (&qval->n_value[n1->n_len],0,scale);
	  memcpy (qval->n_value, n1->n_value,
		  n1->n_len + MIN(n1->n_scale,scale));
	  bc_free_num (quot);
	  *quot = qval;
	}
    }

  /* Set up the divide.  Move the decimal point on n1 by n2's scale.
     Remember, zeros on the end of num2 are wasted effort for dividing. */
  scale2 = n2->n_scale;
  n2ptr = (unsigned char *) n2->n_value+n2->n_len+scale2-1;
  while ((scale2 > 0) && (*n2ptr-- == 0)) scale2--;

  len1 = n1->n_len + scale2;
  scale1 = n1->n_scale - scale2;
  if (scale1 < scale)
    extra = scale - scale1;
  else
    extra = 0;
  num1 = (unsigned char *) malloc (n1->n_len+n1->n_scale+extra+2);
  if (num1 == NULL) bc_out_of_memory();
  memset (num1, 0, n1->n_len+n1->n_scale+extra+2);
  memcpy (num1+1, n1->n_value, n1->n_len+n1->n_scale);

  len2 = n2->n_len + scale2;
  num2 = (unsigned char *) malloc (len2+1);
  if (num2 == NULL) bc_out_of_memory();
  memcpy (num2, n2->n_value, len2);
  *(num2+len2) = 0;
  n2ptr = num2;
  while (*n2ptr == 0)
    {
      n2ptr++;
      len2--;
    }

  /* Calculate the number of quotient digits. */
  if (len2 > len1+scale)
    {
      qdigits = scale+1;
      zero = TRUE;
    }
  else
    {
      zero = FALSE;
      if (len2>len1)
	qdigits = scale+1;  	/* One for the zero integer part. */
      else
	qdigits = len1-len2+scale+1;
    }

  /* Allocate and zero the storage for the quotient. */
  qval = bc_new_num (qdigits-scale,scale);
  memset (qval->n_value, 0, qdigits);

  /* Allocate storage for the temporary storage mval. */
  mval = (unsigned char *) malloc (len2+1);
  if (mval == NULL) bc_out_of_memory ();

  /* Now for the full divide algorithm. */
  if (!zero)
    {
      /* Normalize */
      norm =  10 / ((int)*n2ptr + 1);
      if (norm != 1)
	{
	  _one_mult (num1, len1+scale1+extra+1, norm, num1);
	  _one_mult (n2ptr, len2, norm, n2ptr);
	}

      /* Initialize divide loop. */
      qdig = 0;
      if (len2 > len1)
	qptr = (unsigned char *) qval->n_value+len2-len1;
      else
	qptr = (unsigned char *) qval->n_value;

      /* Loop */
      while (qdig <= len1+scale-len2)
	{
	  /* Calculate the quotient digit guess. */
	  if (*n2ptr == num1[qdig])
	    qguess = 9;
	  else
	    qguess = (num1[qdig]*10 + num1[qdig+1]) / *n2ptr;

	  /* Test qguess. */
	  if (n2ptr[1]*qguess >
	      (num1[qdig]*10 + num1[qdig+1] - *n2ptr*qguess)*10
	       + num1[qdig+2])
	    {
	      qguess--;
	      /* And again. */
	      if (n2ptr[1]*qguess >
		  (num1[qdig]*10 + num1[qdig+1] - *n2ptr*qguess)*10
		  + num1[qdig+2])
		qguess--;
	    }

	  /* Multiply and subtract. */
	  borrow = 0;
	  if (qguess != 0)
	    {
	      *mval = 0;
	      _one_mult (n2ptr, len2, qguess, mval+1);
	      ptr1 = (unsigned char *) num1+qdig+len2;
	      ptr2 = (unsigned char *) mval+len2;
	      for (count = 0; count < len2+1; count++)
		{
		  val = (int) *ptr1 - (int) *ptr2-- - borrow;
		  if (val < 0)
		    {
		      val += 10;
		      borrow = 1;
		    }
		  else
		    borrow = 0;
		  *ptr1-- = val;
		}
	    }

	  /* Test for negative result. */
	  if (borrow == 1)
	    {
	      qguess--;
	      ptr1 = (unsigned char *) num1+qdig+len2;
	      ptr2 = (unsigned char *) n2ptr+len2-1;
	      carry = 0;
	      for (count = 0; count < len2; count++)
		{
		  val = (int) *ptr1 + (int) *ptr2-- + carry;
		  if (val > 9)
		    {
		      val -= 10;
		      carry = 1;
		    }
		  else
		    carry = 0;
		  *ptr1-- = val;
		}
	      if (carry == 1) *ptr1 = (*ptr1 + 1) % 10;
	    }

	  /* We now know the quotient digit. */
	  *qptr++ =  qguess;
	  qdig++;
	}
    }

  /* Clean up and return the number. */
  qval->n_sign = ( n1->n_sign == n2->n_sign ? PLUS : MINUS );
  if (bc_is_zero (qval)) qval->n_sign = PLUS;
  _bc_rm_leading_zeros (qval);
  bc_free_num (quot);
  *quot = qval;

  /* Clean up temporary storage. */
  free (mval);
  free (num1);
  free (num2);

  return 0;	/* Everything is OK. */
}


/* Division *and* modulo for numbers.  This computes both NUM1 / NUM2 and
   NUM1 % NUM2  and puts the results in QUOT and REM, except that if QUOT
   is NULL then that store will be omitted.
 */

int
bc_divmod (num1, num2, quot, rem, scale)
     bc_num num1, num2, *quot, *rem;
     int scale;
{
  bc_num quotient = NULL;
  bc_num temp;
  int rscale;

  /* Check for correct numbers. */
  if (bc_is_zero (num2)) return -1;

  /* Calculate final scale. */
  rscale = MAX (num1->n_scale, num2->n_scale+scale);
  bc_init_num(&temp);

  /* Calculate it. */
  bc_divide (num1, num2, &temp, scale);
  if (quot)
    quotient = bc_copy_num (temp);
  bc_multiply (temp, num2, &temp, rscale);
  bc_sub (num1, temp, rem, rscale);
  bc_free_num (&temp);

  if (quot)
    {
      bc_free_num (quot);
      *quot = quotient;
    }

  return 0;	/* Everything is OK. */
}


/* Modulo for numbers.  This computes NUM1 % NUM2  and puts the
   result in RESULT.   */

int
bc_modulo (num1, num2, result, scale)
     bc_num num1, num2, *result;
     int scale;
{
  return bc_divmod (num1, num2, NULL, result, scale);
}

/* Raise BASE to the EXPO power, reduced modulo MOD.  The result is
   placed in RESULT.  If a EXPO is not an integer,
   only the integer part is used.  */

int
bc_raisemod (base, expo, mod, result, scale)
     bc_num base, expo, mod, *result;
     int scale;
{
  bc_num power, exponent, parity, temp;
  int rscale;

  /* Check for correct numbers. */
  if (bc_is_zero(mod)) return -1;
  if (bc_is_neg(expo)) return -1;

  /* Set initial values.  */
  power = bc_copy_num (base);
  exponent = bc_copy_num (expo);
  temp = bc_copy_num (_one_);
  bc_init_num(&parity);

  /* Check the base for scale digits. */
  if (base->n_scale != 0)
      bc_rt_warn ("non-zero scale in base");

  /* Check the exponent for scale digits. */
  if (exponent->n_scale != 0)
    {
      bc_rt_warn ("non-zero scale in exponent");
      bc_divide (exponent, _one_, &exponent, 0); /*truncate */
    }

  /* Check the modulus for scale digits. */
  if (mod->n_scale != 0)
      bc_rt_warn ("non-zero scale in modulus");

  /* Do the calculation. */
  rscale = MAX(scale, base->n_scale);
  while ( !bc_is_zero(exponent) )
    {
      (void) bc_divmod (exponent, _two_, &exponent, &parity, 0);
      if ( !bc_is_zero(parity) )
	{
	  bc_multiply (temp, power, &temp, rscale);
	  (void) bc_modulo (temp, mod, &temp, scale);
	}

      bc_multiply (power, power, &power, rscale);
      (void) bc_modulo (power, mod, &power, scale);
    }

  /* Assign the value. */
  bc_free_num (&power);
  bc_free_num (&exponent);
  bc_free_num (result);
  *result = temp;
  return 0;	/* Everything is OK. */
}

/* Raise NUM1 to the NUM2 power.  The result is placed in RESULT.
   Maximum exponent is LONG_MAX.  If a NUM2 is not an integer,
   only the integer part is used.  */

void
bc_raise (num1, num2, result, scale)
     bc_num num1, num2, *result;
     int scale;
{
   bc_num temp, power;
   long exponent;
   int rscale;
   int pwrscale;
   int calcscale;
   char neg;

   /* Check the exponent for scale digits and convert to a long. */
   if (num2->n_scale != 0)
     bc_rt_warn ("non-zero scale in exponent");
   exponent = bc_num2long (num2);
   if (exponent == 0 && (num2->n_len > 1 || num2->n_value[0] != 0))
       bc_rt_error ("exponent too large in raise");

   /* Special case if exponent is a zero. */
   if (exponent == 0)
     {
       bc_free_num (result);
       *result = bc_copy_num (_one_);
       return;
     }

   /* Other initializations. */
   if (exponent < 0)
     {
       neg = TRUE;
       exponent = -exponent;
       rscale = scale;
     }
   else
     {
       neg = FALSE;
       rscale = MIN (num1->n_scale*exponent, MAX(scale, num1->n_scale));
     }

   /* Set initial value of temp.  */
   power = bc_copy_num (num1);
   pwrscale = num1->n_scale;
   while ((exponent & 1) == 0)
     {
       pwrscale = 2*pwrscale;
       bc_multiply (power, power, &power, pwrscale);
       exponent = exponent >> 1;
     }
   temp = bc_copy_num (power);
   calcscale = pwrscale;
   exponent = exponent >> 1;

   /* Do the calculation. */
   while (exponent > 0)
     {
       pwrscale = 2*pwrscale;
       bc_multiply (power, power, &power, pwrscale);
       if ((exponent & 1) == 1) {
	 calcscale = pwrscale + calcscale;
	 bc_multiply (temp, power, &temp, calcscale);
       }
       exponent = exponent >> 1;
     }

   /* Assign the value. */
   if (neg)
     {
       bc_divide (_one_, temp, result, rscale);
       bc_free_num (&temp);
     }
   else
     {
       bc_free_num (result);
       *result = temp;
       if ((*result)->n_scale > rscale)
	 (*result)->n_scale = rscale;
     }
   bc_free_num (&power);
}

/* Take the square root NUM and return it in NUM with SCALE digits
   after the decimal place. */

int
bc_sqrt (num, scale)
     bc_num *num;
     int scale;
{
  int rscale, cmp_res, done;
  int cscale;
  bc_num guess, guess1, point5, diff;

  /* Initial checks. */
  cmp_res = bc_compare (*num, _zero_);
  if (cmp_res < 0)
    return 0;		/* error */
  else
    {
      if (cmp_res == 0)
	{
	  bc_free_num (num);
	  *num = bc_copy_num (_zero_);
	  return 1;
	}
    }
  cmp_res = bc_compare (*num, _one_);
  if (cmp_res == 0)
    {
      bc_free_num (num);
      *num = bc_copy_num (_one_);
      return 1;
    }

  /* Initialize the variables. */
  rscale = MAX (scale, (*num)->n_scale);
  bc_init_num(&guess);
  bc_init_num(&guess1);
  bc_init_num(&diff);
  point5 = bc_new_num (1,1);
  point5->n_value[1] = 5;


  /* Calculate the initial guess. */
  if (cmp_res < 0)
    {
      /* The number is between 0 and 1.  Guess should start at 1. */
      guess = bc_copy_num (_one_);
      cscale = (*num)->n_scale;
    }
  else
    {
      /* The number is greater than 1.  Guess should start at 10^(exp/2). */
      bc_int2num (&guess,10);

      bc_int2num (&guess1,(*num)->n_len);
      bc_multiply (guess1, point5, &guess1, 0);
      guess1->n_scale = 0;
      bc_raise (guess, guess1, &guess, 0);
      bc_free_num (&guess1);
      cscale = 3;
    }

  /* Find the square root using Newton's algorithm. */
  done = FALSE;
  while (!done)
    {
      bc_free_num (&guess1);
      guess1 = bc_copy_num (guess);
      bc_divide (*num, guess, &guess, cscale);
      bc_add (guess, guess1, &guess, 0);
      bc_multiply (guess, point5, &guess, cscale);
      bc_sub (guess, guess1, &diff, cscale+1);
      if (bc_is_near_zero (diff, cscale))
	{
	  if (cscale < rscale+1)
	    cscale = MIN (cscale*3, rscale+1);
	  else
	    done = TRUE;
	}
    }

  /* Assign the number and clean up. */
  bc_free_num (num);
  bc_divide (guess,_one_,num,rscale);
  bc_free_num (&guess);
  bc_free_num (&guess1);
  bc_free_num (&point5);
  bc_free_num (&diff);
  return 1;
}


/* The following routines provide output for bcd numbers package
   using the rules of POSIX bc for output. */

/* This structure is used for saving digits in the conversion process. */
typedef struct stk_rec {
	long  digit;
	struct stk_rec *next;
} stk_rec;

/* The reference string for digits. */
static char ref_str[] = "0123456789ABCDEF";


/* A special output routine for "multi-character digits."  Exactly
   SIZE characters must be output for the value VAL.  If SPACE is
   non-zero, we must output one space before the number.  OUT_CHAR
   is the actual routine for writing the characters. */

void
bc_out_long (val, size, space, out_char)
     long val;
     int size, space;
#ifdef __STDC__
     void (*out_char)(int);
#else
     void (*out_char)();
#endif
{
  char digits[40];
  int len, ix;

  if (space) (*out_char) (' ');
  sprintf (digits, "%ld", val);
  len = strlen (digits);
  while (size > len)
    {
      (*out_char) ('0');
      size--;
    }
  for (ix=0; ix < len; ix++)
    (*out_char) (digits[ix]);
}

/* Output of a bcd number.  NUM is written in base O_BASE using OUT_CHAR
   as the routine to do the actual output of the characters. */

void
bc_out_num (num, o_base, out_char, leading_zero)
     bc_num num;
     int o_base;
#ifdef __STDC__
     void (*out_char)(int);
#else
     void (*out_char)();
#endif
     int leading_zero;
{
  char *nptr;
  int  index, fdigit, pre_space;
  stk_rec *digits, *temp;
  bc_num int_part, frac_part, base, cur_dig, t_num, max_o_digit;

  /* The negative sign if needed. */
  if (num->n_sign == MINUS) (*out_char) ('-');

  /* Output the number. */
  if (bc_is_zero (num))
    (*out_char) ('0');
  else
    if (o_base == 10)
      {
	/* The number is in base 10, do it the fast way. */
	nptr = num->n_value;
	if (num->n_len > 1 || *nptr != 0)
	  for (index=num->n_len; index>0; index--)
	    (*out_char) (BCD_CHAR(*nptr++));
	else
	  nptr++;

	if (leading_zero && bc_is_zero (num))
	  (*out_char) ('0');

	/* Now the fraction. */
	if (num->n_scale > 0)
	  {
	    (*out_char) ('.');
	    for (index=0; index<num->n_scale; index++)
	      (*out_char) (BCD_CHAR(*nptr++));
	  }
      }
    else
      {
	/* special case ... */
	if (leading_zero && bc_is_zero (num))
	  (*out_char) ('0');

	/* The number is some other base. */
	digits = NULL;
	bc_init_num (&int_part);
	bc_divide (num, _one_, &int_part, 0);
	bc_init_num (&frac_part);
	bc_init_num (&cur_dig);
	bc_init_num (&base);
	bc_sub (num, int_part, &frac_part, 0);
	/* Make the INT_PART and FRAC_PART positive. */
	int_part->n_sign = PLUS;
	frac_part->n_sign = PLUS;
	bc_int2num (&base, o_base);
	bc_init_num (&max_o_digit);
	bc_int2num (&max_o_digit, o_base-1);


	/* Get the digits of the integer part and push them on a stack. */
	while (!bc_is_zero (int_part))
	  {
	    bc_modulo (int_part, base, &cur_dig, 0);
	    temp = (stk_rec *) malloc (sizeof(stk_rec));
	    if (temp == NULL) bc_out_of_memory();
	    temp->digit = bc_num2long (cur_dig);
	    temp->next = digits;
	    digits = temp;
	    bc_divide (int_part, base, &int_part, 0);
	  }

	/* Print the digits on the stack. */
	if (digits != NULL)
	  {
	    /* Output the digits. */
	    while (digits != NULL)
	      {
		temp = digits;
		digits = digits->next;
		if (o_base <= 16)
		  (*out_char) (ref_str[ (int) temp->digit]);
		else
		  bc_out_long (temp->digit, max_o_digit->n_len, 1, out_char);
		free (temp);
	      }
	  }

	/* Get and print the digits of the fraction part. */
	if (num->n_scale > 0)
	  {
	    (*out_char) ('.');
	    pre_space = 0;
	    t_num = bc_copy_num (_one_);
	    while (t_num->n_len <= num->n_scale) {
	      bc_multiply (frac_part, base, &frac_part, num->n_scale);
	      fdigit = bc_num2long (frac_part);
	      bc_int2num (&int_part, fdigit);
	      bc_sub (frac_part, int_part, &frac_part, 0);
	      if (o_base <= 16)
		(*out_char) (ref_str[fdigit]);
	      else {
		bc_out_long (fdigit, max_o_digit->n_len, pre_space, out_char);
		pre_space = 1;
	      }
	      bc_multiply (t_num, base, &t_num, 0);
	    }
	    bc_free_num (&t_num);
	  }

	/* Clean up. */
	bc_free_num (&int_part);
	bc_free_num (&frac_part);
	bc_free_num (&base);
	bc_free_num (&cur_dig);
	bc_free_num (&max_o_digit);
      }
}
/* Convert a number NUM to a long.  The function returns only the integer
   part of the number.  For numbers that are too large to represent as
   a long, this function returns a zero.  This can be detected by checking
   the NUM for zero after having a zero returned. */

long
bc_num2long (num)
     bc_num num;
{
  long val;
  char *nptr;
  int  index;

  /* Extract the int value, ignore the fraction. */
  val = 0;
  nptr = num->n_value;
  for (index=num->n_len; (index>0) && (val<=(LONG_MAX/BASE)); index--)
    val = val*BASE + *nptr++;

  /* Check for overflow.  If overflow, return zero. */
  if (index>0) val = 0;
  if (val < 0) val = 0;

  /* Return the value. */
  if (num->n_sign == PLUS)
    return (val);
  else
    return (-val);
}


/* Convert an integer VAL to a bc number NUM. */

void
bc_int2num (num, val)
     bc_num *num;
     int val;
{
  char buffer[30];
  char *bptr, *vptr;
  int  ix = 1;
  char neg = 0;

  /* Sign. */
  if (val < 0)
    {
      neg = 1;
      val = -val;
    }

  /* Get things going. */
  bptr = buffer;
  *bptr++ = val % BASE;
  val = val / BASE;

  /* Extract remaining digits. */
  while (val != 0)
    {
      *bptr++ = val % BASE;
      val = val / BASE;
      ix++; 		/* Count the digits. */
    }

  /* Make the number. */
  bc_free_num (num);
  *num = bc_new_num (ix, 0);
  if (neg) (*num)->n_sign = MINUS;

  /* Assign the digits. */
  vptr = (*num)->n_value;
  while (ix-- > 0)
    *vptr++ = *--bptr;
}

/* Convert a numbers to a string.  Base 10 only.*/

char
*num2str (num)
      bc_num num;
{
  char *str, *sptr;
  char *nptr;
  int  index, signch;

  /* Allocate the string memory. */
  signch = ( num->n_sign == PLUS ? 0 : 1 );  /* Number of sign chars. */
  if (num->n_scale > 0)
    str = (char *) malloc (num->n_len + num->n_scale + 2 + signch);
  else
    str = (char *) malloc (num->n_len + 1 + signch);
  if (str == NULL) bc_out_of_memory();

  /* The negative sign if needed. */
  sptr = str;
  if (signch) *sptr++ = '-';

  /* Load the whole number. */
  nptr = num->n_value;
  for (index=num->n_len; index>0; index--)
    *sptr++ = BCD_CHAR(*nptr++);

  /* Now the fraction. */
  if (num->n_scale > 0)
    {
      *sptr++ = '.';
      for (index=0; index<num->n_scale; index++)
	*sptr++ = BCD_CHAR(*nptr++);
    }

  /* Terminate the string and return it! */
  *sptr = '\0';
  return (str);
}
/* Convert strings to bc numbers.  Base 10 only.*/

void
bc_str2num (num, str, scale)
     bc_num *num;
     char *str;
     int scale;
{
  int digits, strscale;
  char *ptr, *nptr;
  char zero_int;

  /* Prepare num. */
  bc_free_num (num);

  /* Check for valid number and count digits. */
  ptr = str;
  digits = 0;
  strscale = 0;
  zero_int = FALSE;
  if ( (*ptr == '+') || (*ptr == '-'))  ptr++;  /* Sign */
  while (*ptr == '0') ptr++;			/* Skip leading zeros. */
  while (isdigit((int)*ptr)) ptr++, digits++;	/* digits */
  if (*ptr == '.') ptr++;			/* decimal point */
  while (isdigit((int)*ptr)) ptr++, strscale++;	/* digits */
  if ((*ptr != '\0') || (digits+strscale == 0))
    {
      *num = bc_copy_num (_zero_);
      return;
    }

  /* Adjust numbers and allocate storage and initialize fields. */
  strscale = MIN(strscale, scale);
  if (digits == 0)
    {
      zero_int = TRUE;
      digits = 1;
    }
  *num = bc_new_num (digits, strscale);

  /* Build the whole number. */
  ptr = str;
  if (*ptr == '-')
    {
      (*num)->n_sign = MINUS;
      ptr++;
    }
  else
    {
      (*num)->n_sign = PLUS;
      if (*ptr == '+') ptr++;
    }
  while (*ptr == '0') ptr++;			/* Skip leading zeros. */
  nptr = (*num)->n_value;
  if (zero_int)
    {
      *nptr++ = 0;
      digits = 0;
    }
  for (;digits > 0; digits--)
    *nptr++ = CH_VAL(*ptr++);


  /* Build the fractional part. */
  if (strscale > 0)
    {
      ptr++;  /* skip the decimal point! */
      for (;strscale > 0; strscale--)
	*nptr++ = CH_VAL(*ptr++);
    }
}

/* pn prints the number NUM in base 10. */

static void
out_char (int c)
{
  putchar(c);
}


void
pn (num)
     bc_num num;
{
  bc_out_num (num, 10, out_char, 0);
  out_char ('\n');
}


/* pv prints a character array as if it was a string of bcd digits. */
void
pv (name, num, len)
     char *name;
     unsigned char *num;
     int len;
{
  int i;
  printf ("%s=", name);
  for (i=0; i<len; i++) printf ("%c",BCD_CHAR(num[i]));
  printf ("\n");
}
