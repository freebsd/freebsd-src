/* Sys.h
 * See the README for details.
 */

/*  $RCSfile: sys.h,v $
 *  $Revision: 14020.13 $
 *  $Date: 93/06/21 06:42:11 $
 */


#ifdef __sun
#	ifndef sun
#		define sun 1
#	endif
#endif

#ifdef sun
#	if !defined(__GNUC__) && !defined(__STDC__) && !defined(SunOverride)
	/* If you choke here, but you know what you're doing, just
	 * define SunOverride.
	 */
	^^^ "You need to use an ANSI C compiler.  Try using gcc or acc." ^^^
#	endif
#	ifdef Solaris	/* not predefined. */
#		ifndef SYSV
#			define SYSV 1
#		endif
#		define System "Solaris"
#	else
#		define System "SunOS"
#		ifndef RINDEX
#			define RINDEX 1
#		endif
#	endif	/* not Solaris */
#	ifndef TERMIOS
#		define TERMIOS 1
#	endif
#	ifndef HAS_DOMAINNAME
#		define HAS_DOMAINNAME 1
#	endif
#endif /* sun */

#ifdef __sgi
#	ifndef sgi
#		define sgi 1
#	endif
#endif

#ifdef sgi
#	define System "IRIX"
#	ifndef SYSV
#		define SYSV 1
#	endif
#	ifndef HERROR
#		define HERROR 1
#	endif
#	ifndef U_WAIT
#		define U_WAIT 1
#	endif
#	ifndef STRICT_PROTOS
#		define STRICT_PROTOS 1
#	endif
#	ifndef TERMIOS
#		define TERMIOS 1
#	endif
#endif /* sgi */

#ifdef	AIX
#	define System "AIX 2.2.1"
#	define BSD_INCLUDES
#	define SYSV
#	define NO_STDLIB
#	define NO_UTIME_H
#	define NO_STRFTIME
#	define NO_STRSTR
#	define NO_MKTIME
#endif	/* AIX */

#ifdef _AIX
#	define System "AIX 3.x"
#	define SYSSELECTH 1
#	define TERMIOS 1
#endif	/* _AIX */

#ifdef __QNX__
#   define QNX
#   define System "QNX 4.21 (POSIX)"
#   define SYSSELECTH
#   define TERMIOS
#   define _POSIX_SOURCE
#   define GETCWDSIZET
#   define STRICT_PROTOS
#   define RINDEX
#	define NO_CURSES_H
#   define unlink remove
#   define bcopy(s,d,l) memcpy((d),(s),(l))
#   define bzero(cp,l) memset((cp),0,(l))
#   define NO_SYSPARAM
#   include <limits.h>
#   define NCARGS _POSIX_ARG_MAX
#endif

#ifdef SCOXNX
#	define System "SCO Xenix"
#	define LAI_TCP
#	define NO_UTIMEH
#	define NO_MKTIME
#	define NO_STRFTIME
#	define NO_STRSTR
#	define NO_RENAME
#	define LINGER   /* else SCO bug causes incomplete transfers */
#	define SYSV 1
#endif	/* SCOXNX */

#ifdef SCO322
#	define System "SCO Unix 3.2v2"
#	define BOTCHED_FOPEN_RW
#	define NO_RENAME	/* it exists, but it corrupts filesystems */
#	define BROKEN_MEMCPY 1
#	define SYSV 1
#endif	/* SCO322 */

#ifdef SCO324
#	define System "SCO Unix 3.2v4"
#	ifndef SYSV
#		define SYSV 1
#	endif
#	ifndef BROKEN_MEMCPY
#		define BROKEN_MEMCPY 1
#	endif
#endif	/* SCO324 */

#ifdef linux
#	define System "Linux"
#	ifndef HAS_DOMAINNAME
#		define HAS_DOMAINNAME 1
#	endif
#	ifndef TERMIOS
#		define TERMIOS 1
#	endif
#	ifndef SYSV
#		define SYSV 1
#	endif
#endif

#ifdef ISC
#	define System "Interactive Unix"
#	ifndef SYSV
#		define SYSV 1
#	endif
#	ifndef BROKEN_MEMCPY
#		define BROKEN_MEMCPY 1
#	endif
#	ifndef NET_ERRNO_H
#		define NET_ERRNO_H 1
#	endif
#endif  /* ISC */

#ifdef aux
#	define System "A/UX"
#	ifndef BROKEN_MEMCPY
#		define BROKEN_MEMCPY 1
#	endif
#	ifndef SYSV
#		define SYSV 1
#	endif
#endif

