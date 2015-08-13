/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/capsicum.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/nv.h>

#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <libcapsicum.h>
#include <libcapsicum_impl.h>
#include <libcasper.h>
#include <libcasper_impl.h>
#include <msgio.h>
#include <pjdlog.h>

#include "msgio.h"

#include "zygote.h"

#define	CASPERD_PIDFILE		"/var/run/casperd.pid"
#define	CASPERD_SERVCONFDIR	"/etc/casper"
#define	CASPERD_SOCKPATH	"/var/run/casper"

typedef void service_function_t(struct service_connection *, const nvlist_t *,
    nvlist_t *);

struct casper_service {
	const char	*cs_name;
	const char	*cs_execpath;
	struct service	*cs_service;
	nvlist_t	*cs_attrs;
	TAILQ_ENTRY(casper_service) cs_next;
};

static TAILQ_HEAD(, casper_service) casper_services =
    TAILQ_HEAD_INITIALIZER(casper_services);

#define	SERVICE_IS_CORE(service)	((service)->cs_execpath == NULL)

static void service_external_execute(int chanfd);

#define KEEP_ERRNO(work)	do {					\
	int _serrno;							\
									\
	_serrno = errno;						\
	work;								\
	errno = _serrno;						\
} while (0)

static struct casper_service *
service_find(const char *name)
{
	struct casper_service *casserv;

	TAILQ_FOREACH(casserv, &casper_services, cs_next) {
		if (strcmp(casserv->cs_name, name) == 0)
			break;
	}
	return (casserv);
}

/*
 * Function always consumes the given attrs.
 */
static void
service_register(nvlist_t *attrs)
{
	struct casper_service *casserv;
	const char *name;

	PJDLOG_ASSERT(nvlist_exists_string(attrs, "name"));
	PJDLOG_ASSERT(nvlist_exists_string(attrs, "execpath") ||
	    (nvlist_exists_number(attrs, "commandfunc") &&
	     nvlist_exists_number(attrs, "limitfunc")));

	name = nvlist_get_string(attrs, "name");
	PJDLOG_ASSERT(name != NULL);
	if (name[0] == '\0') {
		pjdlog_error("Unable to register service with an empty name.");
		nvlist_destroy(attrs);
		return;
	}
	if (service_find(name) != NULL) {
		pjdlog_error("Service \"%s\" is already registered.", name);
		nvlist_destroy(attrs);
		return;
	}

	casserv = malloc(sizeof(*casserv));
	if (casserv == NULL) {
		pjdlog_errno(LOG_ERR, "Unable to register service \"%s\"",
		    name);
		nvlist_destroy(attrs);
		return;
	}
	casserv->cs_name = name;
	if (nvlist_exists_string(attrs, "execpath")) {
		struct stat sb;

		PJDLOG_ASSERT(!nvlist_exists_number(attrs, "commandfunc"));
		PJDLOG_ASSERT(!nvlist_exists_number(attrs, "limitfunc"));

		casserv->cs_service = NULL;

		casserv->cs_execpath = nvlist_get_string(attrs, "execpath");
		if (casserv->cs_execpath == NULL ||
		    casserv->cs_execpath[0] == '\0') {
			pjdlog_error("Unable to register service with an empty execpath.");
			free(casserv);
			nvlist_destroy(attrs);
			return;
		}
		if (stat(casserv->cs_execpath, &sb) == -1) {
			pjdlog_errno(LOG_ERR,
			    "Unable to register service \"%s\", problem with executable \"%s\"",
			    name, casserv->cs_execpath);
			free(casserv);
			nvlist_destroy(attrs);
			return;
		}
	} else /* if (nvlist_exists_number(attrs, "commandfunc")) */ {
		PJDLOG_ASSERT(!nvlist_exists_string(attrs, "execpath"));

		casserv->cs_execpath = NULL;

		casserv->cs_service = service_alloc(name,
		    (void *)(uintptr_t)nvlist_get_number(attrs, "limitfunc"),
		    (void *)(uintptr_t)nvlist_get_number(attrs, "commandfunc"));
		if (casserv->cs_service == NULL) {
			pjdlog_errno(LOG_ERR,
			    "Unable to register service \"%s\"", name);
			free(casserv);
			nvlist_destroy(attrs);
			return;
		}
	}
	casserv->cs_attrs = attrs;
	TAILQ_INSERT_TAIL(&casper_services, casserv, cs_next);
	pjdlog_debug(1, "Service %s successfully registered.",
	    casserv->cs_name);
}

