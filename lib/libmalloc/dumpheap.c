/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"

RCSID("$Id: dumpheap.c,v 1.1 1994/03/06 22:59:30 nate Exp $")

/* 
 *  Same as malloc_verify except that it prints the heap as it goes
 *  along. Some would argue that the printout routine should not have
 *  the ASSERTS, and should print a corrupt heap as well.
 *  Unfortunately, if any of those ASSERTs is false, this routine could
 *  wander off into the sunset because of corrupt tags. I have relaxed
 *  the tests for free list pointers because this routine doesn't need
 *  them so it just whines
 */
void
mal_heapdump(fd)
FILE *fd;
{
	REGISTER Word *ptr;
	REGISTER Word *blk;
	REGISTER Word *blkend;
	char buf[512];	/* long enough for the sprintfs below */

	if (_malloc_loword == NULL) { /* Nothing malloc'ed yet */
		(void) fputs("Null heap - nothing malloc'ed yet\n", fd);
		return;
	}
		
	(void) fputs("Heap printout:\n", fd);
	(void) sprintf(buf, "Rover pointer is 0x%lx\n", (ulong) _malloc_rover);
	(void) fputs(buf, fd);
	if(!(_malloc_rover == NULL
	 || PTR_IN_HEAP(_malloc_rover)
	 || VALID_END_SIZE_FIELD(_malloc_rover)
	 || VALID_NEXT_PTR(_malloc_rover)
	 || VALID_PREV_PTR(_malloc_rover)))
		(void) fputs("corrupt Rover pointer\n", fd);
	for(ptr = _malloc_mem; ptr != NULL; ptr = ptr->next) {
		/* print the arena */
		(void) sprintf(buf, "Arena from 0x%lx to 0x%lx, %lu (0x%lx) words\n",
			       (ulong) ptr, (ulong) (ptr + SIZE(ptr+1)),
			       (ulong) SIZE(ptr+1)+1, (ulong) SIZE(ptr+1)+1);
		(void) fputs(buf, fd);
		(void) sprintf(buf, "Next arena is 0x%lx\n", (ulong)ptr->next);
		(void) fputs(buf, fd);
		(void) fflush(fd);
		ASSERT(SIZEFIELD(ptr+1) == SIZEFIELD(ptr + SIZE(ptr+1)),
		 "corrupt malloc arena");
		blkend = ptr + SIZE(ptr + 1);
		for(blk = ptr + ARENASTART; blk < blkend; blk += SIZE(blk)) {
			(void) sprintf(buf, "  %s blk: 0x%lx to 0x%lx, %lu (0x%lx) words",
				       TAG(blk) == FREE ? "Free" : "Allocated",
				       (ulong) blk, (ulong) (blk+SIZE(blk)-1),
				       (ulong) SIZE(blk), (ulong) SIZE(blk));
			(void) fputs(buf, fd);
			(void) fflush(fd);
 			ASSERT(PTR_IN_HEAP(blk), "corrupt pointer encountered");
			if (TAG(blk) == FREE) {
				int i, n;
				char *cp;

				(void) sprintf(buf, " next=0x%lx, prev=0x%lx\n",
					       (ulong) NEXT(blk + FREESIZE(blk) - 1),
					       (ulong) PREV(blk + FREESIZE(blk) - 1));
				(void) fputs(buf, fd);
				/* Make sure free block is filled with FREEMAGIC */
				n = (SIZE(blk) - FREE_OVERHEAD) *
				 sizeof(Word);
				cp = (char *) (blk + FREEHEADERWORDS);
#ifdef DEBUG
				for (i = 0; i < n; i++, cp++) {
					if (*cp != FREEMAGIC) {
						(void) fputs(
						 "  ** free block changed after being freed.\n", fd);
						break;
					}
				}
#endif
			} else {
#ifdef DEBUG
				(void) sprintf(buf, " really %lu bytes\n", (ulong) REALSIZE(blk));
				(void) fputs(buf, fd);
#else
				(void) fputs("\n", fd);
#endif
			}
			(void) fflush(fd);
			ASSERT(VALID_START_SIZE_FIELD(blk),
			 "corrupt SIZE field encountered in mal_dumpheap()");
			if (TAG(blk) == FREE) {
				if( ! VALID_NEXT_PTR(blk + FREESIZE(blk) - 1))
					(void) fputs("  ** bad next pointer\n", fd);
				if( ! VALID_PREV_PTR(blk + FREESIZE(blk) - 1))
					(void) fputs("  ** bad prev pointer\n", fd);
			} else {
				if ( ! VALID_MAGIC(blk))
					(void) fputs("  ** end of block overwritten\n", fd);
			}
		}
	}
	(void) fputs("==============\n", fd);
	(void) fflush(fd);
}
