/*-
 * Copyright (c) 1983, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)alloc.c	5.8 (Berkeley) 6/8/91";
#endif /* not lint */

/*
 * tc.alloc.c from malloc.c (Caltech) 2/21/82
 * Chris Kingsley, kingsley@cit-20.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small
 * number of different sizes, and keeps free lists of each size.  Blocks that
 * don't exactly fit are passed up to the next larger size.  In this
 * implementation, the available sizes are 2^n-4 (or 2^n-12) bytes long.
 * This is designed for use in a program that uses vast quantities of memory,
 * but bombs when it runs out.
 */

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#if __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include "csh.h"
#include "extern.h"

char   *memtop = NULL;		/* PWP: top of current memory */
char   *membot = NULL;		/* PWP: bottom of allocatable memory */

#ifndef SYSMALLOC

#undef RCHECK
#undef DEBUG


#ifndef NULL
#define	NULL 0
#endif


/*
 * The overhead on a block is at least 4 bytes.  When free, this space
 * contains a pointer to the next free block, and the bottom two bits must
 * be zero.  When in use, the first byte is set to MAGIC, and the second
 * byte is the size index.  The remaining bytes are for alignment.
 * If range checking is enabled and the size of the block fits
 * in two bytes, then the top two bytes hold the size of the requested block
 * plus the range checking words, and the header word MINUS ONE.
 */

#define ROUNDUP	7

#define ALIGN(a) (((a) + ROUNDUP) & ~ROUNDUP)

union overhead {
    union overhead *ov_next;	/* when free */
    struct {
	u_char  ovu_magic;	/* magic number */
	u_char  ovu_index;	/* bucket # */
#ifdef RCHECK
	u_short ovu_size;	/* actual block size */
	u_int   ovu_rmagic;	/* range magic number */
#endif
    }       ovu;
#define	ov_magic	ovu.ovu_magic
#define	ov_index	ovu.ovu_index
#define	ov_size		ovu.ovu_size
#define	ov_rmagic	ovu.ovu_rmagic
};

#define	MAGIC		0xfd	/* magic # on accounting info */
#define RMAGIC		0x55555555	/* magic # on range info */
#ifdef RCHECK
#define	RSLOP		sizeof (u_int)
#else
#define	RSLOP		0
#endif

/*
 * nextf[i] is the pointer to the next free block of size 2^(i+3).  The
 * smallest allocatable block is 8 bytes.  The overhead information
 * precedes the data area returned to the user.
 */
#define	NBUCKETS 30
static union overhead *nextf[NBUCKETS];

static int	findbucket __P((union overhead *, int));
static void	morecore __P((int));

/*
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
 */
static u_int nmalloc[NBUCKETS];


#ifdef DEBUG
#define CHECK(a, str, p) \
    if (a) { \
	xprintf(str, p);	\
	xprintf("memtop = %lx membot = %lx.\n", memtop, membot);	\
	abort(); \
    }	\
    else
#else
#define CHECK(a, str, p) \
    if (a) { \
	xprintf(str, p);	\
	xprintf("memtop = %lx membot = %lx.\n", memtop, membot);	\
	return; \
    }	\
    else
#endif

ptr_t
malloc(nbytes)
    register size_t nbytes;
{
#ifndef lint
    register union overhead *p;
    register int bucket = 0;
    register unsigned shiftr;

    /*
     * Convert amount of memory requested into closest block size stored in
     * hash buckets which satisfies request.  Account for space used per block
     * for accounting.
     */
    nbytes = ALIGN(ALIGN(sizeof(union overhead)) + nbytes + RSLOP);
    shiftr = (nbytes - 1) >> 2;

    /* apart from this loop, this is O(1) */
    while (shiftr >>= 1)
	bucket++;
    /*
     * If nothing in hash bucket right now, request more memory from the
     * system.
     */
    if (nextf[bucket] == NULL)
	morecore(bucket);
    if ((p = (union overhead *) nextf[bucket]) == NULL) {
	child++;
#ifndef DEBUG
	stderror(ERR_NOMEM);
#else
	showall();
	xprintf("nbytes=%d: Out of memory\n", nbytes);
	abort();
#endif
	/* fool lint */
	return ((ptr_t) 0);
    }
    /* remove from linked list */
    nextf[bucket] = nextf[bucket]->ov_next;
    p->ov_magic = MAGIC;
    p->ov_index = bucket;
    nmalloc[bucket]++;
#ifdef RCHECK
    /*
     * Record allocated size of block and bound space with magic numbers.
     */
    if (nbytes <= 0x10000)
	p->ov_size = nbytes - 1;
    p->ov_rmagic = RMAGIC;
    *((u_int *) (((caddr_t) p) + nbytes - RSLOP)) = RMAGIC;
#endif
    return ((ptr_t) (((caddr_t) p) + ALIGN(sizeof(union overhead))));
#else
    if (nbytes)
	return ((ptr_t) 0);
    else
	return ((ptr_t) 0);
#endif				/* !lint */
}

