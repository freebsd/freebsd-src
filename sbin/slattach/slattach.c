/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Adams.
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
"@(#) Copyright (c) 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)slattach.c	4.6 (Berkeley) 6/1/90";*/
static char rcsid[] = "$Id: slattach.c,v 1.21 1996/12/10 17:07:44 wollman Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <fcntl.h>
#include <netdb.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

#include <netinet/in.h>
#include <net/if.h>
#include <net/slip.h>
#include <net/if_slvar.h>

#define DEFAULT_BAUD	9600

void	sighup_handler();	/* SIGHUP handler */
void	sigint_handler();	/* SIGINT handler */
void	sigterm_handler();	/* SIGTERM handler */
void    sigurg_handler();       /* SIGURG handler */
void	exit_handler(int ret);	/* run exit_cmd iff specified upon exit. */
void	setup_line(int cflag);	/* configure terminal settings */
void	slip_discipline();	/* switch to slip line discipline */
void	configure_network();	/* configure slip interface */
void	acquire_line();		/* get tty device as controling terminal */

int	fd = -1;
char	*dev = (char *)0;	/* path name of the tty (e.g. /dev/tty01) */
char    *dvname;                /* basename of dev */
int     locked = 0;             /* uucp lock active */
int	flow_control = 0;	/* non-zero to enable hardware flow control. */
int	modem_control =	HUPCL;	/* !CLOCAL+HUPCL iff we	watch carrier. */
int	comstate;		/* TIOCMGET current state of serial driver */
int	redial_on_startup = 0;	/* iff non-zero execute redial_cmd on startup */
speed_t speed = DEFAULT_BAUD;   /* baud rate of tty */
int	slflags = 0;		/* compression flags */
int	unit = -1;		/* slip device unit number */
int	foreground = 0;		/* act as demon if zero, else don't fork. */
int     keepal = 0;             /* keepalive timeout */
int     outfill = 0;            /* outfill timeout */
int     sl_unit = -1;           /* unit number */
int     uucp_lock = 0;          /* do uucp locking */
int	exiting = 0;		/* allready running exit_handler */

struct	termios tty;		/* tty configuration/state */

char	tty_path[32];		/* path name of the tty (e.g. /dev/tty01) */
char	pidfilename[40];	/* e.g. /var/run/slattach.tty01.pid */
char	*redial_cmd = 0;	/* command to exec upon shutdown. */
char	*config_cmd = 0;	/* command to exec if slip unit changes. */
char	*exit_cmd = 0;		/* command to exec before exiting. */

static char usage_str[] = "\
usage: %s [-acfhlnz] [-e command] [-r command] [-s speed] [-u command] \\\n\
	  [-L] [-K timeout] [-O timeout] [-S unit] device\n\
  -a      -- autoenable VJ compression\n\
  -c      -- enable VJ compression\n\
  -e ECMD -- run ECMD before exiting\n\
  -f      -- run in foreground (don't detach from controlling tty)\n\
  -h      -- turn on cts/rts style flow control\n\
  -l      -- disable modem control (CLOCAL) and ignore carrier detect\n\
  -n      -- throw out ICMP packets\n\
  -r RCMD -- run RCMD upon loss of carrier\n\
  -s #    -- set baud rate (default 9600)\n\
  -u UCMD -- run 'UCMD <old sl#> <new sl#>' before switch to slip discipline\n\
  -z      -- run RCMD upon startup irrespective of carrier\n\
  -L      -- do uucp-style device locking\n\
  -K #    -- set SLIP \"keep alive\" timeout (default 0)\n\
  -O #    -- set SLIP \"out fill\" timeout (default 0)\n\
  -S #    -- set SLIP unit number (default is dynamic)\n\
";

