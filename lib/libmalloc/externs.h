#ifndef EXTERNS_H__
#define EXTERNS_H__

/* Lots of ugliness as we cope with non-standardness */

#ifdef STDHEADERS
  /* if we have properly prototyped standard headers, use them */
# include <stdlib.h>
# include <stddef.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

#else /* ! STDHEADERS */

/* # include <sys/types.h> */
#define caddr_t char *

/* 
 *  Malloc definitions from General Utilities <stdlib.h>. Note that we
 *  disagree with Berkeley Unix on the return type of free/cfree.
 */
extern univptr_t malloc proto((size_t));
extern univptr_t calloc proto((size_t, size_t));
extern univptr_t realloc proto((univptr_t, size_t));
extern void free proto((univptr_t));

/* General Utilities <stdlib.h> */

extern void	abort proto((void));
extern void	exit proto((int));
extern char	*getenv proto((const char *));

/*
 *  Input/Output <stdio.h> Note we disagree with Berkeley Unix on
 *  sprintf().
 */

#if 0	/* can't win with this one */
extern int sprintf proto((char *, const char *, ...));
#endif

extern int fputs proto((const char *, FILE *));
extern int fflush proto((FILE *));
extern int setvbuf proto((FILE *, char *, int, memsize_t));

/* Character Handling: <string.h> */

extern univptr_t memset proto((univptr_t, int, memsize_t));

#ifndef __GNUC__	/* clash with builtin, garn */
extern univptr_t memcpy proto((univptr_t, const univptr_t, memsize_t));
#endif

extern char *strcpy proto((char *, const char *));
extern memsize_t strlen proto((const char *));

/* UNIX -- unistd.h */
extern int write proto((int /*fd*/, const char * /*buf*/, int /*nbytes*/));
extern int open proto((const char */*path*/, int /*flags*/, ...));

#endif /* STDHEADERS */

#ifdef _SC_PAGESIZE	/* Solaris 2.x, SVR4? */
# define getpagesize()		sysconf(_SC_PAGESIZE)
#else /* ! _SC_PAGESIZE */
# ifdef _SC_PAGE_SIZE	/* HP, IBM */
#  define getpagesize()	sysconf(_SC_PAGE_SIZE)
# else /* ! _SC_PAGE_SIZE */
#  ifndef getpagesize
    extern int getpagesize proto((void));
#  endif /* getpagesize */
# endif /* _SC_PAGE_SIZE */
#endif /* _SC_PAGESIZE */

extern caddr_t sbrk proto((int));

/* Backwards compatibility with BSD/Sun -- these are going to vanish one day */
extern univptr_t valloc proto((size_t));
extern univptr_t memalign proto((size_t, size_t));
extern void cfree proto((univptr_t));

/* Malloc definitions - my additions.  Yuk, should use malloc.h properly!!  */
extern univptr_t emalloc proto((size_t));
extern univptr_t ecalloc proto((size_t, size_t));
extern univptr_t erealloc proto((univptr_t, size_t));
extern char *strdup proto((const char *));
extern char *strsave proto((const char *));
extern void mal_debug proto((int));
extern void mal_dumpleaktrace proto((FILE *));
extern void mal_heapdump proto((FILE *));
extern void mal_leaktrace proto((int));
extern void mal_mmap proto((char *));
extern void mal_sbrkset proto((int));
extern void mal_slopset proto((int));
extern void mal_statsdump proto((FILE *));
extern void mal_setstatsfile proto((FILE *));
extern void mal_trace proto((int));
extern int mal_verify proto((int));

/* Internal definitions */
extern int __nothing proto((void));
extern univptr_t _mal_sbrk proto((size_t));
extern univptr_t _mal_mmap proto((size_t));

#ifdef HAVE_MMAP
extern int madvise proto((caddr_t, size_t, int));
#ifndef __FreeBSD__
extern caddr_t mmap proto((caddr_t, size_t, int, int, int, off_t));
#endif /* __FreeBSD__ */
#endif /* HAVE_MMAP */

#endif /* EXTERNS_H__ */ /* Do not add anything after this line */
