/* dynamic memory allocation for GNU.
   Copyright (C) 1985, 1987 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

In other words, you are welcome to use, share and improve this program.
You are forbidden to forbid anyone else to use, share and improve
what you give them.   Help stamp out software-hoarding!  */


/*
 * @(#)nmalloc.c 1 (Caltech) 2/21/82
 *
 *	U of M Modified: 20 Jun 1983 ACT: strange hacks for Emacs
 *
 *	Nov 1983, Mike@BRL, Added support for 4.1C/4.2 BSD.
 *
 * This is a very fast storage allocator.  It allocates blocks of a small
 * number of different sizes, and keeps free lists of each size.  Blocks
 * that don't exactly fit are passed up to the next larger size.  In this
 * implementation, the available sizes are (2^n)-4 (or -16) bytes long.
 * This is designed for use in a program that uses vast quantities of
 * memory, but bombs when it runs out.  To make it a little better, it
 * warns the user when he starts to get near the end.
 *
 * June 84, ACT: modified rcheck code to check the range given to malloc,
 * rather than the range determined by the 2-power used.
 *
 * Jan 85, RMS: calls malloc_warning to issue warning on nearly full.
 * No longer Emacs-specific; can serve as all-purpose malloc for GNU.
 * You should call malloc_init to reinitialize after loading dumped Emacs.
 * Call malloc_stats to get info on memory stats if MSTATS turned on.
 * realloc knows how to return same block given, just changing its size,
 * if the power of 2 is correct.
 */

/*
 * nextf[i] is the pointer to the next free block of size 2^(i+3).  The
 * smallest allocatable block is 8 bytes.  The overhead information will
 * go in the first int of the block, and the returned pointer will point
 * to the second.
 *
#ifdef MSTATS
 * nmalloc[i] is the difference between the number of mallocs and frees
 * for a given block size.
#endif MSTATS
 */

#ifdef emacs
/* config.h specifies which kind of system this is.  */
#include "config.h"
#include <signal.h>
#else

/* Determine which kind of system this is.  */
#include <sys/types.h>
#include <signal.h>

#include <string.h>
#define bcopy(s,d,n)	memcpy ((d), (s), (n))
#define bcmp(s1,s2,n)	memcmp ((s1), (s2), (n))
#define bzero(s,n)	memset ((s), 0, (n))

#ifndef SIGTSTP
#ifndef VMS
#ifndef USG
#define USG
#endif
#endif /* not VMS */
#else /* SIGTSTP */
#ifdef SIGIO
#define BSD4_2
#endif /* SIGIO */
#endif /* SIGTSTP */

#endif /* not emacs */

/* Define getpagesize () if the system does not.  */
#include "getpagesize.h"

#ifdef BSD
#ifdef BSD4_1
#include <sys/vlimit.h>		/* warn the user when near the end */
#else /* if 4.2 or newer */
#include <sys/time.h>
#include <sys/resource.h>
#endif /* if 4.2 or newer */
#endif

#ifdef VMS
#include "vlimit.h"
#endif

extern char *start_of_data ();

#ifdef BSD
#ifndef DATA_SEG_BITS
#define start_of_data() &etext
#endif
#endif

#ifndef emacs
#define start_of_data() &etext
#endif

#define ISALLOC ((char) 0xf7)	/* magic byte that implies allocation */
#define ISFREE ((char) 0x54)	/* magic byte that implies free block */
				/* this is for error checking only */
#define ISMEMALIGN ((char) 0xd6)  /* Stored before the value returned by
				     memalign, with the rest of the word
				     being the distance to the true
				     beginning of the block.  */

extern char etext;

/* These two are for user programs to look at, when they are interested.  */

unsigned int malloc_sbrk_used;       /* amount of data space used now */
unsigned int malloc_sbrk_unused;     /* amount more we can have */

/* start of data space; can be changed by calling init_malloc */
static char *data_space_start;

#ifdef MSTATS
static int nmalloc[30];
static int nmal, nfre;
#endif /* MSTATS */

