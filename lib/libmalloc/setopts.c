/* Set various malloc options */
/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"

RCSID("$Id: setopts.c,v 1.1 1994/03/06 22:59:50 nate Exp $")

/* 
 *  Sets debugging level - level 0 and 1 both perform normal checking -
 *  making sure a pointer is valid before it is used for any heap data,
 *  and doing consistency checking on any block it touches while it
 *  works. Level 2 asks for a mal_verify() on every malloc(), free() or
 *  realloc(), thus checking the entire heap's pointers for consistency.
 *  Level 3 makes mal_verify() check that all free blocks contain a
 *  magic pattern that is put into a free block when it is freed.
 */
void
mal_debug(level)
int level;
{
#ifdef DEBUG
	if (level < 0 || level > 3) {
		return;
	}
	_malloc_debugging = level;
#endif /* DEBUG */
}

/*
 *  Allows you to control the number of system calls made, which might
 *  be helpful in a program allocating a lot of memory - call this once
 *  to set a number big enough to contain all the allocations. Or for
 *  very little allocation, so that you don't get a huge space just
 *  because you alloc'e a couple of strings
 */
void
mal_sbrkset(n)
int n;
{
	if (n < _malloc_minchunk * sizeof(Word)) {
		/* sbrk'ing anything less than a Word isn't a great idea.*/
		return;
	}

	_malloc_sbrkunits = (n + sizeof(Word) - 1) / sizeof(Word);
	return;
}

/* 
 *  Since the minimum size block allocated is sizeof(Word)*_malloc_minchunk,
 *  adjusting _malloc_minchunk is one way to control
 *  memory fragmentation, and if you do a lot of mallocs and frees of
 *  objects that have a similar size, then a good way to speed things up
 *  is to set _malloc_minchunk such that the minimum size block covers
 *  most of the objects you allocate
 */
void
mal_slopset(n)
int n;
{
	if (n < 0) {
		return;
	}

	_malloc_minchunk = (n + sizeof(Word) - 1) / sizeof(Word) + FIXEDOVERHEAD;
	return;
}

/*
 *  Sets the file used for verbose statistics to 'fd'. Does no
 *  verification whatsoever on the file descriptor
 */
void
mal_setstatsfile(fd)
FILE * fd;
{
	_malloc_statsfile = fd;
	/*
	 *  This file descriptor had better not have been written to before
	 *  this
	 */
	(void) setvbuf(fd, (char *) 0, _IONBF, 0);
}

/*
 *  Turns tracing on (if value != 0) or off, (if value == 0)
 */
void
mal_trace(value)
int value;
{
	if (value) {
		/* Try to unbuffer the trace file */
		(void) setvbuf(_malloc_statsfile, (char *) 0, _IONBF, 0);
		/* 
		 *  Write something to the stats file so stdio can initialize
		 *  its buffers i.e. call malloc() at least once while tracing
		 *  is off, if the unbuffering failed.
		 */
		(void) fputs("Malloc tracing starting\n", _malloc_statsfile);
		_malloc_tracing = 1;
		if (_malloc_loword != NULL) {
			/*
			 * malloc happened before tracing turned on, so make
			 * sure we print the heap start for xmem analysis.
			 */
			PRTRACE(sprintf(_malloc_statsbuf, "heapstart 0x%lx\n",
					(ulong) _malloc_loword));
		}
	} else {
		/* For symmetry */
		(void) fputs("Malloc tracing stopped\n", _malloc_statsfile);
		_malloc_tracing = 0;
	}
	(void) fflush(_malloc_statsfile);
}

