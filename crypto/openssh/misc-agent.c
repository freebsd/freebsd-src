/* $OpenBSD: misc-agent.c,v 1.7 2026/02/11 17:05:32 dtucker Exp $ */
/*
 * Copyright (c) 2025 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "digest.h"
#include "log.h"
#include "misc.h"
#include "pathnames.h"
#include "ssh.h"
#include "xmalloc.h"

/* stuff shared by agent listeners (ssh-agent and sshd agent forwarding) */

#define SOCKET_HOSTNAME_HASHLEN 10 /* length of hostname hash in socket path */

/* used for presenting random strings in unix_listener_tmp and hostname_hash */
static const char presentation_chars[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

/* returns a text-encoded hash of the hostname of specified length (max 64) */
static char *
hostname_hash(size_t len)
{
	char hostname[NI_MAXHOST], p[65];
	u_char hash[64];
	int r;
	size_t l, i;

	l = ssh_digest_bytes(SSH_DIGEST_SHA512);
	if (len > 64) {
		error_f("bad length %zu >= max %zd", len, l);
		return NULL;
	}
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		error_f("gethostname: %s", strerror(errno));
		return NULL;
	}
	if ((r = ssh_digest_memory(SSH_DIGEST_SHA512,
	    hostname, strlen(hostname), hash, sizeof(hash))) != 0) {
		error_fr(r, "ssh_digest_memory");
		return NULL;
	}
	memset(p, '\0', sizeof(p));
	for (i = 0; i < l; i++)
		p[i] = presentation_chars[
		    hash[i] % (sizeof(presentation_chars) - 1)];
	/* debug3_f("hostname \"%s\" => hash \"%s\"", hostname, p); */
	p[len] = '\0';
	return xstrdup(p);
}

char *
agent_hostname_hash(void)
{
	return hostname_hash(SOCKET_HOSTNAME_HASHLEN);
}

/*
 * Creates a unix listener at a mkstemp(3)-style path, e.g. "/dir/sock.XXXXXX"
 * Supplied path is modified to the actual one used.
 */
static int
unix_listener_tmp(char *path, int backlog)
{
	struct sockaddr_un sunaddr;
	int good, sock = -1;
	size_t i, xstart;
	mode_t prev_mask;

	/* Find first 'X' template character back from end of string */
	xstart = strlen(path);
	while (xstart > 0 && path[xstart - 1] == 'X')
		xstart--;

	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	prev_mask = umask(0177);
	for (good = 0; !good;) {
		sock = -1;
		/* Randomise path suffix */
		for (i = xstart; path[i] != '\0'; i++) {
			path[i] = presentation_chars[
			    arc4random_uniform(sizeof(presentation_chars)-1)];
		}
		debug_f("trying path \"%s\"", path);

		if (strlcpy(sunaddr.sun_path, path,
		    sizeof(sunaddr.sun_path)) >= sizeof(sunaddr.sun_path)) {
			error_f("path \"%s\" too long for Unix domain socket",
			    path);
			break;
		}

		if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) == -1) {
			error_f("socket: %.100s", strerror(errno));
			break;
		}
		if (bind(sock, (struct sockaddr *)&sunaddr,
		    sizeof(sunaddr)) == -1) {
			if (errno == EADDRINUSE) {
				error_f("bind \"%s\": %.100s",
				    path, strerror(errno));
				close(sock);
				sock = -1;
				continue;
			}
			error_f("bind \"%s\": %.100s", path, strerror(errno));
			break;
		}
		if (listen(sock, backlog) == -1) {
			error_f("listen \"%s\": %s", path, strerror(errno));
			break;
		}
		good = 1;
	}
	umask(prev_mask);
	if (good) {
		debug3_f("listening on unix socket \"%s\" as fd=%d",
		    path, sock);
	} else if (sock != -1) {
		close(sock);
		sock = -1;
	}
	return sock;
}

/*
 * Create a subdirectory under the supplied home directory if it
 * doesn't already exist
 */
static int
ensure_mkdir(const char *homedir, const char *subdir)
{
	char *path;

	xasprintf(&path, "%s/%s", homedir, subdir);
	if (mkdir(path, 0700) == 0)
		debug("created directory %s", path);
	else if (errno != EEXIST) {
		error_f("mkdir %s: %s", path, strerror(errno));
		free(path);
		return -1;
	}
	free(path);
	return 0;
}

static int
agent_prepare_sockdir(const char *homedir)
{
	if (homedir == NULL || *homedir == '\0' ||
	    ensure_mkdir(homedir, _PATH_SSH_USER_DIR) != 0 ||
	    ensure_mkdir(homedir, _PATH_SSH_AGENT_SOCKET_DIR) != 0)
		return -1;
	return 0;
}


/* Get a path template for an agent socket in the user's homedir */
static char *
agent_socket_template(const char *homedir, const char *tag)
{
	char *hostnamehash, *ret;

	if ((hostnamehash = hostname_hash(SOCKET_HOSTNAME_HASHLEN)) == NULL)
		return NULL;
	xasprintf(&ret, "%s/%s/s.%s.%s.XXXXXXXXXX",
	    homedir, _PATH_SSH_AGENT_SOCKET_DIR, hostnamehash, tag);
	free(hostnamehash);
	return ret;
}

