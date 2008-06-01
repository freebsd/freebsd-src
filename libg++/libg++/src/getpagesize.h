#if defined(BSD) || defined(DGUX)
#ifndef BSD4_1
#define HAVE_GETPAGESIZE
#endif
#endif
 
#ifndef HAVE_GETPAGESIZE
 
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

#endif /* not HAVE_GETPAGESIZE */

