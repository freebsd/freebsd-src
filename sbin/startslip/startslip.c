/*-
 * Copyright (c) 1990, 1991, 1993
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
 * $Id: startslip.c,v 1.13 1995/09/16 05:18:20 ache Exp $
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)startslip.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

#include <termios.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_slvar.h>
#include <net/slip.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>

#define DEFAULT_BAUD    B9600
int     speed = DEFAULT_BAUD;
#define	FC_NONE		0	/* flow control: none */
#define FC_HW           1       /* flow control: hardware (RTS/CTS) */
int	flowcontrol = FC_NONE;
int     modem_control = 1;      /* !CLOCAL+HUPCL iff we watch carrier. */
char	*annex;
int	hup;
int     terminate;
int     locked;
int	logged_in = 0;
int	wait_time = 60;		/* then back off */
int     script_timeout = 90;    /* connect script default timeout */
int     MAXTRIES = 6;           /* w/60 sec and doubling, takes an hour */
#define PIDFILE         "%sstartslip.%s.pid"

#define MAXDIALS 20
char *dials[MAXDIALS];
int diali, dialc;

int fd = -1;
FILE *pfd;
char *dvname, *devicename;
char pidfile[80];

#ifdef DEBUG
int	debug = 1;
#undef LOG_ERR
#undef LOG_INFO
#define syslog fprintf
#define LOG_ERR stderr
#define LOG_INFO stderr
#else
int	debug = 0;
#endif
#define	printd	if (debug) printf

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	char *cp, **ap;
	int ch, disc;
	void sighup(), sigterm(), sigurg();
	FILE *wfd = NULL;
	char *dialerstring = 0, buf[BUFSIZ];
	int unitnum, keepal = 0, outfill = 0;
	char unitname[32];
	char *username, *password;
	char *upscript = NULL, *downscript = NULL;
	int first = 1, tries = 0;
	time_t fintimeout;
	pid_t pid;
	struct termios t;

	while ((ch = getopt(argc, argv, "dhlb:s:t:w:A:U:D:W:K:O:")) != EOF)
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'b':
			speed = atoi(optarg);
			break;
		case 's':
			if (diali >= MAXDIALS) {
				(void)fprintf(stderr,
					"max dial strings number (%d) exceeded\n", MAXDIALS);
				exit(1);
			}
			dials[diali++] = strdup(optarg);
			break;
		case 't':
			script_timeout = atoi(optarg);
			break;
		case 'w':
			wait_time = atoi(optarg);
			break;
		case 'W':
			MAXTRIES = atoi(optarg);
			break;
		case 'A':
			annex = strdup(optarg);
			break;
		case 'U':
			upscript = strdup(optarg);
			break;
		case 'D':
			downscript = strdup(optarg);
			break;
		case 'l':
			modem_control = 0;
			break;
		case 'h':
			flowcontrol = FC_HW;
			break;
		case 'K':
			keepal = atoi(optarg);
			break;
		case 'O':
			outfill = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 3)
		usage();

	/*
	 * Copy these so they exist after we clobber them.
	 */
	devicename = strdup(argv[0]);
	username = strdup(argv[1]);
	password = strdup(argv[2]);

	/*
	 * Security hack.  Do not want private information such as the
	 * password and possible phone number to be left around.
	 * So we clobber the arguments.
	 */
	for (ap = argv - optind + 1; ap < argv + 3; ap++)
		for (cp = *ap; *cp != 0; cp++)
			*cp = '\0';

	openlog("startslip", LOG_PID|LOG_PERROR, LOG_DAEMON);

	if (debug)
		setbuf(stdout, NULL);

	signal(SIGTERM, sigterm);
	if ((dvname = strrchr(devicename, '/')) == NULL)
		dvname = devicename;
	else
		dvname++;
	sprintf(pidfile, PIDFILE, _PATH_VARRUN, dvname);
	if ((pfd = fopen(pidfile, "r")) != NULL) {
		pid = 0;
		fscanf(pfd, "%ld\n", &pid);
		if (pid > 0)
			kill(pid, SIGTERM);
		fclose(pfd);
		pfd = NULL;     /* not remove pidfile yet */
		sleep(5);       /* allow down script to be completed */
	} else
