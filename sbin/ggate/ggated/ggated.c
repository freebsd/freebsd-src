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
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/endian.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/disk.h>
#include <sys/bio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <syslog.h>
#include <stdarg.h>

#include <geom/gate/g_gate.h>
#include "ggate.h"


#define	G_GATED_EXPORT_FILE	"/etc/gg.exports"
#define	G_GATED_DEBUG(...)						\
	if (g_gate_verbose) {						\
		printf(__VA_ARGS__);					\
		printf("\n");						\
	}

static const char *exports = G_GATED_EXPORT_FILE;
static int got_sighup = 0;
static int nagle = 1;
static unsigned rcvbuf = G_GATE_RCVBUF;
static unsigned sndbuf = G_GATE_SNDBUF;

struct export {
	char		*e_path;	/* path to device/file */
	in_addr_t	 e_ip;		/* remote IP address */
	in_addr_t	 e_mask;	/* IP mask */
	unsigned	 e_flags;	/* flags (RO/RW) */
	SLIST_ENTRY(export) e_next;
};
static SLIST_HEAD(, export) exports_list =
    SLIST_HEAD_INITIALIZER(&exports_list);

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-nv] [-a address] [-p port] [-R rcvbuf] "
	    "[-S sndbuf] [exports file]\n", getprogname());
	exit(EXIT_FAILURE);
}

static char *
ip2str(in_addr_t ip)
{
	static char sip[16];

	snprintf(sip, sizeof(sip), "%u.%u.%u.%u",
	    ((ip >> 24) & 0xff),
	    ((ip >> 16) & 0xff),
	    ((ip >> 8) & 0xff),
	    (ip & 0xff));
	return (sip);
}

static in_addr_t
countmask(unsigned m)
{
	in_addr_t mask;

	if (m == 0) {
		mask = 0x0;
	} else {
		mask = 1 << (32 - m);
		mask--;
		mask = ~mask;
	}
	return (mask);
}

static void
line_parse(char *line, unsigned lineno)
{
	struct export *ex;
	char *word, *path, *sflags;
	unsigned flags, i, vmask;
	in_addr_t ip, mask;

	ip = mask = flags = vmask = 0;
	path = NULL;
	sflags = NULL;

	for (i = 0, word = strtok(line, " \t"); word != NULL;
	    i++, word = strtok(NULL, " \t")) {
		switch (i) {
		case 0: /* IP address or host name */
			ip = g_gate_str2ip(strsep(&word, "/"));
			if (ip == INADDR_NONE) {
				g_gate_xlog("Invalid IP/host name at line %u.",
				    lineno);
			}
			ip = ntohl(ip);
			if (word == NULL)
				vmask = 32;
			else {
				errno = 0;
				vmask = strtoul(word, NULL, 10);
				if (vmask == 0 && errno != 0) {
					g_gate_xlog("Invalid IP mask value at "
					    "line %u.", lineno);
				}
				if ((unsigned)vmask > 32) {
					g_gate_xlog("Invalid IP mask value at line %u.",
					    lineno);
				}
			}
			mask = countmask(vmask);
			break;
		case 1:	/* flags */
			if (strcasecmp("rd", word) == 0 ||
			    strcasecmp("ro", word) == 0) {
				flags = O_RDONLY;
			} else if (strcasecmp("wo", word) == 0) {
				flags = O_WRONLY;
			} else if (strcasecmp("rw", word) == 0) {
				flags = O_RDWR;
			} else {
				g_gate_xlog("Invalid value in flags field at "
				    "line %u.", lineno);
			}
			sflags = word;
			break;
		case 2:	/* path */
			if (strlen(word) >= MAXPATHLEN) {
				g_gate_xlog("Path too long at line %u. ",
				    lineno);
			}
			path = word;
			break;
		default:
			g_gate_xlog("Too many arguments at line %u. ", lineno);
		}
	}
	if (i != 3)
		g_gate_xlog("Too few arguments at line %u.", lineno);

	ex = malloc(sizeof(*ex));
	if (ex == NULL)
		g_gate_xlog("No enough memory.");
	ex->e_path = strdup(path);
	if (ex->e_path == NULL)
		g_gate_xlog("No enough memory.");

	/* Made 'and' here. */
	ex->e_ip = (ip & mask);
	ex->e_mask = mask;
	ex->e_flags = flags;

	SLIST_INSERT_HEAD(&exports_list, ex, e_next);

	g_gate_log(LOG_DEBUG, "Added %s/%u %s %s to exports list.",
	    ip2str(ex->e_ip), vmask, path, sflags);
}

static void
exports_clear(void)
{
	struct export *ex;

	while (!SLIST_EMPTY(&exports_list)) {
		ex = SLIST_FIRST(&exports_list);
		SLIST_REMOVE_HEAD(&exports_list, e_next);
		free(ex);
	}
}

