/*    malloc.c
 *
 */

/*
  Here are some notes on configuring Perl's malloc.
 
  There are two macros which serve as bulk disablers of advanced
  features of this malloc: NO_FANCY_MALLOC, PLAIN_MALLOC (undef by
  default).  Look in the list of default values below to understand
  their exact effect.  Defining NO_FANCY_MALLOC returns malloc.c to the
  state of the malloc in Perl 5.004.  Additionally defining PLAIN_MALLOC
  returns it to the state as of Perl 5.000.

  Note that some of the settings below may be ignored in the code based
  on values of other macros.  The PERL_CORE symbol is only defined when
  perl itself is being compiled (so malloc can make some assumptions
  about perl's facilities being available to it).

  Each config option has a short description, followed by its name,
  default value, and a comment about the default (if applicable).  Some
  options take a precise value, while the others are just boolean.
  The boolean ones are listed first.

    # Enable code for an emergency memory pool in $^M.  See perlvar.pod
    # for a description of $^M.
    PERL_EMERGENCY_SBRK		(!PLAIN_MALLOC && PERL_CORE)

    # Enable code for printing memory statistics.
    DEBUGGING_MSTATS		(!PLAIN_MALLOC && PERL_CORE)

    # Move allocation info for small buckets into separate areas.
    # Memory optimization (especially for small allocations, of the
    # less than 64 bytes).  Since perl usually makes a large number
    # of small allocations, this is usually a win.
    PACK_MALLOC			(!PLAIN_MALLOC && !RCHECK)

    # Add one page to big powers of two when calculating bucket size.
    # This is targeted at big allocations, as are common in image
    # processing.
    TWO_POT_OPTIMIZE		!PLAIN_MALLOC
 
    # Use intermediate bucket sizes between powers-of-two.  This is
    # generally a memory optimization, and a (small) speed pessimization.
    BUCKETS_ROOT2		!NO_FANCY_MALLOC

    # Do not check small deallocations for bad free().  Memory
    # and speed optimization, error reporting pessimization.
    IGNORE_SMALL_BAD_FREE	(!NO_FANCY_MALLOC && !RCHECK)

    # Use table lookup to decide in which bucket a given allocation will go.
    SMALL_BUCKET_VIA_TABLE	!NO_FANCY_MALLOC

    # Use a perl-defined sbrk() instead of the (presumably broken or
    # missing) system-supplied sbrk().
    USE_PERL_SBRK		undef

    # Use system malloc() (or calloc() etc.) to emulate sbrk(). Normally
    # only used with broken sbrk()s.
    PERL_SBRK_VIA_MALLOC	undef

    # Which allocator to use if PERL_SBRK_VIA_MALLOC
    SYSTEM_ALLOC(a) 		malloc(a)

    # Disable memory overwrite checking with DEBUGGING.  Memory and speed
    # optimization, error reporting pessimization.
    NO_RCHECK			undef

    # Enable memory overwrite checking with DEBUGGING.  Memory and speed
    # pessimization, error reporting optimization
    RCHECK			(DEBUGGING && !NO_RCHECK)

    # Failed allocations bigger than this size croak (if
    # PERL_EMERGENCY_SBRK is enabled) without touching $^M.  See
    # perlvar.pod for a description of $^M.
    BIG_SIZE			 (1<<16)	# 64K

    # Starting from this power of two, add an extra page to the
    # size of the bucket. This enables optimized allocations of sizes
    # close to powers of 2.  Note that the value is indexed at 0.
    FIRST_BIG_POW2 		15		# 32K, 16K is used too often

    # Estimate of minimal memory footprint.  malloc uses this value to
    # request the most reasonable largest blocks of memory from the system.
    FIRST_SBRK 			(48*1024)

    # Round up sbrk()s to multiples of this.
    MIN_SBRK 			2048

    # Round up sbrk()s to multiples of this percent of footprint.
    MIN_SBRK_FRAC 		3

    # Add this much memory to big powers of two to get the bucket size.
    PERL_PAGESIZE 		4096

    # This many sbrk() discontinuities should be tolerated even
    # from the start without deciding that sbrk() is usually
    # discontinuous.
    SBRK_ALLOW_FAILURES		3

    # This many continuous sbrk()s compensate for one discontinuous one.
    SBRK_FAILURE_PRICE		50

    # Some configurations may ask for 12-byte-or-so allocations which
    # require 8-byte alignment (?!).  In such situation one needs to
    # define this to disable 12-byte bucket (will increase memory footprint)
    STRICT_ALIGNMENT		undef

  This implementation assumes that calling PerlIO_printf() does not
  result in any memory allocation calls (used during a panic).

 */

#ifndef NO_FANCY_MALLOC
#  ifndef SMALL_BUCKET_VIA_TABLE
#    define SMALL_BUCKET_VIA_TABLE
#  endif 
#  ifndef BUCKETS_ROOT2
#    define BUCKETS_ROOT2
#  endif 
#  ifndef IGNORE_SMALL_BAD_FREE
#    define IGNORE_SMALL_BAD_FREE
#  endif 
#endif 

#ifndef PLAIN_MALLOC			/* Bulk enable features */
#  ifndef PACK_MALLOC
#      define PACK_MALLOC
#  endif 
#  ifndef TWO_POT_OPTIMIZE
#    define TWO_POT_OPTIMIZE
#  endif 
#  if defined(PERL_CORE) && !defined(PERL_EMERGENCY_SBRK)
#    define PERL_EMERGENCY_SBRK
#  endif 
#  if defined(PERL_CORE) && !defined(DEBUGGING_MSTATS)
#    define DEBUGGING_MSTATS
#  endif 
#endif

#define MIN_BUC_POW2 (sizeof(void*) > 4 ? 3 : 2) /* Allow for 4-byte arena. */
#define MIN_BUCKET (MIN_BUC_POW2 * BUCKETS_PER_POW2)

#if !(defined(I286) || defined(atarist) || defined(__MINT__))
	/* take 2k unless the block is bigger than that */
#  define LOG_OF_MIN_ARENA 11
#else
	/* take 16k unless the block is bigger than that 
	   (80286s like large segments!), probably good on the atari too */
#  define LOG_OF_MIN_ARENA 14
#endif

#ifndef lint
#  if defined(DEBUGGING) && !defined(NO_RCHECK)
#    define RCHECK
#  endif
#  if defined(RCHECK) && defined(IGNORE_SMALL_BAD_FREE)
#    undef IGNORE_SMALL_BAD_FREE
#  endif 
/*
 * malloc.c (Caltech) 2/21/82
 * Chris Kingsley, kingsley@cit-20.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small 
 * number of different sizes, and keeps free lists of each size.  Blocks that
 * don't exactly fit are passed up to the next larger size.  In this 
 * implementation, the available sizes are 2^n-4 (or 2^n-12) bytes long.
 * If PACK_MALLOC is defined, small blocks are 2^n bytes long.
 * This is designed for use in a program that uses vast quantities of memory,
 * but bombs when it runs out. 
 */

#ifdef PERL_CORE
#  include "EXTERN.h"
#  include "perl.h"
#else
#  ifdef PERL_FOR_X2P
#    include "../EXTERN.h"
#    include "../perl.h"
#  else
#    include <stdlib.h>
#    include <stdio.h>
#    include <memory.h>
#    define _(arg) arg
#    ifndef Malloc_t
#      define Malloc_t void *
#    endif
#    ifndef MEM_SIZE
#      define MEM_SIZE unsigned long
#    endif
#    ifndef LONG_MAX
#      define LONG_MAX 0x7FFFFFFF
#    endif
#    ifndef UV
#      define UV unsigned long
#    endif
#    ifndef caddr_t
#      define caddr_t char *
#    endif
#    ifndef Free_t
#      define Free_t void
#    endif
#    define Copy(s,d,n,t) (void)memcpy((char*)(d),(char*)(s), (n) * sizeof(t))
#    define PerlEnv_getenv getenv
#    define PerlIO_printf fprintf
#    define PerlIO_stderr() stderr
#  endif
#  ifndef croak				/* make depend */
#    define croak(mess, arg) warn((mess), (arg)); exit(1);
#  endif 
#  ifndef warn
#    define warn(mess, arg) fprintf(stderr, (mess), (arg));
#  endif 
#  ifdef DEBUG_m
#    undef DEBUG_m
#  endif 
#  define DEBUG_m(a)
#  ifdef DEBUGGING
#     undef DEBUGGING
#  endif
#endif

