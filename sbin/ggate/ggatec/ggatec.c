/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <err.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/bio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <geom/gate/g_gate.h>
#include "ggate.h"


enum { UNSET, ATTACH, CREATE, DESTROY, LIST } action = UNSET;

static const char *path = NULL;
static const char *host = NULL;
static int unit = -1;
static unsigned flags = 0;
static int force = 0;
static int nagle = 1;
static unsigned queue_size = G_GATE_QUEUE_SIZE;
static unsigned port = G_GATE_PORT;
static off_t mediasize;
static unsigned sectorsize = 0;
static unsigned timeout = G_GATE_TIMEOUT;
static unsigned rcvbuf = G_GATE_RCVBUF;
static unsigned sndbuf = G_GATE_SNDBUF;

static void
usage(void)
{

	fprintf(stderr, "usage: %s create [-nv] [-o <ro|wo|rw>] [-p port] "
	    "[-q queue_size] [-R rcvbuf] [-S sndbuf] [-s sectorsize] "
	    "[-t timeout] [-u unit] <host> <path>\n", getprogname());
	fprintf(stderr, "       %s attach [-nv] [-o <ro|wo|rw>] [-p port] "
	    "[-R rcvbuf] [-S sndbuf] <-u unit> <host> <path>\n", getprogname());
	fprintf(stderr, "       %s destroy [-f] <-u unit>\n", getprogname());
	fprintf(stderr, "       %s list [-v] [-u unit]\n", getprogname());
	exit(EXIT_FAILURE);
}

static int
handshake(void)
{
	struct g_gate_cinit cinit;
	struct g_gate_sinit sinit;
	struct sockaddr_in serv;
	struct timeval tv;
	size_t bsize;
	int sfd;

	/*
	 * Do the network stuff.
	 */
	bzero(&serv, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = g_gate_str2ip(host);
	if (serv.sin_addr.s_addr == INADDR_NONE) {
		g_gate_log(LOG_ERR, "Invalid IP/host name: %s.", host);
		return (-1);
	}
	serv.sin_port = htons(port);
	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1)
		g_gate_xlog("Can't open socket: %s.", strerror(errno));
	/*
	 * Some trivial network optimalization.
	 * This should be much more advanced.
	 */
	if (nagle) {
		int on = 1;

		if (setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &on,
		    sizeof(on)) == -1) {
			g_gate_xlog("setsockopt() error: %s.", strerror(errno));
		}
	}
	bsize = rcvbuf;
	if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &bsize, sizeof(bsize)) == -1)
		g_gate_xlog("setsockopt() error: %s.", strerror(errno));
	bsize = sndbuf;
	if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &bsize, sizeof(bsize)) == -1)
		g_gate_xlog("setsockopt() error: %s.", strerror(errno));
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	if (setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1 ||
	    setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
		g_gate_xlog("setsockopt() error: %s.", strerror(errno));
	}
	if (connect(sfd, (struct sockaddr *)&serv, sizeof(serv)) == -1) {
		g_gate_log(LOG_ERR, "Can't connect to server: %s.",
		    strerror(errno));
		return (-1);
	}

	g_gate_log(LOG_INFO, "Connected to the server: %s:%d.", host, port);

	/*
	 * Creating and sending initial packet.
	 */
	if (strlcpy(cinit.gc_path, path, sizeof(cinit.gc_path)) >=
	    sizeof(cinit.gc_path)) {
	        g_gate_xlog("Path name too long.");
	}
	cinit.gc_flags = flags;
	g_gate_log(LOG_DEBUG, "Sending initial packet.");
	g_gate_swap2n_cinit(&cinit);
	if (send(sfd, &cinit, sizeof(cinit), 0) == -1) {
	        g_gate_log(LOG_ERR, "Error while sending initial packet: %s.",
		    strerror(errno));
		return (-1);
	}
	g_gate_swap2h_cinit(&cinit);

	/*
	 * Receiving initial packet from server.
	 */
	g_gate_log(LOG_DEBUG, "Receiving initial packet.");
	if (recv(sfd, &sinit, sizeof(sinit), MSG_WAITALL) == -1) {
		g_gate_log(LOG_ERR, "Error while receiving data: %s.",
		    strerror(errno));
		return (-1);
	}
	g_gate_swap2h_sinit(&sinit);
	if (sinit.gs_error != 0)
	        g_gate_xlog("Error from server: %s.", strerror(sinit.gs_error));

	mediasize = sinit.gs_mediasize;
	if (sectorsize == 0)
		sectorsize = sinit.gs_sectorsize;
	return (sfd);
}