int main(int argc, char **argv)
{
	int option;
	extern char *optarg;
	extern int optind;

	while ((option = getopt(argc, argv, "ace:fhlnr:s:u:zLK:O:S:")) != EOF) {
		switch (option) {
		case 'a':
			slflags |= IFF_LINK2;
			slflags &= ~IFF_LINK0;
			break;
		case 'c':
			slflags |= IFF_LINK0;
			slflags &= ~IFF_LINK2;
			break;
		case 'e':
			exit_cmd = (char*) strdup (optarg);
			break;
		case 'f':
			foreground = 1;
			break;
		case 'h':
			flow_control |= CRTSCTS;
			break;
		case 'l':
			modem_control =	CLOCAL;	/* clear HUPCL too */
			break;
		case 'n':
			slflags |= IFF_LINK1;
			break;
		case 'r':
			redial_cmd = (char*) strdup (optarg);
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'u':
			config_cmd = (char*) strdup (optarg);
			break;
		case 'z':
			redial_on_startup = 1;
			break;
		case 'L':
			uucp_lock = 1;
			break;
		case 'K':
			keepal = atoi(optarg);
			break;
		case 'O':
			outfill = atoi(optarg);
			break;
		case 'S':
			sl_unit = atoi(optarg);
			break;
		default:
			fprintf(stderr, "%s: Invalid option -- '%c'\n",
				argv[0], option);
		case '?':
			fprintf(stderr, usage_str, argv[0]);
			exit_handler(1);
		}
	}

	if (optind == argc - 1)
		dev = argv[optind];

	if (optind < (argc - 1)) {
	    fprintf(stderr, "%s: Too many args, first='%s'\n",
	      argv[0], argv[optind]);
	}
	if (optind > (argc - 1)) {
	    fprintf(stderr, "%s: Not enough args\n", argv[0]);
	}
	if (dev == (char *)0) {
		fprintf(stderr, usage_str, argv[0]);
		exit_handler(2);
	}
	if (strncmp(_PATH_DEV, dev, sizeof(_PATH_DEV) - 1)) {
		strcpy(tty_path, _PATH_DEV);
		strcat(tty_path, "/");
		strncat(tty_path, dev, 10);
		dev = tty_path;
	}
	dvname = strrchr(dev, '/'); /* always succeeds */
	dvname++;                   /* trailing tty pathname component */
	sprintf(pidfilename, "%sslattach.%s.pid", _PATH_VARRUN, dvname);
	printf("%s\n",pidfilename);

	if (!foreground)
		daemon(0,0);	/* fork, setsid, chdir /, and close std*. */
	/* daemon() closed stderr, so log errors from here on. */
	openlog("slattach",LOG_CONS|LOG_PID,LOG_DAEMON);

	acquire_line();		/* get tty device as controling terminal */
	setup_line(0);		/* configure for slip line discipline */
	slip_discipline();	/* switch to slip line discipline */

	/* upon INT log a timestamp and exit.  */
	if ((int)signal(SIGINT,sigint_handler) < 0)
		syslog(LOG_NOTICE,"cannot install SIGINT handler: %m");
	/* upon TERM log a timestamp and exit.  */
	if ((int)signal(SIGTERM,sigterm_handler) < 0)
		syslog(LOG_NOTICE,"cannot install SIGTERM handler: %m");
	/* upon HUP redial and reconnect.  */
	if ((int)signal(SIGHUP,sighup_handler) < 0)
		syslog(LOG_NOTICE,"cannot install SIGHUP handler: %m");

	if (redial_on_startup)
		sighup_handler();
	else if (!(modem_control & CLOCAL)) {
		if (ioctl(fd, TIOCMGET, &comstate) < 0)
			syslog(LOG_NOTICE,"cannot get carrier state: %m");
		if (!(comstate & TIOCM_CD)) { /* check for carrier */
			/* force a redial if no carrier */
			kill (getpid(), SIGHUP);
		} else
			configure_network();
	} else
		configure_network(); /* configure the network if needed. */

	for (;;) {
		sigset_t mask;
		sigemptyset(&mask);
		sigsuspend(&mask);
	}
}

/* Close all FDs, fork, reopen tty port as 0-2, and make it the
   controlling terminal for our process group. */