#ifndef lint
/*
 * Allocate more memory to the indicated bucket.
 */
static void
morecore(bucket)
    register int bucket;
{
    register union overhead *op;
    register int rnu;		/* 2^rnu bytes will be requested */
    register int nblks;		/* become nblks blocks of the desired size */
    register int siz;

    if (nextf[bucket])
	return;
    /*
     * Insure memory is allocated on a page boundary.  Should make getpageize
     * call?
     */
    op = (union overhead *) sbrk(0);
    memtop = (char *) op;
    if (membot == NULL)
	membot = memtop;
    if ((int) op & 0x3ff) {
	memtop = (char *) sbrk(1024 - ((int) op & 0x3ff));
	memtop += 1024 - ((int) op & 0x3ff);
    }

    /* take 2k unless the block is bigger than that */
    rnu = (bucket <= 8) ? 11 : bucket + 3;
    nblks = 1 << (rnu - (bucket + 3));	/* how many blocks to get */
    if (rnu < bucket)
	rnu = bucket;
    memtop = (char *) sbrk(1 << rnu);	/* PWP */
    op = (union overhead *) memtop;
    memtop += 1 << rnu;
    /* no more room! */
    if ((int) op == -1)
	return;
    /*
     * Round up to minimum allocation size boundary and deduct from block count
     * to reflect.
     */
    if (((u_int) op) & ROUNDUP) {
	op = (union overhead *) (((u_int) op + (ROUNDUP + 1)) & ~ROUNDUP);
	nblks--;
    }
    /*
     * Add new memory allocated to that on free list for this hash bucket.
     */
    nextf[bucket] = op;
    siz = 1 << (bucket + 3);
    while (--nblks > 0) {
	op->ov_next = (union overhead *) (((caddr_t) op) + siz);
	op = (union overhead *) (((caddr_t) op) + siz);
    }
}

#endif

#ifdef sun
int
#else
void
#endif
free(cp)
    ptr_t   cp;
{
#ifndef lint
    register int size;
    register union overhead *op;

    if (cp == NULL)
	return;
    CHECK(!memtop || !membot, "free(%lx) called before any allocations.", cp);
    CHECK(cp > (ptr_t) memtop, "free(%lx) above top of memory.", cp);
    CHECK(cp < (ptr_t) membot, "free(%lx) above top of memory.", cp);
    op = (union overhead *) (((caddr_t) cp) - ALIGN(sizeof(union overhead)));
    CHECK(op->ov_magic != MAGIC, "free(%lx) bad block.", cp);

#ifdef RCHECK
    if (op->ov_index <= 13)
	CHECK(*(u_int *) ((caddr_t) op + op->ov_size + 1 - RSLOP) != RMAGIC,
	      "free(%lx) bad range check.", cp);
#endif
    CHECK(op->ov_index >= NBUCKETS, "free(%lx) bad block index.", cp);
    size = op->ov_index;
    op->ov_next = nextf[size];
    nextf[size] = op;

    nmalloc[size]--;

#else
    if (cp == NULL)
	return;
#endif
}

ptr_t
calloc(i, j)
    size_t  i, j;
{
#ifndef lint
    register char *cp, *scp;

    i *= j;
    scp = cp = (char *) xmalloc((size_t) i);
    if (i != 0)
	do
	    *cp++ = 0;
	while (--i);

    return (scp);
#else
    if (i && j)
	return ((ptr_t) 0);
    else
	return ((ptr_t) 0);
#endif
}

/*
 * When a program attempts "storage compaction" as mentioned in the
 * old malloc man page, it realloc's an already freed block.  Usually
 * this is the last block it freed; occasionally it might be farther
 * back.  We have to search all the free lists for the block in order
 * to determine its bucket: 1st we make one pass thru the lists
 * checking only the first block in each; if that fails we search
 * ``realloc_srchlen'' blocks in each list for a match (the variable
 * is extern so the caller can modify it).  If that fails we just copy
 * however many bytes was given to realloc() and hope it's not huge.
 */
#ifndef lint
int     realloc_srchlen = 4;	/* 4 should be plenty, -1 =>'s whole list */

#endif				/* lint */

