/*
 * Copyright (c) 1985, 1988, 1990, 1992, 1993, 1994
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: ftpd.c,v 1.88 1997/06/01 03:13:48 assar Exp $");
#endif

/*
 * FTP server.
 */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if defined(HAVE_SYS_IOCTL_H) && SunOS != 4
#include <sys/ioctl.h>
#endif
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
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

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#define	FTP_NAMES
#include <arpa/ftp.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ARPA_TELNET_H
#include <arpa/telnet.h>
#endif

#include <ctype.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <glob.h>
#include <limits.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif

#include <err.h>

#include "pathnames.h"
#include "extern.h"
#include "common.h"

#include "auth.h"

#include <krb.h>

#include <kafs.h>
#include "roken.h"

#include <otp.h>

#ifdef SOCKS
#include <socks.h>
extern int LIBPREFIX(fclose)      __P((FILE *));
#endif

void yyparse();

#ifndef LOG_FTP
#define LOG_FTP LOG_DAEMON
#endif

static char version[] = "Version 6.00";

extern	off_t restart_point;
extern	char cbuf[];

struct	sockaddr_in ctrl_addr;
struct	sockaddr_in data_source;
struct	sockaddr_in data_dest;
struct	sockaddr_in his_addr;
struct	sockaddr_in pasv_addr;

int	data;
jmp_buf	errcatch, urgcatch;
int	oobflag;
int	logged_in;
struct	passwd *pw;
int	debug;
int	ftpd_timeout = 900;    /* timeout after 15 minutes of inactivity */
int	maxtimeout = 7200;/* don't allow idle time to be set beyond 2 hours */
int	logging;
int	guest;
int	dochroot;
int	type;
int	form;
int	stru;			/* avoid C keyword */
int	mode;
int	usedefault = 1;		/* for data transfers */
int	pdata = -1;		/* for passive mode */
int	transflag;
off_t	file_size;
off_t	byte_count;
#if !defined(CMASK) || CMASK == 0
#undef CMASK
#define CMASK 027
#endif
int	defumask = CMASK;		/* default umask value */
int	guest_umask = 0777;	/* Paranoia for anonymous users */
char	tmpline[10240];
char	hostname[MaxHostNameLen];
char	remotehost[MaxHostNameLen];
static char ttyline[20];

#define AUTH_PLAIN	(1 << 0) /* allow sending passwords */
#define AUTH_OTP	(1 << 1) /* passwords are one-time */
#define AUTH_FTP	(1 << 2) /* allow anonymous login */

static int auth_level = 0; /* Only allow kerberos login by default */

/*
 * Timeout intervals for retrying connections
 * to hosts that don't accept PORT cmds.  This
 * is a kludge, but given the problems with TCP...
 */
#define	SWAITMAX	90	/* wait at most 90 seconds */
#define	SWAITINT	5	/* interval between retries */

int	swaitmax = SWAITMAX;
int	swaitint = SWAITINT;

#ifdef HAVE_SETPROCTITLE
char	proctitle[BUFSIZ];	/* initial part of title */
#endif /* HAVE_SETPROCTITLE */

#define LOGCMD(cmd, file) \
	if (logging > 1) \
	    syslog(LOG_INFO,"%s %s%s", cmd, \
		*(file) == '/' ? "" : curdir(), file);
#define LOGCMD2(cmd, file1, file2) \
	 if (logging > 1) \
	    syslog(LOG_INFO,"%s %s%s %s%s", cmd, \
		*(file1) == '/' ? "" : curdir(), file1, \
		*(file2) == '/' ? "" : curdir(), file2);
#define LOGBYTES(cmd, file, cnt) \
	if (logging > 1) { \
		if (cnt == (off_t)-1) \
		    syslog(LOG_INFO,"%s %s%s", cmd, \
			*(file) == '/' ? "" : curdir(), file); \
		else \
		    syslog(LOG_INFO, "%s %s%s = %ld bytes", \
			cmd, (*(file) == '/') ? "" : curdir(), file, (long)cnt); \
	}

static void	 ack (char *);
static void	 myoob (int);
static int	 checkuser (char *, char *);
static int	 checkaccess (char *);
static FILE	*dataconn (char *, off_t, char *);
static void	 dolog (struct sockaddr_in *);
static void	 end_login (void);
static FILE	*getdatasock (char *);
static char	*gunique (char *);
static RETSIGTYPE	 lostconn (int);
static int	 receive_data (FILE *, FILE *);
static void	 send_data (FILE *, FILE *);
static struct passwd * sgetpwnam (char *);
static void	 usage(void);

static char *
curdir(void)
{
	static char path[MaxPathLen+1+1];	/* path + '/' + '\0' */

	if (getcwd(path, sizeof(path)-2) == NULL)
		return ("");
	if (path[1] != '\0')		/* special case for root dir. */
		strcat(path, "/");
	/* For guest account, skip / since it's chrooted */
	return (guest ? path+1 : path);
}

#ifndef LINE_MAX
#define LINE_MAX 1024
#endif

static int
parse_auth_level(char *str)
{
    char *p;
    int ret = 0;
    char *foo = NULL;

    for(p = strtok_r(str, ",", &foo);
	p;
	p = strtok_r(NULL, ",", &foo)) {
	if(strcmp(p, "user") == 0)
	    ;
	else if(strcmp(p, "otp") == 0)
	    ret |= AUTH_PLAIN|AUTH_OTP;
	else if(strcmp(p, "ftp") == 0 ||
		strcmp(p, "safe") == 0)
	    ret |= AUTH_FTP;
	else if(strcmp(p, "plain") == 0)
	    ret |= AUTH_PLAIN;
	else if(strcmp(p, "none") == 0)
	    ret |= AUTH_PLAIN|AUTH_FTP;
	else
	    warnx("bad value for -a: `%s'", p);
    }
    return ret;	    
}

/*
 * Print usage and die.
 */

static void
usage (void)
{
    fprintf (stderr,
	     "Usage: %s [-d] [-i] [-g guest_umask] [-l] [-p port]"
	     " [-t timeout] [-T max_timeout] [-u umask] [-v]"
	     " [-a auth_level] \n",
	     __progname);
    exit (1);
}

