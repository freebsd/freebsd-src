#define inline

/*
Copyright (C) 1989 Free Software Foundation
    written by Doug Lea (dl@oswego.edu)

This file is part of GNU CC.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU CC General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
GNU CC, but only under the conditions described in the
GNU CC General Public License.   A copy of this license is
supposed to have been given to you along with GNU CC so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.
*/



#ifndef NO_LIBGXX_MALLOC   /* ignore whole file otherwise */

/* compile with -DMALLOC_STATS to collect statistics */
/* collecting statistics slows down malloc by at least 15% */

#ifdef MALLOC_STATS
#define UPDATE_STATS(ARGS) {ARGS;}
#else
#define UPDATE_STATS(ARGS)
#endif

/* History


   Tue Jan 16 04:54:27 1990  Doug Lea  (dl at g.oswego.edu)

     version 1 released in libg++

   Sun Jan 21 05:52:47 1990  Doug Lea  (dl at g.oswego.edu)

     bins are now own struct for, sanity.

     new victim search strategy: scan up and consolidate.
     Both faster and less fragmentation.

     refined when to scan bins for consolidation, via consollink, etc.

     realloc: always try to expand chunk, avoiding some fragmentation.

     changed a few inlines into macros

     hardwired SBRK_UNIT to 4096 for uniformity across systems

   Tue Mar 20 14:18:23 1990  Doug Lea  (dl at g.oswego.edu)

     calloc and cfree now correctly parameterized.

   Sun Apr  1 10:00:48 1990  Doug Lea  (dl at g.oswego.edu)

     added memalign and valloc.

   Sun Jun 24 05:46:48 1990  Doug Lea  (dl at g.oswego.edu)

     #include gepagesize.h only ifndef sun
     cache pagesize after first call

   Wed Jul 25 08:35:19 1990  Doug Lea  (dl at g.oswego.edu)

     No longer rely on a `designated victim':

       1. It sometimes caused splits of large chunks
          when smaller ones would do, leading to
          bad worst-case fragmentation.

       2. Scanning through the av array fast anyway,
          so the overhead isn't worth it.

     To compensate, several other minor changes:

       1. Unusable chunks are checked for consolidation during
          searches inside bins, better distributing chunks
          across bins.

       2. Chunks are returned when found in malloc_find_space,
           rather than finishing cleaning everything up, to
           avoid wasted iterations due to (1).
*/

/*
  A version of malloc/free/realloc tuned for C++ applications.

  Here's what you probably want to know first:

  In various tests, this appears to be about as fast as,
  and usually substantially less memory-wasteful than BSD/GNUemacs malloc.

  Generally, it is slower (by perhaps 20%) than bsd-style malloc
  only when bsd malloc would waste a great deal of space in
  fragmented blocks, which this malloc recovers; or when, by
  chance or design, nearly all requests are near the bsd malloc
  power-of-2 allocation bin boundaries, and as many chunks are
  used as are allocated.

  It uses more space than bsd malloc only when, again by chance
  or design, only bsdmalloc bin-sized requests are malloced, or when
  little dynamic space is malloced, since this malloc may grab larger
  chunks from the system at a time than bsd.

  In other words, this malloc seems generally superior to bsd
  except perhaps for programs that are specially tuned to
  deal with bsdmalloc's characteristics. But even here, the
  performance differences are slight.


  This malloc, like any other, is a compromised design.


  Chunks of memory are maintained using a `boundary tag' method as
  described in e.g., Knuth or Standish.  This means that the size of
  the chunk is stored both in the front of the chunk and at the end.
  This makes consolidating fragmented chunks into bigger chunks very fast.
  The size field is also used to hold bits representing whether a
  chunk is free or in use.

  Malloced chunks have space overhead of 8 bytes: The preceding
  and trailing size fields. When they are freed, the list pointer
  fields are also needed.

  Available chunks are kept in doubly linked lists. The lists are
  maintained in an array of bins using a power-of-two method, except
  that instead of 32 bins (one for each 1 << i), there are 128: each
  power of two is split in quarters. The use of very fine bin sizes
  closely approximates the use of one bin per actually used size,
  without necessitating the overhead of locating such bins. It is
  especially desirable in common C++ applications where large numbers
  of identically-sized blocks are malloced/freed in some dynamic
  manner, and then later are all freed. The finer bin sizes make
  finding blocks fast, with little wasted overallocation. The
  consolidation methods ensure that once the collection of blocks is
  no longer useful, fragments are gathered into bigger chunks awaiting new
  roles.

  The bins av[i] serve as heads of the lists. Bins contain a dummy
  header for the chunk lists, and a `dirty' field used to indicate
  whether the list may need to be scanned for consolidation.

  On allocation, the bin corresponding to the request size is
  scanned, and if there is a chunk with size >= requested, it
  is split, if too big, and used. Chunks on the list which are
  too small are examined for consolidation during this traversal.

  If no chunk exists in the list bigger bins are scanned in search of
  a victim.

  If no victim can be found, then smaller bins are examined for
  consolidation in order to construct a victim.

  Finally, if consolidation fails to come up with a usable chunk,
  more space is obtained from the system.

  After a split, the remainder is placed on
  the back of the appropriate bin list. (All freed chunks are placed
  on fronts of lists. All remaindered or consolidated chunks are
  placed on the rear. Correspondingly, searching within a bin
  starts at the front, but finding victims is from the back. All
  of this approximates  the  effect of having 2 kinds of lists per
  bin: returned chunks vs unallocated chunks, but without the overhead
  of maintaining 2 lists.)

  Deallocation (free) consists only of placing the chunk on
  a list.

  Reallocation proceeds in the usual way. If a chunk can be extended,
  it is, else a malloc-copy-free sequence is taken.

  memalign requests more than enough space from malloc, finds a
  spot within that chunk that meets the alignment request, and
  then possibly frees the leading and trailing space. Overreliance
  on memalign is a sure way to fragment space.


  Some other implementation matters:

  8 byte alignment is currently hardwired into the design. Calling
  memalign will return a chunk that is both 8-byte aligned, and
  meets the requested alignment.

  The basic overhead of a used chunk is 8 bytes: 4 at the front and
  4 at the end.

  When a chunk is free, 8 additional bytes are needed for free list
  pointers. Thus, the minimum allocatable size is 16 bytes.

  The existence of front and back overhead permits some reasonably
  effective fence-bashing checks: The front and back fields must
  be identical. This is checked only within free() and realloc().
  The checks are fast enough to be made non-optional.

  The overwriting of parts of freed memory with the freelist pointers
  can also be very effective (albeit in an annoying way) in helping
  users track down dangling pointers.

  User overwriting of freed space will often result in crashes
  within malloc or free.

  These routines are also tuned to C++ in that free(0) is a noop and
  a failed malloc automatically calls (*new_handler)().

  malloc(0) returns a pointer to something of the minimum allocatable size.

  Additional memory is gathered from the system (via sbrk) in a
  way that allows chunks obtained across different sbrk calls to
  be consolidated, but does not require contiguous memory: Thus,
  it should be safe to intersperse mallocs with other sbrk calls.

  This malloc is NOT designed to work in multiprocessing applications.
  No semaphores or other concurrency control are provided to ensure
  that multiple malloc or free calls don't run at the same time,
  which could be disasterous.

  VERY heavy use of inlines is made, for clarity. If this malloc
  is ported via a compiler without inlining capabilities, all
  inlines should be transformed into macros -- making them non-inline
  makes malloc at least twice as slow.


*/