void acquire_line()
{
	int ttydisc = TTYDISC;
	int oflags;
	FILE *pidfile;

	/* reset to tty discipline */
	if (fd >= 0 && ioctl(fd, TIOCSETD, &ttydisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		exit_handler(1);
	}

	(void)close(STDIN_FILENO); /* close FDs before forking. */
	(void)close(STDOUT_FILENO);
	(void)close(STDERR_FILENO);
	if (fd > 2)
		(void)close(fd);

	signal(SIGHUP, SIG_IGN); /* ignore HUP signal when parent dies. */
	if (daemon(0,0)) {       /* fork, setsid, chdir /, and close std*. */
		syslog(LOG_ERR, "daemon(0,0): %m");
		exit_handler(1);
	}

	while (getppid () != 1)
		sleep (1);	/* Wait for parent to die. */

	/* create PID file */
	if((pidfile = fopen(pidfilename, "w"))) {
		fprintf(pidfile, "%ld\n", getpid());
		fclose(pidfile);
	}

	if ((int)signal(SIGHUP,sighup_handler) < 0) /* Re-enable HUP signal */
		syslog(LOG_NOTICE,"cannot install SIGHUP handler: %m");

	if (uucp_lock) {
		/* unlock not needed here, always re-lock with new pid */
		if (uu_lock(dvname)) {
			syslog(LOG_ERR, "can't lock %s", dev);
			exit_handler(1);
		}
		locked = 1;
	}

	if ((fd = open(dev, O_RDWR | O_NONBLOCK, 0)) < 0) {
		syslog(LOG_ERR, "open(%s) %m", dev);
		exit_handler(1);
	}
	/* Turn off O_NONBLOCK for dumb redialers, if any. */
	if ((oflags = fcntl(fd, F_GETFL)) == -1) {
		syslog(LOG_ERR, "fcntl(F_GETFL) failed: %m");
		exit_handler(1);
	}
	if (fcntl(fd, F_SETFL, oflags & ~O_NONBLOCK) == -1) {
		syslog(LOG_ERR, "fcntl(F_SETFL) failed: %m");
		exit_handler(1);
	}
	(void)dup2(fd, STDIN_FILENO);
	(void)dup2(fd, STDOUT_FILENO);
	(void)dup2(fd, STDERR_FILENO);
	if (fd > 2)
		(void)close (fd);
	fd = STDIN_FILENO;

	/* acquire the serial line as a controling terminal. */
	if (ioctl(fd, TIOCSCTTY, 0) < 0) {
		syslog(LOG_ERR,"ioctl(TIOCSCTTY): %m");
		exit_handler(1);
	}
	/* Make us the foreground process group associated with the
	   slip line which is our controlling terminal. */
	if (tcsetpgrp(fd, getpid()) < 0)
		syslog(LOG_NOTICE,"tcsetpgrp failed: %m");
}

/* Set the tty flags and set DTR. */
/* Call as setup_line(CLOCAL) to force clocal assertion. */
void setup_line(int cflag)
{
	tty.c_lflag = tty.c_iflag = tty.c_oflag = 0;
	tty.c_cflag = CREAD | CS8 | flow_control | modem_control | cflag;
	cfsetispeed(&tty, speed);
	cfsetospeed(&tty, speed);
	/* set the line speed and flow control */
	if (tcsetattr(fd, TCSAFLUSH, &tty) < 0) {
		syslog(LOG_ERR, "tcsetattr(TCSAFLUSH): %m");
		exit_handler(1);
	}
	/* set data terminal ready */
	if (ioctl(fd, TIOCSDTR) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCSDTR): %m");
		exit_handler(1);
	}
}