#ifndef MUTEX_LOCK
#  define MUTEX_LOCK(l)
#endif 

#ifndef MUTEX_UNLOCK
#  define MUTEX_UNLOCK(l)
#endif 

#ifdef DEBUGGING
#  undef DEBUG_m
#  define DEBUG_m(a)  if (PL_debug & 128)   a
#endif

/* I don't much care whether these are defined in sys/types.h--LAW */

#define u_char unsigned char
#define u_int unsigned int

#ifdef HAS_QUAD
#  define u_bigint UV			/* Needs to eat *void. */
#else  /* needed? */
#  define u_bigint unsigned long	/* Needs to eat *void. */
#endif

#define u_short unsigned short

/* 286 and atarist like big chunks, which gives too much overhead. */
#if (defined(RCHECK) || defined(I286) || defined(atarist) || defined(__MINT__)) && defined(PACK_MALLOC)
#  undef PACK_MALLOC
#endif 

/*
 * The description below is applicable if PACK_MALLOC is not defined.
 *
 * The overhead on a block is at least 4 bytes.  When free, this space
 * contains a pointer to the next free block, and the bottom two bits must
 * be zero.  When in use, the first byte is set to MAGIC, and the second
 * byte is the size index.  The remaining bytes are for alignment.
 * If range checking is enabled and the size of the block fits
 * in two bytes, then the top two bytes hold the size of the requested block
 * plus the range checking words, and the header word MINUS ONE.
 */
union	overhead {
	union	overhead *ov_next;	/* when free */
#if MEM_ALIGNBYTES > 4
	double	strut;			/* alignment problems */
#endif
	struct {
		u_char	ovu_magic;	/* magic number */
		u_char	ovu_index;	/* bucket # */
#ifdef RCHECK
		u_short	ovu_size;	/* actual block size */
		u_int	ovu_rmagic;	/* range magic number */
#endif
	} ovu;
#define	ov_magic	ovu.ovu_magic
#define	ov_index	ovu.ovu_index
#define	ov_size		ovu.ovu_size
#define	ov_rmagic	ovu.ovu_rmagic
};

#ifdef DEBUGGING
static void botch _((char *diag, char *s));
#endif
static void morecore _((int bucket));
static int findbucket _((union overhead *freep, int srchlen));
static void add_to_chain(void *p, MEM_SIZE size, MEM_SIZE chip);

#define	MAGIC		0xff		/* magic # on accounting info */
#define RMAGIC		0x55555555	/* magic # on range info */
#define RMAGIC_C	0x55		/* magic # on range info */

#ifdef RCHECK
#  define	RSLOP		sizeof (u_int)
#  ifdef TWO_POT_OPTIMIZE
#    define MAX_SHORT_BUCKET (12 * BUCKETS_PER_POW2)
#  else
#    define MAX_SHORT_BUCKET (13 * BUCKETS_PER_POW2)
#  endif 
#else
#  define	RSLOP		0
#endif

#if !defined(PACK_MALLOC) && defined(BUCKETS_ROOT2)
#  undef BUCKETS_ROOT2
#endif 

#ifdef BUCKETS_ROOT2
#  define BUCKET_TABLE_SHIFT 2
#  define BUCKET_POW2_SHIFT 1
#  define BUCKETS_PER_POW2 2
#else
#  define BUCKET_TABLE_SHIFT MIN_BUC_POW2
#  define BUCKET_POW2_SHIFT 0
#  define BUCKETS_PER_POW2 1
#endif 

#if !defined(MEM_ALIGNBYTES) || ((MEM_ALIGNBYTES > 4) && !defined(STRICT_ALIGNMENT))
/* Figure out the alignment of void*. */
struct aligner {
  char c;
  void *p;
};
#  define ALIGN_SMALL ((int)((caddr_t)&(((struct aligner*)0)->p)))
#else
#  define ALIGN_SMALL MEM_ALIGNBYTES
#endif

#define IF_ALIGN_8(yes,no)	((ALIGN_SMALL>4) ? (yes) : (no))

#ifdef BUCKETS_ROOT2
#  define MAX_BUCKET_BY_TABLE 13
static u_short buck_size[MAX_BUCKET_BY_TABLE + 1] = 
  { 
      0, 0, 0, 0, 4, 4, 8, 12, 16, 24, 32, 48, 64, 80,
  };
#  define BUCKET_SIZE(i) ((i) % 2 ? buck_size[i] : (1 << ((i) >> BUCKET_POW2_SHIFT)))
#  define BUCKET_SIZE_REAL(i) ((i) <= MAX_BUCKET_BY_TABLE		\
			       ? buck_size[i] 				\
			       : ((1 << ((i) >> BUCKET_POW2_SHIFT))	\
				  - MEM_OVERHEAD(i)			\
				  + POW2_OPTIMIZE_SURPLUS(i)))
#else
#  define BUCKET_SIZE(i) (1 << ((i) >> BUCKET_POW2_SHIFT))
#  define BUCKET_SIZE_REAL(i) (BUCKET_SIZE(i) - MEM_OVERHEAD(i) + POW2_OPTIMIZE_SURPLUS(i))
#endif 


#ifdef PACK_MALLOC
/* In this case it is assumed that if we do sbrk() in 2K units, we
 * will get 2K aligned arenas (at least after some initial
 * alignment). The bucket number of the given subblock is on the start
 * of 2K arena which contains the subblock.  Several following bytes
 * contain the magic numbers for the subblocks in the block.
 *
 * Sizes of chunks are powers of 2 for chunks in buckets <=
 * MAX_PACKED, after this they are (2^n - sizeof(union overhead)) (to
 * get alignment right).
 *
 * Consider an arena for 2^n with n>MAX_PACKED.  We suppose that
 * starts of all the chunks in a 2K arena are in different
 * 2^n-byte-long chunks.  If the top of the last chunk is aligned on a
 * boundary of 2K block, this means that sizeof(union
 * overhead)*"number of chunks" < 2^n, or sizeof(union overhead)*2K <
 * 4^n, or n > 6 + log2(sizeof()/2)/2, since a chunk of size 2^n -
 * overhead is used.  Since this rules out n = 7 for 8 byte alignment,
 * we specialcase allocation of the first of 16 128-byte-long chunks.
 *
 * Note that with the above assumption we automatically have enough
 * place for MAGIC at the start of 2K block.  Note also that we
 * overlay union overhead over the chunk, thus the start of small chunks
 * is immediately overwritten after freeing.  */
#  define MAX_PACKED_POW2 6
#  define MAX_PACKED (MAX_PACKED_POW2 * BUCKETS_PER_POW2 + BUCKET_POW2_SHIFT)
#  define MAX_POW2_ALGO ((1<<(MAX_PACKED_POW2 + 1)) - M_OVERHEAD)
#  define TWOK_MASK ((1<<LOG_OF_MIN_ARENA) - 1)
#  define TWOK_MASKED(x) ((u_bigint)(x) & ~TWOK_MASK)
#  define TWOK_SHIFT(x) ((u_bigint)(x) & TWOK_MASK)
#  define OV_INDEXp(block) ((u_char*)(TWOK_MASKED(block)))
#  define OV_INDEX(block) (*OV_INDEXp(block))
#  define OV_MAGIC(block,bucket) (*(OV_INDEXp(block) +			\
				    (TWOK_SHIFT(block)>>		\
				     (bucket>>BUCKET_POW2_SHIFT)) +	\
				    (bucket >= MIN_NEEDS_SHIFT ? 1 : 0)))
    /* A bucket can have a shift smaller than it size, we need to
       shift its magic number so it will not overwrite index: */
#  ifdef BUCKETS_ROOT2
#    define MIN_NEEDS_SHIFT (7*BUCKETS_PER_POW2 - 1) /* Shift 80 greater than chunk 64. */
#  else
#    define MIN_NEEDS_SHIFT (7*BUCKETS_PER_POW2) /* Shift 128 greater than chunk 32. */
#  endif 
#  define CHUNK_SHIFT 0