int
agent_listener(const char *homedir, const char *tag, int *sockp, char **pathp)
{
	int sock;
	char *path;

	*sockp = -1;
	*pathp = NULL;

	if (agent_prepare_sockdir(homedir) != 0)
		return -1; /* error already logged */
	if ((path = agent_socket_template(homedir, tag)) == NULL)
		return -1; /* error already logged */
	if ((sock = unix_listener_tmp(path, SSH_LISTEN_BACKLOG)) == -1) {
		free(path);
		return -1; /* error already logged */
	}
	/* success */
	*sockp = sock;
	*pathp = path;
	return 0;
}

static int
socket_is_stale(const char *path)
{
	int fd, r;
	struct sockaddr_un sunaddr;
	socklen_t l = sizeof(r);

	/* attempt non-blocking connect on socket */
	memset(&sunaddr, '\0', sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	if (strlcpy(sunaddr.sun_path, path,
	    sizeof(sunaddr.sun_path)) >= sizeof(sunaddr.sun_path)) {
		debug_f("path for \"%s\" too long for sockaddr_un", path);
		return 0;
	}
	if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1) {
		error_f("socket: %s", strerror(errno));
		return 0;
	}
	set_nonblock(fd);
	/* a socket without a listener should yield an error immediately */
	if (connect(fd, (struct sockaddr *)&sunaddr, sizeof(sunaddr)) == -1) {
		debug_f("connect \"%s\": %s", path, strerror(errno));
		close(fd);
		return 1;
	}
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &r, &l) == -1) {
		debug_f("getsockopt: %s", strerror(errno));
		close(fd);
		return 0;
	}
	if (r != 0) {
		debug_f("socket error on %s: %s", path, strerror(errno));
		close(fd);
		return 1;
	}
	close(fd);
	debug_f("socket %s seems still active", path);
	return 0;
}

#ifndef HAVE_FSTATAT
# define fstatat(x, y, buf, z) lstat(path, buf)
#endif
#ifndef HAVE_UNLINKAT
# define unlinkat(x, y, z) unlink(path)
#endif

void
agent_cleanup_stale(const char *homedir, int ignore_hosthash)
{
	DIR *d = NULL;
	struct dirent *dp;
	struct stat sb;
	char *prefix = NULL, *dirpath = NULL, *path = NULL;
	struct timespec now, sub, *mtimp = NULL;

	/* Only consider sockets last modified > 1 hour ago */
	if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
		error_f("clock_gettime: %s", strerror(errno));
		return;
	}
	sub.tv_sec = 60 * 60;
	sub.tv_nsec = 0;
	timespecsub(&now, &sub, &now);

	/* Only consider sockets from the same hostname */
	if (!ignore_hosthash) {
		if ((path = agent_hostname_hash()) == NULL) {
			error_f("couldn't get hostname hash");
			return;
		}
		xasprintf(&prefix, "s.%s.", path);
		free(path);
		path = NULL;
	}

	xasprintf(&dirpath, "%s/%s", homedir, _PATH_SSH_AGENT_SOCKET_DIR);
	if ((d = opendir(dirpath)) == NULL) {
		if (errno != ENOENT)
			error_f("opendir \"%s\": %s", dirpath, strerror(errno));
		goto out;
	}

	path = NULL;
	while ((dp = readdir(d)) != NULL) {
		free(path);
		xasprintf(&path, "%s/%s", dirpath, dp->d_name);
#ifdef HAVE_DIRENT_D_TYPE
		if (dp->d_type != DT_SOCK && dp->d_type != DT_UNKNOWN)
			continue;
#endif
		if (fstatat(dirfd(d), dp->d_name,
		    &sb, AT_SYMLINK_NOFOLLOW) != 0 && errno != ENOENT) {
			error_f("stat \"%s/%s\": %s",
			    dirpath, dp->d_name, strerror(errno));
			continue;
		}
		if (!S_ISSOCK(sb.st_mode))
			continue;
#ifdef HAVE_STRUCT_STAT_ST_MTIM
		mtimp = &sb.st_mtim;
#else
		sub.tv_sec = sb.st_mtime;
		sub.tv_nsec = 0;
		mtimp = &sub;
#endif
		if (timespeccmp(mtimp, &now, >)) {
			debug3_f("Ignoring recent socket \"%s/%s\"",
			    dirpath, dp->d_name);
			continue;
		}
		if (!ignore_hosthash &&
		    strncmp(dp->d_name, prefix, strlen(prefix)) != 0) {
			debug3_f("Ignoring socket \"%s/%s\" "
			    "from different host", dirpath, dp->d_name);
			continue;
		}
		if (socket_is_stale(path)) {
			debug_f("cleanup stale socket %s", path);
			unlinkat(dirfd(d), dp->d_name, 0);
		}
	}
 out:
	if (d != NULL)
		closedir(d);
	free(path);
	free(dirpath);
	free(prefix);
}

#undef unlinkat
#undef fstatat