static int
serve(int sfd)
{
	struct g_gate_ctl_io ggio;
	size_t bsize;
	char *buf;

	bsize = G_GATE_BUFSIZE_START;
	buf = malloc(bsize);
	if (buf == NULL) {
		if (action == CREATE)
			g_gate_destroy(unit, 1);
		g_gate_xlog("No enough memory");
	}

	ggio.gctl_version = G_GATE_VERSION;
	ggio.gctl_unit = unit;
	bsize = sectorsize;
	ggio.gctl_data = malloc(bsize);
	for (;;) {
		struct g_gate_hdr hdr;
		int data, error;
once_again:
		ggio.gctl_length = bsize;
		ggio.gctl_error = 0;
		g_gate_ioctl(G_GATE_CMD_START, &ggio);
		error = ggio.gctl_error;
		switch (error) {
		case 0:
			break;
		case ECANCELED:
			/* Exit gracefully. */
			free(ggio.gctl_data);
			g_gate_close_device();
			close(sfd);
			exit(EXIT_SUCCESS);
		case ENOMEM:
			/* Buffer too small. */
			ggio.gctl_data = realloc(ggio.gctl_data,
			    ggio.gctl_length);
			if (ggio.gctl_data != NULL) {
				bsize = ggio.gctl_length;
				goto once_again;
			}
			/* FALLTHROUGH */
		case ENXIO:
		default:
			g_gate_xlog("ioctl(/dev/%s): %s.", G_GATE_CTL_NAME,
			    strerror(error));
		}

		hdr.gh_cmd = ggio.gctl_cmd;
		hdr.gh_offset = ggio.gctl_offset;
		hdr.gh_length = ggio.gctl_length;
		hdr.gh_error = 0;
		g_gate_swap2n_hdr(&hdr);
		data = send(sfd, &hdr, sizeof(hdr), 0);
		g_gate_log(LOG_DEBUG, "Sent hdr packet.");
		g_gate_swap2h_hdr(&hdr);
		if (data != sizeof(hdr)) {
			ggio.gctl_error = EAGAIN;
			goto done;
		}
		if (ggio.gctl_cmd == BIO_DELETE || ggio.gctl_cmd == BIO_WRITE) {
			data = send(sfd, ggio.gctl_data, ggio.gctl_length, 0);
			g_gate_log(LOG_DEBUG, "Sent data packet.");
			if (data != ggio.gctl_length) {
				ggio.gctl_error = EAGAIN;
				goto done;
			}
			g_gate_log(LOG_DEBUG, "Sent %d bytes (offset=%llu, "
			    "size=%u).", data, hdr.gh_offset, hdr.gh_length);
		}
		data = recv(sfd, &hdr, sizeof(hdr), MSG_WAITALL);
		g_gate_log(LOG_DEBUG, "Received hdr packet.");
		g_gate_swap2h_hdr(&hdr);
		if (data != sizeof(hdr)) {
			ggio.gctl_error = EIO;
			goto done;
		}
		if (ggio.gctl_cmd == BIO_READ) {
			if (bsize < (size_t)ggio.gctl_length) {
				ggio.gctl_data = realloc(ggio.gctl_data,
				    ggio.gctl_length);
				if (ggio.gctl_data != NULL)
					bsize = ggio.gctl_length;
				else
					g_gate_xlog("No memory.");
			}
			data = recv(sfd, ggio.gctl_data, ggio.gctl_length,
			    MSG_WAITALL);
			g_gate_log(LOG_DEBUG, "Received data packet.");
			if (data != ggio.gctl_length) {
				ggio.gctl_error = EAGAIN;
				goto done;
			}
			g_gate_log(LOG_DEBUG, "Received %d bytes (offset=%ju, "
			    "size=%zu).", data, (uintmax_t)hdr.gh_offset,
			    (size_t)hdr.gh_length);
		}
done:
		g_gate_ioctl(G_GATE_CMD_DONE, &ggio);
		if (ggio.gctl_error == EAGAIN)
			return (ggio.gctl_error);
	}
	/* NOTREACHED */
	return (0);
}

static void
serve_loop(int sfd)
{

	for (;;) {
		int error;

		error = serve(sfd);
		close(sfd);
		if (error != EAGAIN)
			g_gate_xlog("%s.", strerror(error));
		sfd = handshake();
		if (sfd == -1) {
			sleep(2);
			continue;
		}
	}
}

static void
mydaemon(void)
{

	if (g_gate_verbose > 0)
		return;
	if (daemon(0, 0) == 0)
		return;
	if (action == CREATE)
		g_gate_destroy(unit, 1);
	err(EXIT_FAILURE, "Cannot daemonize");
}

static void
g_gatec_attach(void)
{
	int sfd;

	sfd = handshake();
	g_gate_log(LOG_DEBUG, "Worker created: %u.", getpid());
	mydaemon();
	serve_loop(sfd);
}

