/* $Id: align.h,v 1.1 1994/03/06 22:59:26 nate Exp $ */
#ifndef __ALIGN_H__
#define __ALIGN_H__
/* 
 * 'union word' must be aligned to the most pessimistic alignment needed
 * on the architecture (See align.h) since all pointers returned by
 * malloc and friends are really pointers to a Word. This is the job of
 * the 'foo' field in the union, which actually decides the size. (On
 * Sun3s, 1 int/long (4 bytes) is good enough, on Sun4s, 8 bytes are
 * necessary, i.e. 2 ints/longs)
 */
/*
 * 'union word' should also be the size necessary to ensure that an
 * sbrk() immediately following an sbrk(sizeof(union word) * n) returns
 * an address one higher than the first sbrk. i.e. contiguous space from
 * successive sbrks. (Most sbrks will do this regardless of the size -
 * Sun's doesnt) This is not vital - the malloc will work, but will not
 * be able to coalesce between sbrk'ed segments.
 */

#if defined(sparc) || defined(__sparc__) || defined(__sgi)
/*
 * Sparcs require doubles to be aligned on double word, SGI R4000 based
 * machines are 64 bit, this is the conservative way out
 */
/* this will waste space on R4000s or the 64bit UltraSparc */
# define ALIGN long foo[2]
# define NALIGN 8
#endif

#if defined(__alpha)
/* 64 bit */
# define ALIGN long foo
# define NALIGN 8
#endif

/* This default seems to work on most 32bit machines */
#ifndef ALIGN
# define ALIGN long foo
# define NALIGN 4
#endif

/* Align with power of 2 */
#define	SIMPLEALIGN(X, N) (univptr_t)(((size_t)((char *)(X) + (N-1))) & ~(N-1))

#if	NALIGN == 2 || NALIGN == 4 || NALIGN == 8 || NALIGN == 16
  /* if NALIGN is a power of 2, the next line will do ok */
# define	ALIGNPTR(X) SIMPLEALIGN(X, NALIGN)
#else
  /* otherwise we need the generic version; hope the compiler isn't too smart */
  static size_t _N = NALIGN;
# define	ALIGNPTR(X) (univptr_t)((((size_t)((univptr_t)(X)+(NALIGN-1)))/_N)*_N)
#endif

/*
 * If your sbrk does not return blocks that are aligned to the
 * requirements of malloc(), as specified by ALIGN above, then you need
 * to define SBRKEXTRA so that it gets the extra memory needed to find
 * an alignment point. Not needed on Suns on which sbrk does return
 * properly aligned blocks. But on U*x Vaxen, A/UX and UMIPS at least,
 * you need to do this. It is safer to take the non Sun route (!! since
 * the non-sun part also works on Suns, maybe we should just remove the
 * Sun ifdef)
 */
#if ! (defined(sun) || defined(__sun__))
# define SBRKEXTRA		(sizeof(Word) - 1)
# define SBRKALIGN(x)	ALIGNPTR(x)
#else
# define SBRKEXTRA		0
# define SBRKALIGN(x)	(x)
#endif

#ifndef BITSPERBYTE
  /*
   * These values should work with any binary representation of integers
   * where the high-order bit contains the sign. Should be in values.h,
   * but just in case...
   */
  /* a number used normally for size of a shift */
# if gcos
#  define BITSPERBYTE	9
# else /* ! gcos */
#  define BITSPERBYTE	8
# endif /* gcos */
#endif /* BITSPERBYTE */

#ifndef BITS
# define BITS(type)	(BITSPERBYTE * (int) sizeof(type))
#endif /* BITS */

/* size_t with only the high-order bit turned on */
#define HIBITSZ (((size_t) 1) << (BITS(size_t) - 1))

#endif /* __ALIGN_H__ */ /* Do not add anything after this line */