/* If range checking is not turned on, all we have is a flag indicating
   whether memory is allocated, an index in nextf[], and a size field; to
   realloc() memory we copy either size bytes or 1<<(index+3) bytes depending
   on whether the former can hold the exact size (given the value of
   'index').  If range checking is on, we always need to know how much space
   is allocated, so the 'size' field is never used. */

struct mhead {
	char     mh_alloc;	/* ISALLOC or ISFREE */
	char     mh_index;	/* index in nextf[] */
/* Remainder are valid only when block is allocated */
	unsigned short mh_size;	/* size, if < 0x10000 */
#ifdef rcheck
	unsigned mh_nbytes;	/* number of bytes allocated */
	int      mh_magic4;	/* should be == MAGIC4 */
#endif /* rcheck */
};

/* Access free-list pointer of a block.
  It is stored at block + 4.
  This is not a field in the mhead structure
  because we want sizeof (struct mhead)
  to describe the overhead for when the block is in use,
  and we do not want the free-list pointer to count in that.  */

#define CHAIN(a) \
  (*(struct mhead **) (sizeof (char *) + (char *) (a)))

#ifdef rcheck

/* To implement range checking, we write magic values in at the beginning and
   end of each allocated block, and make sure they are undisturbed whenever a
   free or a realloc occurs. */
/* Written in each of the 4 bytes following the block's real space */
#define MAGIC1 0x55
/* Written in the 4 bytes before the block's real space */
#define MAGIC4 0x55555555
#define ASSERT(p) if (!(p)) botch("p"); else
#define EXTRA  4		/* 4 bytes extra for MAGIC1s */
#else
#define ASSERT(p) if (!(p)) abort (); else
#define EXTRA  0
#endif /* rcheck */


/* nextf[i] is free list of blocks of size 2**(i + 3)  */

static struct mhead *nextf[30];

/* busy[i] is nonzero while allocation of block size i is in progress.  */

static char busy[30];

/* Number of bytes of writable memory we can expect to be able to get */
static unsigned int lim_data;

/* Level number of warnings already issued.
  0 -- no warnings issued.
  1 -- 75% warning already issued.
  2 -- 85% warning already issued.
*/
static int warnlevel;

/* Function to call to issue a warning;
   0 means don't issue them.  */
static void (*warnfunction) ();

/* nonzero once initial bunch of free blocks made */
static int gotpool;

char *_malloc_base;

static void getpool ();

/* Cause reinitialization based on job parameters;
  also declare where the end of pure storage is. */
void
malloc_init (start, warnfun)
     char *start;
     void (*warnfun) ();
{
  if (start)
    data_space_start = start;
  lim_data = 0;
  warnlevel = 0;
  warnfunction = warnfun;
}

/* Return the maximum size to which MEM can be realloc'd
   without actually requiring copying.  */

int
malloc_usable_size (mem)
     char *mem;
{
  struct mhead *p
    = (struct mhead *) (mem - ((sizeof (struct mhead) + 7) & ~7));
  int blocksize = 8 << p->mh_index;

  return blocksize - sizeof (struct mhead) - EXTRA;
}

