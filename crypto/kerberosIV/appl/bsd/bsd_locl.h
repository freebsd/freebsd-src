/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: bsd_locl.h,v 1.98 1997/05/25 01:14:17 assar Exp $ */

#define LOGALL
#define KERBEROS
#define KLOGIN_PARANOID
#define LOGIN_ACCESS
#define PASSWD_FALLBACK

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Any better way to test NO_MOTD? */
#if (SunOS == 5) || defined(__hpux)
#define NO_MOTD
#endif

#ifdef HAVE_SHADOW_H
#define SYSV_SHADOW
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <setjmp.h>

#include <stdarg.h>

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifndef S_ISTXT
#ifdef S_ISVTX
#define S_ISTXT S_ISVTX
#else
#define S_ISTXT 0
#endif
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#include <signal.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif /* HAVE_SYS_RESOURCE_H */
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef NCARGS
#define NCARGS  0x100000 /* (absolute) max # characters in exec arglist */
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#if defined(HAVE_SYS_IOCTL_H) && SunOS != 4
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#ifdef HAVE_SYS_STREAM_H
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif /* HAVE_SYS_UIO_H */
#include <sys/stream.h>
#endif /* HAVE_SYS_STREAM_H */

#ifdef HAVE_SYS_PTYVAR_H
#ifdef HAVE_SYS_PROC_H
#include <sys/proc.h>
#endif
#ifdef HAVE_SYS_TTY_H
#include <sys/tty.h>
#endif
#ifdef HAVE_SYS_PTYIO_H
#include <sys/ptyio.h>
#endif
#include <sys/ptyvar.h>
#endif /* HAVE_SYS_PTYVAR_H */

/* Cray stuff */
#ifdef HAVE_UDB_H
#include <udb.h>
#endif
#ifdef HAVE_SYS_CATEGORY_H
#include <sys/category.h>
#endif

/* Strange ioctls that are not always defined */

#ifndef TIOCPKT_FLUSHWRITE
#define TIOCPKT_FLUSHWRITE      0x02
#endif
 
#ifndef TIOCPKT_NOSTOP
#define TIOCPKT_NOSTOP  0x10
#endif
 
#ifndef TIOCPKT_DOSTOP
#define TIOCPKT_DOSTOP  0x20
#endif

#ifndef TIOCPKT
#define TIOCPKT		_IOW('t', 112, int)   /* pty: set/clear packet mode */
#endif

#ifdef HAVE_LASTLOG_H
#include <lastlog.h>
#endif

#ifdef HAVE_LOGIN_H
#include <login.h>
#endif

#ifdef HAVE_TTYENT_H
#include <ttyent.h>
#endif

#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif

#ifdef HAVE_UTMP_H
#include <utmp.h>
#endif
#ifndef UT_NAMESIZE
#define UT_NAMESIZE     sizeof(((struct utmp *)0)->ut_name)
#endif

#ifdef HAVE_UTMPX_H
#include <utmpx.h>
#endif

#ifdef HAVE_USERPW_H
#include <userpw.h>
#endif /* HAVE_USERPW_H */

#ifdef HAVE_USERSEC_H
#include <usersec.h>
#endif /* HAVE_USERSEC_H */

#ifndef PRIO_PROCESS
#define PRIO_PROCESS 0
#endif

#include <err.h>

#include <roken.h>

#ifdef SOCKS
#include <socks.h>
#endif

#include <des.h>
#include <krb.h>
#include <kafs.h>

int kcmd(int *sock, char **ahost, u_int16_t rport, char *locuser,
	 char *remuser, char *cmd, int *fd2p, KTEXT ticket,
	 char *service, char *realm, CREDENTIALS *cred,
	 Key_schedule schedule, MSG_DAT *msg_data,
	 struct sockaddr_in *laddr, struct sockaddr_in *faddr,
	 int32_t authopts);

int krcmd(char **ahost, u_int16_t rport, char *remuser, char *cmd,
	  int *fd2p, char *realm);

int krcmd_mutual(char **ahost, u_int16_t rport, char *remuser,
		 char *cmd,int *fd2p, char *realm,
		 CREDENTIALS *cred, Key_schedule sched);

int klogin(struct passwd *pw, char *instance, char *localhost, char *password);

typedef struct {
        int cnt;
        char *buf;
} BUF;

char *colon(char *cp);
int okname(char *cp0);
int susystem(char *s, int userid);

int forkpty(int *amaster, char *name,
	    struct termios *termp, struct winsize *winp);

#ifndef MODEMASK
#define	MODEMASK	(S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO)
#endif

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#ifdef HAVE_MAILLOCK_H
#include <maillock.h>
#endif
#include "pathnames.h"

void stty_default (void);

int utmpx_login(char *line, char *user, char *host);

extern char **environ;

void sysv_newenv(int argc, char **argv, struct passwd *pwd,
		 char *term, int pflag);

int login_access(char *user, char *from);
#ifndef HAVE_IRUSEROK
int iruserok(u_int32_t raddr, int superuser, const char *ruser,
	     const char *luser);
#endif
void fatal(int f, const char *msg, int syserr);

extern int LEFT_JUSTIFIED;
int des_enc_read(int fd,char *buf,int len,des_key_schedule sched,
	des_cblock *iv);
int des_enc_write(int fd,char *buf,int len,des_key_schedule sched,
	des_cblock *iv);

void sysv_defaults(void);
void utmp_login(char *tty, char *username, char *hostname);
void sleepexit (int);

#ifndef HAVE_SETPRIORITY
#define setpriority(which, who, niceval) 0
#endif

#ifndef HAVE_GETPRIORITY
#define getpriority(which, who) 0
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0
#endif /* _POSIX_VDISABLE */
#if SunOS == 4
#include <sys/ttold.h>
#endif

#if defined(_AIX)
#include <sys/termio.h>
#endif

#ifndef CEOF
#define CEOF 04
#endif

/* concession to Sun */
#ifndef SIGUSR1
#define	SIGUSR1	30
#endif

#ifndef TIOCPKT_WINDOW
#define TIOCPKT_WINDOW 0x80
#endif

int get_shell_port(int kerberos, int encryption);
int get_login_port(int kerberos, int encryption);
int speed_t2int (speed_t);
speed_t int2speed_t (int);
void ip_options_and_die (int sock, struct sockaddr_in *);
void warning(const char *fmt, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
;

char *clean_ttyname (char *tty);
char *make_id (char *tty);
void prepare_utmp (struct utmp *utmp, char *tty, char *username,
		   char *hostname);