restart:
	signal(SIGHUP, SIG_IGN);
	signal(SIGURG, SIG_IGN);
	hup = 0;
	if (logged_in) {
		sprintf(buf, "LINE=%d %s %s down",
		diali ? (dialc - 1) % diali : 0,
		downscript ? downscript : "/sbin/ifconfig" , unitname);
		(void) system(buf);
		logged_in = 0;
	}
	if (terminate)
		down(0);
	tries++;
	if (MAXTRIES > 0 && tries > MAXTRIES) {
		syslog(LOG_ERR, "exiting login after %d tries\n", tries);
		/* ???
		if (first)
		*/
			down(3);
	}
	if (wfd) {
		printd("fclose, ");
		fclose(wfd);
		uu_unlock(dvname);
		locked = 0;
		wfd = NULL;
		fd = -1;
		sleep(5);
	} else if (fd >= 0) {
		printd("close, ");
		close(fd);
		uu_unlock(dvname);
		locked = 0;
		fd = -1;
		sleep(5);
	}
	if (terminate)
		goto restart;
	if (tries > 1) {
		syslog(LOG_INFO, "sleeping %d seconds (%d tries)",
			wait_time * (tries - 1), tries);
		sleep(wait_time * (tries - 1));
		if (terminate)
			goto restart;
	}

	if (daemon(1, debug) < 0) {
		syslog(LOG_ERR, "daemon: %m");
		down(2);
	}

	pid = getpid();
	printd("restart: pid %ld: ", pid);
	if ((pfd = fopen(pidfile, "w")) != NULL) {
		fprintf(pfd, "%ld\n", pid);
		fclose(pfd);
	}
	printd("open");
	if (uu_lock(dvname)) {
		syslog(LOG_ERR, "can't lock %s", devicename);
		goto restart;
	}
	locked = 1;
	if ((fd = open(devicename, O_RDWR | O_NONBLOCK)) < 0) {
		syslog(LOG_ERR, "open %s: %m\n", devicename);
		if (first)
			down(1);
		else {
			uu_unlock(dvname);
			locked = 0;
			goto restart;
		}
	}
	printd(" %d", fd);
	signal(SIGHUP, sighup);
	if (debug) {
		if (ioctl(fd, TIOCGETD, &disc) < 0)
			syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		else
			printd(" (disc was %d)", disc);
	}
	disc = TTYDISC;
	if (ioctl(fd, TIOCSETD, &disc) < 0) {
		syslog(LOG_ERR, "%s: ioctl (TIOCSETD 0): %m\n",
		    devicename);
		down(2);
	}
	if (ioctl(fd, TIOCSCTTY, 0) < 0) {
		syslog(LOG_ERR, "ioctl (TIOCSCTTY): %m");
		down(2);
	}
	if (tcsetpgrp(fd, getpid()) < 0) {
		syslog(LOG_ERR, "tcsetpgrp failed: %m");
		down(2);
	}
	printd(", ioctl\n");
	if (tcgetattr(fd, &t) < 0) {
		syslog(LOG_ERR, "%s: tcgetattr: %m\n", devicename);
		down(2);
	}
	cfmakeraw(&t);
	switch (flowcontrol) {
	case FC_HW:
		t.c_cflag |= (CRTS_IFLOW|CCTS_OFLOW);
		break;
	case FC_NONE:
		t.c_cflag &= ~(CRTS_IFLOW|CCTS_OFLOW);
		break;
	}
	if (modem_control)
		t.c_cflag |= HUPCL;
	else
		t.c_cflag &= ~(HUPCL);
	t.c_cflag |= CLOCAL;    /* until modem commands passes */
	cfsetspeed(&t, speed);
	if (tcsetattr(fd, TCSAFLUSH, &t) < 0) {
		syslog(LOG_ERR, "%s: tcsetattr: %m\n", devicename);
		down(2);
	}
	sleep(2);		/* wait for flakey line to settle */
	if (hup || terminate)
		goto restart;

	wfd = fdopen(fd, "w+");
	if (wfd == NULL) {
		syslog(LOG_ERR, "can't fdopen %s: %m", devicename);
		down(2);
	}
	setbuf(wfd, NULL);

	if (diali > 0)
		dialerstring = dials[dialc++ % diali];
	if (dialerstring) {
		printd("send dialstring: %s\\r", dialerstring);
		fprintf(wfd, "%s\r", dialerstring);
	} else {
		printd("send \\r");
		putc('\r', wfd);
	}
	printd("\n");

	fintimeout = time(NULL) + script_timeout;
	if (modem_control) {
		printd("waiting for carrier\n");
		while (time(NULL) < fintimeout && !carrier()) {
			sleep(1);
			if (hup || terminate)
				goto restart;
		}
		if (!carrier())
			goto restart;
		t.c_cflag &= ~(CLOCAL);
		if (tcsetattr(fd, TCSANOW, &t) < 0) {
			syslog(LOG_ERR, "%s: tcsetattr: %m", devicename);
			down(2);
		}
		/* Only now we able to receive HUP on carier drop! */
	}

	/*
	 * Log in
	 */
	printd("look for login: ");
	for (;;) {
		if (getline(buf, BUFSIZ, fd, fintimeout) == 0 || hup || terminate)
			goto restart;
		if (annex) {
			if (bcmp(buf, annex, strlen(annex)) == 0) {
				fprintf(wfd, "slip\r");
				printd("Sent \"slip\"\n");
				continue;
			}
			if (bcmp(&buf[1], "sername:", 8) == 0) {
				fprintf(wfd, "%s\r", username);
				printd("Sent login: %s\n", username);
				continue;
			}
			if (bcmp(&buf[1], "assword:", 8) == 0) {
				fprintf(wfd, "%s\r", password);
				printd("Sent password: %s\n", password);
				break;
			}
		} else {
			if (strstr(&buf[1], "ogin:") != NULL) {
				fprintf(wfd, "%s\r", username);
				printd("Sent login: %s\n", username);
				continue;
			}
			if (strstr(&buf[1], "assword:") != NULL) {
				fprintf(wfd, "%s\r", password);
				printd("Sent password: %s\n", password);
				break;
			}
		}
	}

	sleep(5);       /* Wait until login completed */
	if (hup || terminate)
		goto restart;
	/*
	 * Attach
	 */
	printd("setd");
	disc = SLIPDISC;
	if (ioctl(fd, TIOCSETD, &disc) < 0) {
		syslog(LOG_ERR, "%s: ioctl (TIOCSETD SLIP): %m\n",
		    devicename);
		down(2);
	}
	if (ioctl(fd, SLIOCGUNIT, (caddr_t)&unitnum) < 0) {
		syslog(LOG_ERR, "ioctl(SLIOCGUNIT): %m");
		down(2);
	}
	sprintf(unitname, "sl%d", unitnum);

	sprintf(buf, "LINE=%d %s %s up",
		diali ? (dialc - 1) % diali : 0,
		upscript ? upscript : "/sbin/ifconfig" , unitname);
	(void) system(buf);

	if (keepal > 0) {
		signal(SIGURG, sigurg);
		if (ioctl(fd, SLIOCSKEEPAL, &keepal) < 0) {
			syslog(LOG_ERR, "ioctl(SLIOCSKEEPAL): %m");
			down(2);
		}
	}
	if (outfill > 0 && ioctl(fd, SLIOCSOUTFILL, &outfill) < 0) {
		syslog(LOG_ERR, "ioctl(SLIOCSOUTFILL): %m");
		down(2);
	}
	printd(", ready\n");
	if (!first)
		syslog(LOG_INFO, "reconnected on %s (%d tries).\n", unitname, tries);
	first = 0;
	tries = 0;
	logged_in = 1;
	while (hup == 0 && terminate == 0) {
		sigpause(0L);
		printd("sigpause return\n");
	}
	goto restart;
}