static void
morecore (nu)			/* ask system for more memory */
     register int nu;		/* size index to get more of  */
{
  char *sbrk ();
  register char *cp;
  register int nblks;
  register unsigned int siz;
  int oldmask;

#ifdef BSD
#ifndef BSD4_1
  int newmask = -1;
  /* Blocking these signals interferes with debugging, at least on BSD on
     the HP 9000/300.  */
#ifdef SIGTRAP
  newmask &= ~(1 << SIGTRAP);
#endif
#ifdef SIGILL
  newmask &= ~(1 << SIGILL);
#endif
#ifdef SIGTSTP
  newmask &= ~(1 << SIGTSTP);
#endif
#ifdef SIGSTOP
  newmask &= ~(1 << SIGSTOP);
#endif
  oldmask = sigsetmask (newmask);
#endif
#endif

  if (!data_space_start)
    {
      data_space_start = start_of_data ();
    }

  if (lim_data == 0)
    get_lim_data ();

 /* On initial startup, get two blocks of each size up to 1k bytes */
  if (!gotpool)
    { getpool (); getpool (); gotpool = 1; }

  /* Find current end of memory and issue warning if getting near max */

#ifndef VMS
  /* Maximum virtual memory on VMS is difficult to calculate since it
   * depends on several dynmacially changing things. Also, alignment
   * isn't that important. That is why much of the code here is ifdef'ed
   * out for VMS systems.
   */
  cp = sbrk (0);
  siz = cp - data_space_start;

  if (warnfunction)
    switch (warnlevel)
      {
      case 0:
	if (siz > (lim_data / 4) * 3)
	  {
	    warnlevel++;
	    (*warnfunction) ("Warning: past 75% of memory limit");
	  }
	break;
      case 1:
	if (siz > (lim_data / 20) * 17)
	  {
	    warnlevel++;
	    (*warnfunction) ("Warning: past 85% of memory limit");
	  }
	break;
      case 2:
	if (siz > (lim_data / 20) * 19)
	  {
	    warnlevel++;
	    (*warnfunction) ("Warning: past 95% of memory limit");
	  }
	break;
      }

  if ((int) cp & 0x3ff)	/* land on 1K boundaries */
    sbrk (1024 - ((int) cp & 0x3ff));
#endif /* not VMS */

 /* Take at least 2k, and figure out how many blocks of the desired size
    we're about to get */
  nblks = 1;
  if ((siz = nu) < 8)
    nblks = 1 << ((siz = 8) - nu);

  if ((cp = sbrk (1 << (siz + 3))) == (char *) -1)
    {
#ifdef BSD
#ifndef BSD4_1
      sigsetmask (oldmask);
#endif
#endif
      return;			/* no more room! */
    }
  malloc_sbrk_used = siz;
  malloc_sbrk_unused = lim_data - siz;

#ifndef VMS
  if ((int) cp & 7)
    {		/* shouldn't happen, but just in case */
      cp = (char *) (((int) cp + 8) & ~7);
      nblks--;
    }
#endif /* not VMS */

 /* save new header and link the nblks blocks together */
  nextf[nu] = (struct mhead *) cp;
  siz = 1 << (nu + 3);
  while (1)
    {
      ((struct mhead *) cp) -> mh_alloc = ISFREE;
      ((struct mhead *) cp) -> mh_index = nu;
      if (--nblks <= 0) break;
      CHAIN ((struct mhead *) cp) = (struct mhead *) (cp + siz);
      cp += siz;
    }
  CHAIN ((struct mhead *) cp) = 0;

#ifdef BSD
#ifndef BSD4_1
  sigsetmask (oldmask);
#endif
#endif
}

static void
getpool ()
{
  register int nu;
  char * sbrk ();
  register char *cp = sbrk (0);

  if ((int) cp & 0x3ff)	/* land on 1K boundaries */
    sbrk (1024 - ((int) cp & 0x3ff));

  /* Record address of start of space allocated by malloc.  */
  if (_malloc_base == 0)
    _malloc_base = cp;

  /* Get 2k of storage */

  cp = sbrk (04000);
  if (cp == (char *) -1)
    return;

  /* Divide it into an initial 8-word block
     plus one block of size 2**nu for nu = 3 ... 10.  */

  CHAIN (cp) = nextf[0];
  nextf[0] = (struct mhead *) cp;
  ((struct mhead *) cp) -> mh_alloc = ISFREE;
  ((struct mhead *) cp) -> mh_index = 0;
  cp += 8;

  for (nu = 0; nu < 7; nu++)
    {
      CHAIN (cp) = nextf[nu];
      nextf[nu] = (struct mhead *) cp;
      ((struct mhead *) cp) -> mh_alloc = ISFREE;
      ((struct mhead *) cp) -> mh_index = nu;
      cp += 8 << nu;
    }
}