int
main(int argc, char **argv)
{
	int addrlen, ch, on = 1, tos;
	char *cp, line[LINE_MAX];
	FILE *fd;
	int not_inetd = 0;
	int port;
	struct servent *sp;
	char tkfile[1024];

	set_progname (argv[0]);

	/* detach from any tickets and tokens */

	snprintf(tkfile, sizeof(tkfile),
		 "/tmp/ftp_%u", (unsigned)getpid());
	krb_set_tkt_string(tkfile);
	if(k_hasafs())
	    k_setpag();

	sp = getservbyname("ftp", "tcp");
	if(sp)
	    port = sp->s_port;
	else
	    port = htons(21);

	while ((ch = getopt(argc, argv, "a:dg:ilp:t:T:u:v")) != EOF) {
		switch (ch) {
		case 'a':
		    auth_level = parse_auth_level(optarg);
		    break;
		case 'd':
		    debug = 1;
		    break;

		case 'i':
		    not_inetd = 1;
		    break;
		case 'g':
		    {
			long val = 0;

			val = strtol(optarg, &optarg, 8);
			if (*optarg != '\0' || val < 0)
			    warnx("bad value for -g");
			else
			    guest_umask = val;
			break;
		    }
		case 'l':
		    logging++;	/* > 1 == extra logging */
		    break;

		case 'p':
		    sp = getservbyname(optarg, "tcp");
		    if(sp)
			port = sp->s_port;
		    else
			if(isdigit(optarg[0]))
			    port = htons(atoi(optarg));
			else
			    warnx("bad value for -p");
		    break;
		    
		case 't':
		    ftpd_timeout = atoi(optarg);
		    if (maxtimeout < ftpd_timeout)
			maxtimeout = ftpd_timeout;
		    break;

		case 'T':
		    maxtimeout = atoi(optarg);
		    if (ftpd_timeout > maxtimeout)
			ftpd_timeout = maxtimeout;
		    break;

		case 'u':
		    {
			long val = 0;

			val = strtol(optarg, &optarg, 8);
			if (*optarg != '\0' || val < 0)
			    warnx("bad value for -u");
			else
			    defumask = val;
			break;
		    }

		case 'v':
		    debug = 1;
		    break;

		default:
		    usage ();
		}
	}

	if(not_inetd)
	    mini_inetd (port);

	/*
	 * LOG_NDELAY sets up the logging connection immediately,
	 * necessary for anonymous ftp's that chroot and can't do it later.
	 */
	openlog("ftpd", LOG_PID | LOG_NDELAY, LOG_FTP);
	addrlen = sizeof(his_addr);
	if (getpeername(0, (struct sockaddr *)&his_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getpeername (%s): %m",argv[0]);
		exit(1);
	}
	addrlen = sizeof(ctrl_addr);
	if (getsockname(0, (struct sockaddr *)&ctrl_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getsockname (%s): %m",argv[0]);
		exit(1);
	}
#if defined(IP_TOS) && defined(HAVE_SETSOCKOPT)
	tos = IPTOS_LOWDELAY;
	if (setsockopt(0, IPPROTO_IP, IP_TOS, (void *)&tos, sizeof(int)) < 0)
		syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif
	data_source.sin_port = htons(ntohs(ctrl_addr.sin_port) - 1);
	debug = 0;

	/* set this here so it can be put in wtmp */
	snprintf(ttyline, sizeof(ttyline), "ftp%u", (unsigned)getpid());


	/*	freopen(_PATH_DEVNULL, "w", stderr); */
	signal(SIGPIPE, lostconn);
	signal(SIGCHLD, SIG_IGN);
#ifdef SIGURG
	if (signal(SIGURG, myoob) == SIG_ERR)
	    syslog(LOG_ERR, "signal: %m");
#endif

	auth_init();

	/* Try to handle urgent data inline */
#if defined(SO_OOBINLINE) && defined(HAVE_SETSOCKOPT)
	if (setsockopt(0, SOL_SOCKET, SO_OOBINLINE, (void *)&on,
		       sizeof(on)) < 0)
		syslog(LOG_ERR, "setsockopt: %m");
#endif

#ifdef	F_SETOWN
	if (fcntl(fileno(stdin), F_SETOWN, getpid()) == -1)
		syslog(LOG_ERR, "fcntl F_SETOWN: %m");
#endif
	dolog(&his_addr);
	/*
	 * Set up default state
	 */
	data = -1;
	type = TYPE_A;
	form = FORM_N;
	stru = STRU_F;
	mode = MODE_S;
	tmpline[0] = '\0';

	/* If logins are disabled, print out the message. */
	if ((fd = fopen(_PATH_NOLOGIN,"r")) != NULL) {
		while (fgets(line, sizeof(line), fd) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			lreply(530, "%s", line);
		}
		fflush(stdout);
		fclose(fd);
		reply(530, "System not available.");
		exit(0);
	}
	if ((fd = fopen(_PATH_FTPWELCOME, "r")) != NULL) {
		while (fgets(line, sizeof(line), fd) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			lreply(220, "%s", line);
		}
		fflush(stdout);
		fclose(fd);
		/* reply(220,) must follow */
	}
	k_gethostname(hostname, sizeof(hostname));
	reply(220, "%s FTP server (%s+%s) ready.", hostname, 
	      version, krb4_version);
	setjmp(errcatch);
	for (;;)
	    yyparse();
	/* NOTREACHED */
}

static RETSIGTYPE
lostconn(int signo)
{

	if (debug)
		syslog(LOG_DEBUG, "lost connection");
	dologout(-1);
}

/*
 * Helper function for sgetpwnam().
 */
static char *
sgetsave(char *s)
{
	char *new = strdup(s);

	if (new == NULL) {
		perror_reply(421, "Local resource failure: malloc");
		dologout(1);
		/* NOTREACHED */
	}
	return new;
}

/*
 * Save the result of a getpwnam.  Used for USER command, since
 * the data returned must not be clobbered by any other command
 * (e.g., globbing).
 */
static struct passwd *
sgetpwnam(char *name)
{
	static struct passwd save;
	struct passwd *p;

	if ((p = k_getpwnam(name)) == NULL)
		return (p);
	if (save.pw_name) {
		free(save.pw_name);
		free(save.pw_passwd);
		free(save.pw_gecos);
		free(save.pw_dir);
		free(save.pw_shell);
	}
	save = *p;
	save.pw_name = sgetsave(p->pw_name);
	save.pw_passwd = sgetsave(p->pw_passwd);
	save.pw_gecos = sgetsave(p->pw_gecos);
	save.pw_dir = sgetsave(p->pw_dir);
	save.pw_shell = sgetsave(p->pw_shell);
	return (&save);
}

static int login_attempts;	/* number of failed login attempts */
static int askpasswd;		/* had user command, ask for passwd */
static char curname[10];	/* current USER name */
OtpContext otp_ctx;

/*
 * USER command.
 * Sets global passwd pointer pw if named account exists and is acceptable;
 * sets askpasswd if a PASS command is expected.  If logged in previously,
 * need to reset state.  If name is "ftp" or "anonymous", the name is not in
 * _PATH_FTPUSERS, and ftp account exists, set guest and pw, then just return.
 * If account doesn't exist, ask for passwd anyway.  Otherwise, check user
 * requesting login privileges.  Disallow anyone who does not have a standard
 * shell as returned by getusershell().  Disallow anyone mentioned in the file
 * _PATH_FTPUSERS to allow people such as root and uucp to be avoided.
 */
void
user(char *name)
{
	char *cp, *shell;

	if(auth_level == 0 && !auth_complete){
	    reply(530, "No login allowed without authorization.");
	    return;
	}

	if (logged_in) {
		if (guest) {
			reply(530, "Can't change user from guest login.");
			return;
		} else if (dochroot) {
			reply(530, "Can't change user from chroot user.");
			return;
		}
		end_login();
	}

	guest = 0;
	if (strcmp(name, "ftp") == 0 || strcmp(name, "anonymous") == 0) {
	    if ((auth_level & AUTH_FTP) == 0 ||
		checkaccess("ftp") || 
		checkaccess("anonymous"))
		reply(530, "User %s access denied.", name);
	    else if ((pw = sgetpwnam("ftp")) != NULL) {
		guest = 1;
		defumask = guest_umask;	/* paranoia for incoming */
		askpasswd = 1;
		reply(331, "Guest login ok, type your name as password.");
	    } else
		reply(530, "User %s unknown.", name);
	    if (!askpasswd && logging)
		syslog(LOG_NOTICE,
		       "ANONYMOUS FTP LOGIN REFUSED FROM %s(%s)",
		       remotehost, inet_ntoa(his_addr.sin_addr));
	    return;
	}
	if((auth_level & AUTH_PLAIN) == 0 && !auth_complete){
	    reply(530, "Only authorized and anonymous login allowed.");
	    return;
	}
	if ((pw = sgetpwnam(name))) {
		if ((shell = pw->pw_shell) == NULL || *shell == 0)
			shell = _PATH_BSHELL;
		while ((cp = getusershell()) != NULL)
			if (strcmp(cp, shell) == 0)
				break;
		endusershell();

		if (cp == NULL || checkaccess(name)) {
			reply(530, "User %s access denied.", name);
			if (logging)
				syslog(LOG_NOTICE,
				       "FTP LOGIN REFUSED FROM %s(%s), %s",
				       remotehost,
				       inet_ntoa(his_addr.sin_addr),
				       name);
			pw = (struct passwd *) NULL;
			return;
		}
	}
	if (logging)
		strncpy(curname, name, sizeof(curname)-1);
	if(auth_ok())
		ct->userok(name);
	else {
		char ss[256];

		if (otp_challenge(&otp_ctx, name, ss, sizeof(ss)) == 0) {
			reply(331, "Password %s for %s required.",
			      ss, name);
			askpasswd = 1;
		} else if ((auth_level & AUTH_OTP) == 0) {
		    reply(331, "Password required for %s.", name);
		    askpasswd = 1;
		} else {
		    char *s;
		    
		    if (s = otp_error (&otp_ctx))
			lreply(530, "OTP: %s", s);
		    reply(530,
			  "Only authorized, anonymous and OTP "
			  "login allowed.");
		}

	}
	/*
	 * Delay before reading passwd after first failed
	 * attempt to slow down passwd-guessing programs.
	 */
	if (login_attempts)
		sleep(login_attempts);
}

/*
 * Check if a user is in the file "fname"
 */
static int
checkuser(char *fname, char *name)
{
	FILE *fd;
	int found = 0;
	char *p, line[BUFSIZ];

	if ((fd = fopen(fname, "r")) != NULL) {
		while (fgets(line, sizeof(line), fd) != NULL)
			if ((p = strchr(line, '\n')) != NULL) {
				*p = '\0';
				if (line[0] == '#')
					continue;
				if (strcmp(line, name) == 0) {
					found = 1;
					break;
				}
			}
		fclose(fd);
	}
	return (found);
}


/*
 * Determine whether a user has access, based on information in 
 * _PATH_FTPUSERS. The users are listed one per line, with `allow'
 * or `deny' after the username. If anything other than `allow', or
 * just nothing, is given after the username, `deny' is assumed.
 *
 * If the user is not found in the file, but the pseudo-user `*' is,
 * the permission is taken from that line.
 *
 * This preserves the old semantics where if a user was listed in the
 * file he was denied, otherwise he was allowed.
 *
 * Return 1 if the user is denied, or 0 if he is allowed.  */

static int
match(const char *pattern, const char *string)
{
#ifdef HAVE_FNMATCH
    return fnmatch(pattern, string, FNM_NOESCAPE);
#else
    return strcmp(pattern, "*") != 0 && strcmp(pattern, string) != 0;
#endif
}

static int
checkaccess(char *name)
{
#define ALLOWED		0
#define	NOT_ALLOWED	1
    FILE *fd;
    int allowed = ALLOWED;
    char *user, *perm, line[BUFSIZ];
    char *foo;
    
    fd = fopen(_PATH_FTPUSERS, "r");
    
    if(fd == NULL)
	return allowed;

    while (fgets(line, sizeof(line), fd) != NULL)  {
	foo = NULL;
	user = strtok_r(line, " \t\n", &foo);
	if (user == NULL || user[0] == '#')
	    continue;
	perm = strtok_r(NULL, " \t\n", &foo);
	if (match(user, name) == 0){
	    if(perm && strcmp(perm, "allow") == 0)
		allowed = ALLOWED;
	    else
		allowed = NOT_ALLOWED;
	    break;
	}
    }
    fclose(fd);
    return allowed;
}
#undef	ALLOWED
#undef	NOT_ALLOWED

int do_login(int code, char *passwd)
{
        FILE *fd;
	login_attempts = 0;		/* this time successful */
	if (setegid((gid_t)pw->pw_gid) < 0) {
		reply(550, "Can't set gid.");
		return -1;
	}
	initgroups(pw->pw_name, pw->pw_gid);

	/* open wtmp before chroot */
	logwtmp(ttyline, pw->pw_name, remotehost);
	logged_in = 1;

	dochroot = checkuser(_PATH_FTPCHROOT, pw->pw_name);
	if (guest) {
		/*
		 * We MUST do a chdir() after the chroot. Otherwise
		 * the old current directory will be accessible as "."
		 * outside the new root!
		 */
		if (chroot(pw->pw_dir) < 0 || chdir("/") < 0) {
			reply(550, "Can't set guest privileges.");
			return -1;
		}
	} else if (dochroot) {
		if (chroot(pw->pw_dir) < 0 || chdir("/") < 0) {
			reply(550, "Can't change root.");
			return -1;
		}
	} else if (chdir(pw->pw_dir) < 0) {
		if (chdir("/") < 0) {
			reply(530, "User %s: can't change directory to %s.",
			    pw->pw_name, pw->pw_dir);
			return -1;
		} else
			lreply(code, "No directory! Logging in with home=/");
	}
	if (seteuid((uid_t)pw->pw_uid) < 0) {
		reply(550, "Can't set uid.");
		return -1;
	}
	/*
	 * Display a login message, if it exists.
	 * N.B. reply(code,) must follow the message.
	 */
	if ((fd = fopen(_PATH_FTPLOGINMESG, "r")) != NULL) {
		char *cp, line[LINE_MAX];

		while (fgets(line, sizeof(line), fd) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			lreply(code, "%s", line);
		}
	}
	if (guest) {
		reply(code, "Guest login ok, access restrictions apply.");
#ifdef HAVE_SETPROCTITLE
		snprintf (proctitle, sizeof(proctitle),
			  "%s: anonymous/%s",
			  remotehost,
			  passwd);
#endif /* HAVE_SETPROCTITLE */
		if (logging)
			syslog(LOG_INFO, "ANONYMOUS FTP LOGIN FROM %s(%s), %s",
			       remotehost, 
			       inet_ntoa(his_addr.sin_addr),
			       passwd);
	} else {
		reply(code, "User %s logged in.", pw->pw_name);
#ifdef HAVE_SETPROCTITLE
		snprintf(proctitle, sizeof(proctitle), "%s: %s", remotehost, pw->pw_name);
		setproctitle(proctitle);
#endif /* HAVE_SETPROCTITLE */
		if (logging)
			syslog(LOG_INFO, "FTP LOGIN FROM %s(%s) as %s",
			       remotehost,
			       inet_ntoa(his_addr.sin_addr),
			       pw->pw_name);
	}
	umask(defumask);
	return 0;
}

/*
 * Terminate login as previous user, if any, resetting state;
 * used when USER command is given or login fails.
 */
static void
end_login(void)
{

	seteuid((uid_t)0);
	if (logged_in)
		logwtmp(ttyline, "", "");
	pw = NULL;
	logged_in = 0;
	guest = 0;
	dochroot = 0;
}

void
pass(char *passwd)
{
	int rval;

	/* some clients insists on sending a password */
	if (logged_in && askpasswd == 0){
	     reply(230, "Dumpucko!");
	     return;
	}

	if (logged_in || askpasswd == 0) {
		reply(503, "Login with USER first.");
		return;
	}
	askpasswd = 0;
	rval = 1;
	if (!guest) {		/* "ftp" is only account allowed no password */
		if (pw == NULL)
			rval = 1;	/* failure below */
		else if (otp_verify_user (&otp_ctx, passwd) == 0) {
		    rval = 0;
		} else if((auth_level & AUTH_OTP) == 0) {
		    char realm[REALM_SZ];
		    if((rval = krb_get_lrealm(realm, 1)) == KSUCCESS)
			rval = krb_verify_user(pw->pw_name, "", realm, 
					       passwd, 1, NULL);
		    if (rval == KSUCCESS ){
			if(k_hasafs())
			    k_afsklog(0, 0);
		    }else 
			rval = unix_verify_user(pw->pw_name, passwd);
		} else {
		    char *s;
		    
		    if (s = otp_error(&otp_ctx))
			lreply(530, "OTP: %s", s);
		}
		memset (passwd, 0, strlen(passwd));

		/*
		 * If rval == 1, the user failed the authentication
		 * check above.  If rval == 0, either Kerberos or
		 * local authentication succeeded.
		 */
		if (rval) {
			reply(530, "Login incorrect.");
			if (logging)
				syslog(LOG_NOTICE,
				    "FTP LOGIN FAILED FROM %s(%s), %s",
				       remotehost,
				       inet_ntoa(his_addr.sin_addr),
				       curname);
			pw = NULL;
			if (login_attempts++ >= 5) {
				syslog(LOG_NOTICE,
				       "repeated login failures from %s(%s)",
				       remotehost,
				       inet_ntoa(his_addr.sin_addr));
				exit(0);
			}
			return;
		}
	}
	if(!do_login(230, passwd))
	  return;
	
	/* Forget all about it... */
	end_login();
}

void
retrieve(char *cmd, char *name)
{
	FILE *fin = NULL, *dout;
	struct stat st;
	int (*closefunc) (FILE *);
	char line[BUFSIZ];


	if (cmd == 0) {
		fin = fopen(name, "r");
		closefunc = fclose;
		st.st_size = 0;
		if(fin == NULL){
		    struct cmds {
			char *ext;
			char *cmd;
		    } cmds[] = {
			{".tar", "/bin/gtar cPf - %s"},
			{".tar.gz", "/bin/gtar zcPf - %s"},
			{".tar.Z", "/bin/gtar ZcPf - %s"},
			{".gz", "/bin/gzip -c %s"},
			{".Z", "/bin/compress -c %s"},
			{NULL, NULL}
		    };
		    struct cmds *p;
		    for(p = cmds; p->ext; p++){
			char *tail = name + strlen(name) - strlen(p->ext);
			char c = *tail;
			
			if(strcmp(tail, p->ext) == 0 &&
			   (*tail  = 0) == 0 &&
			   access(name, R_OK) == 0){
			    snprintf (line, sizeof(line), p->cmd, name);
			    *tail  = c;
			    break;
			}
			*tail = c;
		    }
		    if(p->ext){
			fin = ftpd_popen(line, "r", 0, 0);
			closefunc = ftpd_pclose;
			st.st_size = -1;
			cmd = line;
		    }
		}
	} else {
		snprintf(line, sizeof(line), cmd, name);
		name = line;
		fin = ftpd_popen(line, "r", 1, 0);
		closefunc = ftpd_pclose;
		st.st_size = -1;
	}
	if (fin == NULL) {
		if (errno != 0) {
			perror_reply(550, name);
			if (cmd == 0) {
				LOGCMD("get", name);
			}
		}
		return;
	}
	byte_count = -1;
	if (cmd == 0){
	    if(fstat(fileno(fin), &st) < 0 || !S_ISREG(st.st_mode)) {
		reply(550, "%s: not a plain file.", name);
		goto done;
	    }
	}
	if (restart_point) {
		if (type == TYPE_A) {
			off_t i, n;
			int c;

			n = restart_point;
			i = 0;
			while (i++ < n) {
				if ((c=getc(fin)) == EOF) {
					perror_reply(550, name);
					goto done;
				}
				if (c == '\n')
					i++;
			}
		} else if (lseek(fileno(fin), restart_point, SEEK_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	dout = dataconn(name, st.st_size, "w");
	if (dout == NULL)
		goto done;
	set_buffer_size(fileno(dout), 0);
	send_data(fin, dout);
	fclose(dout);
	data = -1;
	pdata = -1;
done:
	if (cmd == 0)
		LOGBYTES("get", name, byte_count);
	(*closefunc)(fin);
}

/* filename sanity check */

int 
filename_check(char *filename)
{
  static const char good_chars[] = "+-=_,.";
    char *p;

    p = strrchr(filename, '/');
    if(p)
	filename = p + 1;

    p = filename;

    if(isalnum(*p)){
	p++;
	while(*p && (isalnum(*p) || strchr(good_chars, *p)))
	    p++;
	if(*p == '\0')
	    return 0;
    }
    lreply(553, "\"%s\" is an illegal filename.", filename);
    lreply(553, "The filename must start with an alphanumeric "
	   "character and must only");
    reply(553, "consist of alphanumeric characters or any of the following: %s", 
	  good_chars);
    return 1;
}

void
do_store(char *name, char *mode, int unique)
{
	FILE *fout, *din;
	struct stat st;
	int (*closefunc) (FILE *);

	if(guest && filename_check(name))
	    return;
	if (unique && stat(name, &st) == 0 &&
	    (name = gunique(name)) == NULL) {
		LOGCMD(*mode == 'w' ? "put" : "append", name);
		return;
	}

	if (restart_point)
		mode = "r+";
	fout = fopen(name, mode);
	closefunc = fclose;
	if (fout == NULL) {
		perror_reply(553, name);
		LOGCMD(*mode == 'w' ? "put" : "append", name);
		return;
	}
	byte_count = -1;
	if (restart_point) {
		if (type == TYPE_A) {
			off_t i, n;
			int c;

			n = restart_point;
			i = 0;
			while (i++ < n) {
				if ((c=getc(fout)) == EOF) {
					perror_reply(550, name);
					goto done;
				}
				if (c == '\n')
					i++;
			}
			/*
			 * We must do this seek to "current" position
			 * because we are changing from reading to
			 * writing.
			 */
			if (fseek(fout, 0L, SEEK_CUR) < 0) {
				perror_reply(550, name);
				goto done;
			}
		} else if (lseek(fileno(fout), restart_point, SEEK_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	din = dataconn(name, (off_t)-1, "r");
	if (din == NULL)
		goto done;
	set_buffer_size(fileno(din), 1);
	if (receive_data(din, fout) == 0) {
		if (unique)
			reply(226, "Transfer complete (unique file name:%s).",
			    name);
		else
			reply(226, "Transfer complete.");
	}
	fclose(din);
	data = -1;
	pdata = -1;
done:
	LOGBYTES(*mode == 'w' ? "put" : "append", name, byte_count);
	(*closefunc)(fout);
}

static FILE *
getdatasock(char *mode)
{
	int on = 1, s, t, tries;

	if (data >= 0)
		return (fdopen(data, mode));
	seteuid((uid_t)0);
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		goto bad;
#if defined(SO_REUSEADDR) && defined(HAVE_SETSOCKOPT)
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	    (void *) &on, sizeof(on)) < 0)
		goto bad;
#endif
	/* anchor socket to avoid multi-homing problems */
	data_source.sin_family = AF_INET;
	data_source.sin_addr = ctrl_addr.sin_addr;
	for (tries = 1; ; tries++) {
		if (bind(s, (struct sockaddr *)&data_source,
		    sizeof(data_source)) >= 0)
			break;
		if (errno != EADDRINUSE || tries > 10)
			goto bad;
		sleep(tries);
	}
	seteuid((uid_t)pw->pw_uid);
#if defined(IP_TOS) && defined(HAVE_SETSOCKOPT)
	on = IPTOS_THROUGHPUT;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (void *)&on, sizeof(int)) < 0)
		syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif
	return (fdopen(s, mode));
bad:
	/* Return the real value of errno (close may change it) */
	t = errno;
	seteuid((uid_t)pw->pw_uid);
	close(s);
	errno = t;
	return (NULL);
}

static FILE *
dataconn(char *name, off_t size, char *mode)
{
	char sizebuf[32];
	FILE *file;
	int retry = 0, tos;

	file_size = size;
	byte_count = 0;
	if (size != (off_t) -1)
		snprintf(sizebuf, sizeof(sizebuf), " (%ld bytes)", size);
	else
		strcpy(sizebuf, "");
	if (pdata >= 0) {
		struct sockaddr_in from;
		int s, fromlen = sizeof(from);

		s = accept(pdata, (struct sockaddr *)&from, &fromlen);
		if (s < 0) {
			reply(425, "Can't open data connection.");
			close(pdata);
			pdata = -1;
			return (NULL);
		}
		close(pdata);
		pdata = s;
#if defined(IP_TOS) && defined(HAVE_SETSOCKOPT)
		tos = IPTOS_THROUGHPUT;
		setsockopt(s, IPPROTO_IP, IP_TOS, (void *)&tos,
		    sizeof(int));
#endif
		reply(150, "Opening %s mode data connection for '%s'%s.",
		     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
		return (fdopen(pdata, mode));
	}
	if (data >= 0) {
		reply(125, "Using existing data connection for '%s'%s.",
		    name, sizebuf);
		usedefault = 1;
		return (fdopen(data, mode));
	}
	if (usedefault)
		data_dest = his_addr;
	usedefault = 1;
	file = getdatasock(mode);
	if (file == NULL) {
		reply(425, "Can't create data socket (%s,%d): %s.",
		    inet_ntoa(data_source.sin_addr),
		    ntohs(data_source.sin_port), strerror(errno));
		return (NULL);
	}
	data = fileno(file);
	while (connect(data, (struct sockaddr *)&data_dest,
	    sizeof(data_dest)) < 0) {
		if (errno == EADDRINUSE && retry < swaitmax) {
			sleep((unsigned) swaitint);
			retry += swaitint;
			continue;
		}
		perror_reply(425, "Can't build data connection");
		fclose(file);
		data = -1;
		return (NULL);
	}
	reply(150, "Opening %s mode data connection for '%s'%s.",
	     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
	return (file);
}

/*
 * Tranfer the contents of "instr" to "outstr" peer using the appropriate
 * encapsulation of the data subject * to Mode, Structure, and Type.
 *
 * NB: Form isn't handled.
 */
static void
send_data(FILE *instr, FILE *outstr)
{
	int c, cnt, filefd, netfd;
	static char *buf;
	static size_t bufsize;
	int i = 0;
	char s[1024];

	transflag++;
	if (setjmp(urgcatch)) {
		transflag = 0;
		return;
	}
	switch (type) {

	case TYPE_A:
		while ((c = getc(instr)) != EOF) {
		    byte_count++;
		    if(i > 1022){
			auth_write(fileno(outstr), s, i);
			i = 0;
		    }
		    if(c == '\n')
			s[i++] = '\r';
		    s[i++] = c;
		}
		if(i)
		    auth_write(fileno(outstr), s, i);
		auth_write(fileno(outstr), s, 0);
		fflush(outstr);
		transflag = 0;
		if (ferror(instr))
			goto file_err;
		if (ferror(outstr))
			goto data_err;
		reply(226, "Transfer complete.");
		return;
		
	case TYPE_I:
	case TYPE_L:
#ifdef HAVE_MMAP
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif
	    {
		struct stat st;
		char *chunk;
		int in = fileno(instr);
		if(fstat(in, &st) == 0 && S_ISREG(st.st_mode)) {
		    chunk = mmap(0, st.st_size, PROT_READ, MAP_SHARED, in, 0);
		    if(chunk != (void *)MAP_FAILED) {
			cnt = st.st_size - restart_point;
			auth_write(fileno(outstr),
				   chunk + restart_point,
				   cnt);
			munmap(chunk, st.st_size);
			auth_write(fileno(outstr), NULL, 0);
			byte_count = cnt;
			transflag = 0;
		    }
		}
	    }
	
#endif
	if(transflag){
	    struct stat st;

	    netfd = fileno(outstr);
	    filefd = fileno(instr);
	    buf = alloc_buffer (buf, &bufsize,
				fstat(filefd, &st) >= 0 ? &st : NULL);
	    if (buf == NULL) {
		transflag = 0;
		perror_reply(451, "Local resource failure: malloc");
		return;
	    }
	    while ((cnt = read(filefd, buf, bufsize)) > 0 &&
		   auth_write(netfd, buf, cnt) == cnt)
		byte_count += cnt;
	    auth_write(netfd, buf, 0); /* to end an encrypted stream */
	    transflag = 0;
	    if (cnt != 0) {
		if (cnt < 0)
		    goto file_err;
		goto data_err;
	    }
	}
	reply(226, "Transfer complete.");
	return;
	default:
	    transflag = 0;
	    reply(550, "Unimplemented TYPE %d in send_data", type);
	    return;
	}

data_err:
	transflag = 0;
	perror_reply(426, "Data connection");
	return;

file_err:
	transflag = 0;
	perror_reply(551, "Error on input file");
}

/*
 * Transfer data from peer to "outstr" using the appropriate encapulation of
 * the data subject to Mode, Structure, and Type.
 *
 * N.B.: Form isn't handled.
 */
static int
receive_data(FILE *instr, FILE *outstr)
{
    int cnt, bare_lfs = 0;
    static char *buf;
    static size_t bufsize;
    struct stat st;

    transflag++;
    if (setjmp(urgcatch)) {
	transflag = 0;
	return (-1);
    }

    buf = alloc_buffer (buf, &bufsize,
			fstat(fileno(outstr), &st) >= 0 ? &st : NULL);
    if (buf == NULL) {
	transflag = 0;
	perror_reply(451, "Local resource failure: malloc");
	return -1;
    }
    
    switch (type) {

    case TYPE_I:
    case TYPE_L:
	while ((cnt = auth_read(fileno(instr), buf, bufsize)) > 0) {
	    if (write(fileno(outstr), buf, cnt) != cnt)
		goto file_err;
	    byte_count += cnt;
	}
	if (cnt < 0)
	    goto data_err;
	transflag = 0;
	return (0);

    case TYPE_E:
	reply(553, "TYPE E not implemented.");
	transflag = 0;
	return (-1);

    case TYPE_A:
    {
	char *p, *q;
	int cr_flag = 0;
	while ((cnt = auth_read(fileno(instr),
				buf + cr_flag, 
				bufsize - cr_flag)) > 0){
	    byte_count += cnt;
	    cnt += cr_flag;
	    cr_flag = 0;
	    for(p = buf, q = buf; p < buf + cnt;) {
		if(*p == '\n')
		    bare_lfs++;
		if(*p == '\r')
		    if(p == buf + cnt - 1){
			cr_flag = 1;
			p++;
			continue;
		    }else if(p[1] == '\n'){
			*q++ = '\n';
			p += 2;
			continue;
		    }
		*q++ = *p++;
	    }
	    fwrite(buf, q - buf, 1, outstr);
	    if(cr_flag)
		buf[0] = '\r';
	}
	if(cr_flag)
	    putc('\r', outstr);
	fflush(outstr);
	if (ferror(instr))
	    goto data_err;
	if (ferror(outstr))
	    goto file_err;
	transflag = 0;
	if (bare_lfs) {
	    lreply(226, "WARNING! %d bare linefeeds received in ASCII mode\r\n"
		   "    File may not have transferred correctly.\r\n",
		   bare_lfs);
	}
	return (0);
    }
    default:
	reply(550, "Unimplemented TYPE %d in receive_data", type);
	transflag = 0;
	return (-1);
    }
	
data_err:
    transflag = 0;
    perror_reply(426, "Data Connection");
    return (-1);
	
file_err:
    transflag = 0;
    perror_reply(452, "Error writing file");
    return (-1);
}

void
statfilecmd(char *filename)
{
	FILE *fin;
	int c;
	char line[LINE_MAX];

	snprintf(line, sizeof(line), "/bin/ls -la %s", filename);
	fin = ftpd_popen(line, "r", 1, 0);
	lreply(211, "status of %s:", filename);
	while ((c = getc(fin)) != EOF) {
		if (c == '\n') {
			if (ferror(stdout)){
				perror_reply(421, "control connection");
				ftpd_pclose(fin);
				dologout(1);
				/* NOTREACHED */
			}
			if (ferror(fin)) {
				perror_reply(551, filename);
				ftpd_pclose(fin);
				return;
			}
			putc('\r', stdout);
		}
		putc(c, stdout);
	}
	ftpd_pclose(fin);
	reply(211, "End of Status");
}

void
statcmd(void)
{
#if 0
	struct sockaddr_in *sin;
	u_char *a, *p;

	lreply(211, "%s FTP server status:", hostname, version);
	printf("     %s\r\n", version);
	printf("     Connected to %s", remotehost);
	if (!isdigit(remotehost[0]))
		printf(" (%s)", inet_ntoa(his_addr.sin_addr));
	printf("\r\n");
	if (logged_in) {
		if (guest)
			printf("     Logged in anonymously\r\n");
		else
			printf("     Logged in as %s\r\n", pw->pw_name);
	} else if (askpasswd)
		printf("     Waiting for password\r\n");
	else
		printf("     Waiting for user name\r\n");
	printf("     TYPE: %s", typenames[type]);
	if (type == TYPE_A || type == TYPE_E)
		printf(", FORM: %s", formnames[form]);
	if (type == TYPE_L)
#if NBBY == 8
		printf(" %d", NBBY);
#else
		printf(" %d", bytesize);	/* need definition! */
#endif
	printf("; STRUcture: %s; transfer MODE: %s\r\n",
	    strunames[stru], modenames[mode]);
	if (data != -1)
		printf("     Data connection open\r\n");
	else if (pdata != -1) {
		printf("     in Passive mode");
		sin = &pasv_addr;
		goto printaddr;
	} else if (usedefault == 0) {
		printf("     PORT");
		sin = &data_dest;
printaddr:
		a = (u_char *) &sin->sin_addr;
		p = (u_char *) &sin->sin_port;
#define UC(b) (((int) b) & 0xff)
		printf(" (%d,%d,%d,%d,%d,%d)\r\n", UC(a[0]),
			UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
#undef UC
	} else
		printf("     No data connection\r\n");
#endif
	reply(211, "End of status");
}

void
fatal(char *s)
{

	reply(451, "Error in server: %s\n", s);
	reply(221, "Closing connection due to server error.");
	dologout(0);
	/* NOTREACHED */
}

static void
int_reply(int, char *, const char *, va_list)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 0)))
#endif
;

static void
int_reply(int n, char *c, const char *fmt, va_list ap)
{
  char buf[10240];
  char *p;
  p=buf;
  if(n){
      snprintf(p, sizeof(buf), "%d%s", n, c);
      p+=strlen(p);
  }
  vsnprintf(p, sizeof(buf) - strlen(p), fmt, ap);
  p+=strlen(p);
  snprintf(p, sizeof(buf) - strlen(p), "\r\n");
  p+=strlen(p);
  auth_printf("%s", buf);
  fflush(stdout);
  if (debug)
    syslog(LOG_DEBUG, "<--- %s- ", buf);
}

void
reply(int n, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int_reply(n, " ", fmt, ap);
  delete_ftp_command();
  va_end(ap);
}

void
lreply(int n, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int_reply(n, "-", fmt, ap);
  va_end(ap);
}

void
nreply(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int_reply(0, NULL, fmt, ap);
  va_end(ap);
}

static void
ack(char *s)
{

	reply(250, "%s command successful.", s);
}

void
nack(char *s)
{

	reply(502, "%s command not implemented.", s);
}

/* ARGSUSED */
void
yyerror(char *s)
{
	char *cp;

	if ((cp = strchr(cbuf,'\n')))
		*cp = '\0';
	reply(500, "'%s': command not understood.", cbuf);
}

void
do_delete(char *name)
{
	struct stat st;

	LOGCMD("delete", name);
	if (stat(name, &st) < 0) {
		perror_reply(550, name);
		return;
	}
	if ((st.st_mode&S_IFMT) == S_IFDIR) {
		if (rmdir(name) < 0) {
			perror_reply(550, name);
			return;
		}
		goto done;
	}
	if (unlink(name) < 0) {
		perror_reply(550, name);
		return;
	}
done:
	ack("DELE");
}

void
cwd(char *path)
{

	if (chdir(path) < 0)
		perror_reply(550, path);
	else
		ack("CWD");
}

void
makedir(char *name)
{

	LOGCMD("mkdir", name);
	if(guest && filename_check(name))
	    return;
	if (mkdir(name, 0777) < 0)
		perror_reply(550, name);
	else{
	    if(guest)
		chmod(name, 0700); /* guest has umask 777 */
	    reply(257, "MKD command successful.");
	}
}

void
removedir(char *name)
{

	LOGCMD("rmdir", name);
	if (rmdir(name) < 0)
		perror_reply(550, name);
	else
		ack("RMD");
}

void
pwd(void)
{
    char path[MaxPathLen + 1];
    char *ret;

    /* SunOS has a broken getcwd that does popen(pwd) (!!!), this
     * failes miserably when running chroot 
     */
    ret = getcwd(path, sizeof(path));
    if (ret == NULL)
	reply(550, "%s.", strerror(errno));
    else
	reply(257, "\"%s\" is current directory.", path);
}

char *
renamefrom(char *name)
{
	struct stat st;

	if (stat(name, &st) < 0) {
		perror_reply(550, name);
		return NULL;
	}
	reply(350, "File exists, ready for destination name");
	return (name);
}

void
renamecmd(char *from, char *to)
{

	LOGCMD2("rename", from, to);
	if(guest && filename_check(to))
	    return;
	if (rename(from, to) < 0)
		perror_reply(550, "rename");
	else
		ack("RNTO");
}

static void
dolog(struct sockaddr_in *sin)
{
	inaddr2str (sin->sin_addr, remotehost, sizeof(remotehost));
#ifdef HAVE_SETPROCTITLE
	snprintf(proctitle, sizeof(proctitle), "%s: connected", remotehost);
	setproctitle(proctitle);
#endif /* HAVE_SETPROCTITLE */

	if (logging)
		syslog(LOG_INFO, "connection from %s(%s)",
		       remotehost,
		       inet_ntoa(his_addr.sin_addr));
}

/*
 * Record logout in wtmp file
 * and exit with supplied status.
 */
void
dologout(int status)
{
    transflag = 0;
    if (logged_in) {
	seteuid((uid_t)0);
	logwtmp(ttyline, "", "");
	dest_tkt();
	if(k_hasafs())
	    k_unlog();
    }
    /* beware of flushing buffers after a SIGPIPE */
#ifdef XXX
    exit(status);
#else
    _exit(status);
#endif	
}

void abor(void)
{
}

static void
myoob(int signo)
{
#if 0
	char *cp;
#endif

	/* only process if transfer occurring */
	if (!transflag)
		return;

	/* This is all XXX */
	oobflag = 1;
	/* if the command resulted in a new command, 
	   parse that as well */
	do{
	    yyparse();
	} while(ftp_command);
	oobflag = 0;

#if 0 
	cp = tmpline;
	if (getline(cp, 7) == NULL) {
		reply(221, "You could at least say goodbye.");
		dologout(0);
	}
	upper(cp);
	if (strcmp(cp, "ABOR\r\n") == 0) {
		tmpline[0] = '\0';
		reply(426, "Transfer aborted. Data connection closed.");
		reply(226, "Abort successful");
		longjmp(urgcatch, 1);
	}
	if (strcmp(cp, "STAT\r\n") == 0) {
		if (file_size != (off_t) -1)
			reply(213, "Status: %ld of %ld bytes transferred",
			      (long)byte_count,
			      (long)file_size);
		else
			reply(213, "Status: %ld bytes transferred"
			      (long)byte_count);
	}
#endif
}

/*
 * Note: a response of 425 is not mentioned as a possible response to
 *	the PASV command in RFC959. However, it has been blessed as
 *	a legitimate response by Jon Postel in a telephone conversation
 *	with Rick Adams on 25 Jan 89.
 */
void
passive(void)
{
	int len;
	char *p, *a;

	pdata = socket(AF_INET, SOCK_STREAM, 0);
	if (pdata < 0) {
		perror_reply(425, "Can't open passive connection");
		return;
	}
	pasv_addr = ctrl_addr;
	pasv_addr.sin_port = 0;
	seteuid((uid_t)0);
	if (bind(pdata, (struct sockaddr *)&pasv_addr, sizeof(pasv_addr)) < 0) {
		seteuid((uid_t)pw->pw_uid);
		goto pasv_error;
	}
	seteuid((uid_t)pw->pw_uid);
	len = sizeof(pasv_addr);
	if (getsockname(pdata, (struct sockaddr *) &pasv_addr, &len) < 0)
		goto pasv_error;
	if (listen(pdata, 1) < 0)
		goto pasv_error;
	a = (char *) &pasv_addr.sin_addr;
	p = (char *) &pasv_addr.sin_port;

#define UC(b) (((int) b) & 0xff)

	reply(227, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)", UC(a[0]),
		UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
	return;

pasv_error:
	close(pdata);
	pdata = -1;
	perror_reply(425, "Can't open passive connection");
	return;
}

/*
 * Generate unique name for file with basename "local".
 * The file named "local" is already known to exist.
 * Generates failure reply on error.
 */
static char *
gunique(char *local)
{
	static char new[MaxPathLen];
	struct stat st;
	int count;
	char *cp;

	cp = strrchr(local, '/');
	if (cp)
		*cp = '\0';
	if (stat(cp ? local : ".", &st) < 0) {
		perror_reply(553, cp ? local : ".");
		return NULL;
	}
	if (cp)
		*cp = '/';
	for (count = 1; count < 100; count++) {
		snprintf (new, sizeof(new), "%s.%d", local, count);
		if (stat(new, &st) < 0)
			return (new);
	}
	reply(452, "Unique file name cannot be created.");
	return (NULL);
}

/*
 * Format and send reply containing system error number.
 */
void
perror_reply(int code, char *string)
{
	reply(code, "%s: %s.", string, strerror(errno));
}

static char *onefile[] = {
	"",
	0
};

void
send_file_list(char *whichf)
{
  struct stat st;
  DIR *dirp = NULL;
  struct dirent *dir;
  FILE *dout = NULL;
  char **dirlist, *dirname;
  int simple = 0;
  int freeglob = 0;
  glob_t gl;
  char buf[MaxPathLen];

  if (strpbrk(whichf, "~{[*?") != NULL) {
    int flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE;

    memset(&gl, 0, sizeof(gl));
    freeglob = 1;
    if (glob(whichf, flags, 0, &gl)) {
      reply(550, "not found");
      goto out;
    } else if (gl.gl_pathc == 0) {
      errno = ENOENT;
      perror_reply(550, whichf);
      goto out;
    }
    dirlist = gl.gl_pathv;
  } else {
    onefile[0] = whichf;
    dirlist = onefile;
    simple = 1;
  }

  if (setjmp(urgcatch)) {
    transflag = 0;
    goto out;
  }
  while ((dirname = *dirlist++)) {
    if (stat(dirname, &st) < 0) {
      /*
       * If user typed "ls -l", etc, and the client
       * used NLST, do what the user meant.
       */
      if (dirname[0] == '-' && *dirlist == NULL &&
	  transflag == 0) {
	retrieve("/bin/ls %s", dirname);
	goto out;
      }
      perror_reply(550, whichf);
      if (dout != NULL) {
	fclose(dout);
	transflag = 0;
	data = -1;
	pdata = -1;
      }
      goto out;
    }

    if (S_ISREG(st.st_mode)) {
      if (dout == NULL) {
	dout = dataconn("file list", (off_t)-1, "w");
	if (dout == NULL)
	  goto out;
	transflag++;
      }
      snprintf(buf, sizeof(buf), "%s%s\n", dirname,
	      type == TYPE_A ? "\r" : "");
      auth_write(fileno(dout), buf, strlen(buf));
      byte_count += strlen(dirname) + 1;
      continue;
    } else if (!S_ISDIR(st.st_mode))
      continue;

    if ((dirp = opendir(dirname)) == NULL)
      continue;

    while ((dir = readdir(dirp)) != NULL) {
      char nbuf[MaxPathLen];

      if (!strcmp(dir->d_name, "."))
	continue;
      if (!strcmp(dir->d_name, ".."))
	continue;

      snprintf(nbuf, sizeof(nbuf), "%s/%s", dirname, dir->d_name);

      /*
       * We have to do a stat to insure it's
       * not a directory or special file.
       */
      if (simple || (stat(nbuf, &st) == 0 &&
		     S_ISREG(st.st_mode))) {
	if (dout == NULL) {
	  dout = dataconn("file list", (off_t)-1, "w");
	  if (dout == NULL)
	    goto out;
	  transflag++;
	}
	if(strncmp(nbuf, "./", 2) == 0)
	  snprintf(buf, sizeof(buf), "%s%s\n", nbuf +2,
		   type == TYPE_A ? "\r" : "");
	else
	  snprintf(buf, sizeof(buf), "%s%s\n", nbuf,
		   type == TYPE_A ? "\r" : "");
	auth_write(fileno(dout), buf, strlen(buf));
	byte_count += strlen(nbuf) + 1;
      }
    }
    closedir(dirp);
  }
  if (dout == NULL)
    reply(550, "No files found.");
  else if (ferror(dout) != 0)
    perror_reply(550, "Data connection");
  else
    reply(226, "Transfer complete.");

  transflag = 0;
  if (dout != NULL){
    auth_write(fileno(dout), buf, 0); /* XXX flush */
	    
    fclose(dout);
  }
  data = -1;
  pdata = -1;
out:
  if (freeglob) {
    freeglob = 0;
    globfree(&gl);
  }
}


int
find(char *pattern)
{
    char line[1024];
    FILE *f;

    snprintf(line, sizeof(line),
	     "/bin/locate -d %s %s",
	     ftp_rooted("/etc/locatedb"),
	     pattern);
    f = ftpd_popen(line, "r", 1, 1);
    if(f == NULL){
	perror_reply(550, "/bin/locate");
	return 1;
    }
    lreply(200, "Output from find.");
    while(fgets(line, sizeof(line), f)){
	if(line[strlen(line)-1] == '\n')
	    line[strlen(line)-1] = 0;
	nreply("%s", line);
    }
    reply(200, "Done");
    ftpd_pclose(f);
    return 0;
}