/* preliminaries */

#ifdef __cplusplus
#include <stdio.h>
#else
#include "//usr/include/stdio.h"  /* needed for error reporting */
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USG
extern void*     memset(void*, int, int);
extern void*     memcpy(void*,  const void*, int);
/*inline void      bzero(void* s, int l) { memset(s, 0, l); }*/
#else
/*extern void      bzero(void*, unsigned int);*/
#endif

/*extern void      bcopy(void*, void*, unsigned int);*/

extern void*     sbrk(unsigned int);

/* Put this in instead of commmented out stuff above.  */
#define bcopy(s,d,n)	memcpy((d),(s),(n))
#define bcmp(s1,s2,n)	memcmp((s1),(s2),(n))
#define bzero(s,n)	memset((s),0,(n))


#ifdef __GNUC__
extern volatile void abort();
#else
extern void abort();
#endif

#ifdef __cplusplus
};  /* end of extern "C" */
#endif


/* A good multiple to call sbrk with */

#define SBRK_UNIT 4096



/* how to die on detected error */

#ifdef __GNUC__
static volatile void malloc_user_error()
#else
static void malloc_user_error()
#endif
{
  fputs("malloc/free/realloc: clobbered space detected\n", stderr); abort();
}



/*  Basic overhead for each malloc'ed chunk */


struct malloc_chunk
{
  unsigned int         size;     /* Size in bytes, including overhead. */
                                 /* Or'ed with INUSE if in use. */

  struct malloc_chunk* fd;       /* double links -- used only if free. */
  struct malloc_chunk* bk;

};

typedef struct malloc_chunk* mchunkptr;

struct malloc_bin
{
  struct malloc_chunk hd;        /* dummy list header */
  unsigned int        dirty;     /* True if maybe consolidatable */
                                 /* Wasting a word here makes */
                                 /* sizeof(bin) a power of 2, */
                                 /* which makes size2bin() faster */
};

typedef struct malloc_bin* mbinptr;


/*  sizes, alignments */


#define SIZE_SZ                   (sizeof(unsigned int))
#define MALLOC_MIN_OVERHEAD       (SIZE_SZ + SIZE_SZ)
#define MALLOC_ALIGN_MASK         (MALLOC_MIN_OVERHEAD - 1)

#define MINSIZE (sizeof(struct malloc_chunk) + SIZE_SZ) /* MUST == 16! */


/* pad request bytes into a usable size */

static inline unsigned int request2size(unsigned int request)
{
  return  (request == 0) ?  MINSIZE :
    ((request + MALLOC_MIN_OVERHEAD + MALLOC_ALIGN_MASK)
      & ~(MALLOC_ALIGN_MASK));
}


