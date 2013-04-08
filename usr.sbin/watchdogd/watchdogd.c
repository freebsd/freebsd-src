/*-
 * Copyright (c) 2003-2004  Sean M. Kelly <smkelly@FreeBSD.org>
 * Copyright (c) 2013 iXsystems.com,
 *                    author: Alfred Perlstein <alfred@freebsd.org>
 *
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

#include <getopt.h>

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
static u_int timeout = WD_TO_128SEC;
static u_int pretimeout = 0;
static u_int passive = 0;
static int is_daemon = 0;
static int is_dry_run = 0;  /* do not arm the watchdog, only
			       report on timing of the watch
			       program */
static int do_timedog = 0;
static int do_syslog = 1;
static int fd = -1;
static int nap = 1;
static int carp_thresh_seconds = -1;
static char *test_cmd = NULL;

static const char *getopt_shortopts;

static int pretimeout_set;
static int pretimeout_act;
static int pretimeout_act_set;

static int softtimeout_set;
static int softtimeout_act;
static int softtimeout_act_set;

static struct option longopts[] = {
	{ "debug", no_argument, &debugging, 1 },
	{ "pretimeout", required_argument, &pretimeout_set, 1 },
	{ "pretimeout-action", required_argument, &pretimeout_act_set, 1 },
	{ "softtimeout", no_argument, &softtimeout_set, 1 },
	{ "softtimeout-action", required_argument, &softtimeout_act_set, 1 },
	{ NULL, 0, NULL, 0}
};

/*
 * Ask malloc() to map minimum-sized chunks of virtual address space at a time,
 * so that mlockall() won't needlessly wire megabytes of unused memory into the
 * process.  This must be done using the malloc_conf string so that it gets set
 * up before the first allocation, which happens before entry to main().
 */
const char * malloc_conf = "lg_chunk:0";

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

	if (do_syslog)
		openlog("watchdogd", LOG_CONS|LOG_NDELAY|LOG_PERROR,
		    LOG_DAEMON);

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
				watchdog_onoff(0);
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
	struct timeval tv_start, tv_end, tv_now, tv;
	const char *cmd_prefix, *cmd;
	struct timespec tp_now;
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
		    "%d.%06ld seconds >= %d seconds threshold",
		    cmd_prefix, cmd, sec, (long)tv.tv_usec,
		    carp_thresh_seconds);
	else
		warnx("%s: '%s' took too long: "
		    "%d.%06ld seconds >= %d seconds threshold",
		    cmd_prefix, cmd, sec, (long)tv.tv_usec,
		    carp_thresh_seconds);

	/*
	 * Adjust the sleep interval again in case syslog(3) took a non-trivial
	 * amount of time to run.
	 */
	if (watchdog_getuptime(&tp_now))
		return (sec);
	TIMESPEC_TO_TIMEVAL(&tv_now, &tp_now);
	timersub(&tv_now, &tv_start, &tv);
	sec = tv.tv_sec;

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

		if (failed == 0)
			watchdog_patpat(timeout|WD_ACTIVE);

		waited = watchdog_check_dogfunction_time(&ts_start, &ts_end);
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
	int error;

	/* fake successful watchdog op if a dry run */
	if (is_dry_run)
		return 0;

	if (onoff) {
		/*
		 * Call the WDIOC_SETSOFT regardless of softtimeout_set
		 * because we'll need to turn it off if someone had turned
		 * it on.
		 */
		error = ioctl(fd, WDIOC_SETSOFT, &softtimeout_set);
		if (error) {
			warn("setting WDIOC_SETSOFT %d", softtimeout_set);
			return (error);
		}
		error = watchdog_patpat((timeout|WD_ACTIVE));
		if (error) {
			warn("watchdog_patpat failed");
			goto failsafe;
		}
		if (softtimeout_act_set) {
			error = ioctl(fd, WDIOC_SETSOFTTIMEOUTACT,
			    &softtimeout_act);
			if (error) {
				warn("setting WDIOC_SETSOFTTIMEOUTACT %d",
				    softtimeout_act);
				goto failsafe;
			}
		}
		if (pretimeout_set) {
			error = ioctl(fd, WDIOC_SETPRETIMEOUT, &pretimeout);
			if (error) {
				warn("setting WDIOC_SETPRETIMEOUT %d",
				    pretimeout);
				goto failsafe;
			}
		}
		if (pretimeout_act_set) {
			error = ioctl(fd, WDIOC_SETPRETIMEOUTACT,
			    &pretimeout_act);
			if (error) {
				warn("setting WDIOC_SETPRETIMEOUTACT %d",
				    pretimeout_act);
				goto failsafe;
			}
		}
		/* pat one more time for good measure */
		return watchdog_patpat((timeout|WD_ACTIVE));
	 } else {
		return watchdog_patpat(0);
	 }
