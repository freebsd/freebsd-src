/* Emulate getpagesize on systems that lack it.  */

#ifndef HAVE_GETPAGESIZE

#ifdef VMS
#define getpagesize() 512
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _SC_PAGESIZE
#define getpagesize() sysconf(_SC_PAGESIZE)
#else

#include <sys/param.h>

#ifdef EXEC_PAGESIZE
#define getpagesize() EXEC_PAGESIZE
#else
#ifdef NBPG
#define getpagesize() NBPG * CLSIZE
#ifndef CLSIZE
#define CLSIZE 1
#endif /* no CLSIZE */
#else /* no NBPG */
#ifdef NBPC
#define getpagesize() NBPC
#endif /* NBPC */
#endif /* no NBPG */
#endif /* no EXEC_PAGESIZE */
#endif /* no _SC_PAGESIZE */

#endif /* not HAVE_GETPAGESIZE */
