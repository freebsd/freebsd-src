/*-
 * Copyright (c) 1990, 1993
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)sliplogin.c	8.2 (Berkeley) 2/1/94";
static char rscid[] = "@(#)$FreeBSD$";
#endif /* not lint */

/*
 * sliplogin.c
 * [MUST BE RUN SUID, SLOPEN DOES A SUSER()!]
 *
 * This program initializes its own tty port to be an async TCP/IP interface.
 * It sets the line discipline to slip, invokes a shell script to initialize
 * the network interface, then pauses forever waiting for hangup.
 *
 * It is a remote descendant of several similar programs with incestuous ties:
 * - Kirk Smith's slipconf, modified by Richard Johnsson @ DEC WRL.
 * - slattach, probably by Rick Adams but touched by countless hordes.
 * - the original sliplogin for 4.2bsd, Doug Kingston the mover behind it.
 *
 * There are two forms of usage:
 *
 * "sliplogin"
 * Invoked simply as "sliplogin", the program looks up the username
 * in the file /etc/slip.hosts.
 * If an entry is found, the line on fd0 is configured for SLIP operation
 * as specified in the file.
 *
 * "sliplogin IPhostlogin </dev/ttyb"
 * Invoked by root with a username, the name is looked up in the
 * /etc/slip.hosts file and if found fd0 is configured as in case 1.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <syslog.h>
#include <netdb.h>

#include <termios.h>
#include <sys/ioctl.h>
#include <net/slip.h>
#include <net/if.h>
#include <netinet/in.h>

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <paths.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "pathnames.h"

extern char **environ;

static char *restricted_environ[] = {
	"PATH=" _PATH_STDPATH,
	NULL
};

int	unit;
int	slip_mode;
speed_t speed;
int	uid;
int     keepal;
int     outfill;
int     slunit;
char	loginargs[BUFSIZ];
char	loginfile[MAXPATHLEN];
char	loginname[BUFSIZ];
static char raddr[32];			/* remote address */
char ifname[IFNAMSIZ];          	/* interface name */
static 	char pidfilename[MAXPATHLEN];   /* name of pid file */
static 	char iffilename[MAXPATHLEN];    /* name of if file */
static	pid_t	pid;			/* our pid */

char *
make_ipaddr(void)
{
static char address[20] ="";
struct hostent *he;
unsigned long ipaddr;

address[0] = '\0';
if ((he = gethostbyname(raddr)) != NULL) {
	ipaddr = ntohl(*(long *)he->h_addr_list[0]);
	sprintf(address, "%lu.%lu.%lu.%lu",
		ipaddr >> 24,
		(ipaddr & 0x00ff0000) >> 16,
		(ipaddr & 0x0000ff00) >> 8,
		(ipaddr & 0x000000ff));
	}

return address;
}

struct slip_modes {
	char	*sm_name;
	int	sm_or_flag;
	int	sm_and_flag;
}	 modes[] = {
	"normal",	0        , 0        ,
	"compress",	IFF_LINK0, IFF_LINK2,
	"noicmp",	IFF_LINK1, 0        ,
	"autocomp",	IFF_LINK2, IFF_LINK0,
};

