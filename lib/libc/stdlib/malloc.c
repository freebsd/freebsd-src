/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 */

/*
 * Defining SANITY will enable some checks which will tell you if the users
 * program did botch something
 */
#define SANITY 

/*
 * Defining EXTRA_SANITY will enable some checks which are mostly related
 * to internal conditions in malloc.c
 */
#define EXTRA_SANITY

/*
 * Very verbose progress on stdout...
 */
#if 0
#  define TRACE(foo)    printf  foo
static int malloc_event;
#else
#  define TRACE(foo)	
#endif

#if defined(__i386__) && defined(__FreeBSD__)
#   warning FreeBSD i386 constants hardcoded.
/*
 * If these weren't defined here, they would be calculated on the fly
 */
#   define malloc_pagesize		4096U
#   define malloc_pageshift		12U
#   define malloc_minsize		32U
#endif /* __i386__ && __FreeBSD__ */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <err.h>
#include <sys/types.h>
#include <sys/mman.h>

/*
 * This structure describes a page's worth of chunks.
 */

struct pginfo {
    struct pginfo	*next;	/* next on the free list */
    void		*page;	/* Pointer to the page */
    u_short		size;	/* size of this page's chunks */
    u_short		shift;	/* How far to shift for this size chunks */
    u_short		free;	/* How many free chunks */
    u_short		total;	/* How many chunk */
    u_long		bits[1]; /* Which chunks are free */
};

struct pgfree {
    struct pgfree	*next;	/* next run of free pages */
    struct pgfree	*prev;	/* prev run of free pages */
    void		*page;	/* pointer to free pages */
    void		*end;	/* pointer to end of free pages */
    u_long		size;	/* number of bytes free */
};

/*
 * How many bits per u_long in the bitmap.
 * Change only if not 8 bits/byte
 */
#define	MALLOC_BITS	(8*sizeof(u_long))

/*
 * Magic values to put in the page_directory
 */
#define MALLOC_NOT_MINE	((struct pginfo*) 0)
#define MALLOC_FREE 	((struct pginfo*) 1)
#define MALLOC_FIRST	((struct pginfo*) 2)
#define MALLOC_FOLLOW	((struct pginfo*) 3)
#define MALLOC_MAGIC	((struct pginfo*) 4)

/*
 * The i386 architecture has some very convenient instructions.
 * We might as well use them.
 */
#ifdef __i386__
#  warning i386 inline assembly used.
#define ffs _ffs
static __inline int
_ffs(unsigned input)
{
	int result;
	asm("bsfl %1,%0" : "=r" (result) : "r" (input));
	return result+1;
}

#define fls _fls
static __inline int
_fls(unsigned input)
{
	int result;
	asm("bsrl %1,%0" : "=r" (result) : "r" (input));
	return result+1;
}

#define set_bit _set_bit
static __inline void
_set_bit(struct pginfo *pi, int bit)
{
	asm("btsl %0,(%1)" :
	: "r" (bit & (MALLOC_BITS-1)), "r" (pi->bits+(bit/MALLOC_BITS)));
}

#define clr_bit _clr_bit
static __inline void
_clr_bit(struct pginfo *pi, int bit)
{
	asm("btcl %0,(%1)" :
	: "r" (bit & (MALLOC_BITS-1)), "r" (pi->bits+(bit/MALLOC_BITS)));
}

#endif __i386__

/*
 * Set to one when malloc_init has been called
 */
static	unsigned	initialized;

/*
 * The size of a page.
 * Must be a integral multiplum of the granularity of mmap(2).
 * Your toes will curl if it isn't a power of two
 */
#define malloc_pagemask	((malloc_pagesize)-1)

/*
 * The size of the largest chunk.
 * Half a page.
 */
#define malloc_maxsize	((malloc_pagesize)>>1)

/*
 * malloc_pagesize == 1 << malloc_pageshift
 */
#ifndef malloc_pageshift
static	unsigned	malloc_pageshift;
#endif /* malloc_pageshift */

/*
 * The smallest allocation we bother about.
 * Must be power of two
 */