/* Number of active buckets of given ordinal. */
#ifdef IGNORE_SMALL_BAD_FREE
#define FIRST_BUCKET_WITH_CHECK (6 * BUCKETS_PER_POW2) /* 64 */
#  define N_BLKS(bucket) ( (bucket) < FIRST_BUCKET_WITH_CHECK 		\
			 ? ((1<<LOG_OF_MIN_ARENA) - 1)/BUCKET_SIZE(bucket) \
			 : n_blks[bucket] )
#else
#  define N_BLKS(bucket) n_blks[bucket]
#endif 

static u_short n_blks[LOG_OF_MIN_ARENA * BUCKETS_PER_POW2] = 
  {
#  if BUCKETS_PER_POW2==1
      0, 0,
      (MIN_BUC_POW2==2 ? 384 : 0),
      224, 120, 62, 31, 16, 8, 4, 2
#  else
      0, 0, 0, 0,
      (MIN_BUC_POW2==2 ? 384 : 0), (MIN_BUC_POW2==2 ? 384 : 0),	/* 4, 4 */
      224, 149, 120, 80, 62, 41, 31, 25, 16, 16, 8, 8, 4, 4, 2, 2
#  endif
  };

/* Shift of the first bucket with the given ordinal inside 2K chunk. */
#ifdef IGNORE_SMALL_BAD_FREE
#  define BLK_SHIFT(bucket) ( (bucket) < FIRST_BUCKET_WITH_CHECK 	\
			      ? ((1<<LOG_OF_MIN_ARENA)			\
				 - BUCKET_SIZE(bucket) * N_BLKS(bucket)) \
			      : blk_shift[bucket])
#else
#  define BLK_SHIFT(bucket) blk_shift[bucket]
#endif 

static u_short blk_shift[LOG_OF_MIN_ARENA * BUCKETS_PER_POW2] = 
  { 
#  if BUCKETS_PER_POW2==1
      0, 0,
      (MIN_BUC_POW2==2 ? 512 : 0),
      256, 128, 64, 64,			/* 8 to 64 */
      16*sizeof(union overhead), 
      8*sizeof(union overhead), 
      4*sizeof(union overhead), 
      2*sizeof(union overhead), 
#  else
      0, 0, 0, 0,
      (MIN_BUC_POW2==2 ? 512 : 0), (MIN_BUC_POW2==2 ? 512 : 0),
      256, 260, 128, 128, 64, 80, 64, 48, /* 8 to 96 */
      16*sizeof(union overhead), 16*sizeof(union overhead), 
      8*sizeof(union overhead), 8*sizeof(union overhead), 
      4*sizeof(union overhead), 4*sizeof(union overhead), 
      2*sizeof(union overhead), 2*sizeof(union overhead), 
#  endif 
  };

#else  /* !PACK_MALLOC */

#  define OV_MAGIC(block,bucket) (block)->ov_magic
#  define OV_INDEX(block) (block)->ov_index
#  define CHUNK_SHIFT 1
#  define MAX_PACKED -1
#endif /* !PACK_MALLOC */

#define M_OVERHEAD (sizeof(union overhead) + RSLOP)

#ifdef PACK_MALLOC
#  define MEM_OVERHEAD(bucket) \
  (bucket <= MAX_PACKED ? 0 : M_OVERHEAD)
#  ifdef SMALL_BUCKET_VIA_TABLE
#    define START_SHIFTS_BUCKET ((MAX_PACKED_POW2 + 1) * BUCKETS_PER_POW2)
#    define START_SHIFT MAX_PACKED_POW2
#    ifdef BUCKETS_ROOT2		/* Chunks of size 3*2^n. */
#      define SIZE_TABLE_MAX 80
#    else
#      define SIZE_TABLE_MAX 64
#    endif 
static char bucket_of[] =
  {
#    ifdef BUCKETS_ROOT2		/* Chunks of size 3*2^n. */
      /* 0 to 15 in 4-byte increments. */
      (sizeof(void*) > 4 ? 6 : 5),	/* 4/8, 5-th bucket for better reports */
      6,				/* 8 */
      IF_ALIGN_8(8,7), 8,		/* 16/12, 16 */
      9, 9, 10, 10,			/* 24, 32 */
      11, 11, 11, 11,			/* 48 */
      12, 12, 12, 12,			/* 64 */
      13, 13, 13, 13,			/* 80 */
      13, 13, 13, 13			/* 80 */
#    else /* !BUCKETS_ROOT2 */
      /* 0 to 15 in 4-byte increments. */
      (sizeof(void*) > 4 ? 3 : 2),
      3, 
      4, 4, 
      5, 5, 5, 5,
      6, 6, 6, 6,
      6, 6, 6, 6
#    endif /* !BUCKETS_ROOT2 */
  };
#  else  /* !SMALL_BUCKET_VIA_TABLE */
#    define START_SHIFTS_BUCKET MIN_BUCKET
#    define START_SHIFT (MIN_BUC_POW2 - 1)
#  endif /* !SMALL_BUCKET_VIA_TABLE */
#else  /* !PACK_MALLOC */
#  define MEM_OVERHEAD(bucket) M_OVERHEAD
#  ifdef SMALL_BUCKET_VIA_TABLE
#    undef SMALL_BUCKET_VIA_TABLE
#  endif 
#  define START_SHIFTS_BUCKET MIN_BUCKET
#  define START_SHIFT (MIN_BUC_POW2 - 1)
#endif /* !PACK_MALLOC */

/*
 * Big allocations are often of the size 2^n bytes. To make them a
 * little bit better, make blocks of size 2^n+pagesize for big n.
 */

#ifdef TWO_POT_OPTIMIZE

#  ifndef PERL_PAGESIZE
#    define PERL_PAGESIZE 4096
#  endif 
#  ifndef FIRST_BIG_POW2
#    define FIRST_BIG_POW2 15	/* 32K, 16K is used too often. */
#  endif
#  define FIRST_BIG_BLOCK (1<<FIRST_BIG_POW2)
/* If this value or more, check against bigger blocks. */
#  define FIRST_BIG_BOUND (FIRST_BIG_BLOCK - M_OVERHEAD)
/* If less than this value, goes into 2^n-overhead-block. */
#  define LAST_SMALL_BOUND ((FIRST_BIG_BLOCK>>1) - M_OVERHEAD)

#  define POW2_OPTIMIZE_ADJUST(nbytes)				\
   ((nbytes >= FIRST_BIG_BOUND) ? nbytes -= PERL_PAGESIZE : 0)
#  define POW2_OPTIMIZE_SURPLUS(bucket)				\
   ((bucket >= FIRST_BIG_POW2 * BUCKETS_PER_POW2) ? PERL_PAGESIZE : 0)

#else  /* !TWO_POT_OPTIMIZE */
#  define POW2_OPTIMIZE_ADJUST(nbytes)
#  define POW2_OPTIMIZE_SURPLUS(bucket) 0
#endif /* !TWO_POT_OPTIMIZE */

#if defined(HAS_64K_LIMIT) && defined(PERL_CORE)
#  define BARK_64K_LIMIT(what,nbytes,size)				\
	if (nbytes > 0xffff) {						\
		PerlIO_printf(PerlIO_stderr(),				\
			      "%s too large: %lx\n", what, size);	\
		my_exit(1);						\
	}
#else /* !HAS_64K_LIMIT || !PERL_CORE */
#  define BARK_64K_LIMIT(what,nbytes,size)
#endif /* !HAS_64K_LIMIT || !PERL_CORE */

#ifndef MIN_SBRK
#  define MIN_SBRK 2048
#endif 

#ifndef FIRST_SBRK
#  define FIRST_SBRK (48*1024)
#endif 

/* Minimal sbrk in percents of what is already alloced. */
#ifndef MIN_SBRK_FRAC
#  define MIN_SBRK_FRAC 3
#endif 

#ifndef SBRK_ALLOW_FAILURES
#  define SBRK_ALLOW_FAILURES 3
#endif 

#ifndef SBRK_FAILURE_PRICE
#  define SBRK_FAILURE_PRICE 50
#endif 

#if defined(PERL_EMERGENCY_SBRK) && defined(PERL_CORE)

#  ifndef BIG_SIZE
#    define BIG_SIZE (1<<16)		/* 64K */
#  endif 

#ifdef MUTEX_INIT_CALLS_MALLOC
#  undef      MUTEX_LOCK
#  define MUTEX_LOCK(m)       STMT_START { if (*m) mutex_lock(*m); } STMT_END
#  undef      MUTEX_UNLOCK
#  define MUTEX_UNLOCK(m)     STMT_START { if (*m) mutex_unlock(*m); } STMT_END
#endif

