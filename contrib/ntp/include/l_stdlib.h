/*
 * Proto types for machines that are not ANSI and POSIX	 compliant.
 * This is optional
 */

#ifndef _l_stdlib_h
#define _l_stdlib_h

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

/* Needed for speed_t. */
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif

#include "ntp_types.h"
#include "ntp_proto.h"

/* Let's try to keep this more or less alphabetized... */

#ifdef DECL_ADJTIME_0
struct timeval;
extern	int	adjtime		P((struct timeval *, struct timeval *));
#endif

#ifdef DECL_BCOPY_0
#ifndef bcopy
extern	void	bcopy		P((const char *, char *, int));
#endif
#endif

#ifdef DECL_BZERO_0
#ifndef bzero
extern	void	bzero		P((char *, int));
#endif
#endif

#ifdef DECL_CFSETISPEED_0
struct termios;
extern	int	cfsetispeed	P((struct termios *, speed_t));
extern	int	cfsetospeed	P((struct termios *, speed_t));
#endif

extern	char *	getpass		P((const char *));

#ifdef DECL_HSTRERROR_0
extern	const char * hstrerror	P((int));
#endif

#ifdef DECL_INET_NTOA_0
struct in_addr;
extern	char *	inet_ntoa	P((struct in_addr));
#endif

#ifdef DECL_IOCTL_0
extern	int	ioctl		P((int, u_long, char *));
#endif

#ifdef DECL_IPC_0
struct sockaddr;
extern	int	bind		P((int, struct sockaddr *, int));
extern	int	connect		P((int, struct sockaddr *, int));
extern	int	recv		P((int, char *, int, int));
extern	int	recvfrom	P((int, char *, int, int, struct sockaddr *, int *));
extern	int	send		P((int, char *, int, int));
extern	int	sendto		P((int, char *, int, int, struct sockaddr *, int));
extern	int	setsockopt	P((int, int, int, char *, int));
extern	int	socket		P((int, int, int));
#endif

#ifdef DECL_MEMMOVE_0
extern	void *	memmove		P((void *, const void *, size_t));
#endif

#ifdef DECL_MEMSET_0
extern	char *	memset		P((char *, int, int));
#endif

#ifdef DECL_MKSTEMP_0
extern	int	mkstemp		P((char *));
#endif

#ifdef DECL_MKTEMP_0
extern	char   *mktemp		P((char *));	
#endif

#ifdef DECL_MRAND48_0
extern	long	mrand48		P((void));
#endif

#ifdef DECL_NLIST_0
struct nlist;
extern int	nlist		P((const char *, struct nlist *));
#endif

#ifdef DECL_PLOCK_0
extern	int	plock		P((int));
#endif

#ifdef DECL_RENAME_0
extern	int	rename		P((const char *, const char *));
#endif

#ifdef DECL_SELECT_0
#ifdef _ntp_select_h
extern	int	select		P((int, fd_set *, fd_set *, fd_set *, struct timeval *));
#endif
#endif

#ifdef DECL_SETITIMER_0
struct itimerval;
extern	int	setitimer	P((int , struct itimerval *, struct itimerval *));
#endif

#ifdef PRIO_PROCESS
#ifdef DECL_SETPRIORITY_0
extern	int	setpriority	P((int, int, int));
#endif
#ifdef DECL_SETPRIORITY_1
extern	int	setpriority	P((int, id_t, int));
#endif
#endif

#ifdef DECL_SIGVEC_0
struct sigvec;
extern	int	sigvec		P((int, struct sigvec *, struct sigvec *));
#endif

#ifndef HAVE_SNPRINTF
/* PRINTFLIKE3 */
extern	int	snprintf	P((char *, size_t, const char *, ...));
#endif

#ifdef DECL_SRAND48_0
extern	void	srand48		P((long));
#endif