#ifndef malloc_minsize
static	unsigned  malloc_minsize;
#endif /* malloc_minsize */

/*
 * The largest chunk we care about.
 * Must be smaller than pagesize
 * Must be power of two
 */
#ifndef malloc_maxsize
static	unsigned  malloc_maxsize;
#endif /* malloc_maxsize */

#ifndef malloc_cache
static	unsigned  malloc_cache;
#endif /* malloc_cache */

/*
 * The offset from pagenumber to index into the page directory
 */
static	u_long  malloc_origo;

/*
 * The last index in the page directory we care about
 */
static	u_long  last_index;

/*
 * Pointer to page directory.
 * Allocated "as if with" malloc
 */
static	struct	pginfo **page_dir;

/*
 * How many slots in the page directory
 */
static	unsigned	malloc_ninfo;

/*
 * Free pages line up here 
 */
static struct pgfree	free_list;

/*
 * Abort() if we fail to get VM ?
 */
static int malloc_abort;

/*
 * Are we trying to die ?
 */
static int suicide;

/*
 * dump statistics
 */
static int malloc_stats;

/*
 * always realloc ?
 */
static int malloc_realloc;

/*
 * my last break.
 */
static void *malloc_brk;

/*
 * one location cache for free-list holders
 */
static struct pgfree *px;

static int set_pgdir(void *ptr, struct  pginfo *info);
static int extend_page_directory(u_long index);

void
malloc_dump(FILE *fd)
{
    struct pginfo **pd;
    struct pgfree *pf;
    int j;

    pd = page_dir;

    /* print out all the pages */
    for(j=0;j<=last_index;j++) {
	fprintf(fd,"%08lx %5d ",(j+malloc_origo) << malloc_pageshift,j);
	if (pd[j] == MALLOC_NOT_MINE) {
	    for(j++;j<=last_index && pd[j] == MALLOC_NOT_MINE;j++)
		;
	    j--;
	    fprintf(fd,".. %5d not mine\n",	j);
	} else if (pd[j] == MALLOC_FREE) {
	    for(j++;j<=last_index && pd[j] == MALLOC_FREE;j++)
		;
	    j--;
	    fprintf(fd,".. %5d free\n", j);
	} else if (pd[j] == MALLOC_FIRST) {
	    for(j++;j<=last_index && pd[j] == MALLOC_FOLLOW;j++)
		;
	    j--;
	    fprintf(fd,".. %5d in use\n", j);
	} else if (pd[j] < MALLOC_MAGIC) {
	    fprintf(fd,"(%p)\n", pd[j]);
	} else {
	    fprintf(fd,"%p %d (of %d) x %d @ %p --> %p\n",
		pd[j],pd[j]->free, pd[j]->total, 
		pd[j]->size, pd[j]->page, pd[j]->next);
	}
    }

    for(pf=free_list.next; pf; pf=pf->next) {
	fprintf(fd,"Free: @%p [%p...%p[ %ld ->%p <-%p\n",
		pf,pf->page,pf->end,pf->size,pf->prev,pf->next);
	if (pf == pf->next) {
		fprintf(fd,"Free_list loops.\n");
		break;
	}
    }

    /* print out various info */
    fprintf(fd,"Minsize\t%d\n",malloc_minsize);
    fprintf(fd,"Maxsize\t%d\n",malloc_maxsize);
    fprintf(fd,"Pagesize\t%d\n",malloc_pagesize);
    fprintf(fd,"Pageshift\t%d\n",malloc_pageshift);
    fprintf(fd,"FirstPage\t%ld\n",malloc_origo);
    fprintf(fd,"LastPage\t%ld %lx\n",last_index+malloc_pageshift,
	(last_index + malloc_pageshift) << malloc_pageshift);
    fprintf(fd,"Break\t%ld\n",(u_long)sbrk(0) >> malloc_pageshift);
}

static void
wrterror(char *p)
{
    char *q = "malloc() error: ";
    suicide = 1;
    write(2,q,strlen(q));
    write(2,p,strlen(p));
    malloc_dump(stderr);
    abort();
}