static inline int aligned_OK(void* m)
{
  return ((unsigned int)(m) & (MALLOC_ALIGN_MASK)) == 0;
}


/* size field or'd with INUSE when in use */
#define INUSE  0x1



/* the bins, initialized to have null double linked lists */

#define MAXBIN 120   /* 1 more than needed for 32 bit addresses */

#define FIRSTBIN (&(av[0]))

static struct malloc_bin  av[MAXBIN] =
{
  { { 0, &(av[0].hd),  &(av[0].hd) }, 0 },
  { { 0, &(av[1].hd),  &(av[1].hd) }, 0 },
  { { 0, &(av[2].hd),  &(av[2].hd) }, 0 },
  { { 0, &(av[3].hd),  &(av[3].hd) }, 0 },
  { { 0, &(av[4].hd),  &(av[4].hd) }, 0 },
  { { 0, &(av[5].hd),  &(av[5].hd) }, 0 },
  { { 0, &(av[6].hd),  &(av[6].hd) }, 0 },
  { { 0, &(av[7].hd),  &(av[7].hd) }, 0 },
  { { 0, &(av[8].hd),  &(av[8].hd) }, 0 },
  { { 0, &(av[9].hd),  &(av[9].hd) }, 0 },

  { { 0, &(av[10].hd), &(av[10].hd) }, 0 },
  { { 0, &(av[11].hd), &(av[11].hd) }, 0 },
  { { 0, &(av[12].hd), &(av[12].hd) }, 0 },
  { { 0, &(av[13].hd), &(av[13].hd) }, 0 },
  { { 0, &(av[14].hd), &(av[14].hd) }, 0 },
  { { 0, &(av[15].hd), &(av[15].hd) }, 0 },
  { { 0, &(av[16].hd), &(av[16].hd) }, 0 },
  { { 0, &(av[17].hd), &(av[17].hd) }, 0 },
  { { 0, &(av[18].hd), &(av[18].hd) }, 0 },
  { { 0, &(av[19].hd), &(av[19].hd) }, 0 },

  { { 0, &(av[20].hd), &(av[20].hd) }, 0 },
  { { 0, &(av[21].hd), &(av[21].hd) }, 0 },
  { { 0, &(av[22].hd), &(av[22].hd) }, 0 },
  { { 0, &(av[23].hd), &(av[23].hd) }, 0 },
  { { 0, &(av[24].hd), &(av[24].hd) }, 0 },
  { { 0, &(av[25].hd), &(av[25].hd) }, 0 },
  { { 0, &(av[26].hd), &(av[26].hd) }, 0 },
  { { 0, &(av[27].hd), &(av[27].hd) }, 0 },
  { { 0, &(av[28].hd), &(av[28].hd) }, 0 },
  { { 0, &(av[29].hd), &(av[29].hd) }, 0 },

  { { 0, &(av[30].hd), &(av[30].hd) }, 0 },
  { { 0, &(av[31].hd), &(av[31].hd) }, 0 },
  { { 0, &(av[32].hd), &(av[32].hd) }, 0 },
  { { 0, &(av[33].hd), &(av[33].hd) }, 0 },
  { { 0, &(av[34].hd), &(av[34].hd) }, 0 },
  { { 0, &(av[35].hd), &(av[35].hd) }, 0 },
  { { 0, &(av[36].hd), &(av[36].hd) }, 0 },
  { { 0, &(av[37].hd), &(av[37].hd) }, 0 },
  { { 0, &(av[38].hd), &(av[38].hd) }, 0 },
  { { 0, &(av[39].hd), &(av[39].hd) }, 0 },

  { { 0, &(av[40].hd), &(av[40].hd) }, 0 },
  { { 0, &(av[41].hd), &(av[41].hd) }, 0 },
  { { 0, &(av[42].hd), &(av[42].hd) }, 0 },
  { { 0, &(av[43].hd), &(av[43].hd) }, 0 },
  { { 0, &(av[44].hd), &(av[44].hd) }, 0 },
  { { 0, &(av[45].hd), &(av[45].hd) }, 0 },
  { { 0, &(av[46].hd), &(av[46].hd) }, 0 },
  { { 0, &(av[47].hd), &(av[47].hd) }, 0 },
  { { 0, &(av[48].hd), &(av[48].hd) }, 0 },
  { { 0, &(av[49].hd), &(av[49].hd) }, 0 },

  { { 0, &(av[50].hd), &(av[50].hd) }, 0 },
  { { 0, &(av[51].hd), &(av[51].hd) }, 0 },
  { { 0, &(av[52].hd), &(av[52].hd) }, 0 },
  { { 0, &(av[53].hd), &(av[53].hd) }, 0 },
  { { 0, &(av[54].hd), &(av[54].hd) }, 0 },
  { { 0, &(av[55].hd), &(av[55].hd) }, 0 },
  { { 0, &(av[56].hd), &(av[56].hd) }, 0 },
  { { 0, &(av[57].hd), &(av[57].hd) }, 0 },
  { { 0, &(av[58].hd), &(av[58].hd) }, 0 },
  { { 0, &(av[59].hd), &(av[59].hd) }, 0 },

