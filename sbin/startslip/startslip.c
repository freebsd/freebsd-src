
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
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)startslip.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

#include <sys/param.h>
#if BSD >= 199006
#define POSIX
#endif
#ifdef POSIX
#include <sys/termios.h>
#include <sys/ioctl.h>
#else
#include <sgtty.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
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
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_BAUD    B9600
int     speed = DEFAULT_BAUD;
#define	FC_NONE		0	/* flow control: none */
#define FC_HW           1       /* flow control: hardware (RTS/CTS) */
int	flowcontrol = FC_NONE;
char	*annex;
int	hup;
int     terminate;
int	logged_in = 0;
int	wait_time = 60;		/* then back off */
int     script_timeout = 90;    /* connect script default timeout */
#define	MAXTRIES	6	/* w/60 sec and doubling, takes an hour */
#define PIDFILE         "/var/run/startslip-%s.pid"

#define MAXDIALS 20
char *dials[MAXDIALS];
int diali, dialc;

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
	int fd = -1;
	void sighup(), sigterm();
	FILE *wfd = NULL, *pfd;
	char *dialerstring = 0, buf[BUFSIZ];
	char pidfile[32];
	int unitnum;
	char unitname[32];
	char *devicename, *username, *password;
	char *upscript = NULL, *downscript = NULL;
	int first = 1, tries = 0;
	int pausefirst = 0;
	pid_t pid;
#ifdef POSIX
	struct termios t;
#else
	struct sgttyb sgtty;
