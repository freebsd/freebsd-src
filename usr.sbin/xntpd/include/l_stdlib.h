/*
 * Proto types for machines that are not ANSI and POSIX  compliant.
 * This is optionaly
 */

#ifndef _l_stdlib_h
#define _l_stdlib_h

#if defined(NTP_POSIX_SOURCE)
#include <stdlib.h>
#endif

#ifndef	P
#if defined(__STDC__) || defined(USE_PROTOTYPES)
#define P(x)	x
#else
#define	P(x)	()
#if	!defined(const)
#define	const
#endif
#endif
#endif

/*
 * Unprottyped  library functions for SunOS 4.x.x
 */
#ifdef SYS_SUNOS4
extern	void	closelog	P((void));
extern	void	openlog		P((char *, int, int));
extern	void	syslog		P((int, char *, ...));
extern  int	setlogmask	P((int));

extern  char *	getpass		P((char *));

extern  int	setpriority	P((int ,int ,int));

extern	long	strtol		P((char *, char **, int));

#if !defined(NTP_POSIX_SOURCE)
extern  int	atoi		P((char *));
extern  void	bcopy		P((char *, char *, int));
extern	int	dup2		P((int, int));
extern	int	execve		P((char *, char **,char **));
extern	int	fork		P((void));
extern  int	getdtablesize	P((void));
extern  int     qsort		P((void *, int , int,
                    		   int (*compar)(void *, void *)));
extern	int	rand		P((void));
extern	int	setpgrp		P((int, int));
extern  void	srand		P((unsigned int));
#endif

#ifndef bzero			/* XXX macro prototyping clash */
extern  void    bzero		P((char *, int));
extern  int	bcmp		P((char *, char *, int));
#endif
extern  char   *mktemp		P((char *));	

extern  int	tolower		P((int));

extern  int     isatty		P((int));

extern  unsigned sleep		P((unsigned ));
extern  unsigned int alarm	P((unsigned int));
extern	int	pause		P((void));

extern  int	getpid		P((void));
extern	int	getppid		P((void));

extern	int	close		P((int));
extern  int	ioctl		P((int, int, char *));
extern	int	read		P((int, char *, unsigned));
extern  int	rename		P((char *, char *));
extern	int	write		P((int, char *, int));
extern	int	unlink		P((char *));
extern	int	link		P((char *, char *));

#ifdef FILE
extern  int	fclose		P((FILE *));
extern	int	fflush		P((FILE *));
extern  int	fprintf		P((FILE *, char *, ...));
extern  int	fputs		P((char *, FILE *));
extern	int	fputc		P((char, FILE *));
extern	int	fread		P((char *, int, int, FILE *));
extern  int	printf		P((char *, ...));
extern	int	setbuf		P((FILE *, char *));
extern  int     setvbuf		P((FILE *, char *, int, int));
extern	int	scanf		P((char *, ...));
extern  int	vsprintf	P((char *, char *, ...));
extern  int	_flsbuf		P((int, FILE *));
extern  int	_filbuf		P((FILE *));
extern  void	perror		P((char *));
#ifndef NTP_POSIX_SOURCE
extern	int	setlinebuf	P((FILE *));
#endif
#endif

#ifdef	_ntp_string_h
#ifdef	NTP_POSIX_SOURCE	/* these are builtins */
extern	char	*memcpy();
extern	char	*memset();
extern	int	memcmp();
#endif
#endif

#ifdef	_sys_socket_h
extern  int	bind		P((int, struct sockaddr *, int));
extern	int	connect		P((int,  struct sockaddr *, int));
extern  int     sendto		P((int, char *, int, int, struct sockaddr *, int));
extern  int	setsockopt	P((int, int, int, char *, int));
extern  int	socket		P((int, int, int));
extern  int	recvfrom	P((int, char *, int, int, struct sockaddr *, int *));
#endif /* _sys_socket_h */

