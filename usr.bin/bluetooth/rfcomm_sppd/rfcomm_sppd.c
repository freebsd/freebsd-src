/*
 * rfcomm_sppd.c
 *
 * Copyright (c) 2003 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: rfcomm_sppd.c,v 1.4 2003/09/07 18:15:55 max Exp $
 * $FreeBSD$
 */

#include <sys/stat.h>
#include <bluetooth.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <sdp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

#define SPPD_IDENT		"rfcomm_sppd"
#define SPPD_BUFFER_SIZE	1024
#define max(a, b)		(((a) > (b))? (a) : (b))

int		rfcomm_channel_lookup	(bdaddr_t const *local,
					 bdaddr_t const *remote, 
					 int service, int *channel, int *error);

static int	sppd_ttys_open	(char const *tty, int *amaster, int *aslave);
static int	sppd_read	(int fd, char *buffer, int size);
static int	sppd_write	(int fd, char *buffer, int size);
static void	sppd_sighandler	(int s);
static void	usage		(void);

static int	done;	/* are we done? */

/* Main */
int
main(int argc, char *argv[]) 
{
	struct sigaction	 sa;
	struct sockaddr_rfcomm	 ra;
	bdaddr_t		 addr;
	int			 n, background, channel, s, amaster, aslave;
	fd_set			 rfd;
	char			*tty = NULL, buf[SPPD_BUFFER_SIZE];

	memcpy(&addr, NG_HCI_BDADDR_ANY, sizeof(addr));
	background = channel = 0;

	/* Parse command line options */
	while ((n = getopt(argc, argv, "a:bc:t:h")) != -1) {
		switch (n) { 
		case 'a': /* BDADDR */
			if (!bt_aton(optarg, &addr)) {
				struct hostent	*he = NULL;

				if ((he = bt_gethostbyname(optarg)) == NULL)
					errx(1, "%s: %s", optarg, hstrerror(h_errno));

				memcpy(&addr, he->h_addr, sizeof(addr));
			}
			break;

		case 'c': /* RFCOMM channel */
			channel = atoi(optarg);
			break;

		case 'b': /* Run in background */
			background = 1;
			break;

		case 't': /* Slave TTY name */
			tty = optarg;
			break;

		case 'h':
		default:
			usage();
			/* NOT REACHED */
		}
	}

	/* Check if we have everything we need */
	if (tty == NULL || memcmp(&addr, NG_HCI_BDADDR_ANY, sizeof(addr)) == 0)
		usage();
		/* NOT REACHED */

	/* Set signal handlers */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sppd_sighandler;

	if (sigaction(SIGTERM, &sa, NULL) < 0)
		err(1, "Could not sigaction(SIGTERM)");
 
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		err(1, "Could not sigaction(SIGHUP)");
 
	if (sigaction(SIGINT, &sa, NULL) < 0)
		err(1, "Could not sigaction(SIGINT)");

	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_NOCLDWAIT;

	if (sigaction(SIGCHLD, &sa, NULL) < 0)
		err(1, "Could not sigaction(SIGCHLD)");

	/* Check channel, if was not set then obtain it via SDP */
	if (channel == 0)
		if (rfcomm_channel_lookup(NULL, &addr,
			    SDP_SERVICE_CLASS_SERIAL_PORT, &channel, &n) != 0)
			errc(1, n, "Could not obtain RFCOMM channel");
	if (channel <= 0 || channel > 30)
		errx(1, "Invalid RFCOMM channel number %d", channel);

	/* Open TTYs */
	if (sppd_ttys_open(tty, &amaster, &aslave) < 0)
		exit(1);

	/* Open RFCOMM connection */
	memset(&ra, 0, sizeof(ra));
	ra.rfcomm_len = sizeof(ra);
	ra.rfcomm_family = AF_BLUETOOTH;

	s = socket(PF_BLUETOOTH, SOCK_STREAM, BLUETOOTH_PROTO_RFCOMM);
	if (s < 0)
		err(1, "Could not create socket");

	if (bind(s, (struct sockaddr *) &ra, sizeof(ra)) < 0)
		err(1, "Could not bind socket");

	memcpy(&ra.rfcomm_bdaddr, &addr, sizeof(ra.rfcomm_bdaddr));
	ra.rfcomm_channel = channel;

	if (connect(s, (struct sockaddr *) &ra, sizeof(ra)) < 0)
		err(1, "Could not connect socket");

	/* Became daemon if required */
	if (background) {
		switch (fork()) {
		case -1:
			err(1, "Could not fork()");
			/* NOT REACHED */

		case 0:
			exit(0);
			/* NOT REACHED */

		default:
			if (daemon(0, 0) < 0)
				err(1, "Could not daemon()");
			break;
		}
	}

	openlog(SPPD_IDENT, LOG_NDELAY|LOG_PERROR|LOG_PID, LOG_DAEMON);
	syslog(LOG_INFO, "Starting on %s...", tty);

	for (done = 0; !done; ) {
		FD_ZERO(&rfd);
		FD_SET(amaster, &rfd);
		FD_SET(s, &rfd);

		n = select(max(amaster, s) + 1, &rfd, NULL, NULL, NULL);
		if (n < 0) {
			if (errno == EINTR)
				continue;

			syslog(LOG_ERR, "Could not select(). %s",
					strerror(errno));
			exit(1);
		}

		if (n == 0)
			continue;

		if (FD_ISSET(amaster, &rfd)) {
			n = sppd_read(amaster, buf, sizeof(buf));
			if (n < 0) {
				syslog(LOG_ERR, "Could not read master pty, " \
					"fd=%d. %s", amaster, strerror(errno));
				exit(1);
			}

			if (n == 0)
				break; /* XXX */

			if (sppd_write(s, buf, n) < 0) {
				syslog(LOG_ERR, "Could not write to socket, " \
					"fd=%d, size=%d. %s",
					s, n, strerror(errno));
				exit(1);
			}
		}

		if (FD_ISSET(s, &rfd)) {
			n = sppd_read(s, buf, sizeof(buf));
			if (n < 0) {
				syslog(LOG_ERR, "Could not read socket, " \
					"fd=%d. %s", s, strerror(errno));
				exit(1);
			}

			if (n == 0)
				break;

			if (sppd_write(amaster, buf, n) < 0) {
				syslog(LOG_ERR, "Could not write to master " \
					"pty, fd=%d, size=%d. %s",
					amaster, n, strerror(errno));
				exit(1);
			}
		}
	}

	syslog(LOG_INFO, "Completed on %s", tty);
	closelog();

	close(s);
	close(aslave);
	close(amaster);

	return (0);
}

