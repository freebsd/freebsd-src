/*
 *	$Id$
 */

#if (defined(__sun__) || defined(sun)) && !defined(__svr4__)

/* Stupid SunOS 4 doesn't have prototypes in the header files */

/* Some includes just to make the compiler be quiet */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>

extern int fprintf(FILE *fp, const char *format, ...);
extern int _flsbuf(unsigned char c, FILE *fp);
extern int puts(const char *str);
extern int printf(const char *format, ...);

extern int chdir(const char *path);
extern int gethostname(char *buf, int bufsize);
extern int atoi(const char *str);
extern int perror(const char *str);

extern int socket (int af, int type, int protocol);
extern int bind (int s, struct sockaddr *name, int namelen);
extern int chdir (const char *path);

#endif


#if (defined(__sun__) || defined(sun)) && defined(__svr4__)

extern char *strdup(const char *str);

#define NEED_GETHOSTNAME
#define NEED_SVCSOC_H

#endif


#if defined(hpux) || defined(__hpux__)

/* HP is really... Ah well. */

#define _INCLUDE_HPUX_SOURCE
#define _INCLUDE_XOPEN_SOURCE
#define _INCLUDE_POSIX_SOURCE
#define _INCLUDE_AES_SOURCE

extern void svcerr_systemerr();
#endif


#if defined(linux) || defined(__linux__)

/* Need this because some header files doesn't check for __linux__ */
#if !defined(linux)
#define linux linux
#endif

/* Needed for non-ANSI prototypes */
#define _SVID_SOURCE

/* Needed for gethostname() */
#define _BSD_SOURCE

#endif
