/*  Author: Mark Moraes <moraes@csri.toronto.edu> */
/* $Id: defs.h,v 1.1 1994/03/06 22:59:29 nate Exp $ */
#ifndef __DEFS_H__
#define __DEFS_H__
/* 
 *  This file includes all relevant include files, and defines various
 *  types and constants. It also defines lots of macros which operate on
 *  the malloc blocks and pointers, to try and keep the ugliness
 *  abstracted away from the source code.
 */

#include <stdio.h>
#include <errno.h>
extern int errno;		/* Some errno.h don't declare this */

/* 
 *  ANSI defines malloc to return void *, and take size_t as an
 *  argument, while present Unix convention is char * and unsigned int
 *  respectively. This is not ifdef'ed with STDC because you may want to
 *  compile a Unix malloc with an ANSI C compiler or vice versa. Note
 *  that sys/types.h on BSD Unix systems defines size_t to be int or
 *  long. memsize_t is for routines that expect int in Unix and size_t
 *  in ANSI.
 */
#ifdef STDHEADERS
# define univptr_t		void *
# define memsize_t	size_t
#else	/* ! STDHEADERS */
# define univptr_t		char *
# define	size_t		unsigned int
# define memsize_t	int
#endif	/* STDHEADERS */

#ifndef REGISTER
# ifdef __GNUC__
   /* gcc probably does better register allocation than I do */
#  define REGISTER
# else
#  define REGISTER register
# endif
#endif

/*
 *  Just in case you want an ANSI malloc without an ANSI compiler, or
 *  ANSI includes
 */
#if defined(STDHEADERS) && !defined(offsetof) && !defined(__FreeBSD__)
# define size_t		unsigned long
#endif

#if defined(__STDC__)
# define proto(x)	x
#else
# define proto(x)	()
# define const
# define volatile
#endif

#include "externs.h"
#include "assert.h"

#ifndef EXIT_FAILURE
# define EXIT_FAILURE	1
#endif

#ifdef __STDC__
# include <limits.h>
# ifndef BITSPERBYTE
#  define BITSPERBYTE	CHAR_BIT
# endif
#else
# include <values.h>
#endif
#include "align.h"	/* Needs BITSPERBYTE */

/* 
 *  We assume that FREE is a 0 bit, and the for a free block,
 *  SIZE(p) == SIZEMASK(p) as an optimization to avoid an unnecessary
 *  masking operation with SIZEMASK. See FREESIZE below. Or'ing with
 *  ALLOCED should turn on the high bit, And'ing with SIZEMASK should
 *  turn it off.
 */

#define FREE		((size_t) 0)
#define ALLOCED		(HIBITSZ)
#define SIZEMASK 	(~HIBITSZ)

union word { /* basic unit of storage */
	size_t size;		/* size of this block + 1 bit status */
	union word *next;	/* next free block */
	union word *prev;	/* prev free block */
	univptr_t ptr;		/* stops lint complaining, keeps alignment */
	char c;
	int i;
	char *cp;
	char **cpp;
	int *ip;
	int **ipp;
	ALIGN;			/* alignment stuff - wild fun */
};

typedef union word Word;

/*
 *  WARNING - Many of these macros are UNSAFE because they have multiple
 *  evaluations of the arguments. Use with care, avoid side-effects.
 */
/*
 * These macros define operations on a pointer to a block. The zero'th
 * word of a block is the size field, where the top bit is 1 if the
 * block is allocated. This word is also referred to as the starting tag.
 * The last word of the block is identical to the zero'th word of the
 * block and is the end tag.  IF the block is free, the second last word
 * is a pointer to the next block in the free list (a doubly linked list
 * of all free blocks in the heap), and the third from last word is a
 * pointer to the previous block in the free list.  HEADERWORDS is the
 * number of words before the pointer in the block that malloc returns,
 * TRAILERWORDS is the number of words after the returned block. Note
 * that the zero'th and the last word MUST be the boundary tags - this
 * is hard-wired into the algorithm. Increasing HEADERWORDS or
 * TRAILERWORDS suitably should be accompanied by additional macros to
 * operate on those words. The routines most likely to be affected are
 * malloc/realloc/free/memalign/mal_verify/mal_heapdump.
 */
/*
 * There are two ways we can refer to a block -- by pointing at the
 * start tag, or by pointing at the end tag. For reasons of efficiency
 * and performance, free blocks are always referred to by the end tag,
 * while allocated blocks are usually referred to by the start tag.
 * Accordingly, the following macros indicate the type of their argument
 * by using either 'p', 'sp', or 'ep' to indicate a pointer. 'p' means
 * the pointer could point at either the start or end tag. 'sp' means it
 * must point at a start tag for that macro to work correctly, 'ep'
 * means it must point at the end tag. Usually, macros operating on free
 * blocks (NEXT, PREV, VALID_PREV_PTR, VALID_NEXT_PTR) take 'ep', while
 * macros operating on allocated blocks (REALSIZE, MAGIC_PTR,
 * SET_REALSIZE) take 'sp'. The size field may be validated using either
 * VALID_START_SIZE_FIELD for an 'sp' or VALID_END_SIZE_FIELD for an
 * 'ep'.
 */