  { { 0, &(av[60].hd), &(av[60].hd) }, 0 },
  { { 0, &(av[61].hd), &(av[61].hd) }, 0 },
  { { 0, &(av[62].hd), &(av[62].hd) }, 0 },
  { { 0, &(av[63].hd), &(av[63].hd) }, 0 },
  { { 0, &(av[64].hd), &(av[64].hd) }, 0 },
  { { 0, &(av[65].hd), &(av[65].hd) }, 0 },
  { { 0, &(av[66].hd), &(av[66].hd) }, 0 },
  { { 0, &(av[67].hd), &(av[67].hd) }, 0 },
  { { 0, &(av[68].hd), &(av[68].hd) }, 0 },
  { { 0, &(av[69].hd), &(av[69].hd) }, 0 },

  { { 0, &(av[70].hd), &(av[70].hd) }, 0 },
  { { 0, &(av[71].hd), &(av[71].hd) }, 0 },
  { { 0, &(av[72].hd), &(av[72].hd) }, 0 },
  { { 0, &(av[73].hd), &(av[73].hd) }, 0 },
  { { 0, &(av[74].hd), &(av[74].hd) }, 0 },
  { { 0, &(av[75].hd), &(av[75].hd) }, 0 },
  { { 0, &(av[76].hd), &(av[76].hd) }, 0 },
  { { 0, &(av[77].hd), &(av[77].hd) }, 0 },
  { { 0, &(av[78].hd), &(av[78].hd) }, 0 },
  { { 0, &(av[79].hd), &(av[79].hd) }, 0 },

  { { 0, &(av[80].hd), &(av[80].hd) }, 0 },
  { { 0, &(av[81].hd), &(av[81].hd) }, 0 },
  { { 0, &(av[82].hd), &(av[82].hd) }, 0 },
  { { 0, &(av[83].hd), &(av[83].hd) }, 0 },
  { { 0, &(av[84].hd), &(av[84].hd) }, 0 },
  { { 0, &(av[85].hd), &(av[85].hd) }, 0 },
  { { 0, &(av[86].hd), &(av[86].hd) }, 0 },
  { { 0, &(av[87].hd), &(av[87].hd) }, 0 },
  { { 0, &(av[88].hd), &(av[88].hd) }, 0 },
  { { 0, &(av[89].hd), &(av[89].hd) }, 0 },

  { { 0, &(av[90].hd), &(av[90].hd) }, 0 },
  { { 0, &(av[91].hd), &(av[91].hd) }, 0 },
  { { 0, &(av[92].hd), &(av[92].hd) }, 0 },
  { { 0, &(av[93].hd), &(av[93].hd) }, 0 },
  { { 0, &(av[94].hd), &(av[94].hd) }, 0 },
  { { 0, &(av[95].hd), &(av[95].hd) }, 0 },
  { { 0, &(av[96].hd), &(av[96].hd) }, 0 },
  { { 0, &(av[97].hd), &(av[97].hd) }, 0 },
  { { 0, &(av[98].hd), &(av[98].hd) }, 0 },
  { { 0, &(av[99].hd), &(av[99].hd) }, 0 },

  { { 0, &(av[100].hd), &(av[100].hd) }, 0 },
  { { 0, &(av[101].hd), &(av[101].hd) }, 0 },
  { { 0, &(av[102].hd), &(av[102].hd) }, 0 },
  { { 0, &(av[103].hd), &(av[103].hd) }, 0 },
  { { 0, &(av[104].hd), &(av[104].hd) }, 0 },
  { { 0, &(av[105].hd), &(av[105].hd) }, 0 },
  { { 0, &(av[106].hd), &(av[106].hd) }, 0 },
  { { 0, &(av[107].hd), &(av[107].hd) }, 0 },
  { { 0, &(av[108].hd), &(av[108].hd) }, 0 },
  { { 0, &(av[109].hd), &(av[109].hd) }, 0 },

  { { 0, &(av[110].hd), &(av[110].hd) }, 0 },
  { { 0, &(av[111].hd), &(av[111].hd) }, 0 },
  { { 0, &(av[112].hd), &(av[112].hd) }, 0 },
  { { 0, &(av[113].hd), &(av[113].hd) }, 0 },
  { { 0, &(av[114].hd), &(av[114].hd) }, 0 },
  { { 0, &(av[115].hd), &(av[115].hd) }, 0 },
  { { 0, &(av[116].hd), &(av[116].hd) }, 0 },
  { { 0, &(av[117].hd), &(av[117].hd) }, 0 },
  { { 0, &(av[118].hd), &(av[118].hd) }, 0 },
  { { 0, &(av[119].hd), &(av[119].hd) }, 0 }
};

/*
  indexing into bins
*/

static inline mbinptr size2bin(unsigned int sz)
{
  mbinptr b = av;
  while (sz >= (MINSIZE * 2)) { b += 4; sz >>= 1; } /* find power of 2 */
  b += (sz - MINSIZE) >> 2;                         /* find quadrant */
  return b;
}



/* counts maintained if MALLOC_STATS defined */

#ifdef MALLOC_STATS

static unsigned int sbrked_mem;
static unsigned int requested_mem;
static unsigned int malloced_mem;
static unsigned int freed_mem;
static unsigned int max_used_mem;

