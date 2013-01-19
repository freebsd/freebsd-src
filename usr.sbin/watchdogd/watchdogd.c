/*-
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

#include <sys/mman.h>
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
#include <strings.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

static void	parseargs(int, char *[]);
static void	sighandler(int);
static void	watchdog_loop(void);
static int	watchdog_init(void);
static int	watchdog_onoff(int onoff);
static int	watchdog_patpat(u_int timeout);
static void	usage(void);

static int debugging = 0;
static int end_program = 0;
static const char *pidfile = _PATH_VARRUN "watchdogd.pid";
static u_int timeout = WD_TO_16SEC;
static u_int passive = 0;
static int is_daemon = 0;
static int is_dry_run = 0;  /* do not arm the watchdog, only
			       report on timing of the watch
			       program */
static int do_timedog = 0;
static int do_syslog = 0;
static int fd = -1;
static int nap = 1;
static int carp_thresh_seconds = -1;
static char *test_cmd = NULL;

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

	if (do_syslog) {
		openlog("watchdogd", LOG_CONS|LOG_NDELAY|LOG_PERROR,
		    LOG_DAEMON);

	}

	rtp.type = RTP_PRIO_REALTIME;
	rtp.prio = 0;
	if (rtprio(RTP_SET, 0, &rtp) == -1)
		err(EX_OSERR, "rtprio");

	if (!is_dry_run && watchdog_init() == -1)
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
		if (madvise(0, 0, MADV_PROTECT) != 0)
			warn("madvise failed");
		if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
			warn("mlockall failed");

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

	if (is_dry_run)
		return 0;

	fd = open("/dev/" _PATH_WATCHDOG, O_RDWR);
	if (fd >= 0)
		return (0);
	warn("Could not open watchdog device");
	return (-1);
}

/*
 * If we are doing timing, then get the time.
 */
static int
watchdog_getuptime(struct timespec *tp)
{
	int error;

	if (!do_timedog)
		return 0;

	error = clock_gettime(CLOCK_UPTIME_FAST, tp);
	if (error)
		warn("clock_gettime");
	return (error);
}

static long
watchdog_check_dogfunction_time(struct timespec *tp_start,
    struct timespec *tp_end)
{
	struct timeval tv_start, tv_end, tv;
	const char *cmd_prefix, *cmd;
	int sec;

	if (!do_timedog)
		return (0);

	TIMESPEC_TO_TIMEVAL(&tv_start, tp_start);
	TIMESPEC_TO_TIMEVAL(&tv_end, tp_end);
	timersub(&tv_end, &tv_start, &tv);
	sec = tv.tv_sec;
	if (sec < carp_thresh_seconds)
		return (sec);

	if (test_cmd) {
		cmd_prefix = "Watchdog program";
		cmd = test_cmd;
	} else {
		cmd_prefix = "Watchdog operation";
		cmd = "stat(\"/etc\", &sb)";
	}
	if (do_syslog)
		syslog(LOG_CRIT, "%s: '%s' took too long: "
		    "%d.%06ld seconds >= %d seconds threshhold",
		    cmd_prefix, cmd, sec, (long)tv.tv_usec,
		    carp_thresh_seconds);
	warnx("%s: '%s' took too long: "
	    "%d.%06ld seconds >= %d seconds threshhold",
	    cmd_prefix, cmd, sec, (long)tv.tv_usec, carp_thresh_seconds);
	return (sec);
}


/*
 * Main program loop which is iterated every second.
 */
static void
watchdog_loop(void)
{
	struct timespec ts_start, ts_end;
	struct stat sb;
	long waited;
	int error, failed;

	while (end_program != 2) {
		failed = 0;

		error = watchdog_getuptime(&ts_start);
		if (error) {
			end_program = 1;
			goto try_end;
		}

		if (test_cmd != NULL)
			failed = system(test_cmd);
		else
			failed = stat("/etc", &sb);

		error = watchdog_getuptime(&ts_end);
		if (error) {
			end_program = 1;
			goto try_end;
		}

		waited = watchdog_check_dogfunction_time(&ts_start, &ts_end);

		if (failed == 0)
			watchdog_patpat(timeout|WD_ACTIVE);
		if (nap - waited > 0)
			sleep(nap - waited);

try_end:
		if (end_program != 0) {
			if (watchdog_onoff(0) == 0) {
				end_program = 2;
			} else {
				warnx("Could not stop the watchdog, not exiting");
				end_program = 0;
			}
		}
	}
}

/*
 * Reset the watchdog timer. This function must be called periodically
 * to keep the watchdog from firing.
 */
static int
watchdog_patpat(u_int t)
{

	if (is_dry_run)
		return 0;

	return ioctl(fd, WDIOCPATPAT, &t);
}

/*
 * Toggle the kernel's watchdog. This routine is used to enable and
 * disable the watchdog.
 */
static int
watchdog_onoff(int onoff)
{

	/* fake successful watchdog op if a dry run */
	if (is_dry_run)
		return 0;

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
		fprintf(stderr, "usage: watchdogd [-dnw] [-e cmd] [-I file] [-s sleep] [-t timeout] [-T script_timeout]\n");
	else
		fprintf(stderr, "usage: watchdog [-d] [-t timeout]\n");
	exit(EX_USAGE);
}

static long
fetchtimeout(int opt, const char *myoptarg)
{
	char *p;
	long rv;

	p = NULL;
	errno = 0;
	rv = strtol(myoptarg, &p, 0);
	if ((p != NULL && *p != '\0') || errno != 0)
		errx(EX_USAGE, "-%c argument is not a number", opt);
	return (rv);
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
	    is_daemon ? "I:de:ns:t:ST:w?" : "dt:?")) != -1) {
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
		case 'n':
			is_dry_run = 1;
			break;
#ifdef notyet
		case 'p':
			passive = 1;
			break;
#endif
		case 's':
			nap = fetchtimeout(c, optarg);
			break;
		case 'S':
			do_syslog = 1;
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
				timeout = flsll(a * 1e9);
			if (debugging)
				printf("Timeout is 2^%d nanoseconds\n",
				    timeout);
			break;
		case 'T':
			carp_thresh_seconds = fetchtimeout(c, optarg);
			break;
		case 'w':
			do_timedog = 1;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (carp_thresh_seconds == -1)
		carp_thresh_seconds = nap;

	if (argc != optind)
		errx(EX_USAGE, "extra arguments.");
	if (is_daemon && timeout < WD_TO_1SEC)
		errx(EX_USAGE, "-t argument is less than one second.");
}
