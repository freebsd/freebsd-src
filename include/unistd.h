/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)unistd.h	8.12 (Berkeley) 4/27/95
 * $FreeBSD$
 */

#ifndef _UNISTD_H_
#define	_UNISTD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/unistd.h>

#define	 STDIN_FILENO	0	/* standard input file descriptor */
#define	STDOUT_FILENO	1	/* standard output file descriptor */
#define	STDERR_FILENO	2	/* standard error file descriptor */

#ifndef NULL
#define	NULL		0	/* null pointer constant */
#endif

#ifndef _POSIX_SOURCE
#define	F_ULOCK		0	/* unlock locked section */
#define	F_LOCK		1	/* lock a section for exclusive use */
#define	F_TLOCK		2	/* test and lock a section for exclusive use */
#define	F_TEST		3	/* test a section for locks by other procs */
#endif

__BEGIN_DECLS
void	 _exit __P((int)) __dead2;
int	 access __P((const char *, int));
unsigned int	 alarm __P((unsigned int));
int	 chdir __P((const char *));
int	 chown __P((const char *, uid_t, gid_t));
int	 close __P((int));
int	 dup __P((int));
int	 dup2 __P((int, int));
int	 execl __P((const char *, const char *, ...));
int	 execle __P((const char *, const char *, ...));
int	 execlp __P((const char *, const char *, ...));
int	 execv __P((const char *, char * const *));
int	 execve __P((const char *, char * const *, char * const *));
int	 execvp __P((const char *, char * const *));
pid_t	 fork __P((void));
long	 fpathconf __P((int, int));
char	*getcwd __P((char *, size_t));
gid_t	 getegid __P((void));
uid_t	 geteuid __P((void));
gid_t	 getgid __P((void));
int	 getgroups __P((int, gid_t []));
char	*getlogin __P((void));
pid_t	 getpgrp __P((void));
pid_t	 getpid __P((void));
pid_t	 getppid __P((void));
uid_t	 getuid __P((void));
int	 isatty __P((int));
int	 link __P((const char *, const char *));
#ifndef _LSEEK_DECLARED
#define	_LSEEK_DECLARED
off_t	 lseek __P((int, off_t, int));
#endif
long	 pathconf __P((const char *, int));
int	 pause __P((void));
int	 pipe __P((int *));
ssize_t	 read __P((int, void *, size_t));
int	 rmdir __P((const char *));
int	 setgid __P((gid_t));
int	 setpgid __P((pid_t, pid_t));
void	 setproctitle __P((const char *_fmt, ...)) __printf0like(1, 2);
pid_t	 setsid __P((void));
int	 setuid __P((uid_t));
unsigned int	 sleep __P((unsigned int));
long	 sysconf __P((int));
pid_t	 tcgetpgrp __P((int));
int	 tcsetpgrp __P((int, pid_t));
char	*ttyname __P((int));
int	 unlink __P((const char *));
ssize_t	 write __P((int, const void *, size_t));

extern char *optarg;			/* getopt(3) external variables */
extern int optind, opterr, optopt;
int	 getopt __P((int, char * const [], const char *));