static bool
casper_allowed_service(const nvlist_t *limits, const char *service)
{

	if (limits == NULL)
		return (true);

	if (nvlist_exists_null(limits, service))
		return (true);

	return (false);
}

static int
casper_limit(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const char *name;
	int type;
	void *cookie;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &type, &cookie)) != NULL) {
		if (type != NV_TYPE_NULL)
			return (EINVAL);
		if (!casper_allowed_service(oldlimits, name))
			return (ENOTCAPABLE);
	}

	return (0);
}

static int
casper_command(const char *cmd, const nvlist_t *limits, nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	struct casper_service *casserv;
	const char *servname;
	nvlist_t *nvl;
	int chanfd, execfd, procfd, error;

	if (strcmp(cmd, "open") != 0)
		return (EINVAL);
	if (!nvlist_exists_string(nvlin, "service"))
		return (EINVAL);

	servname = nvlist_get_string(nvlin, "service");

	casserv = service_find(servname);
	if (casserv == NULL)
		return (ENOENT);

	if (!casper_allowed_service(limits, servname))
		return (ENOTCAPABLE);

#ifdef O_EXEC_WORKING
	execfd = open(casserv->cs_execpath, O_EXEC);
#else
	execfd = open(casserv->cs_execpath, O_RDONLY);
#endif
	if (execfd < -1) {
		error = errno;
		pjdlog_errno(LOG_ERR,
		    "Unable to open executable '%s' of service '%s'",
		    casserv->cs_execpath, casserv->cs_name);
		return (error);
	}

	if (zygote_clone(service_external_execute, &chanfd, &procfd) == -1) {
		error = errno;
		close(execfd);
		return (error);
	}

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "service", casserv->cs_name);
	if (nvlist_exists_descriptor(nvlin, "stderrfd")) {
		nvlist_move_descriptor(nvl, "stderrfd",
		    nvlist_take_descriptor(nvlin, "stderrfd"));
	}
	nvlist_move_descriptor(nvl, "execfd", execfd);
	nvlist_move_descriptor(nvl, "procfd", procfd);
	if (nvlist_send(chanfd, nvl) == -1) {
		error = errno;
		pjdlog_errno(LOG_ERR, "Unable to send nvlist");
		nvlist_destroy(nvl);
		close(chanfd);
		return (error);
	}
	nvlist_destroy(nvl);

	nvlist_move_descriptor(nvlout, "chanfd", chanfd);

	return (0);
}

static void
fdswap(int *fd0, int *fd1)
{
	int tmpfd;

	PJDLOG_VERIFY((tmpfd = dup(*fd0)) != -1);
	PJDLOG_VERIFY(dup2(*fd1, *fd0) != -1);
	PJDLOG_VERIFY(dup2(tmpfd, *fd1) != -1);
	close(tmpfd);
	tmpfd = *fd0;
	*fd0 = *fd1;
	*fd1 = tmpfd;
}

static void
fdmove(int *oldfdp, int newfd)
{

	if (*oldfdp != newfd) {
		PJDLOG_VERIFY(dup2(*oldfdp, newfd) != -1);
		close(*oldfdp);
		*oldfdp = newfd;
	}
}

