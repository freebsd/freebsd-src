#include <sys/types.h>
#include <sys/param.h>	/* This defines BSD */
#if defined(BSD) && !defined(BSD4_4) && !defined(__osf__)
# include <stdio.h>
# include <strings.h>
# define strchr(a, b)		index((a), (b))
# define strrchr(a, b)		rindex((a), (b))
# define memcpy(a, b, c)	bcopy((b), (a), (c))
# define memzero(a, b)		bzero((a), (b))
# define memcmp(a, b, c)	bcmp((a), (b), (c))
#if defined(NeXT)
  typedef void sigret_t;
#else
  typedef int sigret_t;
#endif

/* system routines that don't return int */
char *getenv();
caddr_t malloc();

#else 
# include <stdio.h>
# define setbuffer(f, b, s)	setvbuf((f), (b), (b) ? _IOFBF : _IONBF, (s))
# include <string.h>
# include <memory.h>
# include <stdlib.h>
# define memzero(a, b)		memset((a), 0, (b))
  typedef void sigret_t;
#endif

/* some systems declare sys_errlist in stdio.h! */
#if defined(__NetBSD__) || defined(__FreeBSD__)
#if !defined(__m68k__)
#  if !defined(__NetBSD132__)
#define SYS_ERRLIST_DECLARED
#  endif	/* __NetBSD132__ */
#endif
#endif