/* Put the line in slip discipline. */
void slip_discipline()
{
	struct ifreq ifr;
	int slipdisc = SLIPDISC;
	int s, tmp_unit = -1;

	/* Switch to slip line discipline. */
	if (ioctl(fd, TIOCSETD, &slipdisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		exit_handler(1);
	}

	if (sl_unit >= 0 && ioctl(fd, SLIOCSUNIT, &sl_unit) < 0) {
		syslog(LOG_ERR, "ioctl(SLIOCSUNIT): %m");
                exit_handler(1);
        }

	/* find out what unit number we were assigned */
        if (ioctl(fd, SLIOCGUNIT, (caddr_t)&tmp_unit) < 0) {
                syslog(LOG_ERR, "ioctl(SLIOCGUNIT): %m");
                exit_handler(1);
        }

	if (tmp_unit < 0) {
		syslog(LOG_ERR, "bad unit (%d) from ioctl(SLIOCGUNIT)",tmp_unit);
		exit_handler(1);
	}

	if (keepal > 0) {
		signal(SIGURG, sigurg_handler);
		if (ioctl(fd, SLIOCSKEEPAL, &keepal) < 0) {
			syslog(LOG_ERR, "ioctl(SLIOCSKEEPAL): %m");
			exit_handler(1);
		}
	}
	if (outfill > 0 && ioctl(fd, SLIOCSOUTFILL, &outfill) < 0) {
		syslog(LOG_ERR, "ioctl(SLIOCSOUTFILL): %m");
		exit_handler(1);
	}

	/* open a socket as the handle to the interface */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		syslog(LOG_ERR, "socket: %m");
		exit_handler(1);
	}
	sprintf(ifr.ifr_name, "sl%d", tmp_unit);

	/* get the flags for the interface */
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "ioctl (SIOCGIFFLAGS): %m");
		exit_handler(1);
	}

	/* Assert any compression or no-icmp flags. */
#define SLMASK (~(IFF_LINK0 | IFF_LINK1 | IFF_LINK2))
 	ifr.ifr_flags &= SLMASK;
 	ifr.ifr_flags |= slflags;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "ioctl (SIOCSIFFLAGS): %m");
		exit_handler(1);
	}
	close(s);
}

/* configure the interface, eg. by passing the unit number to a script. */
void configure_network()
{
	int new_unit;

	/* find out what unit number we were assigned */
        if (ioctl(fd, SLIOCGUNIT, (caddr_t)&new_unit) < 0) {
                syslog(LOG_ERR, "ioctl(SLIOCGUNIT): %m");
                exit_handler(1);
        }
	/* iff the unit number changes either invoke config_cmd or punt. */
	if (config_cmd) {
		char *s;
		s = (char*) malloc(strlen(config_cmd) + 32);
		sprintf (s, "%s %d %d", config_cmd, unit, new_unit);
		syslog(LOG_NOTICE, "Configuring %s (sl%d):", dev, unit);
		syslog(LOG_NOTICE, "  '%s'", s);
		system(s);
		free (s);
		unit = new_unit;
	} else {
		/* don't compare unit numbers if this is the first time to attach. */
		if (unit < 0)
			unit = new_unit;
		if (new_unit != unit) {
			syslog(LOG_ERR, "slip unit changed from sl%d to sl%d, but no -u CMD was specified!");
			exit_handler(1);
		}
		syslog(LOG_NOTICE,"sl%d connected to %s at %d baud",unit,dev,speed);
	}
}