static char *emergency_buffer;
static MEM_SIZE emergency_buffer_size;
static Malloc_t emergency_sbrk(MEM_SIZE size);

static Malloc_t
emergency_sbrk(MEM_SIZE size)
{
    MEM_SIZE rsize = (((size - 1)>>LOG_OF_MIN_ARENA) + 1)<<LOG_OF_MIN_ARENA;

    if (size >= BIG_SIZE) {
	/* Give the possibility to recover: */
	MUTEX_UNLOCK(&PL_malloc_mutex);
	croak("Out of memory during \"large\" request for %i bytes", size);
    }

    if (emergency_buffer_size >= rsize) {
	char *old = emergency_buffer;
	
	emergency_buffer_size -= rsize;
	emergency_buffer += rsize;
	return old;
    } else {		
	dTHR;
	/* First offense, give a possibility to recover by dieing. */
	/* No malloc involved here: */
	GV **gvp = (GV**)hv_fetch(PL_defstash, "^M", 2, 0);
	SV *sv;
	char *pv;
	int have = 0;
	STRLEN n_a;

	if (emergency_buffer_size) {
	    add_to_chain(emergency_buffer, emergency_buffer_size, 0);
	    emergency_buffer_size = 0;
	    emergency_buffer = Nullch;
	    have = 1;
	}
	if (!gvp) gvp = (GV**)hv_fetch(PL_defstash, "\015", 1, 0);
	if (!gvp || !(sv = GvSV(*gvp)) || !SvPOK(sv) 
	    || (SvLEN(sv) < (1<<LOG_OF_MIN_ARENA) - M_OVERHEAD)) {
	    if (have)
		goto do_croak;
	    return (char *)-1;		/* Now die die die... */
	}
	/* Got it, now detach SvPV: */
	pv = SvPV(sv, n_a);
	/* Check alignment: */
	if (((UV)(pv - sizeof(union overhead))) & ((1<<LOG_OF_MIN_ARENA) - 1)) {
	    PerlIO_puts(PerlIO_stderr(),"Bad alignment of $^M!\n");
	    return (char *)-1;		/* die die die */
	}

	emergency_buffer = pv - sizeof(union overhead);
	emergency_buffer_size = malloced_size(pv) + M_OVERHEAD;
	SvPOK_off(sv);
	SvPVX(sv) = Nullch;
	SvCUR(sv) = SvLEN(sv) = 0;
    }
  do_croak:
    MUTEX_UNLOCK(&PL_malloc_mutex);
    croak("Out of memory during request for %i bytes", size);
}

#else /* !(defined(PERL_EMERGENCY_SBRK) && defined(PERL_CORE)) */
#  define emergency_sbrk(size)	-1
#endif /* !(defined(PERL_EMERGENCY_SBRK) && defined(PERL_CORE)) */

/*
 * nextf[i] is the pointer to the next free block of size 2^i.  The
 * smallest allocatable block is 8 bytes.  The overhead information
 * precedes the data area returned to the user.
 */
#define	NBUCKETS (32*BUCKETS_PER_POW2 + 1)
static	union overhead *nextf[NBUCKETS];

#ifdef USE_PERL_SBRK
#define sbrk(a) Perl_sbrk(a)
Malloc_t Perl_sbrk _((int size));
#else 
#ifdef DONT_DECLARE_STD
#ifdef I_UNISTD
#include <unistd.h>
#endif
#else
extern	Malloc_t sbrk(int);
#endif
#endif

#ifdef DEBUGGING_MSTATS
/*
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
 */
static	u_int nmalloc[NBUCKETS];
static  u_int sbrk_slack;
static  u_int start_slack;
#endif

static	u_int goodsbrk;

#ifdef DEBUGGING
#undef ASSERT
#define	ASSERT(p,diag)   if (!(p)) botch(diag,STRINGIFY(p));  else
static void
botch(char *diag, char *s)
{
	PerlIO_printf(PerlIO_stderr(), "assertion botched (%s?): %s\n", diag, s);
	PerlProc_abort();
}
#else
#define	ASSERT(p, diag)
#endif

Malloc_t
malloc(register size_t nbytes)
{
  	register union overhead *p;
  	register int bucket;
  	register MEM_SIZE shiftr;

#if defined(DEBUGGING) || defined(RCHECK)
	MEM_SIZE size = nbytes;
#endif

	BARK_64K_LIMIT("Allocation",nbytes,nbytes);
#ifdef DEBUGGING
	if ((long)nbytes < 0)
		croak("%s", "panic: malloc");
#endif

	MUTEX_LOCK(&PL_malloc_mutex);
	/*
	 * Convert amount of memory requested into
	 * closest block size stored in hash buckets
	 * which satisfies request.  Account for
	 * space used per block for accounting.
	 */
#ifdef PACK_MALLOC
#  ifdef SMALL_BUCKET_VIA_TABLE
	if (nbytes == 0)
	    bucket = MIN_BUCKET;
	else if (nbytes <= SIZE_TABLE_MAX) {
	    bucket = bucket_of[(nbytes - 1) >> BUCKET_TABLE_SHIFT];
	} else
#  else
	if (nbytes == 0)
	    nbytes = 1;
	if (nbytes <= MAX_POW2_ALGO) goto do_shifts;
	else
#  endif
#endif 
	{
	    POW2_OPTIMIZE_ADJUST(nbytes);
	    nbytes += M_OVERHEAD;
	    nbytes = (nbytes + 3) &~ 3; 
	  do_shifts:
	    shiftr = (nbytes - 1) >> START_SHIFT;
	    bucket = START_SHIFTS_BUCKET;
	    /* apart from this loop, this is O(1) */
	    while (shiftr >>= 1)
  		bucket += BUCKETS_PER_POW2;
	}
	/*
	 * If nothing in hash bucket right now,
	 * request more memory from the system.
	 */
  	if (nextf[bucket] == NULL)    
  		morecore(bucket);
  	if ((p = nextf[bucket]) == NULL) {
		MUTEX_UNLOCK(&PL_malloc_mutex);
#ifdef PERL_CORE
		if (!PL_nomemok) {
		    PerlIO_puts(PerlIO_stderr(),"Out of memory!\n");
		    my_exit(1);
		}
#else
  		return (NULL);
#endif
	}

	DEBUG_m(PerlIO_printf(Perl_debug_log,
			      "0x%lx: (%05lu) malloc %ld bytes\n",
			      (unsigned long)(p+1), (unsigned long)(PL_an++),
			      (long)size));

	/* remove from linked list */
#if defined(RCHECK)
	if (((UV)p) & (MEM_ALIGNBYTES - 1))
	    PerlIO_printf(PerlIO_stderr(), "Corrupt malloc ptr 0x%lx at 0x%lx\n",
		(unsigned long)*((int*)p),(unsigned long)p);
#endif
  	nextf[bucket] = p->ov_next;
#ifdef IGNORE_SMALL_BAD_FREE
	if (bucket >= FIRST_BUCKET_WITH_CHECK)
#endif 
	    OV_MAGIC(p, bucket) = MAGIC;
#ifndef PACK_MALLOC
	OV_INDEX(p) = bucket;
#endif
#ifdef RCHECK
	/*
	 * Record allocated size of block and
	 * bound space with magic numbers.
	 */
	p->ov_rmagic = RMAGIC;
	if (bucket <= MAX_SHORT_BUCKET) {
	    int i;
	    
	    nbytes = size + M_OVERHEAD; 
	    p->ov_size = nbytes - 1;
	    if ((i = nbytes & 3)) {
		i = 4 - i;
		while (i--)
		    *((char *)((caddr_t)p + nbytes - RSLOP + i)) = RMAGIC_C;
	    }
	    nbytes = (nbytes + 3) &~ 3; 
	    *((u_int *)((caddr_t)p + nbytes - RSLOP)) = RMAGIC;
	}
#endif
	MUTEX_UNLOCK(&PL_malloc_mutex);
  	return ((Malloc_t)(p + CHUNK_SHIFT));
}

static char *last_sbrk_top;
static char *last_op;			/* This arena can be easily extended. */
static int sbrked_remains;
static int sbrk_good = SBRK_ALLOW_FAILURES * SBRK_FAILURE_PRICE;