ptr_t
realloc(cp, nbytes)
    ptr_t   cp;
    size_t  nbytes;
{
#ifndef lint
    register u_int onb;
    union overhead *op;
    char   *res;
    register int i;
    int     was_alloced = 0;

    if (cp == NULL)
	return (malloc(nbytes));
    op = (union overhead *) (((caddr_t) cp) - ALIGN(sizeof(union overhead)));
    if (op->ov_magic == MAGIC) {
	was_alloced++;
	i = op->ov_index;
    }
    else
	/*
	 * Already free, doing "compaction".
	 * 
	 * Search for the old block of memory on the free list.  First, check the
	 * most common case (last element free'd), then (this failing) the last
	 * ``realloc_srchlen'' items free'd. If all lookups fail, then assume
	 * the size of the memory block being realloc'd is the smallest
	 * possible.
	 */
	if ((i = findbucket(op, 1)) < 0 &&
	    (i = findbucket(op, realloc_srchlen)) < 0)
	i = 0;

    onb = ALIGN(nbytes + ALIGN(sizeof(union overhead)) + RSLOP);

    /* avoid the copy if same size block */
    if (was_alloced && (onb < (1 << (i + 3))) && (onb >= (1 << (i + 2))))
	return ((ptr_t) cp);
    if ((res = malloc(nbytes)) == NULL)
	return ((ptr_t) 0);
    if (cp != res)		/* common optimization */
	bcopy(cp, res, nbytes);
    if (was_alloced)
	free(cp);
    return (res);
#else
    if (cp && nbytes)
	return ((ptr_t) 0);
    else
	return ((ptr_t) 0);
#endif				/* !lint */
}



#ifndef lint
/*
 * Search ``srchlen'' elements of each free list for a block whose
 * header starts at ``freep''.  If srchlen is -1 search the whole list.
 * Return bucket number, or -1 if not found.
 */
static int
findbucket(freep, srchlen)
    union overhead *freep;
    int     srchlen;
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

#endif


#else				/* SYSMALLOC */

/**
 ** ``Protected versions'' of malloc, realloc, calloc, and free
 **
 ** On many systems:
 **
 ** 1. malloc(0) is bad
 ** 2. free(0) is bad
 ** 3. realloc(0, n) is bad
 ** 4. realloc(n, 0) is bad
 **
 ** Also we call our error routine if we run out of memory.
 **/
char   *
Malloc(n)
    size_t  n;
{
    ptr_t   ptr;

    n = n ? n : 1;

    if ((ptr = malloc(n)) == (ptr_t) 0) {
	child++;
	stderror(ERR_NOMEM);
    }
    return ((char *) ptr);
}

char   *
Realloc(p, n)
    ptr_t   p;
    size_t  n;
{
    ptr_t   ptr;

    n = n ? n : 1;
    if ((ptr = (p ? realloc(p, n) : malloc(n))) == (ptr_t) 0) {
	child++;
	stderror(ERR_NOMEM);
    }
    return ((char *) ptr);
}

char   *
Calloc(s, n)
    size_t  s, n;
{
    char   *sptr;
    ptr_t   ptr;

    n *= s;
    n = n ? n : 1;
    if ((ptr = malloc(n)) == (ptr_t) 0) {
	child++;
	stderror(ERR_NOMEM);
    }

    sptr = (char *) ptr;
    if (n != 0)
	do
	    *sptr++ = 0;
	while (--n);

    return ((char *) ptr);
}

void
Free(p)
    ptr_t   p;
{
    if (p)
	free(p);
}

#endif				/* SYSMALLOC */

/*
 * mstats - print out statistics about malloc
 *
 * Prints two lines of numbers, one showing the length of the free list
 * for each size category, the second showing the number of mallocs -
 * frees for each size category.
 */
void
showall()
{
#ifndef SYSMALLOC
    register int i, j;
    register union overhead *p;
    int     totfree = 0, totused = 0;

    xprintf("csh current memory allocation:\nfree:\t");
    for (i = 0; i < NBUCKETS; i++) {
	for (j = 0, p = nextf[i]; p; p = p->ov_next, j++);
	xprintf(" %4d", j);
	totfree += j * (1 << (i + 3));
    }
    xprintf("\nused:\t");
    for (i = 0; i < NBUCKETS; i++) {
	xprintf(" %4d", nmalloc[i]);
	totused += nmalloc[i] * (1 << (i + 3));
    }
    xprintf("\n\tTotal in use: %d, total free: %d\n",
	    totused, totfree);
    xprintf("\tAllocated memory from 0x%lx to 0x%lx.  Real top at 0x%lx\n",
	    membot, memtop, (char *) sbrk(0));
#else
    xprintf("Allocated memory from 0x%lx to 0x%lx (%ld).\n",
	    membot, memtop = (char *) sbrk(0), memtop - membot);
#endif				/* SYSMALLOC */
}
