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
 *
 *	$Id: ftpd.c,v 1.41 1997/07/24 09:26:12 davidn Exp $
 */

#if 0
#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1985, 1988, 1990, 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */
#endif

#if 0
#ifndef lint
static char sccsid[] = "@(#)ftpd.c	8.4 (Berkeley) 4/16/94";
#endif /* not lint */
#endif

/*
 * FTP server.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#define	FTP_NAMES
#include <arpa/ftp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <grp.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <libutil.h>
#ifdef	LOGIN_CAP
#include <login_cap.h>
#endif

#ifdef	SKEY
#include <skey.h>
#endif

#include "pathnames.h"
#include "extern.h"

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef	INTERNAL_LS
static char version[] = "Version 6.00LS";
#undef main
#else
static char version[] = "Version 6.00";
#endif

extern	off_t restart_point;
extern	char cbuf[];

struct	sockaddr_in server_addr;
struct	sockaddr_in ctrl_addr;
struct	sockaddr_in data_source;
struct	sockaddr_in data_dest;
struct	sockaddr_in his_addr;
struct	sockaddr_in pasv_addr;

int	daemon_mode;
int	data;
jmp_buf	errcatch, urgcatch;
int	logged_in;
struct	passwd *pw;
int	debug;
int	timeout = 900;    /* timeout after 15 minutes of inactivity */
int	maxtimeout = 7200;/* don't allow idle time to be set beyond 2 hours */
int	logging;
int	restricted_data_ports = 1;
int	paranoid = 1;	  /* be extra careful about security */
int	anon_only = 0;    /* Only anonymous ftp allowed */
int	guest;
int	dochroot;
int	stats;
int	statfd = -1;
int	type;
int	form;
int	stru;			/* avoid C keyword */
int	mode;
int	usedefault = 1;		/* for data transfers */
int	pdata = -1;		/* for passive mode */
sig_atomic_t transflag;
off_t	file_size;
off_t	byte_count;
#if !defined(CMASK) || CMASK == 0
#undef CMASK
#define CMASK 027
#endif
int	defumask = CMASK;		/* default umask value */
char	tmpline[7];
char	*hostname;
#ifdef VIRTUAL_HOSTING
char	*ftpuser;

static struct ftphost {
	struct ftphost	*next;
	struct in_addr	hostaddr;
	char		*hostname;
	char		*anonuser;
	char		*statfile;
	char		*welcome;
	char		*loginmsg;
} *thishost, *firsthost;

#endif
char	remotehost[MAXHOSTNAMELEN];
char	*ident = NULL;

static char ttyline[20];
char	*tty = ttyline;		/* for klogin */

#ifdef KERBEROS
int	 klogin __P((struct passwd *, char *, char *, char *));
#endif

struct	in_addr bind_address;
char	*pid_file = NULL;

#if defined(KERBEROS)
int	notickets = 1;
int	noticketsdontcomplain = 1;
char	*krbtkfile_env = NULL;
#endif

/*
 * Timeout intervals for retrying connections
 * to hosts that don't accept PORT cmds.  This
 * is a kludge, but given the problems with TCP...
 */
#define	SWAITMAX	90	/* wait at most 90 seconds */
#define	SWAITINT	5	/* interval between retries */

int	swaitmax = SWAITMAX;
int	swaitint = SWAITINT;

#ifdef SETPROCTITLE
#ifdef OLD_SETPROCTITLE
char	**Argv = NULL;		/* pointer to argument vector */
char	*LastArgv = NULL;	/* end of argv */
#endif /* OLD_SETPROCTITLE */
char	proctitle[LINE_MAX];	/* initial part of title */
#endif /* SETPROCTITLE */

#ifdef SKEY
int	pwok = 0;
char	addr_string[20];	/* XXX */
#endif

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
		    syslog(LOG_INFO, "%s %s%s = %qd bytes", \
			cmd, (*(file) == '/') ? "" : curdir(), file, cnt); \
	}

#ifdef VIRTUAL_HOSTING
static void	 inithosts __P((void));
static void	selecthost __P((struct in_addr *));
#endif
static void	 ack __P((char *));
static void	 myoob __P((int));
static int	 checkuser __P((char *, char *));
static FILE	*dataconn __P((char *, off_t, char *));
static void	 dolog __P((struct sockaddr_in *));
static char	*curdir __P((void));
static void	 end_login __P((void));
static FILE	*getdatasock __P((char *));
static char	*gunique __P((char *));
static void	 lostconn __P((int));
static int	 receive_data __P((FILE *, FILE *));
static void	 send_data __P((FILE *, FILE *, off_t, off_t, int));
static struct passwd *
		 sgetpwnam __P((char *));
static char	*sgetsave __P((char *));
static void	 reapchild __P((int));
static void      logxfer __P((char *, long, long));

static char *
curdir()
{
	static char path[MAXPATHLEN+1+1];	/* path + '/' + '\0' */

	if (getcwd(path, sizeof(path)-2) == NULL)
		return ("");
	if (path[1] != '\0')		/* special case for root dir. */
		strcat(path, "/");
	/* For guest account, skip / since it's chrooted */
	return (guest ? path+1 : path);
}