#ifdef NeXT
#	define System "NeXTStep"
#	ifndef RINDEX
#		define RINDEX 1
#	endif
#	ifndef BSD
#		define BSD 1
#	endif
#	ifndef NO_UNISTDH
#		define NO_UNISTDH 1
#	endif
#	ifndef NO_UTIMEH
#		define NO_UTIMEH
#	endif
#	ifndef HAS_DOMAINNAME
#		define HAS_DOMAINNAME 1
#	endif
#endif

#ifdef pyr
#	define System "OSx"
#	ifndef BSD
#		define BSD 1
#	endif
#	ifndef SGTTYB
#		define SGTTYB 1
#	endif
#	ifndef NO_STDLIBH
#		define NO_STDLIBH 1
#	endif
extern int errno;
#endif	/* pyr */

#ifdef _SEQUENT_
#	if !defined(DYNIXPTX) && !defined(DYNIX)
#		define DYNIXPTX 1
#	endif
#endif

#if DYNIXPTX
#	define System "Dynix/PTX"
#	ifndef SYSV
#		define SYSV 1
#	endif
#	ifndef TRY_NOREPLY
#		define TRY_NOREPLY 1
#	endif
#	define gettimeofday(a, b) get_process_stats(a, getpid(), 0, 0)
#endif  /* DYNIXPTX */

#ifdef DYNIX
#	define System "Dynix"
#	ifndef BSD
#		define BSD 1
#	endif
#	ifndef SGTTYB
#		define SGTTYB 1
#	endif
#	ifndef NO_UTIMEH
#		define NO_UTIMEH 1
#	endif
#	ifndef NO_STDLIBH
#		define NO_STDLIBH 1
#	endif
#	ifndef NO_VARARGS
#		define NO_VARARGS 1
#	endif
#endif	/* DYNIX */

#ifdef ultrix
#	define System "Ultrix"
#	ifndef BSD
#		define BSD 1
#	endif
#	ifndef USE_GETPWUID
#		define USE_GETPWUID 1
#	endif
#	ifndef __GNUC__
#		ifndef NO_CONST
#			define NO_CONST 1
#		endif
#	endif
#endif	/* ultrix */

#ifdef __hpux
#	ifndef HPUX
#		define HPUX 1
#	endif
#	define Select(a,b,c,d,e) select((a), (int *)(b), (c), (d), (e))
#endif

#ifdef HPUX 
#	define System "HP-UX"
#	ifndef _HPUX_SOURCE
#		define _HPUX_SOURCE 1
#	endif
#	ifndef GETCWDSIZET
#		define GETCWDSIZET 1
#	endif
#	define SYSV 1
#endif	/* HPUX */

#ifdef SINIX
#	define System "SINIX"
#	ifndef SYSV
#		define SYSV 1
#	endif
/* You may need to add -lresolv, -lport, -lcurses to MORELIBS in Makefile. */
#endif

#ifdef BULL          /* added 23nov92 for Bull DPX/2 */
#	define _POSIX_SOURCE
#	define _XOPEN_SOURCE
#	define _BULL_SOURCE
#	ifndef SYSV
#		define SYSV 1
#	endif
#	define bull
#	define System "Bull DPX/2 BOS"
#	define SYSSELECTH
#endif  /* BULL */   /* added 23nov92 for Bull DPX/2 */

#ifdef __dgux
#     ifndef DGUX
#             define DGUX 1
#     endif
#endif

#ifdef DGUX
#     ifndef _DGUX_SOURCE
#             define _DGUX_SOURCE
#     endif
#     define GETCWDSIZET 1
#     define BAD_INETADDR 1
#     define SYSV 1
#     define System "DG/UX"
#endif  /* DGUX */

#ifdef apollo
#	ifndef BSD
#		define BSD 43
#	endif
#	define NO_UTIMEH 1
#	define System "Apollo"
#endif

#ifdef __Besta__
#       define SYSV 1
#       define SYSSELECTH 1
#       define NO_UNISTDH 1
#       define NO_STDLIBH 1
#       define NO_UTIMEH 1
#	ifndef BROKEN_MEMCPY
#		define BROKEN_MEMCPY 1
#	endif
#       include <sys/types.h>
#endif

#ifdef __osf__
#	ifdef __alpha    /* DEC OSF/1 */
#		define GETCWDSIZET 1
#	endif
#	ifndef System
#		define System "DEC OSF/1"
#	endif
#endif

