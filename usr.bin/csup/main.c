/*-
 * Copyright (c) 2003-2006, Maxime Henrion <mux@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "fattr.h"
#include "misc.h"
#include "proto.h"
#include "stream.h"

#define	USAGE_OPTFMT	"    %-12s %s\n"
#define	USAGE_OPTFMTSUB	"    %-14s %s\n", ""

int verbose = 1;

static void
usage(char *argv0)
{

	lprintf(-1, "Usage: %s [options] supfile\n", basename(argv0));
	lprintf(-1, "  Options:\n");
	lprintf(-1, USAGE_OPTFMT, "-1", "Don't retry automatically on failure "
	    "(same as \"-r 0\")");
	lprintf(-1, USAGE_OPTFMT, "-4", "Force usage of IPv4 addresses");
	lprintf(-1, USAGE_OPTFMT, "-6", "Force usage of IPv6 addresses");
	lprintf(-1, USAGE_OPTFMT, "-a",
		"Require server to authenticate itself to us");
	lprintf(-1, USAGE_OPTFMT, "-A addr",
	    "Bind local socket to a specific address");
	lprintf(-1, USAGE_OPTFMT, "-b base",
	    "Override supfile's \"base\" directory");
	lprintf(-1, USAGE_OPTFMT, "-c collDir",
	    "Subdirectory of \"base\" for collections (default \"sup\")");
	lprintf(-1, USAGE_OPTFMT, "-d delLimit",
	    "Allow at most \"delLimit\" file deletions (default unlimited)");
	lprintf(-1, USAGE_OPTFMT, "-h host",
	    "Override supfile's \"host\" name");
	lprintf(-1, USAGE_OPTFMT, "-i pattern",
	    "Include only files/directories matching pattern.");
	lprintf(-1, USAGE_OPTFMTSUB,
	    "May be repeated for an OR operation.  Default is");
	lprintf(-1, USAGE_OPTFMTSUB, "to include each entire collection.");
	lprintf(-1, USAGE_OPTFMT, "-k",
	    "Keep bad temporary files when fixups are required");
	lprintf(-1, USAGE_OPTFMT, "-l lockfile",
	    "Lock file during update; fail if already locked");
	lprintf(-1, USAGE_OPTFMT, "-L n",
	    "Verbosity level (0..2, default 1)");
	lprintf(-1, USAGE_OPTFMT, "-p port",
	    "Alternate server port (default 5999)");
	lprintf(-1, USAGE_OPTFMT, "-r n",
	    "Maximum retries on transient errors (default unlimited)");
	lprintf(-1, USAGE_OPTFMT, "-s",
	    "Don't stat client files; trust the checkouts file");
	lprintf(-1, USAGE_OPTFMT, "-v", "Print version and exit");
	lprintf(-1, USAGE_OPTFMT, "-z", "Enable compression for all "
	    "collections");
	lprintf(-1, USAGE_OPTFMT, "-Z", "Disable compression for all "
	    "collections");
}

int
main(int argc, char *argv[])
{
	struct tm tm;
	struct backoff_timer *timer;
	struct config *config;
	struct coll *override;
	struct addrinfo *res;
	struct sockaddr *laddr;
	socklen_t laddrlen;
	struct stream *lock;
	char *argv0, *file, *lockfile;
	int family, error, lockfd, lflag, overridemask;
	int c, i, deletelim, port, retries, status, reqauth;
	time_t nexttry;

	error = 0;
	family = PF_UNSPEC;
	deletelim = -1;
	port = 0;
	lflag = 0;
	lockfd = 0;
	nexttry = 0;
	retries = -1;
	argv0 = argv[0];
	laddr = NULL;
	laddrlen = 0;
	lockfile = NULL;
	override = coll_new(NULL);
	overridemask = 0;
	reqauth = 0;

	while ((c = getopt(argc, argv,
	    "146aA:b:c:d:gh:i:kl:L:p:P:r:svzZ")) != -1) {
		switch (c) {
		case '1':
			retries = 0;
			break;
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
		case 'a':
			/* Require server authentication */
			reqauth = 1;
			break;
		case 'A':
			error = getaddrinfo(optarg, NULL, NULL, &res);
			if (error) {
				lprintf(-1, "%s: %s\n", optarg,
				    gai_strerror(error));
				return (1);
			}
			laddrlen = res->ai_addrlen;
			laddr = xmalloc(laddrlen);
			memcpy(laddr, res->ai_addr, laddrlen);
			freeaddrinfo(res);
			break;
		case 'b':
			if (override->co_base != NULL)
				free(override->co_base);
			override->co_base = xstrdup(optarg);
			break;
		case 'c':
			override->co_colldir = optarg;
			break;
		case 'd':
			error = asciitoint(optarg, &deletelim, 0);
			if (error || deletelim < 0) {
				lprintf(-1, "Invalid deletion limit\n");
				usage(argv0);
				return (1);
			}
			break;
		case 'g':
			/* For compatibility. */
			break;
		case 'h':
			if (override->co_host != NULL)
				free(override->co_host);
			override->co_host = xstrdup(optarg);
			break;
		case 'i':
			pattlist_add(override->co_accepts, optarg);
			break;
		case 'k':
			override->co_options |= CO_KEEPBADFILES;
			overridemask |= CO_KEEPBADFILES;
			break;
		case 'l':
			lockfile = optarg;
			lflag = 1;
			lockfd = open(lockfile,
			    O_CREAT | O_WRONLY | O_TRUNC, 0700);
			if (lockfd != -1) {
				error = flock(lockfd, LOCK_EX | LOCK_NB);
				if (error == -1 && errno == EWOULDBLOCK) {
					if (lockfd != -1)
						close(lockfd);
					lprintf(-1, "\"%s\" is already locked "
					    "by another process\n", lockfile);
					return (1);
				}
			}
			if (lockfd == -1 || error == -1) {
				if (lockfd != -1)
					close(lockfd);
				lprintf(-1, "Error locking \"%s\": %s\n",
				    lockfile, strerror(errno));
				return (1);
			}
			lock = stream_open_fd(lockfd,
			    NULL, stream_write_fd, NULL);
			(void)stream_printf(lock, "%10ld\n", (long)getpid());
			stream_close(lock);
			break;
		case 'L':
			error = asciitoint(optarg, &verbose, 0);
			if (error) {
				lprintf(-1, "Invalid verbosity\n");
				usage(argv0);
				return (1);
			}
			break;
		case 'p':
			/* Use specified server port. */
			error = asciitoint(optarg, &port, 0);
			if (error) {
				lprintf(-1, "Invalid server port\n");
				usage(argv0);
				return (1);
			}
			if (port <= 0 || port >= 65536) {
				lprintf(-1, "Invalid port %d\n", port);
				return (1);
			}
			if (port < 1024) {
				lprintf(-1, "Reserved port %d not permitted\n",
				    port);
				return (1);
			}
			break;
		case 'P':
			/* For compatibility. */
			if (strcmp(optarg, "m") != 0) {
				lprintf(-1,
				    "Client only supports multiplexed mode\n");
				return (1);
			}
			break;
		case 'r':
			error = asciitoint(optarg, &retries, 0);
			if (error || retries < 0) {
				lprintf(-1, "Invalid retry limit\n");
				usage(argv0);
				return (1);
			}
			break;
		case 's':
			override->co_options |= CO_TRUSTSTATUSFILE;
			overridemask |= CO_TRUSTSTATUSFILE;
			break;
		case 'v':
			lprintf(0, "CVSup client written in C\n");
			lprintf(0, "Software version: %s\n", PROTO_SWVER);
			lprintf(0, "Protocol version: %d.%d\n",
			    PROTO_MAJ, PROTO_MIN);
			return (0);
			break;
		case 'z':
			/* Force compression on all collections. */
			override->co_options |= CO_COMPRESS;
			overridemask |= CO_COMPRESS;
			break;
		case 'Z':
			/* Disables compression on all collections. */
			override->co_options &= ~CO_COMPRESS;
			overridemask &= ~CO_COMPRESS;
			break;
		case '?':
		default:
			usage(argv0);
			return (1);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage(argv0);
		return (1);
	}

	file = argv[0];
	lprintf(2, "Parsing supfile \"%s\"\n", file);
	config = config_init(file, override, overridemask);
	coll_free(override);
	if (config == NULL)
		return (1);

	if (config_checkcolls(config) == 0) {
		lprintf(-1, "No collections selected\n");
		return (1);
	}

	if (laddr != NULL) {
		config->laddr = laddr;
		config->laddrlen = laddrlen;
	}
	config->deletelim = deletelim;
	config->reqauth = reqauth;
	lprintf(2, "Connecting to %s\n", config->host);

	i = 0;
	fattr_init();	/* Initialize the fattr API. */
	timer = bt_new(300, 7200, 2.0, 0.1);
	for (;;) {
		status = proto_connect(config, family, port);
		if (status == STATUS_SUCCESS) {
			status = proto_run(config);
			if (status != STATUS_TRANSIENTFAILURE)
				break;
		}
		if (retries >= 0 && i >= retries)
			break;
		nexttry = time(0) + bt_get(timer);
		localtime_r(&nexttry, &tm);
		lprintf(1, "Will retry at %02d:%02d:%02d\n",
		    tm.tm_hour, tm.tm_min, tm.tm_sec);
		bt_pause(timer);
		lprintf(1, "Retrying\n");
		i++;
	}
	bt_free(timer);
	fattr_fini();
	if (lflag) {
		unlink(lockfile);
		flock(lockfd, LOCK_UN);
		close(lockfd);
	}
	config_free(config);
	if (status != STATUS_SUCCESS)
		return (1);
	return (0);
}