/*
 *  SIZE, SIZEFIELD and TAG are valid for allocated and free blocks,
 *  REALSIZE is valid for allocated blocks when debugging, and NEXT and
 *  PREV are valid for free blocks. We could speed things up by making
 *  the free list singly linked when not debugging - the double link are
 *  just so we can check for pointer validity. (PREV(NEXT(ep)) == ep and
 *  NEXT(PREV(ep)) == ep). FREESIZE is used only to get the size field
 *  from FREE blocks - in this implementation, free blocks have a tag
 *  bit of 0 so no masking is necessary to operate on the SIZEFIELD when
 *  a block is free. We take advantage of that as a minor optimization.
 */
#define SIZEFIELD(p)		((p)->size)
#define SIZE(p)			((size_t) (SIZEFIELD(p) & SIZEMASK))
#define ALLOCMASK(n)		((n) | ALLOCED)
#define FREESIZE(p)		SIZEFIELD(p)
/*
 *  FREEMASK should be (n) & SIZEMASK, but since (n) will always have
 *  the hi bit off after the conversion from bytes requested by the user
 *  to words.
 */
#define FREEMASK(n)		(n)
#define TAG(p)			(SIZEFIELD(p) & ~SIZEMASK)

#ifdef DEBUG
# define REALSIZE(sp)		(((sp)+1)->size)
#endif /* DEBUG */

#define NEXT(ep)		(((ep)-1)->next)
#define PREV(ep)		(((ep)-2)->prev)

/*
 *  HEADERWORDS is the real block header in an allocated block - the
 *  free block header uses extra words in the block itself
 */
#ifdef DEBUG
# define HEADERWORDS		2	/* Start boundary tag + real size in bytes */
#else /* ! DEBUG */
# define HEADERWORDS		1	/* Start boundary tag */
#endif /* DEBUG */

#define TRAILERWORDS		1

#define FREEHEADERWORDS		1	/* Start boundary tag */

#define FREETRAILERWORDS	3	/* next and prev, end boundary tag */

#define ALLOC_OVERHEAD	(HEADERWORDS + TRAILERWORDS)
#define FREE_OVERHEAD	(FREEHEADERWORDS + FREETRAILERWORDS)

/*
 *  The allocator views memory as a list of non-contiguous arenas. (If
 *  successive sbrks() return contiguous blocks, they are colaesced into
 *  one arena - if a program never calls sbrk() other than malloc(),
 *  then there should only be one arena. This malloc will however
 *  happily coexist with other allocators calling sbrk() and uses only
 *  the blocks given to it by sbrk. It expects the same courtesy from
 *  other allocators. The arenas are chained into a linked list using
 *  the first word of each block as a pointer to the next arena. The
 *  second word of the arena, and the last word, contain fake boundary
 *  tags that are permanantly marked allocated, so that no attempt is
 *  made to coalesce past them. See the code in dumpheap for more info.
 */
#define ARENASTART		2	/* next ptr + fake start tag */

#ifdef DEBUG
  /* 
   * 1 for prev link in free block - next link is absorbed by header
   * REALSIZE word
   */
# define FIXEDOVERHEAD		(1 + ALLOC_OVERHEAD)
#else /* ! DEBUG */
  /* 1 for prev link, 1 for next link, + header and trailer */
# define FIXEDOVERHEAD		(2 + ALLOC_OVERHEAD)
#endif /* DEBUG */

/* 
 * Check that pointer is safe to dereference i.e. actually points
 * somewhere within the heap and is properly aligned.
 */
#define PTR_IN_HEAP(p) \
	((p) > _malloc_loword && (p) < _malloc_hiword && \
	 ALIGNPTR(p) == ((univptr_t) (p)))

/* Check that the size field is valid */
#define VALID_START_SIZE_FIELD(sp) \
	(PTR_IN_HEAP((sp) + SIZE(sp) - 1) && \
	 SIZEFIELD(sp) == SIZEFIELD((sp) + SIZE(sp) - 1))

#define VALID_END_SIZE_FIELD(ep) \
	(PTR_IN_HEAP((ep) - SIZE(ep) + 1) && \
	 SIZEFIELD(ep) == SIZEFIELD((ep) - SIZE(ep) + 1))


#define ulong unsigned long

#ifdef DEBUG
  /* 
   * Byte that is stored at the end of each block if the requested size
   * of the block is not a multiple of sizeof(Word). (If it is a multiple
   * of sizeof(Word), then we don't need to put the magic because the
   * endboundary tag will be corrupted and the tests that check the
   * validity of the boundary tag should detect it
   */
# ifndef u_char
#  define u_char		unsigned char
# endif /* u_char */

# define MAGIC_BYTE		((u_char) '\252')
  /* 
   * Check if size of the block is identical to requested size. Typical
   * tests will be of the form DONT_NEED_MAGIC(p) || something for
   * short-circuited protection, because blocks where DONT_NEED_MAGIC is
   * true will be tested for boundary tag detection so we don't need the
   * magic byte at the end.
   */
