/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*
 *  All globals are names starting with _malloc, which should not clash
 *  with anything else.
 */
/*
 *  Remember to initialize the variable in globals.c if you want, and
 *  provide an alternative short name in globrename.h
 */
#include "globrename.h"
#include "version.c"

/*
 *  _malloc_minchunk is the minimum number of units that a block can be
 *  cut down to.  If the difference between the required block size, and
 *  the first available block is greater than _malloc_minchunk, the
 *  block is chopped into two pieces, else the whole block is returned.
 *  The larger this is, the less fragmentation there will be, but the
 *  greater the waste. The actual minimum size of a block is therefore
 *  _malloc_minchunk*sizeof(Word) This consists of one word each for the
 *  boundary tags, one for the next and one for the prev pointers in a
 *  free block.
 */
size_t _malloc_minchunk = FIXEDOVERHEAD;

/* 
 *  _malloc_rover is the pointer that points 'someplace' in the free
 *  list. We start our search for a block from _malloc_rover, thus
 *  starting the search at a different place everytime, rather than at
 *  the start of the list.  This improves performance considerably, sez
 *  Knuth
 */
Word *_malloc_rover = NULL;
Word *_malloc_hiword = NULL;
Word *_malloc_loword = NULL;

/* 
 *  _malloc_sbrkunits is the multiple of sizeof(Word) to actually use in
 *  sbrk() calls - _malloc_sbrkunits should be large enough that sbrk
 *  isn't called too often, but small enough that any program that
 *  mallocs a few bytes doesn't end up being very large. I've set it to
 *  1K resulting in a sbrk'ed size of 8K. This is (coincidentally!) the
 *  pagesize on Suns.  I think that this seems a reasonable number for
 *  modern programs that malloc heavily. For small programs, you may
 *  want to set it to a lower number.
 */
size_t _malloc_sbrkunits = DEF_SBRKUNITS;

/*
 *  optimization of keeping total amount available, so we know to sbrk
 *  without searching list. No point searching list unless we have a
 *  fair chance of success. Ideally, we'd keep the size of the largest
 *  block available and a pointer to it, so we could check definitely if
 *  we had enough space. But that is too much housekeeping - we'd have to
 *  update that on all mallocs and frees too. (Updating
 *  _malloc_totalavail is easier)
 */
size_t _malloc_totalavail = 0;

Word *_malloc_mem = NULL;

/*
 *  Do not call any output routine other than fputs() - use sprintf() if
 *  you want to format something before printing. We don't want stdio
 *  calling malloc() if we can help it
 */
int _malloc_tracing = 0;	/* No tracing */
FILE *_malloc_statsfile = stderr;
char _malloc_statsbuf[128];

int _malloc_leaktrace = 0;

#ifdef PROFILESIZES
int _malloc_scount[MAXPROFILESIZE];
#endif /* PROFILESIZES */

#ifdef DEBUG
/*
 *  0 or 1 means checking all pointers before using them. Reasonably
 *  thorough.  2 means check the entire heap on every call to
 *  malloc/free/realloc/memalign. (the rest call these)
 */
int _malloc_debugging = 0;
#endif /* DEBUG */

univptr_t (* _malloc_memfunc) proto((size_t)) = _mal_sbrk;