static unsigned int n_sbrks;
static unsigned int n_mallocs;
static unsigned int n_frees;
static unsigned int n_reallocs;
static unsigned int n_reallocs_with_copy;
static unsigned int n_avail;
static unsigned int max_inuse;

static unsigned int n_malloc_chunks;
static unsigned int n_malloc_bins;

static unsigned int n_split;
static unsigned int n_consol;


static void do_malloc_stats(const mchunkptr p)
{
  ++n_mallocs;
  if ((n_mallocs-n_frees) > max_inuse)
    max_inuse = n_mallocs - n_frees;
  malloced_mem += (p->size & ~(INUSE));
  if (malloced_mem - freed_mem > max_used_mem)
    max_used_mem = malloced_mem - freed_mem;
}

static void do_free_stats(const mchunkptr p)
{
  ++n_frees;
  freed_mem += (p->size & ~(INUSE));
}

#endif



/* Utilities needed below for memalign */
/* This is redundant with libg++ support, but not if used stand-alone */

static unsigned int gcd(unsigned int a, unsigned int b)
{
  unsigned int tmp;

  if (b > a)
  {
    tmp = a; a = b; b = tmp;
  }
  for(;;)
  {
    if (b == 0)
      return a;
    else if (b == 1)
      return b;
    else
    {
      tmp = b;
      b = a % b;
      a = tmp;
    }
  }
}

static inline unsigned int lcm(unsigned int x, unsigned int y)
{
  return x / gcd(x, y) * y;
}



/* maintaining INUSE via size field */


#define inuse(p)       ((p)->size & INUSE)
#define set_inuse(p)   ((p)->size |= INUSE)
#define clear_inuse(b) ((p)->size &= ~INUSE)


/* operations on  malloc_chunk addresses */


/* return ptr to next physical malloc_chunk */

#define next_chunk(p) ((mchunkptr)((char*)(p) + (p)->size))

/* return ptr to previous physical malloc_chunk */

#define prev_chunk(p) ((mchunkptr)((char*)(p)-((((int*)(p))[-1]) & ~(INUSE))))

/* place size at front and back of chunk */


static inline void set_size(mchunkptr p, unsigned int sz)
{
  p->size = *((int*)((char*)(p) + sz - SIZE_SZ)) = sz;
}




/* conversion from malloc headers to user pointers, and back */

static inline void* chunk2mem(mchunkptr p)
{
  void *mem;
  set_inuse(p);
mem =  (void*)((char*)(p) + SIZE_SZ);
  return mem;
}

/* xxxx my own */
mchunkptr sanity_check(void* mem)
{
  mchunkptr p = (mchunkptr)((char*)(mem) - SIZE_SZ);

  /* a quick sanity check */
  unsigned int sz = p->size & ~(INUSE);
  if (p->size == sz || sz != *((int*)((char*)(p) + sz - SIZE_SZ)))
    malloc_user_error();

  return p;
}




static inline mchunkptr mem2chunk(void* mem)
{
  mchunkptr p = (mchunkptr)((char*)(mem) - SIZE_SZ);

  /* a quick sanity check */
  unsigned int sz = p->size & ~(INUSE);
  if (p->size == sz || sz != *((int*)((char*)(p) + sz - SIZE_SZ)))
    malloc_user_error();

  p->size = sz;   /* clears INUSE */
  return p;
}



/* maintaining bins & pointers */


/* maximum bin actually used */

static mbinptr malloc_maxbin = FIRSTBIN;


/* operations on lists inside bins */


/* take a chunk off a list */

static inline void unlink(mchunkptr p)
{
  mchunkptr b = p->bk;
  mchunkptr f = p->fd;

  f->bk = b;  b->fd = f;

  UPDATE_STATS (--n_avail);
}



/* split a chunk and place on the back of a list */

static inline void split(mchunkptr p, unsigned int offset)
{
  unsigned int room = p->size - offset;
  if (room >= MINSIZE)
  {
    mbinptr   bn = size2bin(room);                  /* new bin */
    mchunkptr h  = &(bn->hd);                       /* its head */
    mchunkptr b  = h->bk;                           /* old back element */
    mchunkptr t = (mchunkptr)((char*)(p) + offset); /* remaindered chunk */

    /* set size */
    t->size = *((int*)((char*)(t) + room    - SIZE_SZ)) = room;

    /* link up */
    t->bk = b;  t->fd = h;  h->bk = b->fd = t;

    /* adjust maxbin (h == b means was empty) */
    if (h == b && bn > malloc_maxbin) malloc_maxbin = bn;

    /* adjust size of chunk to be returned */
    p->size = *((int*)((char*)(p) + offset  - SIZE_SZ)) = offset;

    UPDATE_STATS ((++n_split, ++n_avail));
  }
}



/* place a consolidated chunk on the back of a list */
/* like above, except no split */

static inline void consollink(mchunkptr p)
{
  mbinptr   bn = size2bin(p->size);
  mchunkptr h  = &(bn->hd);
  mchunkptr b  = h->bk;

  p->bk = b;  p->fd = h;  h->bk = b->fd = p;

  if (h == b && bn > malloc_maxbin) malloc_maxbin = bn;

  UPDATE_STATS(++n_avail);
}