#define	EXPORTS_LINE_SIZE	2048
static void
exports_get(void)
{
	char buf[EXPORTS_LINE_SIZE], *line;
	unsigned lineno = 0, objs = 0, len;
	FILE *fd;

	exports_clear();

	fd = fopen(exports, "r");
	if (fd == NULL) {
		g_gate_xlog("Cannot open exports file (%s): %s.", exports,
		    strerror(errno));
	}

	g_gate_log(LOG_INFO, "Reading exports file (%s).", exports);

	for (;;) {
		if (fgets(buf, sizeof(buf), fd) == NULL) {
			if (feof(fd))
				break;

			g_gate_xlog("Error while reading exports file: %s.",
			    strerror(errno));
		}

		/* Increase line count. */
		lineno++;

		/* Skip spaces and tabs. */
		for (line = buf; *line == ' ' || *line == '\t'; ++line)
			;

		/* Empty line, comment or empty line at the end of file. */
		if (*line == '\n' || *line == '#' || *line == '\0')
			continue;

		len = strlen(line);
		if (line[len - 1] == '\n') {
			/* Remove new line char. */
			line[len - 1] = '\0';
		} else {
			if (!feof(fd))
				g_gate_xlog("Line %u too long.", lineno);
		}

		line_parse(line, lineno);
		objs++;
	}

	fclose(fd);

	if (objs == 0)
		g_gate_xlog("There are no objects to export.");

	g_gate_log(LOG_INFO, "Exporting %u object(s).", objs);
}

static struct export *
exports_find(struct sockaddr *s, const char *path)
{
	struct export *ex;
	in_addr_t ip;

	ip = htonl(((struct sockaddr_in *)(void *)s)->sin_addr.s_addr);
	SLIST_FOREACH(ex, &exports_list, e_next) {
		if ((ip & ex->e_mask) != ex->e_ip)
			continue;
		if (path != NULL && strcmp(path, ex->e_path) != 0)
			continue;

		g_gate_log(LOG_INFO, "Connection from: %s.", ip2str(ip));
		return (ex);
	}
	g_gate_log(LOG_INFO, "Unauthorized connection from: %s.", ip2str(ip));

	return (NULL);
}

static void
sendfail(int sfd, int error, const char *fmt, ...)
{
	struct g_gate_sinit sinit;
	va_list ap;
	int data;

	sinit.gs_error = error;
	g_gate_swap2n_sinit(&sinit);
	data = send(sfd, &sinit, sizeof(sinit), 0);
	g_gate_swap2h_sinit(&sinit);
	if (data == -1) {
		g_gate_xlog("Error while sending initial packet: %s.",
		    strerror(errno));
	}
	if (fmt != NULL) {
		va_start(ap, fmt);
		g_gate_xvlog(fmt, ap);
		/* NOTREACHED */
		va_end(ap);
	}
	exit(EXIT_FAILURE);
}