static void
wrtwarning(char *p)
{
    char *q = "malloc() warning: ";
    write(2,q,strlen(q));
    write(2,p,strlen(p));
    if (malloc_abort) {
	suicide = 1;
	abort();
    }
}

static void
malloc_exit()
{
    FILE *fd = fopen("malloc.out","a");
    if (fd)
        malloc_dump(fd);
    fclose(fd);
}


/*
 * Allocate a number of pages from the OS
 */
static caddr_t
map_pages(int pages, int update)
{
    caddr_t result,tail;

    result = sbrk(0) + malloc_pagemask - 1;
    result = (caddr_t) ((u_long)result & ~malloc_pagemask);
    tail = result + (pages << malloc_pageshift);
    if (!brk(tail)) {
	last_index = ((u_long)tail >> malloc_pageshift) - malloc_origo -1;
	malloc_brk = tail;
	TRACE(("%6d S %p .. %p\n",malloc_event++, result, tail));
	if (update && 
	  last_index >= malloc_ninfo &&
	  !extend_page_directory(last_index))
	    ;
	else
	    return result;
    }
    TRACE(("%6d s %d %p %d\n",malloc_event++,pages,sbrk(0),errno));
#ifdef EXTRA_SANITY
    wrterror("map_pages fails\n");
#endif
    return 0;
}

/*
 * Set a bit in the bitmap
 */
#ifndef set_bit
static __inline void
set_bit(struct pginfo *pi, int bit)
{
    pi->bits[bit/MALLOC_BITS] |= 1<<(bit%MALLOC_BITS);
}
#endif /* set_bit */

/*
 * Clear a bit in the bitmap
 */
#ifndef clr_bit
static __inline void
clr_bit(struct pginfo *pi, int bit)
{
    pi->bits[bit/MALLOC_BITS] &= ~(1<<(bit%MALLOC_BITS));
}
#endif /* clr_bit */

#ifndef tst_bit
/*
 * Test a bit in the bitmap
 */
static __inline int
tst_bit(struct pginfo *pi, int bit)
{
    return pi->bits[bit/MALLOC_BITS] & (1<<(bit%MALLOC_BITS));
}
#endif /* tst_bit */

/*
 * Find last bit
 */
#ifndef fls
static __inline int
fls(int size)
{
    int i = 1;
    while (size >>= 1)
	i++;
    return i;
}
#endif /* fls */

/*
 * Extend page directory
 */
static int
extend_page_directory(u_long index)
{
    struct  pginfo **new,**old;
    int i;

    TRACE(("%6d E %lu\n",malloc_event++,index));
    
    /* Make it this many pages */
    i = index * sizeof *page_dir;
    i /= malloc_pagesize;
    i += 2;

    /* Get new pages, if you used this much mem you don't care :-) */
    new = (struct pginfo**) map_pages(i,0);
    if (!new)
	return 0;

    /* Copy the old stuff */
    memset(new, 0, i * malloc_pagesize);
    memcpy(new, page_dir,
	    malloc_ninfo * sizeof *page_dir);

    /* register the new size */
    malloc_ninfo = i * malloc_pagesize / sizeof *page_dir;

    /* swap the pointers */
    old = page_dir;
    page_dir = new;

    /* Mark the pages */
    index = ((u_long)new >> malloc_pageshift) - malloc_origo;
    page_dir[index] = MALLOC_FIRST;
    while (--i) {
	page_dir[++index] = MALLOC_FOLLOW;
    }

    /* Now free the old stuff */
    free(old);
    return 1;
}

/*
 * Set entry in page directory.
 * Extend page directory if need be.
 */
static int
set_pgdir(void *ptr, struct  pginfo *info)
{
    u_long index = ((u_long)ptr >> malloc_pageshift) - malloc_origo;

    if (index >= malloc_ninfo && !extend_page_directory(index))
	return 0;
    page_dir[index] = info;
    return 1;
}

/*
 * Initialize the world
 */