#ifdef DECL_STDIO_0
#if defined(FILE) || defined(BUFSIZ)
extern	int	_flsbuf		P((int, FILE *));
extern	int	_filbuf		P((FILE *));
extern	int	fclose		P((FILE *));
extern	int	fflush		P((FILE *));
extern	int	fprintf		P((FILE *, const char *, ...));
extern	int	fscanf		P((FILE *, const char *, ...));
extern	int	fputs		P((const char *, FILE *));
extern	int	fputc		P((int, FILE *));
extern	int	fread		P((char *, int, int, FILE *));
extern	void	perror		P((const char *));
extern	int	printf		P((const char *, ...));
extern	int	setbuf		P((FILE *, char *));
# ifdef HAVE_SETLINEBUF
extern	int	setlinebuf	P((FILE *));
# endif
extern	int	setvbuf		P((FILE *, char *, int, int));
extern	int	scanf		P((const char *, ...));
extern	int	sscanf		P((const char *, const char *, ...));
extern	int	vfprintf	P((FILE *, const char *, ...));
extern	int	vsprintf	P((char *, const char *, ...));
#endif
#endif

#ifdef DECL_STIME_0
extern	int	stime		P((const time_t *));
#endif

#ifdef DECL_STIME_1
extern	int	stime		P((long *));
#endif

#ifdef DECL_STRERROR_0
extern	char *	strerror		P((int errnum));
#endif

#ifdef DECL_STRTOL_0
extern	long	strtol		P((const char *, char **, int));
#endif

#ifdef DECL_SYSCALL
extern	int	syscall		P((int, ...));
#endif

#ifdef DECL_SYSLOG_0
extern	void	closelog	P((void));
#ifndef LOG_DAEMON
extern	void	openlog		P((const char *, int));
#else
extern	void	openlog		P((const char *, int, int));
#endif
extern	int	setlogmask	P((int));
extern	void	syslog		P((int, const char *, ...));
#endif

#ifdef DECL_TIME_0
extern	time_t	time		P((time_t *));
#endif

#ifdef DECL_TIMEOFDAY_0
#ifdef SYSV_TIMEOFDAY
extern	int	gettimeofday	P((struct timeval *));
extern	int	settimeofday	P((struct timeval *));
#else /* not SYSV_TIMEOFDAY */
struct timezone;
extern	int	gettimeofday	P((struct timeval *, struct timezone *));
extern	int	settimeofday	P((struct timeval *, void *));
#endif /* not SYSV_TIMEOFDAY */
#endif

#ifdef DECL_TOLOWER_0
extern	int	tolower		P((int));
#endif

#ifdef DECL_TOUPPER_0
extern	int	toupper		P((int));
#endif

/*
 * Necessary variable declarations.
 */
#ifdef DECL_ERRNO
extern	int	errno;
#endif

#ifdef DECL_H_ERRNO
extern	int	h_errno;
#endif

/*******************************************************/

#if 0
/*
 * Unprotoyped	library functions for SunOS 4.x.x
 */
#ifdef SYS_SUNOS4
extern	void	closelog	P((void));
extern	void	openlog		P((char *, int, int));
extern	void	syslog		P((int, char *, ...));
extern	int	setlogmask	P((int));

extern	char *	getpass		P((char *));

extern	int	setpriority	P((int ,int ,int));

extern	long	strtol		P((char *, char **, int));

#if !defined(NTP_POSIX_SOURCE)
extern	int	atoi		P((char *));
extern	int	dup2		P((int, int));
extern	int	execve		P((char *, char **,char **));
extern	int	fork		P((void));
extern	int	getdtablesize	P((void));
extern	int	qsort		(void *, int , int,
				   int P((*compar)(void *, void *)));
extern	long	random		P((void));
extern	long	mrand48		P((void));
extern	int	setpgrp		P((int, int));
extern	void	srandom		P((unsigned int));
extern	void	bcopy		P((const char *, char *, int));
#endif

#ifndef bzero			/* XXX macro prototyping clash */
extern	void	bzero		P((char *, int));
extern	int	bcmp		P((char *, char *, int));
extern	void	bcopy		P((const char *, char *, int));
#endif
extern	char   *mktemp		P((char *));	

extern	int	tolower		P((int));

extern	int	isatty		P((int));

