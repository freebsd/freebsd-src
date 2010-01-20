/*	$NetBSD: extern.h,v 1.55 2006/02/01 14:20:12 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)extern.h	8.2 (Berkeley) 4/4/94
 */

/*-
 * Copyright (c) 1997-2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef NO_LONG_LONG
# define LLF		"%ld"
# define LLFP(x)	"%" x "ld"
# define LLT		long
# define ULLF		"%lu"
# define ULLFP(x)	"%" x "lu"
# define ULLT		unsigned long
# define STRTOLL(x,y,z)	strtol(x,y,z)
# define LLTMIN		LONG_MIN
# define LLTMAX		LONG_MAX
#else
# define LLF		"%lld"
# define LLFP(x)	"%" x "lld"
# define LLT		long long
# define ULLF		"%llu"
# define ULLFP(x)	"%" x "llu"
# define ULLT		unsigned long long
# define STRTOLL(x,y,z)	strtoll(x,y,z)
# define LLTMIN		LLONG_MIN
# define LLTMAX		LLONG_MAX
#endif

#define FTP_BUFLEN	512

void	abor(void);
void	blkfree(char **);
void	closedataconn(FILE *);
char   *conffilename(const char *);
char  **copyblk(char **);
void	count_users(void);
void	cprintf(FILE *, const char *, ...)
	    __attribute__((__format__(__printf__, 2, 3)));
void	cwd(const char *);
FILE   *dataconn(const char *, off_t, const char *);
void	delete(const char *);
int	display_file(const char *, int);
char  **do_conversion(const char *);
void	dologout(int);
void	fatal(const char *);
void	feat(void);
void	format_path(char *, const char *);
int	ftpd_pclose(FILE *);
FILE   *ftpd_popen(char *[], const char *, int);
int	getline(char *, int, FILE *);
void	init_curclass(void);
void	logxfer(const char *, off_t, const char *, const char *,
	    const struct timeval *, const char *);
struct tab *lookup(struct tab *, const char *);
void	makedir(const char *);
void	mlsd(const char *);
void	mlst(const char *);
void	opts(const char *);
void	parse_conf(const char *);
void	pass(const char *);
void	passive(void);
int	lpsvproto2af(int);
int	af2lpsvproto(int);
int	epsvproto2af(int);
int	af2epsvproto(int);
void	long_passive(char *, int);
int	extended_port(const char *);
void	epsv_protounsupp(const char *);
void	perror_reply(int, const char *);
void	pwd(void);
void	removedir(const char *);
void	renamecmd(const char *, const char *);
char   *renamefrom(const char *);
void	reply(int, const char *, ...)
	    __attribute__((__format__(__printf__, 2, 3)));
void	retrieve(char *[], const char *);
void	send_file_list(const char *);
void	show_chdir_messages(int);
void	sizecmd(const char *);
void	statcmd(void);
void	statfilecmd(const char *);
void	statxfer(void);
void	store(const char *, const char *, int);
void	user(const char *);
char   *ftpd_strdup(const char *);
void	yyerror(char *);

#ifdef SUPPORT_UTMP
struct utmp;

void	ftpd_initwtmp(void);
void	ftpd_logwtmp(const char *, const char *, const char *);
void	ftpd_login(const struct utmp *);
int	ftpd_logout(const char *);
#endif

#ifdef SUPPORT_UTMPX
struct utmpx;
struct sockinet;

void	ftpd_initwtmpx(void);
void	ftpd_logwtmpx(const char *, const char *, const char *, 
    struct sockinet *, int, int);
void	ftpd_loginx(const struct utmpx *);
int	ftpd_logoutx(const char *, int, int);
#endif

#include <netinet/in.h>

#if defined(__NetBSD__)
# define HAVE_SETPROCTITLE	1
# define HAVE_SOCKADDR_SA_LEN	1
#endif

struct sockinet {
	union sockunion {
		struct sockaddr_in  su_sin;
#ifdef INET6
		struct sockaddr_in6 su_sin6;
#endif
	} si_su;
#if !HAVE_SOCKADDR_SA_LEN
	int	si_len;
#endif
};

#if !HAVE_SOCKADDR_SA_LEN
# define su_len		si_len
#else
# define su_len		si_su.su_sin.sin_len
#endif
#define su_addr		si_su.su_sin.sin_addr
#define su_family	si_su.su_sin.sin_family
#define su_port		si_su.su_sin.sin_port
#ifdef INET6
# define su_6addr	si_su.su_sin6.sin6_addr
# define su_scope_id	si_su.su_sin6.sin6_scope_id
#endif

struct tab {
	char	*name;
	short	 token;
	short	 state;
	short	 flags;	/* 1 if command implemented, 2 if has options,
	                   4 if can occur OOB */
	char	*help;
	char	*options;
};