failsafe:
	watchdog_patpat(0);
	return (error);
}

/*
 * Tell user how to use the program.
 */
static void
usage(void)
{
	if (is_daemon)
		fprintf(stderr, "usage:\n"
"  watchdogd [-dnSw] [-e cmd] [-I file] [-s sleep] [-t timeout]\n"
"            [-T script_timeout]\n"
"            [--debug]\n"
"            [--pretimeout seconds] [-pretimeout-action action]\n"
"            [--softtimeout] [-softtimeout-action action]\n"
);
	else
		fprintf(stderr, "usage: watchdog [-d] [-t timeout]\n");
	exit(EX_USAGE);
}

static long
fetchtimeout(int opt, const char *longopt, const char *myoptarg)
{
	const char *errstr;
	char *p;
	long rv;

	errstr = NULL;
	p = NULL;
	errno = 0;
	rv = strtol(myoptarg, &p, 0);
	if ((p != NULL && *p != '\0') || errno != 0)
		errstr = "is not a number";
	if (rv <= 0)
		errstr = "must be greater than zero";
	if (errstr) {
		if (longopt) 
			errx(EX_USAGE, "--%s argument %s", longopt, errstr);
		else 
			errx(EX_USAGE, "-%c argument %s", opt, errstr);
	}
	return (rv);
}

struct act_tbl {
	const char *at_act;
	int at_value;
};

static const struct act_tbl act_tbl[] = {
	{ "panic", WD_SOFT_PANIC },
	{ "ddb", WD_SOFT_DDB },
	{ "log", WD_SOFT_LOG },
	{ "printf", WD_SOFT_PRINTF },
	{ NULL, 0 }
};

static void
timeout_act_error(const char *lopt, const char *badact)
{
	char *opts, *oldopts;
	int i;

	opts = NULL;
	for (i = 0; act_tbl[i].at_act != NULL; i++) {
		oldopts = opts;
		if (asprintf(&opts, "%s%s%s",
		    oldopts == NULL ? "" : oldopts,
		    oldopts == NULL ? "" : ", ",
		    act_tbl[i].at_act) == -1)
			err(EX_OSERR, "malloc");
		free(oldopts);
	}
	warnx("bad --%s argument '%s' must be one of (%s).",
	    lopt, badact, opts);
	usage();
}

/*
 * Take a comma separated list of actions and or the flags
 * together for the ioctl.
 */
static int
timeout_act_str2int(const char *lopt, const char *acts)
{
	int i;
	char *dupacts, *tofree;
	char *o;
	int rv = 0;

	tofree = dupacts = strdup(acts);
	if (!tofree)
		err(EX_OSERR, "malloc");
	while ((o = strsep(&dupacts, ",")) != NULL) {
		for (i = 0; act_tbl[i].at_act != NULL; i++) {
			if (!strcmp(o, act_tbl[i].at_act)) {
				rv |= act_tbl[i].at_value;
				break;
			}
		}
		if (act_tbl[i].at_act == NULL)
			timeout_act_error(lopt, o);
	}
	free(tofree);
	return rv;
}

/*
 * Handle the few command line arguments supported.
 */
static void
parseargs(int argc, char *argv[])
{
	int longindex;
	int c;
	char *p;
	const char *lopt;
	double a;

	/*
	 * if we end with a 'd' aka 'watchdogd' then we are the daemon program,
	 * otherwise run as a command line utility.
	 */
	c = strlen(argv[0]);
	if (argv[0][c - 1] == 'd')
		is_daemon = 1;

	if (is_daemon)
		getopt_shortopts = "I:de:ns:t:ST:w?";
	else
		getopt_shortopts = "dt:?";

	while ((c = getopt_long(argc, argv, getopt_shortopts, longopts,
		    &longindex)) != -1) {
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
			nap = fetchtimeout(c, NULL, optarg);
			break;
		case 'S':
			do_syslog = 0;
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
			carp_thresh_seconds = fetchtimeout(c, "NULL", optarg);
			break;
		case 'w':
			do_timedog = 1;
			break;
		case 0:
			lopt = longopts[longindex].name;
			if (!strcmp(lopt, "pretimeout")) {
				pretimeout = fetchtimeout(0, lopt, optarg);
			} else if (!strcmp(lopt, "pretimeout-action")) {
				pretimeout_act = timeout_act_str2int(lopt,
				    optarg);
			} else if (!strcmp(lopt, "softtimeout-action")) {
				softtimeout_act = timeout_act_str2int(lopt,
				    optarg);
			} else {
		/*		warnx("bad option at index %d: %s", optind,
				    argv[optind]);
				usage();
				*/
			}
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