static void
malloc_init ()
{
    int i;
    char *p;

    TRACE(("%6d I\n",malloc_event++));
    for (p=getenv("MALLOC_OPTIONS"); p && *p; p++) {
	switch (*p) {
	    case 'a': malloc_abort = 0; break;
	    case 'A': malloc_abort = 1; break;
	    case 'd': malloc_stats = 0; break;
	    case 'D': malloc_stats = 1; break;
	    case 'r': malloc_realloc = 0; break;
	    case 'R': malloc_realloc = 1; break;
	    default:
		wrtwarning("Unknown chars in MALLOC_OPTIONS\n");
		break;
	}
    }

    if (malloc_stats)
	atexit(malloc_exit);

#ifndef malloc_pagesize
    /* determine our pagesize */
    malloc_pagesize = getpagesize();
#endif /* malloc_pagesize */

#ifndef malloc_pageshift
    /* determine how much we shift by to get there */
    for (i = malloc_pagesize; i > 1; i >>= 1)
	malloc_pageshift++;
#endif /* malloc_pageshift */

#ifndef malloc_cache
    malloc_cache = 50 << malloc_pageshift;	
#endif /* malloc_cache */

#ifndef malloc_minsize
    /*
     * find the smallest size allocation we will bother about.
     * this is determined as the smallest allocation that can hold
     * it's own pginfo;
     */
    i = 2;
    for(;;) {
	int j;

	/* Figure out the size of the bits */
	j = malloc_pagesize/i;
	j /= 8;
	if (j < sizeof(u_long))
		j = sizeof (u_long);
	if (sizeof(struct pginfo) + j - sizeof (u_long) <= i)
		break;
	i += i;
    }
    malloc_minsize = i;
#endif /* malloc_minsize */


    /* Allocate one page for the page directory */
    page_dir = (struct pginfo **) map_pages(1,0);
    if (!page_dir)
	wrterror("fatal: my first mmap failed.  (check limits ?)\n");

    /*
     * We need a maximum of malloc_pageshift buckets, steal these from the
     * front of the page_directory;
     */
    malloc_origo = (u_long) page_dir >> malloc_pageshift;
    malloc_origo -= malloc_pageshift;

    /* Clear it */
    memset(page_dir,0,malloc_pagesize);

    /* Find out how much it tells us */
    malloc_ninfo = malloc_pagesize / sizeof *page_dir;

    /* Plug the page directory into itself */
    i = set_pgdir(page_dir,MALLOC_FIRST);
    if (!i)
	wrterror("fatal: couldn't set myself in the page directory\n");

    /* Been here, done that */
    initialized++;
}

/*
 * Allocate a number of complete pages
 */
void *
malloc_pages(size_t size)
{
    void *p,*delay_free = 0;
    int i;
    struct pgfree *pf;
    u_long index;

    /* How many pages ? */
    size += (malloc_pagesize-1);
    size &= ~malloc_pagemask;

    p = 0;
    /* Look for free pages before asking for more */
    for(pf = free_list.next; pf; pf = pf->next) {
#ifdef EXTRA_SANITY
	if (pf->page == pf->end)
	    wrterror("zero entry on free_list\n");
	if (pf->page > pf->end) {
	    TRACE(("%6d !s %p %p %p <%d>\n",malloc_event++,
		pf,pf->page,pf->end,__LINE__));
	    wrterror("sick entry on free_list\n");
	}
	if ((void*)pf->page >= sbrk(0))
	    wrterror("entry on free_list past brk\n");
	if (page_dir[((u_long)pf->page >> malloc_pageshift) - malloc_origo] 
	  != MALLOC_FREE) {
	    TRACE(("%6d !f %p %p %p <%d>\n",malloc_event++,
		pf,pf->page,pf->end,__LINE__));
	    wrterror("non-free first page on free-list\n");
	}
	if (page_dir[((u_long)pf->end >> malloc_pageshift) - 1 - malloc_origo] 
	  != MALLOC_FREE)
	    wrterror("non-free last page on free-list\n");
#endif /* EXTRA_SANITY */
	if (pf->size < size) 
	    continue;
	else if (pf->size == size) {
	    p = pf->page;
	    if (pf->next)
		    pf->next->prev = pf->prev;
	    pf->prev->next = pf->next;
	    delay_free = pf;
	    break;
	} else {
	    p = pf->page;
	    pf->page += size;
	    pf->size -= size;
	    break;
        }
    }
#ifdef EXTRA_SANITY
    if (p && page_dir[((u_long)p >> malloc_pageshift) - malloc_origo] 
      != MALLOC_FREE) {
	wrterror("allocated non-free page on free-list\n");
    }
#endif /* EXTRA_SANITY */

    size >>= malloc_pageshift;

    /* Map new pages */
    if (!p)
	p = map_pages(size,1);

    if (p) {
	/* Mark the pages in the directory */
	index = ((u_long)p >> malloc_pageshift) - malloc_origo;
	page_dir[index] = MALLOC_FIRST;
	for (i=1;i<size;i++)
	    page_dir[index+i] = MALLOC_FOLLOW;
    }
    if (delay_free) {
	if (!px) 
	    px = delay_free;
	else
	    free(delay_free);
    }
    return p;
}