# define DONT_NEED_MAGIC(sp) \
	(REALSIZE(sp) == ((SIZE(sp) - ALLOC_OVERHEAD) * sizeof(Word)))

  /* Note that VALID_REALSIZE must not be used if DONT_NEED_MAGIC is true */
# define VALID_REALSIZE(sp) \
	(REALSIZE(sp) < ((SIZE(sp) - ALLOC_OVERHEAD)*sizeof(Word)))
	
  /* Location of the magic byte */
# define MAGIC_PTR(sp)		((u_char *)((sp) + HEADERWORDS) + REALSIZE(sp))

  /*
   * malloc code should only use the next two macros SET_REALSIZE and
   * VALID_MAGIC, since they are the only ones which have non-DEBUG
   * (null) alternatives
   */
   
  /* Macro sets the realsize of a block if necessary */
# define SET_REALSIZE(sp, n) \
	(REALSIZE(sp) = (n), \
	 DONT_NEED_MAGIC(sp) || (*MAGIC_PTR(sp) = MAGIC_BYTE))

  /* Macro tests that the magic byte is valid if it exists */
# define VALID_MAGIC(sp) \
	(DONT_NEED_MAGIC(sp) || \
	 (VALID_REALSIZE(sp) && (*MAGIC_PTR(sp) == MAGIC_BYTE)))

#else /* ! DEBUG */
# define SET_REALSIZE(sp, n)
# define VALID_MAGIC(sp)	(1)
#endif /* DEBUG */

/* 
 *  Check that a free list ptr points to a block with something pointing
 *  back. This is the only reason we use a doubly linked free list.
 */
#define VALID_NEXT_PTR(ep) (PTR_IN_HEAP(NEXT(ep)) && PREV(NEXT(ep)) == (ep))
#define VALID_PREV_PTR(ep) (PTR_IN_HEAP(PREV(ep)) && NEXT(PREV(ep)) == (ep))

/*
 *  quick bit-arithmetic to check a number (including 1) is a power of two.
 */
#define is_power_of_2(x) ((((x) - 1) & (x)) == 0)

/*
 *  An array to try and keep track of the distribution of sizes of
 *  malloc'ed blocks
 */
#ifdef PROFILESIZES
# define MAXPROFILESIZE 2*1024
# define COUNTSIZE(nw) (_malloc_scount[((nw) < MAXPROFILESIZE) ? (nw) : 0]++)
#else
# define COUNTSIZE(nw)
#endif

#define DEF_SBRKUNITS	1024

#ifndef USESTDIO
  /* 
   * Much better not to use stdio - stdio calls malloc() and can
   * therefore cause problems when debugging heap corruption. However,
   * there's no guaranteed way to turn off buffering on a FILE pointer in
   * ANSI.  This is a vile kludge!
   */
# define fputs(s, f)		write(fileno(f), (s), strlen(s))
# define setvbuf(f, s, n, l)	__nothing()
# define fflush(f)		__nothing()
#endif /* USESTDIO */

#ifdef TRACE
  /* 
   * Prints a trace string. For convenience, x is an
   * sprintf(_malloc_statsbuf, ...) or strcpy(_malloc_statsbuf, ...);
   * something which puts the appropriate text to be printed in
   * _malloc_statsbuf - ugly, but more convenient than making x a string.
   */
# define PRTRACE(x) \
  if (_malloc_tracing) { \
	(void) x; \
	(void) fputs(_malloc_statsbuf, _malloc_statsfile); \
  } else \
	_malloc_tracing += 0
#else
# define PRTRACE(x)
#endif

#ifdef DEBUG
# define CHECKHEAP() \
  if (_malloc_debugging >= 2) \
	(void) mal_verify(_malloc_debugging >= 3); \
  else \
	_malloc_debugging += 0
#else
# define CHECKHEAP()
#endif

#define FREEMAGIC		'\125'

/*
 *  Memory functions but in words. We just call memset/memcpy, and hope
 *  that someone has optimized them. If you are on pure 4.2BSD, either
 *  redefine these in terms of bcopy/your own memcpy, or
 *  get the functions from one of 4.3src/Henry Spencer's strings
 *  package/C News src
 */
#define MEMSET(p, c, n) \
	(void) memset((univptr_t) (p), (c), (memsize_t) ((n) * sizeof(Word)))
#define MEMCPY(p1, p2, n) \
	(void) memcpy((univptr_t) (p1), (univptr_t) (p2), \
				  (memsize_t) ((n) * sizeof(Word)))


#ifdef DEBUG
# define DMEMSET(p, n)	MEMSET((p), FREEMAGIC, (n))
#else
# define DMEMSET(p, n)
#endif

/* 
 *  Thanks to Hugh Redelmeier for pointing out that an rcsid is "a
 *  variable accessed in a way not obvious to the compiler", so should
 *  be called volatile. Amazing - a use for const volatile...
 */
#ifndef	RCSID	/* define RCSID(x) to nothing if don't want the rcs headers */
# ifndef lint
#  define RCSID(x)		static const volatile char *rcsid = x ;
# else
#  define RCSID(x)
# endif
#endif

#endif /* __DEFS_H__ */ /* Do not add anything after this line */