#ifdef _ntp_select_h
extern	int	select		P((int, fd_set *, fd_set *, fd_set *, struct timeval *));
#endif

#ifdef _sys_time_h
extern  int	adjtime		P((struct timeval *, struct timeval *));
extern  int	setitimer	P((int , struct itimerval *, struct itimerval *));
#ifdef SYSV_TIMEOFDAY
extern  int	gettimeofday    P((struct timeval *));
extern  int	settimeofday    P((struct timeval *));
#else /* ! SYSV_TIMEOFDAY */
extern  int	gettimeofday    P((struct timeval *, struct timezone *));
extern  int	settimeofday    P((struct timeval *, struct timezone *));
#endif /* SYSV_TIMEOFDAY */
#endif /* _sys_time_h */

#ifdef __time_h
extern  time_t	time		P((time_t *));
#endif

#ifdef  __setjmp_h
extern  int 	setjmp		P((jmp_buf));
extern  void 	longjmp		P((jmp_buf, int));
#endif

#ifdef _sys_resource_h
extern	int	getrusage	P((int, struct rusage *));
#endif

#ifdef  _nlist_h
extern int 	nlist		P((char *, struct nlist *));
#endif

#endif /* SYS_SUNOS4 */

/*
 * Unprototyped  library functions for ULTRIX.
 */
#ifdef SYS_ULTRIX
extern	int	close		P((int));
extern  char *	getpass		P((char *));
extern  int	getpid		P((void));
extern  int	ioctl		P((int, int, char *));
extern  char   *mktemp		P((char *));	
extern	int	unlink		P((const char *));
extern	int	link		P((const char *, const char *));

#if defined(LOG_DEBUG)
extern	void	closelog	P((void));
extern	void	syslog		P((int, char *, ...));
#ifndef LOG_DAEMON
extern	void	openlog		P((char *, int));
#else
extern	void	openlog		P((char *, int, int));
#endif
#endif

extern  int	setpriority	P((int ,int ,int ));

#ifdef SOCK_DGRAM
extern  int	bind		P((int, struct sockaddr *, int));
extern	int	connect		P((int,  struct sockaddr *, int));
extern  int	socket		P((int, int, int));
extern  int     sendto		P((int, char *, int, int, struct sockaddr *, int));
extern  int	setsockopt	P((int, int, int, char *, int));
extern  int	recvfrom	P((int, char *, int, int, struct sockaddr *, int *));
#endif /* SOCK_STREAM */

#ifdef _TIME_H_
extern  int	adjtime		P((struct timeval *, struct timeval *));
extern	int	select		P((int, fd_set *, fd_set *, fd_set *, struct timeval *));
extern  int	setitimer	P((int , struct itimerval *, struct itimerval *));
#endif /* _TIME_H_ */

#ifdef  N_UNDF
extern int 	nlist		P((char *, struct nlist *));
#endif

#ifndef NTP_POSIX_SOURCE
extern  int	atoi		P((char *));
extern  void	bcopy		P((char *, char *, int));
extern  void    bzero		P((char *, int));
extern  int	bcmp		P((char *, char *, int));
extern  void	bcopy		P((char *, char *, int));
extern	int	execve		P((char *, char **,char **));
extern	int	fork		P((void));
extern  int	getdtablesize	P((void));
extern	int	ran		P((void));
extern	int	rand		P((void));
extern  void	srand		P((unsigned int));
#if defined(_STDIO_H_)
extern	int	setlinebuf	P((FILE *));
#endif
#ifdef _TIME_H_
extern  int	gettimeofday    P((struct timeval *, struct timezone *));
#endif
#endif

#endif /* SYS_ULTIRX */

#if defined(__convex__)
extern  char *	getpass		P((char *));
#endif

#ifdef SYS_IRIX4
extern  char *	getpass		P((char *));
#endif /* IRIX4 */

#ifdef SYS_VAX
extern	char *	getpass		P((char *));
#endif /* VAX */

#endif /* l_stdlib_h */