/*
 * Allocate a page of fragments
 */

static __inline int
malloc_make_chunks(int bits)
{
    struct  pginfo *bp;
    void *pp;
    int i,k,l;

    /* Allocate a new bucket */
    pp = malloc_pages(malloc_pagesize);
    if (!pp)
	return 0;
    l = sizeof *bp - sizeof(u_long);
    l += sizeof(u_long) *
	(((malloc_pagesize >> bits)+MALLOC_BITS-1) / MALLOC_BITS);
    if ((1<<(bits)) <= l+l) {
	bp = (struct  pginfo *)pp;
    } else {
	bp = (struct  pginfo *)malloc(l);
    }
    if (!bp)
	return 0;
    bp->size = (1<<bits);
    bp->shift = bits;
    bp->total = bp->free = malloc_pagesize >> bits;
    bp->next = page_dir[bits];
    bp->page = pp;
    i = set_pgdir(pp,bp);
    if (!i)
	return 0;

    /* We can safely assume that there is nobody in this chain */
    page_dir[bits] = bp;

    /* set all valid bits in the bits */
    k = bp->total;
    i = 0;
/*
    for(;k-i >= MALLOC_BITS; i += MALLOC_BITS)
	bp->bits[i / MALLOC_BITS] = ~0;
*/
    for(; i < k; i++)
	set_bit(bp,i);

    if (bp != pp)
	return 1;

    /* We may have used the first ones already */
    for(i=0;l > 0;i++) {
	clr_bit(bp,i);
	bp->free--;
	bp->total--;
	l -= (1 << bits);
    }
    return 1;
}

/*
 * Allocate a fragment
 */
static void *
malloc_bytes(size_t size)
{
    int j;
    struct  pginfo *bp;
    int k;
    u_long *lp;

    /* Don't bother with anything less than this */
    if (size < malloc_minsize)
	size = malloc_minsize;

    /* Find the right bucket */
    j = fls((size)-1);

    /* If it's empty, make a page more of that size chunks */
    if (!page_dir[j] && !malloc_make_chunks(j))
	return 0;

    bp = page_dir[j];

    /* Find first word of bitmap which isn't empty */
    for (lp = bp->bits; !*lp; lp++)
	;

    /* Find that bit */
    k = ffs(*lp) - 1;
    *lp ^= 1<<k;
    bp->free--;
    if (!bp->free) {
	page_dir[j] = bp->next;
	bp->next = 0;
    }
    k += (lp-bp->bits)*MALLOC_BITS;
    return bp->page + (k << bp->shift);
}

/*
 * Allocate a piece of memory
 */
void *
malloc(size_t size)
{
    void *result;

    if (!initialized)
	malloc_init();

    if (suicide)
	abort();

    if (size <= malloc_maxsize)
	result =  malloc_bytes(size);
    else
	result =  malloc_pages(size);
    if (malloc_abort && !result)
	wrterror("malloc() returns NULL\n");
    TRACE(("%6d M %p %d\n",malloc_event++,result,size));
    return result;
}

