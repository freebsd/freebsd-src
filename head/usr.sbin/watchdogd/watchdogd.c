/*
 * Copyright (c) 2003-2004  Sean M. Kelly <smkelly@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Software watchdog daemon.
 */

#include <sys/types.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/rtprio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/watchdog.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <math.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static void	parseargs(int, char *[]);
static void	sighandler(int);
static void	watchdog_loop(void);
static int	watchdog_init(void);
static int	watchdog_onoff(int onoff);
static int	watchdog_patpat(u_int timeout);
static void	usage(void);

int debugging = 0;
int end_program = 0;
const char *pidfile = _PATH_VARRUN "watchdogd.pid";
int reset_mib[3];
size_t reset_miblen = 3;
u_int timeout = WD_TO_16SEC;
u_int passive = 0;
int is_daemon = 0;
int fd = -1;
int nap = 1;
char *test_cmd = NULL;

/*
 * Periodically pat the watchdog, preventing it from firing.
 */
int
main(int argc, char *argv[])
{
	struct rtprio rtp;
	struct pidfh *pfh;
	pid_t otherpid;

	if (getuid() != 0)
		errx(EX_SOFTWARE, "not super user");
		
	parseargs(argc, argv);

	rtp.type = RTP_PRIO_REALTIME;
	rtp.prio = 0;
	if (rtprio(RTP_SET, 0, &rtp) == -1)
		err(EX_OSERR, "rtprio");

	if (watchdog_init() == -1)
		errx(EX_SOFTWARE, "unable to initialize watchdog");

	if (is_daemon) {
		if (watchdog_onoff(1) == -1)
			err(EX_OSERR, "patting the dog");

		pfh = pidfile_open(pidfile, 0600, &otherpid);
		if (pfh == NULL) {
			if (errno == EEXIST) {
				errx(EX_SOFTWARE, "%s already running, pid: %d",
				    getprogname(), otherpid);
			}
			warn("Cannot open or create pidfile");
		}

		if (debugging == 0 && daemon(0, 0) == -1) {
			watchdog_onoff(0);
			pidfile_remove(pfh);
			err(EX_OSERR, "daemon");
		}

		signal(SIGHUP, SIG_IGN);
		signal(SIGINT, sighandler);
		signal(SIGTERM, sighandler);

		pidfile_write(pfh);

		watchdog_loop();

		/* exiting */
		pidfile_remove(pfh);
		return (EX_OK);
	} else {
		if (passive)
			timeout |= WD_PASSIVE;
		else
			timeout |= WD_ACTIVE;
		if (watchdog_patpat(timeout) < 0)
			err(EX_OSERR, "patting the dog");
		return (EX_OK);
	}
}

/*
 * Catch signals and begin shutdown process.
 */
static void
sighandler(int signum)
{

	if (signum == SIGINT || signum == SIGTERM)
		end_program = 1;
}

/*
 * Open the watchdog device.
 */
static int
watchdog_init(void)
{

	fd = open("/dev/" _PATH_WATCHDOG, O_RDWR);
	if (fd >= 0)
		return (0);
	warn("Could not open watchdog device");
	return (-1);
}

/*
 * Main program loop which is iterated every second.
 */
static void
watchdog_loop(void)
{
	struct stat sb;
	int failed;

	while (end_program != 2) {
		failed = 0;

		if (test_cmd != NULL)
			failed = system(test_cmd);
		else
			failed = stat("/etc", &sb);

		if (failed == 0)
			watchdog_patpat(timeout|WD_ACTIVE);
		sleep(nap);

		if (end_program != 0) {
			if (watchdog_onoff(0) == 0) {
				end_program = 2;
			} else {
				warnx("Could not stop the watchdog, not exitting");
				end_program = 0;
			}
		}
	}
}

/*
 * Reset the watchdog timer. This function must be called periodically
 * to keep the watchdog from firing.
 */
int
watchdog_patpat(u_int t)
{

	return ioctl(fd, WDIOCPATPAT, &t);
}

/*
 * Toggle the kernel's watchdog. This routine is used to enable and
 * disable the watchdog.
 */
static int
watchdog_onoff(int onoff)
{

	if (onoff)
		return watchdog_patpat((timeout|WD_ACTIVE));
	else
		return watchdog_patpat(0);
}

/*
 * Tell user how to use the program.
 */
static void
usage(void)
{
	if (is_daemon)
		fprintf(stderr, "usage: watchdogd [-d] [-e cmd] [-I file] [-s sleep] [-t timeout]\n");
	else
		fprintf(stderr, "usage: watchdog [-d] [-t timeout]\n");
	exit(EX_USAGE);
}

/*
 * Handle the few command line arguments supported.
 */
static void
parseargs(int argc, char *argv[])
{
	int c;
	char *p;
	double a;

	c = strlen(argv[0]);
	if (argv[0][c - 1] == 'd')
		is_daemon = 1;
	while ((c = getopt(argc, argv,
	    is_daemon ? "I:de:s:t:?" : "dt:?")) != -1) {
		switch (c) {
		case 'I':
			pidfile = optarg;
			break;
		case 'd':
			debugging = 1;
			break;
		case 'e':
			test_cmd = strdup(optarg);
			break;
#ifdef notyet
		case 'p':
			passive = 1;
			break;
#endif
		case 's':
			p = NULL;
			errno = 0;
			nap = strtol(optarg, &p, 0);
			if ((p != NULL && *p != '\0') || errno != 0)
				errx(EX_USAGE, "-s argument is not a number");
			break;
		case 't':
			p = NULL;
			errno = 0;
			a = strtod(optarg, &p);
			if ((p != NULL && *p != '\0') || errno != 0)
				errx(EX_USAGE, "-t argument is not a number");
			if (a < 0)
				errx(EX_USAGE, "-t argument must be positive");
			if (a == 0)
				timeout = WD_TO_NEVER;
			else
				timeout = 1.0 + log(a * 1e9) / log(2.0);
			if (debugging)
				printf("Timeout is 2^%d nanoseconds\n",
				    timeout);
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	if (argc != optind)
		errx(EX_USAGE, "extra arguments.");
	if (is_daemon && timeout < WD_TO_1SEC)
		errx(EX_USAGE, "-t argument is less than one second.");
}