/* Open TTYs */
static int
sppd_ttys_open(char const *tty, int *amaster, int *aslave)
{
	char		 pty[PATH_MAX];
	struct group	*gr = NULL;
	gid_t		 ttygid;
	struct termios	 tio;

	/*
	 * Master PTY
	 */

	strlcpy(pty, tty, sizeof(pty));
	pty[5] = 'p';

	if (strcmp(pty, tty) == 0) {
		syslog(LOG_ERR, "Master and slave tty are the same (%s)", tty);
		return (-1);
	}

	if ((*amaster = open(pty, O_RDWR, 0)) < 0) {
		syslog(LOG_ERR, "Could not open(%s). %s", pty, strerror(errno));
		return (-1);
	}

	/*
	 * Slave TTY
	 */

	if ((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
		ttygid = -1;

	(void) chown(tty, getuid(), ttygid);
	(void) chmod(tty, S_IRUSR|S_IWUSR|S_IWGRP);
	(void) revoke(tty);

	if ((*aslave = open(tty, O_RDWR, 0)) < 0) {
		syslog(LOG_ERR, "Could not open(%s). %s", tty, strerror(errno));
		close(*amaster);
		return (-1);
	}

	/*
	 * Make slave TTY raw
	 */

	cfmakeraw(&tio);

	if (tcsetattr(*aslave, TCSANOW, &tio) < 0) {
		syslog(LOG_ERR, "Could not tcsetattr(). %s", strerror(errno));
		close(*aslave);
		close(*amaster);
		return (-1);
	}

	return (0);
} /* sppd_ttys_open */

/* Read data */
static int
sppd_read(int fd, char *buffer, int size)
{
	int	n;

again:
	n = read(fd, buffer, size);
	if (n < 0) {
		if (errno == EINTR)
			goto again;

		return (-1);
	}

	return (n);
} /* sppd_read */

/* Write data */
static int
sppd_write(int fd, char *buffer, int size)
{
	int	n, wrote;

	for (wrote = 0; size > 0; ) {
		n = write(fd, buffer, size);
		switch (n) {
		case -1:
			if (errno != EINTR)
				return (-1);
			break;

		case 0: 
			/* XXX can happen? */
			break;

		default:
			wrote += n;
			buffer += n;
			size -= n;
			break;
		}
	}

	return (wrote);
} /* sppd_write */

/* Signal handler */
static void
sppd_sighandler(int s)
{
	syslog(LOG_INFO, "Signal %d received. Total %d signals received\n",
			s, ++ done);
} /* sppd_sighandler */
 
/* Display usage and exit */
static void
usage(void)
{
	fprintf(stdout,
"Usage: %s options\n" \
"Where options are:\n" \
"\t-a bdaddr  BDADDR to connect to (required)\n" \
"\t-b         Run in background\n" \
"\t-c channel RFCOMM channel to connect to\n" \
"\t-t tty     TTY name\n" \
"\t-h         Display this message\n", SPPD_IDENT);

	exit(255);
} /* usage */

