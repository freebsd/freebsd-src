/* Declarations for `mmalloc' and friends.
   Copyright 1990, 1991, 1992 Free Software Foundation

   Written May 1989 by Mike Haertel.
   Heavily modified Mar 1992 by Fred Fish. (fnf@cygnus.com)

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */


#ifndef __MMALLOC_H
#define __MMALLOC_H 1

#ifdef	__STDC__
#  include <stddef.h>
#  define PTR void *
#  define CONST const
#  define PARAMS(paramlist) paramlist
#  include <limits.h>
#  ifndef NULL
#    define NULL (void *) 0
#  endif
#else
#  define PTR char *
#  define CONST /* nothing */
#  define PARAMS(paramlist) ()
#  ifndef size_t
#    define size_t unsigned int
#  endif
#  ifndef CHAR_BIT
#    define CHAR_BIT 8
#  endif
#  ifndef NULL
#    define NULL 0
#  endif
#endif

#ifndef MIN
#  define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

#define MMALLOC_MAGIC		"mmalloc"	/* Mapped file magic number */
#define MMALLOC_MAGIC_SIZE	8		/* Size of magic number buf */
#define MMALLOC_VERSION		1		/* Current mmalloc version */
#define MMALLOC_KEYS		16		/* Keys for application use */

/* The allocator divides the heap into blocks of fixed size; large
   requests receive one or more whole blocks, and small requests
   receive a fragment of a block.  Fragment sizes are powers of two,
   and all fragments of a block are the same size.  When all the
   fragments in a block have been freed, the block itself is freed.  */

#define INT_BIT		(CHAR_BIT * sizeof(int))
#define BLOCKLOG	(INT_BIT > 16 ? 12 : 9)
#define BLOCKSIZE	((unsigned int) 1 << BLOCKLOG)
#define BLOCKIFY(SIZE)	(((SIZE) + BLOCKSIZE - 1) / BLOCKSIZE)

/* The difference between two pointers is a signed int.  On machines where
   the data addresses have the high bit set, we need to ensure that the
   difference becomes an unsigned int when we are using the address as an
   integral value.  In addition, when using with the '%' operator, the
   sign of the result is machine dependent for negative values, so force
   it to be treated as an unsigned int. */

#define ADDR2UINT(addr)	((unsigned int) ((char *) (addr) - (char *) NULL))
#define RESIDUAL(addr,bsize) ((unsigned int) (ADDR2UINT (addr) % (bsize)))

/* Determine the amount of memory spanned by the initial heap table
   (not an absolute limit).  */

#define HEAP		(INT_BIT > 16 ? 4194304 : 65536)

/* Number of contiguous free blocks allowed to build up at the end of
   memory before they will be returned to the system.  */

#define FINAL_FREE_BLOCKS	8

/* Where to start searching the free list when looking for new memory.
   The two possible values are 0 and heapindex.  Starting at 0 seems
   to reduce total memory usage, while starting at heapindex seems to
   run faster.  */

#define MALLOC_SEARCH_START	mdp -> heapindex

/* Address to block number and vice versa.  */

#define BLOCK(A) (((char *) (A) - mdp -> heapbase) / BLOCKSIZE + 1)

#define ADDRESS(B) ((PTR) (((B) - 1) * BLOCKSIZE + mdp -> heapbase))

/* Data structure giving per-block information.  */

typedef union
  {
    /* Heap information for a busy block.  */
    struct
      {
	/* Zero for a large block, or positive giving the
	   logarithm to the base two of the fragment size.  */
	int type;
	union
	  {
	    struct
	      {
		size_t nfree;	/* Free fragments in a fragmented block.  */
		size_t first;	/* First free fragment of the block.  */
	      } frag;
	    /* Size (in blocks) of a large cluster.  */
	    size_t size;
	  } info;
      } busy;
    /* Heap information for a free block (that may be the first of
       a free cluster).  */
    struct
      {
	size_t size;		/* Size (in blocks) of a free cluster.  */
	size_t next;		/* Index of next free cluster.  */
	size_t prev;		/* Index of previous free cluster.  */
      } free;
  } malloc_info;