static void
g_gatec_create(void)
{
	struct g_gate_ctl_create ggioc;
	int sfd;

	sfd = handshake();
	if (sfd == -1)
		exit(EXIT_FAILURE);
	ggioc.gctl_version = G_GATE_VERSION;
	ggioc.gctl_mediasize = mediasize;
	ggioc.gctl_sectorsize = sectorsize;
	ggioc.gctl_flags = flags;
	ggioc.gctl_maxcount = queue_size;
	ggioc.gctl_timeout = timeout;
	ggioc.gctl_unit = unit;
	snprintf(ggioc.gctl_info, sizeof(ggioc.gctl_info), "%s:%u %s", host,
	    port, path);
	g_gate_ioctl(G_GATE_CMD_CREATE, &ggioc);
	g_gate_log(LOG_DEBUG, "Worker created: %u.", getpid());
	if (unit == -1)
		printf("%s%u\n", G_GATE_PROVIDER_NAME, ggioc.gctl_unit);
	unit = ggioc.gctl_unit;
	mydaemon();
	serve_loop(sfd);
}

int
main(int argc, char *argv[])
{

	if (argc < 2)
		usage();
	if (strcasecmp(argv[1], "attach") == 0)
		action = ATTACH;
	else if (strcasecmp(argv[1], "create") == 0)
		action = CREATE;
	else if (strcasecmp(argv[1], "destroy") == 0)
		action = DESTROY;
	else if (strcasecmp(argv[1], "list") == 0)
		action = LIST;
	else
		usage();
	argc -= 1;
	argv += 1;
	for (;;) {
		int ch;

		ch = getopt(argc, argv, "fno:p:q:R:S:s:t:u:v");
		if (ch == -1)
			break;
		switch (ch) {
		case 'f':
			if (action != DESTROY)
				usage();
			force = 1;
			break;
		case 'n':
			if (action != ATTACH && action != CREATE)
				usage();
			nagle = 0;
			break;
		case 'o':
			if (action != ATTACH && action != CREATE)
				usage();
			if (strcasecmp("ro", optarg) == 0)
				flags = G_GATE_FLAG_READONLY;
			else if (strcasecmp("wo", optarg) == 0)
				flags = G_GATE_FLAG_WRITEONLY;
			else if (strcasecmp("rw", optarg) == 0)
				flags = 0;
			else {
				errx(EXIT_FAILURE,
				    "Invalid argument for '-o' option.");
			}
			break;
		case 'p':
			if (action != ATTACH && action != CREATE)
				usage();
			errno = 0;
			port = strtoul(optarg, NULL, 10);
			if (port == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid port.");
			break;
		case 'q':
			if (action != CREATE)
				usage();
			errno = 0;
			queue_size = strtoul(optarg, NULL, 10);
			if (queue_size == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid queue_size.");
			break;
		case 'R':
			if (action != ATTACH && action != CREATE)
				usage();
			errno = 0;
			rcvbuf = strtoul(optarg, NULL, 10);
			if (rcvbuf == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid rcvbuf.");
			break;
		case 'S':
			if (action != ATTACH && action != CREATE)
				usage();
			errno = 0;
			sndbuf = strtoul(optarg, NULL, 10);
			if (sndbuf == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid sndbuf.");
			break;
		case 's':
			if (action != CREATE)
				usage();
			errno = 0;
			sectorsize = strtoul(optarg, NULL, 10);
			if (sectorsize == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid sectorsize.");
			break;
		case 't':
			if (action != CREATE)
				usage();
			errno = 0;
			timeout = strtoul(optarg, NULL, 10);
			if (timeout == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid timeout.");
			break;
		case 'u':
			errno = 0;
			unit = strtol(optarg, NULL, 10);
			if (unit == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid unit number.");
			break;
		case 'v':
			if (action == DESTROY)
				usage();
			g_gate_verbose++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	switch (action) {
	case ATTACH:
		if (argc != 2)
			usage();
		if (unit == -1) {
			fprintf(stderr, "Required unit number.\n");
			usage();
		}
		g_gate_open_device();
		host = argv[0];
		path = argv[1];
		g_gatec_attach();
		break;
	case CREATE:
		if (argc != 2)
			usage();
		g_gate_load_module();
		g_gate_open_device();
		host = argv[0];
		path = argv[1];
		g_gatec_create();
		break;
	case DESTROY:
		if (unit == -1) {
			fprintf(stderr, "Required unit number.\n");
			usage();
		}
		g_gate_verbose = 1;
		g_gate_open_device();
		g_gate_destroy(unit, force);
		break;
	case LIST:
		g_gate_list(unit, g_gate_verbose);
		break;
	case UNSET:
	default:
		usage();
	}
	g_gate_close_device();
	exit(EXIT_SUCCESS);
}