char *
malloc (n)		/* get a block */
     unsigned n;
{
  register struct mhead *p;
  register unsigned int nbytes;
  register int nunits = 0;

  /* Figure out how many bytes are required, rounding up to the nearest
     multiple of 8, then figure out which nestf[] area to use.
     Both the beginning of the header and the beginning of the
     block should be on an eight byte boundary.  */
  nbytes = (n + ((sizeof *p + 7) & ~7) + EXTRA + 7) & ~7;
  {
    register unsigned int   shiftr = (nbytes - 1) >> 2;

    while (shiftr >>= 1)
      nunits++;
  }

  /* In case this is reentrant use of malloc from signal handler,
     pick a block size that no other malloc level is currently
     trying to allocate.  That's the easiest harmless way not to
     interfere with the other level of execution.  */
  while (busy[nunits]) nunits++;
  busy[nunits] = 1;

  /* If there are no blocks of the appropriate size, go get some */
  /* COULD SPLIT UP A LARGER BLOCK HERE ... ACT */
  if (nextf[nunits] == 0)
    morecore (nunits);

  /* Get one block off the list, and set the new list head */
  if ((p = nextf[nunits]) == 0)
    {
      busy[nunits] = 0;
      return 0;
    }
  nextf[nunits] = CHAIN (p);
  busy[nunits] = 0;

  /* Check for free block clobbered */
  /* If not for this check, we would gobble a clobbered free chain ptr */
  /* and bomb out on the NEXT allocate of this size block */
  if (p -> mh_alloc != ISFREE || p -> mh_index != nunits)
#ifdef rcheck
    botch ("block on free list clobbered");
#else /* not rcheck */
    abort ();
#endif /* not rcheck */

  /* Fill in the info, and if range checking, set up the magic numbers */
  p -> mh_alloc = ISALLOC;
#ifdef rcheck
  p -> mh_nbytes = n;
  p -> mh_magic4 = MAGIC4;
  {
    /* Get the location n after the beginning of the user's space.  */
    register char *m = (char *) p + ((sizeof *p + 7) & ~7) + n;

    *m++ = MAGIC1, *m++ = MAGIC1, *m++ = MAGIC1, *m = MAGIC1;
  }
#else /* not rcheck */
  p -> mh_size = n;
#endif /* not rcheck */
#ifdef MSTATS
  nmalloc[nunits]++;
  nmal++;
#endif /* MSTATS */
  return (char *) p + ((sizeof *p + 7) & ~7);
}

free (mem)
     char *mem;
{
  register struct mhead *p;
  {
    register char *ap = mem;

    if (ap == 0)
      return;

    p = (struct mhead *) (ap - ((sizeof *p + 7) & ~7));
    if (p -> mh_alloc == ISMEMALIGN)
      {
	ap -= p->mh_size;
	p = (struct mhead *) (ap - ((sizeof *p + 7) & ~7));
      }

#ifndef rcheck
    if (p -> mh_alloc != ISALLOC)
      abort ();

#else rcheck
    if (p -> mh_alloc != ISALLOC)
      {
	if (p -> mh_alloc == ISFREE)
	  botch ("free: Called with already freed block argument\n");
	else
	  botch ("free: Called with bad argument\n");
      }

    ASSERT (p -> mh_magic4 == MAGIC4);
    ap += p -> mh_nbytes;
    ASSERT (*ap++ == MAGIC1); ASSERT (*ap++ == MAGIC1);
    ASSERT (*ap++ == MAGIC1); ASSERT (*ap   == MAGIC1);
#endif /* rcheck */
  }
  {
    register int nunits = p -> mh_index;

    ASSERT (nunits <= 29);
    p -> mh_alloc = ISFREE;

    /* Protect against signal handlers calling malloc.  */
    busy[nunits] = 1;
    /* Put this block on the free list.  */
    CHAIN (p) = nextf[nunits];
    nextf[nunits] = p;
    busy[nunits] = 0;

#ifdef MSTATS
    nmalloc[nunits]--;
    nfre++;
#endif /* MSTATS */
  }
}