/* List of blocks allocated with `mmemalign' (or `mvalloc').  */

struct alignlist
  {
    struct alignlist *next;
    PTR aligned;		/* The address that mmemaligned returned.  */
    PTR exact;			/* The address that malloc returned.  */
  };

/* Doubly linked lists of free fragments.  */

struct list
  {
    struct list *next;
    struct list *prev;
  };

/* Statistics available to the user.
   FIXME:  By design, the internals of the malloc package are no longer
   exported to the user via an include file, so access to this data needs
   to be via some other mechanism, such as mmstat_<something> where the
   return value is the <something> the user is interested in. */

struct mstats
  {
    size_t bytes_total;		/* Total size of the heap. */
    size_t chunks_used;		/* Chunks allocated by the user. */
    size_t bytes_used;		/* Byte total of user-allocated chunks. */
    size_t chunks_free;		/* Chunks in the free list. */
    size_t bytes_free;		/* Byte total of chunks in the free list. */
  };

/* Internal structure that defines the format of the malloc-descriptor.
   This gets written to the base address of the region that mmalloc is
   managing, and thus also becomes the file header for the mapped file,
   if such a file exists. */

struct mdesc
{
  /* The "magic number" for an mmalloc file. */

  char magic[MMALLOC_MAGIC_SIZE];

  /* The size in bytes of this structure, used as a sanity check when reusing
     a previously created mapped file. */

  unsigned int headersize;

  /* The version number of the mmalloc package that created this file. */

  unsigned char version;

  /* Some flag bits to keep track of various internal things. */

  unsigned int flags;

  /* If a system call made by the mmalloc package fails, the errno is
     preserved for future examination. */

  int saved_errno;

  /* Pointer to the function that is used to get more core, or return core
     to the system, for requests using this malloc descriptor.  For memory
     mapped regions, this is the mmap() based routine.  There may also be
     a single malloc descriptor that points to an sbrk() based routine
     for systems without mmap() or for applications that call the mmalloc()
     package with a NULL malloc descriptor.

     FIXME:  For mapped regions shared by more than one process, this
     needs to be maintained on a per-process basis. */

  PTR (*morecore) PARAMS ((struct mdesc *, int));
     
  /* Pointer to the function that causes an abort when the memory checking
     features are activated.  By default this is set to abort(), but can
     be set to another function by the application using mmalloc().

     FIXME:  For mapped regions shared by more than one process, this
     needs to be maintained on a per-process basis. */

  void (*abortfunc) PARAMS ((void));

  /* Debugging hook for free.

     FIXME:  For mapped regions shared by more than one process, this
     needs to be maintained on a per-process basis. */

  void (*mfree_hook) PARAMS ((PTR, PTR));

  /* Debugging hook for `malloc'.

     FIXME:  For mapped regions shared by more than one process, this
     needs to be maintained on a per-process basis. */

  PTR (*mmalloc_hook) PARAMS ((PTR, size_t));

  /* Debugging hook for realloc.

     FIXME:  For mapped regions shared by more than one process, this
     needs to be maintained on a per-process basis. */

  PTR (*mrealloc_hook) PARAMS ((PTR, PTR, size_t));

  /* Number of info entries.  */

  size_t heapsize;

  /* Pointer to first block of the heap (base of the first block).  */

  char *heapbase;

  /* Current search index for the heap table.  */
  /* Search index in the info table.  */

  size_t heapindex;

  /* Limit of valid info table indices.  */

  size_t heaplimit;

  /* Block information table.
     Allocated with malign/__mmalloc_free (not mmalloc/mfree).  */
  /* Table indexed by block number giving per-block information.  */

  malloc_info *heapinfo;

  /* Instrumentation.  */

  struct mstats heapstats;

  /* Free list headers for each fragment size.  */
  /* Free lists for each fragment size.  */

