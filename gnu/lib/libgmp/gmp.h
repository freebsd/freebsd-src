/* gmp.h -- Definitions for GNU multiple precision functions.

Copyright (C) 1991, 1993 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

The GNU MP Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the GNU MP Library; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef __GMP_H__
#define __GMP_H__

#define __GNU_MP__

#ifndef __MP_H__
#define __need_size_t
#include <stddef.h>
#endif

#ifndef MINT
#ifndef __MP_SMALL__
typedef struct
{
  long int alloc;		/* Number of *limbs* allocated and pointed
				   to by the D field.  */
  long int size;		/* abs(SIZE) is the number of limbs
				   the last field points to.  If SIZE
				   is negative this is a negative
				   number.  */
  unsigned long int *d;		/* Pointer to the limbs.  */
} __MP_INT;
#else
typedef struct
{
  short int alloc;		/* Number of *limbs* allocated and pointed
				   to by the D field.  */
  short int size;		/* abs(SIZE) is the number of limbs
				   the last field points to.  If SIZE
				   is negative this is a negative
				   number.  */
  unsigned long int *d;		/* Pointer to the limbs.  */
} __MP_INT;
#endif
#endif

#define MP_INT __MP_INT

typedef unsigned long int	mp_limb;
typedef long int		mp_limb_signed;
typedef mp_limb *		mp_ptr;
#ifdef __STDC__
typedef const mp_limb *		mp_srcptr;
#else
typedef mp_limb *		mp_srcptr;
#endif
typedef long int		mp_size;

/* Structure for rational numbers.  Zero is represented as 0/any, i.e.
   the denominator is ignored.  Negative numbers have the sign in
   the numerator.  */
typedef struct
{
  MP_INT num;
  MP_INT den;
#if 0
  long int num_alloc;		/* Number of limbs allocated
				   for the numerator.  */
  long int num_size;		/* The absolute value of this field is the
				   length of the numerator; the sign is the
				   sign of the entire rational number.  */
  mp_ptr num;			/* Pointer to the numerator limbs.  */
  long int den_alloc;		/* Number of limbs allocated
				   for the denominator.  */
  long int den_size;		/* Length of the denominator.  (This field
				   should always be positive.) */
  mp_ptr den;			/* Pointer to the denominator limbs.  */
#endif
} MP_RAT;

#ifdef __STDC__
void mp_set_memory_functions (void *(*) (size_t),
			      void *(*) (void *, size_t, size_t),
			      void (*) (void *, size_t));

/**************** Integer (i.e. Z) routines.  ****************/

void mpz_init (MP_INT *);
void mpz_set (MP_INT *, const MP_INT *);
void mpz_set_ui (MP_INT *, unsigned long int);
void mpz_set_si (MP_INT *, signed long int);
int mpz_set_str (MP_INT *, const char *, int);
void mpz_init_set (MP_INT *, const MP_INT *);
void mpz_init_set_ui (MP_INT *, unsigned long int);
void mpz_init_set_si (MP_INT *, signed long int);
int mpz_init_set_str (MP_INT *, const char *, int);
unsigned long int mpz_get_ui (const MP_INT *);
signed long int mpz_get_si (const MP_INT *);
char * mpz_get_str (char *, int, const MP_INT *);
void mpz_clear (MP_INT *);
void * _mpz_realloc (MP_INT *, mp_size);
void mpz_add (MP_INT *, const MP_INT *, const MP_INT *);
void mpz_add_ui (MP_INT *, const MP_INT *, unsigned long int);
void mpz_sub (MP_INT *, const MP_INT *, const MP_INT *);
void mpz_sub_ui (MP_INT *, const MP_INT *, unsigned long int);
void mpz_mul (MP_INT *, const MP_INT *, const MP_INT *);
void mpz_mul_ui (MP_INT *, const MP_INT *, unsigned long int);
void mpz_div (MP_INT *, const MP_INT *, const MP_INT *);
void mpz_div_ui (MP_INT *, const MP_INT *, unsigned long int);
void mpz_mod (MP_INT *, const MP_INT *, const MP_INT *);
void mpz_mod_ui (MP_INT *, const MP_INT *, unsigned long int);
void mpz_divmod (MP_INT *, MP_INT *, const MP_INT *, const MP_INT *);
void mpz_divmod_ui (MP_INT *, MP_INT *, const MP_INT *, unsigned long int);
void mpz_mdiv (MP_INT *, const MP_INT *, const MP_INT *);
void mpz_mdiv_ui (MP_INT *, const MP_INT *, unsigned long int);
void mpz_mmod (MP_INT *, const MP_INT *, const MP_INT *);
unsigned long int mpz_mmod_ui (MP_INT *, const MP_INT *, unsigned long int);
void mpz_mdivmod (MP_INT *, MP_INT *, const MP_INT *, const MP_INT *);
unsigned long int mpz_mdivmod_ui (MP_INT *, MP_INT *, const MP_INT *,
				  unsigned long int);
