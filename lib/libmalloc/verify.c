/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"

RCSID("$Id: verify.c,v 1.1 1994/03/06 22:59:57 nate Exp $")

/*
 *  Goes through the entire heap checking all pointers, tags for
 *  consistency. Should catch most casual heap corruption (overwriting
 *  the end of a malloc'ed chunk, etc..) Nonetheless, heap corrupters
 *  tend to be devious and ingenious in ways they corrupt heaps (Believe
 *  me, I know:-). We should probably do the same thing if DEBUG is not
 *  defined, but return 0 instead of aborting. If fullcheck is non-zero,
 *  it also checks that free blocks contain the magic pattern written
 *  into them when they were freed to make sure the program is not still
 *  trying to access those blocks.
 */
int
mal_verify(fullcheck)
int fullcheck;
{
#ifdef DEBUG
	REGISTER Word *ptr;
	REGISTER Word *blk;
	REGISTER Word *blkend;

	if (_malloc_loword == NULL) /* Nothing malloc'ed yet */
		return(0);
		
	if (_malloc_rover != NULL) {
		ASSERT(PTR_IN_HEAP(_malloc_rover),
		 "corrupt ROVER pointer found by mal_verify()");
		ASSERT(VALID_END_SIZE_FIELD(_malloc_rover),
		 "corrupt ROVER SIZE field found by mal_verify()");
		ASSERT(VALID_NEXT_PTR(_malloc_rover),
		 "corrupt ROVER NEXT pointer found by mal_verify()");
		ASSERT(VALID_PREV_PTR(_malloc_rover),
		 "corrupt ROVER PREV pointer found by mal_verify()");
	}
	for(ptr = _malloc_mem; ptr != NULL; ptr = ptr->next) {
		/*
		 *  Check arena bounds - not same as checking block tags,
		 *  despite similar appearance of the test
		 */
		ASSERT(SIZEFIELD(ptr+1) == SIZEFIELD(ptr + SIZE(ptr+1)),
		 "corrupt malloc arena found by mal_verify");
		blkend = ptr + SIZE(ptr + 1);
		for(blk = ptr + ARENASTART; blk < blkend; blk += SIZE(blk)) {
			ASSERT(PTR_IN_HEAP(blk), "corrupt pointer found by mal_verify()");
			ASSERT(VALID_START_SIZE_FIELD(blk),
			 "corrupt SIZE field found by mal_verify()");
			if (TAG(blk) == FREE) {
				ASSERT(VALID_NEXT_PTR(blk + FREESIZE(blk) - 1),
				 "corrupt NEXT pointer found by mal_verify()");
				ASSERT(VALID_PREV_PTR(blk + FREESIZE(blk) - 1),
				 "corrupt PREV pointer found by mal_verify()");
				if (fullcheck) {
					/* Make sure all free blocks are filled with FREEMAGIC */
					int i, n;
					char *cp;

					n = (SIZE(blk) - FREE_OVERHEAD) *
					 sizeof(Word);
					cp = (char *) (blk + FREEHEADERWORDS);
					for (i = 0; i < n; i++, cp++) {
						ASSERT(*cp == FREEMAGIC,
						 "corrupt free block found by mal_verify()");
					}
				}
			} else {
				ASSERT(VALID_MAGIC(blk),
				 "overwritten end of block found by mal_verify()");
			}
		}
	}
#endif /* DEBUG */
	return(0);
}