static void
fdcloexec(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFD);
	PJDLOG_ASSERT(flags != -1);
	if ((flags & FD_CLOEXEC) != 0)
		PJDLOG_VERIFY(fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) != -1);
}

static void
service_register_core(void)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "name", "core.casper");
	nvlist_add_number(nvl, "limitfunc", (uint64_t)(uintptr_t)casper_limit);
	nvlist_add_number(nvl, "commandfunc",
	    (uint64_t)(uintptr_t)casper_command);
	service_register(nvl);
}

static int
setup_creds(int sock)
{
	struct cmsgcred cred;

	if (cred_recv(sock, &cred) == -1)
		return (-1);

	if (setgroups((int)cred.cmcred_ngroups, cred.cmcred_groups) == -1)
		return (-1);

	if (setgid(cred.cmcred_groups[0]) == -1)
		return (-1);

	if (setuid(cred.cmcred_euid) == -1)
		return (-1);

	return (0);
}

static void
service_external_execute(int chanfd)
{
	char *service, *argv[3];
	int stderrfd, execfd, procfd;
	nvlist_t *nvl;

	nvl = nvlist_recv(chanfd, 0);
	if (nvl == NULL)
		pjdlog_exit(1, "Unable to receive nvlist");
	service = nvlist_take_string(nvl, "service");
	PJDLOG_ASSERT(service != NULL);
	if (nvlist_exists_descriptor(nvl, "stderrfd")) {
		stderrfd = nvlist_take_descriptor(nvl, "stderrfd");
	} else {
		stderrfd = open(_PATH_DEVNULL, O_RDWR);
		if (stderrfd < 0)
			pjdlog_exit(1, "Unable to open %s", _PATH_DEVNULL);
	}
	execfd = nvlist_take_descriptor(nvl, "execfd");
	procfd = nvlist_take_descriptor(nvl, "procfd");
	nvlist_destroy(nvl);

	/*
	 * Move all descriptors into right slots.
	 */

	if (stderrfd != STDERR_FILENO) {
		if (chanfd == STDERR_FILENO)
			fdswap(&stderrfd, &chanfd);
		else if (execfd == STDERR_FILENO)
			fdswap(&stderrfd, &execfd);
		else if (procfd == STDERR_FILENO)
			fdswap(&stderrfd, &procfd);
		fdmove(&stderrfd, STDERR_FILENO);
	}
	fdcloexec(stderrfd);

	if (chanfd != PARENT_FILENO) {
		if (execfd == PARENT_FILENO)
			fdswap(&chanfd, &execfd);
		else if (procfd == PARENT_FILENO)
			fdswap(&chanfd, &procfd);
		fdmove(&chanfd, PARENT_FILENO);
	}
	fdcloexec(chanfd);

	if (execfd != EXECUTABLE_FILENO) {
		if (procfd == EXECUTABLE_FILENO)
			fdswap(&execfd, &procfd);
		fdmove(&execfd, EXECUTABLE_FILENO);
	}
	fdcloexec(execfd);

	if (procfd != PROC_FILENO)
		fdmove(&procfd, PROC_FILENO);
	fdcloexec(procfd);

	/*
	 * Use credentials of the caller process.
	 */
	setup_creds(chanfd);

	argv[0] = service;
	asprintf(&argv[1], "%d", pjdlog_debug_get());
	argv[2] = NULL;

	fexecve(execfd, argv, NULL);
	pjdlog_exit(1, "Unable to execute service %s", service);
}