#ifdef DEBUGGING_MSTATS
static int sbrks;
#endif 

struct chunk_chain_s {
    struct chunk_chain_s *next;
    MEM_SIZE size;
};
static struct chunk_chain_s *chunk_chain;
static int n_chunks;
static char max_bucket;

/* Cutoff a piece of one of the chunks in the chain.  Prefer smaller chunk. */
static void *
get_from_chain(MEM_SIZE size)
{
    struct chunk_chain_s *elt = chunk_chain, **oldp = &chunk_chain;
    struct chunk_chain_s **oldgoodp = NULL;
    long min_remain = LONG_MAX;

    while (elt) {
	if (elt->size >= size) {
	    long remains = elt->size - size;
	    if (remains >= 0 && remains < min_remain) {
		oldgoodp = oldp;
		min_remain = remains;
	    }
	    if (remains == 0) {
		break;
	    }
	}
	oldp = &( elt->next );
	elt = elt->next;
    }
    if (!oldgoodp) return NULL;
    if (min_remain) {
	void *ret = *oldgoodp;
	struct chunk_chain_s *next = (*oldgoodp)->next;
	
	*oldgoodp = (struct chunk_chain_s *)((char*)ret + size);
	(*oldgoodp)->size = min_remain;
	(*oldgoodp)->next = next;
	return ret;
    } else {
	void *ret = *oldgoodp;
	*oldgoodp = (*oldgoodp)->next;
	n_chunks--;
	return ret;
    }
}

static void
add_to_chain(void *p, MEM_SIZE size, MEM_SIZE chip)
{
    struct chunk_chain_s *next = chunk_chain;
    char *cp = (char*)p;
    
    cp += chip;
    chunk_chain = (struct chunk_chain_s *)cp;
    chunk_chain->size = size - chip;
    chunk_chain->next = next;
    n_chunks++;
}

static void *
get_from_bigger_buckets(int bucket, MEM_SIZE size)
{
    int price = 1;
    static int bucketprice[NBUCKETS];
    while (bucket <= max_bucket) {
	/* We postpone stealing from bigger buckets until we want it
	   often enough. */
	if (nextf[bucket] && bucketprice[bucket]++ >= price) {
	    /* Steal it! */
	    void *ret = (void*)(nextf[bucket] - 1 + CHUNK_SHIFT);
	    bucketprice[bucket] = 0;
	    if (((char*)nextf[bucket]) - M_OVERHEAD == last_op) {
		last_op = NULL;		/* Disable optimization */
	    }
	    nextf[bucket] = nextf[bucket]->ov_next;
#ifdef DEBUGGING_MSTATS
	    nmalloc[bucket]--;
	    start_slack -= M_OVERHEAD;
#endif 
	    add_to_chain(ret, (BUCKET_SIZE(bucket) +
			       POW2_OPTIMIZE_SURPLUS(bucket)), 
			 size);
	    return ret;
	}
	bucket++;
    }
    return NULL;
}

static union overhead *
getpages(int needed, int *nblksp, int bucket)
{
    /* Need to do (possibly expensive) system call. Try to
       optimize it for rare calling. */
    MEM_SIZE require = needed - sbrked_remains;
    char *cp;
    union overhead *ovp;
    int slack = 0;

    if (sbrk_good > 0) {
	if (!last_sbrk_top && require < FIRST_SBRK) 
	    require = FIRST_SBRK;
	else if (require < MIN_SBRK) require = MIN_SBRK;

	if (require < goodsbrk * MIN_SBRK_FRAC / 100)
	    require = goodsbrk * MIN_SBRK_FRAC / 100;
	require = ((require - 1 + MIN_SBRK) / MIN_SBRK) * MIN_SBRK;
    } else {
	require = needed;
	last_sbrk_top = 0;
	sbrked_remains = 0;
    }

    DEBUG_m(PerlIO_printf(Perl_debug_log, 
			  "sbrk(%ld) for %ld-byte-long arena\n",
			  (long)require, (long) needed));
    cp = (char *)sbrk(require);
#ifdef DEBUGGING_MSTATS
    sbrks++;
#endif 
    if (cp == last_sbrk_top) {
	/* Common case, anything is fine. */
	sbrk_good++;
	ovp = (union overhead *) (cp - sbrked_remains);
	sbrked_remains = require - (needed - sbrked_remains);
    } else if (cp == (char *)-1) { /* no more room! */
	ovp = (union overhead *)emergency_sbrk(needed);
	if (ovp == (union overhead *)-1)
	    return 0;
	return ovp;
    } else {			/* Non-continuous or first sbrk(). */
	long add = sbrked_remains;
	char *newcp;

	if (sbrked_remains) {	/* Put rest into chain, we
				   cannot use it right now. */
	    add_to_chain((void*)(last_sbrk_top - sbrked_remains),
			 sbrked_remains, 0);
	}

	/* Second, check alignment. */
	slack = 0;

#if !defined(atarist) && !defined(__MINT__) /* on the atari we dont have to worry about this */
#  ifndef I286 	/* The sbrk(0) call on the I286 always returns the next segment */

	/* CHUNK_SHIFT is 1 for PACK_MALLOC, 0 otherwise. */
	if ((UV)cp & (0x7FF >> CHUNK_SHIFT)) { /* Not aligned. */
	    slack = (0x800 >> CHUNK_SHIFT)
		- ((UV)cp & (0x7FF >> CHUNK_SHIFT));
	    add += slack;
	}
#  endif
#endif /* !atarist && !MINT */
		
	if (add) {
	    DEBUG_m(PerlIO_printf(Perl_debug_log, 
				  "sbrk(%ld) to fix non-continuous/off-page sbrk:\n\t%ld for alignement,\t%ld were assumed to come from the tail of the previous sbrk\n",
				  (long)add, (long) slack,
				  (long) sbrked_remains));
	    newcp = (char *)sbrk(add);
#if defined(DEBUGGING_MSTATS)
	    sbrks++;
	    sbrk_slack += add;
#endif
	    if (newcp != cp + require) {
		/* Too bad: even rounding sbrk() is not continuous.*/
		DEBUG_m(PerlIO_printf(Perl_debug_log, 
				      "failed to fix bad sbrk()\n"));
#ifdef PACK_MALLOC
		if (slack) {
		    MUTEX_UNLOCK(&PL_malloc_mutex);
		    croak("%s", "panic: Off-page sbrk");
		}
#endif
		if (sbrked_remains) {
		    /* Try again. */
#if defined(DEBUGGING_MSTATS)
		    sbrk_slack += require;
#endif
		    require = needed;
		    DEBUG_m(PerlIO_printf(Perl_debug_log, 
					  "straight sbrk(%ld)\n",
					  (long)require));
		    cp = (char *)sbrk(require);
#ifdef DEBUGGING_MSTATS
		    sbrks++;
#endif 
		    if (cp == (char *)-1)
			return 0;
		}
		sbrk_good = -1;	/* Disable optimization!
				   Continue with not-aligned... */
	    } else {
		cp += slack;
		require += sbrked_remains;
	    }
	}

	if (last_sbrk_top) {
	    sbrk_good -= SBRK_FAILURE_PRICE;
	}

	ovp = (union overhead *) cp;
	/*
	 * Round up to minimum allocation size boundary
	 * and deduct from block count to reflect.
	 */

#ifndef I286	/* Again, this should always be ok on an 80286 */
	if ((UV)ovp & 7) {
	    ovp = (union overhead *)(((UV)ovp + 8) & ~7);
	    DEBUG_m(PerlIO_printf(Perl_debug_log, 
				  "fixing sbrk(): %d bytes off machine alignement\n",
				  (int)((UV)ovp & 7)));
	    (*nblksp)--;
# if defined(DEBUGGING_MSTATS)
	    /* This is only approx. if TWO_POT_OPTIMIZE: */
	    sbrk_slack += (1 << bucket);
# endif
	}
#endif
	sbrked_remains = require - needed;
    }
    last_sbrk_top = cp + require;
    last_op = (char*) cp;
#ifdef DEBUGGING_MSTATS
    goodsbrk += require;
#endif	
    return ovp;
}