void
findid(name)
	char *name;
{
	FILE *fp;
	static char slopt[5][16];
	static char laddr[16];
	static char mask[16];
	char   slparmsfile[MAXPATHLEN];
	char user[16];
	char buf[128];
	int i, j, n;

	environ = restricted_environ; /* minimal protection for system() */

	(void)strncpy(loginname, name, sizeof(loginname)-1);
	loginname[sizeof(loginname)-1] = '\0';

	if ((fp = fopen(_PATH_ACCESS, "r")) == NULL) {
	accfile_err:
		syslog(LOG_ERR, "%s: %m\n", _PATH_ACCESS);
		exit(1);
	}
	while (fgets(loginargs, sizeof(loginargs) - 1, fp)) {
		if (ferror(fp))
			goto accfile_err;
		if (loginargs[0] == '#' || isspace(loginargs[0]))
			continue;
		n = sscanf(loginargs, "%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s\n",
                        user, laddr, raddr, mask, slopt[0], slopt[1],
			slopt[2], slopt[3], slopt[4]);
		if (n < 4) {
			syslog(LOG_ERR, "%s: wrong format\n", _PATH_ACCESS);
			exit(1);
		}
		if (strcmp(user, name) != 0)
			continue;

		(void) fclose(fp);

		slip_mode = 0;
		for (i = 0; i < n - 4; i++) {
			for (j = 0; j < sizeof(modes)/sizeof(struct slip_modes);
				j++) {
				if (strcmp(modes[j].sm_name, slopt[i]) == 0) {
					slip_mode |= (modes[j].sm_or_flag);
					slip_mode &= ~(modes[j].sm_and_flag);
					break;
				}
			}
		}

		/*
		 * we've found the guy we're looking for -- see if
		 * there's a login file we can use.  First check for
		 * one specific to this host.  If none found, try for
		 * a generic one.
		 */
		(void)snprintf(loginfile, sizeof(loginfile), "%s.%s", 
		    _PATH_SLIP_LOGIN, name);
		if (access(loginfile, R_OK|X_OK) != 0) {
			(void)strncpy(loginfile, _PATH_SLIP_LOGIN,
			    sizeof(loginfile) - 1);
			loginfile[sizeof(loginfile) - 1] = '\0';
			if (access(loginfile, R_OK|X_OK)) {
				syslog(LOG_ERR,
				       "access denied for %s - no %s\n",
				       name, _PATH_SLIP_LOGIN);
				exit(5);
			}
		}
		(void)snprintf(slparmsfile, sizeof(slparmsfile), "%s.%s", _PATH_SLPARMS, name);
		if (access(slparmsfile, R_OK|X_OK) != 0) {
			(void)strncpy(slparmsfile, _PATH_SLPARMS, sizeof(slparmsfile)-1);
			slparmsfile[sizeof(slparmsfile)-1] = '\0';
			if (access(slparmsfile, R_OK|X_OK))
				*slparmsfile = '\0';
		}
		keepal = outfill = 0;
		slunit = -1;
		if (*slparmsfile) {
			if ((fp = fopen(slparmsfile, "r")) == NULL) {
			slfile_err:
				syslog(LOG_ERR, "%s: %m\n", slparmsfile);
				exit(1);
			}
			n = 0;
			while (fgets(buf, sizeof(buf) - 1, fp) != NULL) {
				if (ferror(fp))
					goto slfile_err;
				if (buf[0] == '#' || isspace(buf[0]))
					continue;
				n = sscanf(buf, "%d %d %d", &keepal, &outfill, &slunit);
				if (n < 1) {
				slwrong_fmt:
					syslog(LOG_ERR, "%s: wrong format\n", slparmsfile);
					exit(1);
				}
				(void) fclose(fp);
				break;
			}
			if (n == 0)
				goto slwrong_fmt;
		}

		return;
	}
	syslog(LOG_ERR, "SLIP access denied for %s\n", name);
	exit(4);
	/* NOTREACHED */
}

char *
sigstr(s)
	int s;
{
	static char buf[32];

	switch (s) {
	case SIGHUP:	return("HUP");
	case SIGINT:	return("INT");
	case SIGQUIT:	return("QUIT");
	case SIGILL:	return("ILL");
	case SIGTRAP:	return("TRAP");
	case SIGIOT:	return("IOT");
	case SIGEMT:	return("EMT");
	case SIGFPE:	return("FPE");
	case SIGKILL:	return("KILL");
	case SIGBUS:	return("BUS");
	case SIGSEGV:	return("SEGV");
	case SIGSYS:	return("SYS");
	case SIGPIPE:	return("PIPE");
	case SIGALRM:	return("ALRM");
	case SIGTERM:	return("TERM");
	case SIGURG:	return("URG");
	case SIGSTOP:	return("STOP");
	case SIGTSTP:	return("TSTP");
	case SIGCONT:	return("CONT");
	case SIGCHLD:	return("CHLD");
	case SIGTTIN:	return("TTIN");
	case SIGTTOU:	return("TTOU");
	case SIGIO:	return("IO");
	case SIGXCPU:	return("XCPU");
	case SIGXFSZ:	return("XFSZ");
	case SIGVTALRM:	return("VTALRM");
	case SIGPROF:	return("PROF");
	case SIGWINCH:	return("WINCH");
#ifdef SIGLOST
	case SIGLOST:	return("LOST");
#endif
	case SIGUSR1:	return("USR1");
	case SIGUSR2:	return("USR2");
	}
	(void)snprintf(buf, sizeof(buf), "sig %d", s);
	return(buf);
}