void mpz_sqrt (MP_INT *, const MP_INT *);
void mpz_sqrtrem (MP_INT *, MP_INT *, const MP_INT *);
int mpz_perfect_square_p (const MP_INT *);
int mpz_probab_prime_p (const MP_INT *, int);
void mpz_powm (MP_INT *, const MP_INT *, const MP_INT *, const MP_INT *);
void mpz_powm_ui (MP_INT *, const MP_INT *, unsigned long int, const MP_INT *);
void mpz_pow_ui (MP_INT *, const MP_INT *, unsigned long int);
void mpz_fac_ui (MP_INT *, unsigned long int);
void mpz_gcd (MP_INT *, const MP_INT *, const MP_INT *);
void mpz_gcdext (MP_INT *, MP_INT *, MP_INT *, const MP_INT *, const MP_INT *);
void mpz_neg (MP_INT *, const MP_INT *);
void mpz_com (MP_INT *, const MP_INT *);
void mpz_abs (MP_INT *, const MP_INT *);
int mpz_cmp (const MP_INT *, const MP_INT *);
int mpz_cmp_ui (const MP_INT *, unsigned long int);
int mpz_cmp_si (const MP_INT *, signed long int);
void mpz_mul_2exp (MP_INT *, const MP_INT *, unsigned long int);
void mpz_div_2exp (MP_INT *, const MP_INT *, unsigned long int);
void mpz_mod_2exp (MP_INT *, const MP_INT *, unsigned long int);
void mpz_and (MP_INT *, const MP_INT *, const MP_INT *);
void mpz_ior (MP_INT *, const MP_INT *, const MP_INT *);
void mpz_xor (MP_INT *, const MP_INT *, const MP_INT *);

#if defined (FILE) || defined (_STDIO_H) || defined (__STDIO_H__)
void mpz_inp_raw (MP_INT *, FILE *);
void mpz_inp_str (MP_INT *, FILE *, int);
void mpz_out_raw (FILE *, const MP_INT *);
void mpz_out_str (FILE *, int, const MP_INT *);
#endif

void mpz_array_init (MP_INT [], size_t, mp_size);
void mpz_random (MP_INT *, mp_size);
void mpz_random2 (MP_INT *, mp_size);
size_t mpz_size (const MP_INT *);
size_t mpz_sizeinbase (const MP_INT *, int);

/**************** Rational (i.e. Q) routines.  ****************/

void mpq_init (MP_RAT *);
void mpq_clear (MP_RAT *);
void mpq_set (MP_RAT *, const MP_RAT *);
void mpq_set_ui (MP_RAT *, unsigned long int, unsigned long int);
void mpq_set_si (MP_RAT *, signed long int, unsigned long int);
void mpq_add (MP_RAT *, const MP_RAT *, const MP_RAT *);
void mpq_sub (MP_RAT *, const MP_RAT *, const MP_RAT *);
void mpq_mul (MP_RAT *, const MP_RAT *, const MP_RAT *);
void mpq_div (MP_RAT *, const MP_RAT *, const MP_RAT *);
void mpq_neg (MP_RAT *, const MP_RAT *);
int mpq_cmp (const MP_RAT *, const MP_RAT *);
void mpq_inv (MP_RAT *, const MP_RAT *);
void mpq_set_num (MP_RAT *, const MP_INT *);
void mpq_set_den (MP_RAT *, const MP_INT *);
void mpq_get_num (MP_INT *, const MP_RAT *);
void mpq_get_den (MP_INT *, const MP_RAT *);

