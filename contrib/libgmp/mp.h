/* mp.h -- Definitions for Berkeley compatible multiple precision functions.

Copyright (C) 1991, 1993, 1994, 1995, 1996 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
License for more details.

You should have received a copy of the GNU Library General Public License
along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA. */

#ifndef __MP_H__

#ifndef __GNU_MP__
#define __GNU_MP__ 2
#define __need_size_t
#include <stddef.h>
#undef __need_size_t

#if defined (__STDC__) || defined (__cplusplus)
#define __gmp_const const
#else
#define __gmp_const
#endif

#if defined (__GNUC__)
#define __gmp_inline __inline__
#else
#define __gmp_inline
#endif

#ifndef _EXTERN_INLINE
#ifdef __GNUC__
#define _EXTERN_INLINE extern __inline__
#else
#define _EXTERN_INLINE static
#endif
#endif

#ifdef _SHORT_LIMB
typedef unsigned int		mp_limb_t;
typedef int			mp_limb_signed_t;
#else
#ifdef _LONG_LONG_LIMB
typedef unsigned long long int	mp_limb_t;
typedef long long int		mp_limb_signed_t;
#else
typedef unsigned long int	mp_limb_t;
typedef long int		mp_limb_signed_t;
#endif
#endif

typedef mp_limb_t *		mp_ptr;
typedef __gmp_const mp_limb_t *	mp_srcptr;
typedef int			mp_size_t;
typedef long int		mp_exp_t;

#ifndef __MP_SMALL__
typedef struct
{
  mp_size_t _mp_alloc;		/* Number of *limbs* allocated and pointed
				   to by the D field.  */
  mp_size_t _mp_size;		/* abs(SIZE) is the number of limbs
				   the last field points to.  If SIZE
				   is negative this is a negative
				   number.  */
  mp_limb_t *_mp_d;			/* Pointer to the limbs.  */
} __mpz_struct;
#else
typedef struct
{
  short int _mp_alloc;		/* Number of *limbs* allocated and pointed
				   to by the D field.  */
  short int _mp_size;		/* abs(SIZE) is the number of limbs
				   the last field points to.  If SIZE
				   is negative this is a negative
				   number.  */
  mp_limb_t *_mp_d;		/* Pointer to the limbs.  */
} __mpz_struct;
#endif
#endif /* __GNU_MP__ */

/* User-visible types.  */
typedef __mpz_struct MINT;

#ifdef __STDC__
void mp_set_memory_functions (void *(*) (size_t),
			      void *(*) (void *, size_t, size_t),
			      void (*) (void *, size_t));
MINT *itom (signed short int);
MINT *xtom (const char *);
void move (const MINT *, MINT *);
void madd (const MINT *, const MINT *, MINT *);
void msub (const MINT *, const MINT *, MINT *);
void mult (const MINT *, const MINT *, MINT *);
void mdiv (const MINT *, const MINT *, MINT *, MINT *);
void sdiv (const MINT *, signed short int, MINT *, signed short int *);
void msqrt (const MINT *, MINT *, MINT *);
void pow (const MINT *, const MINT *, const MINT *, MINT *);
void rpow (const MINT *, signed short int, MINT *);
void gcd (const MINT *, const MINT *, MINT *);
int mcmp (const MINT *, const MINT *);
void min (MINT *);
void mout (const MINT *);
char *mtox (const MINT *);
void mfree (MINT *);

#else

void mp_set_memory_functions ();
MINT *itom ();
MINT *xtom ();
void move ();
void madd ();
void msub ();
void mult ();
void mdiv ();
void sdiv ();
void msqrt ();
void pow ();
void rpow ();
void gcd ();
int mcmp ();
void min ();
void mout ();
char *mtox ();
void mfree ();
#endif

#define __MP_H__
#endif /* __MP_H__ */