static void
serve(int sfd, struct sockaddr *s)
{
	struct g_gate_cinit cinit;
	struct g_gate_sinit sinit;
	struct g_gate_hdr hdr;
	struct export *ex;
	char ipmask[32]; /* 32 == strlen("xxx.xxx.xxx.xxx/xxx.xxx.xxx.xxx")+1 */
	size_t bufsize;
	int32_t error;
	int fd, flags;
	ssize_t data;
	char *buf;

	g_gate_log(LOG_DEBUG, "Receiving initial packet.");
	data = recv(sfd, &cinit, sizeof(cinit), MSG_WAITALL);
	g_gate_swap2h_cinit(&cinit);
	if (data == -1) {
		g_gate_xlog("Error while receiving initial packet: %s.",
		    strerror(errno));
	}

	ex = exports_find(s, cinit.gc_path);
	if (ex == NULL) {
		sendfail(sfd, EINVAL, "Requested path isn't exported: %s.",
		    strerror(errno));
	}

	error = 0;
	strlcpy(ipmask, ip2str(ex->e_ip), sizeof(ipmask));
	strlcat(ipmask, "/", sizeof(ipmask));
	strlcat(ipmask, ip2str(ex->e_mask), sizeof(ipmask));
	if ((cinit.gc_flags & G_GATE_FLAG_READONLY) != 0) {
		if (ex->e_flags == O_WRONLY) {
			g_gate_log(LOG_ERR, "Read-only access requested, but "
			    "%s (%s) is exported write-only.", ex->e_path,
			    ipmask);
			error = EPERM;
		} else {
			sinit.gs_flags = G_GATE_FLAG_READONLY;
		}
	} else if ((cinit.gc_flags & G_GATE_FLAG_WRITEONLY) != 0) {
		if (ex->e_flags == O_RDONLY) {
			g_gate_log(LOG_ERR, "Write-only access requested, but "
			    "%s (%s) is exported read-only.", ex->e_path,
			    ipmask);
			error = EPERM;
		} else {
			sinit.gs_flags = G_GATE_FLAG_WRITEONLY;
		}
	} else {
		if (ex->e_flags == O_RDONLY) {
			g_gate_log(LOG_ERR, "Read-write access requested, but "
			    "%s (%s) is exported read-only.", ex->e_path,
			    ipmask);
			error = EPERM;
		} else if (ex->e_flags == O_WRONLY) {
			g_gate_log(LOG_ERR, "Read-write access requested, but "
			    "%s (%s) is exported write-only.", ex->e_path,
			    ipmask);
			error = EPERM;
		} else {
			sinit.gs_flags = 0;
		}
	}
	if (error != 0)
		sendfail(sfd, error, NULL);
	flags = g_gate_openflags(sinit.gs_flags);
	fd = open(ex->e_path, flags);
	if (fd < 0) {
		sendfail(sfd, errno, "Error while opening %s: %s.", ex->e_path,
		    strerror(errno));
	}

	g_gate_log(LOG_DEBUG, "Sending initial packet.");
	/*
	 * This field isn't used by ggc(8) for now.
	 * It should be used in future when user don't give device size.
	 */
	sinit.gs_mediasize = g_gate_mediasize(fd);
	sinit.gs_sectorsize = g_gate_sectorsize(fd);
	sinit.gs_error = 0;
	g_gate_swap2n_sinit(&sinit);
	data = send(sfd, &sinit, sizeof(sinit), 0);
	g_gate_swap2h_sinit(&sinit);
	if (data == -1) {
		sendfail(sfd, errno, "Error while sending initial packet: %s.",
		    strerror(errno));
	}

	bufsize = G_GATE_BUFSIZE_START;
	buf = malloc(bufsize);
	if (buf == NULL)
		g_gate_xlog("No enough memory.");

	g_gate_log(LOG_DEBUG, "New process: %u.", getpid());

	for (;;) {
		/*
		 * Receive request.
		 */
		data = recv(sfd, &hdr, sizeof(hdr), MSG_WAITALL);
		if (data == 0) {
			g_gate_log(LOG_DEBUG, "Process %u exiting.", getpid());
			exit(EXIT_SUCCESS);
		} else if (data == -1) {
			g_gate_xlog("Error while receiving hdr packet: %s.",
			    strerror(errno));
		} else if (data != sizeof(hdr)) {
			g_gate_xlog("Malformed hdr packet received.");
		}
		g_gate_log(LOG_DEBUG, "Received hdr packet.");
		g_gate_swap2h_hdr(&hdr);

		/*
		 * Increase buffer if there is need to.
		 */
		if (hdr.gh_length > bufsize) {
			bufsize = hdr.gh_length;
			g_gate_log(LOG_DEBUG, "Increasing buffer to %u.",
			    bufsize);
			buf = realloc(buf, bufsize);
			if (buf == NULL)
				g_gate_xlog("No enough memory.");
		}

		if (hdr.gh_cmd == BIO_READ) {
			if (pread(fd, buf, hdr.gh_length,
			    hdr.gh_offset) == -1) {
				error = errno;
				g_gate_log(LOG_ERR, "Error while reading data "
				    "(offset=%ju, size=%zu): %s.",
				    (uintmax_t)hdr.gh_offset,
				    (size_t)hdr.gh_length, strerror(error));
			} else {
				error = 0;
			}
			hdr.gh_error = error;
			g_gate_swap2n_hdr(&hdr);
			if (send(sfd, &hdr, sizeof(hdr), 0) == -1) {
				g_gate_xlog("Error while sending status: %s.",
				    strerror(errno));
			}
			g_gate_swap2h_hdr(&hdr);
			/* Send data only if there was no error while pread(). */
			if (error == 0) {
				data = send(sfd, buf, hdr.gh_length, 0);
				if (data == -1) {
					g_gate_xlog("Error while sending data: "
					    "%s.", strerror(errno));
				}
				g_gate_log(LOG_DEBUG, "Sent %d bytes "
				    "(offset=%ju, size=%zu).", data,
				    (uintmax_t)hdr.gh_offset,
				    (size_t)hdr.gh_length);
			}
		} else /* if (hdr.gh_cmd == BIO_WRITE) */ {
			g_gate_log(LOG_DEBUG, "Waiting for %u bytes of data...",
			    hdr.gh_length);
			data = recv(sfd, buf, hdr.gh_length, MSG_WAITALL);
			if (data == -1) {
				g_gate_xlog("Error while receiving data: %s.",
				    strerror(errno));
			}
			if (pwrite(fd, buf, hdr.gh_length, hdr.gh_offset) == -1) {
				error = errno;
				g_gate_log(LOG_ERR, "Error while writing data "
				    "(offset=%llu, size=%u): %s.",
				    hdr.gh_offset, hdr.gh_length,
				    strerror(error));
			} else {
				error = 0;
			}
			hdr.gh_error = error;
			g_gate_swap2n_hdr(&hdr);
			if (send(sfd, &hdr, sizeof(hdr), 0) == -1) {
				g_gate_xlog("Error while sending status: %s.",
				    strerror(errno));
			}
			g_gate_swap2h_hdr(&hdr);
			g_gate_log(LOG_DEBUG, "Received %d bytes (offset=%llu, "
			    "size=%u).", data, hdr.gh_offset, hdr.gh_length);
		}
		g_gate_log(LOG_DEBUG, "Tick.");
	}
}