static void
service_register_external_one(const char *dirpath, int dfd,
    const char *filename)
{
	char execpath[FILENAME_MAX];
	nvlist_t *nvl;
	ssize_t done;
	int fd;

	fd = openat(dfd, filename, O_RDONLY);
	if (fd == -1) {
		pjdlog_errno(LOG_ERR, "Unable to open \"%s/%s\"", dirpath,
		    filename);
		return;
	}

	done = read(fd, execpath, sizeof(execpath));
	if (done == -1) {
		pjdlog_errno(LOG_ERR, "Unable to read content of \"%s/%s\"",
		    dirpath, filename);
		close(fd);
		return;
	}
	close(fd);
	if (done == sizeof(execpath)) {
		pjdlog_error("Executable path too long in \"%s/%s\".", dirpath,
		    filename);
		return;
	}
	execpath[done] = '\0';
	while (done > 0) {
		if (execpath[--done] == '\n')
			execpath[done] = '\0';
	}

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "name", filename);
	nvlist_add_string(nvl, "execpath", execpath);
	if (nvlist_error(nvl) != 0) {
		pjdlog_common(LOG_ERR, 0, nvlist_error(nvl),
		    "Unable to allocate attributes for service \"%s/%s\"",
		    dirpath, filename);
		nvlist_destroy(nvl);
		return;
	}

	service_register(nvl);
	/* service_register() consumed nvl. */
}

static uint8_t
file_type(int dfd, const char *filename)
{
	struct stat sb;

	if (fstatat(dfd, filename, &sb, AT_SYMLINK_NOFOLLOW) == -1) {
		pjdlog_errno(LOG_ERR, "Unable to stat \"%s\"", filename);
		return (DT_UNKNOWN);
	}
	return (IFTODT(sb.st_mode));
}

static void
service_register_external(const char *dirpath)
{
	DIR *dirp;
	struct dirent *dp;
	int dfd;

	dirp = opendir(dirpath);
	if (dirp == NULL) {
		pjdlog_errno(LOG_WARNING, "Unable to open \"%s\"", dirpath);
		return;
	}
	dfd = dirfd(dirp);
	PJDLOG_ASSERT(dfd >= 0);
	while ((dp = readdir(dirp)) != NULL) {
		dp->d_type = file_type(dfd, dp->d_name);
		/* We are only interested in regular files, skip the rest. */
		if (dp->d_type != DT_REG) {
			pjdlog_debug(1,
			    "File \"%s/%s\" is not a regular file, skipping.",
			    dirpath, dp->d_name);
			continue;
		}
		service_register_external_one(dirpath, dfd, dp->d_name);
	}
	closedir(dirp);
}

static void
casper_accept(int lsock)
{
	struct casper_service *casserv;
	struct service_connection *sconn;
	int sock;

	sock = accept(lsock, NULL, NULL);
	if (sock == -1) {
		pjdlog_errno(LOG_ERR, "Unable to accept casper connection");
		return;
	}
	casserv = service_find("core.casper");
	PJDLOG_ASSERT(casserv != NULL);

	sconn = service_connection_add(casserv->cs_service, sock, NULL);
	if (sconn == NULL) {
		close(sock);
		return;
	}
}