#ifdef DELL
#	ifndef System
#		define System "DELL SVR4 Issue 2.2"
#	endif
#	ifndef HAS_DOMAINNAME
#		define HAS_DOMAINNAME 1
#	endif
#	ifndef LINGER
#		define LINGER   /* SVR4/386 Streams TCP/IP bug on close */
#	endif
#endif	/* DELL */

/* -------------------------------------------------------------------- */

#ifdef _SYSV
#	ifndef SYSV
#		define SYSV 1
#	endif
#endif

#ifdef USG
#	ifndef SYSV
#		define SYSV 1
#	endif
#endif

#ifdef _BSD
#	ifndef BSD
#		define BSD 1
#	endif
#endif

#ifdef SVR4
#	ifndef System
#		define System "System V.4"
#	endif
#	ifndef SYSV
#		define SYSV 1
#	endif
#	ifndef VOID
#		define VOID void
#	endif
#	ifndef HERROR
#		define HERROR 1
#	endif
#	ifdef TERMH
#		define TERMH 1
#	endif
#	ifndef Gettimeofday
#		define Gettimeofday gettimeofday
#	endif
#endif  /* SVR4 */

#ifdef SYSV
#	ifndef RINDEX
#		define RINDEX 1
#	endif
#	define bcopy(s,d,l) memcpy((d),(s),(l))
#	define bzero(cp,l) memset((cp),0,(l))
#	ifndef HAS_GETCWD
#		define HAS_GETCWD 1
#	endif
#endif

#ifdef __bsdi__
#	define System "BSDi"
#	ifndef BSD
#		define BSD 1
#	endif
#	ifndef SYSSELECTH
#		define SYSSELECTH 1
#	endif
#	ifndef GETCWDSIZET
#		define GETCWDSIZET 1
#	endif
#	ifndef HERROR
#		define HERROR 1
#	endif
#endif	/* BSDi */

#ifdef __FreeBSD__
#               define System "FreeBSD"
#               define GZCAT "/usr/bin/gzcat"
#		define HAS_DOMAINNAME 1
#       include <sys/types.h>
#       include <sys/param.h>   /* this two for BSD definition */
				/* to avoid redefinition of it to 1 */
#       define HERROR 1
#	define TERMIOS 1
#       define HAS_GETCWD 1
#       define U_WAIT 1
#	define NO_CONST 1       /* avoid prototype conflict */
#endif

#ifdef __NetBSD__
#	define System "NetBSD"
#	define GZCAT "/usr/bin/zcat"
#       define HERROR 1
#	define TERMIOS 1
#       define HAS_GETCWD 1
#	define HAS_DOMAINNAME 1
#       define U_WAIT 1
#	define GETCWDSIZET 1
#	define NO_CONST 1       /* avoid prototype conflict */
#	include <sys/types.h>
#	include <sys/param.h>
#endif

#ifdef BSD
#	ifndef __FreeBSD__
#		ifndef SYSDIRH
#			define SYSDIRH 1
#		endif
#	endif
#	ifndef SGTTYB
#		define SGTTYB 1
#	endif
#endif

/*
 * Generic pointer type, e.g. as returned by malloc().
 */
#ifndef PTRTYPE
#	define PTRTYPE void
#endif

#ifndef Free
#	define Free(a) free((PTRTYPE *)(a))
#endif

/*
 * Some systems besides System V don't use rindex/index (like SunOS).
 * Add -DRINDEX to your SDEFS line if you need to.
 */
#ifdef RINDEX
	/* or #include <strings.h> if you have it. */
#	define rindex strrchr
#	define index strchr
#endif /* RINDEX */

#ifdef SOCKS
#define Getsockname(d,a,l) Rgetsockname((d), (struct sockaddr *)(a), (l))
#else
#ifdef SYSV
#	define Getsockname(d,a,l) getsockname((d), (void *)(a), (l))
#else
#	define Getsockname(d,a,l) getsockname((d), (struct sockaddr *)(a), (l))
#endif
#endif

#ifndef Select
#	define Select(a,b,c,d,e) select((a), (b), (c), (d), (e))
#endif

