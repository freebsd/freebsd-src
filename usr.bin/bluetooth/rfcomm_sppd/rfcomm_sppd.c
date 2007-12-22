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
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
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
	int			 n, background, channel, service,
				 s, amaster, aslave, fd, doserver;
	fd_set			 rfd;
	char			*tty = NULL, *ep = NULL, buf[SPPD_BUFFER_SIZE];

	memcpy(&addr, NG_HCI_BDADDR_ANY, sizeof(addr));
	background = channel = 0;
	service = SDP_SERVICE_CLASS_SERIAL_PORT;
	doserver = 0;

	/* Parse command line options */
	while ((n = getopt(argc, argv, "a:bc:t:hS")) != -1) {
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
			channel = strtoul(optarg, &ep, 10);
			if (*ep != '\0') {
				channel = 0;
				switch (tolower(optarg[0])) {
				case 'd': /* DialUp Networking */
					service = SDP_SERVICE_CLASS_DIALUP_NETWORKING;
					break;

				case 'f': /* Fax */
					service = SDP_SERVICE_CLASS_FAX;
					break;

				case 'l': /* LAN */
					service = SDP_SERVICE_CLASS_LAN_ACCESS_USING_PPP;
					break;

				case 's': /* Serial Port */
					service = SDP_SERVICE_CLASS_SERIAL_PORT;
					break;

				default:
					errx(1, "Unknown service name: %s",
						optarg);
					/* NOT REACHED */
				}
			}
			break;

		case 'b': /* Run in background */
			background = 1;
			break;

		case 't': /* Slave TTY name */
			if (optarg[0] != '/')
				asprintf(&tty, "%s%s", _PATH_DEV, optarg);
			else
				tty = optarg;
			break;

		case 'S':
			doserver = 1;
			break;

		case 'h':
		default:
			usage();
			/* NOT REACHED */
		}
	}

	/* Check if we have everything we need */
	if (!doserver && memcmp(&addr, NG_HCI_BDADDR_ANY, sizeof(addr)) == 0)
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

	/* Open TTYs */
	if (tty == NULL) {
		if (background || doserver)
			usage();

		amaster = STDIN_FILENO;
		fd = STDOUT_FILENO;
	} else {
		if (sppd_ttys_open(tty, &amaster, &aslave) < 0)
			exit(1);
		
		fd = amaster;
	}		

	/* Open RFCOMM connection */

	if (doserver) {
		struct sockaddr_rfcomm	 ma;
		bdaddr_t		 bt_addr_any;
		sdp_lan_profile_t	 lan;
		void			*ss;
		uint32_t		 sdp_handle;
		int			 acceptsock, aaddrlen;

		if (channel == 0) {
			/* XXX: should check if selected channel is unused */
			channel = (getpid() % 30) + 1;
		}
		acceptsock = socket(PF_BLUETOOTH, SOCK_STREAM,
		    BLUETOOTH_PROTO_RFCOMM);
		if (acceptsock < 0)
			err(1, "Could not create socket");

		memset(&ma, 0, sizeof(ma));
		ma.rfcomm_len = sizeof(ma);
		ma.rfcomm_family = AF_BLUETOOTH;
		ma.rfcomm_channel = channel;

		if (bind(acceptsock, (struct sockaddr *)&ma, sizeof(ma)) < 0)
			err(1, "Could not bind socket -- channel %d in use?",
			    channel);
		if (listen(acceptsock, 10) != 0)
			err(1, "Could not listen on socket");

		ss = sdp_open_local(NULL);
		if (ss == NULL)
			errx(1, "Unable to create local SDP session");
		if (sdp_error(ss) != 0)
			errx(1, "Unable to open local SDP session. %s (%d)",
			    strerror(sdp_error(ss)), sdp_error(ss));
		memset(&lan, 0, sizeof(lan));
		lan.server_channel = channel;

		memcpy(&bt_addr_any, NG_HCI_BDADDR_ANY, sizeof(bt_addr_any));
		if (sdp_register_service(ss, service, &bt_addr_any,
		    (void *)&lan, sizeof(lan), &sdp_handle) != 0) {
			errx(1, "Unable to register LAN service with "
			    "local SDP daemon. %s (%d)",
			    strerror(sdp_error(ss)), sdp_error(ss));
		}

		s = -1;
		while (s < 0) {
			aaddrlen = sizeof(ra);
			s = accept(acceptsock, (struct sockaddr *)&ra,
			    &aaddrlen);
			if (s < 0)
				err(1, "Unable to accept()");
			if (memcmp(&addr, NG_HCI_BDADDR_ANY, sizeof(addr)) &&
			    memcmp(&addr, &ra.rfcomm_bdaddr, sizeof(addr))) {
				warnx("Connect from wrong client");
				close(s);
				s = -1;
			}
		}
		sdp_unregister_service(ss, sdp_handle);
		sdp_close(ss);
		close(acceptsock);
	} else {
		/* Check channel, if was not set then obtain it via SDP */
		if (channel == 0 && service != 0)
			if (rfcomm_channel_lookup(NULL, &addr,
				    service, &channel, &n) != 0)
				errc(1, n, "Could not obtain RFCOMM channel");
		if (channel <= 0 || channel > 30)
			errx(1, "Invalid RFCOMM channel number %d", channel);

		s = socket(PF_BLUETOOTH, SOCK_STREAM, BLUETOOTH_PROTO_RFCOMM);
		if (s < 0)
			err(1, "Could not create socket");

		memset(&ra, 0, sizeof(ra));
		ra.rfcomm_len = sizeof(ra);
		ra.rfcomm_family = AF_BLUETOOTH;

		if (bind(s, (struct sockaddr *) &ra, sizeof(ra)) < 0)
			err(1, "Could not bind socket");

		memcpy(&ra.rfcomm_bdaddr, &addr, sizeof(ra.rfcomm_bdaddr));
		ra.rfcomm_channel = channel;

		if (connect(s, (struct sockaddr *) &ra, sizeof(ra)) < 0)
			err(1, "Could not connect socket");
	}

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
	syslog(LOG_INFO, "Starting on %s...", (tty != NULL)? tty : "stdin/stdout");

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

			if (sppd_write(fd, buf, n) < 0) {
				syslog(LOG_ERR, "Could not write to master " \
					"pty, fd=%d, size=%d. %s",
					fd, n, strerror(errno));
				exit(1);
			}
		}
	}

	syslog(LOG_INFO, "Completed on %s", (tty != NULL)? tty : "stdin/stdout");
	closelog();

	close(s);

	if (tty != NULL) {
		close(aslave);
		close(amaster);
	}	

	return (0);
}

/* Open TTYs */
static int
sppd_ttys_open(char const *tty, int *amaster, int *aslave)
{
	char		 pty[PATH_MAX], *slash;
	struct group	*gr = NULL;
	gid_t		 ttygid;
	struct termios	 tio;

	/*
	 * Construct master PTY name. The slave tty name must be less then
	 * PATH_MAX characters in length, must contain '/' character and 
	 * must not end with '/'.
	 */

	if (strlen(tty) >= sizeof(pty)) {
		syslog(LOG_ERR, "Slave tty name is too long");
		return (-1);
	}

	strlcpy(pty, tty, sizeof(pty));
	slash = strrchr(pty, '/');
	if (slash == NULL || slash[1] == '\0') {
		syslog(LOG_ERR, "Invalid slave tty name (%s)", tty);
		return (-1);
	}

	slash[1] = 'p';
	
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
	(void) chmod(tty, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
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
"\t-a address Peer address (required in client mode)\n" \
"\t-b         Run in background\n" \
"\t-c channel RFCOMM channel to connect to or listen on\n" \
"\t-t tty     TTY name (required in background or server mode)\n" \
"\t-S         Server mode\n" \
"\t-h         Display this message\n", SPPD_IDENT);
	exit(255);
} /* usage */

