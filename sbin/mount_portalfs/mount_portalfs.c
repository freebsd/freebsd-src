/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mount_portal.c	8.6 (Berkeley) 4/26/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"
#include "pathnames.h"
#include "portald.h"

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ NULL }
};

static void usage __P((void)) __dead2;

static sig_atomic_t readcf;	/* Set when SIGHUP received */

static void sighup(sig)
int sig;
{
	readcf ++;
}

static void sigchld(sig)
int sig;
{
	pid_t pid;

	while ((pid = waitpid((pid_t) -1, (int *) 0, WNOHANG)) > 0)
		;
	/* wrtp - waitpid _doesn't_ return 0 when no children! */
#ifdef notdef
	if (pid < 0 && errno != ECHILD)
		syslog(LOG_WARNING, "waitpid: %s", strerror(errno));
#endif
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct portal_args args;
	struct sockaddr_un un;
	char *conf;
	char mountpt[MAXPATHLEN];
	int mntflags = 0;
	char tag[32];
	struct vfsconf vfc;
	mode_t um;

	qelem q;
	int rc;
	int so;
	int error = 0;

	/*
	 * Crack command line args
	 */
	int ch;

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch (ch) {
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		default:
			error = 1;
			break;
		}
	}

	if (optind != (argc - 2))
		error = 1;

	if (error)
		usage();

	/*
	 * Get config file and mount point
	 */
	conf = argv[optind];

	/* resolve the mountpoint with realpath(3) */
	(void)checkpath(argv[optind+1], mountpt);

	/*
	 * Construct the listening socket
	 */
	un.sun_family = AF_UNIX;
	if (sizeof(_PATH_TMPPORTAL) >= sizeof(un.sun_path)) {
		errx(EX_SOFTWARE, "portal socket name too long");
	}
	strcpy(un.sun_path, _PATH_TMPPORTAL);
	mktemp(un.sun_path);
	un.sun_len = strlen(un.sun_path);

	so = socket(AF_UNIX, SOCK_STREAM, 0);
	if (so < 0) {
		err(EX_OSERR, "socket");
	}
	um = umask(077);
	(void) unlink(un.sun_path);
	if (bind(so, (struct sockaddr *) &un, sizeof(un)) < 0)
		err(1, NULL);

	(void) unlink(un.sun_path);
	(void) umask(um);

	(void) listen(so, 5);

	args.pa_socket = so;
	sprintf(tag, "portal:%d", getpid());
	args.pa_config = tag;

	error = getvfsbyname("portalfs", &vfc);
	if (error && vfsisloadable("portalfs")) {
		if (vfsload("portalfs"))
			err(EX_OSERR, "vfsload(portalfs)");
		endvfsent();
		error = getvfsbyname("portalfs", &vfc);
	}
	if (error)
		errx(EX_OSERR, "portal filesystem is not available");

	rc = mount(vfc.vfc_name, mountpt, mntflags, &args);
	if (rc < 0)
		err(1, NULL);

	/*
	 * Everything is ready to go - now is a good time to fork
	 */
#ifndef DEBUG
	daemon(0, 0);
#endif

	/*
	 * Start logging (and change name)
	 */
	openlog("portald", LOG_CONS|LOG_PID, LOG_DAEMON);

	q.q_forw = q.q_back = &q;
	readcf = 1;

	signal(SIGCHLD, sigchld);
	signal(SIGHUP, sighup);

	/*
	 * Just loop waiting for new connections and activating them
	 */
	for (;;) {
		struct sockaddr_un un2;
		int len2 = sizeof(un2);
		int so2;
		pid_t pid;
		fd_set fdset;
		int rc;

		/*
		 * Check whether we need to re-read the configuration file
		 */
		if (readcf) {
#ifdef DEBUG
			printf ("re-reading configuration file\n");
#endif
			readcf = 0;
			conf_read(&q, conf);
			continue;
		}

		/*
		 * Accept a new connection
		 * Will get EINTR if a signal has arrived, so just
		 * ignore that error code
		 */
		FD_ZERO(&fdset);
		FD_SET(so, &fdset);
		rc = select(so+1, &fdset, (fd_set *) 0, (fd_set *) 0, (struct timeval *) 0);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "select: %s", strerror(errno));
			exit(EX_OSERR);
		}
		if (rc == 0)
			break;
		so2 = accept(so, (struct sockaddr *) &un2, &len2);
		if (so2 < 0) {
			/*
			 * The unmount function does a shutdown on the socket
			 * which will generated ECONNABORTED on the accept.
			 */
			if (errno == ECONNABORTED)
				break;
			if (errno != EINTR) {
				syslog(LOG_ERR, "accept: %s", strerror(errno));
				exit(EX_OSERR);
			}
			continue;
		}

		/*
		 * Now fork a new child to deal with the connection
		 */
	eagain:;
		switch (pid = fork()) {
		case -1:
			if (errno == EAGAIN) {
				sleep(1);
				goto eagain;
			}
			syslog(LOG_ERR, "fork: %s", strerror(errno));
			break;
		case 0:
			(void) close(so);
			activate(&q, so2);
			exit(0);
		default:
			(void) close(so2);
			break;
		}
	}
	syslog(LOG_INFO, "%s unmounted", mountpt);
	exit(0);
}

static void
usage()
{
	(void)fprintf(stderr,
		"usage: mount_portalfs [-o options] config mount-point\n");
	exit(EX_USAGE);
}