#endif

	while ((ch = getopt(argc, argv, "dhb:s:t:p:w:A:U:D:")) != EOF)
		switch (ch) {
		case 'd':
			debug = 1;
			break;
#ifdef POSIX
		case 'b':
			speed = atoi(optarg);
			break;
#endif
		case 'p':
			pausefirst = atoi(optarg);
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
		case 'A':
			annex = strdup(optarg);
			break;
		case 'U':
			upscript = strdup(optarg);
			break;
		case 'D':
			downscript = strdup(optarg);
			break;
		case 'h':
#ifdef POSIX
			flowcontrol = FC_HW;
			break;
#else
			(void)fprintf(stderr, "flow control not supported\n");
			exit(1);
#endif
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

#if BSD <= 43
	if (debug == 0 && (fd = open("/dev/tty", 0)) >= 0) {
		ioctl(fd, TIOCNOTTY, 0);
		close(fd);
		fd = -1;
	}
#endif

	if (debug)
		setbuf(stdout, NULL);

	if ((cp = strrchr(devicename, '/')) == NULL)
		cp = devicename;
	else
		cp++;
	sprintf(pidfile, PIDFILE, cp);
	if ((pfd = fopen(pidfile, "r")) != NULL) {
		pid = 0;
		fscanf(pfd, "%d\n", &pid);
		if (pid > 0)
			kill(pid, SIGTERM);
		fclose(pfd);
		pfd = NULL;     /* not remove pidfile yet */
		sleep(5);       /* allow down script to be completed */
	} else
restart:
	if (logged_in) {
		sprintf(buf, "%s %s down &", downscript ? downscript : "/sbin/ifconfig" , unitname);
		(void) system(buf);
	}
	if (terminate) {
		if (pfd)
			unlink(pidfile);
		exit(0);
	}
	logged_in = 0;
	if (++tries > MAXTRIES) {
		syslog(LOG_ERR, "exiting login after %d tries\n", tries);
		/* ???
		if (first)
		*/
		{
			if (pfd)
				unlink(pidfile);
			exit(1);
		}
	}
	if (diali > 0)
		dialerstring = dials[dialc++ % diali];

	/*
	 * We may get a HUP below, when the parent (session leader/
	 * controlling process) exits; ignore HUP until into new session.
	 */
	signal(SIGHUP, SIG_IGN);
	hup = 0;
	if (fork() > 0) {
		if (pausefirst)
			sleep(pausefirst);
		if (first)
			printd("parent exit\n");
		exit(0);
	}
	pausefirst = 0;
#ifdef POSIX
	if (setsid() == -1)
		syslog(LOG_ERR, "setsid: %m");
#endif
	pid = getpid();
	printd("restart: pid %d: ", pid);
	if (pfd = fopen(pidfile, "w")) {
		fprintf(pfd, "%d\n", pid);
		fclose(pfd);
	}
	if (wfd) {
		printd("fclose, ");
		fclose(wfd);
		wfd == NULL;
	}
	if (fd >= 0) {
		printd("close, ");
		close(fd);
		sleep(5);
	}
	printd("open");
	if ((fd = open(devicename, O_RDWR)) < 0) {
		syslog(LOG_ERR, "open %s: %m\n", devicename);
		if (first) {
			if (pfd)
				unlink(pidfile);
			exit(1);
		} else {
			syslog(LOG_INFO, "sleeping %d seconds (%d tries).\n", wait_time * tries, tries);
			sleep(wait_time * tries);
			goto restart;
		}
	}
	printd(" %d", fd);
#ifdef TIOCSCTTY
	if (ioctl(fd, TIOCSCTTY, 0) < 0)
		syslog(LOG_ERR, "ioctl (TIOCSCTTY): %m");
#endif
	signal(SIGHUP, sighup);
	signal(SIGTERM, sigterm);
	if (debug) {
		if (ioctl(fd, TIOCGETD, &disc) < 0)
			syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		printd(" (disc was %d)", disc);
	}
	disc = TTYDISC;
	if (ioctl(fd, TIOCSETD, &disc) < 0) {
		syslog(LOG_ERR, "%s: ioctl (TIOCSETD 0): %m\n",
		    devicename);
	}
	printd(", ioctl");
#ifdef POSIX
	if (tcgetattr(fd, &t) < 0) {
		syslog(LOG_ERR, "%s: tcgetattr: %m\n", devicename);
		if (pfd)
			unlink(pidfile);
	        exit(2);
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
	cfsetspeed(&t, speed);
	if (tcsetattr(fd, TCSAFLUSH, &t) < 0) {
		syslog(LOG_ERR, "%s: tcsetattr: %m\n", devicename);
		if (first) {
			if (pfd)
				unlink(pidfile);
			exit(2);
		} else {
			syslog(LOG_INFO, "sleeping %d seconds (%d tries).\n", wait_time * tries, tries);
			sleep(wait_time * tries);
			goto restart;
		}
	}
#else
	if (ioctl(fd, TIOCGETP, &sgtty) < 0) {
		syslog(LOG_ERR, "%s: ioctl (TIOCGETP): %m\n",
		    devicename);
		if (pfd)
			unlink(pidfile);
	        exit(2);
	}
	sgtty.sg_flags = RAW | ANYP;
	sgtty.sg_erase = sgtty.sg_kill = 0377;
	sgtty.sg_ispeed = sgtty.sg_ospeed = speed;
	if (ioctl(fd, TIOCSETP, &sgtty) < 0) {
		syslog(LOG_ERR, "%s: ioctl (TIOCSETP): %m\n",
		    devicename);
		if (first) {
			if (pfd)
				unlink(pidfile);
			exit(2);
		} else {
			syslog(LOG_INFO, "sleeping %d seconds (%d tries).\n", wait_time * tries, tries);
			sleep(wait_time * tries);
			goto restart;
		}
	}
#endif
	sleep(2);		/* wait for flakey line to settle */
	if (hup || terminate)
		goto restart;

	wfd = fdopen(fd, "w+");
	if (wfd == NULL) {
		syslog(LOG_ERR, "can't fdopen slip line\n");
		if (pfd)
			unlink(pidfile);
		exit(10);
	}
	setbuf(wfd, (char *)0);
	if (dialerstring) {
		printd(", send dialstring: %s\\r", dialerstring);
		fprintf(wfd, "%s\r", dialerstring);
	} else {
		printd(", send \\r");
		putc('\r', wfd);
	}
	printd("\n");

	/*
	 * Log in
	 */
	printd("look for login: ");
	for (;;) {
		if (getline(buf, BUFSIZ, fd, script_timeout) == 0 || hup || terminate) {
			if (!terminate) {
				syslog(LOG_INFO, "sleeping %d seconds (%d tries).\n", wait_time * tries, tries);
				sleep(wait_time * tries);
			}
			goto restart;
		}
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

	/*
	 * Attach
	 */
	printd("setd");
	disc = SLIPDISC;
	if (ioctl(fd, TIOCSETD, &disc) < 0) {
		syslog(LOG_ERR, "%s: ioctl (TIOCSETD SLIP): %m\n",
		    devicename);
		if (pfd)
			unlink(pidfile);
	        exit(1);
	}
	if (ioctl(fd, SLIOCGUNIT, (caddr_t)&unitnum) < 0) {
		syslog(LOG_ERR, "ioctl(SLIOCGUNIT): %m");
		if (pfd)
			unlink(pidfile);
		exit(1);
	}
	sprintf(unitname, "sl%d", unitnum);
	if (first && debug == 0) {
		close(0);
		close(1);
		close(2);
		(void) open("/dev/null", O_RDWR);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
	}

	sprintf(buf, "%s %s up &", upscript ? upscript : "/sbin/ifconfig" , unitname);
	(void) system(buf);

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
sigterm()
{

	printd("terminate\n");
	if (terminate == 0 && logged_in)
		syslog(LOG_INFO, "terminate signal\n");
	terminate = 1;
}

getline(buf, size, fd, timeout)
	char *buf;
	int size, fd, timeout;
{
	register int i;
	int ret;
	fd_set readfds;
	struct timeval tv;

	size--;
	for (i = 0; i < size; i++) {
		if (hup || terminate)
			return (0);
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		if ((ret = select(fd + 1, &readfds, NULL, NULL, &tv)) < 0) {
			if (errno != EINTR)
				syslog(LOG_ERR, "getline: select: %m");
		} else {
			if (! ret) {
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

usage()
{
	(void)fprintf(stderr, "\
usage: startslip [-d] [-b speed] [-s string1 [-s string2 [...]]] [-A annexname]\n\
	[-h] [-U upscript] [-D downscript] [-t script_timeout]\n\
	[-w retry_pause] [-p father_pause] device user passwd\n");
	exit(1);
}