#ifndef	_POSIX_SOURCE
#ifdef	__STDC__
struct timeval;				/* select(2) */
#endif
int	 acct __P((const char *));
int	 async_daemon __P((void));
char	*brk __P((const char *));
int	 chroot __P((const char *));
size_t	 confstr __P((int, char *, size_t));
char	*crypt __P((const char *, const char *));
const char *crypt_get_format __P((void));
int	 crypt_set_format __P((const char *));
int	 des_cipher __P((const char *, char *, long, int));
int	 des_setkey __P((const char *key));
int	 encrypt __P((char *, int));
void	 endusershell __P((void));
int	 exect __P((const char *, char * const *, char * const *));
int	 fchdir __P((int));
int	 fchown __P((int, uid_t, gid_t));
char	*fflagstostr __P((u_long));
int	 fsync __P((int));
#ifndef _FTRUNCATE_DECLARED
#define	_FTRUNCATE_DECLARED
int	 ftruncate __P((int, off_t));
#endif
int	 getdomainname __P((char *, int));
int	 getdtablesize __P((void));
int	 getgrouplist __P((const char *, int, int *, int *));
long	 gethostid __P((void));
int	 gethostname __P((char *, int));
char	*getlogin_r __P((char *, int));
mode_t	 getmode __P((const void *, mode_t));
int	 getpagesize __P((void)) __pure2;
char	*getpass __P((const char *));
int	 getpgid __P((pid_t _pid));
int	 getresgid __P((gid_t *, gid_t *, gid_t *));
int	 getresuid __P((uid_t *, uid_t *, uid_t *));
int	 getsid __P((pid_t _pid));
char	*getusershell __P((void));
char	*getwd __P((char *));			/* obsoleted by getcwd() */
int	 initgroups __P((const char *, int));
int	 iruserok __P((unsigned long, int, const char *, const char *));
int	 iruserok_sa __P((const void *, int, int, const char *, const char *));
int	 issetugid __P((void));
int	 lchown __P((const char *, uid_t, gid_t));
int	 lockf __P((int, int, off_t));
char	*mkdtemp __P((char *));
int	 mknod __P((const char *, mode_t, dev_t));
int	 mkstemp __P((char *));
int	 mkstemps __P((char *, int));
char	*mktemp __P((char *));
int	 nfssvc __P((int, void *));
int	 nice __P((int));
ssize_t	 pread __P((int, void *, size_t, off_t));
int	 profil __P((char *, size_t, vm_offset_t, int));
ssize_t	 pwrite __P((int, const void *, size_t, off_t));
int	 rcmd __P((char **, int, const char *,
		const char *, const char *, int *));
int	 rcmd_af __P((char **, int, const char *,
		const char *, const char *, int *, int));
char	*re_comp __P((const char *));
int	 re_exec __P((const char *));
int	 readlink __P((const char *, char *, int));
int	 reboot __P((int));
int	 revoke __P((const char *));
pid_t	 rfork __P((int));
int	 rresvport __P((int *));
int	 rresvport_af __P((int *, int));
int	 ruserok __P((const char *, int, const char *, const char *));
char	*sbrk __P((int));
int	 select __P((int, fd_set *, fd_set *, fd_set *, struct timeval *));
int	 setdomainname __P((const char *, int));
int	 setegid __P((gid_t));
int	 seteuid __P((uid_t));
int	 setgroups __P((int, const gid_t *));
void	 sethostid __P((long));
int	 sethostname __P((const char *, int));
int	 setkey __P((const char *));
int	 setlogin __P((const char *));
void	*setmode __P((const char *));
int	 setpgrp __P((pid_t _pid, pid_t _pgrp)); /* obsoleted by setpgid() */
int	 setregid __P((gid_t, gid_t));
int	 setresgid __P((gid_t, gid_t, gid_t));
int	 setresuid __P((uid_t, uid_t, uid_t));
int	 setreuid __P((uid_t, uid_t));
int	 setrgid __P((gid_t));
int	 setruid __P((uid_t));
void	 setusershell __P((void));
int	 strtofflags __P((char **, u_long *, u_long *));
int	 swapon __P((const char *));
int	 symlink __P((const char *, const char *));
void	 sync __P((void));
int	 syscall __P((int, ...));
off_t	 __syscall __P((quad_t, ...));
#ifndef _TRUNCATE_DECLARED
#define	_TRUNCATE_DECLARED
int	 truncate __P((const char *, off_t));
#endif
int	 ttyslot __P((void));
unsigned int	 ualarm __P((unsigned int, unsigned int));
int	 undelete __P((const char *));
int	 unwhiteout __P((const char *));
int	 usleep __P((unsigned int));
void	*valloc __P((size_t));			/* obsoleted by malloc() */
pid_t	 vfork __P((void));

extern char *suboptarg;			/* getsubopt(3) external variable */
int	 getsubopt __P((char **, char * const *, char **));
#endif /* !_POSIX_SOURCE */
extern int optreset;			/* getopt(3) external variable */
__END_DECLS

#endif /* !_UNISTD_H_ */