/* place a freed chunk on the front of a list */

static inline void frontlink(mchunkptr p)
{
  mbinptr   bn = size2bin(p->size);
  mchunkptr h  = &(bn->hd);
  mchunkptr f  = h->fd;

  p->bk = h;  p->fd = f;  f->bk = h->fd = p;

  if (h == f && bn > malloc_maxbin) malloc_maxbin = bn;

  bn->dirty = 1;

  UPDATE_STATS(++n_avail);
}



/* Dealing with sbrk */


/* To link consecutive sbrk regions when possible */

static int* last_sbrk_end;


/*  who to call when sbrk returns failure */

#ifndef NO_NEW_HANDLER
typedef volatile void (*vfp)();
#ifdef __cplusplus
extern "C" vfp __new_handler;
#else
extern vfp __new_handler;
#endif
#endif

static mchunkptr malloc_from_sys(unsigned nb)
{
  mchunkptr p;
  unsigned int sbrk_size;
  int* ip;

  /* Minimally, we need to pad with enough space */
  /* to place dummy size/use fields to ends if needed */

  sbrk_size = ((nb + SBRK_UNIT - 1 + SIZE_SZ + SIZE_SZ)
               / SBRK_UNIT) * SBRK_UNIT;

  ip = (int*)(sbrk(sbrk_size));
  if ((char*)ip == (char*)(-1)) /* sbrk returns -1 on failure */
  {
#ifndef NO_NEW_HANDLER
    (*__new_handler) ();
#endif
    return 0;
  }

  UPDATE_STATS ((++n_sbrks, sbrked_mem += sbrk_size));


  if (last_sbrk_end != &ip[-1])
  {
    /* It's either first time through or someone else called sbrk. */
    /* Arrange end-markers at front & back */

    /* Shouldn't be necessary, but better to be safe */
    while (!aligned_OK(ip)) { ++ip; sbrk_size -= SIZE_SZ; }


    /* Mark the front as in use to prevent merging. */
    /* Note we can get away with only 1 word, not MINSIZE overhead here */

    *ip++ = SIZE_SZ | INUSE;

    p = (mchunkptr)ip;
    set_size(p,sbrk_size - (SIZE_SZ + SIZE_SZ));

  }
  else
  {
    mchunkptr l;

    /* We can safely make the header start at end of prev sbrked chunk. */
    /* We will still have space left at the end from a previous call */
    /* to place the end marker, below */

    p = (mchunkptr)(last_sbrk_end);
    set_size(p, sbrk_size);


    /* Even better, maybe we can merge with last fragment: */

    l = prev_chunk(p);
    if (!inuse(l))
    {
      unlink(l);
      set_size(l, p->size + l->size);
      p = l;
    }

  }

  /* mark the end of sbrked space as in use to prevent merging */

  last_sbrk_end = (int*)((char*)p + p->size);
  *last_sbrk_end = SIZE_SZ | INUSE;

  UPDATE_STATS((++n_avail, ++n_malloc_chunks));

  /* make it safe to unlink in malloc */
  UPDATE_STATS(++n_avail);
  p->fd = p->bk = p;

  return p;
}



/* Consolidate dirty bins. */
/* Stop if found a chunk big enough to satisfy current malloc request */

/* (It requires much less bookkeeping to consolidate entire bins */
/* at once than to keep records of which chunks might be */
/* consolidatable. So long as the lists are short, which we */
/* try to ensure via small bin ranges, there is little wasted effort.) */

static mchunkptr malloc_find_space(unsigned int nb)
{
  mbinptr b;

  /* first, re-adjust max used bin */

  while (malloc_maxbin >= FIRSTBIN &&
         malloc_maxbin->hd.bk == &(malloc_maxbin->hd))
  {
    malloc_maxbin->dirty = 0;
    --malloc_maxbin;
  }

  for (b = malloc_maxbin; b >= FIRSTBIN; --b)
  {
    UPDATE_STATS(++n_malloc_bins);

    if (b->dirty)
    {
      mchunkptr h = &(b->hd);         /* head of list */
      mchunkptr p = h->fd;            /* chunk traverser */

      while (p != h)
      {
        mchunkptr nextp = p->fd;       /* save, in case of relinks */
        int consolidated = 0;          /* only unlink/relink if consolidated */

        mchunkptr t;

        UPDATE_STATS(++n_malloc_chunks);

        while (!inuse(t = prev_chunk(p))) /* consolidate backward */
        {
          if (!consolidated) { consolidated = 1; unlink(p); }
          if (t == nextp) nextp = t->fd;
          unlink(t);
          set_size(t, t->size + p->size);
          p = t;
          UPDATE_STATS (++n_consol);
        }

        while (!inuse(t = next_chunk(p))) /* consolidate forward */
        {
          if (!consolidated) { consolidated = 1; unlink(p); }
          if (t == nextp) nextp = t->fd;
          unlink(t);
          set_size(p, p->size + t->size);
          UPDATE_STATS (++n_consol);
        }

       if (consolidated)
       {
          if (p->size >= nb)
          {
            /* make it safe to unlink in malloc */
            UPDATE_STATS(++n_avail);
            p->fd = p->bk = p;
            return p;
          }
          else
            consollink(p);
        }

        p = nextp;

      }

      b->dirty = 0;

    }
  }

  /* nothing available - sbrk some more */

  return malloc_from_sys(nb);
}



