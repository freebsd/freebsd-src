/*
 * Copyright (c) 1999 Martin Blapp
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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>

#include <rpc/rpc.h>
#include <nfs/rpcv2.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mounttab.h"

int verbose;

static int do_umount (char *, char *);
static int do_umntall (char *);
static int is_mounted (char *, char *);
static void usage (void);
int	xdr_dir (XDR *, char *);

struct mtablist *mtabhead;

int
main(int argc, char **argv) {
	int ch, keep, success, pathlen;
	time_t expire, *now;
	char *host, *path;
	struct mtablist *mtab;

	mtab = NULL;
	now = NULL;
	expire = 0;
	host = path = '\0';
	success = keep = verbose = 0;
	while ((ch = getopt(argc, argv, "h:kp:ve:")) != -1)
		switch (ch) {
		case 'h':
			host = optarg;
			break;
		case 'e':
			expire = atoi(optarg);
			break;
		case 'k':
			keep = 1;
			break;
		case 'p':
			path = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
			usage();
		default:
		}
	argc -= optind;
	argv += optind;

	/* Ignore SIGINT and SIGQUIT during shutdown */
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	/* Default expiretime is one day */
	if (expire == 0)
		expire = 86400;
	/*
	 * Read PATH_MOUNTTAB and check each entry
	 * and do finally the unmounts.
	 */
	if (host == NULL && path == NULL) {
	    	if (!read_mtab(mtab)) {
			if (verbose)
				warnx("nothing to do, %s does not exist",
				    PATH_MOUNTTAB);
		}
		for (mtab = mtabhead; mtab != NULL; mtab = mtab->mtab_next) {
			if (*mtab->mtab_host != '\0' ||
			    mtab->mtab_time <= (time(now) - expire)) {
				if (keep && is_mounted(mtab->mtab_host,
				    mtab->mtab_dirp)) {
					if (verbose) {
						warnx("skip entry %s:%s",
						    mtab->mtab_host,
						    mtab->mtab_dirp);
					}
				} else if (do_umount(mtab->mtab_host,
				    mtab->mtab_dirp) || 
				    mtab->mtab_time <= (time(now) - expire)) {
					clean_mtab(mtab->mtab_host,
					    mtab->mtab_dirp);
				}
			}
		}
	/* Only do a RPC UMNTALL for this specific host */
	} else if (host != NULL && path == NULL) {
		if (!do_umntall(host))
			exit(1);
		else 
			success = 1;
	/* Someone forgot to enter a hostname */
	} else if (host == NULL && path != NULL)
		usage();
	/* Only do a RPC UMOUNT for this specific mount */
	else {
		for (pathlen = strlen(path);
		    pathlen > 1 && path[pathlen - 1] == '/'; pathlen--)
			path[pathlen - 1] = '\0';
		if (!do_umount(host, path))
			exit(1);
		else 
			success = 1;
	}
	/* Write and unlink PATH_MOUNTTAB if necessary */
	if (success) {
		if (verbose)
			warnx("UMOUNT RPC successfully sent to %s", host);
		if (read_mtab(mtab)) {
			mtab = mtabhead;
			clean_mtab(host, path);
		}
	}
	if (!write_mtab()) {
		free_mtab();
		exit(1);
	}
	free_mtab();
	exit(0);
}

/*
 * Send a RPC_MNT UMNTALL request to hostname.
 * XXX This works for all mountd implementations,
 * but produces a RPC IOERR on non FreeBSD systems.
 */
int
do_umntall(char *hostname) {
	enum clnt_stat clnt_stat;
	struct addrinfo *ai, hints;
	struct timeval try;
	int so, ecode;
	CLIENT *clp;

	memset(&hints, 0, sizeof hints);
        hints.ai_flags = AI_NUMERICHOST;
	ecode = getaddrinfo(hostname, NULL, &hints, &ai);
	if (ecode != 0) {
		warnx("can't get net id for host/nfs: %s",
		    gai_strerror(ecode));
		return (1);
	}
	clp = clnt_create(hostname, RPCPROG_MNT, RPCMNT_VER1, "udp");
	if (clp  == NULL) {
		clnt_pcreateerror("Cannot MNT PRC");
		return (1);
	}
	clp->cl_auth = authunix_create_default();
	try.tv_sec = 3;
	try.tv_usec = 0;
	clnt_stat = clnt_call(clp, RPCMNT_UMNTALL, xdr_void, (caddr_t)0,
	    xdr_void, (caddr_t)0, try);
	if (clnt_stat != RPC_SUCCESS) {
		clnt_perror(clp, "Bad MNT RPC");
		return (0);
	} else
		return (1);
}

/*
 * Send a RPC_MNT UMOUNT request for dirp to hostname.
 */
int
do_umount(char *hostname, char *dirp) {
	enum clnt_stat clnt_stat;
	struct addrinfo *ai, hints;
	struct timeval try;
	CLIENT *clp;
	int so, ecode;

	memset(&hints, 0, sizeof hints);
        hints.ai_flags = AI_NUMERICHOST;
	ecode = getaddrinfo(hostname, NULL, &hints, &ai);
	if (ecode != 0) {
		warnx("can't get net id for host/nfs: %s",
		    gai_strerror(ecode));
		return (1);
	}
	clp = clnt_create(hostname, RPCPROG_MNT, RPCMNT_VER1, "udp");
	if (clp  == NULL) {
		clnt_pcreateerror("Cannot MNT PRC");
		return (1);
	}
	clp->cl_auth = authsys_create_default();
	try.tv_sec = 3;
	try.tv_usec = 0;
	clnt_stat = clnt_call(clp, RPCMNT_UMOUNT, xdr_dir, dirp,
	    xdr_void, (caddr_t)0, try);
	if (clnt_stat != RPC_SUCCESS) {
		clnt_perror(clp, "Bad MNT RPC");
		return (0);
	}
	return (1);
}

/*
 * Check if the entry is still/already mounted.
 */
int
is_mounted(char *hostname, char *dirp) {
	struct statfs *mntbuf;
	char name[MNAMELEN + 1];
	size_t bufsize, hostlen, dirlen;
	int mntsize, i;

	hostlen = strlen(hostname);
	dirlen = strlen(dirp);
	if ((hostlen + dirlen) >= MNAMELEN)
		return (0);
	memmove(name, hostname, hostlen);
	name[hostlen] = ':';
	memmove(name + hostlen + 1, dirp, dirlen);
	name[hostlen + dirlen + 1] = '\0';
	mntsize = getfsstat(NULL, 0, MNT_NOWAIT);
	if (mntsize <= 0)
		return (0);
	bufsize = (mntsize + 1) * sizeof(struct statfs);
	if ((mntbuf = malloc(bufsize)) == NULL)
		err(1, "malloc");
	mntsize = getfsstat(mntbuf, (long)bufsize, MNT_NOWAIT);
	for (i = mntsize - 1; i >= 0; i--) {
		if (strcmp(mntbuf[i].f_mntfromname, name) == 0) {
			free(mntbuf);
			return (1);
		}
	}
	free(mntbuf);
	return (0);
}

/*
 * xdr routines for mount rpc's
 */
int
xdr_dir(XDR *xdrsp, char *dirp) {

	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

static void
usage() {

	(void)fprintf(stderr, "%s\n",
	    "usage: rpc.umntall [-h host] [-k] [-p path] [-t expire] [-v]");
	exit(1);
}