void
hup_handler(s)
	int s;
{
	char logoutfile[MAXPATHLEN];

	(void) close(0);
	seteuid(0);
	(void)snprintf(logoutfile, sizeof(logoutfile), "%s.%s",
	    _PATH_SLIP_LOGOUT, loginname);
	if (access(logoutfile, R_OK|X_OK) != 0) {
		(void)strncpy(logoutfile, _PATH_SLIP_LOGOUT,
		    sizeof(logoutfile) - 1);
		logoutfile[sizeof(logoutfile) - 1] = '\0';
	}
	if (access(logoutfile, R_OK|X_OK) == 0) {
		char logincmd[2*MAXPATHLEN+32];

		(void) snprintf(logincmd, sizeof(logincmd), "%s %d %ld %s", logoutfile, unit, speed, loginargs);
		(void) system(logincmd);
	}
	syslog(LOG_INFO, "closed %s slip unit %d (%s)\n", loginname, unit,
	       sigstr(s));
	if (unlink(pidfilename) < 0 && errno != ENOENT)
		syslog(LOG_WARNING, "unable to delete pid file: %m");
	if (unlink(iffilename) < 0 && errno != ENOENT)
		syslog(LOG_WARNING, "unable to delete if file: %m");
	exit(1);
	/* NOTREACHED */
}


/* Modify the slip line mode and add any compression or no-icmp flags. */
void line_flags(unit)
	int unit;
{
	struct ifreq ifr;
	int s;

	/* open a socket as the handle to the interface */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		syslog(LOG_ERR, "socket: %m");
		exit(1);
	}
	sprintf(ifr.ifr_name, "sl%d", unit);

	/* get the flags for the interface */
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "ioctl (SIOCGIFFLAGS): %m");
		exit(1);
        }

	/* Assert any compression or no-icmp flags. */
#define SLMASK (~(IFF_LINK0 | IFF_LINK1 | IFF_LINK2))
	ifr.ifr_flags &= SLMASK;
	ifr.ifr_flags |= slip_mode;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "ioctl (SIOCSIFFLAGS): %m");
		exit(1);
	}
        close(s);
}