/************ Low level positive-integer (i.e. N) routines.  ************/

mp_limb mpn_add (mp_ptr, mp_srcptr, mp_size, mp_srcptr, mp_size);
mp_size mpn_sub (mp_ptr, mp_srcptr, mp_size, mp_srcptr, mp_size);
mp_size mpn_mul (mp_ptr, mp_srcptr, mp_size, mp_srcptr, mp_size);
mp_size mpn_div (mp_ptr, mp_ptr, mp_size, mp_srcptr, mp_size);
mp_limb mpn_divmod_1 (mp_ptr, mp_srcptr, mp_size, mp_limb);
mp_limb mpn_mod_1 (mp_srcptr, mp_size, mp_limb);
mp_limb mpn_lshift (mp_ptr, mp_srcptr, mp_size, unsigned int);
mp_size mpn_rshift (mp_ptr, mp_srcptr, mp_size, unsigned int);
mp_size mpn_rshiftci (mp_ptr, mp_srcptr, mp_size, unsigned int, mp_limb);
mp_size mpn_sqrt (mp_ptr, mp_ptr, mp_srcptr, mp_size);
int mpn_cmp (mp_srcptr, mp_srcptr, mp_size);

#else /* ! __STDC__ */
void mp_set_memory_functions ();

/**************** Integer (i.e. Z) routines.  ****************/

void mpz_init ();
void mpz_set ();
void mpz_set_ui ();
void mpz_set_si ();
int mpz_set_str ();
void mpz_init_set ();
void mpz_init_set_ui ();
void mpz_init_set_si ();
int mpz_init_set_str ();
unsigned long int mpz_get_ui ();
long int mpz_get_si ();
char * mpz_get_str ();
void mpz_clear ();
void * _mpz_realloc ();
void mpz_add ();
void mpz_add_ui ();
void mpz_sub ();
void mpz_sub_ui ();
void mpz_mul ();
void mpz_mul_ui ();
void mpz_div ();
void mpz_div_ui ();
void mpz_mod ();
void mpz_mod_ui ();
void mpz_divmod ();
void mpz_divmod_ui ();
void mpz_mdiv ();
void mpz_mdiv_ui ();
void mpz_mmod ();
unsigned long int mpz_mmod_ui ();
void mpz_mdivmod ();
unsigned long int mpz_mdivmod_ui ();
void mpz_sqrt ();
void mpz_sqrtrem ();
int mpz_perfect_square_p ();
int mpz_probab_prime_p ();
void mpz_powm ();
void mpz_powm_ui ();
void mpz_pow_ui ();
void mpz_fac_ui ();
void mpz_gcd ();
void mpz_gcdext ();
void mpz_neg ();
void mpz_com ();
void mpz_abs ();
int mpz_cmp ();
int mpz_cmp_ui ();
int mpz_cmp_si ();
void mpz_mul_2exp ();
void mpz_div_2exp ();
void mpz_mod_2exp ();
void mpz_and ();
void mpz_ior ();
void mpz_xor ();

void mpz_inp_raw ();
void mpz_inp_str ();
void mpz_out_raw ();
void mpz_out_str ();

void mpz_array_init ();
void mpz_random ();
void mpz_random2 ();
size_t mpz_size ();
size_t mpz_sizeinbase ();

/**************** Rational (i.e. Q) routines.  ****************/

void mpq_init ();
void mpq_clear ();
void mpq_set ();
void mpq_set_ui ();
void mpq_set_si ();
void mpq_add ();
void mpq_sub ();
void mpq_mul ();
void mpq_div ();
void mpq_neg ();
int mpq_cmp ();
void mpq_inv ();
void mpq_set_num ();
void mpq_set_den ();
void mpq_get_num ();
void mpq_get_den ();

/************ Low level positive-integer (i.e. N) routines.  ************/

mp_limb mpn_add ();
mp_size mpn_sub ();
mp_size mpn_mul ();
mp_size mpn_div ();
mp_limb mpn_lshift ();
mp_size mpn_rshift ();
mp_size mpn_rshiftci ();
int mpn_cmp ();
#endif /* __STDC__ */

#endif /* __GMP_H__ */