static int
getpages_adjacent(int require)
{	    
    if (require <= sbrked_remains) {
	sbrked_remains -= require;
    } else {
	char *cp;

	require -= sbrked_remains;
	/* We do not try to optimize sbrks here, we go for place. */
	cp = (char*) sbrk(require);
#ifdef DEBUGGING_MSTATS
	sbrks++;
	goodsbrk += require;
#endif 
	if (cp == last_sbrk_top) {
	    sbrked_remains = 0;
	    last_sbrk_top = cp + require;
	} else {
	    if (cp == (char*)-1) {	/* Out of memory */
#ifdef DEBUGGING_MSTATS
		goodsbrk -= require;
#endif
		return 0;
	    }
	    /* Report the failure: */
	    if (sbrked_remains)
		add_to_chain((void*)(last_sbrk_top - sbrked_remains),
			     sbrked_remains, 0);
	    add_to_chain((void*)cp, require, 0);
	    sbrk_good -= SBRK_FAILURE_PRICE;
	    sbrked_remains = 0;
	    last_sbrk_top = 0;
	    last_op = 0;
	    return 0;
	}
    }
	    
    return 1;
}

/*
 * Allocate more memory to the indicated bucket.
 */
static void
morecore(register int bucket)
{
  	register union overhead *ovp;
  	register int rnu;       /* 2^rnu bytes will be requested */
  	int nblks;		/* become nblks blocks of the desired size */
	register MEM_SIZE siz, needed;

  	if (nextf[bucket])
  		return;
	if (bucket == sizeof(MEM_SIZE)*8*BUCKETS_PER_POW2) {
	    MUTEX_UNLOCK(&PL_malloc_mutex);
	    croak("%s", "Out of memory during ridiculously large request");
	}
	if (bucket > max_bucket)
	    max_bucket = bucket;

  	rnu = ( (bucket <= (LOG_OF_MIN_ARENA << BUCKET_POW2_SHIFT)) 
		? LOG_OF_MIN_ARENA 
		: (bucket >> BUCKET_POW2_SHIFT) );
	/* This may be overwritten later: */
  	nblks = 1 << (rnu - (bucket >> BUCKET_POW2_SHIFT)); /* how many blocks to get */
	needed = ((MEM_SIZE)1 << rnu) + POW2_OPTIMIZE_SURPLUS(bucket);
	if (nextf[rnu << BUCKET_POW2_SHIFT]) { /* 2048b bucket. */
	    ovp = nextf[rnu << BUCKET_POW2_SHIFT] - 1 + CHUNK_SHIFT;
	    nextf[rnu << BUCKET_POW2_SHIFT]
		= nextf[rnu << BUCKET_POW2_SHIFT]->ov_next;
#ifdef DEBUGGING_MSTATS
	    nmalloc[rnu << BUCKET_POW2_SHIFT]--;
	    start_slack -= M_OVERHEAD;
#endif 
	    DEBUG_m(PerlIO_printf(Perl_debug_log, 
				  "stealing %ld bytes from %ld arena\n",
				  (long) needed, (long) rnu << BUCKET_POW2_SHIFT));
	} else if (chunk_chain 
		   && (ovp = (union overhead*) get_from_chain(needed))) {
	    DEBUG_m(PerlIO_printf(Perl_debug_log, 
				  "stealing %ld bytes from chain\n",
				  (long) needed));
	} else if ( (ovp = (union overhead*)
		     get_from_bigger_buckets((rnu << BUCKET_POW2_SHIFT) + 1,
					     needed)) ) {
	    DEBUG_m(PerlIO_printf(Perl_debug_log, 
				  "stealing %ld bytes from bigger buckets\n",
				  (long) needed));
	} else if (needed <= sbrked_remains) {
	    ovp = (union overhead *)(last_sbrk_top - sbrked_remains);
	    sbrked_remains -= needed;
	    last_op = (char*)ovp;
	} else 
	    ovp = getpages(needed, &nblks, bucket);

	if (!ovp)
	    return;

	/*
	 * Add new memory allocated to that on
	 * free list for this hash bucket.
	 */
  	siz = BUCKET_SIZE(bucket);
#ifdef PACK_MALLOC
	*(u_char*)ovp = bucket;	/* Fill index. */
	if (bucket <= MAX_PACKED) {
	    ovp = (union overhead *) ((char*)ovp + BLK_SHIFT(bucket));
	    nblks = N_BLKS(bucket);
#  ifdef DEBUGGING_MSTATS
	    start_slack += BLK_SHIFT(bucket);
#  endif
	} else if (bucket < LOG_OF_MIN_ARENA * BUCKETS_PER_POW2) {
	    ovp = (union overhead *) ((char*)ovp + BLK_SHIFT(bucket));
	    siz -= sizeof(union overhead);
	} else ovp++;		/* One chunk per block. */
#endif /* PACK_MALLOC */
  	nextf[bucket] = ovp;
#ifdef DEBUGGING_MSTATS
	nmalloc[bucket] += nblks;
	if (bucket > MAX_PACKED) {
	    start_slack += M_OVERHEAD * nblks;
	}
#endif 
  	while (--nblks > 0) {
		ovp->ov_next = (union overhead *)((caddr_t)ovp + siz);
		ovp = (union overhead *)((caddr_t)ovp + siz);
  	}
	/* Not all sbrks return zeroed memory.*/
	ovp->ov_next = (union overhead *)NULL;
#ifdef PACK_MALLOC
	if (bucket == 7*BUCKETS_PER_POW2) { /* Special case, explanation is above. */
	    union overhead *n_op = nextf[7*BUCKETS_PER_POW2]->ov_next;
	    nextf[7*BUCKETS_PER_POW2] = 
		(union overhead *)((caddr_t)nextf[7*BUCKETS_PER_POW2] 
				   - sizeof(union overhead));
	    nextf[7*BUCKETS_PER_POW2]->ov_next = n_op;
	}
#endif /* !PACK_MALLOC */
}

Free_t
free(void *mp)
{   
  	register MEM_SIZE size;
	register union overhead *ovp;
	char *cp = (char*)mp;
#ifdef PACK_MALLOC
	u_char bucket;
#endif 

	DEBUG_m(PerlIO_printf(Perl_debug_log, 
			      "0x%lx: (%05lu) free\n",
			      (unsigned long)cp, (unsigned long)(PL_an++)));

	if (cp == NULL)
		return;
	ovp = (union overhead *)((caddr_t)cp 
				- sizeof (union overhead) * CHUNK_SHIFT);
#ifdef PACK_MALLOC
	bucket = OV_INDEX(ovp);
#endif 
#ifdef IGNORE_SMALL_BAD_FREE
	if ((bucket >= FIRST_BUCKET_WITH_CHECK) 
	    && (OV_MAGIC(ovp, bucket) != MAGIC))
#else
	if (OV_MAGIC(ovp, bucket) != MAGIC)
#endif 
	    {
		static int bad_free_warn = -1;
		if (bad_free_warn == -1) {
		    char *pbf = PerlEnv_getenv("PERL_BADFREE");
		    bad_free_warn = (pbf) ? atoi(pbf) : 1;
		}
		if (!bad_free_warn)
		    return;
#ifdef RCHECK
		warn("%s free() ignored",
		    ovp->ov_rmagic == RMAGIC - 1 ? "Duplicate" : "Bad");
#else
		warn("%s", "Bad free() ignored");
#endif
		return;				/* sanity */
	    }
	MUTEX_LOCK(&PL_malloc_mutex);
#ifdef RCHECK
  	ASSERT(ovp->ov_rmagic == RMAGIC, "chunk's head overwrite");
	if (OV_INDEX(ovp) <= MAX_SHORT_BUCKET) {
	    int i;
	    MEM_SIZE nbytes = ovp->ov_size + 1;

	    if ((i = nbytes & 3)) {
		i = 4 - i;
		while (i--) {
		    ASSERT(*((char *)((caddr_t)ovp + nbytes - RSLOP + i))
			   == RMAGIC_C, "chunk's tail overwrite");
		}
	    }
	    nbytes = (nbytes + 3) &~ 3; 
	    ASSERT(*(u_int *)((caddr_t)ovp + nbytes - RSLOP) == RMAGIC, "chunk's tail overwrite");	    
	}
	ovp->ov_rmagic = RMAGIC - 1;
#endif
  	ASSERT(OV_INDEX(ovp) < NBUCKETS, "chunk's head overwrite");
  	size = OV_INDEX(ovp);
	ovp->ov_next = nextf[size];
  	nextf[size] = ovp;
	MUTEX_UNLOCK(&PL_malloc_mutex);
}

