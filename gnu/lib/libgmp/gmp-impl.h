/* Include file for internal GNU MP types and definitions.

Copyright (C) 1991 Free Software Foundation, Inc.

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
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.   */

#if defined (__GNUC__) || defined (__sparc__) || defined (sparc)
#define alloca __builtin_alloca
#endif

#ifndef NULL
#define NULL 0L
#endif

#if defined (__GNUC__)
volatile void abort (void);
#else
#define inline			/* Empty */
void *alloca();
#endif

#define ABS(x) (x >= 0 ? x : -x)

#include "gmp-mparam.h"

#ifdef __STDC__
void *malloc (size_t);
void *realloc (void *, size_t);
void free (void *);

extern void *	(*_mp_allocate_func) (size_t);
extern void *	(*_mp_reallocate_func) (void *, size_t, size_t);
extern void	(*_mp_free_func) (void *, size_t);

void *_mp_default_allocate (size_t);
void *_mp_default_reallocate (void *, size_t, size_t);
void _mp_default_free (void *, size_t);

char *_mpz_get_str (char *, int, const MP_INT *);
int _mpz_set_str (MP_INT *, const char *, int);
void _mpz_impl_sqrt (MP_INT *, MP_INT *, const MP_INT *);
#else
#define const			/* Empty */
#define signed			/* Empty */

void *malloc ();
void *realloc ();
void free ();

extern void *	(*_mp_allocate_func) ();
extern void *	(*_mp_reallocate_func) ();
extern void	(*_mp_free_func) ();

void *_mp_default_allocate ();
void *_mp_default_reallocate ();
void _mp_default_free ();

char *_mpz_get_str ();
int _mpz_set_str ();
void _mpz_impl_sqrt ();
#endif

/* Copy NLIMBS *limbs* from SRC to DST.  */
#define MPN_COPY(DST, SRC, NLIMBS) \
  do {									\
    mp_size i;								\
    for (i = 0; i < (NLIMBS); i++)					\
      (DST)[i] = (SRC)[i];						\
  } while (0)
/* Zero NLIMBS *limbs* AT DST.  */
#define MPN_ZERO(DST, NLIMBS) \
  do {									\
    mp_size i;								\
    for (i = 0; i < (NLIMBS); i++)					\
      (DST)[i] = 0;							\
  } while (0)

/* Initialize the MP_INT X with space for NLIMBS limbs.
   X should be a temporary variable, and it will be automatically
   cleared out when the running function returns.  */
#define MPZ_TMP_INIT(X, NLIMBS) \
  do {									\
    (X)->alloc = (NLIMBS);						\
    (X)->d = (mp_ptr) alloca ((NLIMBS) * BYTES_PER_MP_LIMB);		\
  } while (0)

/* Structure for conversion between internal binary format and
   strings in base 2..36.  */
struct bases
{
  /* Number of digits in the conversion base that always fits in
     an mp_limb.  For example, for base 10 this is 10, since
     2**32 = 4294967296 has ten digits.  */
  int chars_per_limb;

  /* big_base is conversion_base**chars_per_limb, i.e. the biggest
     number that fits a word, built by factors of conversion_base.
     Exception: For 2, 4, 8, etc, big_base is log2(base), i.e. the
     number of bits used to represent each digit in the base.  */
  mp_limb big_base;

  /* big_base_inverted is a BITS_PER_MP_LIMB bit approximation to
     1/big_base, represented as a fixed-point number.  Instead of
     dividing by big_base an application can choose to multiply
     by big_base_inverted.  */
  mp_limb big_base_inverted;

  /* log(2)/log(conversion_base) */
  float chars_per_bit_exactly;
};

extern const struct bases __mp_bases[37];