static void
main_loop(const char *sockpath, struct pidfh *pfh)
{
	fd_set fds;
	struct sockaddr_un sun;
	struct casper_service *casserv;
	struct service_connection *sconn, *sconntmp;
	int lsock, sock, maxfd, ret;
	mode_t oldumask;

	lsock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (lsock == -1)
		pjdlog_exit(1, "Unable to create socket");

	(void)unlink(sockpath);

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	PJDLOG_VERIFY(strlcpy(sun.sun_path, sockpath, sizeof(sun.sun_path)) <
	    sizeof(sun.sun_path));
	sun.sun_len = SUN_LEN(&sun);

	oldumask = umask(S_IXUSR | S_IXGRP | S_IXOTH);
	if (bind(lsock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		pjdlog_exit(1, "Unable to bind to %s", sockpath);
	(void)umask(oldumask);
	if (listen(lsock, 8) == -1)
		pjdlog_exit(1, "Unable to listen on %s", sockpath);

	for (;;) {
		FD_ZERO(&fds);
		FD_SET(lsock, &fds);
		maxfd = lsock;
		TAILQ_FOREACH(casserv, &casper_services, cs_next) {
			/* We handle only core services. */
			if (!SERVICE_IS_CORE(casserv))
				continue;
			for (sconn = service_connection_first(casserv->cs_service);
			    sconn != NULL;
			    sconn = service_connection_next(sconn)) {
				sock = service_connection_get_sock(sconn);
				FD_SET(sock, &fds);
				maxfd = sock > maxfd ? sock : maxfd;
			}
		}
		maxfd++;

		PJDLOG_ASSERT(maxfd <= (int)FD_SETSIZE);
		ret = select(maxfd, &fds, NULL, NULL, NULL);
		PJDLOG_ASSERT(ret == -1 || ret > 0);	/* select() cannot timeout */
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			KEEP_ERRNO((void)pidfile_remove(pfh));
			pjdlog_exit(1, "select() failed");
		}

		if (FD_ISSET(lsock, &fds))
			casper_accept(lsock);
		TAILQ_FOREACH(casserv, &casper_services, cs_next) {
			/* We handle only core services. */
			if (!SERVICE_IS_CORE(casserv))
				continue;
			for (sconn = service_connection_first(casserv->cs_service);
			    sconn != NULL; sconn = sconntmp) {
				/*
				 * Prepare for connection to be removed from
				 * the list on failure.
				 */
				sconntmp = service_connection_next(sconn);
				sock = service_connection_get_sock(sconn);
				if (FD_ISSET(sock, &fds)) {
					service_message(casserv->cs_service,
					    sconn);
				}
			}
		}
	}
}

static void
usage(void)
{

	pjdlog_exitx(1,
	    "usage: casperd [-Fhv] [-D servconfdir] [-P pidfile] [-S sockpath]");
}

int
main(int argc, char *argv[])
{
	struct pidfh *pfh;
	const char *pidfile, *servconfdir, *sockpath;
	pid_t otherpid;
	int ch, debug;
	bool foreground;

	pjdlog_init(PJDLOG_MODE_STD);

	debug = 0;
	foreground = false;
	pidfile = CASPERD_PIDFILE;
	servconfdir = CASPERD_SERVCONFDIR;
	sockpath = CASPERD_SOCKPATH;

	while ((ch = getopt(argc, argv, "D:FhP:S:v")) != -1) {
		switch (ch) {
		case 'D':
			servconfdir = optarg;
			break;
		case 'F':
			foreground = true;
			break;
		case 'P':
			pidfile = optarg;
			break;
		case 'S':
			sockpath = optarg;
			break;
		case 'v':
			debug++;
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if (!foreground)
		pjdlog_mode_set(PJDLOG_MODE_SYSLOG);
	pjdlog_prefix_set("(casperd) ");
	pjdlog_debug_set(debug);

	if (zygote_init() < 0)
		pjdlog_exit(1, "Unable to create zygote process");

	pfh = pidfile_open(pidfile, 0600, &otherpid);
	if (pfh == NULL) {
		if (errno == EEXIST) {
			pjdlog_exitx(1, "casperd already running, pid: %jd.",
			    (intmax_t)otherpid);
		}
		pjdlog_errno(LOG_WARNING, "Cannot open or create pidfile %s",
		    pidfile);
	}

	if (!foreground) {
		if (daemon(0, 0) == -1) {
			KEEP_ERRNO((void)pidfile_remove(pfh));
			pjdlog_exit(1, "Unable to go into background");
		}
	}

	/* Write PID to a file. */
	if (pidfile_write(pfh) == -1) {
		pjdlog_errno(LOG_WARNING, "Unable to write to pidfile %s",
		    pidfile);
	} else {
		pjdlog_debug(1, "PID stored in %s.", pidfile);
	}

	/*
	 * Register core services.
	 */
	service_register_core();
	/*
	 * Register external services.
	 */
	service_register_external(servconfdir);

	/*
	 * Wait for connections.
	 */
	main_loop(sockpath, pfh);
}