/*
 * When a program attempts "storage compaction" as mentioned in the
 * old malloc man page, it realloc's an already freed block.  Usually
 * this is the last block it freed; occasionally it might be farther
 * back.  We have to search all the free lists for the block in order
 * to determine its bucket: 1st we make one pass thru the lists
 * checking only the first block in each; if that fails we search
 * ``reall_srchlen'' blocks in each list for a match (the variable
 * is extern so the caller can modify it).  If that fails we just copy
 * however many bytes was given to realloc() and hope it's not huge.
 */
int reall_srchlen = 4;  /* 4 should be plenty, -1 =>'s whole list */

Malloc_t
realloc(void *mp, size_t nbytes)
{   
  	register MEM_SIZE onb;
	union overhead *ovp;
  	char *res;
	int prev_bucket;
	register int bucket;
	int was_alloced = 0, incr;
	char *cp = (char*)mp;

#if defined(DEBUGGING) || !defined(PERL_CORE)
	MEM_SIZE size = nbytes;

	if ((long)nbytes < 0)
		croak("%s", "panic: realloc");
#endif

	BARK_64K_LIMIT("Reallocation",nbytes,size);
	if (!cp)
		return malloc(nbytes);

	MUTEX_LOCK(&PL_malloc_mutex);
	ovp = (union overhead *)((caddr_t)cp 
				- sizeof (union overhead) * CHUNK_SHIFT);
	bucket = OV_INDEX(ovp);
#ifdef IGNORE_SMALL_BAD_FREE
	if ((bucket < FIRST_BUCKET_WITH_CHECK) 
	    || (OV_MAGIC(ovp, bucket) == MAGIC))
#else
	if (OV_MAGIC(ovp, bucket) == MAGIC) 
#endif 
	{
		was_alloced = 1;
	} else {
		/*
		 * Already free, doing "compaction".
		 *
		 * Search for the old block of memory on the
		 * free list.  First, check the most common
		 * case (last element free'd), then (this failing)
		 * the last ``reall_srchlen'' items free'd.
		 * If all lookups fail, then assume the size of
		 * the memory block being realloc'd is the
		 * smallest possible.
		 */
		if ((bucket = findbucket(ovp, 1)) < 0 &&
		    (bucket = findbucket(ovp, reall_srchlen)) < 0)
			bucket = 0;
	}
	onb = BUCKET_SIZE_REAL(bucket);
	/* 
	 *  avoid the copy if same size block.
	 *  We are not agressive with boundary cases. Note that it might
	 *  (for a small number of cases) give false negative if
	 *  both new size and old one are in the bucket for
	 *  FIRST_BIG_POW2, but the new one is near the lower end.
	 *
	 *  We do not try to go to 1.5 times smaller bucket so far.
	 */
	if (nbytes > onb) incr = 1;
	else {
#ifdef DO_NOT_TRY_HARDER_WHEN_SHRINKING
	    if ( /* This is a little bit pessimal if PACK_MALLOC: */
		nbytes > ( (onb >> 1) - M_OVERHEAD )
#  ifdef TWO_POT_OPTIMIZE
		|| (bucket == FIRST_BIG_POW2 && nbytes >= LAST_SMALL_BOUND )
#  endif	
		)
#else  /* !DO_NOT_TRY_HARDER_WHEN_SHRINKING */
		prev_bucket = ( (bucket > MAX_PACKED + 1) 
				? bucket - BUCKETS_PER_POW2
				: bucket - 1);
	     if (nbytes > BUCKET_SIZE_REAL(prev_bucket))
#endif /* !DO_NOT_TRY_HARDER_WHEN_SHRINKING */
		 incr = 0;
	     else incr = -1;
	}
	if (!was_alloced
#ifdef STRESS_REALLOC
	    || 1 /* always do it the hard way */
#endif
	    ) goto hard_way;
	else if (incr == 0) {
	  inplace_label:
#ifdef RCHECK
		/*
		 * Record new allocated size of block and
		 * bound space with magic numbers.
		 */
		if (OV_INDEX(ovp) <= MAX_SHORT_BUCKET) {
		       int i, nb = ovp->ov_size + 1;

		       if ((i = nb & 3)) {
			   i = 4 - i;
			   while (i--) {
			       ASSERT(*((char *)((caddr_t)ovp + nb - RSLOP + i)) == RMAGIC_C, "chunk's tail overwrite");
			   }
		       }
		       nb = (nb + 3) &~ 3; 
		       ASSERT(*(u_int *)((caddr_t)ovp + nb - RSLOP) == RMAGIC, "chunk's tail overwrite");
			/*
			 * Convert amount of memory requested into
			 * closest block size stored in hash buckets
			 * which satisfies request.  Account for
			 * space used per block for accounting.
			 */
			nbytes += M_OVERHEAD;
			ovp->ov_size = nbytes - 1;
			if ((i = nbytes & 3)) {
			    i = 4 - i;
			    while (i--)
				*((char *)((caddr_t)ovp + nbytes - RSLOP + i))
				    = RMAGIC_C;
			}
			nbytes = (nbytes + 3) &~ 3; 
			*((u_int *)((caddr_t)ovp + nbytes - RSLOP)) = RMAGIC;
		}
#endif
		res = cp;
		MUTEX_UNLOCK(&PL_malloc_mutex);
		DEBUG_m(PerlIO_printf(Perl_debug_log, 
			      "0x%lx: (%05lu) realloc %ld bytes inplace\n",
			      (unsigned long)res,(unsigned long)(PL_an++),
			      (long)size));
	} else if (incr == 1 && (cp - M_OVERHEAD == last_op) 
		   && (onb > (1 << LOG_OF_MIN_ARENA))) {
	    MEM_SIZE require, newarena = nbytes, pow;
	    int shiftr;

	    POW2_OPTIMIZE_ADJUST(newarena);
	    newarena = newarena + M_OVERHEAD;
	    /* newarena = (newarena + 3) &~ 3; */
	    shiftr = (newarena - 1) >> LOG_OF_MIN_ARENA;
	    pow = LOG_OF_MIN_ARENA + 1;
	    /* apart from this loop, this is O(1) */
	    while (shiftr >>= 1)
  		pow++;
	    newarena = (1 << pow) + POW2_OPTIMIZE_SURPLUS(pow * BUCKETS_PER_POW2);
	    require = newarena - onb - M_OVERHEAD;
	    
	    if (getpages_adjacent(require)) {
#ifdef DEBUGGING_MSTATS
		nmalloc[bucket]--;
		nmalloc[pow * BUCKETS_PER_POW2]++;
#endif 	    
		*(cp - M_OVERHEAD) = pow * BUCKETS_PER_POW2; /* Fill index. */
		goto inplace_label;
	    } else
		goto hard_way;
	} else {
	  hard_way:
	    MUTEX_UNLOCK(&PL_malloc_mutex);
	    DEBUG_m(PerlIO_printf(Perl_debug_log, 
			      "0x%lx: (%05lu) realloc %ld bytes the hard way\n",
			      (unsigned long)cp,(unsigned long)(PL_an++),
			      (long)size));
	    if ((res = (char*)malloc(nbytes)) == NULL)
		return (NULL);
	    if (cp != res)			/* common optimization */
		Copy(cp, res, (MEM_SIZE)(nbytes<onb?nbytes:onb), char);
	    if (was_alloced)
		free(cp);
	}
  	return ((Malloc_t)res);
}

/*
 * Search ``srchlen'' elements of each free list for a block whose
 * header starts at ``freep''.  If srchlen is -1 search the whole list.
 * Return bucket number, or -1 if not found.
 */
static int
findbucket(union overhead *freep, int srchlen)
{
	register union overhead *p;
	register int i, j;

	for (i = 0; i < NBUCKETS; i++) {
		j = 0;
		for (p = nextf[i]; p && j != srchlen; p = p->ov_next) {
			if (p == freep)
				return (i);
			j++;
		}
	}
	return (-1);
}

Malloc_t
calloc(register size_t elements, register size_t size)
{
    long sz = elements * size;
    Malloc_t p = malloc(sz);

    if (p) {
	memset((void*)p, 0, sz);
    }
    return p;
}