/*
 * Change an allocation's size
 */
void *
realloc(void *ptr, size_t size)
{
    void *p;
    u_long osize,page,index;
    struct pginfo **mp;

    if (!initialized)
	malloc_init();

    if (suicide)
	abort();

    /* used as free() */
    if (ptr && !size) {
	free(ptr);
	return 0;
    }

    /* used as malloc() */
    if (!ptr)
	return malloc(size);

    /* Find the page directory entry for the page in question */
    page = (u_long)ptr >> malloc_pageshift;
    index = page - malloc_origo;

    /* make sure it makes sense in some fashion */
    if (index < malloc_pageshift || index > last_index) {
	wrtwarning("junk pointer passed to realloc()\n");
	return 0;
    }

    /* find the size of that allocation, and see if we need to relocate */
    mp = &page_dir[index];
    if (*mp == MALLOC_FIRST) {
	osize = malloc_pagesize;
	while (mp[1] == MALLOC_FOLLOW) {
	    osize += malloc_pagesize;
	    mp++;
	}
        if (!malloc_realloc && 
		size < osize && 
		size > malloc_maxsize &&
		size > (osize - malloc_pagesize))
	    return ptr;
    } else if (*mp >= MALLOC_MAGIC) {
	osize = (*mp)->size;
	if (!malloc_realloc &&
		size < osize && 
		(size > (*mp)->size/2 || (*mp)->size == malloc_minsize))
	    return ptr;
    } else {
	wrterror("realloc() of wrong page.\n");
    }

    /* try to reallocate */
    p = malloc(size);

    if (p) {
	/* copy the lesser of the two sizes */
	if (osize < size)
	    memcpy(p,ptr,osize);
	else
	    memcpy(p,ptr,size);
	free(ptr);
    } else if (malloc_abort)
	wrterror("realloc() returns NULL\n");
    return p;
}

/*
 * Free a sequence of pages
 */

static __inline void
free_pages(void *ptr,u_long page, int index, struct pginfo *info)
{
    int i;
    struct pgfree *pf,*pt;
    u_long l;
    void *tail;

#ifdef SANITY
    /* Is it free already ? */
    if (info == MALLOC_FREE) {
	wrtwarning("freeing free page.\n");
	return;
    }

    /* Is it not the right place to begin ? */
    if (info != MALLOC_FIRST)
	wrterror("freeing wrong page.\n");

    /* Is this really a pointer to a page ? */
    if ((u_long)ptr & malloc_pagemask)
	wrterror("freeing messed up page pointer.\n");
#endif

    /* Count how many pages it is anyway */
    page_dir[index] = MALLOC_FREE;
    for (i = 1; page_dir[index+i] == MALLOC_FOLLOW; i++)
	page_dir[index + i] = MALLOC_FREE;

    l = i << malloc_pageshift;

    tail = ptr+l;

    /* add to free-list */
    if (!px)
	px = malloc(sizeof *pt);
    /* XXX check success */
    px->page = ptr;
    px->end =  tail;
    px->size = l;
    if (!free_list.next) {
	px->next = free_list.next;
	px->prev = &free_list;
	free_list.next = px;
	pf = px;
	px = 0;
    } else {
	tail = ptr+l;
	for(pf = free_list.next; pf->next && pf->end < ptr; pf = pf->next)
	    ;
	for(; pf; pf = pf->next) {
	    if (pf->end == ptr ) {
		/* append to entry */
		pf->end += l;
		pf->size += l;
		if (pf->next && pf->end == pf->next->page ) {
		    pt = pf->next;
		    pf->end = pt->end;
		    pf->size += pt->size;
		    pf->next = pt->next;
		    if (pf->next)
			pf->next->prev = pf;
		    free(pt);
		}
	    } else if (pf->page == tail) {
		/* prepend to entry */
		pf->size += l;
		pf->page = ptr;
	    } else if (pf->page > ptr) {
		px->next = pf;
		px->prev = pf->prev;
		pf->prev = px;
		px->prev->next = px;
		pf = px;
		px = 0;
	    } else if (!pf->next) {
		px->next = 0;
		px->prev = pf;
		pf->next = px;
		pf = px;
		px = 0;
	    } else {
		continue;
	    }
	    break;
	}
    }
    if (!pf->next &&
      pf->size > malloc_cache &&
      pf->end == malloc_brk &&
      malloc_brk == sbrk(0)) {
	pf->end = pf->page + malloc_cache;
	pf->size = malloc_cache;
	TRACE(("%6d U %p %d\n",malloc_event++,pf->end,pf->end - pf->page));
	brk(pf->end);
	malloc_brk = pf->end;
	/* Find the page directory entry for the page in question */
	page = (u_long)pf->end >> malloc_pageshift;
	index = page - malloc_origo;
	/* Now update the directory */
	for(i=index;i <= last_index;)
	    page_dir[i++] = MALLOC_NOT_MINE;
	last_index = index - 1;
    }
}