/*   Finally, the user-level functions  */

void* malloc(unsigned int bytes)
{
  unsigned int nb  = request2size(bytes);  /* padded request size */
  mbinptr      b   = size2bin(nb);         /* corresponding bin */
  mchunkptr    hd  = &(b->hd);             /* head of its list */
  mchunkptr    p   = hd->fd;               /* chunk traverser */

  UPDATE_STATS((requested_mem+=bytes, ++n_malloc_bins));

  /* Try a (near) exact match in own bin */
  /* clean out unusable but consolidatable chunks in bin while traversing */

  while (p != hd)
  {
    UPDATE_STATS(++n_malloc_chunks);
    if (p->size >= nb)
      goto found;
    else    /* try to consolidate; same code as malloc_find_space */
    {
      mchunkptr nextp = p->fd;       /* save, in case of relinks */
      int consolidated = 0;          /* only unlink/relink if consolidated */

      mchunkptr t;

      while (!inuse(t = prev_chunk(p))) /* consolidate backward */
      {
        if (!consolidated) { consolidated = 1; unlink(p); }
        if (t == nextp) nextp = t->fd;
        unlink(t);
        set_size(t, t->size + p->size);
        p = t;
        UPDATE_STATS (++n_consol);
      }

      while (!inuse(t = next_chunk(p))) /* consolidate forward */
      {
        if (!consolidated) { consolidated = 1; unlink(p); }
        if (t == nextp) nextp = t->fd;
        unlink(t);
        set_size(p, p->size + t->size);
        UPDATE_STATS (++n_consol);
      }

      if (consolidated)
      {
        if (p->size >= nb)
        {
          /* make it safe to unlink again below */
          UPDATE_STATS(++n_avail);
          p->fd = p->bk = p;
          goto found;
        }
        else
          consollink(p);
      }

      p = nextp;

    }
  }

  b->dirty = 0; /* true if got here */

  /*  Scan bigger bins for a victim */

  while (++b <= malloc_maxbin)
  {
    UPDATE_STATS(++n_malloc_bins);
    if ((p = b->hd.bk) != &(b->hd))    /* no need to check size */
      goto found;
  }

  /* Consolidate or sbrk */

  p = malloc_find_space(nb);

  if (p == 0) return 0; /* allocation failure */

 found:   /* Use what we found */

  unlink(p);
  split(p, nb);
  UPDATE_STATS(do_malloc_stats(p));
  return chunk2mem(p);
}




void free(void* mem)
{
  if (mem != 0)
  {
    mchunkptr p = mem2chunk(mem);
    UPDATE_STATS(do_free_stats(p));
    frontlink(p);
  }
}


void* calloc(unsigned int n, unsigned int elem_size)
{
  unsigned int sz = n * elem_size;
  void* p = malloc(sz);
  bzero(p, sz);
  return p;
};

/* This is here for compatibility with older systems */
void cfree(void *mem)
{
  free(mem);
}


unsigned int malloc_usable_size(void* mem)
{
  if (mem == 0)
    return 0;
  else
  {
    mchunkptr p = (mchunkptr)((char*)(mem) - SIZE_SZ);
    unsigned int sz = p->size & ~(INUSE);
    if (p->size == sz || sz != *((int*)((char*)(p) + sz - SIZE_SZ)))
      return 0;
    else
      return sz - MALLOC_MIN_OVERHEAD;
  }
}



void* realloc(void* mem, unsigned int bytes)
{
  if (mem == 0)
    return malloc(bytes);
  else
  {
    unsigned int nb      = request2size(bytes);
    mchunkptr    p       = mem2chunk(mem);
    unsigned int oldsize = p->size;
    int          room;
    mchunkptr    nxt;

    UPDATE_STATS((++n_reallocs, requested_mem += bytes-oldsize));

    /* try to expand (even if already big enough), to clean up chunk */

    while (!inuse(nxt = next_chunk(p)))
    {
      UPDATE_STATS ((malloced_mem += nxt->size, ++n_consol));
      unlink(nxt);
      set_size(p, p->size + nxt->size);
    }

    room = p->size - nb;
    if (room >= 0)
    {
      split(p, nb);
      UPDATE_STATS(malloced_mem -= room);
      return chunk2mem(p);
    }
    else /* do the obvious */
    {
      void* newmem;
      set_inuse(p);    /* don't let malloc consolidate us yet! */
      newmem = malloc(nb);
      bcopy(mem, newmem, oldsize - SIZE_SZ);
      free(mem);
      UPDATE_STATS(++n_reallocs_with_copy);
      return newmem;
    }
  }
}



/* return a pointer to space with at least the alignment requested */