char *
realloc (mem, n)
     char *mem;
     register unsigned n;
{
  register struct mhead *p;
  register unsigned int tocopy;
  register unsigned int nbytes;
  register int nunits;

  if (mem == 0)
    return malloc (n);
  p = (struct mhead *) (mem - ((sizeof *p + 7) & ~7));
  nunits = p -> mh_index;
  ASSERT (p -> mh_alloc == ISALLOC);
#ifdef rcheck
  ASSERT (p -> mh_magic4 == MAGIC4);
  {
    register char *m = mem + (tocopy = p -> mh_nbytes);
    ASSERT (*m++ == MAGIC1); ASSERT (*m++ == MAGIC1);
    ASSERT (*m++ == MAGIC1); ASSERT (*m   == MAGIC1);
  }
#else /* not rcheck */
  if (p -> mh_index >= 13)
    tocopy = (1 << (p -> mh_index + 3)) - ((sizeof *p + 7) & ~7);
  else
    tocopy = p -> mh_size;
#endif /* not rcheck */

  /* See if desired size rounds to same power of 2 as actual size. */
  nbytes = (n + ((sizeof *p + 7) & ~7) + EXTRA + 7) & ~7;

  /* If ok, use the same block, just marking its size as changed.  */
  if (nbytes > (4 << nunits) && nbytes <= (8 << nunits))
    {
#ifdef rcheck
      register char *m = mem + tocopy;
      *m++ = 0;  *m++ = 0;  *m++ = 0;  *m++ = 0;
      p-> mh_nbytes = n;
      m = mem + n;
      *m++ = MAGIC1;  *m++ = MAGIC1;  *m++ = MAGIC1;  *m++ = MAGIC1;
#else /* not rcheck */
      p -> mh_size = n;
#endif /* not rcheck */
      return mem;
    }

  if (n < tocopy)
    tocopy = n;
  {
    register char *new;

    if ((new = malloc (n)) == 0)
      return 0;
    bcopy (mem, new, tocopy);
    free (mem);
    return new;
  }
}

/* This is in case something linked with Emacs calls calloc.  */

char *
calloc (num, size)
     unsigned num, size;
{
  register char *mem;

  num *= size;
  mem = malloc (num);
  if (mem != 0)
    bzero (mem, num);
  return mem;
}

#ifndef VMS

char *
memalign (alignment, size)
     unsigned alignment, size;
{
  register char *ptr = malloc (size + alignment);
  register char *aligned;
  register struct mhead *p;

  if (ptr == 0)
    return 0;
  /* If entire block has the desired alignment, just accept it.  */
  if (((int) ptr & (alignment - 1)) == 0)
    return ptr;
  /* Otherwise, get address of byte in the block that has that alignment.  */
  aligned = (char *) (((int) ptr + alignment - 1) & -alignment);

  /* Store a suitable indication of how to free the block,
     so that free can find the true beginning of it.  */
  p = (struct mhead *) (aligned - ((7 + sizeof (struct mhead)) & ~7));
  p -> mh_size = aligned - ptr;
  p -> mh_alloc = ISMEMALIGN;
  return aligned;
}

#ifndef HPUX
/* This runs into trouble with getpagesize on HPUX.
   Patching out seems cleaner than the ugly fix needed.  */
char *
valloc (size)
{
  return memalign (getpagesize (), size);
}
#endif /* not HPUX */
#endif /* not VMS */

#ifdef MSTATS
/* Return statistics describing allocation of blocks of size 2**n. */

struct mstats_value
  {
    int blocksize;
    int nfree;
    int nused;
  };

struct mstats_value
malloc_stats (size)
     int size;
{
  struct mstats_value v;
  register int i;
  register struct mhead *p;

  v.nfree = 0;

  if (size < 0 || size >= 30)
    {
      v.blocksize = 0;
      v.nused = 0;
      return v;
    }

  v.blocksize = 1 << (size + 3);
  v.nused = nmalloc[size];

  for (p = nextf[size]; p; p = CHAIN (p))
    v.nfree++;

  return v;
}
int
malloc_mem_used ()
{
  int i;
  int size_used;

  size_used = 0;

  for (i = 0; i < 30; i++)
    {
      int allocation_size = 1 << (i + 3);
      struct mhead *p;

      size_used += nmalloc[i] * allocation_size;
    }

  return size_used;
}

