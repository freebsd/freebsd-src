/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"

RCSID("$Id: getmem.c,v 1.1 1994/03/06 22:59:42 nate Exp $")

/* gets memory from the system via the sbrk() system call.  Most Un*xes */
univptr_t
_mal_sbrk(nbytes)
size_t nbytes;
{
	return sbrk((int) nbytes);
}

/*
 * gets memory from the system via mmaping a file.  This was written for SunOS
 * versions greater than 4.0.  The filename is specified by the environment
 * variable CSRIMALLOC_MMAPFILE or by the call to mal_mmapset().  Using this
 * instead of sbrk() has the advantage of bypassing the swap system, allowing
 * processes to run with huge heaps even on systems configured with small swap
 * space.
 */
static char *mmap_filename;

#ifdef HAVE_MMAP
/* Sun gets size_t wrong, and these follow, thanks to my #defines! */
#undef caddr_t
#undef size_t
#undef u_char
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>

univptr_t
_mal_mmap(nbytes)
size_t nbytes;
{
	static struct {
		int i_fd;
		caddr_t i_data;
		caddr_t i_end;
		size_t i_size;
		size_t i_alloced;
	} mmf;
	struct stat stbuf;

	if (mmf.i_data != NULL) {
		/* Already initialized & mmaped the file */
		univptr_t p = mmf.i_data + mmf.i_alloced;
		
		if ((char *) p + nbytes > mmf.i_end) {
			errno = ENOMEM;
			return (univptr_t) -1;
		}
		mmf.i_alloced += nbytes;
		return p;
	}

	/*
	 * This code is run the first time the function is called, it opens
	 * the file and mmaps the
	 */
	if (mmap_filename == NULL) {
		mmap_filename = getenv("CSRIMALLOC_MMAPFILE");
		if (mmap_filename == NULL) {
			errno = ENOMEM;
			return (univptr_t) -1;
		}
	}

	mmf.i_fd = open(mmap_filename, O_RDWR, 0666);
	if (mmf.i_fd < 0 || fstat(mmf.i_fd, &stbuf) < 0)
		return (univptr_t) -1;
	if (stbuf.st_size < nbytes) {
		errno = ENOMEM;
		return (univptr_t) -1;
	}
	mmf.i_size = stbuf.st_size;
	mmf.i_data = mmap((caddr_t) 0, mmf.i_size, PROT_READ|PROT_WRITE,
			  MAP_SHARED, mmf.i_fd, (off_t) 0);
	if (mmf.i_data == (caddr_t) -1)
		return (univptr_t) -1;
	mmf.i_end = mmf.i_data + mmf.i_size;
	mmf.i_alloced = nbytes;
	/* Advise vm system of random access pattern */
	(void) madvise(mmf.i_data, mmf.i_size, MADV_RANDOM);
	return mmf.i_data;
}
#else /* !HAVE_MMAP */
univptr_t
_mal_mmap(nbytes)
size_t nbytes;
{
	return (univptr_t) -1;
}
#endif /* HAVE_MMAP */

void
mal_mmap(fname)
char *fname;
{
	_malloc_memfunc = _mal_mmap;
	mmap_filename = fname;
}
