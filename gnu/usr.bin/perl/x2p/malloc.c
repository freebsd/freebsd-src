/* $RCSfile: malloc.c,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:54 $
 *
 * $Log: malloc.c,v $
 * Revision 1.1.1.1  1994/09/10  06:27:54  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.2  1993/08/24  17:57:39  nate
 * Fix for ALIGN macros in PERL that conflict with 4.4 macros
 *
 * Revision 1.1.1.1  1993/08/23  21:30:11  nate
 * PERL!
 *
 * Revision 4.0.1.4  92/06/08  14:28:38  lwall
 * patch20: removed implicit int declarations on functions
 * patch20: hash tables now split only if the memory is available to do so
 * patch20: realloc(0, size) now does malloc in case library routines call it
 *
 * Revision 4.0.1.3  91/11/05  17:57:40  lwall
 * patch11: safe malloc code now integrated into Perl's malloc when possible
 *
 * Revision 4.0.1.2  91/06/07  11:20:45  lwall
 * patch4: many, many itty-bitty portability fixes
 *
 * Revision 4.0.1.1  91/04/11  17:48:31  lwall
 * patch1: Configure now figures out malloc ptr type
 *
 * Revision 4.0  91/03/20  01:28:52  lwall
 * 4.0 baseline.
 *
 */

#ifndef lint
/*SUPPRESS 592*/
static char sccsid[] = "@(#)malloc.c	4.3 (Berkeley) 9/16/83";

#ifdef DEBUGGING
#define RCHECK
#endif
/*
 * malloc.c (Caltech) 2/21/82
 * Chris Kingsley, kingsley@cit-20.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small
 * number of different sizes, and keeps free lists of each size.  Blocks that
 * don't exactly fit are passed up to the next larger size.  In this
 * implementation, the available sizes are 2^n-4 (or 2^n-12) bytes long.
 * This is designed for use in a program that uses vast quantities of memory,
 * but bombs when it runs out.
 */

#include "EXTERN.h"
#include "../perl.h"

static findbucket(), morecore();

/* I don't much care whether these are defined in sys/types.h--LAW */

#define u_char unsigned char
#define u_int unsigned int
#define u_short unsigned short

/*
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
#if ALIGN_BYTES > 4
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

#define	MAGIC		0xff		/* magic # on accounting info */
#define OLDMAGIC	0x7f		/* same after a free() */
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
static	union overhead *nextf[NBUCKETS];
extern	char *sbrk();

#ifdef MSTATS
/*
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
 */
static	u_int nmalloc[NBUCKETS];
#include <stdio.h>
#endif

#ifdef debug
#define	ASSERT(p)   if (!(p)) botch("p"); else
static void
botch(s)
	char *s;
{

	printf("assertion botched: %s\n", s);
	abort();
}
#else
#define	ASSERT(p)
#endif

#ifdef safemalloc
static int an = 0;
#endif

MALLOCPTRTYPE *
malloc(nbytes)
	register MEM_SIZE nbytes;
{
  	register union overhead *p;
  	register int bucket = 0;
  	register MEM_SIZE shiftr;

#ifdef safemalloc
#ifdef DEBUGGING
	MEM_SIZE size = nbytes;
#endif

#ifdef MSDOS
	if (nbytes > 0xffff) {
		fprintf(stderr, "Allocation too large: %lx\n", (long)nbytes);
		exit(1);
	}
#endif /* MSDOS */
#ifdef DEBUGGING
	if ((long)nbytes < 0)
	    fatal("panic: malloc");
#endif
#endif /* safemalloc */

	/*
	 * Convert amount of memory requested into
	 * closest block size stored in hash buckets
	 * which satisfies request.  Account for
	 * space used per block for accounting.
	 */
  	nbytes += sizeof (union overhead) + RSLOP;
  	nbytes = (nbytes + 3) &~ 3;
  	shiftr = (nbytes - 1) >> 2;
	/* apart from this loop, this is O(1) */
  	while (shiftr >>= 1)
  		bucket++;
	/*
	 * If nothing in hash bucket right now,
	 * request more memory from the system.
	 */
  	if (nextf[bucket] == NULL)
  		morecore(bucket);
  	if ((p = (union overhead *)nextf[bucket]) == NULL) {
#ifdef safemalloc
		if (!nomemok) {
		    fputs("Out of memory!\n", stderr);
		    exit(1);
		}
#else
  		return (NULL);
#endif
	}

#ifdef safemalloc
#ifdef DEBUGGING
#  if !(defined(I286) || defined(atarist))
    if (debug & 128)
        fprintf(stderr,"0x%x: (%05d) malloc %ld bytes\n",p+1,an++,(long)size);
#  else
    if (debug & 128)
        fprintf(stderr,"0x%lx: (%05d) malloc %ld bytes\n",p+1,an++,(long)size);
#  endif
#endif
#endif /* safemalloc */

	/* remove from linked list */
#ifdef RCHECK
	if (*((int*)p) & (sizeof(union overhead) - 1))
#if !(defined(I286) || defined(atarist))
	    fprintf(stderr,"Corrupt malloc ptr 0x%x at 0x%x\n",*((int*)p),p);
#else
	    fprintf(stderr,"Corrupt malloc ptr 0x%lx at 0x%lx\n",*((int*)p),p);
#endif
#endif
  	nextf[bucket] = p->ov_next;
	p->ov_magic = MAGIC;
	p->ov_index= bucket;
#ifdef MSTATS
  	nmalloc[bucket]++;
#endif
#ifdef RCHECK
	/*
	 * Record allocated size of block and
	 * bound space with magic numbers.
	 */
  	if (nbytes <= 0x10000)
		p->ov_size = nbytes - 1;
	p->ov_rmagic = RMAGIC;
  	*((u_int *)((caddr_t)p + nbytes - RSLOP)) = RMAGIC;
#endif
  	return ((MALLOCPTRTYPE *)(p + 1));
}