/*
 * Free a chunk, and possibly the page it's on, if the page becomes empty.
 */

static __inline void
free_bytes(void *ptr,u_long page, int index, struct pginfo *info)
{
    int i;
    struct pginfo **mp;
    void *vp;

    /* Make sure that pointer is multiplum of chunk-size */
    if ((u_long)ptr & (info->size - 1))
	wrterror(" freeing messed up chunk pointer\n");

    /* Find the chunk number on the page */
    i = ((u_long)ptr & malloc_pagemask) >> info->shift;

#ifdef SANITY
    /* See if it's free already */
    if (tst_bit(info,i)) {
	wrtwarning("freeing free chunk.\n");
	return;
    }
#endif

    /* Mark it free */
    set_bit(info,i);
    info->free++;

    /* If the page was full before, we need to put it on the queue now */
    if (info->free == 1) {
	mp = page_dir + info->shift;
	while (*mp && (*mp)->next && (*mp)->next->page < info->page)
	    mp = &(*mp)->next;
	info->next = *mp;
	*mp = info;
	return;
    }

    /* If this page isn't empty, don't do anything. */
    if (info->free != info->total)
	return;

    /* We may want to keep at least one page of each size chunks around.  */
    mp = page_dir + info->shift;
    if (0 && (*mp == info) && !info->next)
	return;

    /* Find & remove this page in the queue */
    while (*mp != info) {
	mp = &((*mp)->next);
#ifdef EXTRA_SANITY
	if (!*mp) {
		TRACE(("%6d !q %p\n",malloc_event++,info));
		wrterror("Not on queue\n");
	}
#endif
    }
    *mp = info->next;

    /* Free the page & the info structure if need be */
    set_pgdir(info->page,MALLOC_FIRST);
    if((void*)info->page == (void*)info) {
	free(info->page);
    } else {
	vp = info->page;
	free(info);
	free(vp);
    }
}

void
free(void *ptr)
{
    u_long page;
    struct pginfo *info;
    int index;

    TRACE(("%6d F %p\n",malloc_event++,ptr));
    /* This is legal */
    if (!ptr)
	return;

#ifdef SANITY
    /* There wouldn't be anything to free */
    if (!initialized) {
	wrtwarning("free() called before malloc() ever got called\n");
	return;
    }
#endif

    if (suicide)
	abort();

    /* Find the page directory entry for the page in question */
    page = (u_long)ptr >> malloc_pageshift;
    index = page - malloc_origo;

#ifdef SANITY
    /* make sure it makes sense in some fashion */
    if (index < malloc_pageshift) {
	wrtwarning("junk pointer (low) passed to free()\n");
	return;
    }
    if (index > last_index) {
	wrtwarning("junk pointer (high) passed to free()\n");
	return;
    }
#endif

    /* handle as page-allocation or chunk allocation */
    info = page_dir[index];
    if (info < MALLOC_MAGIC)
        free_pages(ptr,page,index,info);
    else 
	free_bytes(ptr,page,index,info);
    return;
}