struct ftpconv {
	struct ftpconv	*next;
	char		*suffix;	/* Suffix of requested name */
	char		*types;		/* Valid file types */
	char		*disable;	/* File to disable conversions */
	char		*command;	/* Command to do the conversion */
};

typedef enum {
	CLASS_GUEST,
	CLASS_CHROOT,
	CLASS_REAL
} class_ft;

typedef enum {
	FLAG_checkportcmd =	1<<0,	/* Check port commands */
	FLAG_denyquick =	1<<1,	/* Check ftpusers(5) before PASS */
	FLAG_hidesymlinks =	1<<2,	/* For symbolic links, list the file
					   or directory the link references
					   rather than the link itself */
	FLAG_modify =		1<<3,	/* Allow CHMOD, DELE, MKD, RMD, RNFR,
					   UMASK */
	FLAG_passive =		1<<4,	/* Allow PASV mode */
	FLAG_private =		1<<5,	/* Don't publish class info in STAT */
	FLAG_sanenames =	1<<6,	/* Restrict names of uploaded files */ 
	FLAG_upload =		1<<7,	/* As per modify, but also allow
					   APPE, STOR, STOU */
} classflag_t;

#define CURCLASS_FLAGS_SET(x)	(curclass.flags |=  (FLAG_ ## x))
#define CURCLASS_FLAGS_CLR(x)	(curclass.flags &= ~(FLAG_ ## x))
#define CURCLASS_FLAGS_ISSET(x)	(curclass.flags &   (FLAG_ ## x))

struct ftpclass {
	struct sockinet	 advertise;	/* PASV address to advertise as */
	char		*chroot;	/* Directory to chroot(2) to at login */
	char		*classname;	/* Current class */
	struct ftpconv	*conversions;	/* List of conversions */
	char		*display;	/* File to display upon chdir */
	char		*homedir;	/* Directory to chdir(2) to at login */
	classflag_t	 flags;		/* Flags; see classflag_t above */
	LLT		 limit;		/* Max connections (-1 = unlimited) */
	char		*limitfile;	/* File to display if limit reached */
	LLT		 maxfilesize;	/* Maximum file size of uploads */
	LLT		 maxrateget;	/* Maximum get transfer rate throttle */
	LLT		 maxrateput;	/* Maximum put transfer rate throttle */
	LLT		 maxtimeout;	/* Maximum permitted timeout */
	char		*motd;		/* MotD file to display after login */
	char		*notify;	/* Files to notify about upon chdir */
	LLT		 portmin;	/* Minumum port for passive mode */
	LLT		 portmax;	/* Maximum port for passive mode */
	LLT		 rateget;	/* Get (RETR) transfer rate throttle */
	LLT		 rateput;	/* Put (STOR) transfer rate throttle */
	LLT		 timeout;	/* Default timeout */
	class_ft	 type;		/* Class type */
	mode_t		 umask;		/* Umask to use */
	LLT		 mmapsize;	/* mmap window size */
	LLT		 readsize;	/* data read size */
	LLT		 writesize;	/* data write size */
	LLT		 recvbufsize;	/* SO_RCVBUF size */
	LLT		 sendbufsize;	/* SO_SNDBUF size */
	LLT		 sendlowat;	/* SO_SNDLOWAT size */
};