MEM_SIZE
malloced_size(void *p)
{
    union overhead *ovp = (union overhead *)
	((caddr_t)p - sizeof (union overhead) * CHUNK_SHIFT);
    int bucket = OV_INDEX(ovp);
#ifdef RCHECK
    /* The caller wants to have a complete control over the chunk,
       disable the memory checking inside the chunk.  */
    if (bucket <= MAX_SHORT_BUCKET) {
	MEM_SIZE size = BUCKET_SIZE_REAL(bucket);
	ovp->ov_size = size + M_OVERHEAD - 1;
	*((u_int *)((caddr_t)ovp + size + M_OVERHEAD - RSLOP)) = RMAGIC;
    }
#endif
    return BUCKET_SIZE_REAL(bucket);
}

#ifdef DEBUGGING_MSTATS

#  ifdef BUCKETS_ROOT2
#    define MIN_EVEN_REPORT 6
#  else
#    define MIN_EVEN_REPORT MIN_BUCKET
#  endif 
/*
 * mstats - print out statistics about malloc
 * 
 * Prints two lines of numbers, one showing the length of the free list
 * for each size category, the second showing the number of mallocs -
 * frees for each size category.
 */
void
dump_mstats(char *s)
{
  	register int i, j;
  	register union overhead *p;
  	int topbucket=0, topbucket_ev=0, topbucket_odd=0, totfree=0, total=0;
	u_int nfree[NBUCKETS];
	int total_chain = 0;
	struct chunk_chain_s* nextchain = chunk_chain;

  	for (i = MIN_BUCKET ; i < NBUCKETS; i++) {
  		for (j = 0, p = nextf[i]; p; p = p->ov_next, j++)
  			;
		nfree[i] = j;
  		totfree += nfree[i] * BUCKET_SIZE_REAL(i);
  		total += nmalloc[i] * BUCKET_SIZE_REAL(i);
		if (nmalloc[i]) {
		    i % 2 ? (topbucket_odd = i) : (topbucket_ev = i);
		    topbucket = i;
		}
  	}
  	if (s)
	    PerlIO_printf(PerlIO_stderr(),
			  "Memory allocation statistics %s (buckets %ld(%ld)..%ld(%ld)\n",
			  s, 
			  (long)BUCKET_SIZE_REAL(MIN_BUCKET), 
			  (long)BUCKET_SIZE(MIN_BUCKET),
			  (long)BUCKET_SIZE_REAL(topbucket), (long)BUCKET_SIZE(topbucket));
  	PerlIO_printf(PerlIO_stderr(), "%8d free:", totfree);
  	for (i = MIN_EVEN_REPORT; i <= topbucket; i += BUCKETS_PER_POW2) {
  		PerlIO_printf(PerlIO_stderr(), 
			      ((i < 8*BUCKETS_PER_POW2 || i == 10*BUCKETS_PER_POW2)
			       ? " %5d" 
			       : ((i < 12*BUCKETS_PER_POW2) ? " %3d" : " %d")),
			      nfree[i]);
  	}
#ifdef BUCKETS_ROOT2
	PerlIO_printf(PerlIO_stderr(), "\n\t   ");
  	for (i = MIN_BUCKET + 1; i <= topbucket_odd; i += BUCKETS_PER_POW2) {
  		PerlIO_printf(PerlIO_stderr(), 
			      ((i < 8*BUCKETS_PER_POW2 || i == 10*BUCKETS_PER_POW2)
			       ? " %5d" 
			       : ((i < 12*BUCKETS_PER_POW2) ? " %3d" : " %d")),
			      nfree[i]);
  	}
#endif 
  	PerlIO_printf(PerlIO_stderr(), "\n%8d used:", total - totfree);
  	for (i = MIN_EVEN_REPORT; i <= topbucket; i += BUCKETS_PER_POW2) {
  		PerlIO_printf(PerlIO_stderr(), 
			      ((i < 8*BUCKETS_PER_POW2 || i == 10*BUCKETS_PER_POW2)
			       ? " %5d" 
			       : ((i < 12*BUCKETS_PER_POW2) ? " %3d" : " %d")), 
			      nmalloc[i] - nfree[i]);
  	}
#ifdef BUCKETS_ROOT2
	PerlIO_printf(PerlIO_stderr(), "\n\t   ");
  	for (i = MIN_BUCKET + 1; i <= topbucket_odd; i += BUCKETS_PER_POW2) {
  		PerlIO_printf(PerlIO_stderr(), 
			      ((i < 8*BUCKETS_PER_POW2 || i == 10*BUCKETS_PER_POW2)
			       ? " %5d" 
			       : ((i < 12*BUCKETS_PER_POW2) ? " %3d" : " %d")),
			      nmalloc[i] - nfree[i]);
  	}
#endif 
	while (nextchain) {
	    total_chain += nextchain->size;
	    nextchain = nextchain->next;
	}
	PerlIO_printf(PerlIO_stderr(), "\nTotal sbrk(): %d/%d:%d. Odd ends: pad+heads+chain+tail: %d+%d+%d+%d.\n",
		      goodsbrk + sbrk_slack, sbrks, sbrk_good, sbrk_slack,
		      start_slack, total_chain, sbrked_remains);
}
#else
void
dump_mstats(char *s)
{
}
#endif
#endif /* lint */


#ifdef USE_PERL_SBRK

#   if defined(__MACHTEN_PPC__) || defined(__NeXT__)
#      define PERL_SBRK_VIA_MALLOC
/*
 * MachTen's malloc() returns a buffer aligned on a two-byte boundary.
 * While this is adequate, it may slow down access to longer data
 * types by forcing multiple memory accesses.  It also causes
 * complaints when RCHECK is in force.  So we allocate six bytes
 * more than we need to, and return an address rounded up to an
 * eight-byte boundary.
 *
 * 980701 Dominic Dunlop <domo@computer.org>
 */
#      define SYSTEM_ALLOC(a) ((void *)(((unsigned)malloc((a)+6)+6)&~7))
#   endif

#   ifdef PERL_SBRK_VIA_MALLOC
#      if defined(HIDEMYMALLOC) || defined(EMBEDMYMALLOC)
#         undef malloc		/* Expose names that  */
#         undef calloc		/* HIDEMYMALLOC hides */
#         undef realloc
#         undef free
#      else
#         include "Error: -DPERL_SBRK_VIA_MALLOC needs -D(HIDE|EMBED)MYMALLOC"
#      endif

/* it may seem schizophrenic to use perl's malloc and let it call system */
/* malloc, the reason for that is only the 3.2 version of the OS that had */
/* frequent core dumps within nxzonefreenolock. This sbrk routine put an */
/* end to the cores */

#      ifndef SYSTEM_ALLOC
#         define SYSTEM_ALLOC(a) malloc(a)
#      endif

#   endif  /* PERL_SBRK_VIA_MALLOC */

static IV Perl_sbrk_oldchunk;
static long Perl_sbrk_oldsize;

#   define PERLSBRK_32_K (1<<15)
#   define PERLSBRK_64_K (1<<16)

Malloc_t
Perl_sbrk(int size)
{
    IV got;
    int small, reqsize;

    if (!size) return 0;
#ifdef PERL_CORE
    reqsize = size; /* just for the DEBUG_m statement */
#endif
#ifdef PACK_MALLOC
    size = (size + 0x7ff) & ~0x7ff;
#endif
    if (size <= Perl_sbrk_oldsize) {
	got = Perl_sbrk_oldchunk;
	Perl_sbrk_oldchunk += size;
	Perl_sbrk_oldsize -= size;
    } else {
      if (size >= PERLSBRK_32_K) {
	small = 0;
      } else {
	size = PERLSBRK_64_K;
	small = 1;
      }
      got = (IV)SYSTEM_ALLOC(size);
#ifdef PACK_MALLOC
      got = (got + 0x7ff) & ~0x7ff;
#endif
      if (small) {
	/* Chunk is small, register the rest for future allocs. */
	Perl_sbrk_oldchunk = got + reqsize;
	Perl_sbrk_oldsize = size - reqsize;
      }
    }

    DEBUG_m(PerlIO_printf(Perl_debug_log, "sbrk malloc size %ld (reqsize %ld), left size %ld, give addr 0x%lx\n",
		    size, reqsize, Perl_sbrk_oldsize, got));

    return (void *)got;
}

#endif /* ! defined USE_PERL_SBRK */