/* sighup_handler() is invoked when carrier drops, eg. before redial. */
void sighup_handler()
{
	if(exiting) return;

	if (redial_cmd == NULL) {
		syslog(LOG_NOTICE,"SIGHUP on %s (sl%d); exiting", dev, unit);
		exit_handler(1);
	}
again:
	/* invoke a shell for redial_cmd or punt. */
	if (*redial_cmd) {
		syslog(LOG_NOTICE,"SIGHUP on %s (sl%d); running '%s'",
		       dev, unit, redial_cmd);
		acquire_line(); /* reopen dead line */
		setup_line(CLOCAL);
		if (locked) {
			if (uucp_lock)
				uu_unlock(dvname);      /* for redial */
			locked = 0;
		}
		if (system(redial_cmd))
			goto again;
		if (uucp_lock) {
			if (uu_lock(dvname)) {
				syslog(LOG_ERR, "can't relock %s after %s, aborting",
					dev, redial_cmd);
				exit_handler(1);
			}
			locked = 1;
		}
		/* Now check again for carrier (dial command is done): */
		if (!(modem_control & CLOCAL)) {
			tty.c_cflag &= ~CLOCAL;
			if (tcsetattr(fd, TCSAFLUSH, &tty) < 0) {
				syslog(LOG_ERR, "tcsetattr(TCSAFLUSH): %m");
				exit_handler(1);
			}
			ioctl(fd, TIOCMGET, &comstate);
			if (!(comstate & TIOCM_CD)) { /* check for carrier */
				/* force a redial if no carrier */
				goto again;
			}
		} else
			setup_line(0);
	} else {        /* Empty redial command */
		syslog(LOG_NOTICE,"SIGHUP on %s (sl%d); reestablish connection",
			dev, unit);
		acquire_line(); /* reopen dead line */
		setup_line(0);  /* restore ospeed from hangup (B0) */
		/* If modem control, just wait for carrier before attaching.
		   If no modem control, just fall through immediately. */
		if (!(modem_control & CLOCAL)) {
			int carrier = 0;

			syslog(LOG_NOTICE, "Waiting for carrier on %s (sl%d)",
			       dev, unit);
			/* Now wait for carrier before attaching line. */
			/* We must poll since CLOCAL prevents signal. */
			while (! carrier) {
				sleep(2);
				ioctl(fd, TIOCMGET, &comstate);
				if (comstate & TIOCM_CD)
					carrier = 1;
			}
			syslog(LOG_NOTICE, "Carrier now present on %s (sl%d)",
			       dev, unit);
		}
	}
	slip_discipline();
	configure_network();
}
/* Signal handler for SIGINT.  We just log and exit. */
void sigint_handler()
{
	if(exiting) return;
	syslog(LOG_NOTICE,"SIGINT on %s (sl%d); exiting",dev,unit);
	exit_handler(0);
}
/* Signal handler for SIGURG. */
void sigurg_handler()
{
	int ttydisc = TTYDISC;

	signal(SIGURG, SIG_IGN);
	if(exiting) return;
	syslog(LOG_NOTICE,"SIGURG on %s (sl%d); hangup",dev,unit);
	if (ioctl(fd, TIOCSETD, &ttydisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		exit_handler(1);
	}
	cfsetospeed(&tty, B0);
	if (tcsetattr(fd, TCSANOW, &tty) < 0) {
		syslog(LOG_ERR, "tcsetattr(TCSANOW): %m");
		exit_handler(1);
	}
	/* Need to go to sighup handler in any case */
	if (modem_control & CLOCAL)
		kill (getpid(), SIGHUP);

}
/* Signal handler for SIGTERM.  We just log and exit. */
void sigterm_handler()
{
	if(exiting) return;
	syslog(LOG_NOTICE,"SIGTERM on %s (sl%d); exiting",dev,unit);
	exit_handler(0);
}
/* Run config_cmd if specified before exiting. */
void exit_handler(int ret)
{
	if(exiting) return;
	exiting = 1;
	/*
	 * First close the slip line in case exit_cmd wants it (like to hang
	 * up a modem or something).
	 */
	if (fd != -1)
		close(fd);
	if (uucp_lock && locked)
		uu_unlock(dvname);

	/* Remove the PID file */
	(void)unlink(pidfilename);

	if (config_cmd) {
		char *s;
		s = (char*) malloc(strlen(config_cmd) + 32);
		sprintf (s, "%s %d -1", config_cmd, unit);
		syslog(LOG_NOTICE, "Deconfiguring %s (sl%d):", dev, unit);
		syslog(LOG_NOTICE, "  '%s'", s);
		system(s);
		free (s);
	}
	/* invoke a shell for exit_cmd. */
	if (exit_cmd) {
		syslog(LOG_NOTICE,"exiting after running %s", exit_cmd);
		system(exit_cmd);
	}
	exit(ret);
}

/* local variables: */
/* c-indent-level: 8 */
/* c-argdecl-indent: 0 */
/* c-label-offset: -8 */
/* c-continued-statement-offset: 8 */
/* c-brace-offset: 0 */
/* comment-column: 32 */
/* end: */