/*
 * Allocate more memory to the indicated bucket.
 */
static
morecore(bucket)
	register int bucket;
{
  	register union overhead *op;
  	register int rnu;       /* 2^rnu bytes will be requested */
  	register int nblks;     /* become nblks blocks of the desired size */
	register MEM_SIZE siz;

  	if (nextf[bucket])
  		return;
	/*
	 * Insure memory is allocated
	 * on a page boundary.  Should
	 * make getpageize call?
	 */
#ifndef atarist /* on the atari we dont have to worry about this */
  	op = (union overhead *)sbrk(0);
#ifndef I286
  	if ((int)op & 0x3ff)
  		(void)sbrk(1024 - ((int)op & 0x3ff));
#else
	/* The sbrk(0) call on the I286 always returns the next segment */
#endif
#endif /* atarist */

#if !(defined(I286) || defined(atarist))
	/* take 2k unless the block is bigger than that */
  	rnu = (bucket <= 8) ? 11 : bucket + 3;
#else
	/* take 16k unless the block is bigger than that
	   (80286s like large segments!), probably good on the atari too */
  	rnu = (bucket <= 11) ? 14 : bucket + 3;
#endif
  	nblks = 1 << (rnu - (bucket + 3));  /* how many blocks to get */
  	if (rnu < bucket)
		rnu = bucket;
	op = (union overhead *)sbrk(1L << rnu);
	/* no more room! */
  	if ((int)op == -1)
  		return;
	/*
	 * Round up to minimum allocation size boundary
	 * and deduct from block count to reflect.
	 */
#ifndef I286
  	if ((int)op & 7) {
  		op = (union overhead *)(((MEM_SIZE)op + 8) &~ 7);
  		nblks--;
  	}
#else
	/* Again, this should always be ok on an 80286 */
#endif
	/*
	 * Add new memory allocated to that on
	 * free list for this hash bucket.
	 */
  	nextf[bucket] = op;
  	siz = 1 << (bucket + 3);
  	while (--nblks > 0) {
		op->ov_next = (union overhead *)((caddr_t)op + siz);
		op = (union overhead *)((caddr_t)op + siz);
  	}
}