int
main(argc, argv)
	int argc;
	char *argv[];
{
	int fd, s, ldisc;
	char *name;
	struct termios tios;
	char logincmd[2*BUFSIZ+32];

	FILE *pidfile;				/* pid file */
	FILE *iffile;				/* interfaces file */
	char *p;
	int n;
	char devnam[MAXPATHLEN] = _PATH_TTY;   /* Device name */

	if ((name = strrchr(argv[0], '/')) == NULL)
		name = argv[0];
	s = getdtablesize();
	for (fd = 3 ; fd < s ; fd++)
		(void) close(fd);
	openlog(name, LOG_PID|LOG_PERROR, LOG_DAEMON);
	uid = getuid();
	if (argc > 1) {
		findid(argv[1]);

		/*
		 * Disassociate from current controlling terminal, if any,
		 * and ensure that the slip line is our controlling terminal.
		 */
		if (daemon(1, 1)) {
			syslog(LOG_ERR, "daemon(1, 1): %m");
			exit(1);
		}
		if (argc > 2) {
			if ((fd = open(argv[2], O_RDWR)) == -1) {
				syslog(LOG_ERR, "open %s: %m", argv[2]);
				exit(2);
			}
			(void) dup2(fd, 0);
			if (fd > 2)
				close(fd);
		}
		if (ioctl(0, TIOCSCTTY, 0) == -1) {
			syslog(LOG_ERR, "ioctl (TIOCSCTTY): %m");
			exit(1);
		}
		if (tcsetpgrp(0, getpid()) < 0) {
			syslog(LOG_ERR, "tcsetpgrp failed: %m");
			exit(1);
		}
	} else {
		if ((name = getlogin()) == NULL) {
			syslog(LOG_ERR, "access denied - login name not found\n");
			exit(1);
		}
		findid(name);
	}
	(void) fchmod(0, 0600);
	(void) fprintf(stderr, "starting slip login for %s\n", loginname);
        (void) fprintf(stderr, "your address is %s\n\n", make_ipaddr());

	(void) fflush(stderr);
	sleep(1);

	/* set up the line parameters */
	if (tcgetattr(0, &tios) < 0) {
		syslog(LOG_ERR, "tcgetattr: %m");
		exit(1);
	}
	cfmakeraw(&tios);
	if (tcsetattr(0, TCSAFLUSH, &tios) < 0) {
		syslog(LOG_ERR, "tcsetattr: %m");
		exit(1);
	}
	speed = cfgetispeed(&tios);

	ldisc = SLIPDISC;
	if (ioctl(0, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		exit(1);
	}
	if (slunit >= 0 && ioctl(0, SLIOCSUNIT, &slunit) < 0) {
		syslog(LOG_ERR, "ioctl (SLIOCSUNIT): %m");
		exit(1);
	}
	/* find out what unit number we were assigned */
	if (ioctl(0, SLIOCGUNIT, &unit) < 0) {
		syslog(LOG_ERR, "ioctl (SLIOCGUNIT): %m");
		exit(1);
	}
	(void) signal(SIGHUP, hup_handler);
	(void) signal(SIGTERM, hup_handler);

	if (keepal > 0) {
		(void) signal(SIGURG, hup_handler);
		if (ioctl(0, SLIOCSKEEPAL, &keepal) < 0) {
			syslog(LOG_ERR, "ioctl(SLIOCSKEEPAL): %m");
			exit(1);
		}
	}
	if (outfill > 0 && ioctl(0, SLIOCSOUTFILL, &outfill) < 0) {
		syslog(LOG_ERR, "ioctl(SLIOCSOUTFILL): %m");
		exit(1);
	}

        /* write pid to file */
	pid = getpid();
	(void) sprintf(ifname, "sl%d", unit);
	(void) sprintf(pidfilename, "%s%s.pid", _PATH_VARRUN, ifname);
	if ((pidfile = fopen(pidfilename, "w")) != NULL) {
		fprintf(pidfile, "%d\n", pid);
		(void) fclose(pidfile);
	} else {
		syslog(LOG_ERR, "Failed to create pid file %s: %m",
				pidfilename);
		pidfilename[0] = 0;
	}

        /* write interface unit number to file */
	p = ttyname(0);
	if (p)
		strcpy(devnam, p);
	for (n = strlen(devnam); n > 0; n--) 
		if (devnam[n] == '/') {
			n++;
			break;
		}
	(void) sprintf(iffilename, "%s%s.if", _PATH_VARRUN, &devnam[n]);
	if ((iffile = fopen(iffilename, "w")) != NULL) {
		fprintf(iffile, "sl%d\n", unit); 
		(void) fclose(iffile);
	} else {
		syslog(LOG_ERR, "Failed to create if file %s: %m", iffilename);
		iffilename[0] = 0;  
	}


	syslog(LOG_INFO, "attaching slip unit %d for %s\n", unit, loginname);
	(void)snprintf(logincmd, sizeof(logincmd), "%s %d %ld %s", loginfile, unit, speed,
		      loginargs);
	/*
	 * aim stdout and errout at /dev/null so logincmd output won't
	 * babble into the slip tty line.
	 */
	(void) close(1);
	if ((fd = open(_PATH_DEVNULL, O_WRONLY)) != 1) {
		if (fd < 0) {
			syslog(LOG_ERR, "open %s: %m", _PATH_DEVNULL);
			exit(1);
		}
		(void) dup2(fd, 1);
		(void) close(fd);
	}
	(void) dup2(1, 2);

	/*
	 * Run login and logout scripts as root (real and effective);
	 * current route(8) is setuid root, and checks the real uid
	 * to see whether changes are allowed (or just "route get").
	 */
	(void) setuid(0);
	if (s = system(logincmd)) {
		syslog(LOG_ERR, "%s login failed: exit status %d from %s",
		       loginname, s, loginfile);
		exit(6);
	}

	/* Handle any compression or no-icmp flags. */
	line_flags(unit);

	/* reset uid to users' to allow the user to give a signal. */
	seteuid(uid);
	/* twiddle thumbs until we get a signal */
	while (1)
		sigpause(0);

	/* NOTREACHED */
}
