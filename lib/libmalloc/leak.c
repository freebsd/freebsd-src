/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"
#include "sptree.h"

RCSID("$Id: leak.c,v 1.1 1994/03/06 22:59:46 nate Exp $")

/* 
 *  These routines provide an interface for tracing memory leaks. The
 *  user can turn on leak tracing at any time by calling
 *  mal_leaktrace(1), after which every block allocated by
 *  _malloc()/_calloc()/_realloc()/_valloc()/_memalign() has a string
 *  (containing the filename and linenumber of the routine invoking it)
 *  stored in a database. When _free()/_cfree() is called on that block,
 *  the record is deleted from the database. The user can call
 *  mal_dumpleaktrace() to show the list of blocks allocated, and
 *  where they were allocated. The location of leaks can usually be
 *  detected from this.
 */
/*
 *  The tree implementation used to store the blocks is a splay-tree,
 *  using an implementation in C by Dave Brower (daveb@rtech.uucp),
 *  translated from Douglas Jones' original Pascal. However, any data
 *  structure that permits insert(), delete() and traverse()/apply() of
 *  key, value pairs should be suitable. Only this file needs to be
 *  changed.
 */
static SPTREE *sp = NULL;

/*
 *  num is a sequence number, incremented for ever block. min_num gets
 *  set to num after every dumpleaktrace - subsequent dumps do not print
 *  any blocks with sequence numbers less than min_num
 */
static unsigned long min_num = 0;
static unsigned long num = 0;

/*
 * These are used by mal_contents to count number of allocated blocks and the
 * number of bytes allocated.  Better way to do this is to walk the heap
 * rather than scan the splay tree.
 */
static unsigned long nmallocs;
static unsigned long nbytes;

static FILE *dumpfd = NULL;

/* 
 *  Turns recording of FILE and LINE number of each call to
 *  malloc/free/realloc/calloc/cfree/memalign/valloc on (if value != 0)
 *  or off, (if value == 0)
 */
void
mal_leaktrace(value)
int value;
{
	_malloc_leaktrace = (value != 0);
	if (sp == NULL)
		sp = __spinit();
}

/*
 *  The routine which actually does the printing. I know it is silly to
 *  print address in decimal, but sort doesn't read hex, so sorting the
 *  printed data by address is impossible otherwise. Urr. The format is
 *		FILE:LINE: sequence_number address_in_decimal (address_in_hex)
 */
void
__m_prnode(spblk)
SPBLK *spblk;
{
	if ((unsigned long) spblk->datb < min_num)
		return;
	(void) sprintf(_malloc_statsbuf, "%s%8lu %8lu(0x%08lx)\n",
		       (char *) spblk->data, (unsigned long) spblk->datb,
		       (unsigned long) spblk->key, (unsigned long) spblk->key);
	(void) fputs(_malloc_statsbuf, dumpfd);
}

/*
 *  Dumps all blocks which have been recorded.
 */
void
mal_dumpleaktrace(fd)
FILE *fd;
{
	dumpfd = fd;
	__spscan(__m_prnode, (SPBLK *) NULL, sp);
	(void) fflush(dumpfd);
	min_num = num;
}

/*
 *  Inserts a copy of a string keyed by the address addr into the tree
 *  that stores the leak trace information. The string is presumably of
 *  the form "file:linenumber:". It also stores a sequence number that
 *  gets incremented with each call to this routine.
 */
void
__m_install_record(addr, s)
univptr_t addr;
const char *s;
{
	num++;
	(void) __spadd(addr, strsave(s), (char *) num, sp);
}

/* Deletes the record keyed by addr if it exists */
void
__m_delete_record(addr)
univptr_t addr;
{
	SPBLK *result;

	if ((result = __splookup(addr, sp)) != NULL) {
		free(result->data);
		result->data = 0;
		__spdelete(result, sp);
	}
}

void
__m_count(spblk)
SPBLK *spblk;
{
	Word *p;
	
	nmallocs++;
	p = (Word *) spblk->key;
	p -= HEADERWORDS;

	/* A little paranoia... */
	ASSERT(PTR_IN_HEAP(p), "bad pointer seen in __m_count");
	ASSERT(TAG(p) != FREE, "freed block seen in __m_count");
	ASSERT(VALID_START_SIZE_FIELD(p), "corrupt block seen in __m_count");
	ASSERT(VALID_MAGIC(p), "block with end overwritten seen in __m_count");

	nbytes += SIZE(p) * sizeof(Word);
	return;
}

void
mal_contents(fp)
FILE *fp;
{
	void __m_count proto((SPBLK *));

	nmallocs = 0;
	nbytes = 0;
	__spscan(__m_count, (SPBLK *) NULL, sp);
	(void) sprintf(_malloc_statsbuf,
		       "%% %lu bytes %lu mallocs %lu available %lu vm\n",
		       nbytes, nmallocs, (ulong) _malloc_totalavail,
		       (ulong) sbrk(0));
	(void) fputs(_malloc_statsbuf, fp);
	(void) fflush(fp);
}