int
malloc_mem_free ()
{
  int i;
  int size_unused;

  size_unused = 0;

  for (i = 0; i < 30; i++)
    {
      int allocation_size = 1 << (i + 3);
      struct mhead *p;

      for (p = nextf[i]; p ; p = CHAIN (p))
	size_unused += allocation_size;
    }

  return size_unused;
}
#endif /* MSTATS */

/*
 *	This function returns the total number of bytes that the process
 *	will be allowed to allocate via the sbrk(2) system call.  On
 *	BSD systems this is the total space allocatable to stack and
 *	data.  On USG systems this is the data space only.
 */

#ifdef USG

get_lim_data ()
{
  extern long ulimit ();

#ifdef ULIMIT_BREAK_VALUE
  lim_data = ULIMIT_BREAK_VALUE;
#else
  lim_data = ulimit (3, 0);
#endif

  lim_data -= (long) data_space_start;
}

#else /* not USG */
#if defined (BSD4_1) || defined (VMS)

get_lim_data ()
{
  lim_data = vlimit (LIM_DATA, -1);
}

#else /* not BSD4_1 and not VMS */

get_lim_data ()
{
  struct rlimit XXrlimit;

  getrlimit (RLIMIT_DATA, &XXrlimit);
#ifdef RLIM_INFINITY
  lim_data = XXrlimit.rlim_cur & RLIM_INFINITY; /* soft limit */
#else
  lim_data = XXrlimit.rlim_cur;	/* soft limit */
#endif
}

#endif /* not BSD4_1 and not VMS */
#endif /* not USG */

#ifdef VMS
/* There is a problem when dumping and restoring things on VMS. Calls
 * to SBRK don't necessarily result in contiguous allocation. Dumping
 * doesn't work when it isn't. Therefore, we make the initial
 * allocation contiguous by allocating a big chunk, and do SBRKs from
 * there. Once Emacs has dumped there is no reason to continue
 * contiguous allocation, malloc doesn't depend on it.
 *
 * There is a further problem of using brk and sbrk while using VMS C
 * run time library routines malloc, calloc, etc. The documentation
 * says that this is a no-no, although I'm not sure why this would be
 * a problem. In any case, we remove the necessity to call brk and
 * sbrk, by calling calloc (to assure zero filled data) rather than
 * sbrk.
 *
 * VMS_ALLOCATION_SIZE is the size of the allocation array. This
 * should be larger than the malloc size before dumping. Making this
 * too large will result in the startup procedure slowing down since
 * it will require more space and time to map it in.
 *
 * The value for VMS_ALLOCATION_SIZE in the following define was determined
 * by running emacs linked (and a large allocation) with the debugger and
 * looking to see how much storage was used. The allocation was 201 pages,
 * so I rounded it up to a power of two.
 */
#ifndef VMS_ALLOCATION_SIZE
#define VMS_ALLOCATION_SIZE	(512*256)
#endif

/* Use VMS RTL definitions */
#undef sbrk
#undef brk
#undef malloc
int vms_out_initial = 0;
char vms_initial_buffer[VMS_ALLOCATION_SIZE];
static char *vms_current_brk = &vms_initial_buffer;
static char *vms_end_brk = &vms_initial_buffer[VMS_ALLOCATION_SIZE-1];

#include <stdio.h>

char *
sys_sbrk (incr)
     int incr;
{
  char *sbrk(), *temp, *ptr;

  if (vms_out_initial)
    {
      /* out of initial allocation... */
      if (!(temp = malloc (incr)))
	temp = (char *) -1;
    }
  else
    {
      /* otherwise, go out of our area */
      ptr = vms_current_brk + incr; /* new current_brk */
      if (ptr <= vms_end_brk)
	{
	  temp = vms_current_brk;
	  vms_current_brk = ptr;
	}
      else
	{
	  vms_out_initial = 1;	/* mark as out of initial allocation */
	  if (!(temp = malloc (incr)))
	    temp = (char *) -1;
	}
    }
  return temp;
}
#endif /* VMS */