void* memalign(unsigned int alignment, unsigned int bytes)
{
  mchunkptr p;
  unsigned int nb = request2size(bytes);

  /* find an alignment that both we and the user can live with: */
  /* least common multiple guarantees mutual happiness */
  unsigned int align = lcm(alignment, MALLOC_MIN_OVERHEAD);
  unsigned int mask = align - 1;

  /* call malloc with worst case padding to hit alignment; */
  /* we will give back extra */

  unsigned int req = nb + align + MINSIZE;
  void* m = malloc(req);

  if (m == 0) return m;

  p = mem2chunk(m);

  /* keep statistics on track */

  UPDATE_STATS(--n_mallocs);
  UPDATE_STATS(malloced_mem -= p->size);
  UPDATE_STATS(requested_mem -= req);
  UPDATE_STATS(requested_mem += bytes);

  if (((int)(m) & (mask)) != 0) /* misaligned */
  {

    /* find an aligned spot inside chunk */

    mchunkptr ap = (mchunkptr)(( ((int)(m) + mask) & -align) - SIZE_SZ);

    unsigned int gap = (unsigned int)(ap) - (unsigned int)(p);
    unsigned int room;

    /* we need to give back leading space in a chunk of at least MINSIZE */

    if (gap < MINSIZE)
    {
      /* This works since align >= MINSIZE */
      /* and we've malloc'd enough total room */

      ap = (mchunkptr)( (int)(ap) + align );
      gap += align;
    }

    if (gap + nb > p->size) /* can't happen unless chunk sizes corrupted */
      malloc_user_error();

    room = p->size - gap;

    /* give back leader */
    set_size(p, gap);
    consollink(p);

    /* use the rest */
    p = ap;
    set_size(p, room);
  }

  /* also give back spare room at the end */

  split(p, nb);
  UPDATE_STATS(do_malloc_stats(p));
  return chunk2mem(p);

}

#ifndef sun
#include "getpagesize.h"
#endif

static unsigned int malloc_pagesize = 0;

void* valloc(unsigned int bytes)
{
  if (malloc_pagesize == 0) malloc_pagesize = getpagesize();
  return memalign (malloc_pagesize, bytes);
}


void malloc_stats()
{
#ifndef MALLOC_STATS
}
#else
  int i;
  mchunkptr p;
  double nm = (double)(n_mallocs + n_reallocs);

  fprintf(stderr, "\nmalloc statistics\n\n");

  if (n_mallocs != 0)
  fprintf(stderr, "requests  = %10u total size = %10u\tave = %10u\n",
          n_mallocs, requested_mem, requested_mem/n_mallocs);

  if (n_mallocs != 0)
  fprintf(stderr, "mallocs   = %10u total size = %10u\tave = %10u\n",
          n_mallocs, malloced_mem, malloced_mem/n_mallocs);

  if (n_frees != 0)
  fprintf(stderr, "frees     = %10u total size = %10u\tave = %10u\n",
          n_frees, freed_mem, freed_mem/n_frees);

  if (n_mallocs-n_frees != 0)
  fprintf(stderr, "in use    = %10u total size = %10u\tave = %10u\n",
          n_mallocs-n_frees, malloced_mem-freed_mem,
          (malloced_mem-freed_mem) / (n_mallocs-n_frees));

  if (max_inuse != 0)
  fprintf(stderr, "max in use= %10u total size = %10u\tave = %10u\n",
          max_inuse, max_used_mem, max_used_mem / max_inuse);

  if (n_avail != 0)
  fprintf(stderr, "available = %10u total size = %10u\tave = %10u\n",
          n_avail, sbrked_mem - (malloced_mem-freed_mem),
          (sbrked_mem - (malloced_mem-freed_mem)) / n_avail);

  if (n_sbrks != 0)
  fprintf(stderr, "sbrks     = %10u total size = %10u\tave = %10u\n\n",
          n_sbrks, sbrked_mem, sbrked_mem/ n_sbrks);

  if (n_reallocs != 0)
  fprintf(stderr, "reallocs  = %10u with copy  = %10u\n\n",
          n_reallocs, n_reallocs_with_copy);


  if (nm != 0)
  {
    fprintf(stderr, "chunks scanned per malloc = %6.3f\n",
            n_malloc_chunks / nm);
    fprintf(stderr, "bins scanned per malloc   = %6.3f\n",
            n_malloc_bins / nm);
    fprintf(stderr, "splits per malloc         = %6.3f\n",
            n_split / nm);
    fprintf(stderr, "consolidations per malloc = %6.3f\n",
            n_consol / nm);
  }

  fprintf(stderr, "\nfree chunks:\n");
  for (i = 0; i < MAXBIN; ++i)
  {
    p = av[i].hd.fd;
    if (p != &(av[i].hd))
    {
      unsigned int count = 1;
      unsigned int sz = p->size;
      for (p = p->fd; p != &(av[i].hd); p = p->fd)
      {
        if (p->size == sz)
          ++count;
        else
        {
          fprintf(stderr, "\tsize = %10u count = %5u\n", sz, count);
          count = 1;
          sz = p->size;
        }
      }

      fprintf(stderr, "\tsize = %10u count = %5u\n", sz, count);

    }
  }
}
#endif /* MALLOC_STATS */

#endif /* NO_LIBGXX_MALLOC */