void
sighup()
{

	printd("hup\n");
	if (hup == 0 && logged_in)
		syslog(LOG_INFO, "hangup signal\n");
	hup = 1;
}

void
sigurg()
{

	printd("urg\n");
	if (hup == 0 && logged_in)
		syslog(LOG_INFO, "dead line signal\n");
	hup = 1;
}

void
sigterm()
{

	printd("terminate\n");
	if (terminate == 0 && logged_in)
		syslog(LOG_INFO, "terminate signal\n");
	terminate = 1;
}

getline(buf, size, fd, fintimeout)
	char *buf;
	int size, fd;
	time_t fintimeout;
{
	register int i;
	int ret;
	fd_set readfds;
	struct timeval tv;
	time_t timeout;

	size--;
	for (i = 0; i < size; i++) {
		if (hup || terminate)
			return (0);
		if ((timeout = fintimeout - time(NULL)) <= 0)
			goto tout;
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		if ((ret = select(fd + 1, &readfds, NULL, NULL, &tv)) < 0) {
			if (errno != EINTR)
				syslog(LOG_ERR, "getline: select: %m");
		} else {
			if (! ret) {
			tout:
				printd("getline: timed out\n");
				return (0);
			}
			if ((ret = read(fd, &buf[i], 1)) == 1) {
				buf[i] &= 0177;
				if (buf[i] == '\r' || buf[i] == '\0') {
					i--;
					continue;
				}
				if (buf[i] != '\n' && buf[i] != ':')
					continue;
				buf[i + 1] = '\0';
				printd("Got %d: %s", i + 1, buf);
				return (i+1);
			}
			if (ret <= 0) {
				if (ret < 0) {
					syslog(LOG_ERR, "getline: read: %m");
				} else
					syslog(LOG_ERR, "read returned 0");
				buf[i] = '\0';
				printd("returning %d after %d: %s\n", ret, i, buf);
				return (0);
			}
		}
	}
	return (0);
}

carrier()
{
	int comstate;

	if (ioctl(fd, TIOCMGET, &comstate) < 0) {
		syslog(LOG_ERR, "%s: ioctl (TIOCMGET): %m",
		    devicename);
		down(2);
	}
	return !!(comstate & TIOCM_CD);
}

down(code)
{
	int disc = TTYDISC;

	if (fd > -1 && ioctl(fd, TIOCSETD, &disc) < 0)
		syslog(LOG_ERR, "%s: ioctl (TIOCSETD 0): %m",
		    devicename);
	if (pfd)
		unlink(pidfile);
	if (locked)
		uu_unlock(dvname);
	exit(code);
}

usage()
{
	(void)fprintf(stderr, "\
usage: startslip [-d] [-b speed] [-s string1 [-s string2 [...]]] [-A annexname]\n\
	[-h] [-l] [-U upscript] [-D downscript] [-t script_timeout]\n\
	[-w retry_pause] [-W maxtries] [-K keepalive] [-O outfill]\n\
	device user passwd\n");
	exit(1);
}