static void
huphandler(int sig __unused)
{

	got_sighup = 1;
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in serv;
	struct sockaddr from;
	in_addr_t bindaddr;
	socklen_t fromlen;
	struct timeval tv;
	int on, sfd, tmpsfd;
	pid_t childpid;
	unsigned bsize, port;

	bindaddr = htonl(INADDR_ANY);
	port = G_GATE_PORT;
	for (;;) {
		int ch;

		ch = getopt(argc, argv, "a:hnp:R:S:v");
		if (ch == -1)
			break;
		switch (ch) {
		case 'a':
			bindaddr = g_gate_str2ip(optarg);
			if (bindaddr == INADDR_NONE) {
				errx(EXIT_FAILURE,
				    "Invalid IP/host name to bind to.");
			}
			break;
		case 'n':
			nagle = 0;
			break;
		case 'p':
			errno = 0;
			port = strtoul(optarg, NULL, 10);
			if (port == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid port.");
			break;
		case 'R':
			errno = 0;
			rcvbuf = strtoul(optarg, NULL, 10);
			if (rcvbuf == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid rcvbuf.");
			break;
		case 'S':
			errno = 0;
			sndbuf = strtoul(optarg, NULL, 10);
			if (sndbuf == 0 && errno != 0)
				errx(EXIT_FAILURE, "Invalid sndbuf.");
			break;
		case 'v':
			g_gate_verbose++;
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argv[0] != NULL)
		exports = argv[0];
	exports_get();

	if (!g_gate_verbose) {
		/* Run in daemon mode. */
		if (daemon(0, 0) == -1)
			g_gate_xlog("Can't daemonize: %s", strerror(errno));
	}

	signal(SIGCHLD, SIG_IGN);

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1)
		g_gate_xlog("Can't open stream socket: %s.", strerror(errno));
	bzero(&serv, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = bindaddr;
	serv.sin_port = htons(port);
	on = 1;
	if (nagle) {
		if (setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &on, 
		    sizeof(on)) == -1) {
			g_gate_xlog("setsockopt() error: %s.", strerror(errno));
		}
	}
	if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
		g_gate_xlog("setsockopt(): %s.", strerror(errno));
	bsize = rcvbuf;
	if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &bsize, sizeof(bsize)) == -1)
		g_gate_xlog("setsockopt(): %s.", strerror(errno));
	bsize = sndbuf;
	if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &bsize, sizeof(bsize)) == -1)
		g_gate_xlog("setsockopt(): %s.", strerror(errno));
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1 ||
	    setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
		g_gate_xlog("setsockopt() error: %s.", strerror(errno));
	}
	if (bind(sfd, (struct sockaddr *)&serv, sizeof(serv)) == -1)
		g_gate_xlog("bind(): %s.", strerror(errno));
	if (listen(sfd, 5) == -1)
		g_gate_xlog("listen(): %s.", strerror(errno));

	g_gate_log(LOG_INFO, "Listen on port: %d.", port);

	signal(SIGHUP, huphandler);

	for (;;) {
		fromlen = sizeof(from);
		tmpsfd = accept(sfd, &from, &fromlen);
		if (tmpsfd == -1)
			g_gate_xlog("accept(): %s.", strerror(errno));

		if (got_sighup) {
			got_sighup = 0;
			exports_get();
		}

		if (exports_find(&from, NULL) == NULL) {
			close(tmpsfd);
			continue;
		}

		childpid = fork();
		if (childpid < 0) {
			g_gate_xlog("Cannot create child process: %s.",
			    strerror(errno));
		} else if (childpid == 0) {
			close(sfd);
			serve(tmpsfd, &from);
			/* NOTREACHED */
		}
		close(tmpsfd);
	}
	close(sfd);
	exit(EXIT_SUCCESS);
}