extern	unsigned sleep		P((unsigned ));
extern	unsigned int alarm	P((unsigned int));
extern	int	pause		P((void));

extern	int	getpid		P((void));
extern	int	getppid		P((void));

extern	int	close		P((int));
extern	int	ioctl		P((int, int, char *));
extern	int	rename		P((char *, char *));
#if	0
extern	int	read		P((int, void *, size_t));
extern	int	write		P((int, const void *, size_t));
#endif
extern	int	unlink		P((const char *));
extern	int	link		P((const char *, const char *));

#ifdef FILE
extern	int	fclose		P((FILE *));
extern	int	fflush		P((FILE *));
extern	int	fprintf		P((FILE *, char *, ...));
extern	int	fscanf		P((FILE *, char *, ...));
extern	int	fputs		P((char *, FILE *));
extern	int	fputc		P((char, FILE *));
extern	int	fread		P((char *, int, int, FILE *));
extern	int	printf		P((char *, ...));
extern	int	setbuf		P((FILE *, char *));
extern	int	setvbuf		P((FILE *, char *, int, int));
extern	int	scanf		P((char *, ...));
extern	int	sscanf		P((char *, char *, ...));
extern	int	vsprintf	P((char *, char *, ...));
extern	int	_flsbuf		P((int, FILE *));
extern	int	_filbuf		P((FILE *));
extern	void	perror		P((char *));
#ifdef HAVE_SETLINEBUF
extern	int	setlinebuf	P((FILE *));
#endif
#endif

#ifdef	_ntp_string_h
#ifdef	NTP_POSIX_SOURCE	/* these are builtins */
#ifndef NTP_NEED_BOPS		/* but may be emulated by bops */
extern	char	*memcpy P(());
extern	char	*memset P(());
extern	int	memcmp P(());
#endif
#endif
#endif

#ifdef	_sys_socket_h
extern	int	bind		P((int, struct sockaddr *, int));
extern	int	connect		P((int,	 struct sockaddr *, int));
extern	int	sendto		P((int, char *, int, int, struct sockaddr *, int));
extern	int	setsockopt	P((int, int, int, char *, int));
extern	int	socket		P((int, int, int));
extern	int	recvfrom	P((int, char *, int, int, struct sockaddr *, int *));
#endif /* _sys_socket_h */

#ifdef _ntp_select_h
extern	int	select		P((int, fd_set *, fd_set *, fd_set *, struct timeval *));
#endif

#ifdef _sys_time_h
extern	int	adjtime		P((struct timeval *, struct timeval *));
extern	int	setitimer	P((int , struct itimerval *, struct itimerval *));
#ifdef SYSV_TIMEOFDAY
extern	int	gettimeofday	P((struct timeval *));
extern	int	settimeofday	P((struct timeval *));
#else /* ! SYSV_TIMEOFDAY */
extern	int	gettimeofday	P((struct timeval *, struct timezone *));
extern	int	settimeofday	P((struct timeval *, struct timezone *));
#endif /* SYSV_TIMEOFDAY */
#endif /* _sys_time_h */

#ifdef __time_h
extern	time_t	time		P((time_t *));
#endif

#ifdef	__setjmp_h
extern	int	setjmp		P((jmp_buf));
extern	void	longjmp		P((jmp_buf, int));
#endif

#ifdef _sys_resource_h
extern	int	getrusage	P((int, struct rusage *));
#endif

#ifdef	_nlist_h
extern int	nlist		P((char *, struct nlist *));
#endif

#endif /* SYS_SUNOS4 */

/*
 * Unprototyped library functions for DEC OSF/1
 */
#ifdef SYS_DECOSF1
#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_
extern u_short	htons		P((u_short));
extern u_short	ntohs		P((u_short));
extern u_int32	htonl		P((u_int32));
extern u_int32	ntohl		P((u_int32));
#endif /* _MACHINE_ENDIAN_H_ */

/*
extern	char *	getpass		P((char *));
*/
extern	char *	mktemp		P((char *));
#ifndef SYS_IX86OSF1
extern	int	ioctl		P((int, u_long, char *));
extern	void	bzero		P((char *, int));
#endif

