/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"

RCSID("$Id: memalign.c,v 1.1 1994/03/06 22:59:49 nate Exp $")

/* 
 * !! memalign may leave small (< _malloc_minchunk) blocks as garbage.
 * Not worth fixing now -- I've only seen two applications call valloc()
 * or memalign(), and they do it only once in their life.
 */
/* 
 * This is needed to be compatible with Sun mallocs - Dunno how many
 * programs need it - the X server sure does... Returns a block 'size'
 * bytes long, such that the address is a multiple of 'alignment'.
 * (alignment MUST be a power of 2). This routine is possibly more
 * convoluted than free() - certainly uglier. Since it is rarely called
 * - possibly once in a program, it should be ok.  Since this is called
 * from valloc() which is usually needed in conjunction with
 * mmap()/munmap(), note the comment in the Sun manual page about
 * freeing segments of size 128K and greater. Ugh.
 */
univptr_t
memalign(alignment, size)
size_t alignment, size;
{
	univptr_t cp;
	univptr_t addr;
	REGISTER Word *p0, *p1;
	REGISTER size_t before, after;
	size_t blksize;
#ifdef DEBUG
	int tmp_debugging = _malloc_debugging;
#endif /* DEBUG */

	if (alignment < sizeof(int) || !is_power_of_2(alignment) ||size == 0) {
		errno = EINVAL;
		return(NULL);
	}
	if (alignment < sizeof(Word))
		return(malloc(size)); /* We guarantee this alignment anyway */
	/* 
	 *  Life starts to get complicated - need to get a block large
	 *  enough to hold a block 'size' long, starting on an 'alignment'
	 *  boundary
	 */
	if ((cp = malloc((size_t) (size + alignment - 1))) == NULL)
		return(NULL);
	addr = SIMPLEALIGN(cp, alignment);
	/* 
	 *  This is all we really need - can go back now, except that we
	 *  might be wasting 'alignment - 1' bytes, which can be large since
	 *  this junk is usually called to align with things like pagesize.
	 *  So we try to push any free space before 'addr' and after 'addr +
	 *  size' back on the free list by making the memaligned chunk
	 *  ('addr' to 'addr + size') a block, and then doing stuff with the
	 *  space left over - either making them free blocks or coelescing
	 *  them whichever way is simplest. This usually involves making
	 *  them look like allocated blocks and calling free() which has all
	 *  the code to deal with this, and should do it reasonably fast.
	 */
	p0 = (Word *) cp;
	p0 -= HEADERWORDS;
	/*
	 *  p0 now points to the word tag starting the block which we got
	 *  from malloc. This remains invariant from now on - p1 is our
	 *  temporary pointer
	 */
	p1 = (Word *) addr;
	p1 -= HEADERWORDS;
	blksize = (size + sizeof(Word) - 1) / sizeof(Word);
	before = p1 - p0;
	after = SIZE(p0) - ALLOC_OVERHEAD - blksize - before;
	/*
	 *  p1 now points to the word before addr - this is going to be the
	 *  start of the memaligned block
	 */
	if (after < _malloc_minchunk) {
		/*
		 * We merge the extra space after the memaligned block into it
		 * since that space isn't enough for a separate block. Note
		 * that if the block after the one malloc returned is free, we
		 * might be able to merge the space into that block even if it
		 * is too small - unfortunately, free() won't accept a block of
		 * this size, and I don't want to do that code here, so we'll
		 * just let it go to waste in the memaligned block. !! fix later, maybe
		 */
		blksize += after;
		after = 0;
	}
	/*
	 *  We mark the newly carved memaligned block p1 as alloced. addr is
	 *  (p1 + 1) which is the address we'll return
	 */
	SIZEFIELD(p1) = ALLOCMASK(blksize + ALLOC_OVERHEAD);
	SIZEFIELD(p1 + blksize + ALLOC_OVERHEAD - 1) = SIZEFIELD(p1);
	SET_REALSIZE(p1, size);
	if (after > 0) {
		/* We can now free the block after the memaligned block. */
		p1 += blksize + ALLOC_OVERHEAD; /* SIZE(p1) */
		/*
 		 * p1 now points to the space after the memaligned block. we
		 * fix the size, mark it alloced, and call free - the block
		 * after this may be free, which isn't simple to coalesce - let
		 * free() do it.
		 */
		SIZEFIELD(p1) = ALLOCMASK(after);
		SIZEFIELD(p1 + after - 1) = SIZEFIELD(p1);
		SET_REALSIZE(p1, (after - ALLOC_OVERHEAD) * sizeof(Word));
#ifdef DEBUG
		/* Full heap checking will break till we finish memalign */
		_malloc_debugging = 0;
#endif /* DEBUG */
		free((univptr_t) (p1 + HEADERWORDS));
	}
	if (addr != cp) {
		/*
		 *  If what's 'before' is large enough to be freed, add p0 to
		 *  free list after changing its size to just consist of the
		 *  space before the memaligned block, also setting the
		 *  alloced flag. Then call free() -- may merge with preceding
		 *  block. (block after it is the memaligned block)
		 */
		/* 
		 *  Else the space before the block is too small to form a
		 *  free block, and the preceding block isn't free, so we
		 *  aren't touching it. Theoretically, we could put it in
		 *  the preceding alloc'ed block, but there are painful
		 *  complications if this is the start of the arena. We
		 *  pass, but MUST mark it as allocated. This sort of garbage
		 *  can split up the arena -- fix later with special case maybe?!!
		 */
		p1 = p0;
		SIZEFIELD(p1) = ALLOCMASK(before);
		SIZEFIELD(p1 + before - 1) = SIZEFIELD(p1);
		SET_REALSIZE(p1, (before - ALLOC_OVERHEAD) * sizeof(Word));
		if (before >= _malloc_minchunk) {
			free(cp);
		}
	}
#ifdef DEBUG
	_malloc_debugging = tmp_debugging;
#endif /* DEBUG */
	return(addr);
}

/* Just following the Sun manual page here */
univptr_t
valloc(size)
size_t size;
{
	static size_t pagesz = 0;

	if (pagesz == 0)
		pagesz = (size_t) getpagesize();
	return(memalign(pagesz, size));
}