extern void		ftp_loop(void) __attribute__ ((noreturn));
extern void		ftp_handle_line(char *);

#ifndef	GLOBAL
#define	GLOBAL	extern
#endif


GLOBAL	struct sockinet ctrl_addr;
GLOBAL	struct sockinet	data_dest;
GLOBAL	struct sockinet	data_source;
GLOBAL	struct sockinet	his_addr;
GLOBAL	struct sockinet	pasv_addr;
GLOBAL	int		connections;
GLOBAL	struct ftpclass	curclass;
GLOBAL	int		ftpd_debug;
GLOBAL	char		*emailaddr;
GLOBAL	int		form;
GLOBAL	int		gidcount;	/* number of entries in gidlist[] */
GLOBAL	gid_t		*gidlist;
GLOBAL	int		hasyyerrored;
GLOBAL	char		hostname[MAXHOSTNAMELEN+1];
GLOBAL	char		homedir[MAXPATHLEN];
#ifdef KERBEROS5
GLOBAL	krb5_context	kcontext;
#endif
GLOBAL	int		logged_in;
GLOBAL	int		logging;
GLOBAL	int		pdata;			/* for passive mode */
#if HAVE_SETPROCTITLE
GLOBAL	char		proctitle[BUFSIZ];	/* initial part of title */
#endif
GLOBAL	struct passwd  *pw;
GLOBAL	int		quietmessages;
GLOBAL	char		remotehost[MAXHOSTNAMELEN+1];
GLOBAL	off_t		restart_point;
GLOBAL	char		tmpline[FTP_BUFLEN];
GLOBAL	int		type;
GLOBAL	int		usedefault;		/* for data transfers */
GLOBAL	const char     *version;
GLOBAL	int		is_oob;

						/* total file data bytes */
GLOBAL	off_t		total_data_in,  total_data_out,  total_data;
						/* total number of data files */
GLOBAL	off_t		total_files_in, total_files_out, total_files;
						/* total bytes */
GLOBAL	off_t		total_bytes_in, total_bytes_out, total_bytes;
						/* total number of xfers */
GLOBAL	off_t		total_xfers_in, total_xfers_out, total_xfers;

extern	struct tab	cmdtab[];

#define	INTERNAL_LS	"/bin/ls"


#define CMD_IMPLEMENTED(x)	((x)->flags != 0)
#define CMD_HAS_OPTIONS(x)	((x)->flags & 0x2)
#define CMD_OOB(x)		((x)->flags & 0x4)

#define	CPUTC(c, f)	do { \
				putc(c, f); total_bytes++; total_bytes_out++; \
			} while (0);

#define CURCLASSTYPE	curclass.type == CLASS_GUEST  ? "GUEST"  : \
			curclass.type == CLASS_CHROOT ? "CHROOT" : \
			curclass.type == CLASS_REAL   ? "REAL"   : \
			"<unknown>"

#define ISDOTDIR(x)	(x[0] == '.' && x[1] == '\0')
#define ISDOTDOTDIR(x)	(x[0] == '.' && x[1] == '.' && x[2] == '\0')

#define EMPTYSTR(p)	((p) == NULL || *(p) == '\0')
#define NEXTWORD(P, W)	do { \
				(W) = strsep(&(P), " \t"); \
			} while ((W) != NULL && *(W) == '\0')
#define PLURAL(s)	((s) == 1 ? "" : "s")
#define REASSIGN(X,Y)	do { if (X) free(X); (X)=(Y); } while (/*CONSTCOND*/0)

#ifndef IPPORT_ANONMAX
# define IPPORT_ANONMAX	65535
#endif