void
free(mp)
	MALLOCPTRTYPE *mp;
{
  	register MEM_SIZE size;
	register union overhead *op;
	char *cp = (char*)mp;

#ifdef safemalloc
#ifdef DEBUGGING
#  if !(defined(I286) || defined(atarist))
	if (debug & 128)
		fprintf(stderr,"0x%x: (%05d) free\n",cp,an++);
#  else
	if (debug & 128)
		fprintf(stderr,"0x%lx: (%05d) free\n",cp,an++);
#  endif
#endif
#endif /* safemalloc */

  	if (cp == NULL)
  		return;
	op = (union overhead *)((caddr_t)cp - sizeof (union overhead));
#ifdef debug
  	ASSERT(op->ov_magic == MAGIC);		/* make sure it was in use */
#else
	if (op->ov_magic != MAGIC) {
		warn("%s free() ignored",
		    op->ov_magic == OLDMAGIC ? "Duplicate" : "Bad");
		return;				/* sanity */
	}
	op->ov_magic = OLDMAGIC;
#endif
#ifdef RCHECK
  	ASSERT(op->ov_rmagic == RMAGIC);
	if (op->ov_index <= 13)
		ASSERT(*(u_int *)((caddr_t)op + op->ov_size + 1 - RSLOP) == RMAGIC);
#endif
  	ASSERT(op->ov_index < NBUCKETS);
  	size = op->ov_index;
	op->ov_next = nextf[size];
  	nextf[size] = op;
#ifdef MSTATS
  	nmalloc[size]--;
#endif
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
int reall_srchlen = 4;	/* 4 should be plenty, -1 =>'s whole list */

MALLOCPTRTYPE *
realloc(mp, nbytes)
	MALLOCPTRTYPE *mp;
	MEM_SIZE nbytes;
{
  	register MEM_SIZE onb;
	union overhead *op;
  	char *res;
	register int i;
	int was_alloced = 0;
	char *cp = (char*)mp;

#ifdef safemalloc
#ifdef DEBUGGING
	MEM_SIZE size = nbytes;
#endif

#ifdef MSDOS
	if (nbytes > 0xffff) {
		fprintf(stderr, "Reallocation too large: %lx\n", size);
		exit(1);
	}
#endif /* MSDOS */
	if (!cp)
		return malloc(nbytes);
#ifdef DEBUGGING
	if ((long)nbytes < 0)
		fatal("panic: realloc");
#endif
#endif /* safemalloc */

	op = (union overhead *)((caddr_t)cp - sizeof (union overhead));
	if (op->ov_magic == MAGIC) {
		was_alloced++;
		i = op->ov_index;
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
		if ((i = findbucket(op, 1)) < 0 &&
		    (i = findbucket(op, reall_srchlen)) < 0)
			i = 0;
	}
	onb = (1L << (i + 3)) - sizeof (*op) - RSLOP;
	/* avoid the copy if same size block */
	if (was_alloced &&
	    nbytes <= onb && nbytes > (onb >> 1) - sizeof(*op) - RSLOP) {
#ifdef RCHECK
		/*
		 * Record new allocated size of block and
		 * bound space with magic numbers.
		 */
		if (op->ov_index <= 13) {
			/*
			 * Convert amount of memory requested into
			 * closest block size stored in hash buckets
			 * which satisfies request.  Account for
			 * space used per block for accounting.
			 */
			nbytes += sizeof (union overhead) + RSLOP;
			nbytes = (nbytes + 3) &~ 3;
			op->ov_size = nbytes - 1;
			*((u_int *)((caddr_t)op + nbytes - RSLOP)) = RMAGIC;
		}
#endif
		res = cp;
	}
	else {
		if ((res = (char*)malloc(nbytes)) == NULL)
			return (NULL);
		if (cp != res)			/* common optimization */
			Copy(cp, res, (MEM_SIZE)(nbytes<onb?nbytes:onb), char);
		if (was_alloced)
			free(cp);
	}

#ifdef safemalloc
#ifdef DEBUGGING
#  if !(defined(I286) || defined(atarist))
	if (debug & 128) {
	    fprintf(stderr,"0x%x: (%05d) rfree\n",res,an++);
	    fprintf(stderr,"0x%x: (%05d) realloc %ld bytes\n",res,an++,(long)size);
	}
#  else
	if (debug & 128) {
	    fprintf(stderr,"0x%lx: (%05d) rfree\n",res,an++);
	    fprintf(stderr,"0x%lx: (%05d) realloc %ld bytes\n",res,an++,(long)size);
	}
#  endif
#endif
#endif /* safemalloc */
  	return ((MALLOCPTRTYPE*)res);
}

/*
 * Search ``srchlen'' elements of each free list for a block whose
 * header starts at ``freep''.  If srchlen is -1 search the whole list.
 * Return bucket number, or -1 if not found.
 */
static int
findbucket(freep, srchlen)
	union overhead *freep;
	int srchlen;
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

#ifdef MSTATS
/*
 * mstats - print out statistics about malloc
 *
 * Prints two lines of numbers, one showing the length of the free list
 * for each size category, the second showing the number of mallocs -
 * frees for each size category.
 */
void
mstats(s)
	char *s;
{
  	register int i, j;
  	register union overhead *p;
  	int totfree = 0,
  	totused = 0;

  	fprintf(stderr, "Memory allocation statistics %s\nfree:\t", s);
  	for (i = 0; i < NBUCKETS; i++) {
  		for (j = 0, p = nextf[i]; p; p = p->ov_next, j++)
  			;
  		fprintf(stderr, " %d", j);
  		totfree += j * (1 << (i + 3));
  	}
  	fprintf(stderr, "\nused:\t");
  	for (i = 0; i < NBUCKETS; i++) {
  		fprintf(stderr, " %d", nmalloc[i]);
  		totused += nmalloc[i] * (1 << (i + 3));
  	}
  	fprintf(stderr, "\n\tTotal in use: %d, total free: %d\n",
	    totused, totfree);
}
#endif
#endif /* lint */