#ifdef SOCK_DGRAM
extern	int	bind		P((int, const struct sockaddr *, int));
extern	int	connect		P((int, const struct sockaddr *, int));
extern	int	socket		P((int, int, int));
extern	int	sendto		P((int, const void *, int, int, const struct sockaddr *, int));
extern	int	setsockopt	P((int, int, int, const void *, int));
extern	int	recvfrom	P((int, void *, int, int, struct sockaddr *, int *));
#endif /* SOCK_STREAM */

#ifdef _ntp_select_h
extern	int	select		P((int, fd_set *, fd_set *, fd_set *, struct timeval *));
#endif

#endif /* DECOSF1 */

/*
 * Unprototyped library functions for Ultrix
 */
#ifdef SYS_ULTRIX
extern	int	close		P((int));
extern	char *	getpass		P((char *));
extern	int	getpid		P((void));
extern	int	ioctl		P((int, int, char *));
extern	char   *mktemp		P((char *));	
extern	int	unlink		P((const char *));
extern	int	link		P((const char *, const char *));

extern	void	closelog	P((void));
extern	void	syslog		P((int, char *, ...));
#ifndef LOG_DAEMON
extern	void	openlog		P((char *, int));
#else
extern	void	openlog		P((char *, int, int));
#endif

extern	int	setpriority	P((int ,int ,int ));

#ifdef SOCK_DGRAM
extern	int	bind		P((int, struct sockaddr *, int));
extern	int	connect		P((int,	 struct sockaddr *, int));
extern	int	socket		P((int, int, int));
extern	int	sendto		P((int, char *, int, int, struct sockaddr *, int));
extern	int	setsockopt	P((int, int, int, char *, int));
extern	int	recvfrom	P((int, char *, int, int, struct sockaddr *, int *));
#endif /* SOCK_STREAM */

#ifdef _TIME_H_
extern	int	gettimeofday	P((struct timeval *, struct timezone *));
extern	int	settimeofday	P((struct timeval *, struct timezone *));
extern	int	adjtime		P((struct timeval *, struct timeval *));
extern	int	select		P((int, fd_set *, fd_set *, fd_set *, struct timeval *));
extern	int	setitimer	P((int , struct itimerval *, struct itimerval *));
#endif /* _TIME_H_ */

#ifdef	N_UNDF
extern int	nlist		P((char *, struct nlist *));
#endif

#ifndef bzero			/* XXX macro prototyping clash */
extern	void	bzero		P((char *, int));
extern	int	bcmp		P((char *, char *, int));
extern	void	bcopy		P((const char *, char *, int));
#endif

#ifndef NTP_POSIX_SOURCE
extern	int	atoi		P((char *));
extern	void	bzero		P((char *, int));
extern	int	bcmp		P((char *, char *, int));
extern	void	bcopy		P((const char *, char *, int));
extern	int	execve		P((char *, char **,char **));
extern	int	fork		P((void));
extern	int	getdtablesize	P((void));
extern	int	ran		P((void));
extern	int	rand		P((void));
extern	void	srand		P((unsigned int));
#ifdef _TIME_H_
extern	int	gettimeofday	P((struct timeval *, struct timezone *));
extern	int	settimeofday	P((struct timeval *, struct timezone *));
#endif
#endif

#ifdef _RESOURCE_H_
extern	int	getrusage	P((int, struct rusage *));
#endif

#endif /* SYS_ULTRIX */

#if defined(__convex__)
extern	char *	getpass		P((char *));
#endif

#ifdef SYS_IRIX4
extern	char *	getpass		P((char *));
#endif /* IRIX4 */

#ifdef SYS_VAX
extern	char *	getpass		P((char *));
#endif /* VAX */

#ifdef SYS_DOMAINOS
extern	char *	getpass		P((char *));
#endif /* SYS_DOMAINOS */

#ifdef SYS_BSD
#define	   IN_CLASSD(i)		   (((long)(i) & 0xf0000000) == 0xe0000000)
#endif

#endif /* 0 */
#endif /* l_stdlib_h */