int
main(argc, argv, envp)
	int argc;
	char *argv[];
	char **envp;
{
	int addrlen, ch, on = 1, tos;
	char *cp, line[LINE_MAX];
	FILE *fd;

	tzset();		/* in case no timezone database in ~ftp */

#ifdef OLD_SETPROCTITLE
	/*
	 *  Save start and extent of argv for setproctitle.
	 */
	Argv = argv;
	while (*envp)
		envp++;
	LastArgv = envp[-1] + strlen(envp[-1]);
#endif /* OLD_SETPROCTITLE */


	bind_address.s_addr = htonl(INADDR_ANY);
	while ((ch = getopt(argc, argv, "AdlDSURt:T:u:va:p:")) != -1) {
		switch (ch) {
		case 'D':
			daemon_mode++;
			break;

		case 'd':
			debug++;
			break;

		case 'l':
			logging++;	/* > 1 == extra logging */
			break;

		case 'R':
			paranoid = 0;
			break;

		case 'S':
			stats++;
			break;

		case 'T':
			maxtimeout = atoi(optarg);
			if (timeout > maxtimeout)
				timeout = maxtimeout;
			break;

		case 't':
			timeout = atoi(optarg);
			if (maxtimeout < timeout)
				maxtimeout = timeout;
			break;

		case 'U':
			restricted_data_ports = 0;
			break;

		case 'a':
			if (!inet_aton(optarg, &bind_address))
				errx(1, "invalid address for -a");
			break;

		case 'p':
			pid_file = optarg;
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
		case 'A':
			anon_only = 1;
			break;

		case 'v':
			debug = 1;
			break;

		default:
			warnx("unknown flag -%c ignored", optopt);
			break;
		}
	}

#ifdef VIRTUAL_HOSTING
	inithosts();
#endif
	(void) freopen(_PATH_DEVNULL, "w", stderr);

	/*
	 * LOG_NDELAY sets up the logging connection immediately,
	 * necessary for anonymous ftp's that chroot and can't do it later.
	 */
	openlog("ftpd", LOG_PID | LOG_NDELAY, LOG_FTP);

	if (daemon_mode) {
		int ctl_sock, fd;
		struct servent *sv;

		/*
		 * Detach from parent.
		 */
		if (daemon(1, 1) < 0) {
			syslog(LOG_ERR, "failed to become a daemon");
			exit(1);
		}
		(void) signal(SIGCHLD, reapchild);
		/*
		 * Get port number for ftp/tcp.
		 */
		sv = getservbyname("ftp", "tcp");
		if (sv == NULL) {
			syslog(LOG_ERR, "getservbyname for ftp failed");
			exit(1);
		}
		/*
		 * Open a socket, bind it to the FTP port, and start
		 * listening.
		 */
		ctl_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (ctl_sock < 0) {
			syslog(LOG_ERR, "control socket: %m");
			exit(1);
		}
		if (setsockopt(ctl_sock, SOL_SOCKET, SO_REUSEADDR,
		    (char *)&on, sizeof(on)) < 0)
			syslog(LOG_ERR, "control setsockopt: %m");;
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr = bind_address;
		server_addr.sin_port = sv->s_port;
		if (bind(ctl_sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
			syslog(LOG_ERR, "control bind: %m");
			exit(1);
		}
		if (listen(ctl_sock, 32) < 0) {
			syslog(LOG_ERR, "control listen: %m");
			exit(1);
		}
		/*
		 * Atomically write process ID
		 */
		if (pid_file)
		{   
			int fd;
			char buf[20];

			fd = open(pid_file, O_CREAT | O_WRONLY | O_TRUNC
				| O_NONBLOCK | O_EXLOCK, 0644);
			if (fd < 0)
				if (errno == EAGAIN)
					errx(1, "%s: file locked", pid_file);
				else
					err(1, "%s", pid_file);
			snprintf(buf, sizeof(buf),
				"%lu\n", (unsigned long) getpid());
			if (write(fd, buf, strlen(buf)) < 0)
				err(1, "%s: write", pid_file);
			/* Leave the pid file open and locked */
		}
		/*
		 * Loop forever accepting connection requests and forking off
		 * children to handle them.
		 */
		while (1) {
			addrlen = sizeof(his_addr);
			fd = accept(ctl_sock, (struct sockaddr *)&his_addr, &addrlen);
			if (fork() == 0) {
				/* child */
				(void) dup2(fd, 0);
				(void) dup2(fd, 1);
				close(ctl_sock);
				break;
			}
			close(fd);
		}
	} else {
		addrlen = sizeof(his_addr);
		if (getpeername(0, (struct sockaddr *)&his_addr, &addrlen) < 0) {
			syslog(LOG_ERR, "getpeername (%s): %m",argv[0]);
			exit(1);
		}
	}

	(void) signal(SIGCHLD, SIG_IGN);
	(void) signal(SIGPIPE, lostconn);
	if ((int)signal(SIGURG, myoob) < 0)
		syslog(LOG_ERR, "signal: %m");

#ifdef SKEY
	strncpy(addr_string, inet_ntoa(his_addr.sin_addr), sizeof(addr_string));
#endif
	addrlen = sizeof(ctrl_addr);
	if (getsockname(0, (struct sockaddr *)&ctrl_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getsockname (%s): %m",argv[0]);
		exit(1);
	}
#ifdef VIRTUAL_HOSTING
	/* select our identity from virtual host table */
	selecthost(&ctrl_addr.sin_addr);
#endif
#ifdef IP_TOS
	tos = IPTOS_LOWDELAY;
	if (setsockopt(0, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif
	data_source.sin_port = htons(ntohs(ctrl_addr.sin_port) - 1);

	/* set this here so klogin can use it... */
	(void)sprintf(ttyline, "ftp%d", getpid());

	/* Try to handle urgent data inline */
#ifdef SO_OOBINLINE
	if (setsockopt(0, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on)) < 0)
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
		(void) fflush(stdout);
		(void) fclose(fd);
		reply(530, "System not available.");
		exit(0);
	}
#ifdef VIRTUAL_HOSTING
	if ((fd = fopen(thishost->welcome, "r")) != NULL) {
#else
	if ((fd = fopen(_PATH_FTPWELCOME, "r")) != NULL) {
#endif
		while (fgets(line, sizeof(line), fd) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			lreply(220, "%s", line);
		}
		(void) fflush(stdout);
		(void) fclose(fd);
		/* reply(220,) must follow */
	}
#ifndef VIRTUAL_HOSTING
	if ((hostname = malloc(MAXHOSTNAMELEN)) == NULL)
		fatal("Ran out of memory.");
	(void) gethostname(hostname, MAXHOSTNAMELEN);
#endif
	reply(220, "%s FTP server (%s) ready.", hostname, version);
	(void) setjmp(errcatch);
	for (;;)
		(void) yyparse();
	/* NOTREACHED */
}

static void
lostconn(signo)
	int signo;
{

	if (debug)
		syslog(LOG_DEBUG, "lost connection");
	dologout(-1);
}

#ifdef VIRTUAL_HOSTING
/*
 * read in virtual host tables (if they exist)
 */

static void
inithosts()
{
	FILE *fp;
	char *cp;
	struct hostent *hp;
	struct ftphost *hrp, *lhrp;
	char line[1024];

	/*
	 * Fill in the default host information
	 */
	if (gethostname(line, sizeof(line)) < 0)
		line[0] = '\0';
	if ((hrp = malloc(sizeof(struct ftphost))) == NULL ||
	    (hrp->hostname = strdup(line)) == NULL)
		fatal("Ran out of memory.");
	memset(&hrp->hostaddr, 0, sizeof hrp->hostaddr);
	if ((hp = gethostbyname(hrp->hostname)) != NULL)
		(void) memcpy(&hrp->hostaddr,
			      hp->h_addr_list[0],
			      sizeof(hrp->hostaddr));
	hrp->statfile = _PATH_FTPDSTATFILE;
	hrp->welcome  = _PATH_FTPWELCOME;
	hrp->loginmsg = _PATH_FTPLOGINMESG;
	hrp->anonuser = "ftp";
	hrp->next = NULL;
	thishost = firsthost = lhrp = hrp;
	if ((fp = fopen(_PATH_FTPHOSTS, "r")) != NULL) {
		while (fgets(line, sizeof(line), fp) != NULL) {
			int	i;

			if ((cp = strchr(line, '\n')) == NULL) {
				/* ignore long lines */
				while (fgets(line, sizeof(line), fp) != NULL &&
					strchr(line, '\n') == NULL)
					;
				continue;
			}
			*cp = '\0';
			cp = strtok(line, " \t");
			/* skip comments and empty lines */
			if (cp == NULL || line[0] == '#')
				continue;
			/* first, try a standard gethostbyname() */
			if ((hp = gethostbyname(cp)) == NULL)
				continue;
			for (hrp = firsthost; hrp != NULL; hrp = hrp->next) {
				if (memcmp(&hrp->hostaddr,
					   hp->h_addr_list[0],
					   sizeof(hrp->hostaddr)) == 0)
					break;
			}
			if (hrp == NULL) {
				if ((hrp = malloc(sizeof(struct ftphost))) == NULL)
					continue;
				/* defaults */
				hrp->statfile = _PATH_FTPDSTATFILE;
				hrp->welcome  = _PATH_FTPWELCOME;
				hrp->loginmsg = _PATH_FTPLOGINMESG;
				hrp->anonuser = "ftp";
				hrp->next     = NULL;
				lhrp->next = hrp;
				lhrp = hrp;
			}
			(void) memcpy(&hrp->hostaddr,
				      hp->h_addr_list[0],
				      sizeof(hrp->hostaddr));
			/*
			 * determine hostname to use.
			 * force defined name if it is a valid alias
			 * otherwise fallback to primary hostname
			 */
			if ((hp = gethostbyaddr((char*)&hrp->hostaddr,
						sizeof(hrp->hostaddr),
						AF_INET)) != NULL) {
				if (strcmp(cp, hp->h_name) != 0) {
					if (hp->h_aliases == NULL)
						cp = hp->h_name;
					else {
						i = 0;
						while (hp->h_aliases[i] &&
						       strcmp(cp, hp->h_aliases[i]) != 0)
							++i;
						if (hp->h_aliases[i] == NULL)
							cp = hp->h_name;
					}
				}
			}
			hrp->hostname = strdup(cp);
			/* ok, now we now peel off the rest */
			i = 0;
			while (i < 4 && (cp = strtok(NULL, " \t")) != NULL) {
				if (*cp != '-' && (cp = strdup(cp)) != NULL) {
					switch (i) {
					case 0:	/* anon user permissions */
						hrp->anonuser = cp;
						break;
					case 1: /* statistics file */
						hrp->statfile = cp;
						break;
					case 2: /* welcome message */
						hrp->welcome  = cp;
						break;
					case 3: /* login message */
						hrp->loginmsg = cp;
						break;
					}
				}
				++i;
			}
		}
		(void) fclose(fp);
	}
}

static void
selecthost(a)
	struct in_addr *a;
{
	struct ftphost	*hrp;

	hrp = thishost = firsthost;	/* default */
	while (hrp != NULL) {
		if (memcmp(a, &hrp->hostaddr, sizeof(hrp->hostaddr)) == 0) {
			thishost = hrp;
			break;
		}
		hrp = hrp->next;
	}
	/* setup static variables as appropriate */
	hostname = thishost->hostname;
	ftpuser = thishost->anonuser;
}
#endif

/*
 * Helper function for sgetpwnam().
 */
static char *
sgetsave(s)
	char *s;
{
	char *new = malloc((unsigned) strlen(s) + 1);

	if (new == NULL) {
		perror_reply(421, "Local resource failure: malloc");
		dologout(1);
		/* NOTREACHED */
	}
	(void) strcpy(new, s);
	return (new);
}

/*
 * Save the result of a getpwnam.  Used for USER command, since
 * the data returned must not be clobbered by any other command
 * (e.g., globbing).
 */
static struct passwd *
sgetpwnam(name)
	char *name;
{
	static struct passwd save;
	struct passwd *p;

	if ((p = getpwnam(name)) == NULL)
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
user(name)
	char *name;
{
	char *cp, *shell;

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
		if (checkuser(_PATH_FTPUSERS, "ftp") ||
		    checkuser(_PATH_FTPUSERS, "anonymous"))
			reply(530, "User %s access denied.", name);
#ifdef VIRTUAL_HOSTING
		else if ((pw = sgetpwnam(thishost->anonuser)) != NULL) {
#else
		else if ((pw = sgetpwnam("ftp")) != NULL) {
#endif
			guest = 1;
			askpasswd = 1;
			reply(331,
			"Guest login ok, send your email address as password.");
		} else
			reply(530, "User %s unknown.", name);
		if (!askpasswd && logging)
			syslog(LOG_NOTICE,
			    "ANONYMOUS FTP LOGIN REFUSED FROM %s", remotehost);
		return;
	}
	if (anon_only != 0) {
		reply(530, "Sorry, only anonymous ftp allowed.");
		return;
	}
		
	if ((pw = sgetpwnam(name))) {
		if ((shell = pw->pw_shell) == NULL || *shell == 0)
			shell = _PATH_BSHELL;
		while ((cp = getusershell()) != NULL)
			if (strcmp(cp, shell) == 0)
				break;
		endusershell();

		if (cp == NULL || checkuser(_PATH_FTPUSERS, name)) {
			reply(530, "User %s access denied.", name);
			if (logging)
				syslog(LOG_NOTICE,
				    "FTP LOGIN REFUSED FROM %s, %s",
				    remotehost, name);
			pw = (struct passwd *) NULL;
			return;
		}
	}
	if (logging)
		strncpy(curname, name, sizeof(curname)-1);
#ifdef SKEY
	pwok = skeyaccess(name, NULL, remotehost, addr_string);
	reply(331, "%s", skey_challenge(name, pw, pwok));
#else
	reply(331, "Password required for %s.", name);
#endif
	askpasswd = 1;
	/*
	 * Delay before reading passwd after first failed
	 * attempt to slow down passwd-guessing programs.
	 */
	if (login_attempts)
		sleep((unsigned) login_attempts);
}

/*
 * Check if a user is in the file "fname"
 */
static int
checkuser(fname, name)
	char *fname;
	char *name;
{
	FILE *fd;
	int found = 0;
	char *p, line[BUFSIZ];

	if ((fd = fopen(fname, "r")) != NULL) {
		while (!found && fgets(line, sizeof(line), fd) != NULL)
			if ((p = strchr(line, '\n')) != NULL) {
				*p = '\0';
				if (line[0] == '#')
					continue;
				/*
				 * if first chr is '@', check group membership
				 */
				if (line[0] == '@') {
					int i = 0;
					struct group *grp;

					if ((grp = getgrnam(line+1)) == NULL)
						continue;
					while (!found && grp->gr_mem[i])
						found = strcmp(name,
							grp->gr_mem[i++])
							== 0;
				}
				/*
				 * Otherwise, just check for username match
				 */
				else
					found = strcmp(line, name) == 0;
			}
		(void) fclose(fd);
	}
	return (found);
}

/*
 * Terminate login as previous user, if any, resetting state;
 * used when USER command is given or login fails.
 */
static void
end_login()
{

	(void) seteuid((uid_t)0);
	if (logged_in)
		ftpd_logwtmp(ttyline, "", "");
	pw = NULL;
#ifdef	LOGIN_CAP
	setusercontext(NULL, getpwuid(0), (uid_t)0,
		       LOGIN_SETPRIORITY|LOGIN_SETRESOURCES|LOGIN_SETUMASK);
#endif
	logged_in = 0;
	guest = 0;
	dochroot = 0;
}

void
pass(passwd)
	char *passwd;
{
	int rval;
	FILE *fd;
#ifdef	LOGIN_CAP
	login_cap_t *lc = NULL;
#endif
	static char homedir[MAXPATHLEN];

	if (logged_in || askpasswd == 0) {
		reply(503, "Login with USER first.");
		return;
	}
	askpasswd = 0;
	if (!guest) {		/* "ftp" is only account allowed no password */
		if (pw == NULL) {
			rval = 1;	/* failure below */
			goto skip;
		}
#if defined(KERBEROS)
		rval = klogin(pw, "", hostname, passwd);
		if (rval == 0)
			goto skip;
#endif
#ifdef SKEY
		rval = strcmp(skey_crypt(passwd, pw->pw_passwd, pw, pwok),
			      pw->pw_passwd);
		pwok = 0;
#else
		rval = strcmp(crypt(passwd, pw->pw_passwd), pw->pw_passwd);
#endif
		/* The strcmp does not catch null passwords! */
		if (*pw->pw_passwd == '\0' ||
		    (pw->pw_expire && time(NULL) >= pw->pw_expire))
			rval = 1;	/* failure */
skip:
		/*
		 * If rval == 1, the user failed the authentication check
		 * above.  If rval == 0, either Kerberos or local authentication
		 * succeeded.
		 */
		if (rval) {
			reply(530, "Login incorrect.");
			if (logging)
				syslog(LOG_NOTICE,
				    "FTP LOGIN FAILED FROM %s, %s",
				    remotehost, curname);
			pw = NULL;
			if (login_attempts++ >= 5) {
				syslog(LOG_NOTICE,
				    "repeated login failures from %s",
				    remotehost);
				exit(0);
			}
			return;
		}
	}
	login_attempts = 0;		/* this time successful */
	if (setegid((gid_t)pw->pw_gid) < 0) {
		reply(550, "Can't set gid.");
		return;
	}
	/* May be overridden by login.conf */
	(void) umask(defumask);
#ifdef	LOGIN_CAP
	if ((lc = login_getpwclass(pw)) != NULL) {
		char	remote_ip[MAXHOSTNAMELEN];

		strncpy(remote_ip, inet_ntoa(his_addr.sin_addr),
			sizeof(remote_ip) - 1);
		remote_ip[sizeof(remote_ip) - 1] = 0;
		if (!auth_hostok(lc, remotehost, remote_ip)) {
			syslog(LOG_INFO|LOG_AUTH,
			    "FTP LOGIN FAILED (HOST) as %s: permission denied.",
			    pw->pw_name);
			reply(530, "Permission denied.\n");
			pw = NULL;
			return;
		}
		if (!auth_timeok(lc, time(NULL))) {
			reply(530, "Login not available right now.\n");
			pw = NULL;
			return;
		}
	}
	setusercontext(lc, pw, (uid_t)0,
	LOGIN_SETGROUP|LOGIN_SETPRIORITY|LOGIN_SETRESOURCES|LOGIN_SETUMASK);
#else
	(void) initgroups(pw->pw_name, pw->pw_gid);
#endif

	/* open wtmp before chroot */
	ftpd_logwtmp(ttyline, pw->pw_name, remotehost);
	logged_in = 1;

	if (guest && stats && statfd < 0)
#ifdef VIRTUAL_HOSTING
		if ((statfd = open(thishost->statfile, O_WRONLY|O_APPEND)) < 0)
#else
		if ((statfd = open(_PATH_FTPDSTATFILE, O_WRONLY|O_APPEND)) < 0)
#endif
			stats = 0;

	dochroot =
#ifdef	LOGIN_CAP	/* Allow login.conf configuration as well */
		login_getcapbool(lc, "ftp-chroot", 0) ||
#endif
		checkuser(_PATH_FTPCHROOT, pw->pw_name);
	if (guest) {
		/*
		 * We MUST do a chdir() after the chroot. Otherwise
		 * the old current directory will be accessible as "."
		 * outside the new root!
		 */
		if (chroot(pw->pw_dir) < 0 || chdir("/") < 0) {
			reply(550, "Can't set guest privileges.");
			goto bad;
		}
	} else if (dochroot) {
		if (chroot(pw->pw_dir) < 0 || chdir("/") < 0) {
			reply(550, "Can't change root.");
			goto bad;
		}
	} else if (chdir(pw->pw_dir) < 0) {
		if (chdir("/") < 0) {
			reply(530, "User %s: can't change directory to %s.",
			    pw->pw_name, pw->pw_dir);
			goto bad;
		} else
			lreply(230, "No directory! Logging in with home=/");
	}
	if (seteuid((uid_t)pw->pw_uid) < 0) {
		reply(550, "Can't set uid.");
		goto bad;
	}

	/*
	 * Set home directory so that use of ~ (tilde) works correctly.
	 */
	if (getcwd(homedir, MAXPATHLEN) != NULL)
		setenv("HOME", homedir, 1);

	/*
	 * Display a login message, if it exists.
	 * N.B. reply(230,) must follow the message.
	 */
#ifdef VIRTUAL_HOSTING
	if ((fd = fopen(thishost->loginmsg, "r")) != NULL) {
#else
	if ((fd = fopen(_PATH_FTPLOGINMESG, "r")) != NULL) {
#endif
		char *cp, line[LINE_MAX];

		while (fgets(line, sizeof(line), fd) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			lreply(230, "%s", line);
		}
		(void) fflush(stdout);
		(void) fclose(fd);
	}
	if (guest) {
		if (ident != NULL)
			free(ident);
		ident = strdup(passwd);
		if (ident == NULL)
			fatal("Ran out of memory.");

		reply(230, "Guest login ok, access restrictions apply.");
#ifdef SETPROCTITLE
#ifdef VIRTUAL_HOSTING
		if (thishost != firsthost)
			snprintf(proctitle, sizeof(proctitle),
				 "%s: anonymous(%s)/%.*s", remotehost, hostname,
				 sizeof(proctitle) - sizeof(remotehost) -
				 sizeof(": anonymous/"), passwd);
		else
#endif
			snprintf(proctitle, sizeof(proctitle),
				 "%s: anonymous/%.*s", remotehost,
				 sizeof(proctitle) - sizeof(remotehost) -
				 sizeof(": anonymous/"), passwd);
		setproctitle("%s", proctitle);
#endif /* SETPROCTITLE */
		if (logging)
			syslog(LOG_INFO, "ANONYMOUS FTP LOGIN FROM %s, %s",
			    remotehost, passwd);
	} else {
	    if (dochroot)
		reply(230, "User %s logged in, access restrictions apply.", 
			pw->pw_name);
	    else
		reply(230, "User %s logged in.", pw->pw_name);

#ifdef SETPROCTITLE
		snprintf(proctitle, sizeof(proctitle),
			 "%s: %s", remotehost, pw->pw_name);
		setproctitle("%s", proctitle);
#endif /* SETPROCTITLE */
		if (logging)
			syslog(LOG_INFO, "FTP LOGIN FROM %s as %s",
			    remotehost, pw->pw_name);
	}
#ifdef	LOGIN_CAP
	login_close(lc);
#endif
	return;
bad:
	/* Forget all about it... */
#ifdef	LOGIN_CAP
	login_close(lc);
#endif
	end_login();
}

void
retrieve(cmd, name)
	char *cmd, *name;
{
	FILE *fin, *dout;
	struct stat st;
	int (*closefunc) __P((FILE *));
	long start;

	if (cmd == 0) {
		fin = fopen(name, "r"), closefunc = fclose;
		st.st_size = 0;
	} else {
		char line[BUFSIZ];

		(void) sprintf(line, cmd, name), name = line;
		fin = ftpd_popen(line, "r"), closefunc = ftpd_pclose;
		st.st_size = -1;
		st.st_blksize = BUFSIZ;
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
	if (cmd == 0 && (fstat(fileno(fin), &st) < 0 || !S_ISREG(st.st_mode))) {
		reply(550, "%s: not a plain file.", name);
		goto done;
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
		} else if (lseek(fileno(fin), restart_point, L_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	dout = dataconn(name, st.st_size, "w");
	if (dout == NULL)
		goto done;
	time(&start);
	send_data(fin, dout, st.st_blksize, st.st_size,
		  restart_point == 0 && cmd == 0 && S_ISREG(st.st_mode));
	if (cmd == 0 && guest && stats)
		logxfer(name, st.st_size, start);
	(void) fclose(dout);
	data = -1;
	pdata = -1;
done:
	if (cmd == 0)
		LOGBYTES("get", name, byte_count);
	(*closefunc)(fin);
}

void
store(name, mode, unique)
	char *name, *mode;
	int unique;
{
	FILE *fout, *din;
	struct stat st;
	int (*closefunc) __P((FILE *));

	if ((unique || guest) && stat(name, &st) == 0 &&
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
			if (fseek(fout, 0L, L_INCR) < 0) {
				perror_reply(550, name);
				goto done;
			}
		} else if (lseek(fileno(fout), restart_point, L_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	din = dataconn(name, (off_t)-1, "r");
	if (din == NULL)
		goto done;
	if (receive_data(din, fout) == 0) {
		if (unique)
			reply(226, "Transfer complete (unique file name:%s).",
			    name);
		else
			reply(226, "Transfer complete.");
	}
	(void) fclose(din);
	data = -1;
	pdata = -1;
done:
	LOGBYTES(*mode == 'w' ? "put" : "append", name, byte_count);
	(*closefunc)(fout);
}

static FILE *
getdatasock(mode)
	char *mode;
{
	int on = 1, s, t, tries;

	if (data >= 0)
		return (fdopen(data, mode));
	(void) seteuid((uid_t)0);
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		goto bad;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	    (char *) &on, sizeof(on)) < 0)
		goto bad;
	/* anchor socket to avoid multi-homing problems */
	data_source.sin_len = sizeof(struct sockaddr_in);
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
	(void) seteuid((uid_t)pw->pw_uid);
#ifdef IP_TOS
	on = IPTOS_THROUGHPUT;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int)) < 0)
		syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif
#ifdef TCP_NOPUSH
	/*
	 * Turn off push flag to keep sender TCP from sending short packets
	 * at the boundaries of each write().  Should probably do a SO_SNDBUF
	 * to set the send buffer size as well, but that may not be desirable
	 * in heavy-load situations.
	 */
	on = 1;
	if (setsockopt(s, IPPROTO_TCP, TCP_NOPUSH, (char *)&on, sizeof on) < 0)
		syslog(LOG_WARNING, "setsockopt (TCP_NOPUSH): %m");
#endif
#ifdef SO_SNDBUF
	on = 65536;
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&on, sizeof on) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_SNDBUF): %m");
#endif

	return (fdopen(s, mode));
bad:
	/* Return the real value of errno (close may change it) */
	t = errno;
	(void) seteuid((uid_t)pw->pw_uid);
	(void) close(s);
	errno = t;
	return (NULL);
}

static FILE *
dataconn(name, size, mode)
	char *name;
	off_t size;
	char *mode;
{
	char sizebuf[32];
	FILE *file;
	int retry = 0, tos;

	file_size = size;
	byte_count = 0;
	if (size != (off_t) -1)
		(void) sprintf(sizebuf, " (%qd bytes)", size);
	else
		(void) strcpy(sizebuf, "");
	if (pdata >= 0) {
		struct sockaddr_in from;
		int s, fromlen = sizeof(from);
		struct timeval timeout;
		fd_set set;

		FD_ZERO(&set);
		FD_SET(pdata, &set);

		timeout.tv_usec = 0;
		timeout.tv_sec = 120;

		if (select(pdata+1, &set, (fd_set *) 0, (fd_set *) 0, &timeout) == 0 ||
		    (s = accept(pdata, (struct sockaddr *) &from, &fromlen)) < 0) {
			reply(425, "Can't open data connection.");
			(void) close(pdata);
			pdata = -1;
			return (NULL);
		}
		(void) close(pdata);
		pdata = s;
#ifdef IP_TOS
		tos = IPTOS_THROUGHPUT;
		(void) setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos,
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
		(void) fclose(file);
		data = -1;
		return (NULL);
	}
	reply(150, "Opening %s mode data connection for '%s'%s.",
	     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
	return (file);
}

/*
 * Tranfer the contents of "instr" to "outstr" peer using the appropriate
 * encapsulation of the data subject to Mode, Structure, and Type.
 *
 * NB: Form isn't handled.
 */
static void
send_data(instr, outstr, blksize, filesize, isreg)
	FILE *instr, *outstr;
	off_t blksize;
	off_t filesize;
	int isreg;
{
	int c, cnt, filefd, netfd;
	char *buf, *bp;
	size_t len;

	transflag++;
	if (setjmp(urgcatch)) {
		transflag = 0;
		return;
	}
	switch (type) {

	case TYPE_A:
		while ((c = getc(instr)) != EOF) {
			byte_count++;
			if (c == '\n') {
				if (ferror(outstr))
					goto data_err;
				(void) putc('\r', outstr);
			}
			(void) putc(c, outstr);
		}
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
		/*
		 * isreg is only set if we are not doing restart and we
		 * are sending a regular file
		 */
		netfd = fileno(outstr);
		filefd = fileno(instr);

		if (isreg && filesize < (off_t)16 * 1024 * 1024) {
			buf = mmap(0, filesize, PROT_READ, MAP_SHARED, filefd,
				   (off_t)0);
			if (buf == MAP_FAILED) {
				syslog(LOG_WARNING, "mmap(%lu): %m",
				       (unsigned long)filesize);
				goto oldway;
			}
			bp = buf;
			len = filesize;
			do {
				cnt = write(netfd, bp, len);
				len -= cnt;
				bp += cnt;
				if (cnt > 0) byte_count += cnt;
			} while(cnt > 0 && len > 0);

			transflag = 0;
			munmap(buf, (size_t)filesize);
			if (cnt < 0)
				goto data_err;
			reply(226, "Transfer complete.");
			return;
		}

oldway:
		if ((buf = malloc((u_int)blksize)) == NULL) {
			transflag = 0;
			perror_reply(451, "Local resource failure: malloc");
			return;
		}

		while ((cnt = read(filefd, buf, (u_int)blksize)) > 0 &&
		    write(netfd, buf, cnt) == cnt)
			byte_count += cnt;
		transflag = 0;
		(void)free(buf);
		if (cnt != 0) {
			if (cnt < 0)
				goto file_err;
			goto data_err;
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
receive_data(instr, outstr)
	FILE *instr, *outstr;
{
	int c;
	int cnt, bare_lfs;
	char buf[BUFSIZ];

	transflag++;
	if (setjmp(urgcatch)) {
		transflag = 0;
		return (-1);
	}

	bare_lfs = 0;

	switch (type) {

	case TYPE_I:
	case TYPE_L:
		while ((cnt = read(fileno(instr), buf, sizeof(buf))) > 0) {
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
		while ((c = getc(instr)) != EOF) {
			byte_count++;
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				if (ferror(outstr))
					goto data_err;
				if ((c = getc(instr)) != '\n') {
					(void) putc ('\r', outstr);
					if (c == '\0' || c == EOF)
						goto contin2;
				}
			}
			(void) putc(c, outstr);
	contin2:	;
		}
		fflush(outstr);
		if (ferror(instr))
			goto data_err;
		if (ferror(outstr))
			goto file_err;
		transflag = 0;
		if (bare_lfs) {
			lreply(226,
		"WARNING! %d bare linefeeds received in ASCII mode",
			    bare_lfs);
		(void)printf("   File may not have transferred correctly.\r\n");
		}
		return (0);
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
statfilecmd(filename)
	char *filename;
{
	FILE *fin;
	int c;
	char line[LINE_MAX];

	(void)snprintf(line, sizeof(line), _PATH_LS " -lgA %s", filename);
	fin = ftpd_popen(line, "r");
	lreply(211, "status of %s:", filename);
	while ((c = getc(fin)) != EOF) {
		if (c == '\n') {
			if (ferror(stdout)){
				perror_reply(421, "control connection");
				(void) ftpd_pclose(fin);
				dologout(1);
				/* NOTREACHED */
			}
			if (ferror(fin)) {
				perror_reply(551, filename);
				(void) ftpd_pclose(fin);
				return;
			}
			(void) putc('\r', stdout);
		}
		(void) putc(c, stdout);
	}
	(void) ftpd_pclose(fin);
	reply(211, "End of Status");
}

void
statcmd()
{
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
	reply(211, "End of status");
}

void
fatal(s)
	char *s;
{

	reply(451, "Error in server: %s\n", s);
	reply(221, "Closing connection due to server error.");
	dologout(0);
	/* NOTREACHED */
}

void
#if __STDC__
reply(int n, const char *fmt, ...)
#else
reply(n, fmt, va_alist)
	int n;
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)printf("%d ", n);
	(void)vprintf(fmt, ap);
	(void)printf("\r\n");
	(void)fflush(stdout);
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d ", n);
		vsyslog(LOG_DEBUG, fmt, ap);
	}
}

void
#if __STDC__
lreply(int n, const char *fmt, ...)
#else
lreply(n, fmt, va_alist)
	int n;
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)printf("%d- ", n);
	(void)vprintf(fmt, ap);
	(void)printf("\r\n");
	(void)fflush(stdout);
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d- ", n);
		vsyslog(LOG_DEBUG, fmt, ap);
	}
}

static void
ack(s)
	char *s;
{

	reply(250, "%s command successful.", s);
}

void
nack(s)
	char *s;
{

	reply(502, "%s command not implemented.", s);
}

/* ARGSUSED */
void
yyerror(s)
	char *s;
{
	char *cp;

	if ((cp = strchr(cbuf,'\n')))
		*cp = '\0';
	reply(500, "'%s': command not understood.", cbuf);
}

void
delete(name)
	char *name;
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
cwd(path)
	char *path;
{

	if (chdir(path) < 0)
		perror_reply(550, path);
	else
		ack("CWD");
}

void
makedir(name)
	char *name;
{

	LOGCMD("mkdir", name);
	if (mkdir(name, 0777) < 0)
		perror_reply(550, name);
	else
		reply(257, "MKD command successful.");
}

void
removedir(name)
	char *name;
{

	LOGCMD("rmdir", name);
	if (rmdir(name) < 0)
		perror_reply(550, name);
	else
		ack("RMD");
}

void
pwd()
{
	char path[MAXPATHLEN + 1];

	if (getwd(path) == (char *)NULL)
		reply(550, "%s.", path);
	else
		reply(257, "\"%s\" is current directory.", path);
}

char *
renamefrom(name)
	char *name;
{
	struct stat st;

	if (stat(name, &st) < 0) {
		perror_reply(550, name);
		return ((char *)0);
	}
	reply(350, "File exists, ready for destination name");
	return (name);
}

void
renamecmd(from, to)
	char *from, *to;
{
	struct stat st;

	LOGCMD2("rename", from, to);

	if (guest && (stat(to, &st) == 0)) {
		reply(550, "%s: permission denied", to);
		return;
	}

	if (rename(from, to) < 0)
		perror_reply(550, "rename");
	else
		ack("RNTO");
}

static void
dolog(sin)
	struct sockaddr_in *sin;
{
	struct hostent *hp = gethostbyaddr((char *)&sin->sin_addr,
		sizeof(struct in_addr), AF_INET);

	if (hp)
		(void) strncpy(remotehost, hp->h_name, sizeof(remotehost));
	else
		(void) strncpy(remotehost, inet_ntoa(sin->sin_addr),
		    sizeof(remotehost));
#ifdef SETPROCTITLE
#ifdef VIRTUAL_HOSTING
	if (thishost != firsthost)
		snprintf(proctitle, sizeof(proctitle), "%s: connected (to %s)",
			 remotehost, hostname);
	else
#endif
		snprintf(proctitle, sizeof(proctitle), "%s: connected",
			 remotehost);
	setproctitle("%s", proctitle);
#endif /* SETPROCTITLE */

	if (logging) {
#ifdef VIRTUAL_HOSTING
		if (thishost != firsthost)
			syslog(LOG_INFO, "connection from %s (to %s)",
			       remotehost, hostname);
		else
#endif
			syslog(LOG_INFO, "connection from %s", remotehost);
	}
}

/*
 * Record logout in wtmp file
 * and exit with supplied status.
 */
void
dologout(status)
	int status;
{
	/*
	 * Prevent reception of SIGURG from resulting in a resumption
	 * back to the main program loop.
	 */
	transflag = 0;

	if (logged_in) {
		(void) seteuid((uid_t)0);
		ftpd_logwtmp(ttyline, "", "");
#if defined(KERBEROS)
		if (!notickets && krbtkfile_env)
			unlink(krbtkfile_env);
#endif
	}
	/* beware of flushing buffers after a SIGPIPE */
	_exit(status);
}

static void
myoob(signo)
	int signo;
{
	char *cp;

	/* only process if transfer occurring */
	if (!transflag)
		return;
	cp = tmpline;
	if (getline(cp, 7, stdin) == NULL) {
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
			reply(213, "Status: %qd of %qd bytes transferred",
			    byte_count, file_size);
		else
			reply(213, "Status: %qd bytes transferred", byte_count);
	}
}

/*
 * Note: a response of 425 is not mentioned as a possible response to
 *	the PASV command in RFC959. However, it has been blessed as
 *	a legitimate response by Jon Postel in a telephone conversation
 *	with Rick Adams on 25 Jan 89.
 */
void
passive()
{
	int len;
	char *p, *a;

	if (pdata >= 0)		/* close old port if one set */
		close(pdata);

	pdata = socket(AF_INET, SOCK_STREAM, 0);
	if (pdata < 0) {
		perror_reply(425, "Can't open passive connection");
		return;
	}

	(void) seteuid((uid_t)0);

#ifdef IP_PORTRANGE
	{
	    int on = restricted_data_ports ? IP_PORTRANGE_HIGH
					   : IP_PORTRANGE_DEFAULT;

	    if (setsockopt(pdata, IPPROTO_IP, IP_PORTRANGE,
			    (char *)&on, sizeof(on)) < 0)
		    goto pasv_error;
	}
#endif

	pasv_addr = ctrl_addr;
	pasv_addr.sin_port = 0;
	if (bind(pdata, (struct sockaddr *)&pasv_addr,
		 sizeof(pasv_addr)) < 0)
		goto pasv_error;

	(void) seteuid((uid_t)pw->pw_uid);

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
	(void) seteuid((uid_t)pw->pw_uid);
	(void) close(pdata);
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
gunique(local)
	char *local;
{
	static char new[MAXPATHLEN];
	struct stat st;
	int count;
	char *cp;

	cp = strrchr(local, '/');
	if (cp)
		*cp = '\0';
	if (stat(cp ? local : ".", &st) < 0) {
		perror_reply(553, cp ? local : ".");
		return ((char *) 0);
	}
	if (cp)
		*cp = '/';
	(void) snprintf(new, sizeof(new), "%s", local);
	cp = new + strlen(new);
	*cp++ = '.';
	for (count = 1; count < 100; count++) {
		(void)sprintf(cp, "%d", count);
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
perror_reply(code, string)
	int code;
	char *string;
{

	reply(code, "%s: %s.", string, strerror(errno));
}

static char *onefile[] = {
	"",
	0
};

void
send_file_list(whichf)
	char *whichf;
{
	struct stat st;
	DIR *dirp = NULL;
	struct dirent *dir;
	FILE *dout = NULL;
	char **dirlist, *dirname;
	int simple = 0;
	int freeglob = 0;
	glob_t gl;

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
				retrieve(_PATH_LS " %s", dirname);
				goto out;
			}
			perror_reply(550, whichf);
			if (dout != NULL) {
				(void) fclose(dout);
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
			fprintf(dout, "%s%s\n", dirname,
				type == TYPE_A ? "\r" : "");
			byte_count += strlen(dirname) + 1;
			continue;
		} else if (!S_ISDIR(st.st_mode))
			continue;

		if ((dirp = opendir(dirname)) == NULL)
			continue;

		while ((dir = readdir(dirp)) != NULL) {
			char nbuf[MAXPATHLEN];

			if (dir->d_name[0] == '.' && dir->d_namlen == 1)
				continue;
			if (dir->d_name[0] == '.' && dir->d_name[1] == '.' &&
			    dir->d_namlen == 2)
				continue;

			sprintf(nbuf, "%s/%s", dirname, dir->d_name);

			/*
			 * We have to do a stat to insure it's
			 * not a directory or special file.
			 */
			if (simple || (stat(nbuf, &st) == 0 &&
			    S_ISREG(st.st_mode))) {
				if (dout == NULL) {
					dout = dataconn("file list", (off_t)-1,
						"w");
					if (dout == NULL)
						goto out;
					transflag++;
				}
				if (nbuf[0] == '.' && nbuf[1] == '/')
					fprintf(dout, "%s%s\n", &nbuf[2],
						type == TYPE_A ? "\r" : "");
				else
					fprintf(dout, "%s%s\n", nbuf,
						type == TYPE_A ? "\r" : "");
				byte_count += strlen(nbuf) + 1;
			}
		}
		(void) closedir(dirp);
	}

	if (dout == NULL)
		reply(550, "No files found.");
	else if (ferror(dout) != 0)
		perror_reply(550, "Data connection");
	else
		reply(226, "Transfer complete.");

	transflag = 0;
	if (dout != NULL)
		(void) fclose(dout);
	data = -1;
	pdata = -1;
out:
	if (freeglob) {
		freeglob = 0;
		globfree(&gl);
	}
}

void
reapchild(signo)
	int signo;
{
	while (wait3(NULL, WNOHANG, NULL) > 0);
}

#ifdef OLD_SETPROCTITLE
/*
 * Clobber argv so ps will show what we're doing.  (Stolen from sendmail.)
 * Warning, since this is usually started from inetd.conf, it often doesn't
 * have much of an environment or arglist to overwrite.
 */
void
#if __STDC__
setproctitle(const char *fmt, ...)
#else
setproctitle(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	int i;
	va_list ap;
	char *p, *bp, ch;
	char buf[LINE_MAX];

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);

	/* make ps print our process name */
	p = Argv[0];
	*p++ = '-';

	i = strlen(buf);
	if (i > LastArgv - p - 2) {
		i = LastArgv - p - 2;
		buf[i] = '\0';
	}
	bp = buf;
	while (ch = *bp++)
		if (ch != '\n' && ch != '\r')
			*p++ = ch;
	while (p < LastArgv)
		*p++ = ' ';
}
#endif /* OLD_SETPROCTITLE */

static void
logxfer(name, size, start)
	char *name;
	long size;
	long start;
{
	char buf[1024];
	char path[MAXPATHLEN + 1];
	long now;

	if (statfd >= 0 && getwd(path) != NULL) {
		time(&now);
		snprintf(buf, sizeof(buf), "%.20s!%s!%s!%s/%s!%ld!%ld\n",
			ctime(&now)+4, ident, remotehost,
			path, name, size, now - start + (now == start));
		write(statfd, buf, strlen(buf));
	}
}
