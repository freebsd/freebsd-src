/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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

/*
 * malloc.c (Caltech) 2/21/82
 * Chris Kingsley, kingsley@cit-20.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small 
 * number of different sizes, and keeps free lists of each size.  Blocks that
 * don't exactly fit are passed up to the next larger size.  In this 
 * implementation, the available sizes are 2^n-4 (or 2^n-10) bytes long.
 * This is designed for use in a virtual memory environment.
 *
 * Modified for stand-alone use (for kzip) by Gary Jennejohn - 12FEB95
 */
#ifdef USE_KERNEL_INFLATE

#include <sys/types.h>

#define	NULL 0

/*
 * The overhead on a block is at least 4 bytes.  When free, this space
 * contains a pointer to the next free block, and the bottom two bits must
 * be zero.  When in use, the first byte is set to MAGIC, and the second
 * byte is the size index.  The remaining bytes are for alignment.
 * If range checking is enabled then a second word holds the size of the
 * requested block, less 1, rounded up to a multiple of sizeof(RMAGIC).
 * The order of elements is critical: ov_magic must overlay the low order
 * bits of ov_next, and ov_magic can not be a valid ov_next bit pattern.
 */
union	overhead {
	union	overhead *ov_next;	/* when free */
	struct {
		u_char	ovu_magic;	/* magic number */
		u_char	ovu_index;	/* bucket # */
	} ovu;
#define	ov_magic	ovu.ovu_magic
#define	ov_index	ovu.ovu_index
#define	ov_rmagic	ovu.ovu_rmagic
#define	ov_size		ovu.ovu_size
};

#define	MAGIC		0xef		/* magic # on accounting info */
#define RMAGIC		0x5555		/* magic # on range info */

#define	RSLOP		0

/*
 * nextf[i] is the pointer to the next free block of size 2^(i+3).  The
 * smallest allocatable block is 8 bytes.  The overhead information
 * precedes the data area returned to the user.
 */
#define	NBUCKETS 30
static	union overhead *nextf[NBUCKETS];

static	int pagesz;			/* page size */
static	int pagebucket;			/* page size bucket */

#define STORAGE 40960 /* must be at least this big to handle GENERIC */
unsigned char storage [STORAGE];
int next_free; /* need this to emulate sbrk() */

#define	ASSERT(p)

static void morecore (int);

void *
malloc(nbytes, junk1, junk2) /* junk? not used */
	size_t nbytes;
	int junk1, junk2;
{
  	register union overhead *op;
  	register int bucket, n;
	register unsigned amt;

	/*
	 * First time malloc is called, setup page size and
	 * align break pointer so all data will be page aligned.
	 */
	if (pagesz == 0) {
		pagesz = n = 4096;
		op = (union overhead *)storage;
  		n = n - sizeof (*op) - ((int)op & (n - 1));
		if (n < 0)
			n += pagesz;
  		if (n > STORAGE)
				return (NULL);

		next_free = n;

		bucket = 0;
		amt = 8;
		while (pagesz > amt) {
			amt <<= 1;
			bucket++;
		}
		pagebucket = bucket;
	}
	/*
	 * Convert amount of memory requested into closest block size
	 * stored in hash buckets which satisfies request.
	 * Account for space used per block for accounting.
	 */
	if (nbytes <= (n = pagesz - sizeof (*op) - RSLOP)) {
		amt = 8;	/* size of first bucket */
		bucket = 0;
		n = -(sizeof (*op) + RSLOP);
	} else {
		amt = pagesz;
		bucket = pagebucket;
	}
	while (nbytes > amt + n) {
		amt <<= 1;
		if (amt == 0)
			return (NULL);
		bucket++;
	}
	/*
	 * If nothing in hash bucket right now,
	 * request more memory from the system.
	 */
 	if ((op = nextf[bucket]) == NULL) {
  		morecore(bucket);
  		if ((op = nextf[bucket]) == NULL)
  			return (NULL);
	}

	/* remove from linked list */
  	nextf[bucket] = op->ov_next;
	op->ov_magic = MAGIC;
	op->ov_index = bucket;
  	return ((char *)(op + 1));
}

/*
 * Allocate more memory to the indicated bucket.
 */
static void
morecore(bucket)
	int bucket;
{
  	register union overhead *op;
	register int sz;		/* size of desired block */
  	int amt;			/* amount to allocate */
  	int nblks;			/* how many blocks we get */

	/*
	 * sbrk_size <= 0 only for big, FLUFFY, requests (about
	 * 2^30 bytes on a VAX, I think) or for a negative arg.
	 */
	sz = 1 << (bucket + 3);

	if (sz <= 0)
		return;

	if (sz < pagesz) {
		amt = pagesz;
  		nblks = amt / sz;
	} else {
		amt = sz + pagesz;
		nblks = 1;
	}
	/* no more room! */
	if (amt > (STORAGE - next_free))
		return;
	op = (union overhead *)&storage [next_free];
	next_free += amt;
	/*
	 * Add new memory allocated to that on
	 * free list for this hash bucket.
	 */
  	nextf[bucket] = op;
  	while (--nblks > 0) {
		op->ov_next = (union overhead *)((caddr_t)op + sz);
		op = (union overhead *)((caddr_t)op + sz);
  	}
}

void
free(cp, junk) /* junk not used */
	void *cp;
	int junk;
{   
  	register int size;
	register union overhead *op;

  	if (cp == NULL)
  		return;
	op = (union overhead *)((caddr_t)cp - sizeof (union overhead));
	if (op->ov_magic != MAGIC)
		return;				/* sanity */
  	size = op->ov_index;
  	ASSERT(size < NBUCKETS);
	op->ov_next = nextf[size];	/* also clobbers ov_magic */
  	nextf[size] = op;
}

#endif /* USE_KERNEL_INFLATE */