  struct list fraghead[BLOCKLOG];

  /* List of blocks allocated by memalign.  */

  struct alignlist *aligned_blocks;

  /* The base address of the memory region for this malloc heap.  This
     is the location where the bookkeeping data for mmap and for malloc
     begins. */

  char *base;

  /* The current location in the memory region for this malloc heap which
     represents the end of memory in use. */

  char *breakval;

  /* The end of the current memory region for this malloc heap.  This is
     the first location past the end of mapped memory. */

  char *top;

  /* Open file descriptor for the file to which this malloc heap is mapped.
     This will always be a valid file descriptor, since /dev/zero is used
     by default if no open file is supplied by the client.  Also note that
     it may change each time the region is mapped and unmapped. */

  int fd;

  /* An array of keys to data within the mapped region, for use by the
     application.  */

  PTR keys[MMALLOC_KEYS];

};

/* Bits to look at in the malloc descriptor flags word */

#define MMALLOC_DEVZERO		(1 << 0)	/* Have mapped to /dev/zero */
#define MMALLOC_INITIALIZED	(1 << 1)	/* Initialized mmalloc */
#define MMALLOC_MMCHECK_USED	(1 << 2)	/* mmcheck() called already */

/* Allocate SIZE bytes of memory.  */

extern PTR mmalloc PARAMS ((PTR, size_t));

/* Re-allocate the previously allocated block in PTR, making the new block
   SIZE bytes long.  */

extern PTR mrealloc PARAMS ((PTR, PTR, size_t));

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */

extern PTR mcalloc PARAMS ((PTR, size_t, size_t));

/* Free a block allocated by `mmalloc', `mrealloc' or `mcalloc'.  */

extern void mfree PARAMS ((PTR, PTR));

/* Allocate SIZE bytes allocated to ALIGNMENT bytes.  */

extern PTR mmemalign PARAMS ((PTR, size_t, size_t));

/* Allocate SIZE bytes on a page boundary.  */

extern PTR mvalloc PARAMS ((PTR, size_t));

/* Activate a standard collection of debugging hooks.  */

extern int mmcheck PARAMS ((PTR, void (*) (void)));

/* Pick up the current statistics. (see FIXME elsewhere) */

extern struct mstats mmstats PARAMS ((PTR));

/* Internal version of `mfree' used in `morecore'. */

extern void __mmalloc_free PARAMS ((struct mdesc *, PTR));

/* Hooks for debugging versions.  */

extern void (*__mfree_hook) PARAMS ((PTR, PTR));
extern PTR (*__mmalloc_hook) PARAMS ((PTR, size_t));
extern PTR (*__mrealloc_hook) PARAMS ((PTR, PTR, size_t));

/* A default malloc descriptor for the single sbrk() managed region. */

extern struct mdesc *__mmalloc_default_mdp;

/* Initialize the first use of the default malloc descriptor, which uses
   an sbrk() region. */

extern struct mdesc *__mmalloc_sbrk_init PARAMS ((void));

/* Grow or shrink a contiguous mapped region using mmap().
   Works much like sbrk() */

#if defined(HAVE_MMAP)

extern PTR __mmalloc_mmap_morecore PARAMS ((struct mdesc *, int));

#endif

/* Remap a mmalloc region that was previously mapped. */

extern PTR __mmalloc_remap_core PARAMS ((struct mdesc *));

/* Macro to convert from a user supplied malloc descriptor to pointer to the
   internal malloc descriptor.  If the user supplied descriptor is NULL, then
   use the default internal version, initializing it if necessary.  Otherwise
   just cast the user supplied version (which is void *) to the proper type
   (struct mdesc *). */

#define MD_TO_MDP(md) \
  ((md) == NULL \
   ? (__mmalloc_default_mdp == NULL \
      ? __mmalloc_sbrk_init () \
      : __mmalloc_default_mdp) \
   : (struct mdesc *) (md))

#endif  /* __MMALLOC_H */