#ifndef Connect
#ifndef SVR4
#	define Connect(a,b,c) (connect((a), (struct sockaddr *)(b), (int)(c)))
#	define Bind(a,b,c) (bind((a), (struct sockaddr *)(b), (int)(c)))
#	define Accept(a,b,c) (accept((a), (struct sockaddr *)(b), (int *)(c)))
#else  /* SVR4 */
#	define Connect(a,b,c) (connect((a), (caddr_t)(b), (int)(c)))
#	define Bind(a,b,c) (bind((a), (caddr_t)(b), (int)(c)))
#	define Accept(a,b,c) (accept((a), (caddr_t)(b), (int *)(c)))
#endif	/* SVR4 */
#endif	/* Connect */

#ifndef Gettimeofday
#	define Gettimeofday(a) gettimeofday(a, (struct timezone *)0)
#endif /* Gettimeofday */

#ifdef GETPASS
#	define Getpass getpass
#endif

/* Enable connections through firewall gateways */
#ifndef GATEWAY
#	define GATEWAY 1
#endif

#ifdef _POSIX_SOURCE
#	define TERMIOS
#endif

/* Include frequently used headers: */

#include <sys/types.h>

#ifndef NO_SYSPARAM
#include <sys/param.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include <sys/time.h>
#include <time.h>

#ifndef NO_STDLIBH
#	include <stdlib.h>
#else
extern PTRTYPE *malloc(size_t);
extern PTRTYPE *calloc(size_t, size_t);
extern PTRTYPE *malloc(size_t);
extern void	free(PTRTYPE *);
extern PTRTYPE *realloc(PTRTYPE *, size_t);
extern void	exit(int);

#ifdef NO_CONST
extern char *getenv(char *);
extern int atoi(char *);
#else
extern char *getenv(const char *);
extern int atoi(const char *);
#endif

#endif	/* NO_STDLIBH */

#ifndef NO_UNISTDH
#	include <unistd.h>
#else
char *getlogin (void);
#	ifdef NO_CONST
extern char *getenv(char *);
#	else
extern char *getenv(const char *);
#	endif
#endif	/* NO_UNISTDH */

#ifdef NO_STD_PROTOS
extern	int     _filbuf(FILE *);
extern	int     _flsbuf(int, FILE *);
extern	int     fflush(FILE *);
extern	int     fgetc(FILE *);
extern	int     fprintf(FILE *, char *, ...);
extern	int     fputc(int, FILE *);
extern	int     fputs(char *, FILE *);
extern	int     fclose(FILE *);
extern	int     pclose(FILE *);
extern	void    perror(char *);
extern	int     printf(char *, ...);
extern	int     rewind(FILE *);
extern	int     sscanf(char *, char *, ...);
extern	int     vfprintf(FILE *, char *, char *);

extern	char *  mktemp(char *);
extern	int     rename(char *, char *);

extern	int     gettimeofday(struct timeval *, struct timezone *);
extern	time_t  mktime(struct tm *);
extern	int     strftime(char *, int, char *, struct tm *);
extern	time_t  time(time_t *);

extern	int     tolower(int);
extern	int     toupper(int);

#ifndef bcopy
extern	void    bcopy(char *, char *, size_t);
#endif
#ifndef bzero
extern	void    bzero(char *, size_t);
#endif

#ifdef SOCKS 
extern	int     Raccept(int, struct sockaddr *, int *);
extern	int     Rbind(int, struct sockaddr *, int, unsigned long);
extern	int     Rconnect(int, struct sockaddr *, int);
extern	int     Rlisten(int, int);
extern	int     Rgetsockname(int, struct sockaddr *, int *);
#else
extern	int     accept(int, struct sockaddr *, int *);
extern	int     bind(int, struct sockaddr *, int);
extern	int     connect(int, struct sockaddr *, int);
extern	int     listen(int, int);
extern	int     getsockname(int, struct sockaddr *, int *);
#endif
extern	int     gethostname(char *, int), getdomainname(char *, int);
#ifndef Select
extern	int     select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#endif
extern	int     send(int, char *, int, int);
extern	int     setsockopt(int, int, int, char *, int);
extern	int     shutdown(int, int);
extern	int     socket(int, int, int);
#endif	/* NO_STD_PROTOS */

/* This malloc stuff is mostly for our own use. */
#define LIBC_MALLOC 0
#define FAST_MALLOC 1
#define DEBUG_MALLOC 2

#ifdef LIBMALLOC
#	if LIBMALLOC != LIBC_MALLOC
		/* Make sure you use -I to use the malloc.h of choice. */
#		include <malloc.h>
#	endif
#else
#	define LIBMALLOC LIBC_MALLOC
#endif
/* End of personal malloc junk. */

/* eof sys.h */
