/*
 * Copyright (c) 2003  Sean M. Kelly <smkelly@FreeBSD.org>
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

#include <sys/cdefs.h>                                                          
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/rtprio.h>
#include <sys/stat.h>

#include <err.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <signal.h>

static void	parseargs(int, char *[]);
static void	sighandler(int);
static void	watchdog_loop(void);
static int	watchdog_init(void);
static int	watchdog_onoff(int onoff);
static int	watchdog_tickle(void);
static void	usage(void);

int debugging = 0;
int end_program = 0;
const char *pidfile = _PATH_VARRUN "watchdogd.pid";
int reset_mib[3];
size_t reset_miblen = 3;

/*
 * Periodically write to the debug.watchdog.reset sysctl OID
 * to keep the software watchdog from firing.
 */
int
main(int argc, char *argv[])
{
	struct rtprio rtp;
	FILE *fp;

	if (getuid() != 0)
		errx(EX_SOFTWARE, "not super user");
		
	parseargs(argc, argv);

	rtp.type = RTP_PRIO_REALTIME;
	rtp.prio = 0;
	if (rtprio(RTP_SET, 0, &rtp) == -1)
		err(EX_OSERR, "rtprio");

	if (watchdog_init() == -1)
		exit(EX_SOFTWARE);

	if (watchdog_onoff(1) == -1)
		exit(EX_SOFTWARE);

	if (debugging == 0 && daemon(0, 0) == -1) {
		watchdog_onoff(0);
		err(EX_OSERR, "daemon");
	}

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	fp = fopen(pidfile, "w");
	if (fp != NULL) {
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}

	watchdog_loop();

	/* exiting */
	watchdog_onoff(0);
	unlink(pidfile);
	return (EX_OK);
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
 * Locate the OID for the 'debug.watchdog.reset' sysctl setting.
 * Upon finding it, do an initial reset on the watchdog.
 */
static int
watchdog_init()
{
	int error;

	error = sysctlnametomib("debug.watchdog.reset", reset_mib, 
		&reset_miblen);
	if (error == -1) {
		fprintf(stderr, "Could not find reset OID: %s\n",
			strerror(errno));
		return (error);
	}
	return watchdog_tickle();
}

/*
 * Main program loop which is iterated every second.
 */
static void
watchdog_loop(void)
{
	struct stat sb;
	int failed;

	while (end_program == 0) {
		failed = 0;

		failed = stat("/etc", &sb);

		if (failed == 0)
			watchdog_tickle();
		sleep(1);
	}
}

/*
 * Reset the watchdog timer. This function must be called periodically
 * to keep the watchdog from firing.
 */
int
watchdog_tickle(void)
{

	return sysctl(reset_mib, reset_miblen, NULL, NULL, NULL, 0);
}

/*
 * Toggle the kernel's watchdog. This routine is used to enable and
 * disable the watchdog.
 */
static int
watchdog_onoff(int onoff)
{
	int mib[3];
	int error;
	size_t len;

	len = 3;

	error = sysctlnametomib("debug.watchdog.enabled", mib, &len);
	if (error == 0)
		error = sysctl(mib, len, NULL, NULL, &onoff, sizeof(onoff));

	if (error == -1) {
		fprintf(stderr, "Could not %s watchdog: %s\n",
			(onoff > 0) ? "enable" : "disable",
			strerror(errno));
		return (error);
	}
	return (0);
}

/*
 * Tell user how to use the program.
 */
static void
usage()
{
	fprintf(stderr, "usage: watchdogd [-d] [-I file]\n");
	exit(EX_USAGE);
}

/*
 * Handle the few command line arguments supported.
 */
static void
parseargs(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "I:d?")) != -1) {
		switch (c) {
		case 'I':
			pidfile = optarg;
			break;
		case 'd':
			debugging = 1;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
}
